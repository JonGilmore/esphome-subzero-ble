#include "dispatch.h"
#include "protocol.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace esphome::subzero_protocol;

namespace {

// Recording bus — stores every publish_* call into typed maps so tests
// can assert "method X was called with value Y" or "method X was never
// called". Mirrors the production *Bus structs in dispatch_esphome.h
// method-for-method; if dispatch.h gains a new bus call, both this
// recorder AND the production bus must add the matching method or the
// build fails.
struct CommonRecorder {
  std::map<std::string, bool> bools;
  std::map<std::string, std::string> strings;
  std::map<std::string, float> floats;
  std::map<std::string, int> ints;

  // The CommonFields publishers
  void publish_sabbath_on(bool v) { bools["sabbath_on"] = v; }
  void publish_svc_required(bool v) { bools["svc_required"] = v; }
  void publish_model(const std::string &v) { strings["model"] = v; }
  void publish_uptime(const std::string &v) { strings["uptime"] = v; }
  void publish_serial(const std::string &v) { strings["serial"] = v; }
  void publish_appliance_type(const std::string &v) {
    strings["appliance_type"] = v;
  }
  void publish_diag_status(const std::string &v) { strings["diag_status"] = v; }
  void publish_build_date(const std::string &v) { strings["build_date"] = v; }
  void publish_fw_version(const std::string &v) { strings["fw_version"] = v; }
  void publish_api_version(const std::string &v) { strings["api_version"] = v; }
  void publish_bleapp_version(const std::string &v) {
    strings["bleapp_version"] = v;
  }
  void publish_os_version(const std::string &v) { strings["os_version"] = v; }
  void publish_rtapp_version(const std::string &v) {
    strings["rtapp_version"] = v;
  }
  void publish_board_version(const std::string &v) {
    strings["board_version"] = v;
  }
};

struct FridgeRecorder : CommonRecorder {
  void publish_door_ajar(bool v) { bools["door_ajar"] = v; }
  void publish_frz_door_ajar(bool v) { bools["frz_door_ajar"] = v; }
  void publish_ice_maker(bool v) { bools["ice_maker"] = v; }
  void publish_ref2_door_ajar(bool v) { bools["ref2_door_ajar"] = v; }
  void publish_wine_door_ajar(bool v) { bools["wine_door_ajar"] = v; }
  void publish_wine_temp_alert(bool v) { bools["wine_temp_alert"] = v; }
  void publish_air_filter_on(bool v) { bools["air_filter_on"] = v; }

  void publish_set_temp(float v) { floats["set_temp"] = v; }
  void publish_frz_set_temp(float v) { floats["frz_set_temp"] = v; }
  void publish_ref2_set_temp(float v) { floats["ref2_set_temp"] = v; }
  void publish_wine_set_temp(float v) { floats["wine_set_temp"] = v; }
  void publish_wine2_set_temp(float v) { floats["wine2_set_temp"] = v; }
  void publish_crisp_set_temp(float v) { floats["crisp_set_temp"] = v; }
  void publish_air_filter_pct(float v) { floats["air_filter_pct"] = v; }
  void publish_water_filter_pct(float v) { floats["water_filter_pct"] = v; }
};

struct DishwasherRecorder : CommonRecorder {
  bool clear_wash_time_remaining_called = false;

  void publish_door_ajar(bool v) { bools["door_ajar"] = v; }
  void publish_wash_cycle_on(bool v) { bools["wash_cycle_on"] = v; }
  void publish_heated_dry(bool v) { bools["heated_dry"] = v; }
  void publish_extended_dry(bool v) { bools["extended_dry"] = v; }
  void publish_high_temp_wash(bool v) { bools["high_temp_wash"] = v; }
  void publish_sani_rinse(bool v) { bools["sani_rinse"] = v; }
  void publish_rinse_aid_low(bool v) { bools["rinse_aid_low"] = v; }
  void publish_softener_low(bool v) { bools["softener_low"] = v; }
  void publish_light_on(bool v) { bools["light_on"] = v; }
  void publish_remote_ready(bool v) { bools["remote_ready"] = v; }
  void publish_delay_start(bool v) { bools["delay_start"] = v; }
  void publish_wash_status(int v) { ints["wash_status"] = v; }
  void publish_wash_cycle(int v) { ints["wash_cycle"] = v; }
  void publish_wash_time_remaining(int v) { ints["wash_time_remaining"] = v; }
  void publish_wash_cycle_end_time(const std::string &v) {
    strings["wash_cycle_end_time"] = v;
  }

