"""Build the Minnka boot logo as a colour-faithful Mega Drive tile bank.

The source is already composed at the correct cinematic aspect ratio, so the
whole image is retained.  A deliberately authored palette protects the logo's
cream-to-white lettering, indigo flame and orange core from median-cut palette
collapse.
"""

from pathlib import Path
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "minnka_logo.png"
PREVIEW = ROOT / "assets" / "minnka_logo_genesis_preview.png"
OUTPUT = ROOT / "src" / "logo_data.c"
HEADER = ROOT / "src" / "logo_data.h"

WIDTH = 240
HEIGHT = 104
TILES_W = WIDTH // 8
TILES_H = HEIGHT // 8

# One hardware palette, apportioned by subject rather than frequency.  The old
# automatic palette used ten near-black entries and left no orange ramp or
# separate cream/white lettering.  These colours follow the source artwork's
# actual sampled ranges and remain distinct after VDP conversion.
PALETTE = [
    (0, 0, 0),
    (255, 255, 255), (216, 216, 216),
    (184, 168, 120), (120, 104, 80),
    (72, 88, 144), (64, 80, 128), (48, 64, 112),
    (32, 48, 96), (24, 32, 72), (16, 16, 40), (8, 8, 24),
    (232, 96, 16), (208, 72, 16), (176, 48, 0), (112, 24, 0),
]


def indexed_logo():
    source = Image.open(SOURCE).convert("RGB")
    image = source.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)

    palette_image = Image.new("P", (1, 1))
    flat = [channel for colour in PALETTE for channel in colour]
    palette_image.putpalette(flat + [0] * (768 - len(flat)))
    indexed = image.quantize(palette=palette_image, dither=Image.Dither.NONE)

    # Do not let filtered near-black source pixels become a noisy halo around
    # the artwork.  True coloured shadows remain available in the indigo ramp.
    source_px = image.load()
    indexed_px = indexed.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if max(source_px[x, y]) < 10:
                indexed_px[x, y] = 0

    indexed.putpalette(flat + [0] * (768 - len(flat)))
    return indexed


def build_tiles(image):
    pixels = image.load()
    tiles = []
    tile_ids = {}
    tilemap = []

    for tile_y in range(TILES_H):
        row = []
        for tile_x in range(TILES_W):
            packed = []
            for y in range(8):
                value = 0
                for x in range(8):
                    value = (value << 4) | pixels[tile_x * 8 + x,
                                                  tile_y * 8 + y]
                packed.append(value)
            key = tuple(packed)
            if key not in tile_ids:
                tile_ids[key] = len(tiles)
                tiles.append(key)
            row.append(tile_ids[key])
        tilemap.append(row)
    return tiles, tilemap


def generated_c(tiles, tilemap):
    lines = [f"static const u32 tile_logo[{len(tiles)}][8] = {{"]
    for tile in tiles:
        values = ", ".join(f"0x{value:08X}" for value in tile)
        lines.append(f"    {{ {values} }},")
    lines.extend(["};", "",
                  f"static const u16 logo_tilemap[{TILES_H}][{TILES_W}] = {{"])
    for row in tilemap:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "", "static const u16 pal_logo[16] = {"])
    for start in range(0, 16, 4):
        colours = ", ".join(
            f"RGB24_TO_VDPCOLOR(0x{r:02X}{g:02X}{b:02X})"
            for r, g, b in PALETTE[start:start + 4])
        lines.append(f"    {colours},")
    lines.append("};")
    return "\n".join(lines)


def main():
    image = indexed_logo()
    tiles, tilemap = build_tiles(image)
    image.save(PREVIEW)

    source = OUTPUT.read_text()
    pattern = re.compile(
        r"static const u32 tile_logo\[.*?\n\};\n\n"
        r"static const u16 logo_tilemap\[.*?\n\};\n\n"
        r"static const u16 pal_logo\[16\] = \{.*?\n\};",
        re.DOTALL)
    replacement = generated_c(tiles, tilemap)
    source, count = pattern.subn(replacement, source, count=1)
    if count != 1:
        raise RuntimeError("Could not locate generated logo blocks in logo_data.c")
    OUTPUT.write_text(source)

    header = HEADER.read_text()
    header = re.sub(r"#define LOGO_TILE_COUNT\s+\d+",
                    f"#define LOGO_TILE_COUNT  {len(tiles)}", header)
    header = re.sub(r"#define LOGO_TILES_W\s+\d+",
                    f"#define LOGO_TILES_W     {TILES_W}", header)
    header = re.sub(r"#define LOGO_TILES_H\s+\d+",
                    f"#define LOGO_TILES_H     {TILES_H}", header)
    HEADER.write_text(header)
    print(f"Wrote {len(tiles)} Minnka tiles, {TILES_W}x{TILES_H} tilemap and {PREVIEW.name}")


if __name__ == "__main__":
    main()
