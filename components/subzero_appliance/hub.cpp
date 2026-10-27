#include "hub.h"

#include "../subzero_protocol/commands.h"
#include "../subzero_protocol/log_sanitize.h"
#include "../subzero_protocol/protocol.h"

#include <algorithm>
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
constexpr const char *kTimeoutSessionRefresh = "session_refresh";
constexpr const char *kTimeoutSessionRefreshQuiet = "session_refresh_quiet";
constexpr const char *kTimeoutSnapshotWatch = "snapshot_watch";
} // namespace

// =============================================================================
// Lifecycle handlers — translated from the on_connect / on_disconnect /
// on_passkey_request triggers in subzero_base.yaml.
// =============================================================================

void SubzeroHub::handle_connected() {
  if (transport_ == nullptr || scheduler_ == nullptr)
    return;
  if (!session_active_)
    start_session_();
  transport_->request_mtu();

  // Fast path: handles cached from a previous (bonded) connection.
  if (d5_handle_ > 0 && phase_ >= 1) {
    HUB_LOGI("ble", "[%s] Reconnected (fast path, d5=%d, retries=%d)",
             name_.c_str(), d5_handle_, fast_retries_);
    publish_progress_("Reconnected, encrypting...");
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

void SubzeroHub::handle_disconnected(int reason) {
  if (transport_ == nullptr || scheduler_ == nullptr)
    return;
  end_session_(reason);
  scheduler_->cancel_timeout(kTimeoutSnapshotWatch);
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
  scheduler_->cancel_timeout(kTimeoutSessionRefresh);
  post_bond_running_ = false;
  subscribe_running_ = false;
  fast_reconnect_running_ = false;
  pairing_failed_ = false;

  // Anything other than the refresh's own disconnect is real news: stop
  // muting so the drop (and whatever follows) reaches the user.
  if (!intentional_disconnect_)
    session_refresh_quiet_ = false;

  if (d5_handle_ > 0 && phase_ >= 1) {
    if (intentional_disconnect_) {
      // Scheduled session refresh — we initiated this. Don't let it
      // contribute to stale-bond detection; the bond is fine, we're
      // just rotating the unlock session.
      intentional_disconnect_ = false;
      HUB_LOGI("ble",
               "[%s] Disconnected (intentional refresh, handles cached, d5=%d)",
               name_.c_str(), d5_handle_);
    } else {
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
    }
  } else {
    phase_ = 0;
    clear_handles_();
    HUB_LOGI("ble", "[%s] Disconnected", name_.c_str());
  }
  publish_progress_("Disconnected");
}

// esp_ble_auth_fail_rsn_t: 0x4E..0x5B are 0x4D + the SMP reason,
// 0x5C..0x66 are Bluedroid-internal. No IDF headers (host tests).
static const char *auth_fail_reason_str(int reason) {
  switch (reason) {
  case 0x4E:
    return "SMP 0x01 passkey entry failed";
  case 0x4F:
    return "SMP 0x02 OOB not available";
  case 0x50:
    return "SMP 0x03 authentication requirements";
  case 0x51:
    return "SMP 0x04 confirm value failed";
  case 0x52:
    return "SMP 0x05 pairing not supported";
  case 0x53:
    return "SMP 0x06 encryption key size";
  case 0x54:
    return "SMP 0x07 command not supported";
  case 0x55:
    return "SMP 0x08 unspecified reason";
  case 0x56:
    return "SMP 0x09 repeated attempts";
  case 0x57:
    return "SMP 0x0A invalid parameters";
  case 0x58:
    return "SMP 0x0B DHKey check failed";
  case 0x59:
    return "SMP 0x0C numeric comparison failed";
  case 0x5A:
    return "SMP 0x0D BR/EDR pairing in progress";
  case 0x5B:
    return "SMP 0x0E cross-transport key not allowed";
  case 0x5C:
    return "internal error";
  case 0x5D:
    return "unknown IO capability";
  case 0x5E:
    return "SMP init failed";
  case 0x5F:
    return "confirm failed";
  case 0x60:
    return "SMP busy";
  case 0x61:
    return "encryption failed";
  case 0x62:
    return "SMP started";
  case 0x63:
    return "SMP response timeout";
  case 0x64:
    return "DIV not available";
  case 0x65:
    return "unspecified failure";
  case 0x66:
    return "connection timeout";
  default:
    return "unknown reason";
  }
}

void SubzeroHub::handle_auth_complete(bool success, int fail_reason,
                                      int auth_mode) {
  if (success) {
    pairing_failed_ = false;
    HUB_LOGI("ble", "[%s] Link encrypted (auth_mode=0x%02X: bond=%d mitm=%d)",
             name_.c_str(), auth_mode, (auth_mode & 0x01) ? 1 : 0,
             (auth_mode & 0x04) ? 1 : 0);
    return;
  }
  pairing_failed_ = true;
  const char *why = auth_fail_reason_str(fail_reason);
  HUB_LOGE("ble",
           "[%s] Pairing failed (reason=%d: %s). The link is not encrypted, "
           "so the appliance will refuse every command.",
           name_.c_str(), fail_reason, why);
  publish_status_(std::string("Pairing failed (") + why + ")");
  // SMP 0x08 at the confirm check is how CONAPPWM fw 5.13 rejects full key
  // distribution; try the other key set on the next connection.
  if (fail_reason == 0x55) {
    ltk_only_ = !ltk_only_;
    HUB_LOGW("ble", "[%s] Next pairing will distribute %s", name_.c_str(),
             ltk_only_ ? "the LTK only" : "LTK + IRK + CSRK");
  }
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
                                  std::size_t len) {
  // D5 indications are control-channel responses (display_pin /
  // unlock_channel / set / scan acks) and mirror copies of D6 pushes.
  // Nothing to parse - all useful state lands on D6.
  if (!session_d5_seen_) {
    session_d5_seen_ = true;
    HUB_LOGI("conn", "[%s] First D5 data on this connection (%d bytes)",
             name_.c_str(), static_cast<int>(len));
  }
}

void SubzeroHub::handle_d6_notify(const std::uint8_t *data, std::size_t len) {
  if (!session_d6_seen_) {
    session_d6_seen_ = true;
    HUB_LOGI("conn", "[%s] First D6 data on this connection (%d bytes)",
             name_.c_str(), static_cast<int>(len));
  }
  if (json_buf_.feed(data, len)) {
    process_message_complete_();
  }
}

void SubzeroHub::process_message_complete_() {
  auto msg = json_buf_.take_message();
  if (!msg)
    return;

  // ends_nl: whether the appliance's '\n' terminator arrived with the
  // closing brace (the official app frames messages on it).
  HUB_LOGI("szg", "[%s] Parsing JSON (%d bytes, ends_nl=%d)", name_.c_str(),
           static_cast<int>(msg->size()), msg->back() == '\n' ? 1 : 0);

  if (debug_mode_)
    log_chunked_debug_(*msg);

  parsing_snapshot_ = esphome::subzero_protocol::is_full_state_dump(*msg);
  bool ok = parse_and_dispatch_(*msg);
  const bool was_snapshot = parsing_snapshot_;
  parsing_snapshot_ = false;
  if (ok) {
    if (auto seq = esphome::subzero_protocol::extract_push_seq(*msg)) {
      session_pushes_ += 1;
      track_seq_(*seq);
    }
    if (was_snapshot)
      on_full_snapshot_();
  }
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
  // Subclasses call this for every parsed message carrying a `pin` field,
  // which on a healthy link is every poll response — not just the moment
  // the channel actually unlocks. Only the false->true edge (or a PIN
  // that genuinely changed) is news; announcing the steady state on every
  // poll spammed the Home Assistant logbook with a duplicate row roughly
  // every 25 seconds.
  const bool newly_confirmed = !pin_confirmed_ || pin != stored_pin_;
  stored_pin_ = pin;
  pin_confirmed_ = true;
  if (!newly_confirmed)
    return;
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
  }
  poll_ok_ = false;
  json_buf_.clear();

  if (!stored_pin_.empty()) {
    write_unlock_channel_(d6_handle_);
  }
  BleResult err = write_command_(
      d6_handle_, esphome::subzero_protocol::build_poll_command(poll_verb_));
  if (err != BleResult::kOk) {
    HUB_LOGW("szg", "[%s] D6 write failed (err=%d), forcing reconnect",
             name_.c_str(), static_cast<int>(err));
    publish_status_("Write failed, reconnecting...");
    transport_->disconnect();
    return;
  }
  HUB_LOGI("szg",
           "[%s] Periodic poll D6 (verb=%s, miss=%d, retries=%d, "
           "data_age=%ds)",
           name_.c_str(),
           poll_verb_ == esphome::subzero_protocol::PollVerb::kGet
               ? "get"
               : "get_async",
           poll_miss_, fast_retries_, snapshot_age_s());
}

// =============================================================================
// post_bond — 5-stage discovery ladder
// =============================================================================

// The connect chains never wait for poll_offset: Wine/Back fridges whose
// first command came 15-30 s after connect never answered (2026-10-26),
// while the official app writes within about a second.
void SubzeroHub::start_post_bond_() {
  post_bond_running_ = true;
  scheduler_->set_timeout(kTimeoutPostBondInitial, kPostBondInitialDelayMs,
                          [this]() { post_bond_initial_(); });
}

void SubzeroHub::post_bond_initial_() {
  update_handles_from_db_();

  HUB_LOGI("ble", "[%s] D5 %s. Requesting encryption...", name_.c_str(),
           d5_handle_ > 0 ? "found immediately" : "not yet visible");
  transport_->request_encryption(ltk_only_);
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
    // 32 bytes comfortably covers the worst-case %d width (a full signed
    // int, up to 11 digits+sign) even though `attempt` is always 1-2 in
    // practice (bounded by kPostBondMaxPolls); silences a false-positive
    // -Wformat-truncation the compiler can't resolve across the
    // recursive call.
    char status[32];
    std::snprintf(status, sizeof(status), "Poll %d: waiting...", attempt);
    publish_progress_(status);
    const char *next_name =
        (attempt == 1) ? kTimeoutPostBondPoll2 : kTimeoutPostBondPoll3;
    scheduler_->set_timeout(
        next_name, kPostBondPollPeriodMs,
        [this, attempt]() { post_bond_poll_attempt_(attempt + 1); });
    return;
  }
  HUB_LOGW("ble", "[%s] No D5 after 20s. Reconnecting...", name_.c_str());
  publish_progress_("No D5 found. Reconnecting...");
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
  // A characteristic's CCCD is the 0x2902 descriptor between its value
  // handle and the next characteristic's value handle.
  auto find_cccd = [&entries](std::uint16_t value_handle) -> std::uint16_t {
    std::uint32_t next = 0x10000;
    for (const auto &e : entries) {
      if (e.type == GattDbEntry::kCharacteristic && e.handle > value_handle &&
          e.handle < next)
        next = e.handle;
    }
    std::uint16_t best = 0;
    for (const auto &e : entries) {
      if (e.type == GattDbEntry::kDescriptor && e.uuid16 == 0x2902 &&
          e.handle > value_handle && e.handle < next &&
          (best == 0 || e.handle < best))
        best = e.handle;
    }
    return best;
  };
  for (const auto &e : entries) {
    if (e.type != GattDbEntry::kCharacteristic)
      continue;
    if (!e.uuid_is_128bit)
      continue;
    switch (e.uuid_first_byte) {
    case 0xD5:
      if (d5_handle_ == 0) {
        d5_handle_ = e.handle;
        d5_props_ = e.properties;
      }
      break;
    case 0xD6:
      if (d6_handle_ == 0) {
        d6_handle_ = e.handle;
        d6_props_ = e.properties;
      }
      break;
    case 0xD7:
      if (d7_handle_ == 0)
        d7_handle_ = e.handle;
      break;
    }
  }
  if (d5_handle_ > 0 && d5_cccd_ == 0)
    d5_cccd_ = find_cccd(d5_handle_);
  if (d6_handle_ > 0 && d6_cccd_ == 0)
    d6_cccd_ = find_cccd(d6_handle_);
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

  // ESPHome only writes the CCCD itself while it still holds the service
  // list, which it frees right after discovery, so this write is the one
  // that enables notifications. Without a CCCD in the db, fall back to the
  // fw 8.5 layout (value handle + 2). Like the official app, use
  // notifications where the characteristic has them (fw 2.27) and
  // indications otherwise (fw 8.5).
  static constexpr std::uint8_t kCccdNotify[2] = {0x01, 0x00};
  static constexpr std::uint8_t kCccdIndicate[2] = {0x02, 0x00};
  auto write_cccd = [this](const char *ch, std::uint16_t value_handle,
                           std::uint16_t cccd, std::uint8_t props) {
    const bool guessed = cccd == 0;
    if (guessed)
      cccd = static_cast<std::uint16_t>(value_handle + 2);
    const bool notify = (props & 0x10) != 0;
    transport_->write(cccd, notify ? kCccdNotify : kCccdIndicate, 2);
    HUB_LOGI("ble", "[%s] CCCD write %s (h=%d, %s%s)", name_.c_str(), ch, cccd,
             notify ? "notify" : "indicate", guessed ? ", guessed" : "");
  };
  write_cccd("D5", d5_handle_, d5_cccd_, d5_props_);
  if (d6_handle_ > 0)
    write_cccd("D6", d6_handle_, d6_cccd_, d6_props_);
  // The app writes an empty value to D5 on every connect before any
  // command; do the same in case it resets the appliance's command parser.
  static constexpr std::uint8_t kEmpty[1] = {0};
  transport_->write(d5_handle_, kEmpty, 0);
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
  publish_progress_("Auto-unlocking...");

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
  if (pairing_failed_) {
    HUB_LOGW("ble", "[%s] Polling without encryption - expect no data",
             name_.c_str());
  } else {
    publish_status_("Connected and polling.");
  }

  if (!session_snapshot_) {
    scheduler_->set_timeout(kTimeoutSnapshotWatch, kSnapshotWarnMs,
                            [this]() { snapshot_watch_expired_(); });
  }

  // Arm scheduled session refresh: ~18 min from now, proactively
  // disconnect-and-reconnect to refresh the appliance's BLE unlock
  // session before the firmware's own ~20-25 min expiry kicks us off
  // unpredictably. handle_disconnected cancels the timer if the link
  // drops for any other reason, so we never carry it across sessions.
  if (session_refresh_interval_ms_ > 0) {
    scheduler_->set_timeout(kTimeoutSessionRefresh,
                            session_refresh_interval_ms_,
                            [this]() { disconnect_for_session_refresh_(); });
  }
}

