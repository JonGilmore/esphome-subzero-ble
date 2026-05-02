#include "hub.h"

#include "../subzero_protocol/commands.h"
#include "../subzero_protocol/log_sanitize.h"
#include "../subzero_protocol/protocol.h"

#include <cstring>
#include <utility>

#ifdef USE_ESP32
#include "esphome/core/log.h"
#define HUB_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#define HUB_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#define HUB_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#define HUB_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
// Host build: ESPHome's logger headers aren't available. Stub the macros
// via a no-op template that "uses" every argument so callers (including
// the chunked-log lambda's idx/total/chunk params and the
// transport_->write result captured into `err`) don't trip
// -Wunused-{parameter,variable,lambda-capture}.
namespace esphome {
namespace subzero_appliance {
template <typename... Args> inline void hub_log_noop(Args &&.../*unused*/) {}
} // namespace subzero_appliance
} // namespace esphome
// Variadic-only form (no `,##` GNU extension) — every call site already
// passes at least a tag + format string, so __VA_ARGS__ is always non-empty.
#define HUB_LOGI(...) ::esphome::subzero_appliance::hub_log_noop(__VA_ARGS__)
#define HUB_LOGW(...) ::esphome::subzero_appliance::hub_log_noop(__VA_ARGS__)
#define HUB_LOGE(...) ::esphome::subzero_appliance::hub_log_noop(__VA_ARGS__)
#define HUB_LOGD(...) ::esphome::subzero_appliance::hub_log_noop(__VA_ARGS__)
#endif

