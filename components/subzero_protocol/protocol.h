#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace esphome {
namespace subzero_protocol {
struct Version {
  std::optional<std::string> fw;
  std::optional<std::string> api;
  std::optional<std::string> bleapp;
  std::optional<std::string> os;
  std::optional<std::string> rtapp;
  std::optional<std::string> appliance;
};

struct CommonFields {
  std::optional<std::string> pin_confirmed;
  std::optional<bool> sabbath_on;
  std::optional<bool> service_required;
  std::optional<std::string> appliance_model;
  std::optional<std::string> uptime;
  std::optional<std::string> appliance_serial;
  std::optional<std::string> appliance_type;
  std::optional<std::string> diagnostic_status;
  std::optional<std::string> build_date;
  Version version;
};

struct FridgeState {
  bool valid = false;
  // true = full poll response (status/resp); false = push notification
  // (seq/props). Used by the lambdas to distinguish "my poll got a response"
  // from "an unrelated push arrived" — critical for detecting unlock-session
  // expiry where polls stop responding but pushes keep flowing.
  bool is_poll = false;
  // Top-level keys present in the data object (resp or props), in order.
  // Populated on every parse; the lambdas log these when debug mode is on.
  std::vector<std::string> data_keys;
  std::optional<std::string> notif_event;
  CommonFields common;
  std::optional<float> ref_set_temp;
  std::optional<bool> door_ajar;
  std::optional<float> frz_set_temp;
  std::optional<bool> frz_door_ajar;
  std::optional<bool> ice_maker_on;
  std::optional<float> ref2_set_temp;
  std::optional<bool> ref2_door_ajar;
  std::optional<bool> wine_door_ajar;
  std::optional<float> wine_set_temp;
  std::optional<float> wine2_set_temp;
  std::optional<bool> wine_temp_alert_on;
  std::optional<float> crisp_set_temp;
  // Confirmed 2026-07-25 via live BLE testing: crisp_set_temp writes are
  // silently ignored whenever crisp_temp_mode == 1 (the app's "Automatic
  // crisper temperature" toggle, on by default). Writes only take effect
  // once this is set to 0 (manual). Integer 0/1 on the wire, not a JSON
  // bool.
  std::optional<int> crisp_temp_mode;
  std::optional<bool> air_filter_on;
  std::optional<float> air_filter_pct_remaining;
  std::optional<float> water_filter_pct_remaining;
  std::optional<float> water_filter_gal_remaining;
  // Raw date string from the appliance (observed as "YYYY-MM-DD"). Converted
  // to an ISO8601 timestamp ("YYYY-MM-DDT00:00:00+00:00") by the parser so
  // Home Assistant's timestamp device_class accepts it.
  std::optional<std::string> water_filter_end_date;

  // Vacation / ice modes.
  std::optional<bool> long_vacation_on;
  std::optional<bool> short_vacation_on;
  std::optional<bool> high_use_on;
  std::optional<std::string> high_use_start_time;
  std::optional<std::string> high_use_end_time;
  // Integer mode enum, not a boolean — observed values 0/1 in the wild but
  // the appliance may support more; expose the raw int rather than guess
  // at a mapping.
  std::optional<int> night_mode;
  std::optional<bool> night_ice_on;
  std::optional<bool> max_ice_on;
  std::optional<std::string> max_ice_start_time;
  std::optional<std::string> max_ice_end_time;

  // Power / smart grid.
  std::optional<bool> unit_on;
  std::optional<bool> smart_grid_on;

  // Misc diagnostics.
  std::optional<bool> pin_window_open;
  // Only populated when the appliance sends a plain string. Observed as
  // `null` on every unit tested so far; shape when populated (string vs.
  // array/bitmask) is unconfirmed.
  std::optional<std::string> active_faults;
  std::optional<int> humidity_control;
  std::optional<int> door_ajar_timeout;

  // WiFi diagnostics (the appliance's own view of its AP connection).
  std::optional<std::string> ap_ssid;
  std::optional<int> ap_rssi;
  std::optional<int> ap_chan;
  std::optional<int> ap_enc;
};

struct DishwasherState {
  bool valid = false;
  bool is_poll = false;
  std::vector<std::string> data_keys;
  std::optional<std::string> notif_event;
  CommonFields common;
  std::optional<bool> door_ajar;
  std::optional<bool> wash_cycle_on;
  std::optional<bool> heated_dry_on;
  std::optional<bool> extended_dry_on;
  std::optional<bool> high_temp_wash_on;
  std::optional<bool> sani_rinse_on;
  std::optional<bool> rinse_aid_low;
  std::optional<bool> softener_low;
  std::optional<bool> light_on;
  std::optional<bool> remote_ready;
  std::optional<bool> delay_start_timer_active;
  std::optional<int> wash_status;
  std::optional<int> wash_cycle;
  std::optional<std::string> wash_cycle_end_time;
  std::optional<int> wash_time_remaining_min;
};

struct RangeState {
  bool valid = false;
  bool is_poll = false;
  std::vector<std::string> data_keys;
  std::optional<std::string> notif_event;
  CommonFields common;
  std::optional<bool> door_ajar;

