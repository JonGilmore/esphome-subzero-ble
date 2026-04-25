#pragma once

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
  std::optional<bool> air_filter_on;
  std::optional<float> air_filter_pct_remaining;
  std::optional<float> water_filter_pct_remaining;
};

struct DishwasherState {
  bool valid = false;
  bool is_poll = false;
  std::vector<std::string> data_keys;
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

} // namespace subzero_protocol
} // namespace esphome
