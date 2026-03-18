/**
 * @file    RtrStaticRoutePObj.h
 * @brief   RtrStaticRoutePObj — IPv4 static route management object
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Adds or removes a static IPv4 route within a namespace and optional VRF
 * routing table.  In shutdown, the route is removed (or not added).
 *
 * OperStatus:
 *   UP   — route is present in the routing table
 *   DOWN — route is absent (shutdown=true or not yet added)
 *   LLD  — ip route command failed
 */

#ifndef RTRSTATICROUTEPOBJ_H
#define RTRSTATICROUTEPOBJ_H

#include <atomic>
#include <string>
#include "ProcObject.h"
#include "RtrCommon.h"

struct RtrStaticRouteConfig {
    std::string nsName;
    std::string vrfName;
    std::string network;     ///< e.g. "0.0.0.0/0"
    std::string nexthop;     ///< e.g. "10.10.10.1"
    uint32_t    metric   = 1;
    uint32_t    vrfTable = 0; ///< 0 = default table (no VRF or unspecified)
    bool        shutdown = false;
};

struct RtrStaticRouteStatus {
    ProcObject::ObjStatus objStatus  = ProcObject::ObjStatus::IDLE;
    RtrOperStatus         operStatus = RtrOperStatus::LLD;
};

class RtrStaticRoutePObj : public ProcObject {
public:
    ~RtrStaticRoutePObj() override = default;

    void init(EvApplication& loop, LogManager& mgr,
              const std::string& appTag) override;
    bool loadConfig(IniConfig& ini, const std::string& section) override;
    void process() override;

    void        syncSnapshot() override;
    std::string buildStatusJson() const override;

private:
    RtrStaticRouteConfig  config_{};
    RtrStaticRouteStatus  status_{};

    std::atomic<int>      snapIdx_{0};
    RtrStaticRouteStatus  statusSnap_[2]{};

    int monitorTick_ = 0;
    static constexpr int MONITOR_INTERVAL = 5;

    void applyConfig();
    void monitorRoute();
    bool routeExists() const;
    std::string buildRouteCmd(bool add) const;
};

#endif // RTRSTATICROUTEPOBJ_H
