#pragma once

// Test-only helpers: mock BleTransport + fake Scheduler. Used by
// hub_test.cpp and any other tests that exercise SubzeroHub.

#include "../../components/subzero_appliance/ble_transport.h"
#include "../../components/subzero_appliance/scheduler.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace subzero_appliance {

// ----------------------------------------------------------------------
// Fake scheduler — records pending timeouts; tests advance time on demand
// to fire callbacks deterministically.
// ----------------------------------------------------------------------
class FakeScheduler : public Scheduler {
public:
  struct Pending {
    std::uint32_t fire_at_ms;
    std::function<void()> callback;
  };

  void set_timeout(const char *name, std::uint32_t delay_ms,
                   std::function<void()> callback) override {
    pending_[name] = Pending{now_ms_ + delay_ms, std::move(callback)};
  }

  void cancel_timeout(const char *name) override { pending_.erase(name); }

  std::uint32_t now_ms() const override { return now_ms_; }

  // Advance time and fire any callbacks whose deadline has passed.
  // Callbacks that schedule new timeouts are honored on subsequent
  // advances (we don't recursively fire within a single advance).
  void advance_to(std::uint32_t target_ms) {
    while (now_ms_ < target_ms) {
      // Find the earliest pending timeout that fires before target_ms.
      auto it = pending_.end();
      std::uint32_t earliest = target_ms;
      for (auto cur = pending_.begin(); cur != pending_.end(); ++cur) {
        if (cur->second.fire_at_ms <= earliest) {
          earliest = cur->second.fire_at_ms;
          it = cur;
        }
      }
      if (it == pending_.end())
        break;
      now_ms_ = it->second.fire_at_ms;
      // Pop before firing so the callback can re-schedule the same name.
      auto cb = std::move(it->second.callback);
      pending_.erase(it);
      cb();
    }
    if (now_ms_ < target_ms)
      now_ms_ = target_ms;
  }

  // Convenience: advance by N ms.
  void advance_by(std::uint32_t ms) { advance_to(now_ms_ + ms); }

  // Number of pending timeouts (for assertions).
  std::size_t pending_count() const { return pending_.size(); }
  bool has_pending(const std::string &name) const {
    return pending_.find(name) != pending_.end();
  }

private:
  std::uint32_t now_ms_ = 0;
  std::map<std::string, Pending> pending_;
};

// ----------------------------------------------------------------------
// Mock BLE transport — records every call. Tests inject canned GATT db
// snapshots, set the connected/disconnected state, and assert the
// recorded write sequence.
// ----------------------------------------------------------------------
class MockBleTransport : public BleTransport {
public:
  struct WriteCall {
    std::uint16_t handle;
    std::vector<std::uint8_t> bytes;
    bool ack_required;
    BleResult result_returned;
  };

  // ---- BleTransport interface ----
  bool connected() const override { return connected_; }
  void connect() override {
    ++connect_count_;
    connected_ = true;
  }
  void disconnect() override {
    ++disconnect_count_;
    connected_ = false;
  }
  void request_mtu() override { ++mtu_request_count_; }
  void request_encryption() override { ++encryption_request_count_; }
  void remove_bond() override { ++remove_bond_count_; }
  void cache_refresh() override { ++cache_refresh_count_; }
  void cache_clean() override { ++cache_clean_count_; }
  void search_service() override { ++search_service_count_; }

  std::vector<GattDbEntry> read_gatt_db() override {
    ++gatt_db_read_count_;
    return gatt_db_;
  }

  BleResult register_for_notify(std::uint16_t handle) override {
    notify_subscribed_handles_.push_back(handle);
    return BleResult::kOk;
  }

  BleResult write(std::uint16_t handle, const std::uint8_t *data,
                  std::size_t len, bool ack_required = true) override {
    WriteCall c{handle, std::vector<std::uint8_t>(data, data + len),
                ack_required, next_write_result_};
    writes_.push_back(std::move(c));
    return next_write_result_;
  }

  // ---- Test fixture API ----

  // Pretend the BLE link is up / down. handle_disconnected/handle_connected
  // on the hub still need to be called explicitly by the test.
  void set_connected(bool v) { connected_ = v; }

  // Inject a canned GATT db that the next read_gatt_db() will return.
  void set_gatt_db(std::vector<GattDbEntry> db) { gatt_db_ = std::move(db); }

  // Make the next write() return this code (default kOk).
  void set_next_write_result(BleResult r) { next_write_result_ = r; }

  // Helpers for assertions
  std::size_t write_count() const { return writes_.size(); }
  const WriteCall &write_at(std::size_t i) const { return writes_.at(i); }
  std::size_t connect_count() const { return connect_count_; }
  std::size_t disconnect_count() const { return disconnect_count_; }
  std::size_t mtu_request_count() const { return mtu_request_count_; }
  std::size_t encryption_request_count() const {
    return encryption_request_count_;
  }
  std::size_t remove_bond_count() const { return remove_bond_count_; }
  std::size_t cache_refresh_count() const { return cache_refresh_count_; }
  std::size_t cache_clean_count() const { return cache_clean_count_; }
  std::size_t search_service_count() const { return search_service_count_; }
  std::size_t gatt_db_read_count() const { return gatt_db_read_count_; }
  const std::vector<std::uint16_t> &notify_subscribed_handles() const {
    return notify_subscribed_handles_;
  }

  // Was a write to `handle` made whose payload contains substring `needle`?
  bool wrote_command_to(std::uint16_t handle, const std::string &needle) const {
    for (const auto &c : writes_) {
      if (c.handle != handle)
        continue;
      std::string s(c.bytes.begin(), c.bytes.end());
      if (s.find(needle) != std::string::npos)
        return true;
    }
    return false;
  }

  void clear_writes() { writes_.clear(); }

private:
  bool connected_ = false;
  std::vector<WriteCall> writes_;
  std::vector<GattDbEntry> gatt_db_;
  std::vector<std::uint16_t> notify_subscribed_handles_;
  BleResult next_write_result_ = BleResult::kOk;

  std::size_t connect_count_ = 0;
  std::size_t disconnect_count_ = 0;
  std::size_t mtu_request_count_ = 0;
  std::size_t encryption_request_count_ = 0;
  std::size_t remove_bond_count_ = 0;
  std::size_t cache_refresh_count_ = 0;
  std::size_t cache_clean_count_ = 0;
  std::size_t search_service_count_ = 0;
  std::size_t gatt_db_read_count_ = 0;
};

// Convenience: build a single CHARACTERISTIC entry for the GATT db.
inline GattDbEntry make_char_entry(std::uint8_t uuid_first_byte,
                                   std::uint16_t handle) {
  GattDbEntry e;
  e.type = GattDbEntry::kCharacteristic;
  e.uuid_is_128bit = true;
  e.uuid_first_byte = uuid_first_byte;
  e.handle = handle;
  return e;
}

} // namespace subzero_appliance
} // namespace esphome
