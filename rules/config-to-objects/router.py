#!/usr/bin/env python3
"""
config-to-objects/router.py — XCESP-ON-RTR
Maps a 'router' config node to one RtrRouter xcespproc object.

The router instance name becomes the Linux network namespace name.
"default" is the host namespace (no namespace is created/managed).
"""


def run(data):
    return [
        {
            "section": f"object.router-{data['instance_name']}",
            "attrs": {
                "TYPE":      "RtrRouter",
                "NAME":      data["instance_name"],
                "NAMESPACE": data["instance_name"],
                "SHUTDOWN":  data["attrs"].get("shutdown", "false"),
            },
        }
    ]
