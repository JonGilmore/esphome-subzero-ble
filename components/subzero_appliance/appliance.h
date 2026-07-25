#pragma once

// Phase 4: ESPHome-native appliance components.
//
// `FridgeAppliance`, `DishwasherAppliance`, and `RangeAppliance` are
// `Component + BLEClientNode` subclasses that own a typed `*Hub` (the
// host-testable state machine from Phase 3) and wire it into ESPHome's
// runtime — gattc events route to hub methods, setup() does the boot-
// time wiring, set_interval() drives the 60s periodic poll, and a
// `*Bus` of sensor pointers is populated via setters called from the
// Python codegen.
//
// On-device only.

#ifdef USE_ESP32

#include "appliance_base.h"
#include "dishwasher_hub.h"
#include "fridge_hub.h"
#include "range_hub.h"

#include "../subzero_protocol/dispatch_esphome.h"

namespace esphome {
namespace subzero_appliance {

// Each per-type appliance class is mostly setters that fill in the
// appropriate Bus, plus the `wire_bus_()` override that hands the bus
// pointer to the hub. The base class (appliance_base.h) handles the
// rest: setup(), gattc/gap event routing, button actions, and all the
// CommonBus setters (model, uptime, version fields, etc.).

class FridgeAppliance : public ApplianceBase {
public:
  FridgeAppliance() = default;

  // Fridge-specific entity setters
  void set_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.door_ajar = s;
  }
  void set_frz_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.frz_door_ajar = s;
  }
  void set_ice_maker_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ice_maker = s;
  }
  void set_ref2_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ref2_door_ajar = s;
  }
  void set_wine_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.wine_door_ajar = s;
  }
  void set_wine_temp_alert_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.wine_temp_alert = s;
  }
  void set_air_filter_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.air_filter_on = s;
  }

  // Set-temps are read-only Sensors by default; see FridgeBus comment in
  // dispatch_esphome.h. When `enable_temp_control: true`, the codegen
  // instead calls the `_number` setter below with a writable Number.
  void set_set_temp_sensor(esphome::sensor::Sensor *s) { bus_.set_temp = s; }
  void set_set_temp_number(esphome::number::Number *n) {
    bus_.set_temp_number = n;
  }
  void set_frz_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.frz_set_temp = s;
  }
  void set_frz_set_temp_number(esphome::number::Number *n) {
    bus_.frz_set_temp_number = n;
  }
  void set_ref2_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.ref2_set_temp = s;
  }
  void set_wine_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.wine_set_temp = s;
  }
  void set_wine2_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.wine2_set_temp = s;
  }
  void set_crisp_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.crisp_set_temp = s;
  }
  void set_crisp_set_temp_number(esphome::number::Number *n) {
    bus_.crisp_set_temp_number = n;
  }
  void set_air_filter_pct_sensor(esphome::sensor::Sensor *s) {
    bus_.air_filter_pct = s;
  }
  void set_water_filter_pct_sensor(esphome::sensor::Sensor *s) {
    bus_.water_filter_pct = s;
  }
  void set_water_filter_gal_sensor(esphome::sensor::Sensor *s) {
    bus_.water_filter_gal = s;
  }
  void set_water_filter_end_date_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.water_filter_end_date = s;
  }

  // Vacation / ice modes
  void set_long_vacation_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.long_vacation_on = s;
  }
  void set_short_vacation_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.short_vacation_on = s;
  }
  void set_high_use_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.high_use_on = s;
  }
  void set_high_use_start_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.high_use_start_time = s;
  }
  void set_high_use_end_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.high_use_end_time = s;
  }
  void set_night_ice_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.night_ice_on = s;
  }
  void set_max_ice_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.max_ice_on = s;
  }
  void set_max_ice_start_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.max_ice_start_time = s;
  }
  void set_max_ice_end_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.max_ice_end_time = s;
  }

  // Derived selects (see FridgeBus comment in dispatch_esphome.h).
  void set_ice_maker_mode_select(esphome::select::Select *s) {
    bus_.ice_maker_mode = s;
  }
  void set_appliance_mode_select(esphome::select::Select *s) {
    bus_.appliance_mode = s;
  }
  void set_night_mode_select(esphome::select::Select *s) {
    bus_.night_mode_select = s;
  }
  void set_humidity_control_select(esphome::select::Select *s) {
    bus_.humidity_control_select = s;
  }

  // Power / smart grid
  void set_unit_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.unit_on = s;
  }
  // Writable: smart_grid_on is a Switch.
  void set_smart_grid_on_switch(esphome::switch_::Switch *s) {
    bus_.smart_grid_on = s;
  }

  // Misc diagnostics
  void set_pin_window_open_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.pin_window_open = s;
  }
  void set_active_faults_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.active_faults = s;
  }
  void set_door_ajar_timeout_sensor(esphome::sensor::Sensor *s) {
    bus_.door_ajar_timeout = s;
  }

  // WiFi diagnostics
  void set_ap_ssid_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.ap_ssid = s;
  }
  void set_ap_rssi_sensor(esphome::sensor::Sensor *s) { bus_.ap_rssi = s; }
  void set_ap_chan_sensor(esphome::sensor::Sensor *s) { bus_.ap_chan = s; }
  void set_ap_enc_sensor(esphome::sensor::Sensor *s) { bus_.ap_enc = s; }