  void clear_wash_time_remaining_if_running() {
    clear_wash_time_remaining_called = true;
  }
};

struct RangeRecorder : CommonRecorder {
  void publish_door_ajar(bool v) { bools["door_ajar"] = v; }
  void publish_cav_unit_on(bool v) { bools["cav_unit_on"] = v; }
  void publish_cav_at_set_temp(bool v) { bools["cav_at_set_temp"] = v; }
  void publish_cav_light_on(bool v) { bools["cav_light_on"] = v; }
  void publish_cav_remote_ready(bool v) { bools["cav_remote_ready"] = v; }
  void publish_cav_probe_on(bool v) { bools["cav_probe_on"] = v; }
  void publish_cav_probe_at_temp(bool v) { bools["cav_probe_at_temp"] = v; }
  void publish_cav_probe_near(bool v) { bools["cav_probe_near"] = v; }
  void publish_cav_gourmet(bool v) { bools["cav_gourmet"] = v; }
  void publish_cook_timer_done(bool v) { bools["cook_timer_done"] = v; }
  void publish_cook_timer_near(bool v) { bools["cook_timer_near"] = v; }

  void publish_cav_temp(float v) { floats["cav_temp"] = v; }
  void publish_cav_set_temp(float v) { floats["cav_set_temp"] = v; }
  void publish_cav_cook_mode(int v) { ints["cav_cook_mode"] = v; }
  void publish_cav_gourmet_recipe(int v) { ints["cav_gourmet_recipe"] = v; }
  void publish_probe_temp(float v) { floats["probe_temp"] = v; }
  void publish_probe_set_temp(float v) { floats["probe_set_temp"] = v; }

  void publish_ktimer_active(bool v) { bools["ktimer_active"] = v; }
  void publish_ktimer_done(bool v) { bools["ktimer_done"] = v; }
  void publish_ktimer_near(bool v) { bools["ktimer_near"] = v; }
  void publish_ktimer2_active(bool v) { bools["ktimer2_active"] = v; }
  void publish_ktimer2_done(bool v) { bools["ktimer2_done"] = v; }
  void publish_ktimer2_near(bool v) { bools["ktimer2_near"] = v; }
  void publish_ktimer_end_time(const std::string &v) {
    strings["ktimer_end_time"] = v;
  }
  void publish_ktimer2_end_time(const std::string &v) {
    strings["ktimer2_end_time"] = v;
  }

  void publish_cav2_unit_on(bool v) { bools["cav2_unit_on"] = v; }
  void publish_cav2_door_ajar(bool v) { bools["cav2_door_ajar"] = v; }
  void publish_cav2_at_set_temp(bool v) { bools["cav2_at_set_temp"] = v; }
  void publish_cav2_light_on(bool v) { bools["cav2_light_on"] = v; }
  void publish_cav2_remote_ready(bool v) { bools["cav2_remote_ready"] = v; }
  void publish_cav2_probe_on(bool v) { bools["cav2_probe_on"] = v; }
  void publish_cav2_probe_at_temp(bool v) { bools["cav2_probe_at_temp"] = v; }
  void publish_cav2_probe_near(bool v) { bools["cav2_probe_near"] = v; }
  void publish_cav2_gourmet(bool v) { bools["cav2_gourmet"] = v; }
  void publish_cav2_cook_timer_done(bool v) {
    bools["cav2_cook_timer_done"] = v;
  }