// =============================================================================
// Scheduled session refresh
// =============================================================================

void SubzeroHub::disconnect_for_session_refresh_() {
  if (transport_ == nullptr)
    return;
  if (!transport_->connected())
    return;
  HUB_LOGI(
      "ble",
      "[%s] Scheduled session refresh (%u min), reconnecting via fast path",
      name_.c_str(),
      static_cast<unsigned>(session_refresh_interval_ms_ / 60000));
  json_buf_.clear();
  // Mute progress chatter for the duration of the bounce. The user asked
  // for none of this; they just want Status to keep reading "Connected
  // and polling." across a refresh that succeeds.
  session_refresh_quiet_ = true;
  scheduler_->set_timeout(kTimeoutSessionRefreshQuiet,
                          kSessionRefreshQuietMaxMs,
                          [this]() { session_refresh_quiet_expired_(); });
  publish_progress_("Refreshing session...");
  // Tag the next disconnect callback so handle_disconnected skips
  // fast_retries_ accounting — see intentional_disconnect_ comment.
  intentional_disconnect_ = true;
  transport_->disconnect();
}

void SubzeroHub::session_refresh_quiet_expired_() {
  if (!session_refresh_quiet_)
    return; // refresh already completed or errored out; nothing to say
  session_refresh_quiet_ = false;
  HUB_LOGW("ble", "[%s] Session refresh still incomplete after %u s",
           name_.c_str(),
           static_cast<unsigned>(kSessionRefreshQuietMaxMs / 1000));
  publish_status_("Reconnecting...");
}

