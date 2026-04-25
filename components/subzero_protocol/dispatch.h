#pragma once

// Sensor dispatch helpers — convert a parsed appliance state struct into a
// sequence of named publish calls on a "bus". Templated on Bus so production
// (ESPHome sensor pointers, see dispatch_esphome.h) and tests (a recording
// bus that captures method calls, see dispatch_test.cpp) share the same
// pure-logic dispatcher.
//
// Bus method-name convention: `publish_<entity_id_suffix>(value)`, where
// `<entity_id_suffix>` is the YAML id without the `${prefix}_` prefix
// (e.g. `${prefix}_set_temp` → `publish_set_temp`). The dispatch functions
// here are the single source of truth for the parser-field → entity-id
// mapping; production buses MUST provide every method this header calls,
// or the on-device build won't link.
//
// PIN confirmation, status-302 detection, and post-parse global-state
// mutation (poll_ok, fast_retries) intentionally remain in YAML — they
// touch ESPHome globals + scripts that aren't worth threading through a
// host-buildable abstraction.

#include "protocol.h"

namespace esphome {
namespace subzero_protocol {

// Dispatches every CommonFields entry to its corresponding bus method.
// Called by each appliance dispatcher; production buses inherit a
// CommonBus base struct that provides these methods (see
// dispatch_esphome.h).
template <typename Bus>
inline void dispatch_common(const CommonFields &c, Bus &bus) {
  if (c.sabbath_on)
    bus.publish_sabbath_on(*c.sabbath_on);
  if (c.service_required)
    bus.publish_svc_required(*c.service_required);
  if (c.appliance_model)
    bus.publish_model(*c.appliance_model);
  if (c.uptime)
    bus.publish_uptime(*c.uptime);
  if (c.appliance_serial)
    bus.publish_serial(*c.appliance_serial);
  if (c.appliance_type)
    bus.publish_appliance_type(*c.appliance_type);
  if (c.diagnostic_status)
    bus.publish_diag_status(*c.diagnostic_status);
  if (c.build_date)
    bus.publish_build_date(*c.build_date);
  if (c.version.fw)
    bus.publish_fw_version(*c.version.fw);
  if (c.version.api)
    bus.publish_api_version(*c.version.api);
  if (c.version.bleapp)
    bus.publish_bleapp_version(*c.version.bleapp);
  if (c.version.os)
    bus.publish_os_version(*c.version.os);
  if (c.version.rtapp)
    bus.publish_rtapp_version(*c.version.rtapp);
  if (c.version.appliance)
    bus.publish_board_version(*c.version.appliance);
}

template <typename Bus>
inline void dispatch_fridge(const FridgeState &s, Bus &bus) {
  dispatch_common(s.common, bus);
  // Fridge / Freezer
  if (s.ref_set_temp)
    bus.publish_set_temp(*s.ref_set_temp);
  if (s.door_ajar)
    bus.publish_door_ajar(*s.door_ajar);
  if (s.frz_set_temp)
    bus.publish_frz_set_temp(*s.frz_set_temp);
  if (s.frz_door_ajar)
    bus.publish_frz_door_ajar(*s.frz_door_ajar);
  if (s.ice_maker_on)
    bus.publish_ice_maker(*s.ice_maker_on);
  // Refrigerator drawer
  if (s.ref2_set_temp)
    bus.publish_ref2_set_temp(*s.ref2_set_temp);
  if (s.ref2_door_ajar)
    bus.publish_ref2_door_ajar(*s.ref2_door_ajar);
  // Wine
  if (s.wine_door_ajar)
    bus.publish_wine_door_ajar(*s.wine_door_ajar);
  if (s.wine_set_temp)
    bus.publish_wine_set_temp(*s.wine_set_temp);
  if (s.wine2_set_temp)
    bus.publish_wine2_set_temp(*s.wine2_set_temp);
  if (s.wine_temp_alert_on)
    bus.publish_wine_temp_alert(*s.wine_temp_alert_on);
  // Crisper
  if (s.crisp_set_temp)
    bus.publish_crisp_set_temp(*s.crisp_set_temp);
  // Filters
  if (s.air_filter_on)
    bus.publish_air_filter_on(*s.air_filter_on);
  if (s.air_filter_pct_remaining)
    bus.publish_air_filter_pct(*s.air_filter_pct_remaining);
  if (s.water_filter_pct_remaining)
    bus.publish_water_filter_pct(*s.water_filter_pct_remaining);
}

template <typename Bus>
inline void dispatch_dishwasher(const DishwasherState &s, Bus &bus) {
  dispatch_common(s.common, bus);
  if (s.door_ajar)
    bus.publish_door_ajar(*s.door_ajar);
  if (s.wash_cycle_on)
    bus.publish_wash_cycle_on(*s.wash_cycle_on);
  if (s.heated_dry_on)
    bus.publish_heated_dry(*s.heated_dry_on);
  if (s.extended_dry_on)
    bus.publish_extended_dry(*s.extended_dry_on);
  if (s.high_temp_wash_on)
    bus.publish_high_temp_wash(*s.high_temp_wash_on);
  if (s.sani_rinse_on)
    bus.publish_sani_rinse(*s.sani_rinse_on);
  if (s.rinse_aid_low)
    bus.publish_rinse_aid_low(*s.rinse_aid_low);
  if (s.softener_low)
    bus.publish_softener_low(*s.softener_low);
  if (s.light_on)
    bus.publish_light_on(*s.light_on);
  if (s.remote_ready)
    bus.publish_remote_ready(*s.remote_ready);
  if (s.delay_start_timer_active)
    bus.publish_delay_start(*s.delay_start_timer_active);
  if (s.wash_status)
    bus.publish_wash_status(*s.wash_status);
  if (s.wash_cycle)
    bus.publish_wash_cycle(*s.wash_cycle);
  if (s.wash_cycle_end_time)
    bus.publish_wash_cycle_end_time(*s.wash_cycle_end_time);
  if (s.wash_time_remaining_min)
    bus.publish_wash_time_remaining(*s.wash_time_remaining_min);
  // Stateful: when wash_cycle_on flips false, force the remaining-time
  // sensor to 0 (otherwise it sticks at the last computed minute count
  // until the next wash cycle starts).
  if (s.wash_cycle_on && !*s.wash_cycle_on) {
    bus.clear_wash_time_remaining_if_running();
  }
}

template <typename Bus>
inline void dispatch_range(const RangeState &s, Bus &bus) {
  dispatch_common(s.common, bus);
  if (s.door_ajar)
    bus.publish_door_ajar(*s.door_ajar);
  // Primary cavity
  if (s.cav_unit_on)
    bus.publish_cav_unit_on(*s.cav_unit_on);
  if (s.cav_at_set_temp)
    bus.publish_cav_at_set_temp(*s.cav_at_set_temp);
  if (s.cav_light_on)
    bus.publish_cav_light_on(*s.cav_light_on);
  if (s.cav_remote_ready)
    bus.publish_cav_remote_ready(*s.cav_remote_ready);
  if (s.cav_probe_on)
    bus.publish_cav_probe_on(*s.cav_probe_on);
  if (s.cav_probe_at_set_temp)
    bus.publish_cav_probe_at_temp(*s.cav_probe_at_set_temp);
  if (s.cav_probe_within_10deg)
    bus.publish_cav_probe_near(*s.cav_probe_within_10deg);
  if (s.cav_gourmet_mode_on)
    bus.publish_cav_gourmet(*s.cav_gourmet_mode_on);
  if (s.cav_gourmet_recipe)
    bus.publish_cav_gourmet_recipe(*s.cav_gourmet_recipe);
  if (s.cav_cook_timer_complete)
    bus.publish_cook_timer_done(*s.cav_cook_timer_complete);
  if (s.cav_cook_timer_within_1min)
    bus.publish_cook_timer_near(*s.cav_cook_timer_within_1min);
  if (s.cav_temp)
    bus.publish_cav_temp(*s.cav_temp);
  if (s.cav_set_temp)
    bus.publish_cav_set_temp(*s.cav_set_temp);
  if (s.cav_cook_mode)
    bus.publish_cav_cook_mode(*s.cav_cook_mode);
  if (s.cav_probe_temp)
    bus.publish_probe_temp(*s.cav_probe_temp);
  if (s.cav_probe_set_temp)
    bus.publish_probe_set_temp(*s.cav_probe_set_temp);
  // Kitchen timers
  if (s.kitchen_timer_active)
    bus.publish_ktimer_active(*s.kitchen_timer_active);
  if (s.kitchen_timer_complete)
    bus.publish_ktimer_done(*s.kitchen_timer_complete);
  if (s.kitchen_timer_within_1min)
    bus.publish_ktimer_near(*s.kitchen_timer_within_1min);
  if (s.kitchen_timer_end_time)
    bus.publish_ktimer_end_time(*s.kitchen_timer_end_time);
  if (s.kitchen_timer2_active)
    bus.publish_ktimer2_active(*s.kitchen_timer2_active);
  if (s.kitchen_timer2_complete)
    bus.publish_ktimer2_done(*s.kitchen_timer2_complete);
  if (s.kitchen_timer2_within_1min)
    bus.publish_ktimer2_near(*s.kitchen_timer2_within_1min);
  if (s.kitchen_timer2_end_time)
    bus.publish_ktimer2_end_time(*s.kitchen_timer2_end_time);
  // Secondary cavity (dual-oven ranges)
  if (s.cav2_unit_on)
    bus.publish_cav2_unit_on(*s.cav2_unit_on);
  if (s.cav2_door_ajar)
    bus.publish_cav2_door_ajar(*s.cav2_door_ajar);
  if (s.cav2_at_set_temp)
    bus.publish_cav2_at_set_temp(*s.cav2_at_set_temp);
  if (s.cav2_light_on)
    bus.publish_cav2_light_on(*s.cav2_light_on);
  if (s.cav2_remote_ready)
    bus.publish_cav2_remote_ready(*s.cav2_remote_ready);
  if (s.cav2_probe_on)
    bus.publish_cav2_probe_on(*s.cav2_probe_on);
  if (s.cav2_probe_at_set_temp)
    bus.publish_cav2_probe_at_temp(*s.cav2_probe_at_set_temp);
  if (s.cav2_probe_within_10deg)
    bus.publish_cav2_probe_near(*s.cav2_probe_within_10deg);
  if (s.cav2_gourmet_mode_on)
    bus.publish_cav2_gourmet(*s.cav2_gourmet_mode_on);
  if (s.cav2_cook_timer_complete)
    bus.publish_cav2_cook_timer_done(*s.cav2_cook_timer_complete);
  if (s.cav2_temp)
    bus.publish_cav2_temp(*s.cav2_temp);
  if (s.cav2_set_temp)
    bus.publish_cav2_set_temp(*s.cav2_set_temp);
  if (s.cav2_cook_mode)
    bus.publish_cav2_cook_mode(*s.cav2_cook_mode);
  if (s.cav2_probe_temp)
    bus.publish_cav2_probe_temp(*s.cav2_probe_temp);
  if (s.cav2_probe_set_temp)
    bus.publish_cav2_probe_set_temp(*s.cav2_probe_set_temp);
}

} // namespace subzero_protocol
} // namespace esphome