  std::optional<bool> cav_unit_on;
  std::optional<bool> cav_at_set_temp;
  std::optional<bool> cav_light_on;
  std::optional<bool> cav_remote_ready;
  std::optional<bool> cav_probe_on;
  std::optional<bool> cav_probe_at_set_temp;
  std::optional<bool> cav_probe_within_10deg;
  std::optional<bool> cav_gourmet_mode_on;
  std::optional<int> cav_gourmet_recipe;
  std::optional<bool> cav_cook_timer_complete;
  std::optional<bool> cav_cook_timer_within_1min;
  std::optional<float> cav_temp;
  std::optional<float> cav_set_temp;
  std::optional<int> cav_cook_mode;
  std::optional<float> cav_probe_temp;
  std::optional<float> cav_probe_set_temp;

  std::optional<bool> kitchen_timer_active;
  std::optional<bool> kitchen_timer_complete;
  std::optional<bool> kitchen_timer_within_1min;
  std::optional<std::string> kitchen_timer_end_time;
  std::optional<bool> kitchen_timer2_active;
  std::optional<bool> kitchen_timer2_complete;
  std::optional<bool> kitchen_timer2_within_1min;
  std::optional<std::string> kitchen_timer2_end_time;

  std::optional<bool> cav2_unit_on;
  std::optional<bool> cav2_door_ajar;
  std::optional<bool> cav2_at_set_temp;
  std::optional<bool> cav2_light_on;
  std::optional<bool> cav2_remote_ready;
  std::optional<bool> cav2_probe_on;
  std::optional<bool> cav2_probe_at_set_temp;
  std::optional<bool> cav2_probe_within_10deg;
  std::optional<bool> cav2_gourmet_mode_on;
  std::optional<bool> cav2_cook_timer_complete;
  std::optional<float> cav2_temp;
  std::optional<float> cav2_set_temp;
  std::optional<int> cav2_cook_mode;
  std::optional<float> cav2_probe_temp;
  std::optional<float> cav2_probe_set_temp;
};

FridgeState parse_fridge(const std::string &json);
DishwasherState parse_dishwasher(const std::string &json);
RangeState parse_range(const std::string &json);

// Converts the appliance's `uptime` string to a total number of seconds.
//
// The wire format is H:MM:SS with an unbounded hour field ("50:17:54",
// "959:52:0"). This was confirmed against an appliance whose clock had
// never been set: it reported time "2000-01-05T03:50:10" - i.e. 99:50:10
// since the 2000 epoch - alongside uptime "99:50:08", the two fields
// being sampled ~2s apart.
//
// Most firmware truncates the string to 8 characters, which clips the
// last seconds digit once the hour field reaches three digits
// ("627:09:3" is "627:09:3X"). The resulting error is under 10 seconds on
// a value measured in hundreds of hours, so the seconds field is parsed
// as-is rather than being discarded. At four digits of hours the same
// truncation swallows the seconds field entirely ("1000:00:"); that form
// is accepted with seconds treated as zero.
//
// Returns nullopt for anything that isn't three colon-separated fields of
// digits within clock range, so a malformed value leaves the sensor
// unknown instead of publishing a wrong number. Every duration that fits
// in the return type is accepted.
std::optional<std::uint32_t> parse_uptime_seconds(const std::string &v);

// Decides whether a freshly-reported wash cycle end time is worth pushing
// to Home Assistant, given the last value actually published.
//
// The appliance re-estimates the end time on every poll and wobbles a
// minute or two either way - observed live going 17:09 -> 17:10 -> 17:09
// -> 17:11 - and each distinct string became a logbook row. An end time
// is a target, not a countdown, so that wobble is noise; only a genuine
// revision is worth reporting.
//
// Compare against the last *published* value rather than the last one
// seen, so slow but real drift still lands once it accumulates past the
// threshold instead of being suppressed forever.
//
// Fails open: an empty `last` (nothing published yet) or either side
// being unparseable returns true, so a value is never silently swallowed.
bool end_time_revision_is_material(const std::string &last,
                                   const std::string &candidate,
                                   int threshold_min);

} // namespace subzero_protocol
} // namespace esphome
