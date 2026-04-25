#include "buffer.h"

namespace esphome {
namespace subzero_protocol {

bool JsonBuffer::feed(const std::uint8_t *data, std::size_t len) {
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
      depth_--;
      if (depth_ == 0) {
        complete_ = true;
      }
    }
  }
  return complete_;
}

std::optional<std::string> JsonBuffer::take_message() {
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

void JsonBuffer::clear() {
  buf_.clear();
  depth_ = 0;
  complete_ = false;
}

}  // namespace subzero_protocol
}  // namespace esphome
