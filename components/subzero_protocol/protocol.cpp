#include "protocol.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace subzero_protocol {
namespace {
std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t");
  return s.substr(start, end - start + 1);
}

std::optional<std::string> opt_str(JsonVariantConst v) {
  if (!v.is<const char *>())
    return std::nullopt;
  const char *s = v.as<const char *>();
  return s ? std::optional<std::string>(s) : std::nullopt;
}

std::optional<bool> opt_bool(JsonVariantConst v) {
  if (!v.is<bool>())
    return std::nullopt;
  return v.as<bool>();
}

std::optional<int> opt_int(JsonVariantConst v) {
  if (!v.is<int>())
    return std::nullopt;
  return v.as<int>();
}

std::optional<float> opt_float(JsonVariantConst v) {
  if (!v.is<float>())
    return std::nullopt;
  return v.as<float>();
}

// Returns the data object (resp for polls, props for pushes) and sets is_poll.
// Returns a null object if the format is unrecognized or status != 0.
void capture_keys(JsonObjectConst data, std::vector<std::string> &out) {
  for (JsonPairConst kv : data) {
    out.emplace_back(kv.key().c_str());
  }
}

JsonObjectConst extract_data(JsonObjectConst root, bool &is_poll) {
  if (root["status"].is<int>()) {
    if (root["status"].as<int>() != 0)
      return JsonObjectConst();
    is_poll = true;
    return root["resp"].as<JsonObjectConst>();
  }
  if (root["props"].is<JsonObjectConst>()) {
    is_poll = false;
    return root["props"].as<JsonObjectConst>();
  }
  if (root["msg_types"].is<int>() && root["msg_types"].as<int>() == 1) {
    is_poll = false;
    return root;
  }
  return JsonObjectConst();
}

void fill_version(JsonObjectConst v, Version &out) {
  out.fw = opt_str(v["fw"]);
  out.api = opt_str(v["api"]);
  out.bleapp = opt_str(v["bleapp"]);
  out.os = opt_str(v["os"]);
  out.rtapp = opt_str(v["rtapp"]);
  out.appliance = opt_str(v["appliance"]);
}

void fill_common(JsonObjectConst data, CommonFields &out) {
  if (data["pin"].is<const char *>()) {
    const char *pin = data["pin"].as<const char *>();
    if (pin && std::strlen(pin) <= 10) {
      out.pin_confirmed = std::string(pin);
    }
  }
  out.sabbath_on = opt_bool(data["sabbath_on"]);
  out.service_required = opt_bool(data["service_required"]);
  out.appliance_model = opt_str(data["appliance_model"]);
  out.uptime = opt_str(data["uptime"]);
  if (auto s = opt_str(data["appliance_serial"])) {
    out.appliance_serial = trim(*s);
  }
  out.appliance_type = opt_str(data["appliance_type"]);
  out.diagnostic_status = opt_str(data["diagnostic_status"]);
  if (data["version"].is<JsonObjectConst>()) {
    fill_version(data["version"].as<JsonObjectConst>(), out.version);
  }
  if (data["build_info"].is<JsonObjectConst>()) {
    auto bi = data["build_info"].as<JsonObjectConst>();
    out.build_date = opt_str(bi["build_date"]);
  }
}

// Minutes between two ISO-8601 timestamps ("YYYY-MM-DDTHH:MM[:SS...]").
// Returns nullopt if either string can't be parsed. Uses day-of-month math,
// which matches the legacy lambda behavior (adequate for wash cycles under
// a month). Same-month assumption.
std::optional<int> minutes_between(const char *now_iso,
                                   const std::string &end_iso) {
  if (end_iso.size() < 16)
    return std::nullopt;
  int ey, emo, ed, eh, emi;
  if (std::sscanf(end_iso.c_str(), "%d-%d-%dT%d:%d", &ey, &emo, &ed, &eh,
                  &emi) != 5) {
    return std::nullopt;
  }
  int cy, cmo, cd, ch, cmi, cs = 0;
  if (std::sscanf(now_iso, "%d-%d-%dT%d:%d:%d", &cy, &cmo, &cd, &ch, &cmi,
                  &cs) < 5) {
    return std::nullopt;
  }
  int end_mins = (ed * 24 * 60) + (eh * 60) + emi;
  int cur_mins = (cd * 24 * 60) + (ch * 60) + cmi;
  int remaining = end_mins - cur_mins;
  return remaining < 0 ? 0 : remaining;
}

} // namespace

