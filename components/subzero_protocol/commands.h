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
// Used on D6 only post-PR-#72 — D5 returns nothing for get_async.
inline std::string build_get_async() { return "{\"cmd\":\"get_async\"}\n"; }

// `display_pin` makes the appliance show its random pairing PIN on its
// front-panel display for the requested duration (seconds). Sent on D5
// before the user enters that PIN into HA.
inline std::string build_display_pin(int duration_seconds = 30) {
  return "{\"cmd\":\"display_pin\",\"params\":{\"duration\": " +
         std::to_string(duration_seconds) + "}}\n";
}

} // namespace subzero_protocol
} // namespace esphome
