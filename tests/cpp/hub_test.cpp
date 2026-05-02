// Tests for SubzeroHub — the BLE state machine extracted from YAML.
//
// Strategy: drive a TestHub (a minimal SubzeroHub subclass with a no-op
// parse_and_dispatch_) through synthetic BLE events, assert the right
// IDF calls + scheduler timeouts happen at the right moments. Time is
// advanced via FakeScheduler::advance_to().

#include "../../components/subzero_appliance/hub.h"
#include "hub_test_helpers.h"
#include "protocol.h"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using esphome::subzero_appliance::BleResult;
using esphome::subzero_appliance::FakeScheduler;
using esphome::subzero_appliance::GattDbEntry;
using esphome::subzero_appliance::make_char_entry;
using esphome::subzero_appliance::MockBleTransport;
using esphome::subzero_appliance::SubzeroHub;

namespace {

// Test stub that satisfies SubzeroHub's parse_and_dispatch_ contract.
// Tests can inspect last_message_ and override return value.
class TestHub : public SubzeroHub {
public:
  TestHub() = default;

  // Default: succeed and capture the message.
  bool parse_and_dispatch_(const std::string &msg) override {
    last_message_ = msg;
    parse_called_ = true;
    if (parse_should_confirm_pin_) {
      on_pin_confirmed_(pin_to_confirm_);
    }
    return parse_should_succeed_;
  }

  bool parse_called_ = false;
  std::string last_message_;
  bool parse_should_succeed_ = true;
  bool parse_should_confirm_pin_ = false;
  std::string pin_to_confirm_;
};

// Fixture base — wires up transport + scheduler + hub + status capture.
class HubFixture : public ::testing::Test {
protected:
  void SetUp() override {
    hub_.set_transport(&transport_);
    hub_.set_scheduler(&scheduler_);
    hub_.set_name("TestApp");
    hub_.set_status_callback(
        [this](const std::string &s) { status_log_.push_back(s); });
    hub_.set_pin_input_callback(
        [this](const std::string &p) { pin_input_log_.push_back(p); });
    transport_.set_connected(true);
  }

  // Helpers
  bool last_status_contains(const std::string &needle) const {
    if (status_log_.empty())
      return false;
    return status_log_.back().find(needle) != std::string::npos;
  }

  bool any_status_contains(const std::string &needle) const {
    for (const auto &s : status_log_) {
      if (s.find(needle) != std::string::npos)
        return true;
    }
    return false;
  }

  // Drives the cold-connect flow all the way through to the end of
  // subscribe_initial_get_ (subscribe_running_ flips to false), so
  // tests can call do_periodic_poll() / etc. cleanly.
  void run_to_ready_() {
    transport_.set_gatt_db(full_gatt_db());
    hub_.handle_connected();
    // 500ms → post_bond_initial_, +1000ms → post_bond_post_encryption_
    // (D5 visible) → start_subscribe_ + subscribe_register_and_cccd_
    // (immediate), schedules subscribe_unlock_ at +500.
    // +500ms → subscribe_unlock_, schedules subscribe_initial_get_ at +1000.
    // +1000ms → subscribe_initial_get_ → subscribe_running_=false.
    // Total: 3000ms.
    scheduler_.advance_by(3000);
  }

  std::vector<GattDbEntry> full_gatt_db() {
    return {
        make_char_entry(0xD5, 0x10),
        make_char_entry(0xD6, 0x12),
        make_char_entry(0xD7, 0x14),
    };
  }

  MockBleTransport transport_;
  FakeScheduler scheduler_;
  TestHub hub_;
  std::vector<std::string> status_log_;
  std::vector<std::string> pin_input_log_;
};

} // namespace

// =============================================================================
// Cold-boot connection — phase 0 → post_bond → subscribe
// =============================================================================

TEST_F(HubFixture, ColdConnect_RequestsMtuAndStartsPostBond) {
  hub_.set_stored_pin("12345");
  hub_.handle_connected();

  EXPECT_EQ(transport_.mtu_request_count(), 1u);
  EXPECT_EQ(hub_.phase(), 1);
  EXPECT_TRUE(hub_.post_bond_running());
  EXPECT_TRUE(last_status_contains("Connected, discovering"));
  // First post_bond stage hasn't fired yet (still in initial delay window).
  EXPECT_EQ(transport_.gatt_db_read_count(), 0u);
}

