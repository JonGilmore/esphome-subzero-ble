#pragma once

#include <cstdio>
#include <string>

namespace esphome {
namespace subzero_protocol {
namespace detail {

inline std::string escape_json_string(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (char c : s) {
    unsigned char u = static_cast<unsigned char>(c);
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (u < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04X", u);
        out += buf;
      } else {
        out.push_back(c);
      }
      break;
    }
  }
  return out;
}

} // namespace detail

inline std::string build_unlock_channel(const std::string &pin) {
  return "{\"cmd\":\"unlock_channel\",\"params\":{\"pin\":\"" +
         detail::escape_json_string(pin) + "\"}}\n";
}

inline std::string build_get_async() { return "{\"cmd\":\"get_async\"}\n"; }
inline std::string build_get() { return "{\"cmd\":\"get\"}\n"; }

enum class PollVerb {
  kGetAsync,
  kGet,
};

inline std::string build_poll_command(PollVerb v) {
  return v == PollVerb::kGet ? build_get() : build_get_async();
}

inline bool has_status_value(const std::string &msg, char digit) {
  static constexpr const char *kStatusKey = "\"status\":";
  static constexpr std::size_t kKeyLen = 9; // strlen(kStatusKey)
  std::size_t pos = 0;
  while ((pos = msg.find(kStatusKey, pos)) != std::string::npos) {
    std::size_t v = pos + kKeyLen;
    // Skip optional whitespace after colon.
    while (v < msg.size() && (msg[v] == ' ' || msg[v] == '\t'))
      v++;
    if (v < msg.size() && msg[v] == digit) {
      char next = (v + 1 < msg.size()) ? msg[v + 1] : '\0';
      if (next < '0' || next > '9') {
        return true;
      }
    }
    pos++;
  }
  return false;
}

inline bool is_lacking_properties_response(const std::string &msg) {
  if (!has_status_value(msg, '1'))
    return false;
  // Empty resp object — accept both spacing variants observed on the wire.
  return msg.find("\"resp\":{}") != std::string::npos ||
         msg.find("\"resp\": {}") != std::string::npos;
}

inline std::string build_display_pin(int duration_seconds = 30) {
  return "{\"cmd\":\"display_pin\",\"params\":{\"duration\": " +
         std::to_string(duration_seconds) + "}}\n";
}

inline std::string build_set(const std::string &key,
                             const std::string &json_value) {
  return "{\"cmd\":\"set\",\"params\":{\"" + detail::escape_json_string(key) +
         "\":" + json_value + "}}\n";
}

inline std::string build_set_bool(const std::string &key, bool value) {
  return build_set(key, value ? "true" : "false");
}

inline std::string build_set_int(const std::string &key, int value) {
  return build_set(key, std::to_string(value));
}

inline std::string build_set_string(const std::string &key,
                                    const std::string &value) {
  return build_set(key, "\"" + detail::escape_json_string(value) + "\"");
}

} // namespace subzero_protocol
} // namespace esphome
