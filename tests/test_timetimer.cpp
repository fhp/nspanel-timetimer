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

int main() {
  test_resolve_end_time();
  test_colors();
  if (failures == 0) std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
