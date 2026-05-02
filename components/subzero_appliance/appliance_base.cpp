#ifdef USE_ESP32

#include "appliance_base.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace subzero_appliance {

static const char *const TAG = "subzero_appliance";

void ApplianceBase::setup() {
  transport_.set_client(this->parent());
  scheduler_.set_component(this);

  wire_bus_();

  auto *h = hub();
  h->set_transport(&transport_);
  h->set_scheduler(&scheduler_);
  h->set_name(name_str_);
  h->set_poll_offset_ms(poll_offset_ms_);
  if (!pending_pin_.empty()) {
    h->set_stored_pin(pending_pin_);
    h->set_pin_confirmed(true);
  }

  if (status_ts_ != nullptr) {
    auto *ts = status_ts_;
    h->set_status_callback(
        [ts](const std::string &text) { ts->publish_state(text); });
  }

  if (pin_input_ != nullptr) {
    auto *pi = pin_input_;
    h->set_pin_input_callback(
        [pi](const std::string &pin) { pi->publish_state(pin); });
  }

  this->set_interval("subzero_periodic_poll", 60000,
                     [this]() { hub()->do_periodic_poll(); });

  ESP_LOGCONFIG(TAG, "[%s] Hub setup complete (poll_offset=%dms)",
                name_str_.c_str(), static_cast<int>(poll_offset_ms_));
}

float ApplianceBase::get_setup_priority() const {
  return esphome::setup_priority::AFTER_BLUETOOTH - 1.0f;
}

void ApplianceBase::gattc_event_handler(esp_gattc_cb_event_t event,
                                        esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  auto *h = hub();
  switch (event) {
  case ESP_GATTC_OPEN_EVT:
    if (param->open.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] GATT open failed (status=%d)", name_str_.c_str(),
               static_cast<int>(param->open.status));
      h->handle_disconnected();
    }
    break;
  case ESP_GATTC_SEARCH_CMPL_EVT:
    if (param->search_cmpl.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] GATT search failed (status=%d)", name_str_.c_str(),
               static_cast<int>(param->search_cmpl.status));
      h->handle_disconnected();
      break;
    }
    h->handle_connected();
    break;
  case ESP_GATTC_DISCONNECT_EVT:
    h->handle_disconnected();
    break;
  case ESP_GATTC_NOTIFY_EVT: {
    std::uint16_t nh = param->notify.handle;
    std::uint16_t d5 = h->d5_handle();
    std::uint16_t d6 = h->d6_handle();
    if (nh == 0)
      break;
    if (d5 != 0 && nh == d5) {
      h->handle_d5_notify(param->notify.value, param->notify.value_len);
    } else if (d6 != 0 && nh == d6) {
      h->handle_d6_notify(param->notify.value, param->notify.value_len);
    }
    break;
  }
  default:
    break;
  }
}

void ApplianceBase::gap_event_handler(esp_gap_ble_cb_event_t event,
                                      esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT) {
    if (this->parent() == nullptr)
      return;
    if (std::memcmp(param->ble_security.ble_req.bd_addr,
                    this->parent()->get_remote_bda(), 6) != 0) {
      return;
    }
    std::uint32_t passkey = hub()->handle_passkey_request();
    esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, passkey);
  }
}

void ApplianceBase::press_connect() {
  hub()->press_connect();
  transport_.connect();
}

void ApplianceBase::press_disconnect() { transport_.disconnect(); }

void ApplianceBase::press_log_debug_info() {
  if (debug_switch_ != nullptr) {
    debug_switch_->publish_state(true);
  }
  hub()->press_log_debug_info();
}

void ApplianceBase::press_reset_pairing() {
  hub()->press_reset_pairing();
  transport_.disconnect();
}

} // namespace subzero_appliance
} // namespace esphome

#endif // USE_ESP32
