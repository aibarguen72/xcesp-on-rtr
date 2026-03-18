/**
 * @file    RtrRouterPObj.cpp
 * @brief   RtrRouterPObj — Linux network namespace management
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "RtrRouterPObj.h"
#include "ProcObjectRegistry.h"
#include "IpCmd.h"

REGISTER_PROC_OBJECT("RtrRouter", RtrRouterPObj)

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RtrRouterPObj::init(EvApplication& loop, LogManager& mgr,
                         const std::string& appTag)
{
    ProcObject::init(loop, mgr, appTag);
    if (log_) log_->log(LOG_DEBUG, logTag_, "RtrRouter initialized");
}

bool RtrRouterPObj::loadConfig(IniConfig& ini, const std::string& section)
{
    ProcObject::loadConfig(ini, section);

    config_.nsName   = ini.getValue(section, "NAMESPACE", std::string("default"));
    config_.shutdown = ini.getValueBoolean(section, "SHUTDOWN", false);

    return true;
}

// ---------------------------------------------------------------------------
// process()
// ---------------------------------------------------------------------------

void RtrRouterPObj::process()
{
    if (status_.objStatus == ObjStatus::IDLE) {
        applyConfig();
    } else if (status_.objStatus == ObjStatus::ACTIVE) {
        if (++monitorTick_ >= MONITOR_INTERVAL) {
            monitorTick_ = 0;
            monitorNamespace();
        }
    }
    syncSnapshot();
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

void RtrRouterPObj::applyConfig()
{
    if (config_.nsName == "default") {
        // Host namespace always exists
        status_.objStatus  = ObjStatus::ACTIVE;
        status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN : RtrOperStatus::UP;
        status             = ObjStatus::ACTIVE;
        if (log_) log_->log(LOG_INFO, logTag_, "RtrRouter: using host namespace");
        return;
    }

    if (!config_.shutdown) {
        // Create namespace (idempotent: ignore error if already exists)
        IpCmd::exec("ip netns add " + config_.nsName);
    }

    bool exists = namespaceExists();
    status_.objStatus  = ObjStatus::ACTIVE;
    status             = ObjStatus::ACTIVE;
    status_.operStatus = exists
                         ? (config_.shutdown ? RtrOperStatus::DOWN : RtrOperStatus::UP)
                         : RtrOperStatus::LLD;

    if (log_) {
        std::string msg = "RtrRouter ns=" + config_.nsName
                        + " operStatus=" + operStatusStr(status_.operStatus);
        log_->log(LOG_INFO, logTag_, msg);
    }
}

void RtrRouterPObj::monitorNamespace()
{
    if (config_.nsName == "default") {
        status_.operStatus = config_.shutdown ? RtrOperStatus::DOWN : RtrOperStatus::UP;
        return;
    }
    bool exists = namespaceExists();
    if (config_.shutdown) {
        status_.operStatus = RtrOperStatus::DOWN;
    } else {
        status_.operStatus = exists ? RtrOperStatus::UP : RtrOperStatus::LLD;
        if (!exists) {
            // Attempt re-creation
            if (log_) log_->log(LOG_WARNING, logTag_,
                                "namespace " + config_.nsName + " missing; re-creating");
            IpCmd::exec("ip netns add " + config_.nsName);
        }
    }
}

bool RtrRouterPObj::namespaceExists() const
{
    std::string out = IpCmd::run("ip netns list");
    // Look for the exact namespace name as a word
    return out.find(config_.nsName) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Snapshot / JSON
// ---------------------------------------------------------------------------

void RtrRouterPObj::syncSnapshot()
{
    int next = snapIdx_.load(std::memory_order_relaxed) ^ 1;
    statusSnap_[next] = status_;
    snapIdx_.store(next, std::memory_order_release);
}

std::string RtrRouterPObj::buildStatusJson() const
{
    int idx = snapIdx_.load(std::memory_order_acquire);
    const RtrRouterStatus& s = statusSnap_[idx];

    return std::string("{\"type\":\"RtrRouter\",\"name\":\"") + name_ +
           "\",\"node_type\":\"" + nodeType_ +
           "\",\"node_instance\":\"" + nodeInstance_ +
           "\",\"node_path\":\"" + nodePath_ +
           "\",\"status\":\"" + (s.objStatus == ObjStatus::ACTIVE ? "ACTIVE" : "IDLE") +
           "\",\"oper_status\":\"" + operStatusStr(s.operStatus) + "\"}";
}