// =============================================================================
// Connection diagnostics - log-and-count only, no effect on the link.
// =============================================================================

const char *SubzeroHub::disconnect_reason_str(int reason) {
  switch (reason) {
  case kReasonOpenFailed:
    return "GATT open failed";
  case kReasonSearchFailed:
    return "GATT search failed";
  case 0x01:
    return "L2CAP failure";
  case 0x08:
    return "supervision timeout";
  case 0x13:
    return "appliance closed the link";
  case 0x16:
    return "ESP32 closed the link";
  case 0x22:
    return "LMP response timeout";
  case 0x3E:
    return "failed to establish";
  case 0x100:
    return "connection cancelled";
  default:
    return "unknown";
  }
}

int SubzeroHub::snapshot_age_s() const {
  if (!have_snapshot_ || scheduler_ == nullptr)
    return -1;
  return static_cast<int>((scheduler_->now_ms() - last_snapshot_ms_) / 1000);
}

void SubzeroHub::start_session_() {
  session_active_ = true;
  session_snapshot_ = false;
  session_no_snapshot_counted_ = false;
  session_d5_seen_ = false;
  session_d6_seen_ = false;
  session_pushes_ = 0;
  seq_seen_in_session_ = false;
  session_start_ms_ = scheduler_->now_ms();
  sessions_ += 1;
  if (have_drop_) {
    HUB_LOGI("conn", "[%s] Connection %d up, %u ms after the last drop",
             name_.c_str(), sessions_,
             static_cast<unsigned>(session_start_ms_ - last_drop_ms_));
  } else {
    HUB_LOGI("conn", "[%s] Connection %d up", name_.c_str(), sessions_);
  }
  notify_metrics_();
}

