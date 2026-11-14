#include "daycycle.h"

#include <stdio.h>
#include <time.h>

static int secOfDay = 12 * 3600;  // until the first update
static int override = -1;         // >= 0: clock frozen here (used for testing)

#if DAYCYCLE_PREVIEW
static int previewSecs = -1;
#endif

void daycycle_update(void) {
    if (override >= 0) {
        secOfDay = override;
        return;
    }
#if DAYCYCLE_PREVIEW
    if (previewSecs < 0) {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        previewSecs = lt ? lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec : 12 * 3600;
    } else {
        previewSecs = (previewSecs + 1800) % 86400;
    }
    secOfDay = previewSecs;
#else
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);  // the DS RTC holds local time, so there is no timezone to apply
    if (lt) secOfDay = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
#endif
}

void daycycle_set_time(int s) { override = s < 0 ? -1 : s % 86400; }

int daycycle_hour(void) { return secOfDay / 3600; }
int daycycle_minute(void) { return (secOfDay / 60) % 60; }
int daycycle_second(void) { return secOfDay % 60; }

TimeOfDay daycycle_period(void) {
    int h = daycycle_hour();
    if (h >= 4 && h < 10) return TOD_MORNING;
    if (h >= 10 && h < 18) return TOD_DAY;
    return TOD_NIGHT;
}

const char *daycycle_period_name(void) {
    switch (daycycle_period()) {
    case TOD_MORNING: return "MORNING";
    case TOD_DAY: return "DAY";
    default: return "NIGHT";
    }
}

// Colour keyframes across the day. The tint is blended linearly between neighbouring keys, so
// dusk and dawn fade in gradually instead of the screen snapping between three looks.
typedef struct {
    uint16_t minute;  // minutes since midnight
    DayTint tint;
} Key;

static const Key KEYS[] = {
    {0 * 60, {84, 100, 172}},         // deep night
    {4 * 60 + 30, {84, 100, 172}},    // night until just before dawn
    {6 * 60, {190, 160, 190}},        // first light: lilac
    {7 * 60 + 30, {256, 226, 200}},   // sunrise: warm peach
    {9 * 60 + 30, {256, 256, 256}},   // full day
    {16 * 60 + 30, {256, 256, 256}},  // full day
    {18 * 60, {256, 196, 140}},       // sunset: orange
    {19 * 60 + 30, {150, 120, 170}},  // dusk: purple
    {21 * 60, {84, 100, 172}},        // night
    {24 * 60, {84, 100, 172}},        // wraps to the first key
};
#define KEY_COUNT ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

static uint16_t lerp(uint16_t a, uint16_t b, int num, int den) {
    return (uint16_t)(a + ((int)b - (int)a) * num / den);
}

DayTint daycycle_tint(void) {
    int t = secOfDay;  // seconds
    for (int i = 0; i + 1 < KEY_COUNT; i++) {
        int t0 = KEYS[i].minute * 60, t1 = KEYS[i + 1].minute * 60;
        if (t >= t0 && t < t1) {
            DayTint o;
            o.r = lerp(KEYS[i].tint.r, KEYS[i + 1].tint.r, t - t0, t1 - t0);
            o.g = lerp(KEYS[i].tint.g, KEYS[i + 1].tint.g, t - t0, t1 - t0);
            o.b = lerp(KEYS[i].tint.b, KEYS[i + 1].tint.b, t - t0, t1 - t0);
            return o;
        }
    }
    return KEYS[0].tint;
}

void daycycle_time_string(char *out, size_t outSize) {
    int h = daycycle_hour();
    int h12 = h % 12 == 0 ? 12 : h % 12;
    snprintf(out, outSize, "%d:%02d %s", h12, daycycle_minute(), h < 12 ? "AM" : "PM");
}
