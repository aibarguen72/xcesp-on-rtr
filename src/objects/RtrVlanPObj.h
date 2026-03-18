/**
 * @file    RtrVlanPObj.h
 * @brief   RtrVlanPObj — 802.1Q VLAN subinterface management object
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Creates and manages a VLAN subinterface on top of a parent interface.
 * In shutdown, the VLAN device is not created (planned only).
 *
 * OperStatus:
 *   UP   — VLAN device exists and link is operationally UP
 *   DOWN — VLAN device exists and link is DOWN, or shutdown=true
 *   LLD  — parent device not found or VLAN creation failed
 *
 * Alarm: LOS / MAJOR — same as RtrInterfacePObj.
 */

#ifndef RTRVLANPOBJ_H
#define RTRVLANPOBJ_H

#include <atomic>
#include <string>
#include <vector>
#include "ProcObject.h"
#include "RtrCommon.h"

struct RtrVlanConfig {
    std::string              nsName;
    std::string              vrfName;
    std::string              parentDevice;
    uint32_t                 vlanId   = 0;
    std::vector<std::string> addresses;
    bool                     shutdown = false;
};

struct RtrVlanStatus {
    ProcObject::ObjStatus objStatus  = ProcObject::ObjStatus::IDLE;
    RtrOperStatus         operStatus = RtrOperStatus::LLD;
    bool                  linkUp     = false;
};

class RtrVlanPObj : public ProcObject {
public:
    ~RtrVlanPObj() override = default;

    void init(EvApplication& loop, LogManager& mgr,
              const std::string& appTag) override;
    bool loadConfig(IniConfig& ini, const std::string& section) override;
    void process() override;

    void        syncSnapshot() override;
    std::string buildStatusJson() const override;

private:
    RtrVlanConfig  config_{};
    RtrVlanStatus  status_{};

    std::atomic<int> snapIdx_{0};
    RtrVlanStatus    statusSnap_[2]{};

    int  monitorTick_ = 0;
    bool losActive_   = false;
    static constexpr int MONITOR_INTERVAL = 5;

    std::string vlanDevice() const;   // returns "<parent>.<vlanId>"
    void        applyConfig();
    void        monitorLinkState();
    void        updateLinkStatus(const std::string& operstate);
};

#endif // RTRVLANPOBJ_H