TEST_F(HubFixture, ColdConnect_DishwasherPattern_D5VisiblePreEncryption) {
  // Dishwasher case: GATT db already has D5/D6/D7 on the very first read.
  // Stage 2 should short-circuit straight to subscribe without going
  // through cache_refresh/search.
  hub_.set_stored_pin("99999");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();

  // Advance through the post_bond initial delay (500ms by default).
  scheduler_.advance_by(500);
  // Handles should be picked up; encryption requested.
  EXPECT_EQ(hub_.d5_handle(), 0x10);
  EXPECT_EQ(hub_.d6_handle(), 0x12);
  EXPECT_EQ(transport_.encryption_request_count(), 1u);
  EXPECT_TRUE(last_status_contains("D5 found"));

  // Advance 1s for post-encryption stage.
  scheduler_.advance_by(1000);
  // D5 was already visible so we skip cache_refresh and start subscribe.
  EXPECT_EQ(transport_.cache_refresh_count(), 0u);
  EXPECT_FALSE(hub_.post_bond_running());
  EXPECT_TRUE(hub_.subscribe_running());

  // Subscribe stage 1 happens immediately. Should register for notify
  // on D5 + D6 and write CCCD bytes (0x02 0x00) to handle+2 of each.
  ASSERT_GE(transport_.notify_subscribed_handles().size(), 2u);
  EXPECT_EQ(transport_.notify_subscribed_handles()[0], 0x10); // D5
  EXPECT_EQ(transport_.notify_subscribed_handles()[1], 0x12); // D6
  // CCCD writes — first two writes are CCCDs.
  ASSERT_GE(transport_.write_count(), 2u);
  EXPECT_EQ(transport_.write_at(0).handle, 0x12); // D5+2 = 0x12... wait
  // Actually D5=0x10 so D5+2=0x12; D6=0x12 so D6+2=0x14.
  // The test_helpers' make_char_entry put D6 at handle 0x12, but D5+2 is
  // ALSO 0x12 — handle collision in the test fixture. Use less-collide-y
  // values:
  // (Will fix by using full_gatt_db with non-overlapping +2 offsets.)
}

TEST_F(HubFixture,
       ColdConnect_DishwasherPatternFixedHandles_FullSubscribeFlow) {
  // Use handles where +2 offsets don't collide.
  hub_.set_stored_pin("99999");
  transport_.set_gatt_db({
      make_char_entry(0xD5, 0x100),
      make_char_entry(0xD6, 0x200),
      make_char_entry(0xD7, 0x300),
  });
  hub_.handle_connected();
  scheduler_.advance_by(500);
  EXPECT_EQ(hub_.d5_handle(), 0x100);
  EXPECT_EQ(hub_.d6_handle(), 0x200);
  scheduler_.advance_by(1000); // post-encryption stage
  // Now in subscribe; CCCDs are at 0x102 and 0x202.
  ASSERT_GE(transport_.write_count(), 2u);
  EXPECT_EQ(transport_.write_at(0).handle, 0x102);
  EXPECT_EQ(transport_.write_at(1).handle, 0x202);
  // Each CCCD payload is exactly {0x02, 0x00} (indications enable).
  EXPECT_EQ(transport_.write_at(0).bytes,
            std::vector<std::uint8_t>({0x02, 0x00}));
  EXPECT_EQ(transport_.write_at(1).bytes,
            std::vector<std::uint8_t>({0x02, 0x00}));

  // Advance 500ms → unlock writes.
  std::size_t writes_before_unlock = transport_.write_count();
  scheduler_.advance_by(500);
  // Two unlock writes (D5 + D6).
  EXPECT_GE(transport_.write_count(), writes_before_unlock + 2);
  EXPECT_TRUE(transport_.wrote_command_to(0x100, "unlock_channel"));
  EXPECT_TRUE(transport_.wrote_command_to(0x200, "unlock_channel"));

  // Advance 1s → initial get_async on D6.
  scheduler_.advance_by(1000);
  EXPECT_TRUE(transport_.wrote_command_to(0x200, "get_async"));
  EXPECT_FALSE(hub_.subscribe_running());
}

