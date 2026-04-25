#pragma once

// Scheduler abstraction — the second test seam alongside BleTransport.
//
// The hub's state machine has multi-stage delay chains (post_bond's
// 500ms / 1s / 5s / 5s / 5s discovery ladder, subscribe's CCCD→unlock
// 500ms gap and unlock→get_async 1s gap, etc.). YAML scripts express
// these as `delay:` actions between lambdas; in C++ we need an
// equivalent. ESPHome provides `Component::set_timeout(name, delay,
// callback)` which is what production should use, but we want the
// host-test build to be able to advance time on demand without
// actually sleeping — so the hub takes a Scheduler interface and
// production wires it to App.scheduler.
//
// Naming convention for scheduled items: "hub:<stage>" where <stage>
// is the post_bond stage / subscribe step / etc. Re-issuing a timeout
// with the same name replaces the previous one (matching ESPHome's
// set_timeout semantics) — this is how the `mode: restart` scripts
// in the YAML get translated.

#include <cstdint>
#include <functional>

namespace esphome {
namespace subzero_appliance {

class Scheduler {
 public:
  virtual ~Scheduler() = default;

  // Schedule `callback` to run after `delay_ms` from now. If a timeout
  // with `name` is already pending, it is cancelled and replaced.
  virtual void set_timeout(const char *name,
                           std::uint32_t delay_ms,
                           std::function<void()> callback) = 0;

  // Cancel a pending timeout by name. No-op if no such timeout exists.
  virtual void cancel_timeout(const char *name) = 0;

  // Monotonic clock in milliseconds. Production returns millis();
  // tests return a fake time advanced by FakeScheduler::advance_to().
  virtual std::uint32_t now_ms() const = 0;
};

}  // namespace subzero_appliance
}  // namespace esphome
