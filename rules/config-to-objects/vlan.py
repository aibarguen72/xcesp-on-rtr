#!/usr/bin/env python3
"""
config-to-objects/vlan.py — XCESP-ON-RTR
Maps a 'vlan' config node to one RtrVlan xcespproc object.

The parent device name is looked up from the parent vrf node's interface
children, matching by the 'interface' attr value.  Multiple IP addresses
are collected from child 'address' nodes.
"""


def run(data):
    attrs = data["attrs"]

    # Resolve parent device name from the vrf's interface list
    parent_if_name = attrs.get("interface", "")
    parent_device = ""
    for iface in data.get("parent", {}).get("children", {}).get("interface", []):
        if iface["instance_name"] == parent_if_name:
            parent_device = iface["attrs"].get("device", "")
            break

    addresses = ",".join(
        a["instance_name"]
        for a in data.get("children", {}).get("address", [])
    )
    return [
        {
            "section": f"object.vlan-{data['instance_name']}",
            "attrs": {
                "TYPE":          "RtrVlan",
                "NAME":          data["instance_name"],
                "PARENT_DEVICE": parent_device,
                "VLAN_ID":       attrs.get("vlanid", "0"),
                "ADDRESSES":     addresses,
                "SHUTDOWN":      attrs.get("shutdown", "false"),
            },
        }
    ]
