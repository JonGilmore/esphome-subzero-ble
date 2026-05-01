#pragma once

#include "hub.h"

#include "../subzero_protocol/dispatch.h"
#include "../subzero_protocol/protocol.h"

#ifdef USE_ESP32
#include "../subzero_protocol/dispatch_esphome.h"
#endif

namespace esphome {
namespace subzero_appliance {

#ifdef USE_ESP32
class RangeHub : public SubzeroHub {
public:
  RangeHub() = default;
  void set_bus(esphome::subzero_protocol::RangeBus *bus) { bus_ = bus; }

protected:
  bool parse_and_dispatch_(const std::string &msg) override {
    auto s = esphome::subzero_protocol::parse_range(msg);
    if (!s.valid)
      return false;
    log_data_keys_(s.data_keys);
    if (s.common.pin_confirmed) {
      on_pin_confirmed_(*s.common.pin_confirmed);
    }
    if (bus_ != nullptr) {
      esphome::subzero_protocol::dispatch_range(s, *bus_);
    }
    return true;
  }

private:
  esphome::subzero_protocol::RangeBus *bus_ = nullptr;
};
#endif

} // namespace subzero_appliance
} // namespace esphome
