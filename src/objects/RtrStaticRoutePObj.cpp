/**
 * @file    RtrStaticRoutePObj.cpp
 * @brief   RtrStaticRoutePObj — IPv4 static route management
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrStaticRoutePObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

REGISTER_PROC_OBJECT("RtrStaticRoute", RtrStaticRoutePObj)

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RtrStaticRoutePObj::init(EvApplication& loop, LogManager& mgr,
                               const std::string& appTag)
{
    ProcObject::init(loop, mgr, appTag);
    if (log_) log_->log(LOG_DEBUG, logTag_, "RtrStaticRoute initialized");
}

bool RtrStaticRoutePObj::loadConfig(IniConfig& ini, const std::string& section)
{
    ProcObject::loadConfig(ini, section);

    auto [ns, vrf]   = parseNsVrf(nodePath_);
    config_.nsName   = ns;
    config_.vrfName  = vrf;

    config_.network  = ini.getValue(section, "NETWORK",  std::string(""));
    config_.nexthop  = ini.getValue(section, "NEXTHOP",  std::string(""));
    config_.metric   = static_cast<uint32_t>(
                           ini.getValueInteger(section, "METRIC", 1));
    config_.vrfTable = static_cast<uint32_t>(
                           ini.getValueInteger(section, "VRF_TABLE", 0));
    config_.shutdown = ini.getValueBoolean(section, "SHUTDOWN", false);

    return true;
}

// ---------------------------------------------------------------------------
// process()
// ---------------------------------------------------------------------------

void RtrStaticRoutePObj::process()
{
    if (status_.objStatus == ObjStatus::IDLE) {
        applyConfig();
    } else if (status_.objStatus == ObjStatus::ACTIVE) {
        if (++monitorTick_ >= MONITOR_INTERVAL) {
            monitorTick_ = 0;
            monitorRoute();
        }
    }
    syncSnapshot();
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

std::string RtrStaticRoutePObj::buildRouteCmd(bool add) const
{
    std::string cmd = IpCmd::ns(config_.nsName)
                    + " route " + (add ? "add " : "del ")
                    + config_.network
                    + " via " + config_.nexthop
                    + " metric " + std::to_string(config_.metric);
    if (config_.vrfTable > 0)
        cmd += " table " + std::to_string(config_.vrfTable);
    return cmd;
}

bool RtrStaticRoutePObj::routeExists() const
{
    // Show the specific route and check if any output is returned
    std::string cmd = IpCmd::ns(config_.nsName) + " route show " + config_.network;
    if (config_.vrfTable > 0)
        cmd += " table " + std::to_string(config_.vrfTable);
    std::string out = IpCmd::run(cmd);
    return out.find(config_.nexthop) != std::string::npos;
}

void RtrStaticRoutePObj::applyConfig()
{
    status_.objStatus = ObjStatus::ACTIVE;
    status            = ObjStatus::ACTIVE;

    if (config_.network.empty() || config_.nexthop.empty()) {
        status_.operStatus = RtrOperStatus::LLD;
        if (log_) log_->log(LOG_ERROR, logTag_,
                            "NETWORK or NEXTHOP not configured");
        return;
    }

    if (config_.shutdown) {
        // Remove route if present
        if (routeExists())
            IpCmd::exec(buildRouteCmd(false));
        status_.operStatus = RtrOperStatus::DOWN;
        if (log_) log_->log(LOG_INFO, logTag_,
                            "RtrStaticRoute shutdown: route removed");
        return;
    }

    // Add route (idempotent: ignore "already exists" error)
    IpCmd::exec(buildRouteCmd(true));

    bool exists = routeExists();
    status_.operStatus = exists ? RtrOperStatus::UP : RtrOperStatus::LLD;

    if (log_) {
        log_->log(LOG_INFO, logTag_,
                  "RtrStaticRoute " + config_.network + " via " + config_.nexthop +
                  " operStatus=" + operStatusStr(status_.operStatus));
    }
}

void RtrStaticRoutePObj::monitorRoute()
{
    if (config_.shutdown) {
        status_.operStatus = RtrOperStatus::DOWN;
        return;
    }
    bool exists = routeExists();
    status_.operStatus = exists ? RtrOperStatus::UP : RtrOperStatus::DOWN;

    if (!exists) {
        // Route was removed externally — re-add it
        if (log_) log_->log(LOG_WARNING, logTag_,
                            "route " + config_.network + " missing; re-adding");
        IpCmd::exec(buildRouteCmd(true));
    }
}

// ---------------------------------------------------------------------------
// Snapshot / JSON
// ---------------------------------------------------------------------------

void RtrStaticRoutePObj::syncSnapshot()
{
    int next = snapIdx_.load(std::memory_order_relaxed) ^ 1;
    statusSnap_[next] = status_;
    snapIdx_.store(next, std::memory_order_release);
}

std::string RtrStaticRoutePObj::buildStatusJson() const
{
    int idx = snapIdx_.load(std::memory_order_acquire);
    const RtrStaticRouteStatus& s = statusSnap_[idx];

    return std::string("{\"type\":\"RtrStaticRoute\",\"name\":\"") + name_ +
           "\",\"node_type\":\"" + nodeType_ +
           "\",\"node_instance\":\"" + nodeInstance_ +
           "\",\"node_path\":\"" + nodePath_ +
           "\",\"status\":\"" + (s.objStatus == ObjStatus::ACTIVE ? "ACTIVE" : "IDLE") +
           "\",\"oper_status\":\"" + operStatusStr(s.operStatus) + "\"}";
}
