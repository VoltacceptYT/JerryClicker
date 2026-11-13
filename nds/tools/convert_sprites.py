#!/usr/bin/env python3
"""Convert repository PNG sprites to a compact DS RGB15 bitmap C header."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required: python3 -m pip install Pillow") from exc

ASSET_DIRS = (
    "assets/img/cosmetics",
    "assets/img/powerups",
    "assets/img/ui",
    "api/resources",
)


def symbol_for(relative: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", relative).lower()


def rgb15a(r: int, g: int, b: int, a: int) -> int:
    # Bit 15 enables an opaque pixel in a DS 16-bit bitmap; zero means transparent.
    if a < 128:
        return 0
    return 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo_root", type=Path)
    parser.add_argument("--out", type=Path, default=Path("nds/generated"))
    args = parser.parse_args()

    root = args.repo_root.resolve()
    out = args.out.resolve() if args.out.is_absolute() else (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    entries = []
    used_symbols = set()

    for folder in ASSET_DIRS:
        asset_dir = root / folder
        if not asset_dir.exists():
            continue
        for path in sorted(asset_dir.rglob("*.png")):
            rel = path.relative_to(root).as_posix()
            symbol = symbol_for(rel)
            if symbol in used_symbols:
                raise SystemExit(f"Sprite symbol collision: {rel} -> {symbol}")
            used_symbols.add(symbol)
            with Image.open(path) as source:
                image = source.convert("RGBA")
                width, height = image.size
                pixels = [rgb15a(*image.getpixel((x, y))) for y in range(height) for x in range(width)]
            entries.append({"path": rel, "symbol": symbol, "width": width, "height": height, "pixels": pixels})

    if not entries:
        raise SystemExit(f"No sprite PNG files found under {', '.join(ASSET_DIRS)}")

    lines = [
        "#ifndef JERRY_NDS_SPRITES_H",
        "#define JERRY_NDS_SPRITES_H",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "typedef struct { const char* path; int width; int height; const uint16_t* pixels; } JerrySprite;",
    ]
    for item in entries:
        lines.append(f"static const uint16_t {item['symbol']}[{len(item['pixels'])}] = {{")
        for start in range(0, len(item["pixels"]), 16):
            values = item["pixels"][start : start + 16]
            lines.append("    " + ", ".join(f"0x{value:04X}" for value in values) + ",")
        lines.extend(["};", ""])
    lines.append("static const JerrySprite jerry_sprites[] = {")
    for item in entries:
        lines.append(
            f'    {{ "{item["path"]}", {item["width"]}, {item["height"]}, {item["symbol"]} }},'
        )
    lines.extend([
        "};",
        "static const size_t jerry_sprite_count = sizeof(jerry_sprites) / sizeof(jerry_sprites[0]);",
        "#endif",
        "",
    ])
    (out / "sprites.h").write_text("\n".join(lines), encoding="utf-8")
    manifest = [{k: item[k] for k in ("path", "symbol", "width", "height")} for item in entries]
    (out / "sprite_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Converted {len(entries)} actual repository sprites into {out / 'sprites.h'}")


if __name__ == "__main__":
    main()