  void publish_cav2_temp(float v) { floats["cav2_temp"] = v; }
  void publish_cav2_set_temp(float v) { floats["cav2_set_temp"] = v; }
  void publish_cav2_cook_mode(int v) { ints["cav2_cook_mode"] = v; }
  void publish_cav2_probe_temp(float v) { floats["cav2_probe_temp"] = v; }
  void publish_cav2_probe_set_temp(float v) {
    floats["cav2_probe_set_temp"] = v;
  }
};

std::string read_file(const fs::path &p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

// =============================================================================
// Empty-state behavior — no fields → no calls. Catches dispatchers that
// publish a default value (e.g. zero) when the underlying optional is unset.
// =============================================================================

TEST(Dispatch, FridgeEmptyStateNoCalls) {
  FridgeState s; // all optionals nullopt
  FridgeRecorder rec;
  dispatch_fridge(s, rec);
  EXPECT_TRUE(rec.bools.empty());
  EXPECT_TRUE(rec.floats.empty());
  EXPECT_TRUE(rec.strings.empty());
  EXPECT_TRUE(rec.ints.empty());
}

TEST(Dispatch, DishwasherEmptyStateNoCalls) {
  DishwasherState s;
  DishwasherRecorder rec;
  dispatch_dishwasher(s, rec);
  EXPECT_TRUE(rec.bools.empty());
  EXPECT_TRUE(rec.ints.empty());
  EXPECT_TRUE(rec.strings.empty());
  EXPECT_FALSE(rec.clear_wash_time_remaining_called);
}

TEST(Dispatch, RangeEmptyStateNoCalls) {
  RangeState s;
  RangeRecorder rec;
  dispatch_range(s, rec);
  EXPECT_TRUE(rec.bools.empty());
  EXPECT_TRUE(rec.floats.empty());
  EXPECT_TRUE(rec.ints.empty());
  EXPECT_TRUE(rec.strings.empty());
}

// =============================================================================
// CommonFields dispatch — every CommonFields field must reach its bus method.
// One test per field; each catches "added a new common field but forgot to
// wire it up in dispatch_common".
// =============================================================================

TEST(Dispatch, CommonFieldsAllRouted) {
  FridgeState s;
  s.common.sabbath_on = true;
  s.common.service_required = false;
  s.common.appliance_model = std::string("BI36UFDID");
  s.common.uptime = std::string("1d2h");
  s.common.appliance_serial = std::string("0123456789");
  s.common.appliance_type = std::string("fridge");
  s.common.diagnostic_status = std::string("ok");
  s.common.build_date = std::string("2025-01-01");
  s.common.version.fw = std::string("2.27");
  s.common.version.api = std::string("4.0");
  s.common.version.bleapp = std::string("1.5");
  s.common.version.os = std::string("Linux");
  s.common.version.rtapp = std::string("3.1");
  s.common.version.appliance = std::string("BoardA");

  FridgeRecorder rec;
  dispatch_fridge(s, rec);

  EXPECT_EQ(rec.bools["sabbath_on"], true);
  EXPECT_EQ(rec.bools["svc_required"], false);
  EXPECT_EQ(rec.strings["model"], "BI36UFDID");
  EXPECT_EQ(rec.strings["uptime"], "1d2h");
  EXPECT_EQ(rec.strings["serial"], "0123456789");
  EXPECT_EQ(rec.strings["appliance_type"], "fridge");
  EXPECT_EQ(rec.strings["diag_status"], "ok");
  EXPECT_EQ(rec.strings["build_date"], "2025-01-01");
  EXPECT_EQ(rec.strings["fw_version"], "2.27");
  EXPECT_EQ(rec.strings["api_version"], "4.0");
  EXPECT_EQ(rec.strings["bleapp_version"], "1.5");
  EXPECT_EQ(rec.strings["os_version"], "Linux");
  EXPECT_EQ(rec.strings["rtapp_version"], "3.1");
  EXPECT_EQ(rec.strings["board_version"], "BoardA");
}

// =============================================================================
// Fridge-specific field-to-method mapping. One assertion per field.
// =============================================================================

TEST(Dispatch, FridgeFieldsRouted) {
  FridgeState s;
  s.ref_set_temp = 38.0f;
  s.door_ajar = true;
  s.frz_set_temp = -2.0f;
  s.frz_door_ajar = false;
  s.ice_maker_on = true;
  s.ref2_set_temp = 40.0f;
  s.ref2_door_ajar = true;
  s.wine_door_ajar = false;
  s.wine_set_temp = 55.0f;
  s.wine2_set_temp = 65.0f;
  s.wine_temp_alert_on = false;
  s.crisp_set_temp = 33.0f;
  s.air_filter_on = true;
  s.air_filter_pct_remaining = 80.0f;
  s.water_filter_pct_remaining = 50.0f;

  FridgeRecorder rec;
  dispatch_fridge(s, rec);

  EXPECT_FLOAT_EQ(rec.floats["set_temp"], 38.0f);
  EXPECT_EQ(rec.bools["door_ajar"], true);
  EXPECT_FLOAT_EQ(rec.floats["frz_set_temp"], -2.0f);
  EXPECT_EQ(rec.bools["frz_door_ajar"], false);
  EXPECT_EQ(rec.bools["ice_maker"], true);
  EXPECT_FLOAT_EQ(rec.floats["ref2_set_temp"], 40.0f);
  EXPECT_EQ(rec.bools["ref2_door_ajar"], true);
  EXPECT_EQ(rec.bools["wine_door_ajar"], false);
  EXPECT_FLOAT_EQ(rec.floats["wine_set_temp"], 55.0f);
  EXPECT_FLOAT_EQ(rec.floats["wine2_set_temp"], 65.0f);
  EXPECT_EQ(rec.bools["wine_temp_alert"], false);
  EXPECT_FLOAT_EQ(rec.floats["crisp_set_temp"], 33.0f);
  EXPECT_EQ(rec.bools["air_filter_on"], true);
  EXPECT_FLOAT_EQ(rec.floats["air_filter_pct"], 80.0f);
  EXPECT_FLOAT_EQ(rec.floats["water_filter_pct"], 50.0f);
}

// Some fridges (e.g. PRO3650G) wire the main door and the refrigerator drawer
// to a single switch and only report ref_door_ajar. The dispatcher mirrors
// the main-door state to the drawer sensor when ref2_door_ajar is absent,
// so users who enable hide_ref_drawer=false still get a populated drawer
// door entity.
TEST(Dispatch, FridgeRef2DoorMirrorsRefDoorWhenAbsent) {
  FridgeState s;
  s.door_ajar = true; // populated from ref_door_ajar at parse time
  // s.ref2_door_ajar intentionally unset
  FridgeRecorder rec;
  dispatch_fridge(s, rec);
  EXPECT_EQ(rec.bools["door_ajar"], true);
  EXPECT_EQ(rec.bools["ref2_door_ajar"], true);
}

TEST(Dispatch, FridgeRef2DoorExplicitWinsOverFallback) {
  FridgeState s;
  s.door_ajar = true;
  s.ref2_door_ajar = false; // explicit drawer signal disagrees with main door
  FridgeRecorder rec;
  dispatch_fridge(s, rec);
  EXPECT_EQ(rec.bools["door_ajar"], true);
  EXPECT_EQ(rec.bools["ref2_door_ajar"], false);
}

// =============================================================================
// Dishwasher dispatch: includes the stateful "clear remaining time when
// cycle ends" hook which is special-cased in dispatch_dishwasher.
// =============================================================================

TEST(Dispatch, DishwasherFieldsRouted) {
  DishwasherState s;
  s.door_ajar = false;
  s.wash_cycle_on = true;
  s.heated_dry_on = true;
  s.extended_dry_on = false;
  s.high_temp_wash_on = true;
  s.sani_rinse_on = false;
  s.rinse_aid_low = true;
  s.softener_low = false;
  s.light_on = true;
  s.remote_ready = false;
  s.delay_start_timer_active = false;
  s.wash_status = 7;
  s.wash_cycle = 3;
  s.wash_time_remaining_min = 42;
  s.wash_cycle_end_time = std::string("2026-04-25T08:42");

  DishwasherRecorder rec;
  dispatch_dishwasher(s, rec);

  EXPECT_EQ(rec.bools["door_ajar"], false);
  EXPECT_EQ(rec.bools["wash_cycle_on"], true);
  EXPECT_EQ(rec.bools["heated_dry"], true);
  EXPECT_EQ(rec.bools["extended_dry"], false);
  EXPECT_EQ(rec.bools["high_temp_wash"], true);
  EXPECT_EQ(rec.bools["sani_rinse"], false);
  EXPECT_EQ(rec.bools["rinse_aid_low"], true);
  EXPECT_EQ(rec.bools["softener_low"], false);
  EXPECT_EQ(rec.bools["light_on"], true);
  EXPECT_EQ(rec.bools["remote_ready"], false);
  EXPECT_EQ(rec.bools["delay_start"], false);
  EXPECT_EQ(rec.ints["wash_status"], 7);
  EXPECT_EQ(rec.ints["wash_cycle"], 3);
  EXPECT_EQ(rec.ints["wash_time_remaining"], 42);
  EXPECT_EQ(rec.strings["wash_cycle_end_time"], "2026-04-25T08:42");
  // wash_cycle_on was true, so the clear-on-stop hook is NOT triggered
  EXPECT_FALSE(rec.clear_wash_time_remaining_called);
}

TEST(Dispatch, DishwasherCycleEndsTriggersClear) {
  DishwasherState s;
  s.wash_cycle_on = false;
  DishwasherRecorder rec;
  dispatch_dishwasher(s, rec);
  EXPECT_EQ(rec.bools["wash_cycle_on"], false);
  EXPECT_TRUE(rec.clear_wash_time_remaining_called);
}

TEST(Dispatch, DishwasherClearOnlyWhenCycleOnFlipsFalse) {
  // Absent wash_cycle_on must NOT trigger the clear (push notification
  // about an unrelated field shouldn't reset wash_time_remaining).
  DishwasherState s;
  s.heated_dry_on = true;
  DishwasherRecorder rec;
  dispatch_dishwasher(s, rec);
  EXPECT_FALSE(rec.clear_wash_time_remaining_called);
}

// =============================================================================
// Range dispatch — covers primary cavity, kitchen timers, secondary cavity.
// =============================================================================

TEST(Dispatch, RangeFieldsRouted_PrimaryCavity) {
  RangeState s;
  s.door_ajar = false;
  s.cav_unit_on = true;
  s.cav_at_set_temp = false;
  s.cav_light_on = true;
  s.cav_remote_ready = false;
  s.cav_probe_on = true;
  s.cav_probe_at_set_temp = false;
  s.cav_probe_within_10deg = true;
  s.cav_gourmet_mode_on = false;
  s.cav_gourmet_recipe = 12;
  s.cav_cook_timer_complete = false;
  s.cav_cook_timer_within_1min = true;
  s.cav_temp = 350.0f;
  s.cav_set_temp = 375.0f;
  s.cav_cook_mode = 5;
  s.cav_probe_temp = 165.0f;
  s.cav_probe_set_temp = 180.0f;

  RangeRecorder rec;
  dispatch_range(s, rec);

  EXPECT_EQ(rec.bools["door_ajar"], false);
  EXPECT_EQ(rec.bools["cav_unit_on"], true);
  EXPECT_EQ(rec.bools["cav_at_set_temp"], false);
  EXPECT_EQ(rec.bools["cav_light_on"], true);
  EXPECT_EQ(rec.bools["cav_remote_ready"], false);
  EXPECT_EQ(rec.bools["cav_probe_on"], true);
  EXPECT_EQ(rec.bools["cav_probe_at_temp"], false);
  EXPECT_EQ(rec.bools["cav_probe_near"], true);
  EXPECT_EQ(rec.bools["cav_gourmet"], false);
  EXPECT_EQ(rec.ints["cav_gourmet_recipe"], 12);
  EXPECT_EQ(rec.bools["cook_timer_done"], false);
  EXPECT_EQ(rec.bools["cook_timer_near"], true);
  EXPECT_FLOAT_EQ(rec.floats["cav_temp"], 350.0f);
  EXPECT_FLOAT_EQ(rec.floats["cav_set_temp"], 375.0f);
  EXPECT_EQ(rec.ints["cav_cook_mode"], 5);
  EXPECT_FLOAT_EQ(rec.floats["probe_temp"], 165.0f);
  EXPECT_FLOAT_EQ(rec.floats["probe_set_temp"], 180.0f);
}

TEST(Dispatch, RangeFieldsRouted_KitchenTimers) {
  RangeState s;
  s.kitchen_timer_active = true;
  s.kitchen_timer_complete = false;
  s.kitchen_timer_within_1min = true;
  s.kitchen_timer_end_time = std::string("2026-04-25T07:30");
  s.kitchen_timer2_active = false;
  s.kitchen_timer2_complete = true;
  s.kitchen_timer2_within_1min = false;
  s.kitchen_timer2_end_time = std::string("2026-04-25T08:00");

  RangeRecorder rec;
  dispatch_range(s, rec);

  EXPECT_EQ(rec.bools["ktimer_active"], true);
  EXPECT_EQ(rec.bools["ktimer_done"], false);
  EXPECT_EQ(rec.bools["ktimer_near"], true);
  EXPECT_EQ(rec.strings["ktimer_end_time"], "2026-04-25T07:30");
  EXPECT_EQ(rec.bools["ktimer2_active"], false);
  EXPECT_EQ(rec.bools["ktimer2_done"], true);
  EXPECT_EQ(rec.bools["ktimer2_near"], false);
  EXPECT_EQ(rec.strings["ktimer2_end_time"], "2026-04-25T08:00");
}

TEST(Dispatch, RangeFieldsRouted_SecondaryCavity) {
  RangeState s;
  s.cav2_unit_on = true;
  s.cav2_door_ajar = false;
  s.cav2_at_set_temp = true;
  s.cav2_light_on = false;
  s.cav2_remote_ready = true;
  s.cav2_probe_on = false;
  s.cav2_probe_at_set_temp = true;
  s.cav2_probe_within_10deg = false;
  s.cav2_gourmet_mode_on = true;
  s.cav2_cook_timer_complete = false;
  s.cav2_temp = 300.0f;
  s.cav2_set_temp = 325.0f;
  s.cav2_cook_mode = 2;
  s.cav2_probe_temp = 140.0f;
  s.cav2_probe_set_temp = 155.0f;

  RangeRecorder rec;
  dispatch_range(s, rec);

  EXPECT_EQ(rec.bools["cav2_unit_on"], true);
  EXPECT_EQ(rec.bools["cav2_door_ajar"], false);
  EXPECT_EQ(rec.bools["cav2_at_set_temp"], true);
  EXPECT_EQ(rec.bools["cav2_light_on"], false);
  EXPECT_EQ(rec.bools["cav2_remote_ready"], true);
  EXPECT_EQ(rec.bools["cav2_probe_on"], false);
  EXPECT_EQ(rec.bools["cav2_probe_at_temp"], true);
  EXPECT_EQ(rec.bools["cav2_probe_near"], false);
  EXPECT_EQ(rec.bools["cav2_gourmet"], true);
  EXPECT_EQ(rec.bools["cav2_cook_timer_done"], false);
  EXPECT_FLOAT_EQ(rec.floats["cav2_temp"], 300.0f);
  EXPECT_FLOAT_EQ(rec.floats["cav2_set_temp"], 325.0f);
  EXPECT_EQ(rec.ints["cav2_cook_mode"], 2);
  EXPECT_FLOAT_EQ(rec.floats["cav2_probe_temp"], 140.0f);
  EXPECT_FLOAT_EQ(rec.floats["cav2_probe_set_temp"], 155.0f);
}

// =============================================================================
// End-to-end fixture replay — parses real captured payloads, dispatches,
// and asserts a few high-signal sensor publishes match. Catches "parser
// produced expected struct, but dispatch didn't wire X to Y."
// =============================================================================

TEST(Dispatch, FixtureFridgeFullPoll) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "fridge_back_2028_d5_full.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_fridge(raw);
  ASSERT_TRUE(s.valid);
  ASSERT_TRUE(s.is_poll);

  FridgeRecorder rec;
  dispatch_fridge(s, rec);

  // Sample of fields we know are present in the fixture (verified from
  // tests/fixtures/fridge_back_2028_d5_full.expected.json):
  EXPECT_FALSE(rec.strings["model"].empty());
  EXPECT_FALSE(rec.strings["serial"].empty());
  EXPECT_FALSE(rec.strings["fw_version"].empty());
  // The fridge has a setpoint and door state.
  EXPECT_NE(rec.floats.find("set_temp"), rec.floats.end());
  EXPECT_NE(rec.bools.find("door_ajar"), rec.bools.end());
}

TEST(Dispatch, FixtureFridgePushDoor) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "fridge_push_ref_door_true.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_fridge(raw);
  ASSERT_TRUE(s.valid);
  EXPECT_FALSE(s.is_poll); // push notification

