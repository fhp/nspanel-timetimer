#include "timetimer.h"

#include <cstdio>
#include <cstring>

static int failures = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                       \
    }                                                                   \
  } while (0)

using namespace timetimer;

// 2026-09-24 19:00:30 local, as seconds of day and an arbitrary epoch.
static const int64_t NOW = 1790270430;
static const int32_t NOW_SOD = 19 * 3600 + 30;

static void test_resolve_end_time() {
  EndTime r = resolve_end_time("20:00", NOW, NOW_SOD);
  CHECK(r.ok);
  CHECK(r.epoch == NOW + 3570);

  r = resolve_end_time("07:00", NOW, NOW_SOD);
  CHECK(r.ok);
  CHECK(r.epoch == NOW + 12 * 3600 - 30);

  r = resolve_end_time("19:00", NOW, NOW_SOD);  // current minute
  CHECK(!r.ok);
  CHECK(std::strstr(r.error, "nu of net voorbij") != nullptr);

  r = resolve_end_time("18:59", NOW, NOW_SOD);  // just passed: would be tomorrow, more than 12 hours
  CHECK(!r.ok);
  CHECK(std::strstr(r.error, "nu of net voorbij") != nullptr);

  r = resolve_end_time("14:00", NOW, 3600);  // at 01:00: later today, but more than 12 hours ahead
  CHECK(!r.ok);
  CHECK(std::strstr(r.error, "12 uur") != nullptr);

  r = resolve_end_time("07:01", NOW, NOW_SOD);  // just over 12 hours ahead
  CHECK(!r.ok);

  r = resolve_end_time("7:05", NOW, NOW_SOD);
  CHECK(!r.ok);  // 12h04m30s ahead

  r = resolve_end_time("19:01", NOW, NOW_SOD);
  CHECK(r.ok);
  CHECK(r.epoch == NOW + 30);

  const char *bad[] = {"", "20", "24:00", "12:60", "ab:cd", "20:00:00", "-1:00", "20:00x"};
  for (const char *b : bad) {
    r = resolve_end_time(b, NOW, NOW_SOD);
    CHECK(!r.ok);
    CHECK(std::strstr(r.error, "HH:MM") != nullptr);
  }
}

static void test_colors() {
  uint16_t c = 0;
  CHECK(color_from_name("", c) && c == COLOR_RED);
  CHECK(color_from_name("rood", c) && c == 55717);
  CHECK(color_from_name("oranje", c) && c == 60325);
  CHECK(color_from_name("geel", c) && c == 56579);
  CHECK(color_from_name("groen", c) && c == 15561);
  CHECK(color_from_name("Blauw", c) && c == 11130);
  CHECK(color_from_name("PAARS", c) && c == 35449);
  CHECK(!color_from_name("roze", c) && c == COLOR_RED);
}

static void test_machine_schedule_and_visible() {
  Machine m;
  CHECK(m.state() == State::OFF);

  m.start(NOW + 7200, NOW);
  CHECK(m.state() == State::SCHEDULED);
  m.update(NOW + 3599);
  CHECK(m.state() == State::SCHEDULED);
  m.update(NOW + 3600);
  CHECK(m.state() == State::VISIBLE);
  CHECK(m.remaining(NOW + 3600) == 3600);

  m.start(NOW + 600, NOW);
  CHECK(m.state() == State::VISIBLE);
  CHECK(m.end() == NOW + 600);
}

static void test_machine_dismiss() {
  Machine m;
  m.start(NOW + 600, NOW);
  m.tap(NOW + 10);
  CHECK(m.state() == State::DISMISSED);
  m.update(NOW + 39);
  CHECK(m.state() == State::DISMISSED);
  m.tap(NOW + 39);  // touch on another page keeps it dismissed
  m.update(NOW + 68);
  CHECK(m.state() == State::DISMISSED);
  m.update(NOW + 69);
  CHECK(m.state() == State::VISIBLE);

  m.left_page(NOW + 100);
  CHECK(m.state() == State::DISMISSED);
  m.update(NOW + 600);  // end reached while dismissed
  CHECK(m.state() == State::ALARM);
}

