/**
 * @file    RtrInterfacePObj.cpp
 * @brief   RtrInterfacePObj — network interface management (device or loopback)
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrInterfacePObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

#include <algorithm>

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

    std::string ifType = ini.getValue(section, "IFACE_TYPE", std::string("device"));
    config_.ifaceType  = (ifType == "loopback") ? IfaceType::LOOPBACK : IfaceType::DEVICE;

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
// applyConfig — dispatch to device or loopback path
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

    if (config_.ifaceType == IfaceType::LOOPBACK)
        applyLoopback();
    else
        applyDevice();
}

// ---------------------------------------------------------------------------
// applyDevice — manage an already-existing network device
// ---------------------------------------------------------------------------

void RtrInterfacePObj::applyDevice()
{
    const std::string nsPrefix = IpCmd::ns(config_.nsName);

    // 1. Check device exists in the default ns (before namespace move)
    std::string devCheck = IpCmd::run("ip link show " + config_.device);
    if (devCheck.empty()) {
        // Might already be in the target ns
        devCheck = IpCmd::run(nsPrefix + " link show " + config_.device);
        if (devCheck.empty()) {
            status_.operStatus = RtrOperStatus::LLD;
            if (log_) log_->log(LOG_WARNING, logTag_,
                                "device " + config_.device + " not found");
            return;
        }
    }

    // 2. Move to namespace (if not "default" and not already there)
    if (config_.nsName != "default") {
        std::string inNs = IpCmd::run(nsPrefix + " link show " + config_.device);
        if (inNs.empty())
            IpCmd::exec("ip link set " + config_.device + " netns " + config_.nsName);
    }

    // 3. Bind to VRF (if not "default")
    if (config_.vrfName != "default")
        IpCmd::exec(nsPrefix + " link set " + config_.device +
                    " master " + config_.vrfName);

    // 4. Sync IP addresses exactly (add missing, remove extra)
    syncAddresses(nsPrefix, config_.device);

    // 5. Set administrative link state
    std::string state = config_.shutdown ? "down" : "up";
    IpCmd::exec(nsPrefix + " link set " + config_.device + " " + state);

    // 6. Read actual operstate
    std::string json = IpCmd::run(nsPrefix + " -j link show " + config_.device);
    updateLinkStatus(IpCmd::parseOperstate(json));

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrInterface(device) dev=" + config_.device +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

// ---------------------------------------------------------------------------
// applyLoopback — manage a loopback / dummy interface
// ---------------------------------------------------------------------------

void RtrInterfacePObj::applyLoopback()
{
    const std::string nsPrefix = IpCmd::ns(config_.nsName);
    const std::string& dev     = config_.device;

    if (dev == "lo") {
        // Standard kernel loopback — always present in every namespace.
        // Just bring it up (it may be DOWN in a freshly created namespace).
        IpCmd::exec(nsPrefix + " link set lo up");
    } else {
        // Dummy interface — create if it doesn't already exist.
        std::string devCheck = IpCmd::run(nsPrefix + " link show " + dev);
        if (devCheck.empty()) {
            bool ok = IpCmd::exec(nsPrefix + " link add " + dev + " type dummy");
            if (!ok) {
                status_.operStatus = RtrOperStatus::LLD;
                if (log_) log_->log(LOG_ERROR, logTag_,
                                    "failed to create dummy device " + dev);
                return;
            }
        }

        // Bind to VRF (if not "default")
        if (config_.vrfName != "default")
            IpCmd::exec(nsPrefix + " link set " + dev +
                        " master " + config_.vrfName);
    }

    // Sync IP addresses exactly (add missing, remove extra)
    syncAddresses(nsPrefix, dev);

    // Set administrative link state
    std::string state = config_.shutdown ? "down" : "up";
    IpCmd::exec(nsPrefix + " link set " + dev + " " + state);

    // Read actual operstate — dummy/lo show "UNKNOWN" when up; treat as UP
    std::string json = IpCmd::run(nsPrefix + " -j link show " + dev);
    std::string ops  = IpCmd::parseOperstate(json);
    if (ops == "UNKNOWN" || ops == "unknown")
        ops = "UP";
    updateLinkStatus(ops);

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrInterface(loopback) dev=" + dev +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

// ---------------------------------------------------------------------------
// syncAddresses — add missing, remove extra IPv4 addresses on a device
// ---------------------------------------------------------------------------

void RtrInterfacePObj::syncAddresses(const std::string& nsPrefix,
                                      const std::string& dev)
{
    std::string out     = IpCmd::run(nsPrefix + " addr show dev " + dev);
    auto        current = IpCmd::parseAddrList(out);

    // Remove addresses present on device but not in config
    for (const auto& addr : current) {
        if (std::find(config_.addresses.begin(), config_.addresses.end(), addr)
                == config_.addresses.end())
            IpCmd::exec(nsPrefix + " addr del " + addr + " dev " + dev);
    }

    // Add addresses in config but not yet on device
    for (const auto& addr : config_.addresses) {
        if (std::find(current.begin(), current.end(), addr) == current.end())
            IpCmd::exec(nsPrefix + " addr add " + addr + " dev " + dev);
    }
}

// ---------------------------------------------------------------------------
// monitorLinkState
// ---------------------------------------------------------------------------

void RtrInterfacePObj::monitorLinkState()
{
    if (config_.device.empty()) return;

    const std::string nsPrefix = IpCmd::ns(config_.nsName);

    std::string json = IpCmd::run(nsPrefix + " -j link show " + config_.device);
    if (json.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        status_.linkUp     = false;
        if (!losActive_ && !config_.shutdown) {
            losActive_ = true;
            raiseAlarm("LOS", "MAJOR");
        }

        // Loopback dummy device: attempt to recreate it
        if (config_.ifaceType == IfaceType::LOOPBACK &&
            config_.device != "lo") {
            if (log_) log_->log(LOG_WARNING, logTag_,
                                "dummy device " + config_.device +
                                " missing; recreating");
            applyLoopback();
        }
        return;
    }

    std::string ops = IpCmd::parseOperstate(json);
    // Treat UNKNOWN as UP for loopback interfaces
    if (config_.ifaceType == IfaceType::LOOPBACK &&
        (ops == "UNKNOWN" || ops == "unknown"))
        ops = "UP";

    updateLinkStatus(ops);
}

void RtrInterfacePObj::updateLinkStatus(const std::string& operstate)
{
    bool up = (operstate == "UP" || operstate == "up");
    status_.linkUp = up;

    if (config_.shutdown) {
        status_.operStatus = RtrOperStatus::DOWN;
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
