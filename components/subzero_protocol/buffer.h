#pragma once

// JSON message buffer for fragmented BLE indications.
//
// BLE delivers responses as a sequence of indications (small packets, often
// ~20-40 bytes each, sometimes ACL-fragmented further). The full appliance
// response is a single JSON object that may span ~50 fragments. This class
// accumulates fragments and detects when a complete object has been buffered
// by counting unbalanced braces.
//
// Brace counting is intentionally naive (no string-aware skipping): a `}`
// inside a JSON string value would falsely complete. The real protocol does
// not produce such strings in practice, and a more "clever" detector
// (PR #56's "stray { mid-buffer means corruption") was reverted in PR #61
// after it threw away legitimate poll responses whenever a short push
// arrived during a long poll. Keep this simple.
//
// Underflow guard: the depth counter is clamped at 0 on `}`. The prior YAML
// scanner did not clamp; an unmatched `}` in pre-message ACL-corruption
// garbage would drive depth negative, and a complete message arriving after
// it would never trip the depth==0 check. See buffer_test.cpp's
// StrayClosingBraceInPrefixIgnored regression test.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace esphome {
namespace subzero_protocol {

class MessageBuffer {
 public:
  // Reserve hint matches the prior YAML behavior. Initial reserve avoids
  // repeated reallocs across the ~50 fragments of a typical poll response.
  static constexpr std::size_t kReserveHint = 2048;

  // Hard cap before forced reset on the next feed(). Guards against a
  // wedged session that stops emitting `}` and would otherwise grow the
  // buffer forever.
  static constexpr std::size_t kMaxBytes = 4096;

  // Append bytes; return true once a complete message is buffered (a `}`
  // has brought running brace depth to 0). Sticky: once true, stays true
  // through subsequent appends until clear()/take_message() resets state.
  //
  // If the buffer was already over-capacity *before* this append, it is
  // reset first — same recovery semantics as the prior YAML lambda.
  bool feed(const std::uint8_t *data, std::size_t len);

  // If a complete message is buffered, return the substring starting at
  // the first `{` (leading garbage from ACL corruption is dropped) and
  // reset internal state. If no message is complete or no `{` exists,
  // returns nullopt; in the no-`{` case the buffer is also cleared as
  // unrecoverable garbage (matches the prior parse_json script behavior).
  std::optional<std::string> take_message();

  // Manual reset (use on disconnect or when callers detect parse failure
  // and want to discard the current buffer).
  void clear();

  std::size_t size() const { return buf_.size(); }
  bool complete() const { return complete_; }

 private:
  std::string buf_;
  int depth_ = 0;
  bool complete_ = false;
};

}  // namespace subzero_protocol
}  // namespace esphome
