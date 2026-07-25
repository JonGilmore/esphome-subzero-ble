#pragma once

// Production "Bus" structs that hold raw ESPHome sensor pointers and
// implement the publish methods that dispatch.h calls.
//
// One struct per appliance type. Each declares a pointer for every
// `${prefix}_<id>` sensor the appliance YAML creates, plus a one-line
// `publish_<id>(value)` method that null-checks and forwards to
// `Sensor::publish_state()`.
//
// All buses inherit from CommonBus, which holds the appliance-agnostic
// fields (model / uptime / firmware version / etc).
//
// Population pattern in YAML:
//
//     globals:
//       - id: ${prefix}_bus
//         type: esphome::subzero_protocol::FridgeBus
//         restore_value: false
//
//     esphome:
//       on_boot:
//         - priority: -100
//           then:
//             - lambda: |-
//                 auto& bus = id(${prefix}_bus);
//                 bus.set_temp = id(${prefix}_set_temp);
//                 bus.door_ajar = id(${prefix}_door_ajar);
//                 // ... etc
//
// Then parse_json calls `dispatch_fridge(s, id(${prefix}_bus))` once.
//
// This header is on-device only — it pulls in ESPHome's typed sensor
// headers, which aren't available during host gtest builds. Host tests
// instantiate dispatch.h's templates with their own recording bus.

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <optional>
#include <string>

namespace esphome {
namespace subzero_protocol {

namespace detail {

// Null-checked publish helpers. Templated on the ESPHome sensor type
// so we don't write 50 copies of `if (s) s->publish_state(v)`.
template <typename S, typename V> inline void publish_if(S *s, V v) {
  if (s != nullptr)
    s->publish_state(v);
}

} // namespace detail

// Common (appliance-agnostic) bus. Inherited by every appliance bus.
//
// Writable fields per type are exposed as Switch / Number rather than
// BinarySensor / Sensor. publish_X(bool/float) signatures stay the same
// since both flavors accept the same value type.
struct CommonBus {
  esphome::binary_sensor::BinarySensor *sabbath_on = nullptr;
  esphome::binary_sensor::BinarySensor *svc_required = nullptr;
  esphome::text_sensor::TextSensor *model = nullptr;
  esphome::text_sensor::TextSensor *uptime = nullptr;
  esphome::text_sensor::TextSensor *serial = nullptr;
  esphome::text_sensor::TextSensor *appliance_type = nullptr;
  esphome::text_sensor::TextSensor *diag_status = nullptr;
  esphome::text_sensor::TextSensor *build_date = nullptr;
  esphome::text_sensor::TextSensor *fw_version = nullptr;
  esphome::text_sensor::TextSensor *api_version = nullptr;
  esphome::text_sensor::TextSensor *bleapp_version = nullptr;
  esphome::text_sensor::TextSensor *os_version = nullptr;
  esphome::text_sensor::TextSensor *rtapp_version = nullptr;
  esphome::text_sensor::TextSensor *board_version = nullptr;
  // Latest notif_type → human-readable event name (e.g. "fridge_door_open").
  // Updated only on push messages that carry a notif_type; HA automations
  // can trigger on state-changes here.
  esphome::text_sensor::TextSensor *notif_event = nullptr;

