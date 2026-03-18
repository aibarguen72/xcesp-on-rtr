/**
 * @file    RtrVlanPObj.cpp
 * @brief   RtrVlanPObj — 802.1Q VLAN subinterface management
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrVlanPObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

REGISTER_PROC_OBJECT("RtrVlan", RtrVlanPObj)

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RtrVlanPObj::init(EvApplication& loop, LogManager& mgr,
                       const std::string& appTag)
{
    ProcObject::init(loop, mgr, appTag);
    if (log_) log_->log(LOG_DEBUG, logTag_, "RtrVlan initialized");
}

bool RtrVlanPObj::loadConfig(IniConfig& ini, const std::string& section)
{
    ProcObject::loadConfig(ini, section);

    auto [ns, vrf]   = parseNsVrf(nodePath_);
    config_.nsName   = ns;
    config_.vrfName  = vrf;

    config_.parentDevice = ini.getValue(section, "PARENT_DEVICE", std::string(""));
    config_.vlanId       = static_cast<uint32_t>(
                               ini.getValueInteger(section, "VLAN_ID", 0));
    config_.addresses    = splitCsv(ini.getValue(section, "ADDRESSES", std::string("")));
    config_.shutdown     = ini.getValueBoolean(section, "SHUTDOWN", false);

    return true;
}

// ---------------------------------------------------------------------------
// process()
// ---------------------------------------------------------------------------

void RtrVlanPObj::process()
{
    if (status_.objStatus == ObjStatus::IDLE) {
        applyConfig();
    } else if (status_.objStatus == ObjStatus::ACTIVE) {
        if (++monitorTick_ >= MONITOR_INTERVAL) {
            monitorTick_ = 0;
            if (!config_.shutdown)
                monitorLinkState();
        }
    }
    syncSnapshot();
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

std::string RtrVlanPObj::vlanDevice() const
{
    return config_.parentDevice + "." + std::to_string(config_.vlanId);
}

void RtrVlanPObj::applyConfig()
{
    status_.objStatus = ObjStatus::ACTIVE;
    status            = ObjStatus::ACTIVE;

    if (config_.shutdown) {
        status_.operStatus = RtrOperStatus::DOWN;
        if (log_) log_->log(LOG_INFO, logTag_,
                            "RtrVlan shutdown: vlan not deployed");
        return;
    }

    if (config_.parentDevice.empty() || config_.vlanId == 0) {
        status_.operStatus = RtrOperStatus::LLD;
        if (log_) log_->log(LOG_ERROR, logTag_,
                            "PARENT_DEVICE or VLAN_ID not configured");
        return;
    }

    std::string vdev = vlanDevice();

    // 1. Verify parent device exists in the namespace
    std::string parentCheck = IpCmd::run(IpCmd::ns(config_.nsName) +
                                         " link show " + config_.parentDevice);
    if (parentCheck.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        if (log_) log_->log(LOG_WARNING, logTag_,
                            "parent device " + config_.parentDevice + " not found");
        return;
    }

    // 2. Create VLAN subinterface (idempotent)
    IpCmd::exec(IpCmd::ns(config_.nsName) +
                " link add link " + config_.parentDevice +
                " name " + vdev +
                " type vlan id " + std::to_string(config_.vlanId));

    // 3. Bind to VRF (if not "default")
    if (config_.vrfName != "default")
        IpCmd::exec(IpCmd::ns(config_.nsName) +
                    " link set " + vdev + " master " + config_.vrfName);

    // 4. Add IP addresses
    for (const auto& addr : config_.addresses)
        IpCmd::exec(IpCmd::ns(config_.nsName) +
                    " addr add " + addr + " dev " + vdev);

    // 5. Bring VLAN device up
    IpCmd::exec(IpCmd::ns(config_.nsName) + " link set " + vdev + " up");

    // 6. Read actual operstate
    std::string json = IpCmd::run(IpCmd::ns(config_.nsName) +
                                  " -j link show " + vdev);
    if (json.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        if (log_) log_->log(LOG_ERROR, logTag_,
                            "VLAN device " + vdev + " not found after create");
        return;
    }
    updateLinkStatus(IpCmd::parseOperstate(json));

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrVlan dev=" + vdev +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

void RtrVlanPObj::monitorLinkState()
{
    std::string vdev = vlanDevice();
    std::string json = IpCmd::run(IpCmd::ns(config_.nsName) +
                                  " -j link show " + vdev);
    if (json.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        status_.linkUp     = false;
        if (!losActive_) {
            losActive_ = true;
            raiseAlarm("LOS", "MAJOR");
        }
        return;
    }
    updateLinkStatus(IpCmd::parseOperstate(json));
}

void RtrVlanPObj::updateLinkStatus(const std::string& operstate)
{
    bool up = (operstate == "UP" || operstate == "up");
    status_.linkUp     = up;
    status_.operStatus = up ? RtrOperStatus::UP : RtrOperStatus::DOWN;

    if (!up && !losActive_) {
        losActive_ = true;
        raiseAlarm("LOS", "MAJOR");
    } else if (up && losActive_) {
        losActive_ = false;
        clearAlarm("LOS");
    }
}

// ---------------------------------------------------------------------------
// Snapshot / JSON
// ---------------------------------------------------------------------------

void RtrVlanPObj::syncSnapshot()
{
    int next = snapIdx_.load(std::memory_order_relaxed) ^ 1;
    statusSnap_[next] = status_;
    snapIdx_.store(next, std::memory_order_release);
}

std::string RtrVlanPObj::buildStatusJson() const
{
    int idx = snapIdx_.load(std::memory_order_acquire);
    const RtrVlanStatus& s = statusSnap_[idx];

    return std::string("{\"type\":\"RtrVlan\",\"name\":\"") + name_ +
           "\",\"node_type\":\"" + nodeType_ +
           "\",\"node_instance\":\"" + nodeInstance_ +
           "\",\"node_path\":\"" + nodePath_ +
           "\",\"status\":\"" + (s.objStatus == ObjStatus::ACTIVE ? "ACTIVE" : "IDLE") +
           "\",\"oper_status\":\"" + operStatusStr(s.operStatus) +
           "\",\"link_state\":\"" + (s.linkUp ? "UP" : "DOWN") + "\"}";
}
