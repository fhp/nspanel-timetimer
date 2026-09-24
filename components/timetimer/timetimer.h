#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace timetimer {

constexpr int64_t VISIBLE_WINDOW_S = 3600;
constexpr int64_t MAX_AHEAD_S = 12 * 3600;
constexpr int64_t DISMISS_RETURN_S = 30;
constexpr int64_t BEEP_DURATION_S = 30;
constexpr int64_t BEEP_INTERVAL_S = 3;
constexpr int64_t AFTERGLOW_S = 60;

constexpr uint16_t COLOR_RED = 55717;
constexpr uint16_t ALARM_BG = 47427;
constexpr uint16_t WHITE = 65535;

struct Theme {
  uint16_t bg, face, text, muted, tick;
};

constexpr Theme LIGHT{65502, 61276, 6338, 23241, 14757};
constexpr Theme DARK{4226, 8451, 61276, 42226, 35887};

struct EndTime {
  bool ok;
  int64_t epoch;
  const char *error;
};

// Local seconds-of-day is passed separately so this works without a timezone database.
// A DST switch between now and the end time shifts the end by an hour; accepted.
inline EndTime resolve_end_time(const std::string &hhmm, int64_t now_epoch, int32_t now_seconds_of_day) {
  int h = -1, m = -1;
  char extra = 0;
  if (hhmm.size() < 3 || hhmm.size() > 5 ||
      std::sscanf(hhmm.c_str(), "%d:%d%c", &h, &m, &extra) != 2 ||
      h < 0 || h > 23 || m < 0 || m > 59) {
    return {false, 0, "ongeldige eindtijd, verwacht HH:MM"};
  }
  int64_t delta = int64_t(h) * 3600 + int64_t(m) * 60 - now_seconds_of_day;
  if (delta <= 0) delta += 86400;
  if (delta > MAX_AHEAD_S) return {false, 0, "eindtijd ligt meer dan 12 uur vooruit"};
  return {true, now_epoch + delta, nullptr};
}

inline bool color_from_name(const std::string &name, uint16_t &out) {
  struct Entry {
    const char *name;
    uint16_t color;
  };
  static const Entry table[] = {
      {"rood", 55717}, {"oranje", 60325}, {"geel", 56579},
      {"groen", 15561}, {"blauw", 11130}, {"paars", 35449},
  };
  out = COLOR_RED;
  if (name.empty()) return true;
  std::string lower;
  for (char ch : name) lower += char(std::tolower(static_cast<unsigned char>(ch)));
  for (const Entry &e : table) {
    if (lower == e.name) {
      out = e.color;
      return true;
    }
  }
  return false;
}

}  // namespace timetimer
