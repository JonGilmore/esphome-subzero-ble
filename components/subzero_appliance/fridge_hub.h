#pragma once

// Fridge-specific hub: parses fridge JSON and dispatches to a FridgeBus.

#include "hub.h"

#include "../subzero_protocol/dispatch.h"
#include "../subzero_protocol/protocol.h"

#ifdef USE_ESP32
#include "../subzero_protocol/dispatch_esphome.h"
#endif

namespace esphome {
namespace subzero_appliance {

#ifdef USE_ESP32
class FridgeHub : public SubzeroHub {
public:
  FridgeHub() = default;
  void set_bus(esphome::subzero_protocol::FridgeBus *bus) { bus_ = bus; }

protected:
  bool parse_and_dispatch_(const std::string &msg) override {
    auto s = esphome::subzero_protocol::parse_fridge(msg);
    if (!s.valid)
      return false;
    log_data_keys_(s.data_keys);
    if (s.common.pin_confirmed) {
      on_pin_confirmed_(*s.common.pin_confirmed);
    }
    if (bus_ != nullptr) {
      esphome::subzero_protocol::dispatch_fridge(s, *bus_);
    }
    return true;
  }

private:
  esphome::subzero_protocol::FridgeBus *bus_ = nullptr;
};
#endif

} // namespace subzero_appliance
} // namespace esphome
