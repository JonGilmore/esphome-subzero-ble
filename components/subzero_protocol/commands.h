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

#include <string>

namespace esphome {
namespace subzero_protocol {

// `unlock_channel` opens the encrypted command/data channel after bonding
// using the appliance's currently-displayed PIN. Sent on D5 (control)
// during initial subscribe and on D6 (data) before every periodic poll
// — D6's session expires independently of the BLE link.
inline std::string build_unlock_channel(const std::string &pin) {
  return "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"" + pin + "\"}}\n";
}

// `get_async` requests a full state dump on the channel it's written to.
// Used on D6 only post-PR-#72 — D5 returns nothing for get_async.
inline std::string build_get_async() {
  return "{\"cmd\":\"get_async\"}\n";
}

// `display_pin` makes the appliance show its random pairing PIN on its
// front-panel display for the requested duration (seconds). Sent on D5
// before the user enters that PIN into HA.
inline std::string build_display_pin(int duration_seconds = 30) {
  return "{\"cmd\":\"display_pin\",\"params\":{\"duration\": " +
         std::to_string(duration_seconds) + "}}\n";
}

}  // namespace subzero_protocol
}  // namespace esphome