  void publish_sabbath_on(bool v) { detail::publish_if(sabbath_on, v); }
  void publish_svc_required(bool v) { detail::publish_if(svc_required, v); }
  void publish_model(const std::string &v) { detail::publish_if(model, v); }
  void publish_uptime(const std::string &v) { detail::publish_if(uptime, v); }
  void publish_serial(const std::string &v) { detail::publish_if(serial, v); }
  void publish_appliance_type(const std::string &v) {
    detail::publish_if(appliance_type, v);
  }
  void publish_diag_status(const std::string &v) {
    detail::publish_if(diag_status, v);
  }
  void publish_build_date(const std::string &v) {
    detail::publish_if(build_date, v);
  }
  void publish_fw_version(const std::string &v) {
    detail::publish_if(fw_version, v);
  }
  void publish_api_version(const std::string &v) {
    detail::publish_if(api_version, v);
  }
  void publish_bleapp_version(const std::string &v) {
    detail::publish_if(bleapp_version, v);
  }
  void publish_os_version(const std::string &v) {
    detail::publish_if(os_version, v);
  }
  void publish_rtapp_version(const std::string &v) {
    detail::publish_if(rtapp_version, v);
  }
  void publish_board_version(const std::string &v) {
    detail::publish_if(board_version, v);
  }
  void publish_notif_event(const std::string &v) {
    detail::publish_if(notif_event, v);
  }
};

struct FridgeBus : CommonBus {
  esphome::binary_sensor::BinarySensor *door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *frz_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *ice_maker = nullptr;
  esphome::binary_sensor::BinarySensor *ref2_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *wine_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *wine_temp_alert = nullptr;
  esphome::binary_sensor::BinarySensor *air_filter_on = nullptr;

  // Set-temps are read-only Sensors by default. Writing them via `set`
  // was assumed not to work, based on testing on fw 8.5 units (appliance
  // accepts the write with status:0 but the setpoint never actually
  // changes). Confirmed via community testing that writes DO take real
  // effect on at least one fw 2.27 unit (front-panel display, official
  // app, and BLE state all agreed after a write). Since behavior may
  // still vary by firmware/model, this stays opt-in: the `set_temp`
  // Sensor pointer is used by default (read-only, existing behavior
  // unchanged); when a user sets `enable_temp_control: true`, the Python
  // codegen instead populates `set_temp_number` (a writable Number) and
  // leaves `set_temp` null. Exactly one of the two is ever non-null for
  // a given config; publish_set_temp() forwards to whichever is set.
  esphome::sensor::Sensor *set_temp = nullptr;
  esphome::number::Number *set_temp_number = nullptr;
  esphome::sensor::Sensor *frz_set_temp = nullptr;
  esphome::number::Number *frz_set_temp_number = nullptr;
  esphome::sensor::Sensor *ref2_set_temp = nullptr;
  esphome::sensor::Sensor *wine_set_temp = nullptr;
  esphome::sensor::Sensor *wine2_set_temp = nullptr;
  esphome::sensor::Sensor *crisp_set_temp = nullptr;
  esphome::number::Number *crisp_set_temp_number = nullptr;
  // "Automatic crisper temperature" toggle from the app. Gates whether
  // crisp_set_temp writes actually take effect (see protocol.h comment) —
  // opt-in alongside crisp_set_temp_number via enable_temp_control.
  esphome::switch_::Switch *crisp_temp_mode = nullptr;
  esphome::sensor::Sensor *air_filter_pct = nullptr;
  esphome::sensor::Sensor *water_filter_pct = nullptr;
  esphome::sensor::Sensor *water_filter_gal = nullptr;
  esphome::text_sensor::TextSensor *water_filter_end_date = nullptr;

  // Vacation / ice modes. `long_vacation_on`/`short_vacation_on`/
  // `high_use_on`/`sabbath_on` (the last inherited from CommonBus) and
  // `ice_maker_on`/`max_ice_on`/`night_ice_on` are each still parsed as
  // independent booleans (that's what the appliance actually sends), but
  // the app presents each group as a single mutually-exclusive picker —
  // see `appliance_mode`/`ice_maker_mode` below, which derive a select
  // state from these cached values. Kept as plain BinarySensor pointers
  // too (still populated, still useful for automations that want just
  // one flag) — Python codegen decides whether to wire them.
  esphome::binary_sensor::BinarySensor *long_vacation_on = nullptr;
  esphome::binary_sensor::BinarySensor *short_vacation_on = nullptr;
  esphome::binary_sensor::BinarySensor *high_use_on = nullptr;
  esphome::text_sensor::TextSensor *high_use_start_time = nullptr;
  esphome::text_sensor::TextSensor *high_use_end_time = nullptr;
  esphome::binary_sensor::BinarySensor *night_ice_on = nullptr;
  esphome::binary_sensor::BinarySensor *max_ice_on = nullptr;
  esphome::text_sensor::TextSensor *max_ice_start_time = nullptr;
  esphome::text_sensor::TextSensor *max_ice_end_time = nullptr;