namespace esphome {
namespace subzero_appliance {

namespace {
// Names used with Scheduler::set_timeout — re-using the same name
// effectively cancels-and-replaces the prior timeout. Matches the
// `mode: restart` semantics of the YAML scripts.
constexpr const char *kTimeoutPostBondInitial = "post_bond_initial";
constexpr const char *kTimeoutPostBondPostEnc = "post_bond_post_enc";
constexpr const char *kTimeoutPostBondSearch = "post_bond_search";
constexpr const char *kTimeoutPostBondPoll1 = "post_bond_poll1";
constexpr const char *kTimeoutPostBondPoll2 = "post_bond_poll2";
constexpr const char *kTimeoutPostBondPoll3 = "post_bond_poll3";
constexpr const char *kTimeoutPostBondGiveup = "post_bond_giveup";
constexpr const char *kTimeoutSubscribeUnlock = "subscribe_unlock";
constexpr const char *kTimeoutSubscribeGet = "subscribe_get";
constexpr const char *kTimeoutFastReconnect = "fast_reconnect";
constexpr const char *kTimeoutSubmitPinPoll = "submit_pin_poll";
constexpr const char *kTimeoutVerbFallbackRetry = "verb_fallback_retry";
} // namespace

// =============================================================================
// Lifecycle handlers — translated from the on_connect / on_disconnect /
// on_passkey_request triggers in subzero_base.yaml.
// =============================================================================

void SubzeroHub::handle_connected() {
  if (transport_ == nullptr || scheduler_ == nullptr)
    return;
  transport_->request_mtu();

  // Fast path: handles cached from a previous (bonded) connection.
  if (d5_handle_ > 0 && phase_ >= 1) {
    HUB_LOGI("ble", "[%s] Reconnected (fast path, d5=%d, retries=%d)",
             name_.c_str(), d5_handle_, fast_retries_);
    publish_status_("Reconnected, encrypting...");
    start_fast_reconnect_();
    return;
  }

  // Cold path: never bonded, OR a previous post_bond left handles unset.
  if (phase_ == 0) {
    phase_ = 1;
    HUB_LOGI("ble", "[%s] Connected, discovering chars...", name_.c_str());
    publish_status_("Connected, discovering...");
    start_post_bond_();
    return;
  }

  if (d5_handle_ > 0) {
    HUB_LOGI("gatt", "[%s] D5 already found, skipping", name_.c_str());
    return;
  }
  if (post_bond_running_) {
    HUB_LOGI("ble", "[%s] post_bond still running, skipping", name_.c_str());
    return;
  }
  phase_ += 1;
  HUB_LOGI("ble", "[%s] Reconnected, starting discovery", name_.c_str());
  publish_status_("Reconnected, discovering...");
  start_post_bond_();
}

void SubzeroHub::handle_disconnected() {
  if (transport_ == nullptr || scheduler_ == nullptr)
    return;
  json_buf_.clear();
  scheduler_->cancel_timeout(kTimeoutPostBondInitial);
  scheduler_->cancel_timeout(kTimeoutPostBondPostEnc);
  scheduler_->cancel_timeout(kTimeoutPostBondSearch);
  scheduler_->cancel_timeout(kTimeoutPostBondPoll1);
  scheduler_->cancel_timeout(kTimeoutPostBondPoll2);
  scheduler_->cancel_timeout(kTimeoutPostBondPoll3);
  scheduler_->cancel_timeout(kTimeoutPostBondGiveup);
  scheduler_->cancel_timeout(kTimeoutSubscribeUnlock);
  scheduler_->cancel_timeout(kTimeoutSubscribeGet);
  scheduler_->cancel_timeout(kTimeoutFastReconnect);
  scheduler_->cancel_timeout(kTimeoutSubmitPinPoll);
  scheduler_->cancel_timeout(kTimeoutVerbFallbackRetry);
  post_bond_running_ = false;
  subscribe_running_ = false;
  fast_reconnect_running_ = false;

  if (d5_handle_ > 0 && phase_ >= 1) {
    fast_retries_ += 1;
    if (fast_retries_ >= kStaleBondsThreshold) {
      HUB_LOGW("ble", "[%s] Stale bond (%d failures), clearing for re-pair",
               name_.c_str(), fast_retries_);
      transport_->remove_bond();
      clear_handles_();
      phase_ = 0;
      fast_retries_ = 0;
      publish_status_("Bond cleared, re-pairing on next connect...");
    } else {
      HUB_LOGI("ble", "[%s] Disconnected (handles cached, d5=%d, retries=%d)",
               name_.c_str(), d5_handle_, fast_retries_);
    }
  } else {
    phase_ = 0;
    clear_handles_();
    HUB_LOGI("ble", "[%s] Disconnected", name_.c_str());
  }
  publish_status_("Disconnected");
}

std::uint32_t SubzeroHub::handle_passkey_request() {
  if (stored_pin_.empty()) {
    HUB_LOGW("ble", "[%s] Passkey requested but no PIN set!", name_.c_str());
    publish_status_("Enter PIN in HA, then reconnect.");
    HUB_LOGI("ble", "[%s] Replying with PIN: (none)", name_.c_str());
    return 0;
  }
  HUB_LOGI("ble", "[%s] Replying with PIN: %s", name_.c_str(),
           stored_pin_.c_str());
  return static_cast<std::uint32_t>(std::atoi(stored_pin_.c_str()));
}

// =============================================================================
// Notify handlers — translated from the D5 / D6 BLE characteristic
// notify lambdas.
// =============================================================================

void SubzeroHub::handle_d5_notify(const std::uint8_t * /*data*/,
                                  std::size_t /*len*/) {
  // D5 indications are control-channel responses (display_pin /
  // unlock_channel / set / scan acks) and duplicate copies of D6 push
  // notifications — never poll responses. Setting poll_ok_=true here
  // would mask the fw 8.5 silent-D6-poll case the same way D6 push
  // traffic used to (the appliance keeps acking D5 commands AND
  // mirroring D6 pushes onto D5 even when get_async on D6 has gone
  // silent). poll_ok_ flips only in process_message_complete_ on a
  // successful POLL RESPONSE — see CodeRabbit review on PR f795296.
  // Body intentionally empty.
}

void SubzeroHub::handle_d6_notify(const std::uint8_t *data, std::size_t len) {
  // Don't set poll_ok_ here — push notifications on D6 (msg_types:1
  // diagnostic_status, msg_types:2 props) would mask the fw 8.5
  // silent-poll case where the appliance keeps pushing but never
  // answers get_async. poll_ok_ is set in process_message_complete_
  // only when a parsed message is a real POLL RESPONSE (status:0).
  // The zombie detector then correctly forces a reconnect every ~3 min
  // on fw 8.5, which is the only way to refresh full state on those
  // devices (D6 unlock session expires server-side after ~60-80s of
  // idle).
  if (json_buf_.feed(data, len)) {
    process_message_complete_();
  }
}

void SubzeroHub::process_message_complete_() {
  auto msg = json_buf_.take_message();
  if (!msg)
    return;

  HUB_LOGI("szg", "[%s] Parsing JSON (%d bytes)", name_.c_str(),
           static_cast<int>(msg->size()));

  if (debug_mode_)
    log_chunked_debug_(*msg);

  bool ok = parse_and_dispatch_(*msg);
  if (!ok) {
    if (msg->find("\"status\":302") != std::string::npos) {
      HUB_LOGE("szg",
               "[%s] Pairing rejected (status 302). The PIN has likely "
               "changed - press 'Start Pairing' on the appliance and "
               "enter the new PIN in HA.",
               name_.c_str());
      publish_status_(
          "Pairing required - press Start Pairing and re-enter PIN");
      return;
    }
    if (handle_lacking_properties_(*msg))
      return;
    HUB_LOGW("szg", "[%s] Parse failed or status!=0, skipping (%s...)",
             name_.c_str(),
             esphome::subzero_protocol::sanitize_for_log(
                 *msg, 0, std::min<std::size_t>(80, msg->size()))
                 .c_str());
    return;
  }
  fast_retries_ = 0;
  // poll_ok_ tracks specifically successful POLL RESPONSES (full state),
  // not pushes. Setting it on incidental pushes (msg_types:1
  // diagnostic_status, msg_types:2 props) would mask the fw 8.5
  // silent-poll case — appliance keeps pushing but never answers
  // get_async, so the zombie detector must still trigger a reconnect.
  // has_status_value is whitespace-tolerant and digit-suffix-safe (won't
  // false-match `"status":01` or `"status": 0` or `"status":09`).
  if (esphome::subzero_protocol::has_status_value(*msg, '0')) {
    poll_ok_ = true;
  }
}

bool SubzeroHub::handle_lacking_properties_(const std::string &msg) {
  if (!esphome::subzero_protocol::is_lacking_properties_response(msg))
    return false;
  if (poll_verb_ == esphome::subzero_protocol::PollVerb::kGetAsync) {
    HUB_LOGW("szg",
             "[%s] Appliance returned 'lacking properties' to get_async; "
             "switching to {\"cmd\":\"get\"} fallback (verified working on "
             "IR36550ST per issue #91; not a verb the official app sends "
             "over BLE — see commands.h)",
             name_.c_str());
    poll_verb_ = esphome::subzero_protocol::PollVerb::kGet;
    publish_status_("Switching to get fallback...");
    if (scheduler_ != nullptr) {
      scheduler_->set_timeout(kTimeoutVerbFallbackRetry,
                              kVerbFallbackRetryDelayMs, [this]() {
                                if (transport_ == nullptr || d6_handle_ == 0)
                                  return;
                                write_poll_command_(d6_handle_);
                                publish_status_("Retrying with get...");
                              });
    }
    return true;
  }

  HUB_LOGE("szg",
           "[%s] Appliance returned 'lacking properties' to BOTH get_async "
           "AND get; firmware does not support state polling. Push "
           "notifications via CCCD may still work.",
           name_.c_str());
  publish_status_("Appliance does not support state polling");
  return true;
}

void SubzeroHub::on_pin_confirmed_(const std::string &pin) {
  stored_pin_ = pin;
  pin_confirmed_ = true;
  if (pin_input_cb_)
    pin_input_cb_(pin);
  HUB_LOGI("szg", "[%s] PIN confirmed: %s", name_.c_str(), pin.c_str());
  publish_status_("PIN confirmed! Channel unlocked.");
}

void SubzeroHub::log_data_keys_(const std::vector<std::string> &keys) {
  if (!debug_mode_)
    return;
  for (const auto &k : keys) {
    HUB_LOGI("szg-debug", "[%s] key=%s", name_.c_str(), k.c_str());
  }
}

void SubzeroHub::log_chunked_debug_(const std::string &msg) {
  std::string nm = name_;
  esphome::subzero_protocol::chunk_for_log(
      msg, [&nm](std::size_t idx, std::size_t total, const std::string &chunk) {
        HUB_LOGI("szg", "[%s] Response[%d/%d]: %s", nm.c_str(),
                 static_cast<int>(idx), static_cast<int>(total), chunk.c_str());
      });
}

// =============================================================================
// Periodic poll
// =============================================================================

void SubzeroHub::do_periodic_poll() {
  if (transport_ == nullptr)
    return;
  if (!pin_confirmed_)
    return;
  if (subscribe_running_ || post_bond_running_ || fast_reconnect_running_) {
    return;
  }
  if (d6_handle_ == 0)
    return;
  if (!transport_->connected())
    return;

  if (poll_ok_) {
    poll_miss_ = 0;
  } else {
    poll_miss_ += 1;
    if (poll_miss_ >= kZombiePollMissThreshold) {
      HUB_LOGW("szg",
               "[%s] Appliance unresponsive (%d intervals, no data), "
               "forcing reconnect",
               name_.c_str(), poll_miss_);
      poll_miss_ = 0;
      json_buf_.clear();
      publish_status_("Connection stale, reconnecting...");
      transport_->disconnect();
      return;
    }
  }
  poll_ok_ = false;
  json_buf_.clear();

  if (!stored_pin_.empty()) {
    write_unlock_channel_(d6_handle_);
  }
  std::string cmd = esphome::subzero_protocol::build_poll_command(poll_verb_);
  BleResult err = transport_->write(
      d6_handle_, reinterpret_cast<const std::uint8_t *>(cmd.data()),
      cmd.size());
  if (err != BleResult::kOk) {
    HUB_LOGW("szg", "[%s] D6 write failed (err=%d), forcing reconnect",
             name_.c_str(), static_cast<int>(err));
    publish_status_("Write failed, reconnecting...");
    transport_->disconnect();
    return;
  }
  HUB_LOGI("szg", "[%s] Periodic poll D6 (verb=%s, miss=%d, retries=%d)",
           name_.c_str(),
           poll_verb_ == esphome::subzero_protocol::PollVerb::kGet
               ? "get"
               : "get_async",
           poll_miss_, fast_retries_);
}

// =============================================================================
// post_bond — 5-stage discovery ladder
// =============================================================================

void SubzeroHub::start_post_bond_() {
  post_bond_running_ = true;
  scheduler_->set_timeout(kTimeoutPostBondInitial,
                          poll_offset_ms_ + kPostBondInitialDelayMs,
                          [this]() { post_bond_initial_(); });
}

void SubzeroHub::post_bond_initial_() {
  update_handles_from_db_();

  HUB_LOGI("ble", "[%s] D5 %s. Requesting encryption...", name_.c_str(),
           d5_handle_ > 0 ? "found immediately" : "not yet visible");
  transport_->request_encryption();
  if (d5_handle_ > 0) {
    publish_status_("D5 found! Encrypting...");
  } else {
    publish_status_("Requesting encryption...");
  }

  scheduler_->set_timeout(kTimeoutPostBondPostEnc, kPostEncryptionDelayMs,
                          [this]() { post_bond_post_encryption_(); });
}

void SubzeroHub::post_bond_post_encryption_() {
  if (d5_handle_ > 0) {
    HUB_LOGI("ble", "[%s] Post-encryption: D5 already known, subscribing...",
             name_.c_str());
    post_bond_running_ = false;
    start_subscribe_();
    return;
  }
  HUB_LOGI("ble", "[%s] Post-encryption: refreshing GATT cache...",
           name_.c_str());
  transport_->cache_refresh();

  scheduler_->set_timeout(kTimeoutPostBondSearch, kSearchPollDelayMs,
                          [this]() { post_bond_trigger_search_(); });
}

void SubzeroHub::post_bond_trigger_search_() {
  if (d5_handle_ > 0)
    return;
  transport_->search_service();

  scheduler_->set_timeout(kTimeoutPostBondPoll1, kPostBondPollPeriodMs,
                          [this]() { post_bond_poll_attempt_(1); });
}

void SubzeroHub::post_bond_poll_attempt_(int attempt) {
  if (d5_handle_ > 0)
    return;
  update_handles_from_db_();
  HUB_LOGI("poll", "[%s] Poll %d", name_.c_str(), attempt);
  if (d5_handle_ > 0) {
    post_bond_running_ = false;
    start_subscribe_();
    return;
  }
  if (attempt < kPostBondMaxPolls) {
    char status[24];
    std::snprintf(status, sizeof(status), "Poll %d: waiting...", attempt);
    publish_status_(status);
    const char *next_name =
        (attempt == 1) ? kTimeoutPostBondPoll2 : kTimeoutPostBondPoll3;
    scheduler_->set_timeout(
        next_name, kPostBondPollPeriodMs,
        [this, attempt]() { post_bond_poll_attempt_(attempt + 1); });
    return;
  }
  HUB_LOGW("ble", "[%s] No D5 after 20s. Reconnecting...", name_.c_str());
  publish_status_("No D5 found. Reconnecting...");
  phase_ = 1;
  post_bond_giveup_();
}

void SubzeroHub::post_bond_giveup_() {
  if (d5_handle_ != 0) {
    post_bond_running_ = false;
    return;
  }
  transport_->disconnect();
  scheduler_->set_timeout(kTimeoutPostBondGiveup, kPostBondGiveupDisconnectMs,
                          [this]() {
                            if (d5_handle_ != 0) {
                              post_bond_running_ = false;
                              return;
                            }
                            transport_->cache_clean();
                            transport_->cache_refresh();
                            // auto_connect handles the reconnect.
                            post_bond_running_ = false;
                          });
}

void SubzeroHub::update_handles_from_db_() {
  auto entries = transport_->read_gatt_db();
  for (const auto &e : entries) {
    if (e.type != GattDbEntry::kCharacteristic)
      continue;
    if (!e.uuid_is_128bit)
      continue;
    switch (e.uuid_first_byte) {
    case 0xD5:
      if (d5_handle_ == 0)
        d5_handle_ = e.handle;
      break;
    case 0xD6:
      if (d6_handle_ == 0)
        d6_handle_ = e.handle;
      break;
    case 0xD7:
      if (d7_handle_ == 0)
        d7_handle_ = e.handle;
      break;
    }
  }
}

// =============================================================================
// subscribe
// =============================================================================

void SubzeroHub::start_subscribe_() {
  subscribe_running_ = true;
  subscribe_register_and_cccd_();
}

void SubzeroHub::subscribe_register_and_cccd_() {
  if (d5_handle_ == 0) {
    HUB_LOGE("ble", "[%s] D5 handle not set!", name_.c_str());
    publish_status_("ERROR: D5 handle not found");
    subscribe_running_ = false;
    return;
  }
  // Fire YAML-side hook BEFORE we issue our own register_for_notify —
  // the YAML uses this to inject discovered handles into ESPHome's
  // ble_client notify sensors (so their lambdas keep firing on fast
  // reconnect with stale GATT cache).
  if (subscribe_cb_)
    subscribe_cb_();
  HUB_LOGI("ble", "[%s] Injected D5 handle %d", name_.c_str(), d5_handle_);
  if (d6_handle_ > 0) {
    HUB_LOGI("ble", "[%s] Injected D6 handle %d", name_.c_str(), d6_handle_);
  }

  transport_->register_for_notify(d5_handle_);
  HUB_LOGI("ble", "[%s] Subscribe D5 (h=%d)", name_.c_str(), d5_handle_);
  if (d6_handle_ > 0) {
    transport_->register_for_notify(d6_handle_);
    HUB_LOGI("ble", "[%s] Subscribe D6 (h=%d) for data + pushes", name_.c_str(),
             d6_handle_);
  }

  static constexpr std::uint8_t kCccdOn[2] = {0x02, 0x00};
  transport_->write(static_cast<std::uint16_t>(d5_handle_ + 2), kCccdOn,
                    sizeof(kCccdOn));
  HUB_LOGI("ble", "[%s] CCCD write D5 (h=%d)", name_.c_str(), d5_handle_ + 2);
  if (d6_handle_ > 0) {
    transport_->write(static_cast<std::uint16_t>(d6_handle_ + 2), kCccdOn,
                      sizeof(kCccdOn));
    HUB_LOGI("ble", "[%s] CCCD write D6 (h=%d)", name_.c_str(), d6_handle_ + 2);
  }
  json_buf_.clear();

  scheduler_->set_timeout(kTimeoutSubscribeUnlock, kSubscribeUnlockDelayMs,
                          [this]() { subscribe_unlock_(); });
}

void SubzeroHub::subscribe_unlock_() {
  if (stored_pin_.empty() || !pin_confirmed_) {
    HUB_LOGI("ble", "[%s] No saved PIN. Waiting for user to pair.",
             name_.c_str());
    publish_status_("Connected! Press 'Start Pairing', then enter PIN.");
    subscribe_running_ = false;
    return;
  }
  write_unlock_channel_(d5_handle_);
  HUB_LOGI("ble", "[%s] Auto-unlock D5 (PIN=%s)", name_.c_str(),
           stored_pin_.c_str());
  if (d6_handle_ > 0) {
    write_unlock_channel_(d6_handle_);
    HUB_LOGI("ble", "[%s] Auto-unlock D6", name_.c_str());
  }
  publish_status_("Auto-unlocking...");

  scheduler_->set_timeout(kTimeoutSubscribeGet, kSubscribeInitialGetDelayMs,
                          [this]() { subscribe_initial_get_(); });
}

void SubzeroHub::subscribe_initial_get_() {
  subscribe_running_ = false;
  if (!pin_confirmed_)
    return;
  if (d6_handle_ == 0)
    return;
  poll_ok_ = false;
  write_poll_command_(d6_handle_);
  publish_status_("Connected and polling.");
}

// =============================================================================
// fast_reconnect
// =============================================================================

void SubzeroHub::start_fast_reconnect_() {
  fast_reconnect_running_ = true;
  scheduler_->set_timeout(kTimeoutFastReconnect, poll_offset_ms_, [this]() {
    transport_->request_encryption();
    HUB_LOGI("ble", "[%s] Fast reconnect: encryption requested", name_.c_str());
    scheduler_->set_timeout(kTimeoutFastReconnect, kFastReconnectEncryptDelayMs,
                            [this]() { fast_reconnect_subscribe_(); });
  });
}

void SubzeroHub::fast_reconnect_subscribe_() {
  fast_reconnect_running_ = false;
  start_subscribe_();
}

// =============================================================================
// Button actions
// =============================================================================

void SubzeroHub::press_connect() {
  phase_ = 0;
  clear_handles_();
  json_buf_.clear();
  if (scheduler_ != nullptr) {
    scheduler_->cancel_timeout(kTimeoutPostBondInitial);
    scheduler_->cancel_timeout(kTimeoutPostBondPostEnc);
    scheduler_->cancel_timeout(kTimeoutPostBondSearch);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll1);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll2);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll3);
    scheduler_->cancel_timeout(kTimeoutPostBondGiveup);
    scheduler_->cancel_timeout(kTimeoutSubscribeUnlock);
    scheduler_->cancel_timeout(kTimeoutSubscribeGet);
    scheduler_->cancel_timeout(kTimeoutFastReconnect);
    scheduler_->cancel_timeout(kTimeoutSubmitPinPoll);
    scheduler_->cancel_timeout(kTimeoutVerbFallbackRetry);
  }
  post_bond_running_ = false;
  subscribe_running_ = false;
  fast_reconnect_running_ = false;
}

