/**
 * @file    RtrVrfPObj.h
 * @brief   RtrVrfPObj — Linux VRF device management object
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Manages a Linux VRF device within a network namespace.
 * "vrf default" maps to the default routing table (no device created).
 * Table number is optional; when absent one is auto-assigned from the
 * VRF name hash in the range [1000, 9999].
 */

#ifndef RTRVRFPOBJ_H
#define RTRVRFPOBJ_H

#include <atomic>
#include <string>
#include "ProcObject.h"
#include "RtrCommon.h"

struct RtrVrfConfig {
    std::string nsName;           ///< from NODE_PATH
    std::string vrfName;          ///< vrf instance name; "default" = no device
    uint32_t    tableId   = 0;    ///< 0 = auto-assign
    bool        shutdown  = false;
};

struct RtrVrfStatus {
    ProcObject::ObjStatus objStatus  = ProcObject::ObjStatus::IDLE;
    RtrOperStatus         operStatus = RtrOperStatus::LLD;
};

class RtrVrfPObj : public ProcObject {
public:
    ~RtrVrfPObj() override = default;

    void init(EvApplication& loop, LogManager& mgr,
              const std::string& appTag) override;
    bool loadConfig(IniConfig& ini, const std::string& section) override;
    void process() override;

    void        syncSnapshot() override;
    std::string buildStatusJson() const override;

private:
    RtrVrfConfig  config_{};
    RtrVrfStatus  status_{};

    std::atomic<int> snapIdx_{0};
    RtrVrfStatus     statusSnap_[2]{};

    int monitorTick_ = 0;
    static constexpr int MONITOR_INTERVAL = 5;

    void    applyConfig();
    void    monitorVrf();
    bool    vrfDeviceExists() const;
    uint32_t autoTableId() const;
};

#endif // RTRVRFPOBJ_H