  // Derived selects (opt-in via `enable_mode_selects: true`). Mirror the
  // app's own grouped pickers. Writing one of these sends a `set` for
  // every field in that option's mapping (see ApplianceSetGroupedSelect
  // in appliance_base.h). Confirmed via live BLE testing 2026-07-25: every
  // option in both selects (including the "Off"/"Normal" baselines,
  // Sabbath, and both vacation modes) round-trips correctly.
  esphome::select::Select *ice_maker_mode = nullptr;
  esphome::select::Select *appliance_mode = nullptr;
  // 2-option selects backed by a single int field. Confirmed via live BLE
  // testing 2026-07-25 that the two fields do NOT share an encoding:
  // night_mode is 0=Disabled/1=Enabled (matches option index order), but
  // humidity_control is 1=Normal/2=Enhanced (does not — see
  // ApplianceSetIntSelect::add_value in appliance_base.h).
  esphome::select::Select *night_mode_select = nullptr;
  esphome::select::Select *humidity_control_select = nullptr;

  // Cached last-known values, used only to recompute the derived selects
  // above when any one contributing field changes (a push may update
  // just one of several fields at a time).
  std::optional<bool> ice_maker_on_cached_;
  std::optional<bool> max_ice_on_cached_;
  std::optional<bool> night_ice_on_cached_;
  std::optional<bool> high_use_on_cached_;
  std::optional<bool> short_vacation_on_cached_;
  std::optional<bool> long_vacation_on_cached_;
  std::optional<bool> sabbath_on_cached_;

  void recompute_ice_maker_mode_() {
    if (ice_maker_mode == nullptr || !ice_maker_on_cached_.has_value())
      return;
    std::string label;
    if (max_ice_on_cached_.value_or(false))
      label = "Max Ice";
    else if (night_ice_on_cached_.value_or(false))
      label = "Night Ice";
    else if (ice_maker_on_cached_.value_or(false))
      label = "Normal";
    else
      label = "Off";
    ice_maker_mode->publish_state(label);
  }

  void recompute_appliance_mode_() {
    if (appliance_mode == nullptr)
      return;
    if (!sabbath_on_cached_.has_value() && !high_use_on_cached_.has_value() &&
        !short_vacation_on_cached_.has_value() &&
        !long_vacation_on_cached_.has_value())
      return;
    std::string label;
    if (sabbath_on_cached_.value_or(false))
      label = "Sabbath";
    else if (long_vacation_on_cached_.value_or(false))
      label = "Long Vacation";
    else if (short_vacation_on_cached_.value_or(false))
      label = "Short Vacation";
    else if (high_use_on_cached_.value_or(false))
      label = "High Usage";
    else
      label = "Normal";
    appliance_mode->publish_state(label);
  }

  // Power / smart grid. smart_grid_on was briefly a writable Switch, but
  // live testing 2026-07-25 confirmed writes are ignored (state reverts
  // to `true` within seconds — same "wrote it, appliance's real value
  // won" pattern as dishwasher light_on). No corresponding control exists
  // in the official app or the appliance's own display, suggesting this
  // is an automatically-managed status field, not a user setting. Back
  // to a read-only BinarySensor.
  esphome::binary_sensor::BinarySensor *unit_on = nullptr;
  esphome::binary_sensor::BinarySensor *smart_grid_on = nullptr;

  // Misc diagnostics.
  esphome::binary_sensor::BinarySensor *pin_window_open = nullptr;
  esphome::text_sensor::TextSensor *active_faults = nullptr;
  esphome::sensor::Sensor *door_ajar_timeout = nullptr;