void SubzeroHub::press_start_pairing() {
  if (transport_ == nullptr)
    return;
  if (d5_handle_ == 0) {
    publish_status_("ERROR: Not connected. Press Connect first.");
    return;
  }
  std::string cmd = esphome::subzero_protocol::build_display_pin();
  BleResult err = transport_->write(
      d5_handle_, reinterpret_cast<const std::uint8_t *>(cmd.data()),
      cmd.size());
  HUB_LOGI("ble", "[%s] display_pin -> D5 err=%d", name_.c_str(),
           static_cast<int>(err));
  publish_status_("Check appliance display for PIN.");
}

void SubzeroHub::press_submit_pin() {
  if (transport_ == nullptr || scheduler_ == nullptr)
    return;
  if (d5_handle_ == 0) {
    publish_status_("ERROR: Not connected.");
    return;
  }
  if (stored_pin_.empty()) {
    publish_status_("ERROR: Enter PIN first.");
    return;
  }
  write_unlock_channel_(d5_handle_);
  HUB_LOGI("ble", "[%s] unlock_channel (PIN=%s)", name_.c_str(),
           stored_pin_.c_str());
  publish_status_("Unlock sent...");
  scheduler_->set_timeout(kTimeoutSubmitPinPoll, kSubmitPinPollDelayMs,
                          [this]() {
                            if (d5_handle_ == 0)
                              return;
                            write_poll_command_(d5_handle_);
                          });
}

