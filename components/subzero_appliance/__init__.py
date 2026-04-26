"""
Native ESPHome component for Sub-Zero / Wolf / Cove BLE appliances.

User YAML shape:

    external_components:
      - source: github://JonGilmore/esphome-subzero-ble@main
        components: [patch_acl_reassembly, subzero_protocol, subzero_appliance]

    ble_client:
      - mac_address: !secret main_fridge_mac
        id: main_fridge_ble
        name: "SZG Main Fridge"
        auto_connect: true

    subzero_appliance:
      - type: fridge
        id: main_fridge
        ble_client_id: main_fridge_ble
        name: "Main Fridge"
        pin: !secret main_fridge_pin
        # type-specific:
        hide_freezer: true
        hide_ice_maker: true

The component generates ~30 sensors per appliance type, the PIN text
input, the debug-mode switch, the status text sensor, and seven
control buttons (Connect / Start Pairing / Submit PIN & Unlock / Poll /
Log Debug Info / Disconnect / Reset Pairing). Multiple appliances can
coexist on one ESP — the component is MULTI_CONF.
"""

from __future__ import annotations

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.components import (
    binary_sensor,
    ble_client,
    button,
    sensor,
    switch,
    text,
    text_sensor,
)
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MODE,
    CONF_NAME,
    CONF_PIN,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

