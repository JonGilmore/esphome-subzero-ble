#include "protocol.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace esphome::subzero_protocol;

namespace {
std::string read_file(const fs::path &p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Numeric-tolerant deep equality: treats int 38 == float 38.0.
bool json_equal(const json &a, const json &b) {
  if (a.is_number() && b.is_number()) {
    return a.get<double>() == b.get<double>();
  }
  if (a.type() != b.type())
    return false;
  if (a.is_object()) {
    if (a.size() != b.size())
      return false;
    for (auto it = a.begin(); it != a.end(); ++it) {
      auto bi = b.find(it.key());
      if (bi == b.end())
        return false;
      if (!json_equal(it.value(), *bi))
        return false;
    }
    return true;
  }
  if (a.is_array()) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); i++) {
      if (!json_equal(a[i], b[i]))
        return false;
    }
    return true;
  }
  return a == b;
}

#define OPT_PUT(obj, s, field)                                                 \
  if ((s).field)                                                               \
  (obj)[#field] = *(s).field

json version_to_json(const Version &v) {
  json o = json::object();
  OPT_PUT(o, v, fw);
  OPT_PUT(o, v, api);
  OPT_PUT(o, v, bleapp);
  OPT_PUT(o, v, os);
  OPT_PUT(o, v, rtapp);
  OPT_PUT(o, v, appliance);
  return o;
}

json common_to_json(const CommonFields &c) {
  json o = json::object();
  OPT_PUT(o, c, pin_confirmed);
  OPT_PUT(o, c, sabbath_on);
  OPT_PUT(o, c, service_required);
  OPT_PUT(o, c, appliance_model);
  OPT_PUT(o, c, uptime);
  OPT_PUT(o, c, appliance_serial);
  OPT_PUT(o, c, appliance_type);
  OPT_PUT(o, c, diagnostic_status);
  OPT_PUT(o, c, build_date);
  json v = version_to_json(c.version);
  if (!v.empty())
    o["version"] = v;
  return o;
}

json fridge_to_json(const FridgeState &s) {
  json o = json::object();
  o["valid"] = s.valid;
  if (!s.valid)
    return o;
  json c = common_to_json(s.common);
  if (!c.empty())
    o["common"] = c;
  OPT_PUT(o, s, notif_event);
  OPT_PUT(o, s, ref_set_temp);
  OPT_PUT(o, s, door_ajar);
  OPT_PUT(o, s, frz_set_temp);
  OPT_PUT(o, s, frz_door_ajar);
  OPT_PUT(o, s, ice_maker_on);
  OPT_PUT(o, s, ref2_set_temp);
  OPT_PUT(o, s, ref2_door_ajar);
  OPT_PUT(o, s, wine_door_ajar);
  OPT_PUT(o, s, wine_set_temp);
  OPT_PUT(o, s, wine2_set_temp);
  OPT_PUT(o, s, wine_temp_alert_on);
  OPT_PUT(o, s, crisp_set_temp);
  OPT_PUT(o, s, crisp_temp_mode);
  OPT_PUT(o, s, air_filter_on);
  OPT_PUT(o, s, air_filter_pct_remaining);
  OPT_PUT(o, s, water_filter_pct_remaining);
  OPT_PUT(o, s, water_filter_gal_remaining);
  OPT_PUT(o, s, water_filter_end_date);
  OPT_PUT(o, s, long_vacation_on);
  OPT_PUT(o, s, short_vacation_on);
  OPT_PUT(o, s, high_use_on);
  OPT_PUT(o, s, high_use_start_time);
  OPT_PUT(o, s, high_use_end_time);
  OPT_PUT(o, s, night_mode);
  OPT_PUT(o, s, night_ice_on);
  OPT_PUT(o, s, max_ice_on);
  OPT_PUT(o, s, max_ice_start_time);
  OPT_PUT(o, s, max_ice_end_time);
  OPT_PUT(o, s, unit_on);
  OPT_PUT(o, s, smart_grid_on);
  OPT_PUT(o, s, pin_window_open);
  OPT_PUT(o, s, active_faults);
  OPT_PUT(o, s, humidity_control);
  OPT_PUT(o, s, door_ajar_timeout);
  OPT_PUT(o, s, ap_ssid);
  OPT_PUT(o, s, ap_rssi);
  OPT_PUT(o, s, ap_chan);
  OPT_PUT(o, s, ap_enc);
  return o;
}

json dishwasher_to_json(const DishwasherState &s) {
  json o = json::object();
  o["valid"] = s.valid;
  if (!s.valid)
    return o;
  json c = common_to_json(s.common);
  if (!c.empty())
    o["common"] = c;
  OPT_PUT(o, s, notif_event);
  OPT_PUT(o, s, door_ajar);
  OPT_PUT(o, s, wash_cycle_on);
  OPT_PUT(o, s, heated_dry_on);
  OPT_PUT(o, s, extended_dry_on);
  OPT_PUT(o, s, high_temp_wash_on);
  OPT_PUT(o, s, sani_rinse_on);
  OPT_PUT(o, s, rinse_aid_low);
  OPT_PUT(o, s, softener_low);
  OPT_PUT(o, s, light_on);
  OPT_PUT(o, s, remote_ready);
  OPT_PUT(o, s, delay_start_timer_active);
  OPT_PUT(o, s, wash_status);
  OPT_PUT(o, s, wash_cycle);
  OPT_PUT(o, s, wash_cycle_end_time);
  OPT_PUT(o, s, wash_time_remaining_min);
  return o;
}

json range_to_json(const RangeState &s) {
  json o = json::object();
  o["valid"] = s.valid;
  if (!s.valid)
    return o;
  json c = common_to_json(s.common);
  if (!c.empty())
    o["common"] = c;
  OPT_PUT(o, s, notif_event);
  OPT_PUT(o, s, door_ajar);
  OPT_PUT(o, s, cav_unit_on);
  OPT_PUT(o, s, cav_at_set_temp);
  OPT_PUT(o, s, cav_light_on);
  OPT_PUT(o, s, cav_remote_ready);
  OPT_PUT(o, s, cav_probe_on);
  OPT_PUT(o, s, cav_probe_at_set_temp);
  OPT_PUT(o, s, cav_probe_within_10deg);
  OPT_PUT(o, s, cav_gourmet_mode_on);
  OPT_PUT(o, s, cav_gourmet_recipe);
  OPT_PUT(o, s, cav_cook_timer_complete);
  OPT_PUT(o, s, cav_cook_timer_within_1min);
  OPT_PUT(o, s, cav_temp);
  OPT_PUT(o, s, cav_set_temp);
  OPT_PUT(o, s, cav_cook_mode);
  OPT_PUT(o, s, cav_probe_temp);
  OPT_PUT(o, s, cav_probe_set_temp);
  OPT_PUT(o, s, kitchen_timer_active);
  OPT_PUT(o, s, kitchen_timer_complete);
  OPT_PUT(o, s, kitchen_timer_within_1min);
  OPT_PUT(o, s, kitchen_timer_end_time);
  OPT_PUT(o, s, kitchen_timer2_active);
  OPT_PUT(o, s, kitchen_timer2_complete);
  OPT_PUT(o, s, kitchen_timer2_within_1min);
  OPT_PUT(o, s, kitchen_timer2_end_time);
  OPT_PUT(o, s, cav2_unit_on);
  OPT_PUT(o, s, cav2_door_ajar);
  OPT_PUT(o, s, cav2_at_set_temp);
  OPT_PUT(o, s, cav2_light_on);
  OPT_PUT(o, s, cav2_remote_ready);
  OPT_PUT(o, s, cav2_probe_on);
  OPT_PUT(o, s, cav2_probe_at_set_temp);
  OPT_PUT(o, s, cav2_probe_within_10deg);
  OPT_PUT(o, s, cav2_gourmet_mode_on);
  OPT_PUT(o, s, cav2_cook_timer_complete);
  OPT_PUT(o, s, cav2_temp);
  OPT_PUT(o, s, cav2_set_temp);
  OPT_PUT(o, s, cav2_cook_mode);
  OPT_PUT(o, s, cav2_probe_temp);
  OPT_PUT(o, s, cav2_probe_set_temp);
  return o;
}

enum class Parser { Fridge, Dishwasher, Range };

Parser dispatch(const std::string &stem) {
  if (stem.rfind("dishwasher_", 0) == 0)
    return Parser::Dishwasher;
  if (stem.rfind("range_", 0) == 0 || stem.rfind("walloven_", 0) == 0)
    return Parser::Range;
  // fridge_*, error_*, pin_*: all use parse_fridge (PIN & error paths are
  // identical across the three parsers, so fridge is the representative).
  return Parser::Fridge;
}

json run_parser(Parser p, const std::string &input) {
  switch (p) {
  case Parser::Fridge:
    return fridge_to_json(parse_fridge(input));
  case Parser::Dishwasher:
    return dishwasher_to_json(parse_dishwasher(input));
  case Parser::Range:
    return range_to_json(parse_range(input));
  }
  return {};
}

struct Fixture {
  std::string name;
  fs::path input_path;
  fs::path expected_path;
};

std::vector<Fixture> discover_fixtures() {
  std::vector<Fixture> out;
  fs::path dir(FIXTURES_DIR);
  for (auto &entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file())
      continue;
    auto &p = entry.path();
    if (p.extension() != ".json")
      continue;
    std::string stem = p.stem().string();
    // Skip expected files — those end with ".expected"
    if (stem.size() >= 9 && stem.compare(stem.size() - 9, 9, ".expected") == 0)
      continue;
    fs::path expected = dir / (stem + ".expected.json");
    if (!fs::exists(expected))
      continue;
    out.push_back({stem, p, expected});
  }
  std::sort(out.begin(), out.end(),
            [](auto &a, auto &b) { return a.name < b.name; });
  return out;
}