void SubzeroHub::press_poll() {
  if (transport_ == nullptr)
    return;
  if (d6_handle_ == 0) {
    publish_status_("ERROR: Not connected");
    return;
  }
  if (!stored_pin_.empty()) {
    write_unlock_channel_(d6_handle_);
  }
  write_poll_command_(d6_handle_);
  HUB_LOGI("ble", "[%s] POLL D6: re-unlock + %s", name_.c_str(),
           poll_verb_ == esphome::subzero_protocol::PollVerb::kGet
               ? "get"
               : "get_async");
  publish_status_("Polling...");
}

void SubzeroHub::press_log_debug_info() {
  if (transport_ == nullptr)
    return;
  if (d6_handle_ == 0) {
    publish_status_("ERROR: Not connected");
    return;
  }
  debug_mode_ = true;
  HUB_LOGI("ble", "[%s] Log Debug Info: forcing reconnect for fresh unlock",
           name_.c_str());
  publish_status_("Debug mode ON - reconnecting for fresh poll...");
  transport_->disconnect();
}

void SubzeroHub::press_reset_pairing() {
  if (transport_ == nullptr)
    return;
  if (scheduler_ != nullptr) {
    scheduler_->cancel_timeout(kTimeoutPostBondInitial);
    scheduler_->cancel_timeout(kTimeoutPostBondPostEnc);
    scheduler_->cancel_timeout(kTimeoutPostBondSearch);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll1);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll2);
    scheduler_->cancel_timeout(kTimeoutPostBondPoll3);
    scheduler_->cancel_timeout(kTimeoutPostBondGiveup);
    scheduler_->cancel_timeout(kTimeoutSubscribeUnlock);
    scheduler_->cancel_timeout(kTimeoutSubscribeGet);
    scheduler_->cancel_timeout(kTimeoutFastReconnect);
    scheduler_->cancel_timeout(kTimeoutSubmitPinPoll);
    scheduler_->cancel_timeout(kTimeoutVerbFallbackRetry);
  }
  pin_confirmed_ = false;
  clear_handles_();
  phase_ = 0;
  fast_retries_ = 0;
  poll_miss_ = 0;
  json_buf_.clear();
  poll_verb_ = esphome::subzero_protocol::PollVerb::kGetAsync;
  post_bond_running_ = false;
  subscribe_running_ = false;
  fast_reconnect_running_ = false;
  transport_->cache_clean();
  transport_->remove_bond();
  HUB_LOGW("ble", "[%s] Bond removed, GATT cache cleared, all state reset",
           name_.c_str());
  publish_status_(
      "Pairing fully reset. Power-cycle appliance, then press Connect.");
}

