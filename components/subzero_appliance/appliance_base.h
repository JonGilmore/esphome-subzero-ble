#pragma once

#ifdef USE_ESP32

#include "esp_idf_transport.h"
#include "esphome_scheduler.h"
#include "hub.h"

#include "../subzero_protocol/dispatch_esphome.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/button/button.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include <string>

namespace esphome {
namespace subzero_appliance {

// Common base for FridgeAppliance / DishwasherAppliance / RangeAppliance.
//
// Inherits from Component (for setup() + set_interval()) and
// BLEClientNode (for gattc_event_handler routing). Holds all the
// state-machine collaborators (hub, transport, scheduler) plus the
// callback wiring for the status text sensor and PIN text input.
//
// Subclasses provide:
//   * `hub()` — pointer to their typed `*Hub` (FridgeHub etc.)
//   * `common_bus()` — pointer to the type's bus (which inherits CommonBus)
//   * `wire_bus_()` — sets the typed bus pointer on their hub
class ApplianceBase : public esphome::Component,
                      public esphome::ble_client::BLEClientNode {
public:
  // ---- Configuration setters (called from Python codegen) ----

  void set_pin(const std::string &pin) { pending_pin_ = pin; }
  void set_appliance_name(const std::string &name) { name_str_ = name; }
  void set_poll_offset_ms(std::uint32_t ms) { poll_offset_ms_ = ms; }

  // Status / PIN entities
  void set_status_text_sensor(esphome::text_sensor::TextSensor *s) {
    status_ts_ = s;
  }
  void set_pin_input(esphome::text::Text *t) { pin_input_ = t; }
  // Debug Mode switch — held so press_log_debug_info() can flip the HA
  // UI state when the user clicks the "Log Debug Info" button
  void set_debug_switch(esphome::switch_::Switch *s) { debug_switch_ = s; }

  // CommonBus setters — match the fields on subzero_protocol::CommonBus.
  // Subclass's bus inherits CommonBus, so writing through common_bus()
  // hits the right (shared) members regardless of appliance type.
  void set_sabbath_on_sensor(esphome::binary_sensor::BinarySensor *s) {
    common_bus()->sabbath_on = s;
  }
  void set_svc_required_sensor(esphome::binary_sensor::BinarySensor *s) {
    common_bus()->svc_required = s;
  }
  void set_model_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->model = s;
  }
  void set_uptime_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->uptime = s;
  }
  void set_serial_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->serial = s;
  }
  void set_appliance_type_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->appliance_type = s;
  }
  void set_diag_status_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->diag_status = s;
  }
  void set_build_date_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->build_date = s;
  }
  void set_fw_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->fw_version = s;
  }
  void set_api_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->api_version = s;
  }
  void set_bleapp_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->bleapp_version = s;
  }
  void set_os_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->os_version = s;
  }
  void set_rtapp_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->rtapp_version = s;
  }
  void set_board_version_sensor(esphome::text_sensor::TextSensor *s) {
    common_bus()->board_version = s;
  }

  // ---- ESPHome lifecycle ----

  void setup() override;
  float get_setup_priority() const override;

  // ---- BLE event routing ----

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t *param) override;

  // ---- Forwarded setters that need access to hub() ----

  // Used by ApplianceDebugSwitch::write_state.
  void set_debug_mode(bool enabled) { hub()->set_debug_mode(enabled); }

  // Used by AppliancePinText::control when the user types a new PIN
  // into the HA text input.
  void set_stored_pin_from_user(const std::string &pin) {
    hub()->set_stored_pin(pin);
  }

  // Used by ApplianceSetSwitch / ApplianceSetNumber when HA writes a
  // value. Forwards directly to the hub which handles JSON-formatting
  // and the D5 write.
  void write_set_bool(const std::string &key, bool value) {
    hub()->write_set_bool(key, value);
  }
  void write_set_int(const std::string &key, int value) {
    hub()->write_set_int(key, value);
  }
  void write_set_string(const std::string &key, const std::string &value) {
    hub()->write_set_string(key, value);
  }

  // ---- Button actions (called from ApplianceButton::press_action) ----

  // Connect: reset hub state and trigger ble_client connect.
  void press_connect();
  void press_disconnect();
  void press_start_pairing() { hub()->press_start_pairing(); }
  void press_submit_pin() { hub()->press_submit_pin(); }
  void press_poll() { hub()->press_poll(); }
  void press_log_debug_info();
  void press_reset_pairing();
  void press_clear_cloud_token() {
    write_set_string("remote_svc_reg_token", "");
  }

protected:
  // Subclass plug-in points
  virtual SubzeroHub *hub() = 0;
  virtual esphome::subzero_protocol::CommonBus *common_bus() = 0;
  virtual void wire_bus_() = 0;

  // Collaborators owned by this component (lifetime = ours)
  EspIdfTransport transport_;
  EsphomeScheduler scheduler_;

  // Config (set from Python codegen)
  std::string pending_pin_;
  std::string name_str_;
  std::uint32_t poll_offset_ms_ = 0;

  // Entity refs (set from Python codegen)
  esphome::text_sensor::TextSensor *status_ts_ = nullptr;
  esphome::text::Text *pin_input_ = nullptr;
  esphome::switch_::Switch *debug_switch_ = nullptr;
};

