// Background music for Jerry Clicker.
//
// The track is stored in the DS sound hardware's native IMA-ADPCM format (see
// tools/convert_music.py), embedded in the ROM by music_data.s and played on one
// hardware voice with libnds' soundPlaySample(). Playback is done entirely by the
// ARM7 sound hardware, so it costs the ARM9 (which software-renders both screens)
// nothing and needs no special ARM7 core / maxmod soundbank.
#include <nds.h>

#include "audio.h"
#include "music_info.h"

extern const u8 jerry_music_adpcm[];
extern const u8 jerry_music_adpcm_end[];

#define MUSIC_VOLUME 100 // 0..127
#define MUSIC_PAN 64     // centre

static int musicCh = -1;
static bool audible = true;

void audio_init(void) {
    soundEnable();

    const u32 bytes = (u32)(jerry_music_adpcm_end - jerry_music_adpcm);

    // The ARM7 reads the sample straight from main RAM: make sure no stale cache lines hide it.
    DC_FlushRange(jerry_music_adpcm, bytes);

    // The hardware length register counts the words *after* the loop point, and libnds forwards
    // dataSize/4 to it unchanged, so subtract the loop-start region or the voice would read
    // MUSIC_LOOP_WORD words past the end of the stream on every repeat.
    musicCh = soundPlaySample(jerry_music_adpcm, SoundFormat_ADPCM, bytes - MUSIC_LOOP_WORD * 4,
                              MUSIC_RATE, MUSIC_VOLUME, MUSIC_PAN, true, MUSIC_LOOP_WORD);
}

bool audio_music_playing(void) { return musicCh >= 0; }

bool audio_music_toggle(void) {
    audible = !audible;
    // Mute through the volume (not pause): resuming a hardware voice restarts it from the beginning.
    if (musicCh >= 0) soundSetVolume(musicCh, audible ? MUSIC_VOLUME : 0);
    return audible;
}
