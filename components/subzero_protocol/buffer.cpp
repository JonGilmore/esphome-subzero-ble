#include "buffer.h"

namespace esphome {
namespace subzero_protocol {

bool MessageBuffer::feed(const std::uint8_t *data, std::size_t len) {
  if (buf_.size() > kMaxBytes) {
    clear();
  }
  if (buf_.capacity() < kReserveHint) {
    buf_.reserve(kReserveHint);
  }
  for (std::size_t i = 0; i < len; i++) {
    char c = static_cast<char>(data[i]);
    buf_.push_back(c);
    if (complete_) {
      continue;
    }
    if (c == '{') {
      depth_++;
    } else if (c == '}') {
      // Clamp at 0: a stray '}' in pre-message ACL-corruption garbage
      // would otherwise drive depth_ negative, leaving the matching '}' of
      // the real message at -1 instead of 0 — the message would never
      // trip complete_ and would be silently lost when kMaxBytes flushes.
      // Naive in the same way the prior YAML scanner was, just underflow-safe.
      if (depth_ > 0) {
        depth_--;
        if (depth_ == 0) {
          complete_ = true;
        }
      }
    }
  }
  return complete_;
}

std::optional<std::string> MessageBuffer::take_message() {
  if (!complete_) {
    return std::nullopt;
  }
  std::size_t start = buf_.find('{');
  if (start == std::string::npos) {
    clear();
    return std::nullopt;
  }
  std::string out = (start == 0) ? std::move(buf_) : buf_.substr(start);
  clear();
  return out;
}

void MessageBuffer::clear() {
  buf_.clear();
  depth_ = 0;
  complete_ = false;
}

}  // namespace subzero_protocol
}  // namespace esphome
