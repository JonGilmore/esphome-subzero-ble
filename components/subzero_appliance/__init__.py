"""
SubzeroHub — the BLE connection state machine, lifted out of YAML scripts
into a host-testable C++ class. See components/subzero_appliance/hub.h
for the architecture.

This component (PR-A / Phase 3) only registers the namespace and pulls
in the headers via ESPHome's auto-include of every .h file in a
registered component dir. The user's YAML still declares the hub +
transport + scheduler as globals and wires them up in on_boot. PR-B
(Phase 4) will add a config schema that does this codegen automatically.
"""

import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@JonGilmore"]
DEPENDENCIES = ["ble_client", "json", "subzero_protocol"]

subzero_appliance_ns = cg.esphome_ns.namespace("subzero_appliance")

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
