# Jerry Clicker (Nintendo DS)

Port of [JerryClicker](https://github.com/VoltacceptYT/JerryClicker/) to the Nintendo DS using devkitARM + libnds.
The original sprites are used for Jerry, the power-up, the floating candy and every upgrade icon.
The UI uses the original Jerrified Pixels font and the original UI sprites (stone background, buttons, locked overlay), software-rendered on both screens.

> Looking for another version? See the [`main` branch](https://github.com/VoltacceptYT/JerryClicker) for the table of contents

## Controls
| Input | Action |
|---|---|
| Touch Jerry / **B** | Click |
| **Up/Down** | Select upgrade (Upgrades page) |
| **L/R** | Switch page (Upgrades / Jerry Isles) |
| **A** | Buy selected upgrade (Upgrades page) |
| **Select** | Music on/off |
| **Start** or touch `[PRESTIGE]` | Prestige (confirm with A/B or touch YES/NO) |
| Touch `[SAVE]` | Save now (also autosaves every 30 s) |
| Touch Golden Jerry | Random bonus |

Saves go to `JerryClicker.sav` next to the ROM on a flashcart/SD (needs DLDI/FAT). Without storage the game still runs, it just can't save.

## Toy store
On the Jerry Isles page the whole bottom screen is the **Jerry Toys** store: a 3x2 grid of shop windows (touch a toy, or D-pad + **A**, to buy it; the SAVE / PRESTIGE buttons sit on the shop counter). You can own up to 4 of each toy and they are kept across prestige.

| Toy | Price | What it does on the island |
|---|---|---|
| Beach Ball | 250 | Bounces around; critters kick it |
| Plastic Shovel | 500 | Critters build sandcastles |
| Squeaky Mouse | 750 | Scurries about on its own; critters chase it |
| Magic 8-Ball | 1.5K | Heavy: barely bounces, rolls a long way |
| Balloon Snake | 3K | Floats above the island and drifts on the breeze |
| White Dice | 6K | Critters roll it for D&D: the d20 result pops up when it lands (gold on a 20, red on a 1) |

Toy textures: `assets/toys/` (the mouse, 8-ball, balloon snake and white dice come from the Skyblock texture pack). To add another toy, add its PNG, append it to `TOY_IDS` in `tools/convert_assets.py` and to `TOYS[]` / the enum in `source/game.h`+`game.c`, then run the converter.

## Jerry Isles
The second top-screen page is a tiled island (autotiled from `assets/island/island_tiles.png`: sea, sand, grass, palms, rocks, shells) filling the whole screen under the header. Every 5 purchases of an upgrade add one wandering critter wearing that upgrade's icon (up to 48 at once, shared fairly between upgrades). Critters stay on land, walk around palms, rocks and the bench, drift toward the parasol and palms, bump into each other, and greet each other (hearts for the same upgrade, a note for different ones); sometimes one follows the other around. They disappear on prestige along with the purchases.

## Day/night cycle
Like Pokemon HeartGold/SoulSilver, the Jerry Isles follow the DS's real-time clock, so it is evening on the island when it is evening in real life. The periods use HGSS's boundaries (**Morning** 4:00-9:59, **Day** 10:00-17:59, **Night** 18:00-3:59) and the time of day is painted on the bench at the bottom of the Isles page. Instead of snapping between three looks, the light blends smoothly through lilac first light, a peach sunrise, full daylight, an orange sunset, purple dusk and a blue night (the keyframes are the `KEYS` table in `source/daycycle.c`).
The terrain is re-tinted only when the light has shifted visibly (every few minutes at dawn/dusk, never in full daylight) and sprites are tinted as they are drawn, so it costs almost nothing. Set the clock in the DS settings (or your emulator) to change the time. To preview the whole cycle, set `DAYCYCLE_PREVIEW` to `1` in `source/daycycle.h`: the clock then runs 1800x faster (a full day in ~48 s).

## Tested Devices/Emulators
| Device/Emu | Tested | Tester |
|---|---|---|
| DS Lite | Works | VoltacceptYT |
| MelonDS | Works | Emarxian |
| DeSmuME | Failed | Emarxian |


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
- **Sprites:** Mojang / VoltacceptYT / ic22487 (Modrinth)
- **Tiles:** Wabby Wabbo (Itch.io)
- **Music:** VO!DWiNG (Discord)

### Other Thanks
- **devkitPro** - The Compiling Tools
- **Hypixel** - Creating Jerry Himself