class FixtureTest : public ::testing::TestWithParam<Fixture> {};

TEST_P(FixtureTest, Parses) {
  const auto &fx = GetParam();
  std::string input = read_file(fx.input_path);
  json expected = json::parse(read_file(fx.expected_path));
  json actual = run_parser(dispatch(fx.name), input);
  EXPECT_TRUE(json_equal(actual, expected))
      << "Fixture: " << fx.name << "\n"
      << "Expected: " << expected.dump(2) << "\n"
      << "Actual:   " << actual.dump(2);
}

INSTANTIATE_TEST_SUITE_P(AllFixtures, FixtureTest,
                         ::testing::ValuesIn(discover_fixtures()),
                         [](const ::testing::TestParamInfo<Fixture> &info) {
                           return info.param.name;
                         });

// A handful of targeted tests for protocol behaviors that deserve explicit
// naming (rather than just fixture diffs).

TEST(ProtocolTest, EmptyStringIsInvalid) {
  auto f = parse_fridge("");
  EXPECT_FALSE(f.valid);
}

TEST(ProtocolTest, PinConfirmationPopulatesCommon) {
  auto f = parse_fridge(R"({"status":0,"resp":{"pin":"654321"}})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.common.pin_confirmed.has_value());
  EXPECT_EQ(*f.common.pin_confirmed, "654321");
}

TEST(ProtocolTest, PinTooLongIsIgnored) {
  auto f = parse_fridge(R"({"status":0,"resp":{"pin":"12345678901"}})");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.common.pin_confirmed.has_value());
}

