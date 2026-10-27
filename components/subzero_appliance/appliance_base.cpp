#ifdef USE_ESP32

#include "appliance_base.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>

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
  h->set_session_refresh_interval_ms(session_refresh_interval_ms_);
  h->set_metrics_callback([this]() { publish_metrics_(); });
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

  // poll_offset staggers the 60 s polls of appliances sharing this ESP32.
  this->set_timeout("subzero_poll_offset", poll_offset_ms_, [this]() {
    this->set_interval("subzero_periodic_poll", 60000, [this]() {
      hub()->do_periodic_poll();
      publish_data_age_();
    });
  });

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
      h->handle_disconnected(SubzeroHub::kReasonOpenFailed);
    }
    break;
  case ESP_GATTC_SEARCH_CMPL_EVT:
    if (param->search_cmpl.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] GATT search failed (status=%d)", name_str_.c_str(),
               static_cast<int>(param->search_cmpl.status));
      h->handle_disconnected(SubzeroHub::kReasonSearchFailed);
      break;
    }
    h->handle_connected();
    break;
  case ESP_GATTC_DISCONNECT_EVT:
    h->handle_disconnected(static_cast<int>(param->disconnect.reason));
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
  case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    if (param->reg_for_notify.status != ESP_GATT_OK) {
      ESP_LOGW(TAG,
               "[%s] Notify registration for handle 0x%04X failed "
               "(status=0x%02X)",
               name_str_.c_str(), param->reg_for_notify.handle,
               static_cast<int>(param->reg_for_notify.status));
    }
    break;
  // Without encryption every D5/D6 write is refused; make that visible.
  case ESP_GATTC_WRITE_CHAR_EVT:
  case ESP_GATTC_WRITE_DESCR_EVT:
    if (param->write.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] Write to handle 0x%04X refused (status=0x%02X%s)",
               name_str_.c_str(), param->write.handle,
               static_cast<int>(param->write.status),
               param->write.status == ESP_GATT_INSUF_ENCRYPTION
                   ? ": insufficient encryption"
               : param->write.status == ESP_GATT_INSUF_AUTHENTICATION
                   ? ": insufficient authentication"
                   : "");
    }
    break;
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
    return;
  }

  if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
    if (this->parent() == nullptr)
      return;
    const auto &ac = param->ble_security.auth_cmpl;
    if (std::memcmp(ac.bd_addr, this->parent()->get_remote_bda(), 6) != 0)
      return;
    hub()->handle_auth_complete(ac.success, static_cast<int>(ac.fail_reason),
                                static_cast<int>(ac.auth_mode));
  }
}

void ApplianceBase::publish_metrics_() {
  auto *h = hub();
  if (connections_s_ != nullptr)
    connections_s_->publish_state(h->sessions());
  if (missed_pushes_s_ != nullptr)
    missed_pushes_s_->publish_state(h->missed_pushes());
  if (gap_door_changes_s_ != nullptr)
    gap_door_changes_s_->publish_state(h->gap_door_changes());
  if (no_data_connections_s_ != nullptr)
    no_data_connections_s_->publish_state(h->sessions_without_snapshot());
  if (disconnect_reason_ts_ != nullptr && !h->last_disconnect_reason().empty())
    disconnect_reason_ts_->publish_state(h->last_disconnect_reason());
  publish_data_age_();
}

void ApplianceBase::publish_data_age_() {
  if (data_age_s_ == nullptr)
    return;
  const int age = hub()->snapshot_age_s();
  data_age_s_->publish_state(age < 0 ? NAN : static_cast<float>(age));
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