void SubzeroHub::end_session_(int reason) {
  if (intentional_disconnect_) {
    last_disconnect_reason_ = "session refresh";
  } else if (reason < 0) {
    last_disconnect_reason_ = disconnect_reason_str(reason);
  } else {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s (0x%X)", disconnect_reason_str(reason),
                  static_cast<unsigned>(reason));
    last_disconnect_reason_ = buf;
  }
  if (!session_active_) {
    HUB_LOGI("conn", "[%s] Link failed before connect: %s", name_.c_str(),
             last_disconnect_reason_.c_str());
    notify_metrics_();
    return;
  }
  session_active_ = false;
  const std::uint32_t now = scheduler_->now_ms();
  have_drop_ = true;
  last_drop_ms_ = now;
  if (!session_snapshot_ && !session_no_snapshot_counted_) {
    sessions_without_snapshot_ += 1;
    session_no_snapshot_counted_ = true;
  }
  std::string open_doors;
  for (const auto &d : doors_) {
    if (!d.second)
      continue;
    if (!open_doors.empty())
      open_doors += ",";
    open_doors += d.first;
  }
  // Pushes that arrive before the next snapshot overwrite this copy key
  // by key; whatever is left gets checked against that snapshot.
  doors_at_drop_ = doors_;
  HUB_LOGI("conn",
           "[%s] Connection %d down after %u ms: %s, snapshot=%s, pushes=%d, "
           "doors open=%s",
           name_.c_str(), sessions_,
           static_cast<unsigned>(now - session_start_ms_),
           last_disconnect_reason_.c_str(), session_snapshot_ ? "yes" : "NO",
           session_pushes_, open_doors.empty() ? "none" : open_doors.c_str());
  notify_metrics_();
}

