"""Clean the indexed Team Select / Match Up athlete artwork in-place.

The national kit and skin ramps remain recolourable. Silhouette ink, hair and
the ball are reassigned to fixed palette entries so they can never inherit a
country or skin colour at runtime.
"""

from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "matchup_tiles.inc"
PREVIEW = ROOT / "assets" / "matchup_figures_clean_preview.png"
PLAYER_SHEET = ROOT / "assets" / "player_isometric_sheet.png"
WIDTH, HEIGHT = 72, 128
BLACK, DARK_GREY, WHITE, BALL_GREY = 10, 11, 8, 9


def parse() -> tuple[list[bytes], list[list[list[int]]], list[int]]:
    # Always rebuild from the checked-in source artwork. Reading the generated
    # output back in made earlier cleanup passes accumulate and deform the man.
    try:
        text = subprocess.check_output(
            ["git", "show", "HEAD:MicroRetroDodgeball/src/matchup_tiles.inc"],
            cwd=ROOT.parent, text=True)
    except (OSError, subprocess.CalledProcessError):
        text = SOURCE.read_text(encoding="utf-8")
    tile_section = re.search(
        r"static const u32 matchup_fig_tiles.*?= \{(.*?)\n\};", text, re.S)
    values = [int(value, 16) for value in
              re.findall(r"0x[0-9a-fA-F]+", tile_section.group(1))]
    tiles = []
    for start in range(0, len(values), 8):
        pixels = []
        for word in values[start:start + 8]:
            pixels.extend((word >> shift) & 15 for shift in range(28, -1, -4))
        tiles.append(bytes(pixels))

    maps = []
    for figure in (1, 2):
        section = re.search(
            rf"matchup_fig{figure}_map.*?= \{{(.*?)\n\}};", text, re.S)
        maps.append([[int(value) for value in re.findall(r"\d+", row)]
                     for row in re.findall(r"\{([^{}]+)\}", section.group(1))])

    palette_section = re.search(
        r"matchup_base_palette\[16\] = \{(.*?)\};", text, re.S)
    palette = [int(value, 16) for value in
               re.findall(r"0x[0-9a-fA-F]+", palette_section.group(1))]
    return tiles, maps, palette


def reconstruct(tiles: list[bytes], tilemap: list[list[int]]) -> np.ndarray:
    image = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)
    for tile_y, row in enumerate(tilemap):
        for tile_x, tile_index in enumerate(row):
            if tile_index:
                image[tile_y * 8:tile_y * 8 + 8,
                      tile_x * 8:tile_x * 8 + 8] = np.frombuffer(
                          tiles[tile_index], dtype=np.uint8).reshape(8, 8)
    return image


def add_black_silhouette(image: np.ndarray) -> None:
    occupied = image != 0
    padded = np.pad(occupied, 1, constant_values=False)
    interior = (padded[0:-2, 0:-2] & padded[0:-2, 1:-1] &
                padded[0:-2, 2:] & padded[1:-1, 0:-2] &
                padded[1:-1, 2:] & padded[2:, 0:-2] &
                padded[2:, 1:-1] & padded[2:, 2:])
    image[occupied & ~interior] = BLACK


