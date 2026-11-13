# Jerry Clicker (Nintendo DS)

Port of [JerryClicker](https://github.com/) to the Nintendo DS using devkitARM + libnds.
The original sprites are used for Jerry, the power-up, the floating candy and every upgrade icon.
All UI (panels, buttons, stats) is plain text drawn with the built-in console font.

## Controls
| Input | Action |
|---|---|
| Touch Jerry / **B** | Click |
| **Up/Down**, **L/R** | Select upgrade / page |
| **A** | Buy selected upgrade |
| **Start** or touch `[PRESTIGE]` | Prestige (confirm with A/B or touch YES/NO) |
| Touch `[SAVE]` | Save now (also autosaves every 30 s) |
| Touch Golden Jerry | Random bonus |

Saves go to `JerryClicker.sav` next to the ROM on a flashcart/SD (needs DLDI/FAT). Without storage the game still runs, it just can't save.

## Building
Push to GitHub; the workflow in `.github/workflows/build.yml` builds `JerryClicker.nds` and uploads it as an artifact.
Locally: `make` with devkitARM and `nds-dev` installed.

To change sprites, edit `assets/` and run `python3 tools/convert_assets.py` (needs Pillow); it regenerates `source/sprites.c`.

Upgrade prices grow by `COST_GROWTH` (1.15, in `source/game.h`) per purchase.
