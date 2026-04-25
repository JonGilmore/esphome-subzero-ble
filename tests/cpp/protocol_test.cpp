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
  OPT_PUT(o, s, air_filter_on);
  OPT_PUT(o, s, air_filter_pct_remaining);
  OPT_PUT(o, s, water_filter_pct_remaining);
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

TEST(ProtocolTest, FridgeDoorFallsBackToGenericDoor) {
  auto f =
      parse_fridge(R"({"seq":1,"props":{"door_ajar":true},"msg_types":2})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.door_ajar.has_value());
  EXPECT_TRUE(*f.door_ajar);
}

TEST(ProtocolTest, FridgeSetpointFallsBackToFreezer) {
  auto f = parse_fridge(R"({"status":0,"resp":{"frz_set_temp":-5}})");
  ASSERT_TRUE(f.valid);
  ASSERT_TRUE(f.ref_set_temp.has_value());
  EXPECT_FLOAT_EQ(*f.ref_set_temp, -5.0f);
  ASSERT_TRUE(f.frz_set_temp.has_value());
  EXPECT_FLOAT_EQ(*f.frz_set_temp, -5.0f);
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

} // namespace
