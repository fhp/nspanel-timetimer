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

enum class State : uint8_t { OFF, SCHEDULED, VISIBLE, DISMISSED, ALARM, AFTERGLOW };

inline bool shows_page(State s) {
  return s == State::VISIBLE || s == State::ALARM || s == State::AFTERGLOW;
}

class Machine {
 public:
  void start(int64_t end_epoch, int64_t now) {
    end_ = end_epoch;
    state_ = State::SCHEDULED;
    update(now);
  }

  // Used after boot: an end time that passed while the panel was down is dropped silently.
  void restore(int64_t end_epoch, int64_t now) {
    if (end_epoch <= now) {
      stop();
      return;
    }
    start(end_epoch, now);
  }

  void stop() {
    state_ = State::OFF;
    end_ = 0;
  }

  void update(int64_t now) {
    switch (state_) {
      case State::OFF:
        break;
      case State::SCHEDULED:
        if (now >= end_) enter(State::ALARM, now);
        else if (end_ - now <= VISIBLE_WINDOW_S) state_ = State::VISIBLE;
        break;
      case State::VISIBLE:
        if (now >= end_) enter(State::ALARM, now);
        break;
      case State::DISMISSED:
        if (now >= end_) enter(State::ALARM, now);
        else if (now - since_ >= DISMISS_RETURN_S) state_ = State::VISIBLE;
        break;
      case State::ALARM:
        if (now - since_ >= BEEP_DURATION_S) enter(State::AFTERGLOW, now);
        break;
      case State::AFTERGLOW:
        if (now - since_ >= AFTERGLOW_S) stop();
        break;
    }
  }

  void tap(int64_t now) {
    switch (state_) {
      case State::VISIBLE:
      case State::DISMISSED:
        enter(State::DISMISSED, now);
        break;
      case State::ALARM:
        enter(State::AFTERGLOW, now);
        break;
      case State::AFTERGLOW:
        stop();
        break;
      default:
        break;
    }
  }

  // Something other than a tap on the timer page navigated away from it.
  void left_page(int64_t now) {
    if (state_ == State::VISIBLE) enter(State::DISMISSED, now);
    else if (state_ == State::ALARM || state_ == State::AFTERGLOW) stop();
  }

  bool should_beep(int64_t now) const {
    return state_ == State::ALARM && (now - since_) % BEEP_INTERVAL_S == 0;
  }

  int64_t remaining(int64_t now) const { return end_ > now ? end_ - now : 0; }
  State state() const { return state_; }
  int64_t end() const { return end_; }

 private:
  void enter(State s, int64_t now) {
    state_ = s;
    since_ = now;
  }

  State state_{State::OFF};
  int64_t end_{0};
  int64_t since_{0};
};

constexpr int CX = 140;
constexpr int CY = 166;
constexpr int R = 108;
constexpr int LABEL_R = 123;
constexpr int TICK_OUTER = R - 2;
constexpr int TICK_INNER = R - 16;
constexpr int HUB_R = 10;
constexpr double TAU = 6.283185307179586;

struct Span {
  int16_t x, y, w;
};

struct Point {
  int x, y;
};

inline double fraction_for(int64_t remaining_s) {
  if (remaining_s <= 0) return 0.0;
  if (remaining_s >= VISIBLE_WINDOW_S) return 1.0;
  return double(remaining_s) / double(VISIBLE_WINDOW_S);
}

// Counterclockwise from 12 o'clock, like the face of a Time Timer; result in [0, 1).
inline double fraction_at(int dx, int dy) {
  double a = std::atan2(-double(dx), -double(dy));
  if (a < 0) a += TAU;
  double f = a / TAU;
  return f >= 1.0 ? 0.0 : f;
}

inline Point on_circle(int cx, int cy, double r, double fraction) {
  return {int(std::lround(cx - r * std::sin(fraction * TAU))), int(std::lround(cy - r * std::cos(fraction * TAU)))};
}

inline std::vector<Span> sector_spans(int cx, int cy, int r, double from, double to) {
  std::vector<Span> out;
  if (to <= from) return out;
  for (int dy = -r; dy <= r; ++dy) {
    bool in_run = false;
    int run_start = 0;
    for (int dx = -r; dx <= r + 1; ++dx) {
      bool inside = false;
      if (dx <= r && dx * dx + dy * dy <= r * r) {
        double f = fraction_at(dx, dy);
        inside = f >= from && f < to;
      }
      if (inside && !in_run) {
        in_run = true;
        run_start = dx;
      } else if (!inside && in_run) {
        in_run = false;
        out.push_back({int16_t(cx + run_start), int16_t(cy + dy), int16_t(dx - run_start)});
      }
    }
  }
  return out;
}

}  // namespace timetimer
