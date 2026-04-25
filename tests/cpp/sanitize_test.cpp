#include "log_sanitize.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using esphome::subzero_protocol::chunk_for_log;
using esphome::subzero_protocol::sanitize_for_log;

TEST(Sanitize, AsciiPassthrough) {
  std::string s = "hello world";
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), s);
}

TEST(Sanitize, HighBitBecomesQuestionMark) {
  std::string s;
  s.push_back(0x80);
  s.push_back('a');
  s.push_back(static_cast<char>(0xFF));
  s.push_back('b');
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), "?a?b");
}

TEST(Sanitize, EmbeddedNullBecomesQuestionMark) {
  std::string s = std::string("a\0b", 3);
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), "a?b");
}

TEST(Sanitize, WhitespaceAllowed) {
  std::string s = "line1\nline2\rcol\twidth";
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), s);
}

TEST(Sanitize, ControlCharsBecomeQuestionMark) {
  std::string s;
  s.push_back(0x01);  // SOH
  s.push_back('a');
  s.push_back(0x1F);  // US (just below 0x20)
  s.push_back(0x7F);  // DEL (just above 0x7E)
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), "?a??");
}

TEST(Sanitize, OffsetAndLengthRespected) {
  std::string s = "abcdefghij";
  EXPECT_EQ(sanitize_for_log(s, 2, 4), "cdef");
  EXPECT_EQ(sanitize_for_log(s, 0, 3), "abc");
  EXPECT_EQ(sanitize_for_log(s, 8, 2), "ij");
}

TEST(Sanitize, LengthClampedToBuffer) {
  std::string s = "abc";
  EXPECT_EQ(sanitize_for_log(s, 1, 100), "bc");
  EXPECT_EQ(sanitize_for_log(s, 0, 100), "abc");
}

TEST(Sanitize, OffsetPastEndReturnsEmpty) {
  std::string s = "abc";
  EXPECT_EQ(sanitize_for_log(s, 5, 10), "");
  EXPECT_EQ(sanitize_for_log(s, 3, 1), "");
}

TEST(Sanitize, EmptyInput) {
  EXPECT_EQ(sanitize_for_log("", 0, 0), "");
  EXPECT_EQ(sanitize_for_log("", 0, 100), "");
}

TEST(Sanitize, NewlineKeptForJsonTerminator) {
  // The Sub-Zero protocol terminates every response with \n. Keeping it
  // unsanitized means a real corruption byte stands out as '?'.
  std::string s = "{\"a\":1}\n";
  EXPECT_EQ(sanitize_for_log(s, 0, s.size()), s);
}

namespace {

struct Recorded {
  std::size_t idx;
  std::size_t total;
  std::string chunk;
};

}  // namespace

TEST(ChunkForLog, SingleChunkBelowSize) {
  std::vector<Recorded> calls;
  std::string msg = "small";
  chunk_for_log(msg,
                [&](std::size_t i, std::size_t t, const std::string &c) {
                  calls.push_back({i, t, c});
                });
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].idx, 1u);
  EXPECT_EQ(calls[0].total, 1u);
  EXPECT_EQ(calls[0].chunk, "small");
}

TEST(ChunkForLog, ExactBoundary) {
  std::vector<Recorded> calls;
  std::string msg(800, 'x');  // exactly 2 chunks at 400
  chunk_for_log(msg,
                [&](std::size_t i, std::size_t t, const std::string &c) {
                  calls.push_back({i, t, c});
                });
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0].idx, 1u);
  EXPECT_EQ(calls[0].total, 2u);
  EXPECT_EQ(calls[0].chunk.size(), 400u);
  EXPECT_EQ(calls[1].idx, 2u);
  EXPECT_EQ(calls[1].total, 2u);
  EXPECT_EQ(calls[1].chunk.size(), 400u);
}

TEST(ChunkForLog, TrailingPartial) {
  std::vector<Recorded> calls;
  std::string msg(950, 'x');  // 2.375 chunks → 3 chunks (400, 400, 150)
  chunk_for_log(msg,
                [&](std::size_t i, std::size_t t, const std::string &c) {
                  calls.push_back({i, t, c});
                });
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_EQ(calls[0].chunk.size(), 400u);
  EXPECT_EQ(calls[1].chunk.size(), 400u);
  EXPECT_EQ(calls[2].chunk.size(), 150u);
  EXPECT_EQ(calls[2].total, 3u);
}

TEST(ChunkForLog, EmptyMessageNoCalls) {
  int calls = 0;
  chunk_for_log("", [&](std::size_t, std::size_t, const std::string &) { calls++; });
  EXPECT_EQ(calls, 0);
}

TEST(ChunkForLog, SanitizesEachChunk) {
  // High-bit bytes anywhere in the message become '?' in the chunk
  // delivered to the callback.
  std::string msg = std::string(400, 'a');
  msg.push_back(static_cast<char>(0xFF));
  msg.append(std::string(50, 'b'));
  std::vector<Recorded> calls;
  chunk_for_log(msg,
                [&](std::size_t i, std::size_t t, const std::string &c) {
                  calls.push_back({i, t, c});
                });
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0].chunk, std::string(400, 'a'));
  EXPECT_EQ(calls[1].chunk, "?" + std::string(50, 'b'));
}

TEST(ChunkForLog, CustomChunkSize) {
  std::vector<Recorded> calls;
  chunk_for_log("abcdef",
                [&](std::size_t i, std::size_t t, const std::string &c) {
                  calls.push_back({i, t, c});
                },
                2);
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_EQ(calls[0].chunk, "ab");
  EXPECT_EQ(calls[1].chunk, "cd");
  EXPECT_EQ(calls[2].chunk, "ef");
}
