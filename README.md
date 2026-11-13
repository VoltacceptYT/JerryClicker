# Jerry Clicker (Nintendo DS)

Port of [JerryClicker](https://github.com/) to the Nintendo DS using devkitARM + libnds.
The original sprites are used for Jerry, the power-up, the floating candy and every upgrade icon.
The UI uses the original Jerrified Pixels font and the original UI sprites (stone background, buttons, locked overlay), software-rendered on both screens.

## Controls
| Input | Action |
|---|---|
| Touch Jerry / **B** | Click |
| **Up/Down**, **L/R** | Select upgrade / page |
| **A** | Buy selected upgrade |
| **Select** | Music on/off |
| **Start** or touch `[PRESTIGE]` | Prestige (confirm with A/B or touch YES/NO) |
| Touch `[SAVE]` | Save now (also autosaves every 30 s) |
| Touch Golden Jerry | Random bonus |

Saves go to `JerryClicker.sav` next to the ROM on a flashcart/SD (needs DLDI/FAT). Without storage the game still runs, it just can't save.

## Building
Push to GitHub; the workflow in `.github/workflows/build.yml` builds `JerryClicker.nds` and uploads it as an artifact.
Locally: `make` with devkitARM and `nds-dev` installed.

To change sprites or the font, edit `assets/` and run `python3 tools/convert_assets.py` (needs Pillow); it regenerates `source/sprites.c` and `source/gfxdata.c`.

Upgrade prices grow by `COST_GROWTH` (1.15, in `source/game.h`) per purchase.

## Music
`assets/music/JerryUnited.mp3` is converted to the DS hardware's native IMA-ADPCM format (mono, 22.05 kHz, ~1.4 MiB) and embedded in the ROM via `source/music_data.s`. It loops on one ARM7 sound voice through `soundPlaySample`, so it costs the ARM9 nothing and needs no maxmod/soundbank.
To change the track or quality: `python3 tools/convert_music.py --rate 22050` (needs ffmpeg + numpy; 16000 = smaller, 32000 = better but ~2.2 MiB). The generated `.adpcm` and `source/music_info.h` are committed, so normal builds need neither.
