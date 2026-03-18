/**
 * @file    RtrInterfacePObj.h
 * @brief   RtrInterfacePObj — physical/logical network interface object
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Manages an existing Linux network device within a namespace/VRF.
 * The device must already exist; it cannot be created.
 *
 * "shutdown" controls administrative link state (up/down).
 * The object is always deployed — even in shutdown — so it can
 * report the device's presence and set the link state.
 *
 * OperStatus:
 *   UP   — device found, link is operationally UP
 *   DOWN — device found, link is operationally DOWN
 *   LLD  — device not found or command failed
 *
 * Alarm: LOS / MAJOR — raised when shutdown=false and link goes DOWN.
 *                       Cleared when link comes back UP.
 */

#ifndef RTRINTERFACEPOBJ_H
#define RTRINTERFACEPOBJ_H

#include <atomic>
#include <string>
#include <vector>
#include "ProcObject.h"
#include "RtrCommon.h"

struct RtrInterfaceConfig {
    std::string              nsName;
    std::string              vrfName;
    std::string              device;
    std::vector<std::string> addresses;
    bool                     shutdown = false;
};

struct RtrInterfaceStatus {
    ProcObject::ObjStatus objStatus  = ProcObject::ObjStatus::IDLE;
    RtrOperStatus         operStatus = RtrOperStatus::LLD;
    bool                  linkUp     = false;
};

class RtrInterfacePObj : public ProcObject {
public:
    ~RtrInterfacePObj() override = default;

    void init(EvApplication& loop, LogManager& mgr,
              const std::string& appTag) override;
    bool loadConfig(IniConfig& ini, const std::string& section) override;
    void process() override;

    void        syncSnapshot() override;
    std::string buildStatusJson() const override;

private:
    RtrInterfaceConfig  config_{};
    RtrInterfaceStatus  status_{};

    std::atomic<int>    snapIdx_{0};
    RtrInterfaceStatus  statusSnap_[2]{};

    int  monitorTick_ = 0;
    bool losActive_   = false;
    static constexpr int MONITOR_INTERVAL = 5;

    void applyConfig();
    void monitorLinkState();
    void updateLinkStatus(const std::string& operstate);
};

#endif // RTRINTERFACEPOBJ_H
