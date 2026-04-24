"""
Pure C++ JSON parser for Sub-Zero / Wolf / Cove appliance BLE messages.

The parser is factored out of the YAML lambdas so its behavior can be
unit-tested on the host (see tests/cpp/). The lambdas in subzero_fridge.yaml,
subzero_dishwasher.yaml, and subzero_range.yaml call into this component
and publish the returned struct fields to ESPHome entities.
"""

import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@JonGilmore"]
DEPENDENCIES = ["json"]

subzero_protocol_ns = cg.esphome_ns.namespace("subzero_protocol")

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # Header is auto-picked up by esphome.h generation (writer.py scans every
    # registered component dir for .h files), so no add_global needed here.
    pass