TEST(ProtocolTest, StatusNonZeroIsInvalid) {
  auto f = parse_fridge(R"({"status":1,"resp":{"ref_set_temp":38}})");
  EXPECT_FALSE(f.valid);
}

TEST(ProtocolTest, FridgeWaterFilterGallonsAndEndDate) {
  auto f = parse_fridge(R"({"status":0,"resp":{
    "water_filter_pct_remaining": 50,
    "water_filter_gal_remaining": 162.5,
    "water_filter_end_date": "2027-05-10"
  }})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.water_filter_gal_remaining.has_value());
  EXPECT_FLOAT_EQ(*f.water_filter_gal_remaining, 162.5f);
  ASSERT_TRUE(f.water_filter_end_date.has_value());
  // Date-only input is promoted to a full ISO8601 timestamp so HA's
  // `timestamp` device_class accepts the string.
  EXPECT_EQ(*f.water_filter_end_date, "2027-05-10T00:00:00+00:00");
}

TEST(ProtocolTest, FridgeWaterFilterEndDatePreservesFullTimestamp) {
  // If a future firmware ever sends a full timestamp, pass it through
  // unmodified.
  auto f = parse_fridge(R"({"status":0,"resp":{
    "water_filter_end_date": "2027-05-10T12:34:56Z"
  }})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.water_filter_end_date.has_value());
  EXPECT_EQ(*f.water_filter_end_date, "2027-05-10T12:34:56Z");
}

