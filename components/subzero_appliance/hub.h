#pragma once

// SubzeroHub — the connection state machine, lifted out of YAML scripts
// and into a testable C++ class. One instance per appliance.
//
// The hub doesn't inherit from ESPHome's component / BLEClientNode
// classes — staying as a plain class keeps the host gtest build clean.
// All side effects go through:
//
//   * BleTransport (ble_transport.h) — every IDF call: write, register
//     for notify, request encryption, GATT db dump, cache management,
//     bond removal. EspIdfTransport adapts to the real Bluedroid stack
//     in production; MockBleTransport records calls for tests.
//   * Scheduler (scheduler.h) — every multi-stage delay chain. ESPHome's
//     Scheduler in production; FakeScheduler in tests.
//   * Status / PIN callbacks — for the `${prefix}_debug` text_sensor
//     and the `${prefix}_pin_input` text input. The hub doesn't depend
//     on ESPHome sensor types directly.
//
// Subclasses (FridgeHub / DishwasherHub / RangeHub) override
// parse_and_dispatch_() to call the right `parse_X(json)` and
// `dispatch_X(state, bus)` from subzero_protocol.

#include "../subzero_protocol/buffer.h"
#include "../subzero_protocol/commands.h"
#include "ble_transport.h"
#include "scheduler.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace subzero_appliance {

class SubzeroHub {
public:
  // Default-constructible so the hub can be declared as an ESPHome
  // global variable. Wire up via set_transport/set_scheduler/set_name
  // in on_boot before any handle_* call lands. Tests construct the hub
  // and immediately call those setters.
  SubzeroHub() = default;

  virtual ~SubzeroHub() = default;

  // Configuration setters — must be called once at startup before any
  // BLE event is dispatched to the hub. The hub holds non-owning
  // pointers to all three; lifetime is the caller's responsibility.
  void set_transport(BleTransport *t) { transport_ = t; }
  void set_scheduler(Scheduler *s) { scheduler_ = s; }
  void set_name(std::string n) { name_ = std::move(n); }

  // ---- YAML-driven entry points (called from triggers / actions) ----

  // Connection lifecycle (called from ble_client's on_connect /
  // on_disconnect / on_passkey_request triggers).
  void handle_connected();
  void handle_disconnected();
  // Returns the passkey to reply with, or 0 if no PIN is stored
  // (caller should still call passkey_reply with 0 or skip).
  std::uint32_t handle_passkey_request();

  // BLE indication arrivals (called from D5/D6 notify sensor lambdas).
  // D5 is the control channel — heartbeat only, resets zombie counter.
  // D6 is the data channel — accumulates fragments into json_buf_ and
  // triggers parse_and_dispatch_() once a complete message lands.
  void handle_d5_notify(const std::uint8_t *data, std::size_t len);
  void handle_d6_notify(const std::uint8_t *data, std::size_t len);

  // 60s interval tick (called from interval block).
  void do_periodic_poll();

  // ---- Button actions ----

  // "Connect" button — full reset, BLE client takes care of dialing.
  // The actual `ble_client.connect:` action stays in YAML; this method
  // resets the hub state so post_bond runs cleanly on the next connect.
  void press_connect();
  // "Start Pairing" — write display_pin to D5.
  void press_start_pairing();
  // "Submit PIN & Unlock" — write unlock_channel to D5, then 3s later
  // write get_async to D5. (The new D6 polling path is taken on the
  // next periodic_poll cycle.)
  void press_submit_pin();
  // "Poll" button — re-unlock D6 + write get_async to D6.
  void press_poll();
  // "Log Debug Info" — enable debug, force disconnect (auto_connect
  // brings it back with a fresh full poll).
  void press_log_debug_info();
  // "Reset Pairing" — clear bond + handles, full state wipe.
  // The actual ble_client.disconnect / remove_bond actions stay in
  // YAML; this method handles all the in-process cleanup.
  void press_reset_pairing();