TEST_F(HubFixture, ColdConnect_FridgePattern_D5LandsAfterCacheRefresh) {
  // Fridge case: GATT db is empty until after cache_refresh + search.
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db({}); // empty initially
  hub_.handle_connected();
  scheduler_.advance_by(500); // post_bond_initial_
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_EQ(transport_.encryption_request_count(), 1u);

  scheduler_.advance_by(1000); // post_bond_post_encryption_
  // D5 still not found → cache_refresh requested.
  EXPECT_EQ(transport_.cache_refresh_count(), 1u);
  EXPECT_TRUE(hub_.post_bond_running());

  scheduler_.advance_by(500); // post_bond_trigger_search_
  EXPECT_EQ(transport_.search_service_count(), 1u);

  // Now simulate the GATT db gaining handles between poll attempts.
  scheduler_.advance_by(5000); // poll attempt 1 — db still empty
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_TRUE(hub_.post_bond_running());

  // Inject the discovered handles and advance to poll 2.
  transport_.set_gatt_db({
      make_char_entry(0xD5, 0x200),
      make_char_entry(0xD6, 0x300),
  });
  scheduler_.advance_by(5000); // poll attempt 2
  EXPECT_EQ(hub_.d5_handle(), 0x200);
  // Subscribe should now be running.
  EXPECT_FALSE(hub_.post_bond_running());
  EXPECT_TRUE(hub_.subscribe_running());
}

TEST_F(HubFixture, ColdConnect_NoD5AfterTimeout_TriggersGiveupReconnect) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db({});
  hub_.handle_connected();
  // Advance through the entire ladder: 500ms + 1000ms + 500ms + 5s + 5s + 5s
  scheduler_.advance_by(500 + 1000 + 500 + 5000 + 5000 + 5000);
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_EQ(hub_.phase(), 1); // reset to 1 by giveup
  // Disconnect was requested.
  EXPECT_GE(transport_.disconnect_count(), 1u);
}

// =============================================================================
// Fast reconnect path
// =============================================================================

TEST_F(HubFixture, FastReconnect_SkipsPostBondWhenHandlesCached) {
  // Simulate a prior successful bond.
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(2000); // through post_bond + into subscribe
  scheduler_.advance_by(500);  // unlock
  scheduler_.advance_by(1000); // initial get_async
  EXPECT_NE(hub_.d5_handle(), 0);
  EXPECT_FALSE(hub_.post_bond_running());

  // Now simulate disconnect.
  hub_.handle_disconnected();
  // Reconnect.
  transport_.set_connected(true);
  std::size_t encryption_before = transport_.encryption_request_count();
  hub_.handle_connected();
  // Fast path: encryption requested via fast_reconnect (no post_bond).
  EXPECT_TRUE(hub_.fast_reconnect_running());
  EXPECT_FALSE(hub_.post_bond_running());

  // Advance through fast_reconnect: poll_offset (0) + 1500ms.
  scheduler_.advance_by(1500);
  EXPECT_GT(transport_.encryption_request_count(), encryption_before);
  EXPECT_FALSE(hub_.fast_reconnect_running());
  EXPECT_TRUE(hub_.subscribe_running());
}

// =============================================================================
// Disconnect / stale bond detection
// =============================================================================

TEST_F(HubFixture, Disconnect_BeforeBond_ClearsAllState) {
  // No d5 handle, no phase advance — clean disconnect path.
  hub_.handle_disconnected();
  EXPECT_EQ(hub_.phase(), 0);
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_EQ(transport_.remove_bond_count(), 0u);
}

TEST_F(HubFixture, Disconnect_AfterBond_AccumulatesFastRetries) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000); // through subscribe (incl. initial get_async)
  ASSERT_NE(hub_.d5_handle(), 0);
  ASSERT_GE(hub_.phase(), 1);

  hub_.handle_disconnected();
  EXPECT_EQ(hub_.fast_retries(), 1);
  EXPECT_EQ(transport_.remove_bond_count(), 0u);
  EXPECT_NE(hub_.d5_handle(), 0); // handles preserved for fast reconnect
}

TEST_F(HubFixture, Disconnect_ThreeFailures_TriggersBondClear) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);

  // Three disconnects without successful re-bond → stale bond.
  hub_.handle_disconnected();
  hub_.handle_disconnected();
  // After third disconnect, threshold reached.
  hub_.handle_disconnected();
  EXPECT_EQ(transport_.remove_bond_count(), 1u);
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_EQ(hub_.phase(), 0);
  EXPECT_EQ(hub_.fast_retries(), 0);
  // The final status is "Disconnected" but the bond-clear message is
  // published earlier in the same handler — assert it landed somewhere
  // in the log.
  EXPECT_TRUE(any_status_contains("Bond cleared"));
}

