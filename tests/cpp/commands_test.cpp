#include "commands.h"

#include <gtest/gtest.h>

#include <string>

using esphome::subzero_protocol::build_display_pin;
using esphome::subzero_protocol::build_get_async;
using esphome::subzero_protocol::build_unlock_channel;

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