  // ---- Property writes (HA -> appliance via `set` on D5) ----
  //
  // All three forward to write_set_property_(); the type-specific
  // overloads exist so callers don't have to JSON-format the value
  // themselves. No-ops when the channel isn't open (d5_handle_ == 0)
  // or PIN isn't confirmed — same guard as write_unlock_channel_.
  void write_set_bool(const std::string &key, bool value);
  void write_set_int(const std::string &key, int value);
  void write_set_string(const std::string &key, const std::string &value);

  // ---- Configuration setters (called once at boot) ----

  // The PIN in the HA text input. on_value writes through this.
  void set_stored_pin(const std::string &pin);
  void set_pin_confirmed(bool v) { pin_confirmed_ = v; }
  void set_debug_mode(bool enabled) { debug_mode_ = enabled; }
  // Per-appliance stagger to avoid concurrent bonding races on shared
  // ESP32. Translated from the `poll_offset` substitution.
  void set_poll_offset_ms(std::uint32_t ms) { poll_offset_ms_ = ms; }
  // Status text callback — connected to ${prefix}_debug.publish_state.
  void set_status_callback(std::function<void(const std::string &)> cb) {
    status_cb_ = std::move(cb);
  }
  // PIN-input text callback — connected to
  // ${prefix}_pin_input.publish_state. Fires when an `unlock_channel`
  // response confirms the PIN, so the HA text input reflects the
  // appliance-confirmed value.
  void set_pin_input_callback(std::function<void(const std::string &)> cb) {
    pin_input_cb_ = std::move(cb);
  }
  // Subscribe-stage hook — fires once at the top of the subscribe step
  // (after handles are known, before register_for_notify + CCCD writes).
  // Used by the YAML to inject discovered handles into ESPHome's
  // ble_client::BLEClientSensor objects that wrap D5/D6 notifications.
  // Without this, fast-reconnect (stale GATT cache) leaves the ESPHome
  // sensors with handle=0 and their lambdas stop firing — the manual
  // injection is the workaround the prior YAML's subscribe script had.
  void set_subscribe_callback(std::function<void()> cb) {
    subscribe_cb_ = std::move(cb);
  }

  // ---- State accessors ----

  bool pin_confirmed() const { return pin_confirmed_; }
  bool poll_ok() const { return poll_ok_; }
  int poll_miss() const { return poll_miss_; }
  int fast_retries() const { return fast_retries_; }
  std::uint16_t d5_handle() const { return d5_handle_; }
  std::uint16_t d6_handle() const { return d6_handle_; }
  std::uint16_t d7_handle() const { return d7_handle_; }
  int phase() const { return phase_; }
  bool debug_mode() const { return debug_mode_; }
  bool post_bond_running() const { return post_bond_running_; }
  bool subscribe_running() const { return subscribe_running_; }
  bool fast_reconnect_running() const { return fast_reconnect_running_; }
  const std::string &stored_pin() const { return stored_pin_; }
  esphome::subzero_protocol::PollVerb poll_verb() const { return poll_verb_; }
  void set_poll_verb(esphome::subzero_protocol::PollVerb v) { poll_verb_ = v; }

protected:
  // Subclass hook — called when a complete JSON message has been
  // assembled in json_buf_. Returns true if the parse succeeded
  // (regardless of whether PIN was confirmed); false on parse failure
  // or status!=0 (in which case the hub also publishes a status).
  //
  // The base class hub provides the surrounding logic (chunked debug
  // log, status-302 detection, set poll_ok / fast_retries on success).
  // The subclass just plugs in the type-specific parse+dispatch.
  virtual bool parse_and_dispatch_(const std::string &msg) = 0;

  // ---- Subclass hooks for PIN confirmation ----
  // Called from parse_and_dispatch_() implementation when the parsed
  // CommonFields contains pin_confirmed. The hub updates stored_pin_
  // and pin_confirmed_, then notifies via pin_input_cb_.
  void on_pin_confirmed_(const std::string &pin);
  // Virtual so host tests can spy on the subclass call contract
  virtual void log_data_keys_(const std::vector<std::string> &keys);

private:
  // ---- post_bond stages (translated from the 5-stage YAML script) ----
  void start_post_bond_();
  void post_bond_initial_();
  void post_bond_post_encryption_();
  void post_bond_trigger_search_();
  void post_bond_poll_attempt_(int attempt);
  void post_bond_giveup_();

