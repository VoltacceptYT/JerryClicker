# Jerry Clicker — Nintendo DS

A native libnds port of the browser clicker, built with devkitPro's supported `ds_rules` make rules. The port uses the original repository PNG sprites: Jerry and cosmetic variants, Golden Jerry, upgrade icons, candy, and UI buttons.

## Build toolchain

Tested configuration target: devkitPro/devkitARM + libnds (ARM9). Install Python 3 and Pillow as well:

```sh
python3 -m pip install Pillow
```

Set `DEVKITPRO` if it is not `/opt/devkitpro`, then build from this directory:

```sh
make clean
make
```

The build runs `tools/convert_sprites.py` to generate `generated/sprites.h`, compiles the ARM9 application with `$(DEVKITARM)/ds_rules`, and emits `jerryclicker.nds`. If the sprite conversion dependency is unavailable, install Pillow in the Python environment used by `make`.

## Controls

- Touch Jerry on the top screen or press **A** to earn candies.
- Tap Golden Jerry when it appears for a random windfall, frenzy, or lucky bonus.
- On the lower screen, tap an upgrade row to purchase it. Use **L/R** to change pages and **X** to buy the selected row.
- The game includes the browser game's upgrade costs/effects, auto-generation, cosmetic selection, prestige unlock, signature multiplier, and prestige reset progression.

## Notes

The game is a native port, not a browser wrapper. The sprite converter preserves alpha as the DS bitmap transparency bit and reduces source colors to DS RGB15. Generated output is ignored by git and recreated by `make`. The ARM9 build needs no filesystem at runtime.