TEST(ProtocolTest, FridgeDoorFallsBackToGenericDoor) {
  auto f =
      parse_fridge(R"({"seq":1,"props":{"door_ajar":true},"msg_types":2})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.door_ajar.has_value());
  EXPECT_TRUE(*f.door_ajar);
}

TEST(ProtocolTest, FridgeFreezerOnlyDoesNotFallBackToFreezer) {
  auto f = parse_fridge(R"({"status":0,"resp":{
    "frz_door_ajar": false,
    "frz_set_temp": 0,
    "ice_maker_on": true,
    "water_filter_pct_remaining": 31
  }})");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.ref_set_temp.has_value());
  EXPECT_FALSE(f.door_ajar.has_value());
  ASSERT_TRUE(f.frz_set_temp.has_value());
  EXPECT_FLOAT_EQ(*f.frz_set_temp, 0.0f);
  ASSERT_TRUE(f.frz_door_ajar.has_value());
  EXPECT_FALSE(*f.frz_door_ajar);
  ASSERT_TRUE(f.ice_maker_on.has_value());
  EXPECT_TRUE(*f.ice_maker_on);
  ASSERT_TRUE(f.water_filter_pct_remaining.has_value());
  EXPECT_FLOAT_EQ(*f.water_filter_pct_remaining, 31.0f);
}

// Wine-only fridges (e.g. DEU2450WDZ) publish wine_* keys but no
// ref_set_temp / door_ajar / frz_*
TEST(ProtocolTest, FridgeWineOnlyDoesNotFallBackToWine) {
  auto f = parse_fridge(R"({"status":0,"resp":{
    "wine_door_ajar": false,
    "wine_set_temp": 40,
    "wine2_set_temp": 55,
    "wine_temp_alert_on": false
  }})");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.ref_set_temp.has_value());
  EXPECT_FALSE(f.door_ajar.has_value());
  ASSERT_TRUE(f.wine_set_temp.has_value());
  EXPECT_FLOAT_EQ(*f.wine_set_temp, 40.0f);
  ASSERT_TRUE(f.wine2_set_temp.has_value());
  EXPECT_FLOAT_EQ(*f.wine2_set_temp, 55.0f);
  ASSERT_TRUE(f.wine_door_ajar.has_value());
  EXPECT_FALSE(*f.wine_door_ajar);
}

TEST(ProtocolTest, RangeDoorPrefersCavDoor) {
  auto r = parse_range(
      R"({"status":0,"resp":{"cav_door_ajar":true,"door_ajar":false}})");
  ASSERT_TRUE(r.valid);
  ASSERT_TRUE(r.door_ajar.has_value());
  EXPECT_TRUE(*r.door_ajar);
}

TEST(ProtocolTest, DishwasherSerialIsTrimmed) {
  auto d = parse_dishwasher(
      R"({"status":0,"resp":{"appliance_serial":"  12345  "}})");
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(d.common.appliance_serial.has_value());
  EXPECT_EQ(*d.common.appliance_serial, "12345");
}

TEST(ProtocolTest, DishwasherComputesTimeRemaining) {
  // End time is 45 minutes after root timestamp.
  auto d = parse_dishwasher(R"({
    "seq": 1, "msg_types": 2,
    "timestamp": "2026-04-24T14:00:00.000",
    "props": {"wash_cycle_end_time": "2026-04-24T14:45"}
  })");
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(d.wash_time_remaining_min.has_value());
  EXPECT_EQ(*d.wash_time_remaining_min, 45);
}

TEST(ProtocolTest, IsPollTrueForFullResponse) {
  auto f = parse_fridge(R"({"status":0,"resp":{"ref_set_temp":38}})");
  ASSERT_TRUE(f.valid);
  EXPECT_TRUE(f.is_poll);
}

TEST(ProtocolTest, IsPollFalseForPushNotification) {
  auto f =
      parse_fridge(R"({"seq":1,"msg_types":2,"props":{"ref_door_ajar":true}})");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.is_poll);
}