// One Button subclass for all 7 button actions. Python codegen
// instantiates 7 of these per appliance, sets the appropriate action
// kind, and registers each with HA via button.new_button.
enum class ApplianceButtonKind {
  kConnect,
  kDisconnect,
  kStartPairing,
  kSubmitPin,
  kPoll,
  kLogDebugInfo,
  kResetPairing,
  kClearCloudToken,
};

class ApplianceButton : public esphome::button::Button {
public:
  void set_parent(ApplianceBase *p) { parent_ = p; }
  void set_kind(ApplianceButtonKind k) { kind_ = k; }

protected:
  void press_action() override {
    if (parent_ == nullptr)
      return;
    switch (kind_) {
    case ApplianceButtonKind::kConnect:
      parent_->press_connect();
      break;
    case ApplianceButtonKind::kDisconnect:
      parent_->press_disconnect();
      break;
    case ApplianceButtonKind::kStartPairing:
      parent_->press_start_pairing();
      break;
    case ApplianceButtonKind::kSubmitPin:
      parent_->press_submit_pin();
      break;
    case ApplianceButtonKind::kPoll:
      parent_->press_poll();
      break;
    case ApplianceButtonKind::kLogDebugInfo:
      parent_->press_log_debug_info();
      break;
    case ApplianceButtonKind::kResetPairing:
      parent_->press_reset_pairing();
      break;
    case ApplianceButtonKind::kClearCloudToken:
      parent_->press_clear_cloud_token();
      break;
    }
  }

private:
  ApplianceBase *parent_ = nullptr;
  ApplianceButtonKind kind_ = ApplianceButtonKind::kConnect;
};

// Switch subclass for the Debug Mode toggle.
class ApplianceDebugSwitch : public esphome::switch_::Switch {
public:
  void set_parent(ApplianceBase *p) { parent_ = p; }

protected:
  void write_state(bool state) override {
    if (parent_ != nullptr) {
      // Forward to the hub via ApplianceBase. The switch's own state is
      // tracked by ESPHome; this just propagates to the hub's debug flag.
      // (ApplianceBase has hub() but it's protected; expose via setter.)
      parent_->set_debug_mode(state);
    }
    this->publish_state(state);
  }

private:
  ApplianceBase *parent_ = nullptr;
};

// Switch subclass for writable boolean properties (cav_light_on, sabbath_on,
// dishwasher light_on, etc.). One class powers all of them — the property
// key is wired in by Python codegen. write_state forwards to the hub via
// `set` on D5; the appliance acks then pushes the new value back on D6,
// which our normal read pipeline catches and publishes back via the
// dispatch bus, keeping the switch in sync. publish_state happens here too
// so the UI updates instantly without waiting for the round-trip echo.
class ApplianceSetSwitch : public esphome::switch_::Switch {
public:
  void set_parent(ApplianceBase *p) { parent_ = p; }
  void set_property_key(const std::string &k) { property_key_ = k; }

protected:
  void write_state(bool state) override {
    if (parent_ != nullptr && !property_key_.empty()) {
      parent_->write_set_bool(property_key_, state);
    }
    this->publish_state(state);
  }

private:
  ApplianceBase *parent_ = nullptr;
  std::string property_key_;
};

// Number subclass for writable numeric properties (set_temp, frz_set_temp,
// kitchen_timer_duration, etc.). Sub-Zero's protocol uses integers for all
// the writable numerics we've observed (temps in whole degrees F, timer
// durations in whole seconds), so control() rounds the float to int before
// formatting. publish_state echoes back so the UI updates instantly.
class ApplianceSetNumber : public esphome::number::Number {
public:
  void set_parent(ApplianceBase *p) { parent_ = p; }
  void set_property_key(const std::string &k) { property_key_ = k; }

protected:
  void control(float value) override {
    if (parent_ != nullptr && !property_key_.empty()) {
      parent_->write_set_int(property_key_, static_cast<int>(value));
    }
    this->publish_state(value);
  }

private:
  ApplianceBase *parent_ = nullptr;
  std::string property_key_;
};

// Text input subclass for the PIN field. esphome::text::Text is abstract
// (control() is pure virtual); we override it to forward the new value
// to the hub's stored_pin and publish the state back so the HA UI
// reflects what was entered.
class AppliancePinText : public esphome::text::Text {
public:
  void set_parent(ApplianceBase *p) { parent_ = p; }

protected:
  void control(const std::string &value) override {
    // Only publish when the hub actually accepts the value. Otherwise
    // an empty user-submit would push "" to HA while the hub keeps the
    // old PIN — the UI text field and the actual stored PIN would
    // desync. With this guard, an empty submit is silently ignored:
    // HA re-syncs to the server-side value (the previous PIN) and the
    // hub stays consistent.
    if (parent_ != nullptr && !value.empty()) {
      parent_->set_stored_pin_from_user(value);
      this->publish_state(value);
    }
  }

private:
  ApplianceBase *parent_ = nullptr;
};

} // namespace subzero_appliance
} // namespace esphome

#endif // USE_ESP32
