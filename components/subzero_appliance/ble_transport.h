#pragma once

// Abstract BLE transport — the seam between the SubzeroHub state machine
// (host-testable) and the ESP-IDF Bluedroid stack (on-device only).
//
// Production wires up an EspIdfTransport (esp_idf_transport.h) that
// forwards each call to the matching `esp_ble_gattc_*` IDF function.
// Host tests inject a MockBleTransport that records every call and lets
// the test fixture drive synthetic GATT events into the hub.
//
// Why this exists: every "real" call the hub makes — write a
// characteristic, register for notifications, request encryption,
// dump the GATT db, refresh the cache, remove a bond — touches IDF
// headers that aren't available in the host gtest build. Hiding them
// behind a virtual interface lets the hub's state-machine logic
// (phase tracking, retry counters, zombie detection, multi-stage
// discovery ladder) run unmodified in unit tests.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace subzero_appliance {

// Mirror of esp_gattc_db_elem_t carrying only the fields the hub uses
// for handle extraction. EspIdfTransport adapts the IDF struct to this
// shape; tests construct it directly.
struct GattDbEntry {
  enum Type { kService, kCharacteristic, kDescriptor, kOther };
  Type type = kOther;
  bool uuid_is_128bit = false;
  std::uint8_t uuid_first_byte = 0;
  std::uint16_t handle = 0;
};

// Result codes — a thin abstraction over esp_err_t. Production maps
// directly; tests can inject failures.
enum class BleResult {
  kOk = 0,
  kFailed,        // generic write/read failure
  kNotConnected,  // call attempted while disconnected
  kInvalidHandle, // handle was 0 / not yet discovered
};

class BleTransport {
public:
  virtual ~BleTransport() = default;

  // Connection state — checked before each command write.
  virtual bool connected() const = 0;

  // Initiate connection (used by press_connect).
  virtual void connect() = 0;

  // Initiate disconnect (auto_connect will bring it back).
  virtual void disconnect() = 0;

  // Request the GATT MTU negotiation. Sub-Zero firmwares accept up to
  // 244 bytes; default 23 fragments responses heavily.
  virtual void request_mtu() = 0;

  // Encryption / bonding.
  virtual void request_encryption() = 0;
  virtual void remove_bond() = 0;

  // GATT cache management. cache_refresh re-runs discovery without
  // disconnecting; cache_clean wipes the cached attribute db (next
  // discovery starts from scratch).
  virtual void cache_refresh() = 0;
  virtual void cache_clean() = 0;

  // Trigger the IDF's async service search. Results come back via the
  // gattc event handler; the hub polls with read_gatt_db().
  virtual void search_service() = 0;

  // Snapshot the current GATT database. Returns whatever the cache
  // currently holds; on fridges this is empty until after bonding +
  // cache_refresh.
  virtual std::vector<GattDbEntry> read_gatt_db() = 0;

  // Subscribe to indications/notifications on the given attribute
  // handle. Sub-Zero requires manual CCCD writes (0x0002 = indications)
  // afterwards because ESPHome's auto-CCCD writes 0x01 = notifications.
  virtual BleResult register_for_notify(std::uint16_t char_handle) = 0;

  // Write to a GATT characteristic (or descriptor) by handle.
  // ack_required=true uses ESP_GATT_WRITE_TYPE_RSP; false uses
  // WRITE_TYPE_NO_RSP (not currently used by this protocol but kept for
  // future flexibility).
  virtual BleResult write(std::uint16_t handle, const std::uint8_t *data,
                          std::size_t len, bool ack_required = true) = 0;
};

} // namespace subzero_appliance
} // namespace esphome
