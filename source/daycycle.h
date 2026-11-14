// Real-time day/night cycle, Pokemon HeartGold/SoulSilver style: the game follows the DS's
// real-time clock (RTC), so it is night in the game when it is night in real life.
// Portable C: the only platform call is time() (libnds fills it from the DS clock).
#ifndef DAYCYCLE_H
#define DAYCYCLE_H

#include <stddef.h>
#include <stdint.h>

// Set to 1 to preview the whole cycle on a device/emulator: the clock starts at the real time
// but then runs 1800x faster (a full day in ~48 s).
#define DAYCYCLE_PREVIEW 0

// The three periods HGSS uses, with the same boundaries:
// Morning 4:00-9:59, Day 10:00-17:59, Night 18:00-3:59.
typedef enum { TOD_MORNING, TOD_DAY, TOD_NIGHT } TimeOfDay;

// Per-channel colour multiplier, 0..256 (256 = unchanged). Daytime is {256,256,256}.
typedef struct {
    uint16_t r, g, b;
} DayTint;

void daycycle_update(void);                 // re-read the clock; cheap, call about once a second
void daycycle_set_time(int secondOfDay);    // freeze the clock at a time (0..86399); -1 = follow the RTC again

int daycycle_hour(void);                    // 0..23
int daycycle_minute(void);                  // 0..59
int daycycle_second(void);                  // 0..59
TimeOfDay daycycle_period(void);
const char *daycycle_period_name(void);     // "MORNING" / "DAY" / "NIGHT"
DayTint daycycle_tint(void);                // smoothly blended between sunrise, day, sunset and night
void daycycle_time_string(char *out, size_t outSize); // "7:42 PM"

#endif