void SubzeroHub::on_full_snapshot_() {
  const std::uint32_t now = scheduler_->now_ms();
  have_snapshot_ = true;
  last_snapshot_ms_ = now;
  if (session_active_ && !session_snapshot_) {
    session_snapshot_ = true;
    scheduler_->cancel_timeout(kTimeoutSnapshotWatch);
    HUB_LOGI("conn", "[%s] First snapshot %u ms after connection %d",
             name_.c_str(), static_cast<unsigned>(now - session_start_ms_),
             sessions_);
    for (const auto &d : doors_at_drop_) {
      auto it = doors_.find(d.first);
      if (it == doors_.end() || it->second == d.second)
        continue;
      gap_door_changes_ += 1;
      HUB_LOGW("conn",
               "[%s] %s changed while disconnected: %s at the drop, %s in "
               "the snapshot (edge lost, snapshot corrected it)",
               name_.c_str(), d.first.c_str(), d.second ? "open" : "closed",
               it->second ? "open" : "closed");
    }
    doors_at_drop_.clear();
  }
  notify_metrics_();
}

void SubzeroHub::track_seq_(int seq) {
  if (last_seq_ >= 0) {
    if (seq > last_seq_ + 1) {
      const int missed = seq - last_seq_ - 1;
      missed_pushes_ += missed;
      if (seq_seen_in_session_) {
        HUB_LOGW("conn", "[%s] Push seq gap: %d -> %d (%d missed)",
                 name_.c_str(), last_seq_, seq, missed);
      } else {
        HUB_LOGI("conn", "[%s] Push seq across reconnect: %d -> %d (%d missed)",
                 name_.c_str(), last_seq_, seq, missed);
      }
      notify_metrics_();
    } else if (seq <= last_seq_) {
      HUB_LOGI("conn", "[%s] Push seq went back: %d -> %d%s", name_.c_str(),
               last_seq_, seq,
               seq_seen_in_session_ ? "" : " (first this connection)");
    }
  }
  last_seq_ = seq;
  seq_seen_in_session_ = true;
}

void SubzeroHub::snapshot_watch_expired_() {
  if (!session_active_ || session_snapshot_ || session_no_snapshot_counted_)
    return;
  session_no_snapshot_counted_ = true;
  sessions_without_snapshot_ += 1;
  HUB_LOGW("conn",
           "[%s] No snapshot %u s after get_async on connection %d "
           "(pushes=%d), sending unlock + %s again",
           name_.c_str(), static_cast<unsigned>(kSnapshotWarnMs / 1000),
           sessions_, session_pushes_,
           poll_verb_ == esphome::subzero_protocol::PollVerb::kGet
               ? "get"
               : "get_async");
  notify_metrics_();
  // One early retry instead of waiting for the next 60 s poll, in case a
  // write was dropped while other appliances on this ESP32 were busy.
  if (transport_ != nullptr && transport_->connected() && d6_handle_ > 0) {
    write_unlock_channel_(d5_handle_);
    write_unlock_channel_(d6_handle_);
    write_poll_command_(d6_handle_);
  }
}