  // WiFi diagnostics.
  esphome::text_sensor::TextSensor *ap_ssid = nullptr;
  esphome::sensor::Sensor *ap_rssi = nullptr;
  esphome::sensor::Sensor *ap_chan = nullptr;
  esphome::sensor::Sensor *ap_enc = nullptr;

  void publish_door_ajar(bool v) { detail::publish_if(door_ajar, v); }
  void publish_frz_door_ajar(bool v) { detail::publish_if(frz_door_ajar, v); }
  void publish_ice_maker(bool v) {
    detail::publish_if(ice_maker, v);
    ice_maker_on_cached_ = v;
    recompute_ice_maker_mode_();
  }
  void publish_ref2_door_ajar(bool v) { detail::publish_if(ref2_door_ajar, v); }
  void publish_wine_door_ajar(bool v) { detail::publish_if(wine_door_ajar, v); }
  void publish_wine_temp_alert(bool v) {
    detail::publish_if(wine_temp_alert, v);
  }
  void publish_air_filter_on(bool v) { detail::publish_if(air_filter_on, v); }

  void publish_set_temp(float v) {
    detail::publish_if(set_temp, v);
    detail::publish_if(set_temp_number, v);
  }
  void publish_frz_set_temp(float v) {
    detail::publish_if(frz_set_temp, v);
    detail::publish_if(frz_set_temp_number, v);
  }
  void publish_ref2_set_temp(float v) { detail::publish_if(ref2_set_temp, v); }
  void publish_wine_set_temp(float v) { detail::publish_if(wine_set_temp, v); }
  void publish_wine2_set_temp(float v) {
    detail::publish_if(wine2_set_temp, v);
  }
  void publish_crisp_set_temp(float v) {
    detail::publish_if(crisp_set_temp, v);
    detail::publish_if(crisp_set_temp_number, v);
  }
  // 1 = Automatic (matches the app's toggle "on" state), 0 = Manual.
  void publish_crisp_temp_mode(int v) {
    detail::publish_if(crisp_temp_mode, v == 1);
  }
  void publish_air_filter_pct(float v) {
    detail::publish_if(air_filter_pct, v);
  }
  void publish_water_filter_pct(float v) {
    detail::publish_if(water_filter_pct, v);
  }
  void publish_water_filter_gal(float v) {
    detail::publish_if(water_filter_gal, v);
  }
  void publish_water_filter_end_date(const std::string &v) {
    detail::publish_if(water_filter_end_date, v);
  }

