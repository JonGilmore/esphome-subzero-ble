#include "commands.h"

#include <gtest/gtest.h>

#include <string>

using esphome::subzero_protocol::build_display_pin;
using esphome::subzero_protocol::build_get;
using esphome::subzero_protocol::build_get_async;
using esphome::subzero_protocol::build_poll_command;
using esphome::subzero_protocol::build_set;
using esphome::subzero_protocol::build_set_bool;
using esphome::subzero_protocol::build_set_int;
using esphome::subzero_protocol::build_set_string;
using esphome::subzero_protocol::build_unlock_channel;
using esphome::subzero_protocol::has_status_value;
using esphome::subzero_protocol::is_lacking_properties_response;
using esphome::subzero_protocol::PollVerb;

TEST(Commands, GetAsyncIsExactWireFormat) {
  EXPECT_EQ(build_get_async(), "{\"cmd\":\"get_async\"}\n");
}

TEST(Commands, GetAsyncTerminatesWithNewline) {
  // The appliance's serial parser only emits a response after \n.
  // Drop this test only if the protocol changes.
  EXPECT_EQ(build_get_async().back(), '\n');
}

TEST(Commands, UnlockChannelInterpolatesPin) {
  EXPECT_EQ(build_unlock_channel("12345"),
            "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"12345\"}}\n");
}

TEST(Commands, UnlockChannelHandlesEmptyPin) {
  // The YAML caller is supposed to guard against this (and does), but
  // we should produce well-formed JSON regardless rather than corrupting
  // the wire stream.
  EXPECT_EQ(build_unlock_channel(""),
            "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"\"}}\n");
}

TEST(Commands, UnlockChannelTerminatesWithNewline) {
  EXPECT_EQ(build_unlock_channel("99999").back(), '\n');
}

// Regression for CodeRabbit on PR #73: PINs must be JSON-escaped so a
// hostile or accidental input can't break the wire format.
TEST(Commands, UnlockChannelEscapesQuoteInPin) {
  // Real PINs are 4-5 digit numeric, but the HA text input is mode:text
  // and accepts arbitrary strings. A literal " in the PIN must not
  // terminate the JSON value.
  EXPECT_EQ(build_unlock_channel("ab\"cd"),
            "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"ab\\\"cd\"}}\n");
}

TEST(Commands, UnlockChannelEscapesBackslashInPin) {
  EXPECT_EQ(build_unlock_channel("ab\\cd"),
            "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"ab\\\\cd\"}}\n");
}

TEST(Commands, UnlockChannelEscapesNamedControlChars) {
  std::string pin = "a\b\f\n\r\tb";
  EXPECT_EQ(build_unlock_channel(pin), "{\"cmd\":\"unlock_channel\",\"params\":"
                                       "{\"pin\":\"a\\b\\f\\n\\r\\tb\"}}\n");
}

TEST(Commands, UnlockChannelEscapesOtherControlCharsAsUnicode) {
  // 0x01 (SOH) and 0x1F (US) have no named escape; they become 
  // and . Anything else under 0x20 takes the same path.
  std::string pin;
  pin.push_back(0x01);
  pin.push_back(0x1F);
  EXPECT_EQ(
      build_unlock_channel(pin),
      "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"\\u0001\\u001F\"}}\n");
}

TEST(Commands, UnlockChannelPassesNumericPinThrough) {
  // The common case must not be regressed by the escape pass.
  EXPECT_EQ(build_unlock_channel("12345"),
            "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"12345\"}}\n");
}

