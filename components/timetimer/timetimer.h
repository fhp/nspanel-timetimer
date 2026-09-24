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
  if (delta <= 0) {
    delta += 86400;
    if (delta > MAX_AHEAD_S) return {false, 0, "eindtijd is nu of net voorbij"};
  }
  if (delta > MAX_AHEAD_S) return {false, 0, "eindtijd ligt meer dan 12 uur vooruit"};
  return {true, now_epoch + delta, nullptr};
}

// Expands the panel's clock format (mui_time_format). The blueprint's formats use glibc's
// %-H / %-I, which newlib's strftime does not support.
inline std::string format_clock(const std::string &fmt, int hour, int minute, const std::string &am,
                                const std::string &pm) {
  auto two = [](int v) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02d", v);
    return std::string(buf);
  };
  const int hour12 = hour % 12 == 0 ? 12 : hour % 12;
  std::string out;
  for (size_t i = 0; i < fmt.size(); ++i) {
    if (fmt[i] != '%' || i + 1 >= fmt.size()) {
      out += fmt[i];
      continue;
    }
    std::string spec = fmt.substr(i + 1, fmt[i + 1] == '-' ? 2 : 1);
    if (spec == "H") out += two(hour);
    else if (spec == "-H") out += std::to_string(hour);
    else if (spec == "I") out += two(hour12);
    else if (spec == "-I") out += std::to_string(hour12);
    else if (spec == "M") out += two(minute);
    else if (spec == "p") out += hour < 12 ? am : pm;
    else if (spec == "%") out += '%';
    else {
      out += '%';
      continue;
    }
    i += spec.size();
  }
  return out;
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

// mdi:bell-ring, in the code point range the panel's MDI font uses.
constexpr const char *BELL_ICON = "";

struct View {
  Theme theme;
  uint16_t color;
  int64_t remaining_s;
  std::string label;
  std::string end_text;
  std::string clock_text;
};

// Nextion string literals have no escaping for these characters.
inline std::string escape_text(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) out += c == '"' ? '\'' : c == '\\' ? '/' : c;
  return out;
}

inline std::string format_mmss(int64_t seconds) {
  if (seconds < 0) seconds = 0;
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d:%02d", int(seconds / 60), int(seconds % 60));
  return buf;
}

inline std::string xstr(int x, int y, int w, int h, int font, uint16_t pco, uint16_t bco, int xcen,
                        const std::string &text) {
  return "xstr " + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(w) + "," +
         std::to_string(h) + "," + std::to_string(font) + "," + std::to_string(pco) + "," +
         std::to_string(bco) + "," + std::to_string(xcen) + ",1,1,\"" + escape_text(text) + "\"";
}

inline std::string cmd_fill(int x, int y, int w, int h, uint16_t color) {
  return "fill " + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(w) + "," +
         std::to_string(h) + "," + std::to_string(color);
}

inline std::string cmd_xy2(const char *op, int x1, int y1, int x2, int y2, uint16_t color) {
  return std::string(op) + " " + std::to_string(x1) + "," + std::to_string(y1) + "," + std::to_string(x2) + "," +
         std::to_string(y2) + "," + std::to_string(color);
}

inline std::string cmd_cirs(int x, int y, int r, uint16_t color) {
  return "cirs " + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(r) + "," +
         std::to_string(color);
}

inline void render_spans(const std::vector<Span> &spans, uint16_t color, std::vector<std::string> &out) {
  for (const Span &s : spans) out.push_back(cmd_fill(s.x, s.y, s.w, 1, color));
}

// Ticks and hub sit on top of the sector and are repainted after every sector change.
inline void render_face_overlay(const View &v, std::vector<std::string> &out) {
  for (int i = 0; i < 12; ++i) {
    Point a = on_circle(CX, CY, TICK_OUTER, i / 12.0);
    Point b = on_circle(CX, CY, TICK_INNER, i / 12.0);
    bool mostly_horizontal = std::abs(a.x - b.x) > std::abs(a.y - b.y);
    int ox = mostly_horizontal ? 0 : 1;
    int oy = mostly_horizontal ? 1 : 0;
    out.push_back(cmd_xy2("line", a.x, a.y, b.x, b.y, v.theme.tick));
    out.push_back(cmd_xy2("line", a.x + ox, a.y + oy, b.x + ox, b.y + oy, v.theme.tick));
  }
  out.push_back(cmd_cirs(CX, CY, HUB_R, v.theme.tick));
}

inline void render_countdown(const View &v, std::vector<std::string> &out) {
  out.push_back(xstr(276, 112, 194, 80, 6, v.color, v.theme.bg, 0, format_mmss(v.remaining_s)));
}

inline void render_timer_clock(const View &v, std::vector<std::string> &out) {
  out.push_back(xstr(356, 8, 100, 28, 2, v.theme.muted, v.theme.bg, 2, v.clock_text));
}

inline void render_timer_full(const View &v, std::vector<std::string> &out) {
  out.push_back(cmd_fill(0, 0, 480, 320, v.theme.bg));
  out.push_back(cmd_cirs(CX, CY, R, v.theme.face));
  render_spans(sector_spans(CX, CY, R, 0.0, fraction_for(v.remaining_s)), v.color, out);
  render_face_overlay(v, out);
  for (int i = 0; i < 12; ++i) {
    Point p = on_circle(CX, CY, LABEL_R, i / 12.0);
    out.push_back(xstr(p.x - 14, p.y - 9, 28, 18, 0, v.theme.muted, v.theme.bg, 1, std::to_string(i * 5)));
  }
  out.push_back(xstr(276, 70, 194, 34, 3, v.theme.text, v.theme.bg, 0, v.label));
  render_countdown(v, out);
  out.push_back(xstr(276, 196, 194, 24, 1, v.theme.muted, v.theme.bg, 0, "klaar om " + v.end_text));
  render_timer_clock(v, out);
}

inline void render_sliver(const View &v, double from, double to, std::vector<std::string> &out) {
  render_spans(sector_spans(CX, CY, R, from, to), v.theme.face, out);
  render_face_overlay(v, out);
}

inline void render_alarm_clock(const View &v, std::vector<std::string> &out) {
  out.push_back(xstr(356, 8, 100, 28, 2, WHITE, ALARM_BG, 2, v.clock_text));
}

inline void render_alarm_full(const View &v, std::vector<std::string> &out) {
  out.push_back(cmd_fill(0, 0, 480, 320, ALARM_BG));
  out.push_back(xstr(0, 40, 480, 60, 10, WHITE, ALARM_BG, 1, BELL_ICON));
  out.push_back(xstr(0, 104, 480, 60, 5, WHITE, ALARM_BG, 1, "Tijd is om!"));
  out.push_back(xstr(0, 166, 480, 34, 3, WHITE, ALARM_BG, 1, v.label));
  out.push_back(cmd_xy2("draw", 150, 216, 330, 272, WHITE));
  out.push_back(cmd_xy2("draw", 151, 217, 329, 271, WHITE));
  out.push_back(xstr(152, 218, 176, 52, 3, WHITE, ALARM_BG, 1, "Stop"));
  render_alarm_clock(v, out);
}

}  // namespace timetimer
