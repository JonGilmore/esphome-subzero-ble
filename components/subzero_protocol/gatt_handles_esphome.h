#pragma once

// On-device wrapper that adapts esp_gattc_db_elem_t arrays directly to
// the host-buildable extract_handles in gatt_handles.h.
//
// Lives in its own header (rather than gatt_handles.h) so the host gtest
// build can include gatt_handles.h without needing ESP-IDF stubs.

#include "esp_gattc_api.h"

#include "gatt_handles.h"

namespace esphome {
namespace subzero_protocol {

// Scan an IDF GATT db array (as returned by esp_ble_gattc_get_db) and
// update the three handle globals in-place. Existing non-zero values
// are preserved — calling this multiple times across discovery passes
// is idempotent and additive.
//
// The signature takes int refs (rather than uint16_t) to match the YAML
// globals' declared type — every `${prefix}_d*_handle` is `type: int`.
inline void update_handles_from_db(const esp_gattc_db_elem_t *db,
                                   std::uint16_t count, int &d5_handle,
                                   int &d6_handle, int &d7_handle) {
  AppHandles h;
  h.d5 = static_cast<std::uint16_t>(d5_handle);
  h.d6 = static_cast<std::uint16_t>(d6_handle);
  h.d7 = static_cast<std::uint16_t>(d7_handle);
  for (std::uint16_t i = 0; i < count; i++) {
    const auto &e = db[i];
    if (e.type != ESP_GATT_DB_CHARACTERISTIC)
      continue;
    if (e.uuid.len != ESP_UUID_LEN_128)
      continue;
    std::uint8_t suffix = e.uuid.uuid.uuid128[0];
    switch (suffix) {
    case 0xD5:
      if (h.d5 == 0)
        h.d5 = e.attribute_handle;
      break;
    case 0xD6:
      if (h.d6 == 0)
        h.d6 = e.attribute_handle;
      break;
    case 0xD7:
      if (h.d7 == 0)
        h.d7 = e.attribute_handle;
      break;
    }
  }
  d5_handle = h.d5;
  d6_handle = h.d6;
  d7_handle = h.d7;
}

} // namespace subzero_protocol
} // namespace esphome
