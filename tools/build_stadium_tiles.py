"""Convert the authored stadium source into a 320x224 Mega Drive tilemap.

The perspective-preserving vertical squeeze gives most of the screen to the
court while retaining the far grandstand and a thin near crowd foreground.
The output is deterministic and uses one fixed 16-colour VDP palette.
"""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "stadium_source_v1.png"
PREVIEW = ROOT / "assets" / "stadium_genesis_preview.png"
OUTPUT = ROOT / "src" / "stadium_tiles.inc"

PALETTE = [
    (0, 0, 0), (248, 248, 240), (66, 184, 80), (39, 132, 60),
    (8, 24, 48), (64, 88, 120), (240, 192, 40), (53, 160, 72),
    (16, 24, 40), (216, 56, 56), (240, 200, 56), (56, 112, 200),
    (88, 216, 240), (120, 136, 152), (40, 48, 56), (248, 248, 248),
]


def nearest_colour(rgb):
    return min(range(16), key=lambda i: sum((rgb[c] - PALETTE[i][c]) ** 2 for c in range(3)))


def prepare_image():
    source = Image.open(SOURCE).convert("RGB")
    target_ratio = 10 / 7
    if source.width / source.height > target_ratio:
        crop_w = round(source.height * target_ratio)
        left = (source.width - crop_w) // 2
        source = source.crop((left, 0, left + crop_w, source.height))
    else:
        crop_h = round(source.width / target_ratio)
        top = (source.height - crop_h) // 2
        source = source.crop((0, top, source.width, top + crop_h))

    split = round(source.height * 0.70)
    far_pitch = source.crop((0, 0, source.width, split)).resize((160, 96), Image.Resampling.LANCZOS)
    foreground = source.crop((0, split, source.width, source.height)).resize((160, 16), Image.Resampling.LANCZOS)
    small = Image.new("RGB", (160, 112))
    small.paste(far_pitch, (0, 0))
    small.paste(foreground, (0, 96))
    image = small.resize((320, 224), Image.Resampling.NEAREST)

    # The playable court is authored from the same projection used by the
    # simulation (depth = y - x/4).  Keeping these points here in lock-step
    # with game_state.h means a player's feet can never visibly cross a line
    # while the collision code still thinks they are in bounds.
    draw = ImageDraw.Draw(image)

    def edge_x(depth, right=False):
        return (312 if right else 64) - ((depth - 24) // 2)

    def point(depth, right=False):
        x = edge_x(depth, right)
        return (x, depth + x // 4)

    far_l, far_r = point(24), point(24, True)
    near_l, near_r = point(144), point(144, True)

    # Clean, readable striped turf replaces the old football-box markings.
    # Bands follow court depth, preserving the isometric perspective.
    draw.polygon([far_l, far_r, near_r, near_l], fill=PALETTE[3])
    for depth in range(24, 144, 15):
        next_depth = min(depth + 15, 144)
        colour = PALETTE[2] if ((depth - 24) // 15) % 2 == 0 else PALETTE[7]
        draw.polygon([
            point(depth), point(depth, True),
            point(next_depth, True), point(next_depth)
        ], fill=colour)

    # Sparse checker highlights add 16-bit texture without noisying the play.
    for depth in range(31, 141, 16):
        left = edge_x(depth)
        right = edge_x(depth, True)
        for x in range(left + 13, right - 8, 32):
            y = depth + x // 4
            draw.rectangle((x, y, x + 7, y + 2), fill=PALETTE[3])

    # Strong double-edged boundary, exactly on the movement polygon.
    boundary = [far_l, far_r, near_r, near_l, far_l]
    draw.line(boundary, fill=PALETTE[4], width=4, joint="curve")
    draw.line(boundary, fill=PALETTE[15], width=2, joint="curve")

    # Clear centre board: glass-blue panels with a bright top rail, base rail,
    # and visible posts. It separates the teams without becoming a solid wall.
    board_depth = 84
    base_l, base_r = point(board_depth), point(board_depth, True)
    top_l = (base_l[0], base_l[1] - 12)
    top_r = (base_r[0], base_r[1] - 12)
    draw.polygon([top_l, top_r, base_r, base_l], fill=(40, 48, 56))
    for i in range(1, 16, 2):
        t = i / 16
        x = round(top_l[0] + (top_r[0] - top_l[0]) * t)
        y_top = round(top_l[1] + (top_r[1] - top_l[1]) * t)
        y_base = round(base_l[1] + (base_r[1] - base_l[1]) * t)
        draw.line((x, y_top + 2, x, y_base - 2), fill=PALETTE[11], width=1)
    for i in range(5):
        t = i / 4
        x = round(base_l[0] + (base_r[0] - base_l[0]) * t)
        y_base = round(base_l[1] + (base_r[1] - base_l[1]) * t)
        draw.line((x + 1, y_base - 13, x + 1, y_base + 3), fill=PALETTE[4], width=3)
        draw.line((x, y_base - 13, x, y_base + 2), fill=PALETTE[12], width=1)
    for segment in ((top_l, top_r), (base_l, base_r)):
        draw.line(segment, fill=PALETTE[4], width=4)
        draw.line(segment, fill=PALETTE[12], width=2)
        draw.line((segment[0][0], segment[0][1] - 1,
                   segment[1][0], segment[1][1] - 1), fill=PALETTE[15], width=1)

    return image


def main():
    image = prepare_image()
    indexed = Image.new("P", image.size)
    flat_palette = [channel for colour in PALETTE for channel in colour] + [0] * (768 - 48)
    indexed.putpalette(flat_palette)
    indexed.putdata([nearest_colour(pixel) for pixel in image.getdata()])
    indexed.save(PREVIEW)

    unique = []
    tile_ids = {}
    tilemap = []
    pixels = indexed.load()
    for ty in range(28):
        row = []
        for tx in range(40):
            tile = tuple(pixels[tx * 8 + x, ty * 8 + y] for y in range(8) for x in range(8))
            if tile not in tile_ids:
                tile_ids[tile] = len(unique)
                unique.append(tile)
            row.append(tile_ids[tile])
        tilemap.append(row)

    lines = [
        "/* Generated by tools/build_stadium_tiles.py; do not hand-edit. */",
        f"#define STADIUM_TILE_COUNT {len(unique)}",
        "static const u32 stadium_tiles[STADIUM_TILE_COUNT][8] = {",
    ]
    for tile in unique:
        words = []
        for y in range(8):
            word = 0
            for value in tile[y * 8:(y + 1) * 8]:
                word = (word << 4) | value
            words.append(f"0x{word:08X}")
        lines.append("    { " + ", ".join(words) + " },")
    lines.extend(["};", "", "static const u16 stadium_tilemap[28][40] = {"])
    for row in tilemap:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", ""])
    OUTPUT.write_text("\n".join(lines), encoding="ascii")
    print(f"Wrote {len(unique)} unique tiles, {PREVIEW.name}, and {OUTPUT.name}")


if __name__ == "__main__":
    main()
