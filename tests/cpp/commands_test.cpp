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