TEST(ProtocolTest, IsPollAcrossAllThreeParsers) {
  EXPECT_TRUE(parse_fridge(R"({"status":0,"resp":{}})").is_poll);
  EXPECT_TRUE(parse_dishwasher(R"({"status":0,"resp":{}})").is_poll);
  EXPECT_TRUE(parse_range(R"({"status":0,"resp":{}})").is_poll);
  EXPECT_FALSE(parse_fridge(R"({"seq":1,"props":{},"msg_types":2})").is_poll);
  EXPECT_FALSE(
      parse_dishwasher(R"({"seq":1,"props":{},"msg_types":2})").is_poll);
  EXPECT_FALSE(parse_range(R"({"seq":1,"props":{},"msg_types":2})").is_poll);
}

TEST(ProtocolTest, MsgTypes1PushExtractsDiagnosticStatus) {
  const std::string msg =
      R"({"diagnostic_status":"0x00000301111","msg_types":1,)"
      R"("seq":475,"timestamp":"2026-05-01T23:08:02-05:00"})";
  auto f = parse_fridge(msg);
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.is_poll);
  ASSERT_TRUE(f.common.diagnostic_status.has_value());
  EXPECT_EQ(*f.common.diagnostic_status, "0x00000301111");
}

TEST(ProtocolTest, MsgTypes1PushAcrossAllParsers) {
  const std::string msg =
      R"({"diagnostic_status":"0x12345","msg_types":1,"seq":1})";
  auto f = parse_fridge(msg);
  auto d = parse_dishwasher(msg);
  auto r = parse_range(msg);

  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(r.valid);
  EXPECT_FALSE(f.is_poll);
  EXPECT_FALSE(d.is_poll);
  EXPECT_FALSE(r.is_poll);

  ASSERT_TRUE(f.common.diagnostic_status.has_value());
  ASSERT_TRUE(d.common.diagnostic_status.has_value());
  ASSERT_TRUE(r.common.diagnostic_status.has_value());
  EXPECT_EQ(*f.common.diagnostic_status, "0x12345");
  EXPECT_EQ(*d.common.diagnostic_status, "0x12345");
  EXPECT_EQ(*r.common.diagnostic_status, "0x12345");
}

TEST(ProtocolTest, DataKeysCapturedInOrder) {
  auto f = parse_fridge(
      R"({"status":0,"resp":{"ref_set_temp":38,"ice_maker_on":true,"appliance_model":"2028"}})");
  ASSERT_TRUE(f.valid);
  ASSERT_EQ(f.data_keys.size(), 3u);
  EXPECT_EQ(f.data_keys[0], "ref_set_temp");
  EXPECT_EQ(f.data_keys[1], "ice_maker_on");
  EXPECT_EQ(f.data_keys[2], "appliance_model");
}

TEST(ProtocolTest, DataKeysPopulatedForPushMessages) {
  auto r = parse_range(
      R"({"seq":1,"msg_types":2,"props":{"cav_temp":350,"cav_unit_on":true}})");
  ASSERT_TRUE(r.valid);
  ASSERT_EQ(r.data_keys.size(), 2u);
  EXPECT_EQ(r.data_keys[0], "cav_temp");
  EXPECT_EQ(r.data_keys[1], "cav_unit_on");
}

TEST(ProtocolTest, FridgeNotifTypeMapsToEvent) {
  // msg_types=6 push: props + notif_type at root.
  auto f = parse_fridge(R"({
    "msg_types":6,"seq":1,"notif_seq":1,"notif_type":108,
    "props":{"water_filter_pct_remaining":0}
  })");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.notif_event.has_value());
  EXPECT_EQ(*f.notif_event, "water_filter_expired");
  // The push's data field was still extracted.
  ASSERT_TRUE(f.water_filter_pct_remaining.has_value());
}

TEST(ProtocolTest, FridgeMsgTypes4NotifOnlyPushIsValid) {
  auto f = parse_fridge(R"({
    "msg_types":4,"seq":103,"notif_seq":1360,"notif_type":109,
    "timestamp":"2026-05-04T09:38:51.391-05:00"
  })");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.is_poll);
  ASSERT_TRUE(f.notif_event.has_value());
  EXPECT_EQ(*f.notif_event, "air_filter_expired");
  // No data fields set since there are no props/resp.
  EXPECT_FALSE(f.door_ajar.has_value());
  EXPECT_TRUE(f.data_keys.empty());
}