void SubzeroHub::note_door_(const char *key, bool open) {
  doors_[key] = open;
  if (!parsing_snapshot_)
    doors_at_drop_.erase(key);
}

void SubzeroHub::notify_metrics_() {
  if (metrics_cb_)
    metrics_cb_();
}

// =============================================================================
// fast_reconnect
// =============================================================================

void SubzeroHub::start_fast_reconnect_() {
  fast_reconnect_running_ = true;
  scheduler_->set_timeout(kTimeoutFastReconnect, 0, [this]() {
    transport_->request_encryption(ltk_only_);
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
    scheduler_->cancel_timeout(kTimeoutSessionRefresh);
    scheduler_->cancel_timeout(kTimeoutSnapshotWatch);
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
  BleResult err = write_command_(
      d5_handle_, esphome::subzero_protocol::build_display_pin());
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
    scheduler_->cancel_timeout(kTimeoutSessionRefresh);
    scheduler_->cancel_timeout(kTimeoutSnapshotWatch);
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
  // A real state won the race — stop muting progress chatter.
  session_refresh_quiet_ = false;
  if (text == last_status_)
    return;
  last_status_ = text;
  if (status_cb_)
    status_cb_(text);
}

void SubzeroHub::publish_progress_(const std::string &text) {
  if (session_refresh_quiet_ || pairing_failed_)
    return;
  publish_status_(text);
}

void SubzeroHub::clear_handles_() {
  d5_handle_ = 0;
  d6_handle_ = 0;
  d7_handle_ = 0;
  d5_cccd_ = 0;
  d6_cccd_ = 0;
  d5_props_ = 0;
  d6_props_ = 0;
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
  write_command_(handle,
                 esphome::subzero_protocol::build_unlock_channel(stored_pin_));
}

void SubzeroHub::write_poll_command_(std::uint16_t handle) {
  if (transport_ == nullptr)
    return;
  if (handle == 0)
    return;
  write_command_(handle,
                 esphome::subzero_protocol::build_poll_command(poll_verb_));
}

// The official app writes every command in chunks of at most 20 bytes,
// even with a 247-byte MTU (HCI captures 2026-04-25). Some appliances
// (IW30R wine, Back fridge) ack a longer single write but never act on it.
BleResult SubzeroHub::write_command_(std::uint16_t handle,
                                     const std::string &cmd) {
  BleResult result = BleResult::kOk;
  for (std::size_t off = 0; off < cmd.size(); off += kCommandChunkBytes) {
    const std::size_t n = std::min(kCommandChunkBytes, cmd.size() - off);
    BleResult r = transport_->write(
        handle, reinterpret_cast<const std::uint8_t *>(cmd.data() + off), n);
    if (r != BleResult::kOk && result == BleResult::kOk)
      result = r;
  }
  return result;
}

void SubzeroHub::write_set_property_(const std::string &key,
                                     const std::string &json_value) {
  if (transport_ == nullptr)
    return;
  // `set` requires the control channel to be unlocked. d5_handle_ is
  // populated after subscribe completes; pin_confirmed_ flips true when
  // the unlock_channel response lands. Drop the write if either guard
  // fails — the alternative is queueing, which adds complexity for no
  // visible UX win (HA will retry on the next user action). Still log
  // it: a silently-dropped write previously looked identical to "nothing
  // happened" with no way to tell the two apart from the logs.
  if (d5_handle_ == 0 || !pin_confirmed_) {
    // Deliberately omit json_value here — this path also carries
    // remote_svc_reg_token (the "Clear Cloud Token" button), so logging
    // the attempted value verbatim risks putting a cloud token in the
    // logs. The key plus readiness flags are enough to diagnose a drop.
    HUB_LOGW("szg",
             "[%s] Dropping write to %s: control channel not ready "
             "(d5_handle=%d, pin_confirmed=%d)",
             name_.c_str(), key.c_str(), d5_handle_, pin_confirmed_);
    return;
  }
  write_command_(d5_handle_,
                 esphome::subzero_protocol::build_set(key, json_value));
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
