#pragma once

// Production Scheduler — wraps esphome::App.scheduler. Header-only.
// On-device only.

#ifdef USE_ESP32

#include "scheduler.h"

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace subzero_appliance {

class EsphomeScheduler : public Scheduler {
 public:
  // The Component pointer is the timeout's "owner" — when the component
  // is removed/destructed, all its timeouts are cancelled. Pass any
  // long-lived Component (the BLE client is the natural choice).
  void set_component(esphome::Component *c) { component_ = c; }

  void set_timeout(const char *name, std::uint32_t delay_ms,
                   std::function<void()> callback) override {
    if (component_ == nullptr) return;
    esphome::App.scheduler.set_timeout(component_, name, delay_ms,
                                       std::move(callback));
  }

  void cancel_timeout(const char *name) override {
    if (component_ == nullptr) return;
    esphome::App.scheduler.cancel_timeout(component_, name);
  }

  std::uint32_t now_ms() const override { return esphome::millis(); }

 private:
  esphome::Component *component_ = nullptr;
};

}  // namespace subzero_appliance
}  // namespace esphome

#endif  // USE_ESP32
