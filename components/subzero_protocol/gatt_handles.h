#pragma once

// GATT characteristic handle extraction for the Sub-Zero BLE protocol.
//
// Sub-Zero appliances expose five 128-bit-UUID characteristics whose only
// distinguishing byte is the first byte of the UUID — D4 / D5 / D6 / D7 / D8.
// Post-PR-#72 we only care about three:
//
//   D5 = encrypted control channel
//   D6 = encrypted data channel
//   D7 = open pre-auth diagnostic channel
//
// The post_bond script polls the GATT database up to four times during
// discovery (initial dump, then 3 retry passes 5s apart), and each pass
// repeats the same "scan db, find characteristics, extract handles by
// UUID first byte" loop. This header centralizes that logic so it can be
// host-tested with synthetic GATT entries.

#include <cstdint>

namespace esphome {
namespace subzero_protocol {

// A trimmed-down view of esp_gattc_db_elem_t carrying only the fields
// the handle-extractor needs. The YAML lambda converts each
// esp_gattc_db_elem_t to one of these before calling extract_handles.
//
// Decoupling from esp_gattc_api.h keeps the helper host-buildable
// (ESP-IDF headers aren't available outside the ESP32 build).
struct GattEntry {
  bool is_characteristic = false; // true if type == ESP_GATT_DB_CHARACTERISTIC
  bool is_uuid128 = false;        // true if uuid.len == ESP_UUID_LEN_128
  std::uint8_t uuid_first_byte =
      0;                    // uuid.uuid.uuid128[0]; valid only if is_uuid128
  std::uint16_t handle = 0; // attribute_handle
};

// Sub-Zero's data-plane handles, populated incrementally across
// discovery retries. A value of 0 means "not yet found".
struct AppHandles {
  std::uint16_t d5 = 0;
  std::uint16_t d6 = 0;
  std::uint16_t d7 = 0;

  bool has_d5() const { return d5 != 0; }
  bool has_d6() const { return d6 != 0; }
  bool has_d7() const { return d7 != 0; }

  // True once we have everything we need to subscribe + poll.
  bool ready() const { return has_d5() && has_d6(); }
};

// Iterate `entries` and update any not-yet-found handle in `out` whose
// UUID first byte matches D5/D6/D7. Existing non-zero handles in `out`
// are preserved — calling this across multiple discovery passes is
// idempotent and additive.
//
// Templated on Range so callers can pass a std::vector<GattEntry>,
// std::array, or a raw range constructed directly from
// esp_gattc_db_elem_t[] (see the YAML conversion call site).
template <typename Range>
inline void extract_handles(const Range &entries, AppHandles &out) {
  for (const auto &e : entries) {
    if (!e.is_characteristic || !e.is_uuid128)
      continue;
    switch (e.uuid_first_byte) {
    case 0xD5:
      if (out.d5 == 0)
        out.d5 = e.handle;
      break;
    case 0xD6:
      if (out.d6 == 0)
        out.d6 = e.handle;
      break;
    case 0xD7:
      if (out.d7 == 0)
        out.d7 = e.handle;
      break;
    }
  }
}

} // namespace subzero_protocol
} // namespace esphome