  FridgeRecorder rec;
  dispatch_fridge(s, rec);

  // Push for a door event should publish the door state and not much else.
  ASSERT_NE(rec.bools.find("door_ajar"), rec.bools.end());
  EXPECT_EQ(rec.bools["door_ajar"], true);
  // No firmware version in a push notification.
  EXPECT_TRUE(rec.strings["fw_version"].empty());
  // Drawer sensor mirrors the main door for shared-switch models.
  ASSERT_NE(rec.bools.find("ref2_door_ajar"), rec.bools.end());
  EXPECT_EQ(rec.bools["ref2_door_ajar"], true);
}

// PRO3650G has a separate refrigerator drawer with its own setpoint
// (`ref2_set_temp`) but no separate `ref2_door_ajar` — the main door switch
// covers the drawer too. Verify dispatch produces independent `set_temp` and
// `ref2_set_temp` publishes, while `ref2_door_ajar` mirrors the main door.
TEST(Dispatch, FixturePro3650gSeparateDrawerSetTempSharedDoor) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "fridge_pro3650g_d4_full.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_fridge(raw);
  ASSERT_TRUE(s.valid);
  ASSERT_TRUE(s.is_poll);
  // Parser keeps state pristine: drawer door is unset because the JSON
  // doesn't carry ref2_door_ajar.
  EXPECT_TRUE(s.ref_set_temp.has_value());
  EXPECT_TRUE(s.ref2_set_temp.has_value());
  EXPECT_FALSE(s.ref2_door_ajar.has_value());

  FridgeRecorder rec;
  dispatch_fridge(s, rec);

  // Main fridge and drawer setpoints publish independently.
  ASSERT_NE(rec.floats.find("set_temp"), rec.floats.end());
  ASSERT_NE(rec.floats.find("ref2_set_temp"), rec.floats.end());
  EXPECT_FLOAT_EQ(rec.floats["set_temp"], 36.0f);
  EXPECT_FLOAT_EQ(rec.floats["ref2_set_temp"], 36.0f);
  // Main door state mirrors to the drawer sensor at dispatch time.
  ASSERT_NE(rec.bools.find("door_ajar"), rec.bools.end());
  ASSERT_NE(rec.bools.find("ref2_door_ajar"), rec.bools.end());
  EXPECT_EQ(rec.bools["door_ajar"], false);
  EXPECT_EQ(rec.bools["ref2_door_ajar"], false);
  // Freezer remains independent (drawer-style freezer with its own switch).
  EXPECT_EQ(rec.bools["frz_door_ajar"], true);
  EXPECT_FLOAT_EQ(rec.floats["frz_set_temp"], -1.0f);
}