CODEOWNERS = ["@JonGilmore"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = [
    "binary_sensor",
    "button",
    "json",
    "sensor",
    "subzero_protocol",
    "switch",
    "text",
    "text_sensor",
]
MULTI_CONF = True

subzero_appliance_ns = cg.esphome_ns.namespace("subzero_appliance")

ApplianceBase = subzero_appliance_ns.class_(
    "ApplianceBase", cg.Component, ble_client.BLEClientNode
)
FridgeAppliance = subzero_appliance_ns.class_("FridgeAppliance", ApplianceBase)
DishwasherAppliance = subzero_appliance_ns.class_("DishwasherAppliance", ApplianceBase)
RangeAppliance = subzero_appliance_ns.class_("RangeAppliance", ApplianceBase)

ApplianceButton = subzero_appliance_ns.class_("ApplianceButton", button.Button)
ApplianceDebugSwitch = subzero_appliance_ns.class_(
    "ApplianceDebugSwitch", switch.Switch
)
AppliancePinText = subzero_appliance_ns.class_("AppliancePinText", text.Text)
ApplianceButtonKind = subzero_appliance_ns.enum("ApplianceButtonKind", is_class=True)

CONF_PIN_INPUT = "pin_input_id"
CONF_DEBUG_SWITCH = "debug_switch_id"
CONF_STATUS = "status_id"
CONF_POLL_OFFSET = "poll_offset"

# ----------------------------------------------------------------------
# Sensor descriptors — compact form so we can iterate and generate via
# new_X factories. Each entry is (suffix, name_suffix, kwargs_for_new_X).
# `setter` is the C++ method called on the appliance to wire the sensor
# pointer.
# ----------------------------------------------------------------------

UNIT_FAHRENHEIT = "°F"

# Common (appliance-agnostic) — all three types get these.
COMMON_BINARY_SENSORS = [
    # (suffix, friendly-name suffix, setter, kwargs)
    ("svc_required", "Service Required", "set_svc_required_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM}),
]

COMMON_TEXT_SENSORS = [
    ("model", "Model", "set_model_sensor",
     {CONF_ICON: "mdi:devices", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("uptime", "Uptime", "set_uptime_sensor",
     {CONF_ICON: "mdi:timer-outline", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("serial", "Appliance Serial", "set_serial_sensor",
     {CONF_ICON: "mdi:identifier", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("appliance_type", "Appliance Type", "set_appliance_type_sensor",
     {CONF_ICON: "mdi:shape", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("diag_status", "Diagnostic Status", "set_diag_status_sensor",
     {CONF_ICON: "mdi:stethoscope", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("fw_version", "Firmware Version", "set_fw_version_sensor",
     {CONF_ICON: "mdi:chip", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("api_version", "API Version", "set_api_version_sensor",
     {CONF_ICON: "mdi:api", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("bleapp_version", "BLE App Version", "set_bleapp_version_sensor",
     {CONF_ICON: "mdi:bluetooth", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("os_version", "OS Version", "set_os_version_sensor",
     {CONF_ICON: "mdi:memory", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("rtapp_version", "RTApp Version", "set_rtapp_version_sensor",
     {CONF_ICON: "mdi:application-cog", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("board_version", "Appliance Board Version", "set_board_version_sensor",
     {CONF_ICON: "mdi:developer-board", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
    ("build_date", "Build Date", "set_build_date_sensor",
     {CONF_ICON: "mdi:calendar-clock", CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC}),
]

# Buttons — same 7 across all appliance types.
BUTTON_DEFINITIONS = [
    # (kind, friendly-name format string {name})
    ("kConnect", "Connect to {name}", "mdi:bluetooth-connect", None),
    ("kStartPairing", "{name} Start Pairing", "mdi:key-plus", None),
    ("kSubmitPin", "{name} Submit PIN & Unlock", "mdi:lock-open-variant", None),
    ("kPoll", "Poll {name}", "mdi:refresh", None),
    ("kLogDebugInfo", "{name} Log Debug Info", "mdi:bug-play", ENTITY_CATEGORY_DIAGNOSTIC),
    ("kDisconnect", "Disconnect {name}", "mdi:bluetooth-off", None),
    ("kResetPairing", "Reset {name} Pairing", "mdi:bluetooth-settings", ENTITY_CATEGORY_CONFIG),
]

# Per-type sensor descriptors. Each entry: (suffix, name_suffix, setter, kwargs, hide_key_or_None).
# `hide_key_or_None` when set means the sensor's `internal:` flag mirrors the
# user's `hide_X: true|false` config (default true → hidden).

FRIDGE_BINARY_SENSORS = [
    ("door_ajar", "Door", "set_door_ajar_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, None),
    ("sabbath_on", "Sabbath Mode", "set_sabbath_on_sensor",
     {CONF_ICON: "mdi:candelabra"}, "hide_sabbath"),
    ("frz_door_ajar", "Freezer Door", "set_frz_door_ajar_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, "hide_freezer"),
    ("ice_maker", "Ice Maker", "set_ice_maker_sensor",
     {CONF_ICON: "mdi:ice-cream"}, "hide_ice_maker"),
    ("ref2_door_ajar", "Refrigerator Drawer Door", "set_ref2_door_ajar_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, "hide_ref_drawer"),
    ("wine_door_ajar", "Wine Door", "set_wine_door_ajar_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, "hide_wine"),
    ("wine_temp_alert", "Wine Temperature Alert", "set_wine_temp_alert_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM}, "hide_wine"),
    ("air_filter_on", "Air Filter", "set_air_filter_on_sensor",
     {CONF_ICON: "mdi:air-filter"}, "hide_air_filter"),
]

TEMP_KWARGS = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_FAHRENHEIT,
    CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
    "state_class": STATE_CLASS_MEASUREMENT,
    "accuracy_decimals": 0,
}

FRIDGE_SENSORS = [
    ("set_temp", "Set Temperature", "set_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer"}, None),
    ("frz_set_temp", "Freezer Set Temperature", "set_frz_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:snowflake-thermometer"}, "hide_freezer"),
    ("wine_set_temp", "Wine Zone Upper Set Temperature", "set_wine_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:glass-wine"}, "hide_wine"),
    ("wine2_set_temp", "Wine Zone Lower Set Temperature", "set_wine2_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:glass-wine"}, "hide_wine"),
    ("ref2_set_temp", "Refrigerator Drawer Set Temperature", "set_ref2_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer"}, "hide_ref_drawer"),
    ("crisp_set_temp", "Crisper Drawer Set Temperature", "set_crisp_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer"}, "hide_crisper"),
    ("air_filter_pct", "Air Filter Remaining", "set_air_filter_pct_sensor",
     {CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT, "state_class": STATE_CLASS_MEASUREMENT,
      "accuracy_decimals": 0, CONF_ICON: "mdi:air-filter"}, "hide_air_filter"),
    ("water_filter_pct", "Water Filter Remaining", "set_water_filter_pct_sensor",
     {CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT, "state_class": STATE_CLASS_MEASUREMENT,
      "accuracy_decimals": 0, CONF_ICON: "mdi:water-percent"}, "hide_water_filter"),
]

DISHWASHER_BINARY_SENSORS = [
    ("door_ajar", "Door", "set_door_ajar_sensor", {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, None),
    ("wash_cycle_on", "Wash Cycle Active", "set_wash_cycle_on_sensor",
     {CONF_ICON: "mdi:dishwasher"}, None),
    ("heated_dry", "Heated Dry", "set_heated_dry_sensor", {CONF_ICON: "mdi:heat-wave"}, None),
    ("extended_dry", "Extended Dry", "set_extended_dry_sensor", {CONF_ICON: "mdi:heat-wave"}, None),
    ("high_temp_wash", "High Temp Wash", "set_high_temp_wash_sensor",
     {CONF_ICON: "mdi:thermometer-high"}, None),
    ("sani_rinse", "Sanitize Rinse", "set_sani_rinse_sensor", {CONF_ICON: "mdi:hand-wash"}, None),
    ("rinse_aid_low", "Rinse Aid Low", "set_rinse_aid_low_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM}, None),
    ("softener_low", "Softener Low", "set_softener_low_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM}, "hide_softener"),
    ("light_on", "Light", "set_light_on_sensor", {CONF_ICON: "mdi:lightbulb"}, None),
    ("remote_ready", "Remote Ready", "set_remote_ready_sensor", {CONF_ICON: "mdi:remote"}, None),
    ("delay_start", "Delay Start", "set_delay_start_sensor", {CONF_ICON: "mdi:timer-sand"}, None),
]

DISHWASHER_SENSORS = [
    ("wash_status", "Wash Status", "set_wash_status_sensor",
     {CONF_ICON: "mdi:dishwasher", "accuracy_decimals": 0}, None),
    ("wash_cycle", "Wash Cycle", "set_wash_cycle_sensor",
     {CONF_ICON: "mdi:counter", "accuracy_decimals": 0}, None),
    ("wash_time_remaining", "Wash Time Remaining", "set_wash_time_remaining_sensor",
     {CONF_ICON: "mdi:timer-sand", CONF_UNIT_OF_MEASUREMENT: "min",
      "accuracy_decimals": 0, CONF_DEVICE_CLASS: DEVICE_CLASS_DURATION}, None),
]

DISHWASHER_TEXT_SENSORS = [
    ("wash_cycle_end_time", "Wash Cycle End Time", "set_wash_cycle_end_time_sensor",
     {CONF_ICON: "mdi:clock-end"}, None),
]

RANGE_BINARY_SENSORS = [
    ("door_ajar", "Door", "set_door_ajar_sensor", {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, None),
    ("sabbath_on", "Sabbath Mode", "set_sabbath_on_sensor", {CONF_ICON: "mdi:candelabra"}, None),
    ("cav_unit_on", "Oven", "set_cav_unit_on_sensor", {CONF_ICON: "mdi:stove"}, None),
    ("cav_at_set_temp", "Oven At Temperature", "set_cav_at_set_temp_sensor",
     {CONF_ICON: "mdi:thermometer-check"}, None),
    ("cav_light_on", "Oven Light", "set_cav_light_on_sensor", {CONF_ICON: "mdi:lightbulb"}, None),
    ("cav_remote_ready", "Oven Remote Ready", "set_cav_remote_ready_sensor",
     {CONF_ICON: "mdi:remote"}, None),
    ("cav_probe_on", "Probe Inserted", "set_cav_probe_on_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, None),
    ("cav_probe_at_temp", "Probe At Temperature", "set_cav_probe_at_temp_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, None),
    ("cav_probe_near", "Probe Within 10°", "set_cav_probe_near_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, None),
    ("cav_gourmet", "Gourmet Mode", "set_cav_gourmet_sensor", {CONF_ICON: "mdi:chef-hat"}, None),
    ("cook_timer_done", "Cook Timer Complete", "set_cook_timer_done_sensor",
     {CONF_ICON: "mdi:timer-alert"}, None),
    ("cook_timer_near", "Cook Timer Within 1 Min", "set_cook_timer_near_sensor",
     {CONF_ICON: "mdi:timer-alert-outline"}, None),
    ("ktimer_active", "Kitchen Timer Active", "set_ktimer_active_sensor",
     {CONF_ICON: "mdi:timer"}, None),
    ("ktimer_done", "Kitchen Timer Complete", "set_ktimer_done_sensor",
     {CONF_ICON: "mdi:timer-alert"}, None),
    ("ktimer_near", "Kitchen Timer Within 1 Min", "set_ktimer_near_sensor",
     {CONF_ICON: "mdi:timer-alert-outline"}, None),
    ("ktimer2_active", "Kitchen Timer 2 Active", "set_ktimer2_active_sensor",
     {CONF_ICON: "mdi:timer"}, None),
    ("ktimer2_done", "Kitchen Timer 2 Complete", "set_ktimer2_done_sensor",
     {CONF_ICON: "mdi:timer-alert"}, None),
    ("ktimer2_near", "Kitchen Timer 2 Within 1 Min", "set_ktimer2_near_sensor",
     {CONF_ICON: "mdi:timer-alert-outline"}, None),
    # Oven 2 (dual-oven)
    ("cav2_unit_on", "Oven 2", "set_cav2_unit_on_sensor",
     {CONF_ICON: "mdi:stove"}, "hide_oven2"),
    ("cav2_door_ajar", "Oven 2 Door", "set_cav2_door_ajar_sensor",
     {CONF_DEVICE_CLASS: DEVICE_CLASS_DOOR}, "hide_oven2"),
    ("cav2_at_set_temp", "Oven 2 At Temperature", "set_cav2_at_set_temp_sensor",
     {CONF_ICON: "mdi:thermometer-check"}, "hide_oven2"),
    ("cav2_light_on", "Oven 2 Light", "set_cav2_light_on_sensor",
     {CONF_ICON: "mdi:lightbulb"}, "hide_oven2"),
    ("cav2_remote_ready", "Oven 2 Remote Ready", "set_cav2_remote_ready_sensor",
     {CONF_ICON: "mdi:remote"}, "hide_oven2"),
    ("cav2_probe_on", "Oven 2 Probe Inserted", "set_cav2_probe_on_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, "hide_oven2"),
    ("cav2_probe_at_temp", "Oven 2 Probe At Temperature", "set_cav2_probe_at_temp_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, "hide_oven2"),
    ("cav2_probe_near", "Oven 2 Probe Within 10°", "set_cav2_probe_near_sensor",
     {CONF_ICON: "mdi:thermometer-probe"}, "hide_oven2"),
    ("cav2_gourmet", "Oven 2 Gourmet Mode", "set_cav2_gourmet_sensor",
     {CONF_ICON: "mdi:chef-hat"}, "hide_oven2"),
    ("cav2_cook_timer_done", "Oven 2 Cook Timer Complete", "set_cav2_cook_timer_done_sensor",
     {CONF_ICON: "mdi:timer-alert"}, "hide_oven2"),
]

RANGE_SENSORS = [
    ("cav_temp", "Oven Temperature", "set_cav_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer"}, None),
    ("cav_set_temp", "Oven Set Temperature", "set_cav_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-check"}, None),
    ("cav_cook_mode", "Cook Mode", "set_cav_cook_mode_sensor",
     {CONF_ICON: "mdi:stove", "accuracy_decimals": 0}, None),
    ("cav_gourmet_recipe", "Gourmet Recipe", "set_cav_gourmet_recipe_sensor",
     {CONF_ICON: "mdi:chef-hat", "accuracy_decimals": 0}, None),
    ("probe_temp", "Probe Temperature", "set_probe_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-probe"}, None),
    ("probe_set_temp", "Probe Set Temperature", "set_probe_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-probe-off"}, None),
    ("cav2_temp", "Oven 2 Temperature", "set_cav2_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer"}, "hide_oven2"),
    ("cav2_set_temp", "Oven 2 Set Temperature", "set_cav2_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-check"}, "hide_oven2"),
    ("cav2_cook_mode", "Oven 2 Cook Mode", "set_cav2_cook_mode_sensor",
     {CONF_ICON: "mdi:stove", "accuracy_decimals": 0}, "hide_oven2"),
    ("cav2_probe_temp", "Oven 2 Probe Temperature", "set_cav2_probe_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-probe"}, "hide_oven2"),
    ("cav2_probe_set_temp", "Oven 2 Probe Set Temperature", "set_cav2_probe_set_temp_sensor",
     {**TEMP_KWARGS, CONF_ICON: "mdi:thermometer-probe-off"}, "hide_oven2"),
]

RANGE_TEXT_SENSORS = [
    ("ktimer_end_time", "Kitchen Timer End Time", "set_ktimer_end_time_sensor",
     {CONF_ICON: "mdi:clock-end"}, None),
    ("ktimer2_end_time", "Kitchen Timer 2 End Time", "set_ktimer2_end_time_sensor",
     {CONF_ICON: "mdi:clock-end"}, None),
]

# ----------------------------------------------------------------------
# Schema
# ----------------------------------------------------------------------

# Per-type optional config keys (the hide_X flags etc.).
TYPE_SCHEMAS = {
    "fridge": {
        cv.Optional("hide_freezer", default=False): cv.boolean,
        cv.Optional("hide_ice_maker", default=False): cv.boolean,
        cv.Optional("hide_sabbath", default=False): cv.boolean,
        cv.Optional("hide_wine", default=True): cv.boolean,
        cv.Optional("hide_ref_drawer", default=True): cv.boolean,
        cv.Optional("hide_crisper", default=True): cv.boolean,
        cv.Optional("hide_air_filter", default=True): cv.boolean,
        cv.Optional("hide_water_filter", default=True): cv.boolean,
    },
    "dishwasher": {
        cv.Optional("hide_softener", default=True): cv.boolean,
    },
    "range": {
        cv.Optional("hide_oven2", default=True): cv.boolean,
    },
}

TYPE_TO_CLASS = {
    "fridge": FridgeAppliance,
    "dishwasher": DishwasherAppliance,
    "range": RangeAppliance,
}


def _schema_for_type(type_: str) -> cv.Schema:
    base = {
        cv.GenerateID(): cv.declare_id(TYPE_TO_CLASS[type_]),
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_PIN): cv.string,
        cv.Optional(CONF_POLL_OFFSET, default="0s"): cv.positive_time_period_milliseconds,
    }
    base.update(TYPE_SCHEMAS[type_])
    return (
        cv.Schema(base)
        .extend(ble_client.BLE_CLIENT_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA)
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        type_: _schema_for_type(type_) for type_ in TYPE_TO_CLASS
    },
    key=CONF_TYPE,
)


# ----------------------------------------------------------------------
# Codegen helpers
# ----------------------------------------------------------------------


def _entity_id(parent_id: core.ID, suffix: str, type_: type) -> core.ID:
    return core.ID(f"{parent_id.id}_{suffix}", is_declaration=True, type=type_)


# ESPHome's `new_X` factory functions expect configs that have been
# through their schema validator (so default keys like
# `disabled_by_default`, `internal`, `id` are present). Hand-built dicts
# crash. We construct minimal dicts and run them through the schema to
# get a complete, validated config.

def _validate_sensor(cfg):
    return sensor.sensor_schema()(cfg)


def _validate_binary_sensor(cfg):
    return binary_sensor.binary_sensor_schema()(cfg)


def _validate_text_sensor(cfg):
    return text_sensor.text_sensor_schema()(cfg)


def _build_sensor_config(parent_id, suffix, name_suffix, kwargs, hidden):
    cfg = {
        CONF_ID: _entity_id(parent_id, suffix, sensor.Sensor),
        CONF_NAME: f"{name_suffix}",
    }
    cfg.update(kwargs)
    if hidden:
        cfg[CONF_INTERNAL] = True
    return _validate_sensor(cfg)


def _build_binary_sensor_config(parent_id, suffix, name_suffix, kwargs, hidden):
    cfg = {
        CONF_ID: _entity_id(parent_id, suffix, binary_sensor.BinarySensor),
        CONF_NAME: f"{name_suffix}",
    }
    cfg.update(kwargs)
    if hidden:
        cfg[CONF_INTERNAL] = True
    return _validate_binary_sensor(cfg)


def _build_text_sensor_config(parent_id, suffix, name_suffix, kwargs, hidden):
    cfg = {
        CONF_ID: _entity_id(parent_id, suffix, text_sensor.TextSensor),
        CONF_NAME: f"{name_suffix}",
    }
    cfg.update(kwargs)
    if hidden:
        cfg[CONF_INTERNAL] = True
    return _validate_text_sensor(cfg)


def _resolve_hidden(config, hide_key):
    """Returns True if the user wants this sensor hidden via the per-type
    hide_X flag. None means "always shown"."""
    if hide_key is None:
        return False
    return bool(config.get(hide_key, False))


# ----------------------------------------------------------------------
# to_code
# ----------------------------------------------------------------------


async def to_code(config):
    parent_id = config[CONF_ID]
    type_ = config[CONF_TYPE]
    name = config[CONF_NAME]

    var = cg.new_Pvariable(parent_id)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_appliance_name(name))
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_poll_offset_ms(config[CONF_POLL_OFFSET]))

    # ---- Status text sensor ----
    status_cfg = _validate_text_sensor({
        CONF_ID: _entity_id(parent_id, "status", text_sensor.TextSensor),
        CONF_NAME: f"{name} Status",
    })
    status_var = await text_sensor.new_text_sensor(status_cfg)
    cg.add(var.set_status_text_sensor(status_var))

    # ---- Common binary sensors ----
    for suffix, name_suffix, setter, kwargs in COMMON_BINARY_SENSORS:
        cfg = _build_binary_sensor_config(parent_id, suffix, f"{name} {name_suffix}", kwargs, False)
        bs = await binary_sensor.new_binary_sensor(cfg)
        cg.add(getattr(var, setter)(bs))

    # ---- Common text sensors ----
    for suffix, name_suffix, setter, kwargs in COMMON_TEXT_SENSORS:
        cfg = _build_text_sensor_config(parent_id, suffix, f"{name} {name_suffix}", kwargs, False)
        ts = await text_sensor.new_text_sensor(cfg)
        cg.add(getattr(var, setter)(ts))

    # ---- Type-specific entities ----
    if type_ == "fridge":
        bs_list = FRIDGE_BINARY_SENSORS
        s_list = FRIDGE_SENSORS
        ts_list = []
    elif type_ == "dishwasher":
        bs_list = DISHWASHER_BINARY_SENSORS
        s_list = DISHWASHER_SENSORS
        ts_list = DISHWASHER_TEXT_SENSORS
    else:  # range
        bs_list = RANGE_BINARY_SENSORS
        s_list = RANGE_SENSORS
        ts_list = RANGE_TEXT_SENSORS

    for suffix, name_suffix, setter, kwargs, hide_key in bs_list:
        cfg = _build_binary_sensor_config(
            parent_id, suffix, f"{name} {name_suffix}", kwargs,
            _resolve_hidden(config, hide_key),
        )
        bs = await binary_sensor.new_binary_sensor(cfg)
        cg.add(getattr(var, setter)(bs))

    for suffix, name_suffix, setter, kwargs, hide_key in s_list:
        cfg = _build_sensor_config(
            parent_id, suffix, f"{name} {name_suffix}", kwargs,
            _resolve_hidden(config, hide_key),
        )
        s = await sensor.new_sensor(cfg)
        cg.add(getattr(var, setter)(s))

    for suffix, name_suffix, setter, kwargs, hide_key in ts_list:
        cfg = _build_text_sensor_config(
            parent_id, suffix, f"{name} {name_suffix}", kwargs,
            _resolve_hidden(config, hide_key),
        )
        ts = await text_sensor.new_text_sensor(cfg)
        cg.add(getattr(var, setter)(ts))

    # ---- PIN text input ----
    # esphome::text::Text is abstract (control() is pure virtual); use
    # our AppliancePinText concrete subclass so the new() expression in
    # the generated main.cpp resolves.
    pin_cfg_raw = {
        CONF_ID: _entity_id(parent_id, "pin_input", AppliancePinText),
        CONF_NAME: f"{name} PIN",
        CONF_ICON: "mdi:key-variant",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        # PIN entries are masked in the UI (PASSWORD mode). Sub-Zero PINs
        # aren't high-security, but plaintext display in HA dashboards is
        # gratuitous — mask is the obvious default for a key-icon field.
        CONF_MODE: "password",
    }
    pin_cfg = text.text_schema(AppliancePinText)(pin_cfg_raw)
    pin_var = await text.new_text(pin_cfg, min_length=0, max_length=10)
    cg.add(pin_var.set_parent(var))
    cg.add(var.set_pin_input(pin_var))

    # ---- Debug switch ----
    debug_sw_cfg_raw = {
        CONF_ID: _entity_id(parent_id, "debug_switch", ApplianceDebugSwitch),
        CONF_NAME: f"{name} Debug Mode",
        CONF_ICON: "mdi:bug",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    }
    debug_sw_cfg = switch.switch_schema(ApplianceDebugSwitch)(debug_sw_cfg_raw)
    debug_sw = await switch.new_switch(debug_sw_cfg)
    cg.add(debug_sw.set_parent(var))

    # ---- Buttons (7) ----
    for kind_name, name_fmt, icon, entity_category in BUTTON_DEFINITIONS:
        btn_cfg_raw = {
            CONF_ID: _entity_id(
                parent_id,
                f"btn_{kind_name.lstrip('k').lower()}",
                ApplianceButton,
            ),
            CONF_NAME: name_fmt.format(name=name),
            CONF_ICON: icon,
        }
        if entity_category is not None:
            btn_cfg_raw[CONF_ENTITY_CATEGORY] = entity_category
        btn_cfg = button.button_schema(ApplianceButton)(btn_cfg_raw)
        btn_var = await button.new_button(btn_cfg)
        cg.add(btn_var.set_parent(var))
        cg.add(btn_var.set_kind(getattr(ApplianceButtonKind, kind_name)))