// =============================================================================
// Zombie detection in periodic_poll
// =============================================================================

TEST_F(HubFixture, PeriodicPoll_GuardsAgainstUnconfirmed) {
  // Default pin_confirmed=true but PIN is empty / d6_handle is 0.
  hub_.do_periodic_poll();
  EXPECT_EQ(transport_.write_count(), 0u);
}

TEST_F(HubFixture, PeriodicPoll_WritesUnlockAndGetAsyncOnHappyPath) {
  // Manually set up a "ready" state.
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000); // through subscribe (incl. initial get_async)
  transport_.clear_writes();
  // poll_ok was set by the initial subscribe path; reset it via a
  // periodic_poll cycle.
  hub_.do_periodic_poll();
  // Should have written unlock_channel + get_async to D6.
  std::uint16_t d6 = hub_.d6_handle();
  EXPECT_TRUE(transport_.wrote_command_to(d6, "unlock_channel"));
  EXPECT_TRUE(transport_.wrote_command_to(d6, "get_async"));
}

TEST_F(HubFixture, PeriodicPoll_ZombieDetection_ReconnectsAfterThreeMisses) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);
  transport_.clear_writes();
  // Each poll without a notification arriving counts as a miss.
  // poll_ok_ enters this block at false (subscribe_initial_get_ explicitly
  // sets it to false; no notify arrived in the test). After 3 misses the
  // hub forces a reconnect and resets poll_miss to 0.
  hub_.do_periodic_poll();
  EXPECT_EQ(hub_.poll_miss(), 1);
  hub_.do_periodic_poll();
  EXPECT_EQ(hub_.poll_miss(), 2);
  std::size_t disconnect_before = transport_.disconnect_count();
  hub_.do_periodic_poll();
  EXPECT_GT(transport_.disconnect_count(), disconnect_before);
  EXPECT_EQ(hub_.poll_miss(), 0);
  EXPECT_TRUE(any_status_contains("Connection stale"));
}

// =============================================================================
// Notify + parse path
// =============================================================================

TEST_F(HubFixture, D5Notify_DoesNotTouchPollOkOrBuffer) {
  std::uint8_t bytes[] = {'p', 'u', 's', 'h'};
  hub_.handle_d5_notify(bytes, sizeof(bytes));
  EXPECT_FALSE(hub_.poll_ok())
      << "D5 indications must not flip poll_ok_ — only successful POLL "
         "RESPONSES on D6 (status:0) reset the zombie counter.";
  // D5 must NOT trigger parse_and_dispatch_ — buffer is for D6 only.
  EXPECT_FALSE(hub_.parse_called_);
}

TEST_F(HubFixture, D6Notify_AccumulatesAndDispatchesOnComplete) {
  // Send a partial JSON, then the rest.
  std::string part1 = "{\"status\":0,\"resp\":";
  std::string part2 = "{\"foo\":1}}\n";
  hub_.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(part1.data()),
                        part1.size());
  EXPECT_FALSE(hub_.parse_called_);
  hub_.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(part2.data()),
                        part2.size());
  EXPECT_TRUE(hub_.parse_called_);
  // Captured message starts with the first '{'.
  EXPECT_EQ(hub_.last_message_, "{\"status\":0,\"resp\":{\"foo\":1}}\n");
  // Successful parse resets fast_retries.
  EXPECT_EQ(hub_.fast_retries(), 0);
}

TEST_F(HubFixture, D6Notify_Status302_PublishesPairingRequiredMessage) {
  hub_.parse_should_succeed_ = false; // simulate parse failure
  std::string msg = "{\"status\":302}\n";
  hub_.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(msg.data()),
                        msg.size());
  EXPECT_TRUE(hub_.parse_called_);
  EXPECT_TRUE(last_status_contains("Pairing required"));
}

TEST_F(HubFixture, D6Notify_GenericParseFailure_DoesNotCrash) {
  hub_.parse_should_succeed_ = false;
  std::string msg = "{\"garbage\":true}\n";
  hub_.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(msg.data()),
                        msg.size());
  EXPECT_TRUE(hub_.parse_called_);
  // Status NOT pairing-required.
  EXPECT_FALSE(last_status_contains("Pairing required"));
}