  void publish_long_vacation_on(bool v) {
    detail::publish_if(long_vacation_on, v);
    long_vacation_on_cached_ = v;
    recompute_appliance_mode_();
  }
  void publish_short_vacation_on(bool v) {
    detail::publish_if(short_vacation_on, v);
    short_vacation_on_cached_ = v;
    recompute_appliance_mode_();
  }
  void publish_high_use_on(bool v) {
    detail::publish_if(high_use_on, v);
    high_use_on_cached_ = v;
    recompute_appliance_mode_();
  }
  void publish_high_use_start_time(const std::string &v) {
    detail::publish_if(high_use_start_time, v);
  }
  void publish_high_use_end_time(const std::string &v) {
    detail::publish_if(high_use_end_time, v);
  }
  // Shadows CommonBus::publish_sabbath_on (resolved statically per Bus
  // type by dispatch_common<Bus>, so this override is picked up when
  // called through a FridgeBus instance). Preserves the existing
  // read-only sabbath_on binary_sensor, and additionally feeds
  // appliance_mode.
  void publish_sabbath_on(bool v) {
    CommonBus::publish_sabbath_on(v);
    sabbath_on_cached_ = v;
    recompute_appliance_mode_();
  }
  // Enum-to-label mapping (0="Disabled", 1="Enabled") confirmed via live
  // BLE testing 2026-07-25, both directions.
  void publish_night_mode(int v) {
    if (night_mode_select != nullptr)
      night_mode_select->publish_state(v == 1 ? "Enabled" : "Disabled");
  }
  void publish_night_ice_on(bool v) {
    detail::publish_if(night_ice_on, v);
    night_ice_on_cached_ = v;
    recompute_ice_maker_mode_();
  }
  void publish_max_ice_on(bool v) {
    detail::publish_if(max_ice_on, v);
    max_ice_on_cached_ = v;
    recompute_ice_maker_mode_();
  }
  void publish_max_ice_start_time(const std::string &v) {
    detail::publish_if(max_ice_start_time, v);
  }
  void publish_max_ice_end_time(const std::string &v) {
    detail::publish_if(max_ice_end_time, v);
  }
  void publish_unit_on(bool v) { detail::publish_if(unit_on, v); }
  void publish_smart_grid_on(bool v) { detail::publish_if(smart_grid_on, v); }
  void publish_pin_window_open(bool v) {
    detail::publish_if(pin_window_open, v);
  }
  void publish_active_faults(const std::string &v) {
    detail::publish_if(active_faults, v);
  }
  // Enum-to-label mapping confirmed 2026-07-25 by changing the setting in
  // the official app while watching raw BLE state: 1="Normal",
  // 2="Enhanced" (NOT 0/1 like night_mode — do not assume index order for
  // this field).
  void publish_humidity_control(int v) {
    if (humidity_control_select != nullptr)
      humidity_control_select->publish_state(v == 2 ? "Enhanced" : "Normal");
  }
  void publish_door_ajar_timeout(int v) {
    detail::publish_if(door_ajar_timeout, static_cast<float>(v));
  }
  void publish_ap_ssid(const std::string &v) { detail::publish_if(ap_ssid, v); }
  void publish_ap_rssi(int v) {
    detail::publish_if(ap_rssi, static_cast<float>(v));
  }
  void publish_ap_chan(int v) {
    detail::publish_if(ap_chan, static_cast<float>(v));
  }
  void publish_ap_enc(int v) {
    detail::publish_if(ap_enc, static_cast<float>(v));
  }
};

struct DishwasherBus : CommonBus {
  esphome::binary_sensor::BinarySensor *door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *wash_cycle_on = nullptr;
  esphome::binary_sensor::BinarySensor *heated_dry = nullptr;
  esphome::binary_sensor::BinarySensor *extended_dry = nullptr;
  esphome::binary_sensor::BinarySensor *high_temp_wash = nullptr;
  esphome::binary_sensor::BinarySensor *sani_rinse = nullptr;
  esphome::binary_sensor::BinarySensor *rinse_aid_low = nullptr;
  esphome::binary_sensor::BinarySensor *softener_low = nullptr;
  // Read-only: writing `set light_on` doesn't actually toggle the light.
  esphome::binary_sensor::BinarySensor *light_on = nullptr;
  esphome::binary_sensor::BinarySensor *remote_ready = nullptr;
  esphome::binary_sensor::BinarySensor *delay_start = nullptr;

  esphome::sensor::Sensor *wash_status = nullptr;
  esphome::sensor::Sensor *wash_cycle = nullptr;
  esphome::sensor::Sensor *wash_time_remaining = nullptr;

  esphome::text_sensor::TextSensor *wash_cycle_end_time = nullptr;

  void publish_door_ajar(bool v) { detail::publish_if(door_ajar, v); }
  void publish_wash_cycle_on(bool v) { detail::publish_if(wash_cycle_on, v); }
  void publish_heated_dry(bool v) { detail::publish_if(heated_dry, v); }
  void publish_extended_dry(bool v) { detail::publish_if(extended_dry, v); }
  void publish_high_temp_wash(bool v) { detail::publish_if(high_temp_wash, v); }
  void publish_sani_rinse(bool v) { detail::publish_if(sani_rinse, v); }
  void publish_rinse_aid_low(bool v) { detail::publish_if(rinse_aid_low, v); }
  void publish_softener_low(bool v) { detail::publish_if(softener_low, v); }
  void publish_light_on(bool v) { detail::publish_if(light_on, v); }
  void publish_remote_ready(bool v) { detail::publish_if(remote_ready, v); }
  void publish_delay_start(bool v) { detail::publish_if(delay_start, v); }

