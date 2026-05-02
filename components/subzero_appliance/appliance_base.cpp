#ifdef USE_ESP32

#include "appliance_base.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace subzero_appliance {

static const char *const TAG = "subzero_appliance";

void ApplianceBase::setup() {
  // Wire the transport + scheduler to ESPHome's runtime.
  transport_.set_client(this->parent());
  scheduler_.set_component(this);

  // Wire up the (typed) bus on the hub. Subclass overrides this to call
  // `hub_.set_bus(&typed_bus_)`.
  wire_bus_();

  // Configure the hub
  auto *h = hub();
  h->set_transport(&transport_);
  h->set_scheduler(&scheduler_);
  h->set_name(name_str_);
  h->set_poll_offset_ms(poll_offset_ms_);
  if (!pending_pin_.empty()) {
    h->set_stored_pin(pending_pin_);
    h->set_pin_confirmed(true);
  }

  // Status callback → publish to the status text sensor (if wired).
  if (status_ts_ != nullptr) {
    auto *ts = status_ts_;
    h->set_status_callback(
        [ts](const std::string &text) { ts->publish_state(text); });
  }

  // PIN-confirmation callback → update the HA text input + persist
  // through the hub's stored_pin.
  if (pin_input_ != nullptr) {
    auto *pi = pin_input_;
    h->set_pin_input_callback(
        [pi](const std::string &pin) { pi->publish_state(pin); });
  }

  // Subscribe-stage hook is intentionally NOT wired here. Phase 3's
  // subscribe_callback existed to inject discovered handles into the
  // ESPHome `ble_client::sensor` notify objects. In Phase 4 we don't
  // create those sensors at all — gattc_event_handler routes raw
  // ESP_GATTC_NOTIFY_EVT events directly to hub_.handle_d{5,6}_notify
  // by handle, so the ESPHome sensor's stale handle is irrelevant.

  // Periodic poll every 60s. set_interval is provided by Component.
  this->set_interval("subzero_periodic_poll", 60000,
                     [this]() { hub()->do_periodic_poll(); });

  ESP_LOGCONFIG(TAG, "[%s] Hub setup complete (poll_offset=%dms)",
                name_str_.c_str(), static_cast<int>(poll_offset_ms_));
}

float ApplianceBase::get_setup_priority() const {
  // After ble_client (which is AFTER_BLUETOOTH = 700) — we depend on
  // the parent BLE client being ready for transport_.set_client().
  return esphome::setup_priority::AFTER_BLUETOOTH - 1.0f;
}

void ApplianceBase::gattc_event_handler(esp_gattc_cb_event_t event,
                                        esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  auto *h = hub();
  switch (event) {
  case ESP_GATTC_OPEN_EVT:
    // Route OPEN-with-error to the hub's disconnect handler (cancels any
    // pending discovery timeouts; idempotent for a not-yet-connected
    // state). On OPEN-success we do NOT call handle_connected yet — the
    // GATT cache isn't populated until SEARCH_CMPL fires, and our
    // subscribe ladder needs handles+descriptors visible to bluedroid
    // (otherwise register_for_notify and CCCD writes silently no-op,
    // leaving the appliance unsubscribed and no data ever flows)
    if (param->open.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] GATT open failed (status=%d)", name_str_.c_str(),
               static_cast<int>(param->open.status));
      h->handle_disconnected();
    }
    break;
  case ESP_GATTC_SEARCH_CMPL_EVT:
    // bluedroid GATT db is populated, safe to discover handles, register
    // for notify & write CCCDs — but only if the search itself succeeded.
    // A failed search means the GATT db is empty/stale; routing to
    // handle_connected() would publish "Connected, discovering..." and
    // start the post-bond ladder against handles that don't exist.
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
    // Route notification to D5 or D6 handler based on which handle
    // it arrived on. The hub keeps these handles internally. Defensive
    // guard: a stray notify with handle 0 (or any case where the hub
    // hasn't discovered D5/D6 yet, so its handles are still 0) must
    // NOT match — without this, `param->notify.handle == h->d5_handle()`
    // succeeds at 0 == 0 and we'd dispatch garbage as a D5 heartbeat.
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
    // Match the address; the same callback fires for every BLE client
    // on the bus, so confirm this one is for our parent before responding.
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
  // Reset hub state FIRST (clears phase_ to 0, zeros handles, drops
  // pending timeouts), then ask the transport to (re)dial. The order
  // matters: if we called transport_.connect() first and the BLEClient
  // happened to be already-connected (e.g. user double-pressed Connect),
  // BLEClient::connect() short-circuits via internal duplicate-detection
  // and we'd end up with stale handles in the hub. Reset-then-connect
  // ensures the next on_connect callback enters the cold-discovery path.
  hub()->press_connect();
  transport_.connect();
}

void ApplianceBase::press_disconnect() { transport_.disconnect(); }

void ApplianceBase::press_log_debug_info() {
  // Sync the HA Debug Mode switch state BEFORE forwarding to the hub —
  // the hub's press_log_debug_info() immediately disconnects, so any
  // post-disconnect publish would race with the BLE teardown.
  if (debug_switch_ != nullptr) {
    debug_switch_->publish_state(true);
  }
  hub()->press_log_debug_info();
}

void ApplianceBase::press_reset_pairing() {
  hub()->press_reset_pairing();
  transport_.disconnect();
  // remove_bond was already called by hub.press_reset_pairing via
  // transport_.remove_bond(); the YAML's old `ble_client.remove_bond:`
  // action did the same thing. No additional 1s-delayed action needed.
}

} // namespace subzero_appliance
} // namespace esphome

#endif // USE_ESP32