static void test_machine_alarm_and_afterglow() {
  Machine m;
  m.start(NOW + 10, NOW);
  m.update(NOW + 10);
  CHECK(m.state() == State::ALARM);
  CHECK(m.should_beep(NOW + 10));
  CHECK(!m.should_beep(NOW + 11));
  CHECK(!m.should_beep(NOW + 13));
  CHECK(m.should_beep(NOW + 15));
  CHECK(m.should_beep(NOW + 35));
  CHECK(!m.should_beep(NOW + 37));
  m.update(NOW + 39);
  CHECK(m.state() == State::ALARM);
  m.update(NOW + 40);
  CHECK(m.state() == State::AFTERGLOW);
  CHECK(!m.should_beep(NOW + 40));
  m.update(NOW + 99);
  CHECK(m.state() == State::AFTERGLOW);
  m.update(NOW + 100);
  CHECK(m.state() == State::OFF);

  m.start(NOW + 10, NOW);
  m.update(NOW + 10);
  m.tap(NOW + 12);
  CHECK(m.state() == State::AFTERGLOW);
  m.tap(NOW + 20);
  CHECK(m.state() == State::OFF);

  m.start(NOW + 10, NOW);
  m.update(NOW + 10);
  m.left_page(NOW + 11);
  CHECK(m.state() == State::OFF);
}

static void test_machine_stop_restore_replace() {
  const State states_to_try[] = {State::SCHEDULED, State::VISIBLE, State::DISMISSED, State::ALARM, State::AFTERGLOW};
  for (State target : states_to_try) {
    Machine m;
    m.start(NOW + 7200, NOW);
    if (target != State::SCHEDULED) m.update(NOW + 3600);
    if (target == State::DISMISSED) m.tap(NOW + 3600);
    if (target == State::ALARM || target == State::AFTERGLOW) m.update(NOW + 7200);
    if (target == State::AFTERGLOW) m.tap(NOW + 7201);
    CHECK(m.state() == target);
    m.stop();
    CHECK(m.state() == State::OFF);
    CHECK(m.end() == 0);
  }

  Machine m;
  m.restore(NOW - 5, NOW);
  CHECK(m.state() == State::OFF);
  m.restore(0, NOW);
  CHECK(m.state() == State::OFF);
  m.restore(NOW + 300, NOW);
  CHECK(m.state() == State::VISIBLE);

  m.start(NOW + 900, NOW + 1);
  CHECK(m.end() == NOW + 900);
  CHECK(m.state() == State::VISIBLE);

  CHECK(shows_page(State::VISIBLE) && shows_page(State::ALARM) && shows_page(State::AFTERGLOW));
  CHECK(!shows_page(State::OFF) && !shows_page(State::SCHEDULED) && !shows_page(State::DISMISSED));
}

static long pixel_count(const std::vector<Span> &spans) {
  long n = 0;
  for (const Span &s : spans) n += s.w;
  return n;
}

static bool spans_valid(const std::vector<Span> &spans, int cx, int cy, int r) {
  for (size_t i = 0; i < spans.size(); ++i) {
    const Span &s = spans[i];
    if (s.w <= 0) return false;
    for (int x = s.x; x < s.x + s.w; ++x) {
      int dx = x - cx, dy = s.y - cy;
      if (dx * dx + dy * dy > r * r) return false;
    }
    if (i > 0 && spans[i - 1].y == s.y && spans[i - 1].x + spans[i - 1].w >= s.x) return false;  // spans on one row must not touch or overlap
  }
  return true;
}