  void publish_wash_status(int v) {
    detail::publish_if(wash_status, static_cast<float>(v));
  }
  void publish_wash_cycle(int v) {
    detail::publish_if(wash_cycle, static_cast<float>(v));
  }
  void publish_wash_time_remaining(int v) {
    detail::publish_if(wash_time_remaining, static_cast<float>(v));
  }

  void publish_wash_cycle_end_time(const std::string &v) {
    detail::publish_if(wash_cycle_end_time, v);
  }

  // Stateful: only force the remaining-time sensor to 0 if its current
  // state is non-zero. Avoids publishing 0 every poll cycle once a wash
  // ends and the cycle stays off (would spam HA history with zeros).
  void clear_wash_time_remaining_if_running() {
    if (wash_time_remaining != nullptr && wash_time_remaining->state > 0) {
      wash_time_remaining->publish_state(0);
    }
  }
};

struct RangeBus : CommonBus {
  // Primary cavity
  esphome::binary_sensor::BinarySensor *door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *cav_unit_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav_at_set_temp = nullptr;
  esphome::switch_::Switch *cav_light_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav_remote_ready = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_at_temp = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_near = nullptr;
  esphome::binary_sensor::BinarySensor *cav_gourmet = nullptr;
  esphome::binary_sensor::BinarySensor *cook_timer_done = nullptr;
  esphome::binary_sensor::BinarySensor *cook_timer_near = nullptr;

  esphome::sensor::Sensor *cav_temp = nullptr;
  esphome::number::Number *cav_set_temp = nullptr; // writable
  esphome::sensor::Sensor *cav_cook_mode = nullptr;
  esphome::sensor::Sensor *cav_gourmet_recipe = nullptr;
  esphome::sensor::Sensor *probe_temp = nullptr;
  esphome::number::Number *probe_set_temp = nullptr; // writable

  // Kitchen timers (1 + 2)
  esphome::binary_sensor::BinarySensor *ktimer_active = nullptr;
  esphome::binary_sensor::BinarySensor *ktimer_done = nullptr;
  esphome::binary_sensor::BinarySensor *ktimer_near = nullptr;
  esphome::binary_sensor::BinarySensor *ktimer2_active = nullptr;
  esphome::binary_sensor::BinarySensor *ktimer2_done = nullptr;
  esphome::binary_sensor::BinarySensor *ktimer2_near = nullptr;
  esphome::text_sensor::TextSensor *ktimer_end_time = nullptr;
  esphome::text_sensor::TextSensor *ktimer2_end_time = nullptr;

  // Secondary cavity (dual-oven)
  esphome::binary_sensor::BinarySensor *cav2_unit_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_at_set_temp = nullptr;
  esphome::switch_::Switch *cav2_light_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_remote_ready = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_at_temp = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_near = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_gourmet = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_cook_timer_done = nullptr;

  esphome::sensor::Sensor *cav2_temp = nullptr;
  esphome::number::Number *cav2_set_temp = nullptr; // writable
  esphome::sensor::Sensor *cav2_cook_mode = nullptr;
  esphome::sensor::Sensor *cav2_probe_temp = nullptr;
  esphome::number::Number *cav2_probe_set_temp = nullptr; // writable