TEST(ProtocolTest, FridgeUnknownNotifTypeFallsBack) {
  auto f =
      parse_fridge(R"({"msg_types":4,"seq":1,"notif_type":142,"notif_seq":1})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.notif_event.has_value());
  EXPECT_EQ(*f.notif_event, "fridge_event_142");
}

TEST(ProtocolTest, FridgeMsgTypes4WithoutNotifTypeIsInvalid) {
  auto f = parse_fridge(R"({"msg_types":4,"seq":1})");
  EXPECT_FALSE(f.valid);
}

TEST(ProtocolTest, DishwasherWashCycleStartedEvent) {
  auto d = parse_dishwasher(R"({
    "seq":711,"timestamp":"2026-05-03T15:09:37.021-05:00",
    "props":{"wash_cycle_on":true},
    "notif_seq":31,"notif_type":301,"msg_types":6
  })");
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(d.notif_event.has_value());
  EXPECT_EQ(*d.notif_event, "wash_cycle_started");
  ASSERT_TRUE(d.wash_cycle_on.has_value());
  EXPECT_TRUE(*d.wash_cycle_on);
}

TEST(ProtocolTest, DishwasherWashCycleCompleteEvent) {
  auto d = parse_dishwasher(R"({
    "msg_types":6,"seq":602,"notif_seq":1,"notif_type":302,
    "props":{"wash_cycle_on":false}
  })");
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(d.notif_event.has_value());
  EXPECT_EQ(*d.notif_event, "wash_cycle_complete");
}

TEST(ProtocolTest, RangeOvenPreheatCompleteEvent) {
  auto r = parse_range(R"({
    "seq":3606,"timestamp":"2026-04-30T21:37:15.519-05:00",
    "props":{"cav_at_set_temp":true},
    "notif_seq":14,"notif_type":201,"msg_types":6
  })");
  ASSERT_TRUE(r.valid);
  ASSERT_TRUE(r.notif_event.has_value());
  EXPECT_EQ(*r.notif_event, "oven_preheat_complete");
}

TEST(ProtocolTest, RangeKitchenTimerEndedEvents) {
  auto r1 = parse_range(R"({
    "msg_types":6,"seq":1,"notif_seq":1,"notif_type":207,
    "props":{"kitchen_timer_complete":true}
  })");
  ASSERT_TRUE(r1.notif_event.has_value());
  EXPECT_EQ(*r1.notif_event, "kitchen_timer_ended");

  auto r2 = parse_range(R"({
    "msg_types":6,"seq":1,"notif_seq":1,"notif_type":208,
    "props":{"kitchen_timer2_complete":true}
  })");
  ASSERT_TRUE(r2.notif_event.has_value());
  EXPECT_EQ(*r2.notif_event, "kitchen_timer2_ended");
}

TEST(ProtocolTest, NotifTypeInPollResponseIsIgnored) {
  auto f = parse_fridge(R"({
    "status":0,"resp":{
      "notifs":[{"notif_type":109,"notif_seq":1,"timestamp":"2026-05-04T09:38:51"}],
      "ref_set_temp":38
    }
  })");
  ASSERT_TRUE(f.valid);
  EXPECT_FALSE(f.notif_event.has_value());
}

TEST(ProtocolTest, DishwasherNegativeRemainingClampsToZero) {
  auto d = parse_dishwasher(R"({
    "seq": 1, "msg_types": 2,
    "timestamp": "2026-04-24T15:00:00.000",
    "props": {"wash_cycle_end_time": "2026-04-24T14:45"}
  })");
  ASSERT_TRUE(d.valid);
  ASSERT_TRUE(d.wash_time_remaining_min.has_value());
  EXPECT_EQ(*d.wash_time_remaining_min, 0);
}

// ---------------------------------------------------------------------
// parse_uptime_seconds — H:MM:SS with an unbounded hour field.
// ---------------------------------------------------------------------

TEST(UptimeTest, ParsesPlainHmmss) {
  auto v = parse_uptime_seconds("50:17:54");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 50u * 3600 + 17 * 60 + 54);
}

