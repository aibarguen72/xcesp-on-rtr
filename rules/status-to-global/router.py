#!/usr/bin/env python3
"""
status-to-global/router.py — XCESP-ON-RTR
Maps RtrRouter object status to global path/value pairs.
"""


def run(data):
    result = []
    for obj in data.get("objects", []):
        path = (obj.get("node_path")
                or f"{obj.get('node_type','router')}/{obj.get('node_instance','?')}")
        result.append({"path": f"{path}/oper-status",
                        "value": obj.get("oper_status", "LLD")})
    return result