  // Primary cavity publishes
  void publish_door_ajar(bool v) { detail::publish_if(door_ajar, v); }
  void publish_cav_unit_on(bool v) { detail::publish_if(cav_unit_on, v); }
  void publish_cav_at_set_temp(bool v) {
    detail::publish_if(cav_at_set_temp, v);
  }
  void publish_cav_light_on(bool v) { detail::publish_if(cav_light_on, v); }
  void publish_cav_remote_ready(bool v) {
    detail::publish_if(cav_remote_ready, v);
  }
  void publish_cav_probe_on(bool v) { detail::publish_if(cav_probe_on, v); }
  void publish_cav_probe_at_temp(bool v) {
    detail::publish_if(cav_probe_at_temp, v);
  }
  void publish_cav_probe_near(bool v) { detail::publish_if(cav_probe_near, v); }
  void publish_cav_gourmet(bool v) { detail::publish_if(cav_gourmet, v); }
  void publish_cook_timer_done(bool v) {
    detail::publish_if(cook_timer_done, v);
  }
  void publish_cook_timer_near(bool v) {
    detail::publish_if(cook_timer_near, v);
  }

  void publish_cav_temp(float v) { detail::publish_if(cav_temp, v); }
  void publish_cav_set_temp(float v) { detail::publish_if(cav_set_temp, v); }
  void publish_cav_cook_mode(int v) {
    detail::publish_if(cav_cook_mode, static_cast<float>(v));
  }
  void publish_cav_gourmet_recipe(int v) {
    detail::publish_if(cav_gourmet_recipe, static_cast<float>(v));
  }
  void publish_probe_temp(float v) { detail::publish_if(probe_temp, v); }
  void publish_probe_set_temp(float v) {
    detail::publish_if(probe_set_temp, v);
  }

  // Kitchen timer publishes
  void publish_ktimer_active(bool v) { detail::publish_if(ktimer_active, v); }
  void publish_ktimer_done(bool v) { detail::publish_if(ktimer_done, v); }
  void publish_ktimer_near(bool v) { detail::publish_if(ktimer_near, v); }
  void publish_ktimer2_active(bool v) { detail::publish_if(ktimer2_active, v); }
  void publish_ktimer2_done(bool v) { detail::publish_if(ktimer2_done, v); }
  void publish_ktimer2_near(bool v) { detail::publish_if(ktimer2_near, v); }
  void publish_ktimer_end_time(const std::string &v) {
    detail::publish_if(ktimer_end_time, v);
  }
  void publish_ktimer2_end_time(const std::string &v) {
    detail::publish_if(ktimer2_end_time, v);
  }

  // Secondary cavity publishes
  void publish_cav2_unit_on(bool v) { detail::publish_if(cav2_unit_on, v); }
  void publish_cav2_door_ajar(bool v) { detail::publish_if(cav2_door_ajar, v); }
  void publish_cav2_at_set_temp(bool v) {
    detail::publish_if(cav2_at_set_temp, v);
  }
  void publish_cav2_light_on(bool v) { detail::publish_if(cav2_light_on, v); }
  void publish_cav2_remote_ready(bool v) {
    detail::publish_if(cav2_remote_ready, v);
  }
  void publish_cav2_probe_on(bool v) { detail::publish_if(cav2_probe_on, v); }
  void publish_cav2_probe_at_temp(bool v) {
    detail::publish_if(cav2_probe_at_temp, v);
  }
  void publish_cav2_probe_near(bool v) {
    detail::publish_if(cav2_probe_near, v);
  }
  void publish_cav2_gourmet(bool v) { detail::publish_if(cav2_gourmet, v); }
  void publish_cav2_cook_timer_done(bool v) {
    detail::publish_if(cav2_cook_timer_done, v);
  }

  void publish_cav2_temp(float v) { detail::publish_if(cav2_temp, v); }
  void publish_cav2_set_temp(float v) { detail::publish_if(cav2_set_temp, v); }
  void publish_cav2_cook_mode(int v) {
    detail::publish_if(cav2_cook_mode, static_cast<float>(v));
  }
  void publish_cav2_probe_temp(float v) {
    detail::publish_if(cav2_probe_temp, v);
  }
  void publish_cav2_probe_set_temp(float v) {
    detail::publish_if(cav2_probe_set_temp, v);
  }
};

} // namespace subzero_protocol
} // namespace esphome
