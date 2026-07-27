"""Convert the authored stadium source into a 320x224 Mega Drive tilemap.

The perspective-preserving vertical squeeze gives most of the screen to the
court while retaining the far grandstand and a thin near crowd foreground.
The output is deterministic and uses one fixed 16-colour VDP palette.
"""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "stadium_source_v1.png"
CROWD_TEXTURE = ROOT / "assets" / "crowd_texture.png"
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

    # ---- Authored stadium that FRAMES the pitch on all sides --------------
    # Start from a flat dark fill, then lay dark stand backing around every
    # side of the pitch, then paint an authored "rows of seated spectators"
    # crowd over it. The pitch is drawn on top afterwards, so the crowd wraps
    # the whole court and it reads as a stadium, not an empty field.
    draw.rectangle((0, 0, 319, 223), fill=PALETTE[8])
    STAND = PALETTE[4]
    far_wedge   = [(0, 24), (319, 24), far_r, far_l]
    left_strip  = [(0, 24), far_l, near_l, (0, 223)]
    right_strip = [far_r, (319, 24), (319, 223), near_r]
    near_apron  = [near_l, near_r, (319, 223), (0, 223)]
    for poly in (far_wedge, left_strip, right_strip, near_apron):
        draw.polygon(poly, fill=STAND)

    # Crowd sampled from an AI-generated aerial-crowd texture (assets/
    # crowd_texture.png), downscaled so the spectators are a few pixels each and
    # quantised into our 16-colour palette. This gives a genuinely believable
    # packed crowd instead of a hand-drawn dither. A pure-crowd corner of the
    # source is used (its centre court is ignored) and tiled across the stands.
    crowd_src = (Image.open(CROWD_TEXTURE).convert("RGB")
                 .crop((24, 24, 470, 470))
                 .resize((96, 96), Image.Resampling.LANCZOS))
    cp = crowd_src.load()
    cw, ch = crowd_src.size

    px = image.load()
    for y in range(24, 224):
        for x in range(320):
            if px[x, y] == STAND:
                px[x, y] = PALETTE[nearest_colour(cp[x % cw, y % ch])]

    # Roof shadow + a lit front rail along the top of the far grandstand.
    draw.rectangle((0, 24, 319, 27), fill=PALETTE[8])
    draw.line((0, 28, 319, 28), fill=PALETTE[13], width=1)
    # Concrete tier walkways step the far stand into decks.
    for ry in (40, 54, 68, 82):
        sx = (ry - 24) * 4
        if sx < 313:
            draw.line((max(0, sx), ry, 313, ry), fill=PALETTE[13], width=1)
            draw.line((max(0, sx), ry + 1, 313, ry + 1), fill=PALETTE[14], width=1)

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

    # FIFA-94-style perimeter advertising hoardings. Segmented colour boards
    # ring the pitch just outside the touchlines, following the same dimetric
    # projection, framing the court like a broadcast football stadium.
    def hoardings(p0, p1, outward, segs, thickness):
        ad_cols = (PALETTE[15], PALETTE[9], PALETTE[12], PALETTE[6])
        ox, oy = outward
        for i in range(segs):
            t0, t1 = i / segs, (i + 1) / segs
            x0 = round(p0[0] + (p1[0] - p0[0]) * t0) + ox
            y0 = round(p0[1] + (p1[1] - p0[1]) * t0) + oy
            x1 = round(p0[0] + (p1[0] - p0[0]) * t1) + ox
            y1 = round(p0[1] + (p1[1] - p0[1]) * t1) + oy
            draw.line((x0, y0, x1, y1), fill=ad_cols[i & 3], width=thickness)
            draw.line((x0, y0 - thickness, x1, y1 - thickness),
                      fill=PALETTE[14], width=1)

    hoardings(near_l, near_r, (0, 6), 10, 3)     # near touchline, closest to camera
    hoardings(far_l, near_l, (-7, 0), 6, 3)      # left touchline
    hoardings(far_r, near_r, (7, 0), 6, 3)       # right touchline

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