TEST(Dispatch, FixtureDishwasherFullPoll) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "dishwasher_dw2450_d5_full.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_dishwasher(raw);
  ASSERT_TRUE(s.valid);

  DishwasherRecorder rec;
  dispatch_dishwasher(s, rec);

  EXPECT_FALSE(rec.strings["model"].empty());
  // The fixture should populate at least one wash-related field.
  bool has_wash_field = rec.bools.count("wash_cycle_on") > 0 ||
                        rec.ints.count("wash_status") > 0 ||
                        rec.ints.count("wash_cycle") > 0;
  EXPECT_TRUE(has_wash_field);
}

TEST(Dispatch, FixtureRangeFullPoll) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "range_df36450_d5_full.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_range(raw);
  ASSERT_TRUE(s.valid);

  RangeRecorder rec;
  dispatch_range(s, rec);

  EXPECT_FALSE(rec.strings["model"].empty());
  EXPECT_FALSE(rec.strings["fw_version"].empty());
}

TEST(Dispatch, FixtureRangePushLightOn) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "range_push_light_on.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_range(raw);
  ASSERT_TRUE(s.valid);
  EXPECT_FALSE(s.is_poll);

  RangeRecorder rec;
  dispatch_range(s, rec);

  ASSERT_NE(rec.bools.find("cav_light_on"), rec.bools.end());
  EXPECT_EQ(rec.bools["cav_light_on"], true);
}

TEST(Dispatch, FixtureWallovenPushSetTemp) {
  std::string raw =
      read_file(fs::path(FIXTURES_DIR) / "walloven_push_set_temp.json");
  ASSERT_FALSE(raw.empty());
  auto s = parse_range(raw);
  ASSERT_TRUE(s.valid);

  RangeRecorder rec;
  dispatch_range(s, rec);

  ASSERT_NE(rec.floats.find("cav_set_temp"), rec.floats.end());
}

TEST(Dispatch, InvalidParseProducesNoCalls) {
  // dispatch_* assumes valid==true; the YAML caller already guards. But
  // verify dispatching a struct with all-nullopt optionals (the shape of
  // an "invalid" parse result) doesn't accidentally call anything.
  FridgeState s;
  s.valid = false; // dispatch doesn't check this — fields are still empty
  FridgeRecorder rec;
  dispatch_fridge(s, rec);
  EXPECT_TRUE(rec.bools.empty());
  EXPECT_TRUE(rec.floats.empty());
  EXPECT_TRUE(rec.strings.empty());
}
