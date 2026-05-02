#pragma once

// JSON command builders for the Sub-Zero BLE protocol.
//
// All commands are written to the appliance's D5 (control) or D6 (data)
// characteristic and MUST be terminated with '\n' — the appliance's serial
// parser only emits a response once the newline arrives.
//
// Pure string functions, host-testable. Existing call sites in the YAML
// scripts inline these literals; centralizing them makes the protocol
// vocabulary explicit and gives Phase 3's eventual BleTransport a single
// source of truth for what bytes go on the wire.

#include <cstdio>
#include <string>

namespace esphome {
namespace subzero_protocol {

namespace detail {

// JSON-escape a string for safe embedding inside a `"..."` literal.
// Real Sub-Zero PINs are 4-5 digit numeric, but the HA text input that
// stores the PIN is `mode: text` and accepts arbitrary chars — a user
// could paste a string containing a quote or backslash, which would
// silently corrupt the unlock_channel payload and either drop the
// command on the floor or, worse, confuse the appliance's serial parser.
//
// Handles the spec-required escapes (RFC 8259 §7): backslash, quote,
// and the named C0 controls (\b \f \n \r \t). Other control bytes
// (0x00..0x1F) become `\u00XX`. Higher-bit bytes are passed through
// unmodified — the protocol is ASCII in practice and we don't want to
// re-encode arbitrary UTF-8.
inline std::string escape_json_string(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (char c : s) {
    unsigned char u = static_cast<unsigned char>(c);
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (u < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04X", u);
        out += buf;
      } else {
        out.push_back(c);
      }
      break;
    }
  }
  return out;
}

} // namespace detail

// `unlock_channel` opens the encrypted command/data channel after bonding
// using the appliance's currently-displayed PIN. Sent on D5 (control)
// during initial subscribe and on D6 (data) before every periodic poll
// — D6's session expires independently of the BLE link.
//
// PIN is JSON-escaped before embedding (CodeRabbit on PR #73). Sub-Zero
// PINs are numeric in practice, but the HA text input stores arbitrary
// strings and we want a malformed PIN to produce well-formed JSON rather
// than a corrupted wire payload.
inline std::string build_unlock_channel(const std::string &pin) {
  return "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"" +
         detail::escape_json_string(pin) + "\"}}\n";
}

// `get_async` requests a full state dump on the channel it's written to.
// Used on D6 only post-PR-#72 — D5 returns nothing for get_async. Verified
// in libapp.so blutter dump as the only BLE poll verb in
// `BluetoothCommands::getAsyncCmd`.
inline std::string build_get_async() { return "{\"cmd\":\"get_async\"}\n"; }

// `get` is our BLE-side fallback when an appliance returns
// `{"status":1,"resp":{},"status_msg":"An error occurred"}` to `get_async`
// — what the app's log strings call "appliance lacking properties".
//
// NOT a verb the official Android app sends over BLE. blutter
// decompilation of libapp.so v4.5.1 (Dart 3.11.0) shows
// `BluetoothCommands` has exactly 6 baked JSON verbs: `set`,
// `unlock_channel`, `scan`, `get_async`, `display_pin`,
// `reset_air_filter`. The official app's "Sending fallback GetAll
// command" log lives in the *cloud* path (`AzureApplianceCommands.
// sendGetCommand` Direct Method via `azure_appliance_commands.dart`),
// not BLE — and the `getAllWithParameters`/`GetAllParameters` symbols
// belong to the `shared_preferences_android` Flutter plugin, unrelated
// to the Sub-Zero protocol entirely.
//
// `{"cmd":"get"}` is original protocol work, justified by issue #91:
// mwbourgeois empirically confirmed it returns a 2394-byte full-state
// response from a Sub-Zero IR36550ST induction range fw 2.27 across
// multiple consecutive poll cycles. Plausibly accepted via firmware-side
// prefix matching (`get` → `get_async`) or a Sub-Zero service verb.
// Same response shape as `get_async` on success.
inline std::string build_get() { return "{\"cmd\":\"get\"}\n"; }

// Polling verb selector — the hub latches to whichever one a given
// appliance accepts. Default is kGetAsync (the only BLE poll verb in the
// official app); kGet is our empirical fallback for appliances that
// return the "lacking properties" sentinel to get_async.
enum class PollVerb {
  kGetAsync,
  kGet,
};

inline std::string build_poll_command(PollVerb v) {
  return v == PollVerb::kGet ? build_get() : build_get_async();
}

// True if `msg` matches the "appliance lacking properties" sentinel:
// status:1 plus an empty resp object. The exact wire bytes vary across
// firmwares (some include "status_msg":"An error occurred", some don't,
// whitespace differs), so we substring-match on the two stable tokens
// rather than parsing JSON. Compatible with both `"resp":{}` (no space)
// and `"resp": {}` (with space) — covers every observed variant.
inline bool is_lacking_properties_response(const std::string &msg) {
  if (msg.find("\"status\":1") == std::string::npos)
    return false;
  if (msg.find("\"resp\":{}") != std::string::npos)
    return true;
  if (msg.find("\"resp\": {}") != std::string::npos)
    return true;
  return false;
}

// `display_pin` makes the appliance show its random pairing PIN on its
// front-panel display for the requested duration (seconds). Sent on D5
// before the user enters that PIN into HA.
inline std::string build_display_pin(int duration_seconds = 30) {
  return "{\"cmd\":\"display_pin\",\"params\":{\"duration\": " +
         std::to_string(duration_seconds) + "}}\n";
}

// `set` writes a single property on the appliance. Sent on D5 (control
// channel) — the appliance acks with `{"status":0,"resp":{}}` and within
// ~1s pushes a `msg_types:2` notification on D6 echoing the new value,
// which our normal read pipeline picks up.
//
// Verified verbs from official-app HCI snoop (2026-04-26): bool fields
// like `cav_light_on`, integer fields like `kitchen_timer_duration`,
// string fields like `remote_svc_reg_token` and the WiFi triplet
// (`ap_ssid`/`ap_pass`/`ap_enc`).
//
// `json_value` is the already-formatted JSON literal (no quotes for
// numbers/bools, quotes + escaping for strings — use the typed helpers
// below instead of build_set directly when possible).
inline std::string build_set(const std::string &key,
                             const std::string &json_value) {
  return "{\"cmd\":\"set\",\"params\":{\"" + detail::escape_json_string(key) +
         "\":" + json_value + "}}\n";
}

inline std::string build_set_bool(const std::string &key, bool value) {
  return build_set(key, value ? "true" : "false");
}

inline std::string build_set_int(const std::string &key, int value) {
  return build_set(key, std::to_string(value));
}

inline std::string build_set_string(const std::string &key,
                                    const std::string &value) {
  return build_set(key, "\"" + detail::escape_json_string(value) + "\"");
}

} // namespace subzero_protocol
} // namespace esphome