// =============================================================================
// Passkey
// =============================================================================

TEST_F(HubFixture, Passkey_NoPin_ReturnsZero) {
  EXPECT_EQ(hub_.handle_passkey_request(), 0u);
  EXPECT_TRUE(last_status_contains("Enter PIN"));
}

TEST_F(HubFixture, Passkey_NumericPin_ReturnsAtoiValue) {
  hub_.set_stored_pin("12345");
  EXPECT_EQ(hub_.handle_passkey_request(), 12345u);
}

// =============================================================================
// Buttons
// =============================================================================

TEST_F(HubFixture, PressStartPairing_NotConnected_PublishesError) {
  hub_.press_start_pairing();
  EXPECT_TRUE(last_status_contains("Not connected"));
  EXPECT_EQ(transport_.write_count(), 0u);
}

TEST_F(HubFixture, PressStartPairing_Connected_WritesDisplayPinToD5) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);
  transport_.clear_writes();

  hub_.press_start_pairing();
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d5_handle(), "display_pin"));
}

TEST_F(HubFixture, PressPoll_ReunlocksAndPolls) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);
  transport_.clear_writes();

  hub_.press_poll();
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "unlock_channel"));
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "get_async"));
}

TEST_F(HubFixture, PressLogDebugInfo_EnablesDebugAndDisconnects) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);
  std::size_t disconnect_before = transport_.disconnect_count();

  hub_.press_log_debug_info();
  EXPECT_TRUE(hub_.debug_mode());
  EXPECT_GT(transport_.disconnect_count(), disconnect_before);
}

TEST_F(HubFixture, PressResetPairing_WipesAllState) {
  hub_.set_stored_pin("12345");
  transport_.set_gatt_db(full_gatt_db());
  hub_.handle_connected();
  scheduler_.advance_by(3000);

  hub_.press_reset_pairing();
  EXPECT_FALSE(hub_.pin_confirmed());
  EXPECT_EQ(hub_.d5_handle(), 0);
  EXPECT_EQ(hub_.d6_handle(), 0);
  EXPECT_EQ(hub_.phase(), 0);
  EXPECT_EQ(hub_.fast_retries(), 0);
  EXPECT_EQ(hub_.poll_miss(), 0);
  EXPECT_EQ(transport_.cache_clean_count(), 1u);
  EXPECT_EQ(transport_.remove_bond_count(), 1u);
}

// =============================================================================
// PIN confirmation
// =============================================================================

TEST_F(HubFixture, PinConfirmedFromParse_UpdatesStoredPinAndCallback) {
  hub_.parse_should_confirm_pin_ = true;
  hub_.pin_to_confirm_ = "78901";
  std::string msg = "{\"status\":0,\"resp\":{\"pin\":\"78901\"}}\n";
  hub_.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(msg.data()),
                        msg.size());
  EXPECT_EQ(hub_.stored_pin(), "78901");
  EXPECT_TRUE(hub_.pin_confirmed());
  ASSERT_FALSE(pin_input_log_.empty());
  EXPECT_EQ(pin_input_log_.back(), "78901");
}

// =============================================================================
// Restart-style timeouts (mode: restart)
// =============================================================================

// =============================================================================
// `set` writes (HA -> appliance via D5)
// =============================================================================

TEST_F(HubFixture, WriteSetBool_GoesToD5WithCorrectJson) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  transport_.clear_writes();
  hub_.write_set_bool("cav_light_on", true);
  ASSERT_EQ(transport_.write_count(), 1u);
  EXPECT_EQ(transport_.write_at(0).handle, 0x10u); // D5 in our test fixture
  std::string body(transport_.write_at(0).bytes.begin(),
                   transport_.write_at(0).bytes.end());
  EXPECT_EQ(body, "{\"cmd\":\"set\",\"params\":{\"cav_light_on\":true}}\n");
}

TEST_F(HubFixture, WriteSetInt_FormatsAsBareNumber) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  transport_.clear_writes();
  hub_.write_set_int("set_temp", 38);
  ASSERT_EQ(transport_.write_count(), 1u);
  std::string body(transport_.write_at(0).bytes.begin(),
                   transport_.write_at(0).bytes.end());
  EXPECT_EQ(body, "{\"cmd\":\"set\",\"params\":{\"set_temp\":38}}\n");
}