def recolour_hair(image: np.ndarray, polygon: list[tuple[int, int]]) -> None:
    mask_image = Image.new("1", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(mask_image).polygon(polygon, fill=1)
    mask = np.asarray(mask_image, dtype=bool) & (image != 0)
    # Preserve a restrained dark-grey highlight while forcing every brown or
    # skin-derived hair shade out of the national/skin ramps.
    highlight = mask & np.isin(image, (5, 6, 8))
    image[mask] = BLACK
    image[highlight] = DARK_GREY


def restore_thrower_face(image: np.ndarray) -> None:
    """Keep the front figure's face distinct from the adjacent black hair."""
    cx, cy, rx, ry = 39.5, 38.0, 9.5, 11.0
    for y in range(27, 50):
        for x in range(29, 51):
            distance = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if distance > 1.0 or image[y, x] == 0:
                continue
            if distance >= 0.80:
                image[y, x] = BLACK
            elif x <= 39 and y <= 39:
                image[y, x] = 5
            else:
                image[y, x] = 6
    # Brow, eye, nose shadow and mouth remain deliberate near-black accents.
    for x, y in ((41, 34), (42, 34), (43, 35),
                 (45, 39), (43, 43), (44, 43)):
        image[y, x] = BLACK


def draw_white_ball(image: np.ndarray) -> None:
    # A clean outlined oval replaces the skin-coloured source ball. It remains
    # wholly inside the existing 72px figure canvas and reads at 1x resolution.
    cx, cy, rx, ry = 57.5, 65.0, 11.5, 14.0
    for y in range(49, 81):
        for x in range(47, 72):
            distance = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if distance > 1.0:
                continue
            if distance >= 0.73:
                image[y, x] = BLACK
            elif x >= 59 and y >= 65:
                image[y, x] = BALL_GREY
            else:
                image[y, x] = WHITE
    # Two restrained seam pixels keep it reading as a ball rather than a disc.
    for x, y in ((53, 61), (54, 62), (61, 69), (62, 70)):
        image[y, x] = BALL_GREY


def paint_polygon(image: np.ndarray, points: list[tuple[int, int]],
                  colour: int) -> None:
    layer = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(layer).polygon(points, fill=255)
    image[np.asarray(layer) != 0] = colour


def clear_disc(image: np.ndarray, cx: float, cy: float,
               rx: float, ry: float) -> None:
    """Erase an earlier generated ball so regeneration stays idempotent."""
    for y in range(max(0, int(cy - ry - 2)), min(HEIGHT, int(cy + ry + 3))):
        for x in range(max(0, int(cx - rx - 2)), min(WIDTH, int(cx + rx + 3))):
            if ((x - cx) / (rx + 1)) ** 2 + ((y - cy) / (ry + 1)) ** 2 <= 1:
                image[y, x] = 0


def draw_short_white_ball(image: np.ndarray) -> None:
    """Draw one white ball gripped by the bent hand on screen-right."""
    cx, cy, rx, ry = 62.0, 56.0, 8.5, 9.5
    for y in range(45, 68):
        for x in range(52, 72):
            distance = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if distance > 1.0:
                continue
            if distance >= 0.69:
                image[y, x] = BLACK
            elif x >= 63 and y >= 56:
                image[y, x] = BALL_GREY
            else:
                image[y, x] = WHITE
    for x, y in ((58, 52), (59, 53), (65, 59), (66, 60)):
        image[y, x] = BALL_GREY


def repair_short_player(image: np.ndarray) -> None:
    """Make small repairs without replacing the original athlete's anatomy."""
    # The original art is canonical, but erase any generated balls defensively.
    clear_disc(image, 7.5, 78.0, 8.5, 10.0)
    clear_disc(image, 62.0, 57.0, 10.0, 11.0)

    # Sleeve/forearm boundary on screen-left. Only recolour existing pixels so
    # the original arm length and silhouette cannot become disconnected.
    arm_mask = Image.new("1", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(arm_mask).polygon(
        [(16, 55), (20, 59), (18, 64), (12, 70), (8, 78),
         (3, 76), (5, 68), (12, 61)], fill=1)
    mask = np.asarray(arm_mask, dtype=bool) & (image != 0) & (image != BLACK)
    old = image.copy()
    image[mask & np.isin(old, (2, 8))] = 5
    image[mask & np.isin(old, (3, 9, 11))] = 6
    image[mask & np.isin(old, (4, 7))] = 7

    # Shorts palette guard. It changes erroneous skin/neutral pixels in place;
    # no large replacement shapes, so the original folds and hems survive.
    shorts_mask = Image.new("1", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(shorts_mask).polygon(
        [(18, 66), (58, 66), (63, 76), (58, 89), (44, 90),
         (38, 84), (34, 89), (21, 87), (17, 76)], fill=1)
    mask = np.asarray(shorts_mask, dtype=bool) & (image != 0) & (image != BLACK)
    old = image.copy()
    image[mask & np.isin(old, (5, 8))] = 2
    image[mask & np.isin(old, (6, 9, 11))] = 3
    image[mask & np.isin(old, (7,))] = 4

    # The source has stray skin highlights just outside its own shorts contour.
    # Treat the complete waistband-to-hem rectangle as fabric; the legs begin
    # below this line, giving both hems a clean and unambiguous edge.
    shorts = image[66:92, 16:64]
    shorts[np.isin(shorts, (5, 8))] = 2
    shorts[np.isin(shorts, (6, 9, 11))] = 3
    shorts[shorts == 7] = 4

    # Small attached boots extend the original leg endpoints by only 3-5px.
    paint_polygon(image, [(18, 106), (28, 106), (32, 111), (31, 116),
                          (18, 116), (15, 113)], BLACK)
    paint_polygon(image, [(20, 109), (27, 109), (29, 112), (19, 112)],
                  DARK_GREY)
    paint_polygon(image, [(55, 112), (62, 112), (69, 116), (70, 120),
                          (58, 120), (53, 117)], BLACK)
    paint_polygon(image, [(58, 115), (62, 115), (67, 117), (58, 117)],
                  DARK_GREY)

    # One compact ball sits against the existing bent screen-right arm. A
    # visible skin-coloured fist overlaps its lower-left rim to show the grip.
    draw_short_white_ball(image)
    paint_polygon(image, [(52, 59), (55, 57), (59, 59), (59, 63),
                          (56, 66), (52, 64)], BLACK)
    paint_polygon(image, [(54, 59), (57, 59), (58, 61), (56, 64),
                          (54, 63)], 6)


def build_short_from_game_art() -> np.ndarray:
    """Create the selector man from the complete in-game throwing artwork.

    The source pose already has two connected arms, two feet, and the ball
    gripped in the anatomical right hand. It is reduced to a 68x90 indexed
    sprite and mapped onto the same recolourable kit/skin ramps as gameplay.
    """
    source = Image.open(PLAYER_SHEET).convert("RGBA")
    # Connected-component bounds of the complete overhead throwing pose.
    pose = source.crop((1070, 170, 1387, 588))
    pose.thumbnail((68, 90), Image.Resampling.LANCZOS)
    rgba = np.asarray(pose)
    indexed = np.zeros((pose.height, pose.width), dtype=np.uint8)

    for y in range(pose.height):
        for x in range(pose.width):
            r, g, b, a = (int(v) for v in rgba[y, x])
            if a < 72:
                continue
            maximum = max(r, g, b)
            minimum = min(r, g, b)
            saturation = maximum - minimum
            luminance = (r * 3 + g * 5 + b * 2) // 10

            if maximum < 48:
                colour = BLACK
            elif saturation < 30:
                if luminance >= 190:
                    colour = WHITE
                elif luminance >= 65:
                    colour = BALL_GREY
                else:
                    colour = DARK_GREY
            elif r > 115 and g < 30 and b < 30:
                # Red source uniform -> national kit ramp.
                colour = 2 if r >= 205 else (3 if r >= 140 else 4)
            elif r > 85 and r > g and g > b * 13 // 10:
                # Orange/brown source flesh -> country skin ramp.
                colour = 5 if luminance >= 205 else (6 if luminance >= 125 else 7)
            else:
                colour = BLACK if luminance < 90 else DARK_GREY
            indexed[y, x] = colour

    # Centre the complete pose with balanced transparent space below it.
    result = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)
    offset_x = (WIDTH - pose.width) // 2
    offset_y = 17
    result[offset_y:offset_y + pose.height,
           offset_x:offset_x + pose.width] = indexed

    add_black_silhouette(result)
    return result


def encode_tile(tile: bytes) -> list[int]:
    rows = []
    for y in range(8):
        word = 0
        for colour in tile[y * 8:y * 8 + 8]:
            word = (word << 4) | colour
        rows.append(word)
    return rows


def vdp_rgb(value: int) -> tuple[int, int, int]:
    return (((value >> 1) & 7) * 255 // 7,
            ((value >> 5) & 7) * 255 // 7,
            ((value >> 9) & 7) * 255 // 7)


def main() -> None:
    tiles, old_maps, palette = parse()
    figures = [reconstruct(tiles, old_maps[0]), build_short_from_game_art()]

    add_black_silhouette(figures[0])
    recolour_hair(figures[0], [(23, 14), (43, 14), (51, 25),
                                (46, 30), (29, 31), (22, 27)])
    restore_thrower_face(figures[0])
    draw_white_ball(figures[0])

    # Dedicated fixed colours: kit remains 2-4 and skin remains 5-7.
    palette[BLACK] = 0x0000
    palette[DARK_GREY] = 0x0222
    palette[WHITE] = 0x0EEE
    palette[BALL_GREY] = 0x0AAA

    blank = bytes(64)
    unique = [blank]
    lookup = {blank: 0}
    maps = []
    for figure in figures:
        tilemap = []
        for tile_y in range(16):
            row = []
            for tile_x in range(9):
                tile = bytes(figure[tile_y * 8:tile_y * 8 + 8,
                                    tile_x * 8:tile_x * 8 + 8].reshape(-1))
                if tile not in lookup:
                    lookup[tile] = len(unique)
                    unique.append(tile)
                row.append(lookup[tile])
            tilemap.append(row)
        maps.append(tilemap)

    lines = [
        "/* Cleaned by tools/clean_matchup_figures.py. */",
        "#define MATCHUP_FIG_W 9",
        "#define MATCHUP_FIG_H 16",
        f"#define MATCHUP_FIG_TILE_COUNT {len(unique)}",
        "static const u32 matchup_fig_tiles[MATCHUP_FIG_TILE_COUNT][8] = {",
    ]
    for tile in unique:
        rows = encode_tile(tile)
        lines.append("    {" + ",".join(f"0x{word:08x}" for word in rows) + "},")
    lines.append("};")
    for number, tilemap in enumerate(maps, 1):
        lines.append(f"static const u16 matchup_fig{number}_map[MATCHUP_FIG_H][MATCHUP_FIG_W] = {{")
        for row in tilemap:
            lines.append("    {" + ",".join(str(value) for value in row) + "},")
        lines.append("};")
    lines.append("static const u16 matchup_base_palette[16] = {")
    lines.append("    " + ",".join(f"0x{value:04X}" for value in palette))
    lines.append("};")
    SOURCE.write_text("\n".join(lines) + "\n", encoding="ascii")

    preview = Image.new("RGB", (WIDTH * 2 + 16, HEIGHT), (10, 18, 34))
    colours = [vdp_rgb(value) for value in palette]
    for index, figure in enumerate(figures):
        rgb = Image.new("RGB", (WIDTH, HEIGHT), (10, 18, 34))
        pixels = rgb.load()
        for y in range(HEIGHT):
            for x in range(WIDTH):
                colour = int(figure[y, x])
                if colour:
                    pixels[x, y] = colours[colour]
        preview.paste(rgb, (index * (WIDTH + 16), 0))
    preview.resize((preview.width * 4, preview.height * 4),
                   Image.Resampling.NEAREST).save(PREVIEW)
    print(f"Wrote {SOURCE} ({len(unique)} unique tiles)")
    print(f"Wrote {PREVIEW}")


if __name__ == "__main__":
    main()
