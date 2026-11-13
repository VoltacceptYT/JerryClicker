# JerryClicker NDS Port

This project turns the existing JerryClicker web game into a Nintendo DS build using the repository's real sprite assets.

What is included:
- A libnds-based project skeleton for the DS
- A sprite conversion tool that reads the actual PNGs from this repository
- A playable DS-style clicker loop with Jerry, upgrades, and Golden Jerry events
- A build process that regenerates the sprite header before compilation

Structure:
- `nds/Makefile` — build configuration
- `nds/source/main.cpp` — DS game logic and rendering
- `nds/tools/convert_sprites.py` — converts repo PNGs into generated C sprite data
- `nds/generated/` — generated output created during the build

Requirements:
- devkitPro / libnds installed at `/opt/devkitpro` or adjust `DEVKITPRO` in the Makefile
- Python 3 with Pillow (`python3 -m pip install pillow`)

Build:

```bash
cd nds
make
```

The build will automatically run the sprite conversion script, read the source PNGs from the repo, and generate `generated/sprites.h` before compiling.

Run:
- Use an emulator like melonDS, DeSmuME, or a real Nintendo DS.

Notes:
- This project uses the repo's real Jerry assets rather than custom placeholders.
- The DS app keeps the clicker loop lightweight and portable: tap Jerry to gain candies, buy upgrades, and catch Golden Jerry for a burst of currency.
