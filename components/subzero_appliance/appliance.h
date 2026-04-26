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

  void set_set_temp_sensor(esphome::sensor::Sensor *s) { bus_.set_temp = s; }
  void set_frz_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.frz_set_temp = s;
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
  void set_air_filter_pct_sensor(esphome::sensor::Sensor *s) {
    bus_.air_filter_pct = s;
  }
  void set_water_filter_pct_sensor(esphome::sensor::Sensor *s) {
    bus_.water_filter_pct = s;
  }

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
  void set_cav_light_on_sensor(esphome::binary_sensor::BinarySensor *s) {
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
  void set_cav_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.cav_set_temp = s;
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
  void set_probe_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.probe_set_temp = s;
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
  void set_cav2_light_on_sensor(esphome::binary_sensor::BinarySensor *s) {
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
  void set_cav2_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_set_temp = s;
  }
  void set_cav2_cook_mode_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_cook_mode = s;
  }
  void set_cav2_probe_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_probe_temp = s;
  }
  void set_cav2_probe_set_temp_sensor(esphome::sensor::Sensor *s) {
    bus_.cav2_probe_set_temp = s;
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