protected:
  SubzeroHub *hub() override { return &hub_; }
  esphome::subzero_protocol::CommonBus *common_bus() override { return &bus_; }
  void wire_bus_() override { hub_.set_bus(&bus_); }

private:
  FridgeHub hub_;
  esphome::subzero_protocol::FridgeBus bus_;
};

class DishwasherAppliance : public ApplianceBase {
public:
  DishwasherAppliance() = default;

  void set_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.door_ajar = s;
  }
  void set_wash_cycle_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.wash_cycle_on = s;
  }
  void set_heated_dry_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.heated_dry = s;
  }
  void set_extended_dry_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.extended_dry = s;
  }
  void set_high_temp_wash_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.high_temp_wash = s;
  }
  void set_sani_rinse_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.sani_rinse = s;
  }
  void set_rinse_aid_low_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.rinse_aid_low = s;
  }
  void set_softener_low_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.softener_low = s;
  }
  // Read-only: dishwasher light_on can't actually be toggled via `set`
  // (appliance acks but doesn't honor the write). Stays a binary_sensor.
  void set_light_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.light_on = s;
  }
  void set_remote_ready_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.remote_ready = s;
  }
  void set_delay_start_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.delay_start = s;
  }

  void set_wash_status_sensor(esphome::sensor::Sensor *s) {
    bus_.wash_status = s;
  }
  void set_wash_cycle_sensor(esphome::sensor::Sensor *s) {
    bus_.wash_cycle = s;
  }
  void set_wash_time_remaining_sensor(esphome::sensor::Sensor *s) {
    bus_.wash_time_remaining = s;
  }
  void set_wash_cycle_end_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.wash_cycle_end_time = s;
  }

protected:
  SubzeroHub *hub() override { return &hub_; }
  esphome::subzero_protocol::CommonBus *common_bus() override { return &bus_; }
  void wire_bus_() override { hub_.set_bus(&bus_); }

private:
  DishwasherHub hub_;
  esphome::subzero_protocol::DishwasherBus bus_;
};

class RangeAppliance : public ApplianceBase {
public:
  RangeAppliance() = default;

