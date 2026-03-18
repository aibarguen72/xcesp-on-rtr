#!/usr/bin/env python3
"""
config-to-objects/static-route.py — XCESP-ON-RTR
Maps a 'static-route' config node to one RtrStaticRoute xcespproc object.

VRF_TABLE is populated from the parent vrf's 'table' attr when present,
allowing RtrStaticRoutePObj to add the route to the correct routing table.
"""


def run(data):
    attrs = data["attrs"]

    obj_attrs = {
        "TYPE":     "RtrStaticRoute",
        "NAME":     data["instance_name"],
        "NETWORK":  attrs.get("network", ""),
        "NEXTHOP":  attrs.get("next-hop", ""),
        "METRIC":   attrs.get("metric", "1"),
        "SHUTDOWN": attrs.get("shutdown", "false"),
    }

    # Propagate parent VRF table number (needed for VRF-aware route injection)
    vrf_table = data.get("parent", {}).get("attrs", {}).get("table", "")
    if vrf_table:
        obj_attrs["VRF_TABLE"] = vrf_table

    return [
        {
            "section": f"object.route-{data['instance_name']}",
            "attrs": obj_attrs,
        }
    ]
