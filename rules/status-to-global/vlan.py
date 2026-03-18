#!/usr/bin/env python3
"""
status-to-global/vlan.py — XCESP-ON-RTR
Maps RtrVlan object status to global path/value pairs.
Emits oper-status and link-state.
"""


def run(data):
    result = []
    for obj in data.get("objects", []):
        path = (obj.get("node_path")
                or f"{obj.get('node_type','vlan')}/{obj.get('node_instance','?')}")
        result.append({"path": f"{path}/oper-status",
                        "value": obj.get("oper_status", "LLD")})
        result.append({"path": f"{path}/link-state",
                        "value": obj.get("link_state", "DOWN")})
    return result
