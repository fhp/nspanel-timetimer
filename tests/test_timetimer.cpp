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

  r = resolve_end_time("19:00", NOW, NOW_SOD);  // current minute: tomorrow, too far ahead
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
  CHECK(m.should_beep(NOW + 13));
  CHECK(m.should_beep(NOW + 37));
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

int main() {
  test_resolve_end_time();
  test_colors();
  test_machine_schedule_and_visible();
  test_machine_dismiss();
  test_machine_alarm_and_afterglow();
  test_machine_stop_restore_replace();
  if (failures == 0) std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
