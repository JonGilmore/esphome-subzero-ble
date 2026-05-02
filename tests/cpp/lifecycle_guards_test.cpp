#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const std::string &path) {
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "Could not open " << path;
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string extract_case_body(const std::string &src,
                              const std::string &case_label) {
  std::string needle = "case " + case_label + ":";
  auto start = src.find(needle);
  if (start == std::string::npos)
    return "";
  start += needle.size();
  std::size_t next_case = src.find("\n  case ", start);
  std::size_t next_default = src.find("\n  default:", start);
  std::size_t stop = std::min(next_case, next_default);
  if (stop == std::string::npos)
    return src.substr(start);
  return src.substr(start, stop - start);
}

} // namespace

TEST(LifecycleGuards, HubCppGatesLogsOnUseEsp32) {
  std::string src = read_file(std::string(APPLIANCE_DIR) + "/hub.cpp");

  // Must use USE_ESP32 — the on-device sentinel. ESPHome's build
  // defines this; the host gtest build does not.
  EXPECT_NE(src.find("#ifdef USE_ESP32"), std::string::npos)
      << "hub.cpp must gate the on-device log macros on USE_ESP32 — see "
         "tests/cpp/lifecycle_guards_test.cpp";

  EXPECT_EQ(src.find("#ifdef ESPHOME_LOG_HAS_INFO"), std::string::npos)
      << "hub.cpp must NOT gate on ESPHOME_LOG_HAS_INFO — that symbol "
         "is defined inside esphome/core/log.h and the #ifdef would "
         "silently turn HUB_LOGI/W/E/D into no-ops on device. Use "
         "USE_ESP32 instead.";
}

TEST(LifecycleGuards, ApplianceBaseRoutesConnectFromSearchCmpl) {
  std::string src =
      read_file(std::string(APPLIANCE_DIR) + "/appliance_base.cpp");

  std::string search_cmpl_body =
      extract_case_body(src, "ESP_GATTC_SEARCH_CMPL_EVT");
  ASSERT_FALSE(search_cmpl_body.empty())
      << "appliance_base.cpp must have a `case ESP_GATTC_SEARCH_CMPL_EVT:` "
         "arm in gattc_event_handler";
  EXPECT_NE(search_cmpl_body.find("handle_connected()"), std::string::npos)
      << "appliance_base.cpp's ESP_GATTC_SEARCH_CMPL_EVT arm must call "
         "h->handle_connected() — that's when subscribe is safe to fire.";
}

TEST(LifecycleGuards, ApplianceBaseDoesNotRouteConnectFromOpenEvt) {
  std::string src =
      read_file(std::string(APPLIANCE_DIR) + "/appliance_base.cpp");

  std::string open_body = extract_case_body(src, "ESP_GATTC_OPEN_EVT");
  ASSERT_FALSE(open_body.empty())
      << "appliance_base.cpp must have a `case ESP_GATTC_OPEN_EVT:` arm";
  EXPECT_EQ(open_body.find("handle_connected()"), std::string::npos)
      << "appliance_base.cpp's ESP_GATTC_OPEN_EVT arm must NOT call "
         "h->handle_connected(). The GATT cache isn't populated until "
         "SEARCH_CMPL — calling subscribe earlier silently no-ops the "
         "register_for_notify / CCCD writes and the appliance never "
         "sends data back. Hook handle_connected() to "
         "ESP_GATTC_SEARCH_CMPL_EVT instead.";
}