FridgeState parse_fridge(const std::string &json) {
  FridgeState state;
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return state;
  if (!doc.is<JsonObject>())
    return state;
  bool is_poll = true;
  JsonObjectConst data = extract_data(doc.as<JsonObjectConst>(), is_poll);
  if (data.isNull())
    return state;
  state.valid = true;
  state.is_poll = is_poll;
  capture_keys(data, state.data_keys);
  fill_common(data, state.common);

  // Setpoint with freezer-only fallback.
  if (data["ref_set_temp"].is<float>()) {
    state.ref_set_temp = data["ref_set_temp"].as<float>();
  } else if (data["frz_set_temp"].is<float>()) {
    state.ref_set_temp = data["frz_set_temp"].as<float>();
  }
  // Door with generic fallback.
  if (data["ref_door_ajar"].is<bool>()) {
    state.door_ajar = data["ref_door_ajar"].as<bool>();
  } else if (data["door_ajar"].is<bool>()) {
    state.door_ajar = data["door_ajar"].as<bool>();
  }
  state.frz_set_temp = opt_float(data["frz_set_temp"]);
  state.frz_door_ajar = opt_bool(data["frz_door_ajar"]);
  state.ice_maker_on = opt_bool(data["ice_maker_on"]);
  state.ref2_set_temp = opt_float(data["ref2_set_temp"]);
  state.ref2_door_ajar = opt_bool(data["ref2_door_ajar"]);
  state.wine_door_ajar = opt_bool(data["wine_door_ajar"]);
  state.wine_set_temp = opt_float(data["wine_set_temp"]);
  state.wine2_set_temp = opt_float(data["wine2_set_temp"]);
  state.wine_temp_alert_on = opt_bool(data["wine_temp_alert_on"]);
  state.crisp_set_temp = opt_float(data["crisp_set_temp"]);
  state.air_filter_on = opt_bool(data["air_filter_on"]);
  state.air_filter_pct_remaining = opt_float(data["air_filter_pct_remaining"]);
  state.water_filter_pct_remaining =
      opt_float(data["water_filter_pct_remaining"]);
  return state;
}

DishwasherState parse_dishwasher(const std::string &json) {
  DishwasherState state;
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return state;
  if (!doc.is<JsonObject>())
    return state;
  bool is_poll = true;
  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst data = extract_data(root, is_poll);
  if (data.isNull())
    return state;
  state.valid = true;
  state.is_poll = is_poll;
  capture_keys(data, state.data_keys);
  fill_common(data, state.common);

  state.door_ajar = opt_bool(data["door_ajar"]);
  state.wash_cycle_on = opt_bool(data["wash_cycle_on"]);
  state.heated_dry_on = opt_bool(data["heated_dry_on"]);
  state.extended_dry_on = opt_bool(data["extended_dry_on"]);
  state.high_temp_wash_on = opt_bool(data["high_temp_wash_on"]);
  state.sani_rinse_on = opt_bool(data["sani_rinse_on"]);
  state.rinse_aid_low = opt_bool(data["rinse_aid_low"]);
  state.softener_low = opt_bool(data["softener_low"]);
  state.light_on = opt_bool(data["light_on"]);
  state.remote_ready = opt_bool(data["remote_ready"]);
  state.delay_start_timer_active = opt_bool(data["delay_start_timer_active"]);
  state.wash_status = opt_int(data["wash_status"]);
  state.wash_cycle = opt_int(data["wash_cycle"]);
  state.wash_cycle_end_time = opt_str(data["wash_cycle_end_time"]);

  if (state.wash_cycle_end_time) {
    const char *now_iso = nullptr;
    if (root["timestamp"].is<const char *>()) {
      now_iso = root["timestamp"].as<const char *>();
    } else if (is_poll && data["time"].is<const char *>()) {
      now_iso = data["time"].as<const char *>();
    }
    if (now_iso) {
      state.wash_time_remaining_min =
          minutes_between(now_iso, *state.wash_cycle_end_time);
    }
  }
  return state;
}

