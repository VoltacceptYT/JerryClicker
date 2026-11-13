# JerryClicker NDS Port

This directory contains a Nintendo DS port of the Jerry Clicker concept using the sprite assets already present in this repository as the source of truth.

## What this port does

- Uses the existing JerryClicker art as the basis for the handheld version
- Converts the repo’s sprite PNG files into NDS-friendly raw sprite data via a build script
- Keeps the gameplay loop simple and portable: tap Jerry to gain candies, buy upgrades, and collect a Golden Jerry bonus
- Uses the actual sprites from the repo instead of drawing new placeholder art

## Source assets used

The conversion script reads from the original repo assets:

- `assets/img/cosmetics/jerry.png`
- `assets/img/cosmetics/angel_jerry.png`
- `assets/img/cosmetics/iron_jerry.png`
- `assets/img/cosmetics/lord_jerry.png`
- `assets/img/cosmetics/scuba_jerry.png`
- `assets/img/powerups/golden_jerry.png`
- `api/resources/jerry_candy.png`
- `assets/img/ui/button.png`

## Build prerequisites

Install devkitPro / libnds:

- https://devkitpro.org/

Then install Python dependencies for the build script:

```bash
python3 -m pip install pillow
```

## Convert the sprites

From the repository root:

```bash
cd nds
python3 tools/convert_sprites.py ../assets ../api/resources
```

This produces a `build/generated` folder containing the sprite data for the DS project.

## Compile

```bash
make
```

Then run the resulting ROM in a DS emulator such as DeSmuME, melonDS, or an actual Nintendo DS console.

## Notes

The DS app is intentionally small and efficient, matching the original clicker loop while keeping the project portable. The actual repo PNGs are preserved and converted at build time, so any new sprite work in the browser repo can be re-exported into the DS build by rerunning the conversion script.