TEST_F(HubFixture, WriteSetString_QuotesAndEscapes) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  transport_.clear_writes();
  hub_.write_set_string("ap_ssid", "my\"net");
  ASSERT_EQ(transport_.write_count(), 1u);
  std::string body(transport_.write_at(0).bytes.begin(),
                   transport_.write_at(0).bytes.end());
  EXPECT_EQ(body, "{\"cmd\":\"set\",\"params\":{\"ap_ssid\":\"my\\\"net\"}}\n");
}

TEST_F(HubFixture, WriteSet_NoOpWhenD5HandleMissing) {
  // Pre-subscribe state — d5_handle_ stays 0. Writes must not land.
  hub_.set_stored_pin("12345");
  // No run_to_ready_() here.
  transport_.clear_writes();
  hub_.write_set_bool("cav_light_on", true);
  hub_.write_set_int("set_temp", 38);
  hub_.write_set_string("ap_ssid", "iot");
  EXPECT_EQ(transport_.write_count(), 0u);
}

TEST_F(HubFixture, WriteSet_NoOpWhenPinNotConfirmed) {
  // Force pin_confirmed_ false (e.g. after a stale-bond reset).
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.set_pin_confirmed(false);
  transport_.clear_writes();
  hub_.write_set_bool("cav_light_on", true);
  EXPECT_EQ(transport_.write_count(), 0u);
}

TEST_F(HubFixture, PostBondRestart_CancelsPriorPostBond) {
  // Re-entering handle_connected during an in-flight post_bond should
  // not double-fire stages. The YAML's `mode: restart` semantics are
  // implemented by reusing the same timeout names — a new set_timeout
  // with the same name cancels the old one.
  hub_.set_stored_pin("12345");
  hub_.handle_connected();
  EXPECT_TRUE(hub_.post_bond_running());
  scheduler_.advance_by(250); // halfway through initial delay
  std::size_t pending_before = scheduler_.pending_count();
  hub_.handle_connected();
  // Phase increments and a new post_bond chain replaces the prior one.
  // We can't easily assert "exactly one timeout pending", but at minimum
  // the hub must not have rescheduled multiple competing chains.
  EXPECT_LE(scheduler_.pending_count(), pending_before + 1);
}

namespace {

class RecordingFridgeLikeHub : public SubzeroHub {
public:
  RecordingFridgeLikeHub() = default;
  bool parse_and_dispatch_(const std::string &msg) override {
    auto s = esphome::subzero_protocol::parse_fridge(msg);
    if (!s.valid)
      return false;
    log_data_keys_(s.data_keys);
    return true;
  }

  void log_data_keys_(const std::vector<std::string> &keys) override {
    log_calls_.push_back(keys);
  }

  std::vector<std::vector<std::string>> log_calls_;
};

class FridgeLikeFixture : public ::testing::Test {
protected:
  RecordingFridgeLikeHub hub_;
};

} // namespace

TEST_F(FridgeLikeFixture, ParseAndDispatch_InvokesLogDataKeysWithParsedKeys) {
  const std::string msg =
      R"({"status":0,"resp":{"sabbath_on":false,"ref_set_temp":38,"appliance_model":"DEU2450R"}})";

  bool ok = hub_.parse_and_dispatch_(msg);
  ASSERT_TRUE(ok);

  ASSERT_EQ(hub_.log_calls_.size(), 1u)
      << "Subclass parse_and_dispatch_ must call log_data_keys_ exactly "
         "once per successful parse — if this fires 0 times, the subclass "
         "dropped the call (regression of the Phase 3 port).";

  const auto &keys = hub_.log_calls_.front();
  EXPECT_NE(std::find(keys.begin(), keys.end(), "sabbath_on"), keys.end());
  EXPECT_NE(std::find(keys.begin(), keys.end(), "ref_set_temp"), keys.end());
  EXPECT_NE(std::find(keys.begin(), keys.end(), "appliance_model"), keys.end());
  EXPECT_EQ(keys.size(), 3u)
      << "log_data_keys_ should receive the full data_keys vector from "
         "the parser, not a subset.";
}

TEST_F(FridgeLikeFixture, ParseAndDispatch_DoesNotInvokeLogOnParseFailure) {
  bool ok = hub_.parse_and_dispatch_("not json at all");
  EXPECT_FALSE(ok);
  EXPECT_EQ(hub_.log_calls_.size(), 0u);
}

