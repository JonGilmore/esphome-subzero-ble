#include "gatt_handles.h"

#include <gtest/gtest.h>

#include <vector>

using esphome::subzero_protocol::AppHandles;
using esphome::subzero_protocol::extract_handles;
using esphome::subzero_protocol::GattEntry;

namespace {

GattEntry make_char(std::uint8_t uuid_first, std::uint16_t handle) {
  return GattEntry{/*is_characteristic=*/true, /*is_uuid128=*/true, uuid_first,
                   handle};
}

} // namespace

TEST(AppHandles, EmptyByDefault) {
  AppHandles h;
  EXPECT_EQ(h.d5, 0);
  EXPECT_EQ(h.d6, 0);
  EXPECT_EQ(h.d7, 0);
  EXPECT_FALSE(h.has_d5());
  EXPECT_FALSE(h.has_d6());
  EXPECT_FALSE(h.has_d7());
  EXPECT_FALSE(h.ready());
}

TEST(AppHandles, ReadyWantsBothD5AndD6) {
  AppHandles h;
  EXPECT_FALSE(h.ready());
  h.d5 = 0x10;
  EXPECT_FALSE(h.ready());
  h.d6 = 0x20;
  EXPECT_TRUE(h.ready());
  // D7 is optional for ready-state.
  AppHandles h2;
  h2.d7 = 0x30;
  EXPECT_FALSE(h2.ready());
}

TEST(ExtractHandles, EmptyInputLeavesOutputUntouched) {
  std::vector<GattEntry> entries;
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_EQ(h.d5, 0);
  EXPECT_EQ(h.d6, 0);
  EXPECT_EQ(h.d7, 0);
}

TEST(ExtractHandles, FullDiscoveryPicksUpAllThree) {
  std::vector<GattEntry> entries{
      make_char(0xD4, 0x10), // ignored: D4 unused post-PR-#72
      make_char(0xD5, 0x12), make_char(0xD6, 0x14), make_char(0xD7, 0x16),
      make_char(0xD8, 0x18), // ignored: D8 unused post-PR-#72
  };
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_EQ(h.d5, 0x12);
  EXPECT_EQ(h.d6, 0x14);
  EXPECT_EQ(h.d7, 0x16);
}

TEST(ExtractHandles, FridgeStylePartialFirstPass) {
  // On fridges, D5 is not visible until after bonding + cache refresh.
  // The first GATT db dump returns only the services / 16-bit-UUID
  // characteristics; D5/D6/D7 land in a later pass.
  std::vector<GattEntry> entries{
      // Service entries (not characteristics): ignored.
      GattEntry{/*char=*/false, /*128=*/false, 0xD5, 0x99},
      // 16-bit UUID characteristic: ignored even if first byte matches.
      GattEntry{/*char=*/true, /*128=*/false, 0xD5, 0x99},
  };
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_FALSE(h.ready());
}

TEST(ExtractHandles, IsAdditiveAcrossPasses) {
  AppHandles h;
  // Pass 1: only D7 visible (open pre-auth channel).
  std::vector<GattEntry> pass1{make_char(0xD7, 0x16)};
  extract_handles(pass1, h);
  EXPECT_EQ(h.d5, 0);
  EXPECT_EQ(h.d6, 0);
  EXPECT_EQ(h.d7, 0x16);

  // Pass 2: D5 and D6 now visible (post-encryption).
  std::vector<GattEntry> pass2{
      make_char(0xD5, 0x12),
      make_char(0xD6, 0x14),
  };
  extract_handles(pass2, h);
  EXPECT_EQ(h.d5, 0x12);
  EXPECT_EQ(h.d6, 0x14);
  EXPECT_EQ(h.d7, 0x16);
  EXPECT_TRUE(h.ready());
}

TEST(ExtractHandles, DoesNotOverwriteAlreadyFoundHandles) {
  // Some appliances re-emit characteristic entries with different
  // attribute_handles after a cache refresh (Bluedroid quirk). Once we
  // have a handle, keep it — re-subscribing on a different handle would
  // confuse the existing notify wiring.
  AppHandles h;
  h.d5 = 0x12;
  h.d6 = 0x14;
  std::vector<GattEntry> later_pass{
      make_char(0xD5, 0xAA), // would-be replacement
      make_char(0xD6, 0xBB),
      make_char(0xD7, 0x16),
  };
  extract_handles(later_pass, h);
  EXPECT_EQ(h.d5, 0x12); // preserved
  EXPECT_EQ(h.d6, 0x14); // preserved
  EXPECT_EQ(h.d7, 0x16); // newly found
}

TEST(ExtractHandles, IgnoresDescriptorsAndServices) {
  std::vector<GattEntry> entries{
      // Service entry shaped like D5 — must not be picked up.
      GattEntry{/*char=*/false, /*128=*/true, 0xD5, 0x10},
      // Real D5 characteristic.
      make_char(0xD5, 0x12),
  };
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_EQ(h.d5, 0x12);
}

TEST(ExtractHandles, IgnoresUnknownUuidFirstBytes) {
  std::vector<GattEntry> entries{
      make_char(0x00, 0x10),
      make_char(0x42, 0x12),
      make_char(0xFF, 0x14),
  };
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_EQ(h.d5, 0);
  EXPECT_EQ(h.d6, 0);
  EXPECT_EQ(h.d7, 0);
}

TEST(ExtractHandles, RealisticDualOvenRange) {
  // Reflects what we observed on the Wolf SO3050PESP wall oven during
  // a successful discovery pass.
  std::vector<GattEntry> entries{
      // Generic Access Profile (1800) primary service entries...
      GattEntry{/*char=*/false, /*128=*/false, 0x00, 0x0001},
      // Device Name characteristic (16-bit UUID 2A00) — ignored.
      GattEntry{/*char=*/true, /*128=*/false, 0x00, 0x0003},
      // Sub-Zero proprietary service entry (128-bit but not a char).
      GattEntry{/*char=*/false, /*128=*/true, 0xF4, 0x000A},
      // The five characteristics, only D5/D6/D7 picked up.
      make_char(0xD4, 0x000C),
      make_char(0xD5, 0x000F),
      make_char(0xD6, 0x0012),
      make_char(0xD7, 0x0015),
      make_char(0xD8, 0x0018),
  };
  AppHandles h;
  extract_handles(entries, h);
  EXPECT_EQ(h.d5, 0x000F);
  EXPECT_EQ(h.d6, 0x0012);
  EXPECT_EQ(h.d7, 0x0015);
  EXPECT_TRUE(h.ready());
}
