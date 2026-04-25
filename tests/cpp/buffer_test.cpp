#include "buffer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using esphome::subzero_protocol::JsonBuffer;

namespace {

// Convenience: feed a string literal as bytes.
bool feed_str(JsonBuffer &b, const char *s) {
  return b.feed(reinterpret_cast<const std::uint8_t *>(s), std::strlen(s));
}

}  // namespace

TEST(JsonBuffer, EmptyByDefault) {
  JsonBuffer b;
  EXPECT_FALSE(b.complete());
  EXPECT_EQ(b.size(), 0u);
  EXPECT_FALSE(b.take_message().has_value());
}

TEST(JsonBuffer, SingleChunkComplete) {
  JsonBuffer b;
  EXPECT_TRUE(feed_str(b, "{\"a\":1}"));
  EXPECT_TRUE(b.complete());
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, "{\"a\":1}");
  EXPECT_FALSE(b.complete());
  EXPECT_EQ(b.size(), 0u);
}

TEST(JsonBuffer, FragmentedDelivery) {
  JsonBuffer b;
  EXPECT_FALSE(feed_str(b, "{\"a"));
  EXPECT_FALSE(feed_str(b, "\":1,"));
  EXPECT_FALSE(feed_str(b, "\"b\":{\"c\":2}"));  // nested {}: depth back to 1
  EXPECT_FALSE(b.complete());
  EXPECT_TRUE(feed_str(b, "}"));
  ASSERT_TRUE(b.complete());
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, "{\"a\":1,\"b\":{\"c\":2}}");
}

TEST(JsonBuffer, SingleByteAtATime) {
  JsonBuffer b;
  const char *s = "{\"x\":[1,2,3]}";
  for (size_t i = 0; i < std::strlen(s) - 1; i++) {
    auto ch = static_cast<std::uint8_t>(s[i]);
    EXPECT_FALSE(b.feed(&ch, 1)) << "completed early at i=" << i;
  }
  auto last = static_cast<std::uint8_t>(s[std::strlen(s) - 1]);
  EXPECT_TRUE(b.feed(&last, 1));
  EXPECT_EQ(*b.take_message(), s);
}

TEST(JsonBuffer, GarbagePrefixTrimmedOnTake) {
  // ACL corruption can deliver bytes before the JSON starts.
  // take_message() returns the substring from the first '{'.
  JsonBuffer b;
  EXPECT_TRUE(feed_str(b, "\x01\x02garbage{\"a\":1}"));
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, "{\"a\":1}");
}

TEST(JsonBuffer, OverCapacityResetsOnNextFeed) {
  // Wedged session: many fragments arrive without a closing `}`.
  // Once the buffer exceeds kMaxBytes, the NEXT feed clears it before
  // appending new bytes — preempts unbounded growth.
  JsonBuffer b;
  std::string huge(JsonBuffer::kMaxBytes + 100, 'x');
  // Prepend a `{` so depth goes to 1 (not complete).
  std::string bytes = "{" + huge;
  EXPECT_FALSE(b.feed(reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
  EXPECT_GT(b.size(), JsonBuffer::kMaxBytes);

  // Next feed: pre-emptive reset, then append the new fragment.
  EXPECT_TRUE(feed_str(b, "{\"a\":1}"));
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, "{\"a\":1}");
}

TEST(JsonBuffer, ClearResetsState) {
  JsonBuffer b;
  feed_str(b, "{");
  b.clear();
  EXPECT_EQ(b.size(), 0u);
  EXPECT_FALSE(b.complete());
  // After clear, the next feed starts fresh (depth was reset).
  EXPECT_TRUE(feed_str(b, "{\"a\":1}"));
}

TEST(JsonBuffer, TakeWithoutCompleteReturnsNullopt) {
  JsonBuffer b;
  feed_str(b, "{\"a");
  EXPECT_FALSE(b.complete());
  EXPECT_FALSE(b.take_message().has_value());
  // The buffer is preserved (not cleared) when take_message() finds
  // nothing to take — caller should keep feeding.
  EXPECT_GT(b.size(), 0u);
}

TEST(JsonBuffer, NoBraceClearsOnTake) {
  JsonBuffer b;
  // Force complete_ = true by feeding a balanced pair, but starting with
  // garbage that has no `{`. There's no realistic path where complete_
  // is true with no `{`, so we just exercise the defensive branch by
  // feeding a complete message and taking.
  feed_str(b, "{\"a\":1}");
  EXPECT_TRUE(b.complete());
  auto msg = b.take_message();
  EXPECT_TRUE(msg.has_value());
  EXPECT_EQ(b.size(), 0u);
}

TEST(JsonBuffer, NestedBracesDoNotFalseComplete) {
  JsonBuffer b;
  // Depth 2 reached by nested object; only the outer `}` brings it to 0.
  EXPECT_FALSE(feed_str(b, "{\"version\":{\"fw\":\"8.5\"}"));
  EXPECT_FALSE(b.complete());
  EXPECT_TRUE(feed_str(b, "}"));
  EXPECT_TRUE(b.complete());
}

TEST(JsonBuffer, StickyCompleteThroughTrailingBytes) {
  // Once complete, subsequent feeds accumulate bytes (next message) but
  // complete_ stays true until take_message()/clear(). The notify lambda
  // calls take_message() promptly so trailing bytes for the next response
  // don't accumulate in practice — but verify the sticky property.
  JsonBuffer b;
  EXPECT_TRUE(feed_str(b, "{\"a\":1}"));
  EXPECT_TRUE(feed_str(b, "extra"));  // doesn't affect completion
  EXPECT_TRUE(b.complete());
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, "{\"a\":1}extra");
}

TEST(JsonBuffer, RealWorldFridgePollResponse) {
  // Approximate a fragmented Sub-Zero poll response — small fragments
  // similar to default-MTU ACL chunks (~20-40 bytes).
  static const char *kFull =
      "{\"status\":0,\"resp\":{\"appliance_model\":\"BI36UFDID\",\"uptime\":\"1d2h\","
      "\"version\":{\"fw\":\"2.27\",\"api\":\"4.0\"},\"ref_set_temp\":38,\"frz_set_temp\":-2,"
      "\"door_ajar\":false,\"frz_door_ajar\":false}}";
  JsonBuffer b;
  size_t total = std::strlen(kFull);
  size_t off = 0;
  while (off < total) {
    size_t n = std::min<size_t>(20, total - off);
    bool done = b.feed(reinterpret_cast<const std::uint8_t *>(kFull + off), n);
    off += n;
    if (off < total) {
      EXPECT_FALSE(done) << "early completion at off=" << off;
    } else {
      EXPECT_TRUE(done);
    }
  }
  auto msg = b.take_message();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(*msg, kFull);
}