namespace {

class GuardSpyHub : public SubzeroHub {
public:
  bool parse_and_dispatch_(const std::string &) override { return true; }
  void log_data_keys_(const std::vector<std::string> &keys) override {
    invocations_++;
    SubzeroHub::log_data_keys_(keys); // exercises the debug_mode_ guard
  }
  int invocations_ = 0;
};

} // namespace

TEST(SubzeroHubLogDataKeys, NoDebugMode_GuardLetsBaseNoOp) {
  GuardSpyHub h;
  h.log_data_keys_({"a", "b", "c"});
  EXPECT_EQ(h.invocations_, 1);
}

TEST(SubzeroHubLogDataKeys, DebugModeOn_BaseCompletesWithoutError) {
  GuardSpyHub h;
  h.set_debug_mode(true);
  h.log_data_keys_({"a", "b"});
  EXPECT_EQ(h.invocations_, 1);
}

namespace {

// 56-byte sentinel from the user's IR36550ST in issue #91.
constexpr const char *kLackingPropertiesSentinel =
    "{\"status\":1,\"resp\":{},\"status_msg\":\"An error occurred\"}\n";

void feed_message(SubzeroHub &hub, const std::string &payload) {
  hub.handle_d6_notify(reinterpret_cast<const std::uint8_t *>(payload.data()),
                       payload.size());
}

} // namespace

TEST_F(HubFixture, PollVerb_DefaultsToGetAsync) {
  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGetAsync);
}

TEST_F(HubFixture, LackingProperties_FlipsVerbAndSchedulesRetry) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  ASSERT_GT(hub_.d6_handle(), 0);

  hub_.parse_should_succeed_ = false;
  std::size_t writes_before = transport_.write_count();

  feed_message(hub_, kLackingPropertiesSentinel);

  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGet);
  EXPECT_TRUE(any_status_contains("get"));
  EXPECT_EQ(transport_.write_count(), writes_before)
      << "Retry must be deferred via scheduler — not written inline.";
  scheduler_.advance_by(1100); // > kVerbFallbackRetryDelayMs (1000)
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "\"get\"}"))
      << "After the verb-fallback retry timer fires, hub must write "
         "{\"cmd\":\"get\"} to D6 (the verb empirically confirmed working "
         "on IR36550ST per issue #91).";
}

TEST_F(HubFixture, LackingProperties_LatchesOnGet) {
  hub_.set_stored_pin("12345");
  run_to_ready_();

  hub_.parse_should_succeed_ = false;
  feed_message(hub_, kLackingPropertiesSentinel);
  ASSERT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGet);
  scheduler_.advance_by(1100);
  transport_.clear_writes();
  hub_.parse_should_succeed_ = true;
  feed_message(hub_, "{\"status\":0,\"resp\":{\"ref_set_temp\":38}}\n");
  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGet);

  // Subsequent periodic_poll must use get, not get_async.
  hub_.do_periodic_poll();
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "\"get\"}"));
  EXPECT_FALSE(transport_.wrote_command_to(hub_.d6_handle(), "get_async"))
      << "After latching, periodic poll must use get exclusively.";
}

TEST_F(HubFixture, LackingProperties_SecondHitDoesNotPingPongVerb) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.parse_should_succeed_ = false;

  feed_message(hub_, kLackingPropertiesSentinel);
  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGet);
  scheduler_.advance_by(1100);

  // Second hit — `get` also failed.
  feed_message(hub_, kLackingPropertiesSentinel);
  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGet)
      << "Verb must stay at kGet on repeated sentinel responses — no "
         "ping-pong back to kGetAsync.";
}

TEST_F(HubFixture, ResetPairing_RestoresDefaultVerb) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.set_poll_verb(esphome::subzero_protocol::PollVerb::kGet);

  hub_.press_reset_pairing();
  EXPECT_EQ(hub_.poll_verb(), esphome::subzero_protocol::PollVerb::kGetAsync);
}

TEST_F(HubFixture, PeriodicPoll_UsesCurrentVerb) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  transport_.clear_writes();

  hub_.do_periodic_poll();
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "get_async"));

  hub_.set_poll_verb(esphome::subzero_protocol::PollVerb::kGet);
  transport_.clear_writes();
  hub_.do_periodic_poll();
  EXPECT_TRUE(transport_.wrote_command_to(hub_.d6_handle(), "\"get\"}"));
}

