# Jerry Clicker (Nintendo DS)

Port of [JerryClicker](https://github.com/VoltacceptYT/JerryClicker/) to the Nintendo DS using devkitARM + libnds.
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

## Tested Devices/Emulators
| Device/Emu | Tested | Tester |
|---|---|---|
| DS Lite | Works | VoltacceptYT |


Any Device **NOT** listed hasn't been tested yet 

## Building
Push to GitHub; the workflow in `.github/workflows/Build.yml` converts the graphics and music and builds `JerryClicker.nds` and uploads it as an artifact.
Locally: run `python3 tools/convert_music.py` once (see Music), then `make` with devkitARM and `nds-dev` installed.

To change sprites or the font, edit `assets/` (buttons use `ui/<name>.png` plus a `ui/<name>_active.png` pressed variant) and run `python3 tools/convert_assets.py` (needs Pillow); it regenerates `source/sprites.c`, `source/gfxdata.c` and `icon.bmp`. The workflow runs it too, and the generated files stay committed so local builds work without it.

Upgrade prices grow by `COST_GROWTH` (1.15, in `source/game.h`) per purchase.

## Music
`assets/music/JerryUnited.mp3` is converted to the DS hardware's native IMA-ADPCM format (mono, 22.05 kHz, ~1.4 MiB) and embedded in the ROM via `source/music_data.s`. It loops on one ARM7 sound voice through `soundPlaySample`, so it costs the ARM9 nothing and needs no maxmod/soundbank.
To change the track or quality: `python3 tools/convert_music.py --rate 22050` (needs ffmpeg + numpy; 16000 = smaller, 32000 = better but ~2.2 MiB). The generated `.adpcm` and `source/music_info.h` are not committed: the GitHub Actions workflow runs the converter before `make`, and local builds need to run it once first.

## Credits
- **Game:** VoltacceptYT
- **Sprites:** Mojang / VoltacceptYT
- **Music:** VO!DWiNG

### Other Thanks
- **devkitPro** - The Compiling Tools
- **Hypixel** - Creating Jerry Himself
