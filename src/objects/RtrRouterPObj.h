/**
 * @file    RtrRouterPObj.h
 * @brief   RtrRouterPObj — Linux network namespace management object
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Manages a single Linux network namespace.  "default" maps to the host
 * network namespace (no create/delete; always operStatus UP).
 */

#ifndef RTRROUTERPOBJ_H
#define RTRROUTERPOBJ_H

#include <atomic>
#include <string>
#include "ProcObject.h"
#include "RtrCommon.h"

// ---------------------------------------------------------------------------
// Config / Status structs
// ---------------------------------------------------------------------------

struct RtrRouterConfig {
    std::string nsName;        ///< namespace name; "default" = host ns
    bool        shutdown = false;
};

struct RtrRouterStatus {
    ProcObject::ObjStatus objStatus  = ProcObject::ObjStatus::IDLE;
    RtrOperStatus         operStatus = RtrOperStatus::LLD;
};

// ---------------------------------------------------------------------------
// RtrRouterPObj
// ---------------------------------------------------------------------------

class RtrRouterPObj : public ProcObject {
public:
    ~RtrRouterPObj() override = default;

    void init(EvApplication& loop, LogManager& mgr,
              const std::string& appTag) override;
    bool loadConfig(IniConfig& ini, const std::string& section) override;
    void process() override;

    void        syncSnapshot() override;
    std::string buildStatusJson() const override;

private:
    RtrRouterConfig  config_{};
    RtrRouterStatus  status_{};

    std::atomic<int> snapIdx_{0};
    RtrRouterStatus  statusSnap_[2]{};

    int monitorTick_ = 0;
    static constexpr int MONITOR_INTERVAL = 5;

    void    applyConfig();
    void    monitorNamespace();
    bool    namespaceExists() const;
};

#endif // RTRROUTERPOBJ_H