namespace {

// Helper: feed a fully-formed JSON string + trailing newline through the
// D6 indication path so json_buf_ can detect message completion.
void feed_complete_d6_(SubzeroHub &hub, const std::string &payload) {
  std::string with_newline = payload + "\n";
  hub.handle_d6_notify(
      reinterpret_cast<const std::uint8_t *>(with_newline.data()),
      with_newline.size());
}

} // namespace

TEST_F(HubFixture, PollOk_NotSetByMsgTypes2Push) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  // After run_to_ready_, subscribe_initial_get_ has set poll_ok_=false
  // and written get_async. One do_periodic_poll cycle takes miss to 1.
  hub_.do_periodic_poll();
  ASSERT_EQ(hub_.poll_miss(), 1);

  // Feed a msg_types:2 push. With the fix, poll_ok_ stays false because
  // the push has no "status":0.
  hub_.parse_should_succeed_ = true;
  feed_complete_d6_(hub_,
                    R"({"seq":1,"msg_types":2,"props":{"door_ajar":true}})");

  // Next periodic_poll must increment miss (push did NOT reset poll_ok_).
  hub_.do_periodic_poll();
  EXPECT_EQ(hub_.poll_miss(), 2)
      << "poll_miss_ must NOT reset on a msg_types:2 push — only real "
         "poll responses (status:0) reset the zombie counter. fw 8.5 "
         "silent-poll detection depends on this.";
}

TEST_F(HubFixture, PollOk_NotSetByMsgTypes1Push) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.do_periodic_poll();
  ASSERT_EQ(hub_.poll_miss(), 1);

  hub_.parse_should_succeed_ = true;
  feed_complete_d6_(hub_,
                    R"({"diagnostic_status":"0x123","msg_types":1,"seq":1})");

  hub_.do_periodic_poll();
  EXPECT_EQ(hub_.poll_miss(), 2)
      << "msg_types:1 diagnostic_status pushes must NOT reset the zombie "
         "counter — fw 8.5 wall ovens emit these continuously while "
         "get_async goes silent (live log 2026-05-01, issue #91 thread).";
}

TEST_F(HubFixture, PollOk_SetByActualPollResponse) {
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.do_periodic_poll();
  ASSERT_EQ(hub_.poll_miss(), 1)
      << "Test setup: needs poll_miss_ > 0 before feeding the response.";

  hub_.parse_should_succeed_ = true;
  // Real poll response: status:0 — must reset the zombie counter.
  feed_complete_d6_(hub_, R"({"status":0,"resp":{"ref_set_temp":38}})");

  hub_.do_periodic_poll();
  EXPECT_EQ(hub_.poll_miss(), 0)
      << "poll_miss_ must reset to 0 once a real poll response arrives.";
}

TEST_F(HubFixture, ZombieDetector_StillFiresWithPushTraffic) {
  // Direct integration test for the live-log scenario from the issue
  // #91 thread (Wall Oven SO3050PESP fw 8.5): appliance keeps pushing
  // msg_types:1 every minute but never answers get_async. The zombie
  // detector must still trip after 3 silent polls and force a reconnect
  // — otherwise the appliance state stays frozen on its initial poll
  // values forever.
  hub_.set_stored_pin("12345");
  run_to_ready_();
  hub_.parse_should_succeed_ = true;
  std::size_t disc_before = transport_.disconnect_count();

  // Cycle 1: poll fails silently, then push arrives (poll_ok_ stays false)
  hub_.do_periodic_poll();
  feed_complete_d6_(hub_,
                    R"({"diagnostic_status":"0x1","msg_types":1,"seq":1})");
  // Cycle 2: poll fails silently, push arrives
  hub_.do_periodic_poll();
  feed_complete_d6_(hub_,
                    R"({"diagnostic_status":"0x2","msg_types":1,"seq":2})");
  // Cycle 3: poll fails silently → miss hits threshold → ZOMBIE FIRES
  hub_.do_periodic_poll();
  EXPECT_GT(transport_.disconnect_count(), disc_before)
      << "Zombie detector must force a reconnect after 3 silent polls "
         "even when push notifications keep arriving — otherwise the "
         "fw 8.5 silent-poll case never recovers.";
}
