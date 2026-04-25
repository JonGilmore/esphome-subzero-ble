#pragma once

// Sanitization helpers for logging raw BLE-buffer contents.
//
// The raw buffer may contain non-UTF-8 bytes from ACL fragment corruption.
// Anything we log MUST be sanitized first: ESPHome ships log lines to HA
// over a protobuf-typed transport (UTF-8 required); a high-bit byte
// (0x80-0xFF) that isn't a valid multi-byte sequence crashes HA's protobuf
// decoder, which then caches the bad event and loops.
//
// Allowlist: printable ASCII (0x20..0x7E) and the whitespace controls
// \t \n \r — all valid UTF-8 and harmless in protobuf. Everything else
// becomes '?'. Keeping \n unsanitized means the protocol's terminator at
// the end of every response no longer triggers a nuisance '?', so any '?'
// appearing in a log line is a real corruption signal.

#include <algorithm>
#include <cstddef>
#include <string>

namespace esphome {
namespace subzero_protocol {

// Replaces non-printable bytes in [off, off+len) of `s` with '?' and
// returns the resulting string. Out-of-range len is clamped.
//
// Defined inline (header-only): see buffer.h for rationale (avoid
// tripping ESPHome's GLOB_RECURSE source-list cache).
inline std::string sanitize_for_log(const std::string &s, std::size_t off, std::size_t len) {
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

// Splits a message into chunks (default 400 bytes, sized for ESPHome's
// per-line log budget so the full payload survives) and invokes
// cb(idx_1based, total, sanitized_chunk) for each. Used for fixture-capture
// debug logging — grep `Response\[` and concatenate the chunks to
// reassemble the raw payload.
template <typename Cb>
void chunk_for_log(const std::string &msg, Cb cb, std::size_t chunk_size = 400) {
  if (msg.empty() || chunk_size == 0) {
    return;
  }
  std::size_t total = (msg.size() + chunk_size - 1) / chunk_size;
  for (std::size_t i = 0; i < total; i++) {
    std::size_t off = i * chunk_size;
    std::size_t len = std::min(chunk_size, msg.size() - off);
    cb(i + 1, total, sanitize_for_log(msg, off, len));
  }
}

}  // namespace subzero_protocol
}  // namespace esphome