def prepare_foreground():
    """Transparent high-priority rails/posts that correctly occlude players."""
    overlay = Image.new("P", (320, 224), 0)
    flat_palette = [channel for colour in PALETTE for channel in colour] + [0] * (768 - 48)
    overlay.putpalette(flat_palette)
    draw = ImageDraw.Draw(overlay)

    depth = 84
    left_x = 64 - ((depth - 24) // 2)
    right_x = 312 - ((depth - 24) // 2)
    base_l = (left_x, depth + left_x // 4)
    base_r = (right_x, depth + right_x // 4)
    top_l = (base_l[0], base_l[1] - 12)
    top_r = (base_r[0], base_r[1] - 12)

    # Fine clipped mesh makes depth readable even through the clear panels:
    # a far-side ball is crossed by at least one thread rather than appearing
    # pasted on top of an almost-empty glass area.
    for x in range(top_l[0] + 4, top_r[0], 8):
        t = (x - top_l[0]) / (top_r[0] - top_l[0])
        y_top = round(top_l[1] + (top_r[1] - top_l[1]) * t)
        y_base = round(base_l[1] + (base_r[1] - base_l[1]) * t)
        draw.line((x, y_top + 2, x, y_base - 2),
                  fill=12 if ((x // 8) & 1) else 11, width=1)
    draw.line((top_l[0], top_l[1] + 6, top_r[0], top_r[1] + 6),
              fill=11, width=1)
    for i in range(5):
        t = i / 4
        x = round(base_l[0] + (base_r[0] - base_l[0]) * t)
        y_base = round(base_l[1] + (base_r[1] - base_l[1]) * t)
        draw.line((x + 1, y_base - 13, x + 1, y_base + 3), fill=4, width=3)
        draw.line((x, y_base - 13, x, y_base + 2), fill=12, width=1)
    for segment in ((top_l, top_r), (base_l, base_r)):
        draw.line(segment, fill=4, width=4)
        draw.line(segment, fill=12, width=2)
        draw.line((segment[0][0], segment[0][1] - 1,
                   segment[1][0], segment[1][1] - 1), fill=15, width=1)
    return overlay


def main():
    image = prepare_image()
    indexed = Image.new("P", image.size)
    flat_palette = [channel for colour in PALETTE for channel in colour] + [0] * (768 - 48)
    indexed.putpalette(flat_palette)
    indexed.putdata([nearest_colour(pixel) for pixel in image.getdata()])
    indexed.save(PREVIEW)
    foreground = prepare_foreground()

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

    fg_unique = [tuple([0] * 64)]
    fg_ids = {fg_unique[0]: 0}
    fg_map = []
    fg_pixels = foreground.load()
    for ty in range(28):
        row = []
        for tx in range(40):
            tile = tuple(fg_pixels[tx * 8 + x, ty * 8 + y]
                         for y in range(8) for x in range(8))
            if tile not in fg_ids:
                fg_ids[tile] = len(fg_unique)
                fg_unique.append(tile)
            row.append(fg_ids[tile])
        fg_map.append(row)
    lines.extend([
        f"#define STADIUM_FOREGROUND_TILE_COUNT {len(fg_unique)}",
        "static const u32 stadium_foreground_tiles[STADIUM_FOREGROUND_TILE_COUNT][8] = {",
    ])
    for tile in fg_unique:
        words = []
        for y in range(8):
            word = 0
            for value in tile[y * 8:(y + 1) * 8]:
                word = (word << 4) | value
            words.append(f"0x{word:08X}")
        lines.append("    { " + ", ".join(words) + " },")
    lines.extend(["};", "", "static const u16 stadium_foreground_tilemap[28][40] = {"])
    for row in fg_map:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", ""])
    OUTPUT.write_text("\n".join(lines), encoding="ascii")
    print(f"Wrote {len(unique)} court tiles + {len(fg_unique)} foreground tiles, "
          f"{PREVIEW.name}, and {OUTPUT.name}")


if __name__ == "__main__":
    main()