static void test_geometry() {
  const int cx = 140, cy = 166, r = 108;
  auto full = sector_spans(cx, cy, r, 0.0, 1.0);
  CHECK(spans_valid(full, cx, cy, r));
  CHECK(full.size() == size_t(2 * r + 1));  // one span per row
  long disc = pixel_count(full);
  CHECK(std::fabs(disc - M_PI * r * r) < 2 * M_PI * r);

  CHECK(sector_spans(cx, cy, r, 0.0, 0.0).empty());
  CHECK(sector_spans(cx, cy, r, 0.5, 0.25).empty());

  auto quarter = sector_spans(cx, cy, r, 0.0, 0.25);  // top-left quadrant
  CHECK(spans_valid(quarter, cx, cy, r));
  for (const Span &s : quarter) CHECK(s.x + s.w - 1 <= cx && s.y <= cy);

  auto a = sector_spans(cx, cy, r, 0.0, 0.3);
  auto b = sector_spans(cx, cy, r, 0.3, 0.75);
  auto c = sector_spans(cx, cy, r, 0.75, 1.0);
  CHECK(pixel_count(a) + pixel_count(b) + pixel_count(c) == disc);  // no gaps, no overlap
  CHECK(spans_valid(a, cx, cy, r) && spans_valid(b, cx, cy, r) && spans_valid(c, cx, cy, r));

  auto sliver = sector_spans(cx, cy, r, 0.5, 0.5 + 1.0 / 360.0);
  CHECK(!sliver.empty());
  CHECK(spans_valid(sliver, cx, cy, r));

  Point top = on_circle(cx, cy, 100, 0.0);
  CHECK(top.x == 140 && top.y == 66);
  Point left = on_circle(cx, cy, 100, 0.25);  // counterclockwise: 15 minutes is at 9 o'clock
  CHECK(left.x == 40 && left.y == 166);
  Point bottom = on_circle(cx, cy, 100, 0.5);
  CHECK(bottom.x == 140 && bottom.y == 266);

  CHECK(fraction_for(1800) == 0.5);
  CHECK(fraction_for(-5) == 0.0);
  CHECK(fraction_for(7200) == 1.0);
}

static bool starts_with(const std::string &s, const char *prefix) { return s.rfind(prefix, 0) == 0; }

static size_t count_prefix(const std::vector<std::string> &cmds, const char *prefix) {
  size_t n = 0;
  for (const auto &c : cmds) n += starts_with(c, prefix);
  return n;
}

static View sample_view() {
  return View{LIGHT, 11130, 1350, "Bedtijd", "20:00", "19:37"};
}

