// Background music: a looping hardware-ADPCM voice played straight from the ROM image.
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

void audio_init(void);          // power up sound and start the looping music
bool audio_music_toggle(void);  // mute/unmute (keeps playing silently so it resumes in time); returns true if now audible
bool audio_music_playing(void); // false if no sound channel could be started

#endif