// =============================================================================
// Setters & helpers
// =============================================================================

void SubzeroHub::set_stored_pin(const std::string &pin) {
  stored_pin_ = pin;
  if (!pin.empty()) {
    HUB_LOGI("szg", "[%s] PIN updated: %s", name_.c_str(), pin.c_str());
  }
}

void SubzeroHub::publish_status_(const std::string &text) {
  if (status_cb_)
    status_cb_(text);
}

void SubzeroHub::clear_handles_() {
  d5_handle_ = 0;
  d6_handle_ = 0;
  d7_handle_ = 0;
}

void SubzeroHub::clear_session_state_() {
  json_buf_.clear();
  poll_ok_ = false;
  poll_miss_ = 0;
}

void SubzeroHub::write_unlock_channel_(std::uint16_t handle) {
  if (transport_ == nullptr)
    return;
  if (stored_pin_.empty() || handle == 0)
    return;
  std::string cmd =
      esphome::subzero_protocol::build_unlock_channel(stored_pin_);
  transport_->write(handle, reinterpret_cast<const std::uint8_t *>(cmd.data()),
                    cmd.size());
}

void SubzeroHub::write_poll_command_(std::uint16_t handle) {
  if (transport_ == nullptr)
    return;
  if (handle == 0)
    return;
  std::string cmd = esphome::subzero_protocol::build_poll_command(poll_verb_);
  transport_->write(handle, reinterpret_cast<const std::uint8_t *>(cmd.data()),
                    cmd.size());
}

void SubzeroHub::write_set_property_(const std::string &key,
                                     const std::string &json_value) {
  if (transport_ == nullptr)
    return;
  // `set` requires the control channel to be unlocked. d5_handle_ is
  // populated after subscribe completes; pin_confirmed_ flips true when
  // the unlock_channel response lands. Drop the write if either guard
  // fails — the alternative is queueing, which adds complexity for no
  // visible UX win (HA will retry on the next user action).
  if (d5_handle_ == 0 || !pin_confirmed_)
    return;
  std::string cmd = esphome::subzero_protocol::build_set(key, json_value);
  transport_->write(d5_handle_,
                    reinterpret_cast<const std::uint8_t *>(cmd.data()),
                    cmd.size());
}

void SubzeroHub::write_set_bool(const std::string &key, bool value) {
  write_set_property_(key, value ? "true" : "false");
}

void SubzeroHub::write_set_int(const std::string &key, int value) {
  write_set_property_(key, std::to_string(value));
}

void SubzeroHub::write_set_string(const std::string &key,
                                  const std::string &value) {
  write_set_property_(
      key, "\"" + esphome::subzero_protocol::detail::escape_json_string(value) +
               "\"");
}

} // namespace subzero_appliance
} // namespace esphome
