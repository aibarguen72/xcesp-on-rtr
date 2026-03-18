/**
 * @file    RtrInterfacePObj.h
 * @brief   RtrInterfacePObj — network interface management (device or loopback)
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Two interface types are supported via the IFACE_TYPE INI key:
 *
 *   device   (default) — manages an already-existing Linux network device.
 *                        The device is never created or deleted.
 *                        IP addresses are synchronised exactly (add missing,
 *                        remove extra).
 *
 *   loopback           — manages a loopback-style software interface.
 *                        If DEVICE == "lo" the standard kernel loopback is used.
 *                        Any other name is treated as a dummy interface that is
 *                        created on demand (`ip link add <dev> type dummy`).
 *                        IP addresses are synchronised exactly.
 *
 * OperStatus:
 *   UP   — device found and operationally UP (or UNKNOWN for loopback)
 *   DOWN — device found but operationally DOWN
 *   LLD  — device not found or a command failed
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

enum class IfaceType { DEVICE, LOOPBACK };

struct RtrInterfaceConfig {
    std::string              nsName;
    std::string              vrfName;
    std::string              device;
    IfaceType                ifaceType = IfaceType::DEVICE;
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
    void applyDevice();
    void applyLoopback();
    void syncAddresses(const std::string& nsPrefix, const std::string& dev);
    void monitorLinkState();
    void updateLinkStatus(const std::string& operstate);
};

#endif // RTRINTERFACEPOBJ_H