static void test_render() {
  CHECK(escape_text("a\"b\\c") == "a'b/c");
  CHECK(format_mmss(1350) == "22:30");
  CHECK(format_mmss(3600) == "60:00");
  CHECK(format_mmss(5) == "0:05");
  CHECK(format_mmss(-3) == "0:00");

  std::vector<std::string> cmds;
  View v = sample_view();
  render_timer_full(v, cmds);
  CHECK(cmds.front() == "fill 0,0,480,320,65502");
  CHECK(cmds[1] == "cirs 140,166,108,61276");
  size_t sector_rows = sector_spans(CX, CY, R, 0.0, fraction_for(v.remaining_s)).size();
  CHECK(count_prefix(cmds, "fill ") == 1 + sector_rows);
  CHECK(count_prefix(cmds, "line ") == 24);  // 12 ticks, 2 px wide
  bool has_label = false, has_countdown = false, has_end = false, has_clock = false, has_zero = false, has_55 = false;
  for (const auto &c : cmds) {
    has_label |= c == "xstr 276,70,194,34,3,6338,65502,0,1,1,\"Bedtijd\"";
    has_countdown |= c == "xstr 276,112,194,80,6,11130,65502,0,1,1,\"22:30\"";
    has_end |= c == "xstr 276,196,194,24,1,23241,65502,0,1,1,\"klaar om 20:00\"";
    has_clock |= c == "xstr 344,8,100,28,2,23241,65502,2,1,1,\"19:37\"";
    has_zero |= c == "xstr 126,34,28,18,0,23241,65502,1,1,1,\"0\"";
    has_55 |= starts_with(c, "xstr ") && c.find(",\"55\"") != std::string::npos;
  }
  CHECK(has_label && has_countdown && has_end && has_clock && has_zero && has_55);
  CHECK(count_prefix(cmds, "cirs 140,166,10,14757") == 1);

  for (const auto &c : cmds) {
    if (!starts_with(c, "fill ") || c == cmds.front()) continue;
    CHECK(c.find(",1,11130") != std::string::npos);  // sector rows are 1 px high in the timer colour
  }

  cmds.clear();
  View q = v;
  q.label = "Zeg \"hoi\"";
  render_timer_full(q, cmds);
  bool escaped = false;
  for (const auto &c : cmds) escaped |= c.find("\"Zeg 'hoi'\"") != std::string::npos;
  CHECK(escaped);

  cmds.clear();
  render_sliver(v, 0.37, 0.375, cmds);
  CHECK(!cmds.empty());
  for (const auto &c : cmds) {
    if (starts_with(c, "fill ")) CHECK(c.find(",1,61276") != std::string::npos);  // painted in face colour
  }
  CHECK(count_prefix(cmds, "line ") == 24);
  CHECK(count_prefix(cmds, "cirs 140,166,10,") == 1);

  cmds.clear();
  render_countdown(v, cmds);
  CHECK(cmds.size() == 1 && cmds[0] == "xstr 276,112,194,80,6,11130,65502,0,1,1,\"22:30\"");

  cmds.clear();
  render_alarm_full(v, cmds);
  CHECK(cmds.front() == "fill 0,0,480,320,47427");
  CHECK(count_prefix(cmds, "draw ") == 2);
  bool has_title = false, has_alabel = false, has_stop = false, has_aclock = false, has_bell = false;
  for (const auto &c : cmds) {
    has_bell |= c == "xstr 0,40,480,60,10,65535,47427,1,1,1,\"\uE09D\"";  // mdi:bell-ring in NSPanel-Easy
    has_title |= c == "xstr 0,104,480,60,5,65535,47427,1,1,1,\"Tijd is om!\"";
    has_alabel |= c == "xstr 0,166,480,34,3,65535,47427,1,1,1,\"Bedtijd\"";
    has_stop |= c == "xstr 152,218,176,52,3,65535,47427,1,1,1,\"Stop\"";
    has_aclock |= c == "xstr 344,8,100,28,2,65535,47427,2,1,1,\"19:37\"";
  }
  CHECK(has_title && has_alabel && has_stop && has_aclock && has_bell);
}

static void test_format_clock() {
  CHECK(format_clock("%H:%M", 7, 5, "AM", "PM") == "07:05");
  CHECK(format_clock("%-H:%M", 7, 5, "AM", "PM") == "7:05");
  CHECK(format_clock("%-H.%M", 19, 30, "AM", "PM") == "19.30");
  CHECK(format_clock("%-I:%M %p", 19, 30, "AM", "PM") == "7:30 PM");
  CHECK(format_clock("%-I:%M %p", 0, 15, "am", "pm") == "12:15 am");
  CHECK(format_clock("%-I:%M %p", 12, 0, "AM", "PM") == "12:00 PM");
  CHECK(format_clock("%I:%M", 9, 0, "AM", "PM") == "09:00");
  CHECK(format_clock("100%% %M", 9, 0, "AM", "PM") == "100% 00");
  CHECK(format_clock("%Q", 9, 0, "AM", "PM") == "%Q");
}

int main() {
  test_resolve_end_time();
  test_colors();
  test_machine_schedule_and_visible();
  test_machine_dismiss();
  test_machine_alarm_and_afterglow();
  test_machine_stop_restore_replace();
  test_geometry();
  test_render();
  test_format_clock();
  if (failures == 0) std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
