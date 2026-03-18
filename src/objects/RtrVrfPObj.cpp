/**
 * @file    RtrVrfPObj.cpp
 * @brief   RtrVrfPObj — Linux VRF device management
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrVrfPObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

#include <functional>

REGISTER_PROC_OBJECT("RtrVrf", RtrVrfPObj)

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RtrVrfPObj::init(EvApplication& loop, LogManager& mgr,
                      const std::string& appTag)
{
    ProcObject::init(loop, mgr, appTag);
    if (log_) log_->log(LOG_DEBUG, logTag_, "RtrVrf initialized");
}

bool RtrVrfPObj::loadConfig(IniConfig& ini, const std::string& section)
{
    ProcObject::loadConfig(ini, section);

    auto [ns, vrf] = parseNsVrf(nodePath_);
    config_.nsName  = ns;
    config_.vrfName = vrf;

    int64_t tbl = ini.getValueInteger(section, "TABLE", 0);
    config_.tableId  = (tbl > 0) ? static_cast<uint32_t>(tbl) : 0;
    config_.shutdown = ini.getValueBoolean(section, "SHUTDOWN", false);

    return true;
}

// ---------------------------------------------------------------------------
// process()
// ---------------------------------------------------------------------------

void RtrVrfPObj::process()
{
    if (status_.objStatus == ObjStatus::IDLE) {
        applyConfig();
    } else if (status_.objStatus == ObjStatus::ACTIVE) {
        if (++monitorTick_ >= MONITOR_INTERVAL) {
            monitorTick_ = 0;
            monitorVrf();
        }
    }
    syncSnapshot();
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

uint32_t RtrVrfPObj::autoTableId() const
{
    return 1000u + static_cast<uint32_t>(
                       std::hash<std::string>{}(config_.vrfName)) % 9000u;
}

void RtrVrfPObj::applyConfig()
{
    status_.objStatus = ObjStatus::ACTIVE;
    status            = ObjStatus::ACTIVE;

    if (config_.vrfName == "default") {
        status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN : RtrOperStatus::UP;
        if (log_) log_->log(LOG_INFO, logTag_, "RtrVrf: using default routing table");
        return;
    }

    if (!config_.shutdown) {
        uint32_t tbl = (config_.tableId > 0) ? config_.tableId : autoTableId();
        std::string cmd = IpCmd::ns(config_.nsName)
                        + " link add " + config_.vrfName
                        + " type vrf table " + std::to_string(tbl);
        IpCmd::exec(cmd);   // idempotent: ignore "already exists" error
        IpCmd::exec(IpCmd::ns(config_.nsName) + " link set " + config_.vrfName + " up");
    }

    bool exists = vrfDeviceExists();
    status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN
                       : (exists ? RtrOperStatus::UP : RtrOperStatus::LLD);

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrVrf vrf=" + config_.vrfName +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

void RtrVrfPObj::monitorVrf()
{
    if (config_.vrfName == "default") {
        status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN : RtrOperStatus::UP;
        return;
    }
    bool exists = vrfDeviceExists();
    status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN
                       : (exists ? RtrOperStatus::UP : RtrOperStatus::LLD);
}

bool RtrVrfPObj::vrfDeviceExists() const
{
    std::string out = IpCmd::run(IpCmd::ns(config_.nsName) +
                                 " link show " + config_.vrfName);
    return !out.empty();
}

// ---------------------------------------------------------------------------
// Snapshot / JSON
// ---------------------------------------------------------------------------

void RtrVrfPObj::syncSnapshot()
{
    int next = snapIdx_.load(std::memory_order_relaxed) ^ 1;
    statusSnap_[next] = status_;
    snapIdx_.store(next, std::memory_order_release);
}

std::string RtrVrfPObj::buildStatusJson() const
{
    int idx = snapIdx_.load(std::memory_order_acquire);
    const RtrVrfStatus& s = statusSnap_[idx];

    return std::string("{\"type\":\"RtrVrf\",\"name\":\"") + name_ +
           "\",\"node_type\":\"" + nodeType_ +
           "\",\"node_instance\":\"" + nodeInstance_ +
           "\",\"node_path\":\"" + nodePath_ +
           "\",\"status\":\"" + (s.objStatus == ObjStatus::ACTIVE ? "ACTIVE" : "IDLE") +
           "\",\"oper_status\":\"" + operStatusStr(s.operStatus) + "\"}";
}
