#!/usr/bin/env python3
"""
config-to-objects/vrf.py — XCESP-ON-RTR
Maps a 'vrf' config node to one RtrVrf xcespproc object.

The vrf instance name becomes the Linux VRF device name.
"default" is the default routing table (no VRF device is created).
An optional TABLE attr overrides the auto-assigned routing table number.
"""


def run(data):
    attrs = data["attrs"]
    obj_attrs = {
        "TYPE":     "RtrVrf",
        "NAME":     data["instance_name"],
        "SHUTDOWN": attrs.get("shutdown", "false"),
    }
    if "table" in attrs:
        obj_attrs["TABLE"] = attrs["table"]
    return [
        {
            "section": f"object.vrf-{data['instance_name']}",
            "attrs": obj_attrs,
        }
    ]
