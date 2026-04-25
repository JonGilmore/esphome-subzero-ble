#include "log_sanitize.h"

namespace esphome {
namespace subzero_protocol {

std::string sanitize_for_log(const std::string &s, std::size_t off, std::size_t len) {
  if (off >= s.size()) {
    return std::string();
  }
  std::size_t avail = s.size() - off;
  if (len > avail) {
    len = avail;
  }
  std::string out(s, off, len);
  for (char &c : out) {
    unsigned char u = static_cast<unsigned char>(c);
    bool ok = (u >= 0x20 && u <= 0x7E) || u == 0x09 || u == 0x0A || u == 0x0D;
    if (!ok) {
      c = '?';
    }
  }
  return out;
}

}  // namespace subzero_protocol
}  // namespace esphome