TEST(UptimeTest, ParsesUnboundedHourField) {
  auto v = parse_uptime_seconds("959:52:07");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 959u * 3600 + 52 * 60 + 7);
}

// Most firmware truncates the string to 8 chars, clipping the last
// seconds digit once hours reach three digits. The value must still
// parse — the resulting sub-10-second error is irrelevant here.
TEST(UptimeTest, ParsesFirmwareTruncatedSecondsField) {
  auto v = parse_uptime_seconds("627:09:3");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 627u * 3600 + 9 * 60 + 3);
}

TEST(UptimeTest, ParsesZeroSeconds) {
  auto v = parse_uptime_seconds("725:35:0");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 725u * 3600 + 35 * 60);
}

// Cross-check against the one captured appliance whose clock was never
// set: it reported time 2000-01-05T03:50:10 (99:50:10 since the 2000
// epoch) alongside uptime "99:50:08", sampled ~2s apart. This is the
// evidence that the third field is seconds, not tenths or deciseconds.
TEST(UptimeTest, MatchesUnsetClockCrossCheck) {
  auto v = parse_uptime_seconds("99:50:08");
  ASSERT_TRUE(v.has_value());
  const std::uint32_t since_epoch = 99u * 3600 + 50 * 60 + 10;
  EXPECT_NEAR(static_cast<double>(*v), static_cast<double>(since_epoch), 5.0);
}

// At four digits of hours the 8-character truncation eats the whole
// seconds field. The captured fixtures already reach 959 hours, so this
// arrives ~41 hours later; rejecting it would strand the sensor at
// unknown for the rest of the appliance's uptime.
TEST(UptimeTest, ParsesTruncatedFourDigitHourForm) {
  auto v = parse_uptime_seconds("1000:00:");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 1000u * 3600);

  auto w = parse_uptime_seconds("1234:56:");
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(*w, 1234u * 3600 + 56 * 60);
}

// Every duration representable in the uint32 return type is accepted -
// there is no arbitrary digit-count ceiling below that bound.
TEST(UptimeTest, AcceptsFullRepresentableRange) {
  auto v = parse_uptime_seconds("1000000:00:00");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 1000000ull * 3600);

  // 1193046:28:15 == 0xFFFFFFFF seconds exactly, the largest value that
  // fits; one second more must be rejected rather than wrapping.
  auto max_v = parse_uptime_seconds("1193046:28:15");
  ASSERT_TRUE(max_v.has_value());
  EXPECT_EQ(*max_v, 0xFFFFFFFFu);
  EXPECT_FALSE(parse_uptime_seconds("1193046:28:16").has_value());
}

TEST(UptimeTest, RejectsMalformedValues) {
  EXPECT_FALSE(parse_uptime_seconds("").has_value());
  EXPECT_FALSE(parse_uptime_seconds("1d2h").has_value());
  EXPECT_FALSE(parse_uptime_seconds("12:34").has_value());
  EXPECT_FALSE(parse_uptime_seconds("12:34:56:78").has_value());
  EXPECT_FALSE(parse_uptime_seconds(":34:56").has_value());
  EXPECT_FALSE(parse_uptime_seconds("12::56").has_value());
  EXPECT_FALSE(parse_uptime_seconds("12:34:5x").has_value());
  // Minutes/seconds outside a clock range mean it isn't H:MM:SS at all.
  EXPECT_FALSE(parse_uptime_seconds("12:60:00").has_value());
  EXPECT_FALSE(parse_uptime_seconds("12:00:60").has_value());
  // A digit run long enough to overflow the accumulator is malformed.
  EXPECT_FALSE(parse_uptime_seconds("99999999999999999999:00:00").has_value());
}

// Every uptime string present in the captured fixtures must parse — a
// regression here means real appliances would report unknown.
TEST(UptimeTest, ParsesEveryCapturedFixtureValue) {
  const char *captured[] = {
      "797:58:1",  "627:09:2", "627:09:3",  "139:31:5", "136:59:2",
      "959:52:0",  "113:12:02", "50:17:54", "99:49:49", "99:50:08",
      "725:35:0",
  };
  for (const char *s : captured) {
    EXPECT_TRUE(parse_uptime_seconds(s).has_value())
        << "captured fixture value failed to parse: " << s;
  }
}

} // namespace