  // Primary cavity binary sensors
  void set_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.door_ajar = s;
  }
  void set_cav_unit_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_unit_on = s;
  }
  void set_cav_at_set_temp_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_at_set_temp = s;
  }
  // Writable: Oven Light is a Switch.
  void set_cav_light_on_switch(esphome::switch_::Switch *s) {
    bus_.cav_light_on = s;
  }
  void set_cav_remote_ready_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_remote_ready = s;
  }
  void set_cav_probe_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_probe_on = s;
  }
  void set_cav_probe_at_temp_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_probe_at_temp = s;
  }
  void set_cav_probe_near_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_probe_near = s;
  }
  void set_cav_gourmet_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav_gourmet = s;
  }
  void set_cook_timer_done_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cook_timer_done = s;
  }
  void set_cook_timer_near_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cook_timer_near = s;
  }

  // Primary cavity numeric / mode
  void set_cav_temp_sensor(esphome::sensor::Sensor *s) { bus_.cav_temp = s; }
  // Writable: cav_set_temp is a Number (HA writes target temp).
  void set_cav_set_temp_number(esphome::number::Number *n) {
    bus_.cav_set_temp = n;
  }
  void set_cav_cook_mode_sensor(esphome::sensor::Sensor *s) {
    bus_.cav_cook_mode = s;
  }
  void set_cav_gourmet_recipe_sensor(esphome::sensor::Sensor *s) {
    bus_.cav_gourmet_recipe = s;
  }
  void set_probe_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.probe_temp = s;
  }
  // Writable: probe_set_temp is a Number.
  void set_probe_set_temp_number(esphome::number::Number *n) {
    bus_.probe_set_temp = n;
  }

  // Kitchen timers
  void set_ktimer_active_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer_active = s;
  }
  void set_ktimer_done_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer_done = s;
  }
  void set_ktimer_near_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer_near = s;
  }
  void set_ktimer2_active_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer2_active = s;
  }
  void set_ktimer2_done_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer2_done = s;
  }
  void set_ktimer2_near_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.ktimer2_near = s;
  }
  void set_ktimer_end_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.ktimer_end_time = s;
  }
  void set_ktimer2_end_time_sensor(esphome::text_sensor::TextSensor *s) {
    bus_.ktimer2_end_time = s;
  }

  // Secondary cavity (dual-oven)
  void set_cav2_unit_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_unit_on = s;
  }
  void set_cav2_door_ajar_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_door_ajar = s;
  }
  void set_cav2_at_set_temp_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_at_set_temp = s;
  }
  // Writable: Oven 2 Light is a Switch.
  void set_cav2_light_on_switch(esphome::switch_::Switch *s) {
    bus_.cav2_light_on = s;
  }
  void set_cav2_remote_ready_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_remote_ready = s;
  }
  void set_cav2_probe_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_probe_on = s;
  }
  void set_cav2_probe_at_temp_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_probe_at_temp = s;
  }
  void set_cav2_probe_near_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_probe_near = s;
  }
  void set_cav2_gourmet_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_gourmet = s;
  }
  void
  set_cav2_cook_timer_done_sensor(esphome::binary_sensor::BinarySensor *s) {
    bus_.cav2_cook_timer_done = s;
  }
  void set_cav2_temp_sensor(esphome::sensor::Sensor *s) { bus_.cav2_temp = s; }
  // Writable: cav2_set_temp is a Number.
  void set_cav2_set_temp_number(esphome::number::Number *n) {
    bus_.cav2_set_temp = n;
  }
  void set_cav2_cook_mode_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_cook_mode = s;
  }
  void set_cav2_probe_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_probe_temp = s;
  }
  // Writable: cav2_probe_set_temp is a Number.
  void set_cav2_probe_set_temp_number(esphome::number::Number *n) {
    bus_.cav2_probe_set_temp = n;
  }

protected:
  SubzeroHub *hub() override { return &hub_; }
  esphome::subzero_protocol::CommonBus *common_bus() override { return &bus_; }
  void wire_bus_() override { hub_.set_bus(&bus_); }

private:
  RangeHub hub_;
  esphome::subzero_protocol::RangeBus bus_;
};

} // namespace subzero_appliance
} // namespace esphome

#endif // USE_ESP32
