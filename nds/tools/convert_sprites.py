#!/usr/bin/env python3
"""Convert JerryClicker PNG assets into a C header for the Nintendo DS build.

This script reads the repository's real sprite PNG files and emits a generated
header with raw RGBA pixel data. The DS project then renders those exact sprites
without painting placeholder art.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable, List, Tuple

from PIL import Image

DEFAULT_GLOBS = (
    "assets/img/cosmetics/*.png",
    "assets/img/powerups/*.png",
    "assets/img/ui/*.png",
    "api/resources/*.png",
)


def sanitize_name(name: str) -> str:
    sanitized = []
    for ch in name:
        if ch.isalnum() or ch == "_":
            sanitized.append(ch)
        else:
            sanitized.append("_")
    result = "".join(sanitized)
    if not result:
        result = "sprite"
    if result[0].isdigit():
        result = "sprite_" + result
    return result.lower()


def rgba_to_u32(r: int, g: int, b: int, a: int) -> int:
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)


def iter_png_files(repo_root: Path) -> Iterable[Path]:
    seen = set()
    for pattern in DEFAULT_GLOBS:
        for path in sorted((repo_root / pattern).parent.glob(pattern.split("/")[-1])):
            if path.suffix.lower() != ".png":
                continue
            if path not in seen:
                seen.add(path)
                yield path


def build_header(manifest: List[dict]) -> str:
    lines: List[str] = []
    lines.append("#ifndef JERRY_NDS_SPRITES_H")
    lines.append("#define JERRY_NDS_SPRITES_H")
    lines.append("")
    lines.append("#include <stddef.h>")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    const char* name;")
    lines.append("    int width;")
    lines.append("    int height;")
    lines.append("    const uint32_t* pixels;")
    lines.append("} SpriteMeta;")
    lines.append("")

    for entry in manifest:
        symbol = entry["symbol"]
        pixels = entry["pixels"]
        lines.append(f"static const uint32_t {symbol}_pixels[{len(pixels)}] = {{")
        for i in range(0, len(pixels), 12):
            chunk = pixels[i : i + 12]
            lines.append("    " + ", ".join(str(v) for v in chunk) + ",")
        lines.append("};")
        lines.append("")
        lines.append(f"static const SpriteMeta {symbol}_meta = {{ \"{entry['source_name']}\", {entry['width']}, {entry['height']}, {symbol}_pixels }};")
        lines.append("")

    lines.append("static const SpriteMeta sprite_list[] = {")
    for entry in manifest:
        lines.append(f"    {entry['symbol']}_meta,")
    lines.append("};")
    lines.append("")
    lines.append("static const size_t sprite_count = sizeof(sprite_list) / sizeof(sprite_list[0]);")
    lines.append("")
    lines.append("#endif")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert JerryClicker PNG files into NDS sprite C data")
    parser.add_argument("repo_root", type=Path, help="Repository root containing the original JerryClicker assets")
    parser.add_argument("--out", type=Path, default=Path("build/generated"), help="Output folder for generated sprite data")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    out_dir = args.out.resolve() if args.out.is_absolute() else (repo_root / args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    manifest: List[dict] = []
    for path in iter_png_files(repo_root):
        with Image.open(path) as image:
            rgba = image.convert("RGBA")
            width, height = rgba.size
            pixels: List[int] = []
            for y in range(height):
                for x in range(width):
                    r, g, b, a = rgba.getpixel((x, y))
                    pixels.append(rgba_to_u32(r, g, b, a))

        source_name = path.relative_to(repo_root).as_posix()
        symbol = sanitize_name(path.stem)
        entry = {
            "symbol": symbol,
            "source_name": source_name,
            "width": width,
            "height": height,
            "pixels": pixels,
        }
        manifest.append(entry)

    header = build_header(manifest)
    (out_dir / "sprites.h").write_text(header, encoding="utf-8")

    manifest_json = [
        {
            "name": item["source_name"],
            "symbol": item["symbol"],
            "width": item["width"],
            "height": item["height"],
        }
        for item in manifest
    ]
    (out_dir / "sprite_manifest.json").write_text(json.dumps(manifest_json, indent=2), encoding="utf-8")

    print(f"Generated {len(manifest)} sprites into {out_dir}")


if __name__ == "__main__":
    main()