  // ---- subscribe stages ----
  void start_subscribe_();
  void subscribe_register_and_cccd_();
  void subscribe_unlock_();
  void subscribe_initial_get_();

  // ---- fast_reconnect stages ----
  void start_fast_reconnect_();
  void fast_reconnect_subscribe_();

  // ---- helpers ----
  void publish_status_(const std::string &text);
  void clear_handles_();
  void clear_session_state_();
  void process_message_complete_();
  void log_chunked_debug_(const std::string &msg);
  void write_unlock_channel_(std::uint16_t handle);
  void write_poll_command_(std::uint16_t handle);
  bool handle_lacking_properties_(const std::string &msg);
  void write_set_property_(const std::string &key,
                           const std::string &json_value);
  void update_handles_from_db_();

  // ---- collaborators (non-owning pointers, lifetime owned by caller) ----
  BleTransport *transport_ = nullptr;
  Scheduler *scheduler_ = nullptr;
  std::string name_;
  std::function<void(const std::string &)> status_cb_;
  std::function<void(const std::string &)> pin_input_cb_;
  std::function<void()> subscribe_cb_;

  // ---- state (1:1 with the previous YAML globals) ----
  int phase_ = 0;
  std::uint16_t d5_handle_ = 0;
  std::uint16_t d6_handle_ = 0;
  std::uint16_t d7_handle_ = 0;
  std::string stored_pin_;
  bool pin_confirmed_ = true; // matches YAML's restore_value:true initial
  bool poll_ok_ = false;
  int fast_retries_ = 0;
  int poll_miss_ = 0;
  bool debug_mode_ = false;
  std::uint32_t poll_offset_ms_ = 0;
  esphome::subzero_protocol::PollVerb poll_verb_ =
      esphome::subzero_protocol::PollVerb::kGetAsync;

  // Pending-flow flags — match the YAML scripts' `is_running()` checks.
  // periodic_poll consults these to avoid clobbering an in-progress
  // connection setup.
  bool post_bond_running_ = false;
  bool subscribe_running_ = false;
  bool fast_reconnect_running_ = false;

  // D6 message buffer (D5 is heartbeat-only post-PR-#72).
  esphome::subzero_protocol::MessageBuffer json_buf_;

  // ---- tunables ----
  // Brace-matched to the YAML's hard-coded values. If any of these
  // change, on-device verification is required — they encode hard-won
  // Bluedroid timing fixes.
  static constexpr std::uint32_t kPostBondInitialDelayMs = 500;
  static constexpr std::uint32_t kPostEncryptionDelayMs = 1000;
  static constexpr std::uint32_t kSearchPollDelayMs = 500;
  static constexpr std::uint32_t kPostBondPollPeriodMs = 5000;
  static constexpr int kPostBondMaxPolls = 3;
  static constexpr std::uint32_t kPostBondGiveupDisconnectMs = 2000;
  static constexpr std::uint32_t kPostBondGiveupReconnectMs = 1000;
  static constexpr std::uint32_t kFastReconnectEncryptDelayMs = 1500;
  static constexpr std::uint32_t kSubscribeUnlockDelayMs = 500;
  static constexpr std::uint32_t kSubscribeInitialGetDelayMs = 1000;
  static constexpr std::uint32_t kSubmitPinPollDelayMs = 3000;
  static constexpr std::uint32_t kResetPairingDisconnectDelayMs = 1000;
  static constexpr int kStaleBondsThreshold = 3;
  static constexpr int kZombiePollMissThreshold = 3;
  static constexpr std::uint32_t kVerbFallbackRetryDelayMs = 1000;
};

} // namespace subzero_appliance
} // namespace esphome
