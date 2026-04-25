#include "poll_state.h"
#include "protocol.h"

#include <gtest/gtest.h>

using esphome::subzero_protocol::apply_parse_result;
using esphome::subzero_protocol::DishwasherState;
using esphome::subzero_protocol::FridgeState;
using esphome::subzero_protocol::RangeState;

namespace {

FridgeState make_poll(const char *fw = nullptr) {
  FridgeState s;
  s.valid = true;
  s.is_poll = true;
  if (fw) {
    s.common.version.fw = std::string(fw);
  }
  return s;
}

FridgeState make_push() {
  FridgeState s;
  s.valid = true;
  s.is_poll = false;
  return s;
}

}  // namespace

TEST(PollState, PollClearsCounters) {
  bool poll_in_flight = true;
  int unanswered_polls = 2;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 1;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll());

  EXPECT_FALSE(poll_in_flight);
  EXPECT_EQ(unanswered_polls, 0);
  EXPECT_TRUE(first_poll_done);
  EXPECT_FALSE(fw_85_detected);  // no fw in payload
  EXPECT_EQ(fast_retries, 0);
}

TEST(PollState, PushDoesNotClearPollInFlight) {
  // CRITICAL: pushes prove the connection is alive but NOT the command
  // channel. Clearing poll_in_flight on a push would defeat the unlock-
  // expiry detector for fw 8.5 appliances. See poll_state.h invariant 1.
  bool poll_in_flight = true;
  int unanswered_polls = 2;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 3;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_push());

  EXPECT_TRUE(poll_in_flight);                      // unchanged
  EXPECT_EQ(unanswered_polls, 2);                   // unchanged
  EXPECT_FALSE(first_poll_done);                    // unchanged
  EXPECT_FALSE(fw_85_detected);                     // unchanged
  EXPECT_EQ(fast_retries, 0);                       // RESET on any valid parse
}

TEST(PollState, Fw85StickyOnPoll) {
  bool poll_in_flight = false;
  int unanswered_polls = 0;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 0;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll("8.5"));
  EXPECT_TRUE(fw_85_detected);

  // Subsequent poll without fw field doesn't clear the sticky flag.
  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll());
  EXPECT_TRUE(fw_85_detected);
}

TEST(PollState, Fw227NotDetected) {
  bool poll_in_flight = false;
  int unanswered_polls = 0;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 0;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll("2.27"));
  EXPECT_FALSE(fw_85_detected);
}

TEST(PollState, Fw85PushIgnored) {
  // version.fw on a push notification (rare but possible) does NOT set
  // fw_85_detected — only is_poll responses count, because push payloads
  // don't carry the full version object reliably.
  FridgeState s = make_push();
  s.common.version.fw = std::string("8.5");

  bool poll_in_flight = false;
  int unanswered_polls = 0;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 0;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, s);
  EXPECT_FALSE(fw_85_detected);
  EXPECT_FALSE(first_poll_done);
}

TEST(PollState, FastRetriesResetOnEveryValidParse) {
  bool poll_in_flight = true;
  int unanswered_polls = 1;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 5;

  // Push: fast_retries still resets.
  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_push());
  EXPECT_EQ(fast_retries, 0);

  // Re-set, then poll.
  fast_retries = 7;
  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll());
  EXPECT_EQ(fast_retries, 0);
}

TEST(PollState, WorksForDishwasherState) {
  DishwasherState s;
  s.valid = true;
  s.is_poll = true;
  s.common.version.fw = std::string("8.5");

  bool poll_in_flight = true;
  int unanswered_polls = 0;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 0;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, s);
  EXPECT_FALSE(poll_in_flight);
  EXPECT_TRUE(first_poll_done);
  EXPECT_TRUE(fw_85_detected);
}

TEST(PollState, WorksForRangeState) {
  RangeState s;
  s.valid = true;
  s.is_poll = false;  // push
  s.common.version.fw = std::string("8.5");

  bool poll_in_flight = true;
  int unanswered_polls = 1;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 4;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, s);
  EXPECT_TRUE(poll_in_flight);    // push: unchanged
  EXPECT_FALSE(fw_85_detected);   // push: not sticky-set
  EXPECT_EQ(fast_retries, 0);     // any valid parse resets
}

TEST(PollState, FirstPollDoneOneShot) {
  bool poll_in_flight = false;
  int unanswered_polls = 0;
  bool first_poll_done = false;
  bool fw_85_detected = false;
  int fast_retries = 0;

  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_poll());
  EXPECT_TRUE(first_poll_done);

  // Subsequent push doesn't unset it (first_poll_done is sticky-true).
  apply_parse_result(poll_in_flight, unanswered_polls, first_poll_done,
                     fw_85_detected, fast_retries, make_push());
  EXPECT_TRUE(first_poll_done);
}
