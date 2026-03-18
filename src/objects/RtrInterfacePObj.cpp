/**
 * @file    RtrInterfacePObj.cpp
 * @brief   RtrInterfacePObj — physical/logical network interface management
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrInterfacePObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

REGISTER_PROC_OBJECT("RtrInterface", RtrInterfacePObj)

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RtrInterfacePObj::init(EvApplication& loop, LogManager& mgr,
                             const std::string& appTag)
{
    ProcObject::init(loop, mgr, appTag);
    if (log_) log_->log(LOG_DEBUG, logTag_, "RtrInterface initialized");
}

bool RtrInterfacePObj::loadConfig(IniConfig& ini, const std::string& section)
{
    ProcObject::loadConfig(ini, section);

    auto [ns, vrf]  = parseNsVrf(nodePath_);
    config_.nsName  = ns;
    config_.vrfName = vrf;

    config_.device    = ini.getValue(section, "DEVICE", std::string(""));
    config_.addresses = splitCsv(ini.getValue(section, "ADDRESSES", std::string("")));
    config_.shutdown  = ini.getValueBoolean(section, "SHUTDOWN", false);

    if (config_.device.empty() && log_)
        log_->log(LOG_WARNING, logTag_, "DEVICE not set");

    return true;
}

// ---------------------------------------------------------------------------
// process()
// ---------------------------------------------------------------------------

void RtrInterfacePObj::process()
{
    if (status_.objStatus == ObjStatus::IDLE) {
        applyConfig();
    } else if (status_.objStatus == ObjStatus::ACTIVE) {
        if (++monitorTick_ >= MONITOR_INTERVAL) {
            monitorTick_ = 0;
            monitorLinkState();
        }
    }
    syncSnapshot();
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

void RtrInterfacePObj::applyConfig()
{
    status_.objStatus = ObjStatus::ACTIVE;
    status            = ObjStatus::ACTIVE;

    if (config_.device.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        if (log_) log_->log(LOG_ERROR, logTag_, "DEVICE not configured");
        return;
    }

    // 1. Check device exists in the default ns (before namespace move)
    std::string devCheck = IpCmd::run("ip link show " + config_.device);
    if (devCheck.empty()) {
        // Might already be in the target ns
        devCheck = IpCmd::run(IpCmd::ns(config_.nsName) +
                              " link show " + config_.device);
        if (devCheck.empty()) {
            status_.operStatus = RtrOperStatus::LLD;
            if (log_) log_->log(LOG_WARNING, logTag_,
                                "device " + config_.device + " not found");
            return;
        }
    }

    // 2. Move to namespace (if not "default" and not already there)
    if (config_.nsName != "default") {
        // Only move if not already in the target ns
        std::string inNs = IpCmd::run(IpCmd::ns(config_.nsName) +
                                      " link show " + config_.device);
        if (inNs.empty())
            IpCmd::exec("ip link set " + config_.device + " netns " + config_.nsName);
    }

    // 3. Bind to VRF (if not "default")
    if (config_.vrfName != "default")
        IpCmd::exec(IpCmd::ns(config_.nsName) +
                    " link set " + config_.device + " master " + config_.vrfName);

    // 4. Add IP addresses (idempotent: ip addr add returns error if already set)
    for (const auto& addr : config_.addresses)
        IpCmd::exec(IpCmd::ns(config_.nsName) +
                    " addr add " + addr + " dev " + config_.device);

    // 5. Set administrative link state
    std::string state = config_.shutdown ? "down" : "up";
    IpCmd::exec(IpCmd::ns(config_.nsName) +
                " link set " + config_.device + " " + state);

    // 6. Read actual operstate
    std::string json = IpCmd::run(IpCmd::ns(config_.nsName) +
                                  " -j link show " + config_.device);
    updateLinkStatus(IpCmd::parseOperstate(json));

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrInterface dev=" + config_.device +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

void RtrInterfacePObj::monitorLinkState()
{
    if (config_.device.empty()) return;

    std::string json = IpCmd::run(IpCmd::ns(config_.nsName) +
                                  " -j link show " + config_.device);
    if (json.empty()) {
        // Device disappeared
        status_.operStatus = RtrOperStatus::LLD;
        status_.linkUp     = false;
        if (!losActive_ && !config_.shutdown) {
            losActive_ = true;
            raiseAlarm("LOS", "MAJOR");
        }
        return;
    }
    updateLinkStatus(IpCmd::parseOperstate(json));
}

void RtrInterfacePObj::updateLinkStatus(const std::string& operstate)
{
    bool up = (operstate == "UP" || operstate == "up");
    status_.linkUp = up;

    if (config_.shutdown) {
        status_.operStatus = RtrOperStatus::DOWN;
        // In shutdown: clear any LOS that was active
        if (losActive_) {
            losActive_ = false;
            clearAlarm("LOS");
        }
    } else {
        status_.operStatus = up ? RtrOperStatus::UP : RtrOperStatus::DOWN;
        if (!up && !losActive_) {
            losActive_ = true;
            raiseAlarm("LOS", "MAJOR");
        } else if (up && losActive_) {
            losActive_ = false;
            clearAlarm("LOS");
        }
    }
}

// ---------------------------------------------------------------------------
// Snapshot / JSON
// ---------------------------------------------------------------------------

void RtrInterfacePObj::syncSnapshot()
{
    int next = snapIdx_.load(std::memory_order_relaxed) ^ 1;
    statusSnap_[next] = status_;
    snapIdx_.store(next, std::memory_order_release);
}

std::string RtrInterfacePObj::buildStatusJson() const
{
    int idx = snapIdx_.load(std::memory_order_acquire);
    const RtrInterfaceStatus& s = statusSnap_[idx];

    return std::string("{\"type\":\"RtrInterface\",\"name\":\"") + name_ +
           "\",\"node_type\":\"" + nodeType_ +
           "\",\"node_instance\":\"" + nodeInstance_ +
           "\",\"node_path\":\"" + nodePath_ +
           "\",\"status\":\"" + (s.objStatus == ObjStatus::ACTIVE ? "ACTIVE" : "IDLE") +
           "\",\"oper_status\":\"" + operStatusStr(s.operStatus) +
           "\",\"link_state\":\"" + (s.linkUp ? "UP" : "DOWN") + "\"}";
}
