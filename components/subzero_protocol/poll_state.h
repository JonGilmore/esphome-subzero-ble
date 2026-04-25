#pragma once

// Post-parse state-machine mutation, extracted from the per-appliance
// parse_json scripts (where it lived as ~10 lines of identical boilerplate
// across subzero_fridge.yaml, subzero_dishwasher.yaml, subzero_range.yaml).
//
// Invariants preserved here:
//
// 1. Only `is_poll == true` responses reset poll-related counters.
//    Push notifications prove the connection is alive (they do reset
//    poll_ok in the notify handler) but do NOT prove the command channel
//    is responding. If we cleared poll_in_flight on a push, the unlock-
//    expiry detector in periodic_poll would never escalate to a reconnect
//    on fw 8.5 appliances whose unlock session expires while pushes keep
//    flowing.
//
// 2. fw_85_detected is set sticky once seen, on a poll response only.
//    Cleared on disconnect (by the YAML on_disconnect handler).
//
// 3. fast_retries is reset on EVERY successful parse (poll or push) — any
//    valid response means the connection is functional, so prior fast-
//    reconnect failures aren't relevant anymore.

#include <string>

namespace esphome {
namespace subzero_protocol {

// Updates the YAML-side state-machine globals after a successful parse.
// Pass each global by reference (callers are ESPHome lambdas that look
// them up via id()).
//
// Templated on the parser-result type (FridgeState / DishwasherState /
// RangeState) — they all expose `.is_poll` and
// `.common.version.fw` (std::optional<std::string>).
template <typename T>
inline void apply_parse_result(bool &poll_in_flight,
                               int &unanswered_polls,
                               bool &first_poll_done,
                               bool &fw_85_detected,
                               int &fast_retries,
                               const T &s) {
  if (s.is_poll) {
    poll_in_flight = false;
    unanswered_polls = 0;
    first_poll_done = true;
    if (s.common.version.fw && *s.common.version.fw == std::string("8.5")) {
      fw_85_detected = true;
    }
  }
  fast_retries = 0;
}

}  // namespace subzero_protocol
}  // namespace esphome
