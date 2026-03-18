#!/usr/bin/env python3
"""
config-to-objects/interface.py — XCESP-ON-RTR
Maps an 'interface' config node to one RtrInterface xcespproc object.

Multiple IP addresses are collected from child 'address' nodes whose
instance names carry the IP/prefix (e.g. "10.10.10.10/24").
Namespace and VRF are derived from NODE_PATH injected by xcespmap.

IFACE_TYPE values:
  device   (default) — existing Linux device; check existence, sync IPs
  loopback           — dummy interface (or standard 'lo'); create if needed
"""


def run(data):
    attrs = data["attrs"]
    addresses = ",".join(
        a["instance_name"]
        for a in data.get("children", {}).get("address", [])
    )
    return [
        {
            "section": f"object.iface-{data['instance_name']}",
            "attrs": {
                "TYPE":       "RtrInterface",
                "NAME":       data["instance_name"],
                "DEVICE":     attrs.get("device", ""),
                "IFACE_TYPE": attrs.get("type", "device"),
                "ADDRESSES":  addresses,
                "SHUTDOWN":   attrs.get("shutdown", "false"),
            },
        }
    ]