RangeState parse_range(const std::string &json) {
  RangeState state;
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return state;
  if (!doc.is<JsonObject>())
    return state;
  bool is_poll = true;
  JsonObjectConst data = extract_data(doc.as<JsonObjectConst>(), is_poll);
  if (data.isNull())
    return state;
  state.valid = true;
  state.is_poll = is_poll;
  capture_keys(data, state.data_keys);
  fill_common(data, state.common);

  // Door with generic fallback.
  if (data["cav_door_ajar"].is<bool>()) {
    state.door_ajar = data["cav_door_ajar"].as<bool>();
  } else if (data["door_ajar"].is<bool>()) {
    state.door_ajar = data["door_ajar"].as<bool>();
  }

  state.cav_unit_on = opt_bool(data["cav_unit_on"]);
  state.cav_at_set_temp = opt_bool(data["cav_at_set_temp"]);
  state.cav_light_on = opt_bool(data["cav_light_on"]);
  state.cav_remote_ready = opt_bool(data["cav_remote_ready"]);
  state.cav_probe_on = opt_bool(data["cav_probe_on"]);
  state.cav_probe_at_set_temp = opt_bool(data["cav_probe_at_set_temp"]);
  state.cav_probe_within_10deg = opt_bool(data["cav_probe_within_10deg"]);
  state.cav_gourmet_mode_on = opt_bool(data["cav_gourmet_mode_on"]);
  state.cav_gourmet_recipe = opt_int(data["cav_gourmet_recipe"]);
  state.cav_cook_timer_complete = opt_bool(data["cav_cook_timer_complete"]);
  state.cav_cook_timer_within_1min =
      opt_bool(data["cav_cook_timer_within_1min"]);
  state.cav_temp = opt_float(data["cav_temp"]);
  state.cav_set_temp = opt_float(data["cav_set_temp"]);
  state.cav_cook_mode = opt_int(data["cav_cook_mode"]);
  state.cav_probe_temp = opt_float(data["cav_probe_temp"]);
  state.cav_probe_set_temp = opt_float(data["cav_probe_set_temp"]);

  state.kitchen_timer_active = opt_bool(data["kitchen_timer_active"]);
  state.kitchen_timer_complete = opt_bool(data["kitchen_timer_complete"]);
  state.kitchen_timer_within_1min = opt_bool(data["kitchen_timer_within_1min"]);
  state.kitchen_timer_end_time = opt_str(data["kitchen_timer_end_time"]);
  state.kitchen_timer2_active = opt_bool(data["kitchen_timer2_active"]);
  state.kitchen_timer2_complete = opt_bool(data["kitchen_timer2_complete"]);
  state.kitchen_timer2_within_1min =
      opt_bool(data["kitchen_timer2_within_1min"]);
  state.kitchen_timer2_end_time = opt_str(data["kitchen_timer2_end_time"]);

  state.cav2_unit_on = opt_bool(data["cav2_unit_on"]);
  state.cav2_door_ajar = opt_bool(data["cav2_door_ajar"]);
  state.cav2_at_set_temp = opt_bool(data["cav2_at_set_temp"]);
  state.cav2_light_on = opt_bool(data["cav2_light_on"]);
  state.cav2_remote_ready = opt_bool(data["cav2_remote_ready"]);
  state.cav2_probe_on = opt_bool(data["cav2_probe_on"]);
  state.cav2_probe_at_set_temp = opt_bool(data["cav2_probe_at_set_temp"]);
  state.cav2_probe_within_10deg = opt_bool(data["cav2_probe_within_10deg"]);
  state.cav2_gourmet_mode_on = opt_bool(data["cav2_gourmet_mode_on"]);
  state.cav2_cook_timer_complete = opt_bool(data["cav2_cook_timer_complete"]);
  state.cav2_temp = opt_float(data["cav2_temp"]);
  state.cav2_set_temp = opt_float(data["cav2_set_temp"]);
  state.cav2_cook_mode = opt_int(data["cav2_cook_mode"]);
  state.cav2_probe_temp = opt_float(data["cav2_probe_temp"]);
  state.cav2_probe_set_temp = opt_float(data["cav2_probe_set_temp"]);
  return state;
}
} // namespace subzero_protocol
} // namespace esphome
