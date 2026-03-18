/**
 * @file    RtrCommon.h
 * @brief   Shared types and inline utilities for XCESP-ON-RTR objects
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#ifndef RTRCOMMON_H
#define RTRCOMMON_H

#include <string>
#include <utility>
#include <vector>
#include "ProcObject.h"

// ---------------------------------------------------------------------------
// OperStatus — reported in buildStatusJson() and status-to-global rules
// ---------------------------------------------------------------------------

enum class RtrOperStatus {
    UP,    ///< Deployed without errors; for iface/vlan: link is UP
    DOWN,  ///< Deployed without errors; for iface/vlan: link is DOWN
           ///<   or object is administratively shutdown
    LLD    ///< Low-Level-Down: linux command failed / device unavailable
};

inline const char* operStatusStr(RtrOperStatus s)
{
    switch (s) {
        case RtrOperStatus::UP:   return "UP";
        case RtrOperStatus::DOWN: return "DOWN";
        case RtrOperStatus::LLD:  return "LLD";
    }
    return "LLD";
}

// ---------------------------------------------------------------------------
// parseNsVrf — extract namespace and VRF names from NODE_PATH
//
// NODE_PATH format injected by xcespmap:
//   router/<ns>/vrf/<vrf>/interface/<name>
//   router/<ns>/vrf/<vrf>/vlan/<name>
//   router/<ns>/vrf/<vrf>/static-route/<name>
//   router/<ns>/vrf/<vrf>
//   router/<ns>
//
// Returns {"default","default"} if segments are not present.
// ---------------------------------------------------------------------------

inline std::pair<std::string, std::string> parseNsVrf(const std::string& path)
{
    std::vector<std::string> parts;
    std::string seg;
    for (char c : path) {
        if (c == '/') {
            if (!seg.empty()) { parts.push_back(seg); seg.clear(); }
        } else {
            seg += c;
        }
    }
    if (!seg.empty()) parts.push_back(seg);

    std::string ns  = (parts.size() > 1) ? parts[1] : "default";
    std::string vrf = (parts.size() > 3) ? parts[3] : "default";
    return {ns, vrf};
}

// ---------------------------------------------------------------------------
// splitCsv — split a comma-separated string into a vector of tokens
// ---------------------------------------------------------------------------

inline std::vector<std::string> splitCsv(const std::string& csv)
{
    std::vector<std::string> result;
    if (csv.empty()) return result;
    std::string tok;
    for (char c : csv) {
        if (c == ',') {
            if (!tok.empty()) { result.push_back(tok); tok.clear(); }
        } else {
            tok += c;
        }
    }
    if (!tok.empty()) result.push_back(tok);
    return result;
}

#endif // RTRCOMMON_H
