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
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

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
};

struct FridgeBus : CommonBus {
  esphome::binary_sensor::BinarySensor *door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *frz_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *ice_maker = nullptr;
  esphome::binary_sensor::BinarySensor *ref2_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *wine_door_ajar = nullptr;
  esphome::binary_sensor::BinarySensor *wine_temp_alert = nullptr;
  esphome::binary_sensor::BinarySensor *air_filter_on = nullptr;

  esphome::sensor::Sensor *set_temp = nullptr;
  esphome::sensor::Sensor *frz_set_temp = nullptr;
  esphome::sensor::Sensor *ref2_set_temp = nullptr;
  esphome::sensor::Sensor *wine_set_temp = nullptr;
  esphome::sensor::Sensor *wine2_set_temp = nullptr;
  esphome::sensor::Sensor *crisp_set_temp = nullptr;
  esphome::sensor::Sensor *air_filter_pct = nullptr;
  esphome::sensor::Sensor *water_filter_pct = nullptr;

  void publish_door_ajar(bool v) { detail::publish_if(door_ajar, v); }
  void publish_frz_door_ajar(bool v) { detail::publish_if(frz_door_ajar, v); }
  void publish_ice_maker(bool v) { detail::publish_if(ice_maker, v); }
  void publish_ref2_door_ajar(bool v) { detail::publish_if(ref2_door_ajar, v); }
  void publish_wine_door_ajar(bool v) { detail::publish_if(wine_door_ajar, v); }
  void publish_wine_temp_alert(bool v) {
    detail::publish_if(wine_temp_alert, v);
  }
  void publish_air_filter_on(bool v) { detail::publish_if(air_filter_on, v); }

  void publish_set_temp(float v) { detail::publish_if(set_temp, v); }
  void publish_frz_set_temp(float v) { detail::publish_if(frz_set_temp, v); }
  void publish_ref2_set_temp(float v) { detail::publish_if(ref2_set_temp, v); }
  void publish_wine_set_temp(float v) { detail::publish_if(wine_set_temp, v); }
  void publish_wine2_set_temp(float v) {
    detail::publish_if(wine2_set_temp, v);
  }
  void publish_crisp_set_temp(float v) {
    detail::publish_if(crisp_set_temp, v);
  }
  void publish_air_filter_pct(float v) {
    detail::publish_if(air_filter_pct, v);
  }
  void publish_water_filter_pct(float v) {
    detail::publish_if(water_filter_pct, v);
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
  esphome::binary_sensor::BinarySensor *cav_light_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav_remote_ready = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_at_temp = nullptr;
  esphome::binary_sensor::BinarySensor *cav_probe_near = nullptr;
  esphome::binary_sensor::BinarySensor *cav_gourmet = nullptr;
  esphome::binary_sensor::BinarySensor *cook_timer_done = nullptr;
  esphome::binary_sensor::BinarySensor *cook_timer_near = nullptr;

  esphome::sensor::Sensor *cav_temp = nullptr;
  esphome::sensor::Sensor *cav_set_temp = nullptr;
  esphome::sensor::Sensor *cav_cook_mode = nullptr;
  esphome::sensor::Sensor *cav_gourmet_recipe = nullptr;
  esphome::sensor::Sensor *probe_temp = nullptr;
  esphome::sensor::Sensor *probe_set_temp = nullptr;

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
  esphome::binary_sensor::BinarySensor *cav2_light_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_remote_ready = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_on = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_at_temp = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_probe_near = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_gourmet = nullptr;
  esphome::binary_sensor::BinarySensor *cav2_cook_timer_done = nullptr;

  esphome::sensor::Sensor *cav2_temp = nullptr;
  esphome::sensor::Sensor *cav2_set_temp = nullptr;
  esphome::sensor::Sensor *cav2_cook_mode = nullptr;
  esphome::sensor::Sensor *cav2_probe_temp = nullptr;
  esphome::sensor::Sensor *cav2_probe_set_temp = nullptr;

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