TEST(Commands, UnlockChannelPassesHighBitBytesThrough) {
  // Allowing 0x80+ bytes through unmodified — the protocol is ASCII in
  // practice and we don't want to re-encode arbitrary UTF-8 sequences.
  // (HA's text input would normally reject these but we don't enforce.)
  std::string pin;
  pin.push_back(static_cast<char>(0xC3));
  pin.push_back(static_cast<char>(0xA9)); // é in UTF-8
  EXPECT_EQ(build_unlock_channel(pin),
            std::string("{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"") +
                pin + "\"}}\n");
}

TEST(Commands, DisplayPinDefaultDuration) {
  EXPECT_EQ(build_display_pin(),
            "{\"cmd\":\"display_pin\",\"params\":{\"duration\": 30}}\n");
}

TEST(Commands, DisplayPinCustomDuration) {
  EXPECT_EQ(build_display_pin(5),
            "{\"cmd\":\"display_pin\",\"params\":{\"duration\": 5}}\n");
  EXPECT_EQ(build_display_pin(120),
            "{\"cmd\":\"display_pin\",\"params\":{\"duration\": 120}}\n");
}

TEST(Commands, DisplayPinTerminatesWithNewline) {
  EXPECT_EQ(build_display_pin().back(), '\n');
  EXPECT_EQ(build_display_pin(60).back(), '\n');
}

TEST(Commands, SetBoolTrueAndFalse) {
  EXPECT_EQ(build_set_bool("cav_light_on", true),
            "{\"cmd\":\"set\",\"params\":{\"cav_light_on\":true}}\n");
  EXPECT_EQ(build_set_bool("cav_light_on", false),
            "{\"cmd\":\"set\",\"params\":{\"cav_light_on\":false}}\n");
}

TEST(Commands, SetIntPositive) {
  EXPECT_EQ(build_set_int("set_temp", 38),
            "{\"cmd\":\"set\",\"params\":{\"set_temp\":38}}\n");
}

TEST(Commands, SetIntZeroCancelsTimer) {
  // Setting kitchen_timer_duration:0 cancels a running timer per HCI snoop.
  EXPECT_EQ(build_set_int("kitchen_timer_duration", 0),
            "{\"cmd\":\"set\",\"params\":{\"kitchen_timer_duration\":0}}\n");
}

TEST(Commands, SetIntNegative) {
  // Freezer set_temp can be negative on Fahrenheit appliances.
  EXPECT_EQ(build_set_int("frz_set_temp", -5),
            "{\"cmd\":\"set\",\"params\":{\"frz_set_temp\":-5}}\n");
}

TEST(Commands, SetStringEscapesQuotes) {
  // Keep the wire format well-formed even with hostile/odd inputs - same
  // discipline as build_unlock_channel for PINs.
  EXPECT_EQ(build_set_string("ap_ssid", "my\"ssid"),
            "{\"cmd\":\"set\",\"params\":{\"ap_ssid\":\"my\\\"ssid\"}}\n");
}

TEST(Commands, SetStringEmptyClearsCloudToken) {
  // The "BT-only mode" diagnostic action — clearing remote_svc_reg_token
  // deregisters the appliance from Azure IoT Hub.
  EXPECT_EQ(build_set_string("remote_svc_reg_token", ""),
            "{\"cmd\":\"set\",\"params\":{\"remote_svc_reg_token\":\"\"}}\n");
}

TEST(Commands, SetGenericAcceptsRawJsonValue) {
  // For the rare composite payloads (e.g. WiFi config sends three keys
  // in one call). Generic build_set passes the value through verbatim.
  EXPECT_EQ(build_set("foo", "[1,2,3]"),
            "{\"cmd\":\"set\",\"params\":{\"foo\":[1,2,3]}}\n");
}

TEST(Commands, SetTerminatesWithNewline) {
  EXPECT_EQ(build_set_bool("cav_light_on", true).back(), '\n');
  EXPECT_EQ(build_set_int("set_temp", 38).back(), '\n');
  EXPECT_EQ(build_set_string("ap_ssid", "iot").back(), '\n');
}

TEST(Commands, SetEscapesKeyName) {
  EXPECT_EQ(build_set_int("a\"b", 1),
            "{\"cmd\":\"set\",\"params\":{\"a\\\"b\":1}}\n");
}

TEST(Commands, BuildGetProducesCanonicalLiteral) {
  EXPECT_EQ(build_get(), "{\"cmd\":\"get\"}\n");
  EXPECT_EQ(build_get().back(), '\n');
}

TEST(Commands, BuildPollCommandSelectsByVerb) {
  EXPECT_EQ(build_poll_command(PollVerb::kGetAsync), build_get_async());
  EXPECT_EQ(build_poll_command(PollVerb::kGet), build_get());
}

TEST(Commands, IsLackingProperties_ExactSentinel) {
  EXPECT_TRUE(is_lacking_properties_response(
      "{\"status\":1,\"resp\":{},\"status_msg\":\"An error occurred\"}\n"));
}

TEST(Commands, IsLackingProperties_AcceptsWhitespaceVariant) {
  // Some firmwares may emit a space after the colon — both must trip.
  EXPECT_TRUE(is_lacking_properties_response("{\"status\":1,\"resp\": {}}"));
}

TEST(Commands, IsLackingProperties_RejectsStatusZero) {
  // Successful response — has data in resp, status:0. Must NOT trip.
  EXPECT_FALSE(is_lacking_properties_response(
      "{\"status\":0,\"resp\":{\"ref_set_temp\":38}}"));
}

TEST(Commands, IsLackingProperties_RejectsStatusNonzeroWithData) {
  EXPECT_FALSE(is_lacking_properties_response(
      "{\"status\":1,\"resp\":{\"ref_set_temp\":38}}"));
}

TEST(Commands, IsLackingProperties_RejectsStatus302) {
  EXPECT_FALSE(is_lacking_properties_response("{\"status\":302,\"resp\":{}}"));
}

TEST(Commands, IsLackingProperties_RejectsHealthyPushNotification) {
  EXPECT_FALSE(is_lacking_properties_response(
      "{\"msg_types\":2,\"seq\":92,\"props\":{\"ref_door_ajar\":true}}"));
}

TEST(Commands, IsLackingProperties_RejectsStatusWithDigitSuffix) {
  EXPECT_FALSE(is_lacking_properties_response("{\"status\":10,\"resp\":{}}"));
  EXPECT_FALSE(is_lacking_properties_response("{\"status\":11,\"resp\":{}}"));
  EXPECT_FALSE(is_lacking_properties_response("{\"status\":100,\"resp\":{}}"));
  EXPECT_FALSE(is_lacking_properties_response("{\"status\":1234,\"resp\":{}}"));
}

TEST(Commands, IsLackingProperties_AcceptsWhitespaceAfterColon) {
  // some firmwares may emit `"status": 1` with a space after the colon
  EXPECT_TRUE(is_lacking_properties_response("{\"status\": 1,\"resp\":{}}"));
  EXPECT_TRUE(is_lacking_properties_response("{\"status\": 1, \"resp\": {}}"));
}

TEST(Commands, HasStatusValue_ZeroExactMatch) {
  EXPECT_TRUE(has_status_value("{\"status\":0,\"resp\":{}}", '0'));
  EXPECT_TRUE(has_status_value("{\"status\":0}", '0'));
}

TEST(Commands, HasStatusValue_ZeroAcceptsWhitespace) {
  // Tab and space variants between the colon and the value.
  EXPECT_TRUE(has_status_value("{\"status\": 0,\"resp\":{}}", '0'));
  EXPECT_TRUE(has_status_value("{\"status\":\t0,\"resp\":{}}", '0'));
}

TEST(Commands, HasStatusValue_ZeroRejectsLargerNumber) {
  // The killer case: 0 followed by another digit must NOT match.
  EXPECT_FALSE(has_status_value("{\"status\":01,\"resp\":{}}", '0'));
  EXPECT_FALSE(has_status_value("{\"status\":09,\"resp\":{}}", '0'));
  EXPECT_FALSE(has_status_value("{\"status\":01234}", '0'));
}

TEST(Commands, HasStatusValue_ZeroRejectsPushNotification) {
  // Push notifications never have a top-level "status" field.
  EXPECT_FALSE(has_status_value(
      "{\"msg_types\":2,\"seq\":1,\"props\":{\"door_ajar\":true}}", '0'));
  EXPECT_FALSE(has_status_value(
      "{\"diagnostic_status\":\"0x123\",\"msg_types\":1,\"seq\":1}", '0'));
}

TEST(Commands, HasStatusValue_DigitParameterDistinguishesValues) {
  // Same message: status:1 matches digit '1' but not '0', and vice versa.
  const std::string m1 = "{\"status\":1,\"resp\":{}}";
  EXPECT_TRUE(has_status_value(m1, '1'));
  EXPECT_FALSE(has_status_value(m1, '0'));
  const std::string m0 = "{\"status\":0,\"resp\":{\"foo\":1}}";
  EXPECT_TRUE(has_status_value(m0, '0'));
  EXPECT_FALSE(has_status_value(m0, '1'));
}
