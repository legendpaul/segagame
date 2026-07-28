"""Convert the authored stadium source into a 320x224 Mega Drive tilemap.

The perspective-preserving vertical squeeze gives most of the screen to the
court while retaining the far grandstand and a thin near crowd foreground.
The output is deterministic and uses one fixed 16-colour VDP palette.
"""

from pathlib import Path
import colorsys
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "stadium_source_v1.png"
CROWD_TEXTURE = ROOT / "assets" / "crowd_texture.png"
PREVIEW = ROOT / "assets" / "stadium_genesis_preview.png"
ELIMINATOR_PREVIEW = ROOT / "assets" / "eliminator_stadium_preview.png"
TITLE_FIRE_SOURCE = ROOT / "assets" / "title_screen_v2_preview.png"
OUTPUT = ROOT / "src" / "stadium_tiles.inc"

PALETTE = [
    (0, 0, 0), (248, 248, 240), (66, 184, 80), (39, 132, 60),
    (8, 24, 48), (64, 88, 120), (240, 192, 40), (53, 160, 72),
    (16, 24, 40), (184, 56, 64), (216, 168, 64), (52, 88, 144),
    (88, 216, 240), (120, 136, 152), (40, 48, 56), (248, 248, 248),
]

ELIMINATOR_PALETTE = [
    (0, 0, 8), (232, 248, 248), (16, 40, 88), (5, 12, 36),
    (4, 8, 32), (16, 40, 72), (240, 152, 24), (10, 24, 64),
    (2, 3, 24), (216, 64, 24), (240, 176, 48), (24, 56, 136),
    (112, 216, 240), (96, 120, 152), (16, 24, 48), (248, 255, 255),
]


def nearest_colour(rgb):
    return min(range(16), key=lambda i: sum((rgb[c] - PALETTE[i][c]) ** 2 for c in range(3)))


def keep_largest_alpha_component(image):
    """Discard detached keyed fragments while preserving the main 8-way shape."""
    alpha = image.getchannel("A")
    pixels = alpha.load()
    seen = set()
    largest = []

    for start_y in range(image.height):
        for start_x in range(image.width):
            start = (start_x, start_y)
            if not pixels[start_x, start_y] or start in seen:
                continue
            stack = [start]
            seen.add(start)
            component = []
            while stack:
                x, y = stack.pop()
                component.append((x, y))
                for ny in range(max(0, y - 1), min(image.height, y + 2)):
                    for nx in range(max(0, x - 1), min(image.width, x + 2)):
                        neighbour = (nx, ny)
                        if pixels[nx, ny] and neighbour not in seen:
                            seen.add(neighbour)
                            stack.append(neighbour)
            if len(component) > len(largest):
                largest = component

    keep = set(largest)
    cleaned = image.copy()
    cleaned_px = cleaned.load()
    for y in range(image.height):
        for x in range(image.width):
            if (x, y) not in keep:
                cleaned_px[x, y] = (0, 0, 0, 0)
    return cleaned


def title_fire_floor_mark():
    """Extract both title flames and project the complete mark onto the court."""
    source = Image.open(TITLE_FIRE_SOURCE).convert("RGB")
    def extract_fire(box, region_gate, size, keep_fragments=False):
        crop_left, crop_top, crop_right, crop_bottom = box
        crop = source.crop(box)
        keyed = Image.new("RGBA", crop.size, (0, 0, 0, 0))
        src_px = crop.load()
        dst_px = keyed.load()
        for y in range(crop.height):
            for x in range(crop.width):
                global_x = crop_left + x
                global_y = crop_top + y
                r, g, b = src_px[x, y]
                hue, saturation, value = colorsys.rgb_to_hsv(
                    r / 255, g / 255, b / 255)
                warm = ((hue < 0.16 or hue > 0.97)
                        and saturation > 0.62 and value > 0.38)
                if warm and region_gate(global_x, global_y):
                    # Retain the original red/orange/gold modelling while
                    # mapping directly onto colours already present in PAL0.
                    if g > 175 and r > 210:
                        colour = PALETTE[10]
                    elif g > 88:
                        colour = PALETTE[6]
                    else:
                        colour = PALETTE[9]
                    dst_px[x, y] = (*colour, 255)
        keyed = keyed.resize(size, Image.Resampling.NEAREST)
        if keep_fragments:
            return keyed
        return keep_largest_alpha_component(keyed)

    # The fiery orbit above MICRO RETRO. The shaped lower edge excludes the
    # nearby lettering but keeps the hooked right-hand end of the arc.
    def upper_flame_gate(x, y):
        # Follow the hot arc itself: it rises sharply at the left, travels
        # above the lettering, then hooks down at the far right.
        if x < 180:
            lower_edge = 50 - int((x - 160) * 0.9)
        elif x < 220:
            lower_edge = 32
        else:
            lower_edge = 32 + int((x - 220) * 0.78)
        return y <= lower_edge

    upper = extract_fire(
        (160, 18, 263, 76), upper_flame_gate,
        (105, 30), keep_fragments=True)
    # The source arc is only a couple of title pixels thick; one nearest-
    # neighbour expansion keeps it readable once projected across floor tiles.
    upper = upper.filter(ImageFilter.MaxFilter(3))

    # The long championship flame beneath DODGEBALL. Its sloped upper gate
    # excludes the wordmark underline and the red foreground player.
    lower_left = 82
    lower = extract_fire(
        (lower_left, 118, 240, 176),
        lambda x, y: y >= 139 - int((x - lower_left) * 0.18),
        (120, 34))

    # Rebuild the title relationship before applying the single court-plane
    # projection: upper orbit above/right, long lower flame sweeping left.
    keyed = Image.new("RGBA", (124, 62), (0, 0, 0, 0))
    keyed.paste(upper, (17, 0), upper)
    keyed.paste(lower, (2, 28), lower)
    alpha = keyed.getchannel("A")
    outline = alpha.filter(ImageFilter.MaxFilter(5))

    # Project the title-space mark into screen space. Court depth is
    # screen-y minus screen-x/4, so the +x/4 shear makes every flame edge sit
    # at the same orientation as the touchlines and mowing stripes.
    projected = Image.new("RGBA", (128, 96), (0, 0, 0, 0))
    outline_px = outline.load()
    keyed_px = keyed.load()
    projected_px = projected.load()
    for y in range(keyed.height):
        for x in range(keyed.width):
            py = y + 18 + ((x - keyed.width // 2) // 4)
            px = x + 2
            if outline_px[x, y]:
                projected_px[px, py] = (*PALETTE[4], 255)
    for y in range(keyed.height):
        for x in range(keyed.width):
            if keyed_px[x, y][3]:
                py = y + 18 + ((x - keyed.width // 2) // 4)
                projected_px[x + 2, py] = keyed_px[x, y]
    return projected


def prepare_image(with_net=True):
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

    # Depth of the LED ring band in the far stand (above the far touchline).
    FAR_BAND_DEPTH = -8

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

    # Tiered arena SEATING - like the reference photo, the stands are rows of
    # (mostly empty) red seats, not a packed crowd. A regular seat grid is
    # exactly right here, reads cleanly at 8x8 and dedupes to few tiles. Each
    # seat is a small red block; broad dark steps separate each row; occasional
    # blue and pale spectators break the red; wide vomitory aisles split decks.
    # EVERY line in the stand must run at the SAME angle as the pitch. Seat rows
    # therefore follow lines of constant court depth (v = y - x/4, exactly the
    # touchline slope) instead of flat horizontal rows, and the aisles run up
    # the stand along that same projection instead of straight vertical bars.
    # Horizontal rows / vertical aisles clashed badly against the dimetric court.
    RED, BLUE, PALE, STEP, GAP = 9, 11, 1, 14, 8
    def seat_idx(x, y):
        v = y - (x >> 2)            # up-the-stand axis (parallel to touchline)
        u = x + (y >> 1)            # along-the-stand axis, sheared to match
        if (u % 52) < 7:            # broad, legible vomitory aisle
            return GAP
        row = v % 6
        if row >= 4:                 # two-pixel shadowed step between rows
            return STEP
        if (u % 5) >= 3:             # breathing room between seat clusters
            return GAP
        s = ((u // 5) * 5 + (v // 6) * 7) % 19
        if s == 0:  return BLUE
        if s == 9:  return PALE
        return RED

    px = image.load()
    for y in range(24, 224):
        for x in range(320):
            if px[x, y] == STAND:
                px[x, y] = PALETTE[seat_idx(x, y)]

    # Long architectural rails divide the crowd into readable tiers. They
    # follow court depth, so every structural line belongs to the same camera
    # projection instead of fighting it. The pitch painted below masks their
    # centre portions while the side/near stands retain the framing.
    for tier_depth in (4, 18, 34, 158, 178):
        y0 = tier_depth + (-24 >> 2)
        y1 = tier_depth + (344 >> 2)
        draw.line((-24, y0, 344, y1), fill=PALETTE[14], width=3)
        draw.line((-24, y0 - 1, 344, y1 - 1), fill=PALETTE[5], width=1)

    # Roof shadow across the very top (kept flat - it is the screen edge).
    draw.rectangle((0, 24, 319, 31), fill=PALETTE[8])
    draw.line((0, 31, 319, 31), fill=PALETTE[5], width=1)
    for fx in range(8, 318, 28):
        draw.rectangle((fx, 26, fx + 7, 27), fill=PALETTE[12])
        draw.rectangle((fx + 2, 25, fx + 5, 26), fill=PALETTE[15])

    # LED advertising ring band, drawn ALONG the court angle (constant depth)
    # so it is parallel to the touchlines, hoardings and net - not a flat bar.
    def depth_band(depth, thickness, colour):
        x0, x1 = -20, 340
        draw.line((x0, depth + (x0 >> 2), x1, depth + (x1 >> 2)),
                  fill=colour, width=thickness)

    depth_band(FAR_BAND_DEPTH - 2, 1, PALETTE[14])
    depth_band(FAR_BAND_DEPTH,     4, PALETTE[11])
    depth_band(FAR_BAND_DEPTH + 3, 1, PALETTE[14])
    for i in range(-1, 24):         # bright LED ticks along the same slope
        bx = i * 14
        draw.rectangle((bx, FAR_BAND_DEPTH + (bx >> 2) - 1,
                        bx + 2, FAR_BAND_DEPTH + (bx >> 2)), fill=PALETTE[15])

    # ---- Track separation and near-side roof canopy ----------------------
    # A broadcast stadium never puts the first spectator row directly on the
    # touchline.  This larger projected polygon clears a running-track-width
    # safety apron around all four sides before the playable court is painted.
    def perimeter_point(depth, right=False, lateral=0):
        base = (312 + lateral) if right else (64 - lateral)
        x = base - ((depth - 24) // 2)
        return (x, depth + x // 4)

    track_far_l = perimeter_point(5, False, 18)
    track_far_r = perimeter_point(5, True, 18)
    track_near_l = perimeter_point(163, False, 18)
    track_near_r = perimeter_point(163, True, 18)
    track_outer = [track_far_l, track_far_r, track_near_r, track_near_l]

    # Solid roof decks replace the near and right seating entirely.  They are
    # deliberately drawn after every crowd/tier mark, so no fan pixels can
    # bleed through the canopy surface or its inner fascia.
    draw.polygon([track_near_l, track_near_r, (319, 223), (0, 223)],
                 fill=PALETTE[8])
    draw.polygon([track_far_r, (319, 24), (319, 223), track_near_r],
                 fill=PALETTE[8])

    # Restrained roof-panel ribs add scale without resembling spectator rows.
    for offset in (12, 27, 42):
        draw.line((track_near_l[0], track_near_l[1] + offset,
                   track_near_r[0], track_near_r[1] + offset),
                  fill=PALETTE[4], width=3)
        draw.line((track_near_l[0], track_near_l[1] + offset - 1,
                   track_near_r[0], track_near_r[1] + offset - 1),
                  fill=PALETTE[5], width=1)

    # Slate competition track/concourse.  A second projected loop reads as a
    # lane marking and makes the stand-to-pitch distance obvious at a glance.
    draw.polygon(track_outer, fill=PALETTE[5])
    track_mid = [
        perimeter_point(14, False, 9), perimeter_point(14, True, 9),
        perimeter_point(153, True, 9), perimeter_point(153, False, 9)
    ]
    draw.line(track_outer + [track_outer[0]], fill=PALETTE[4],
              width=4, joint="curve")
    draw.line(track_outer + [track_outer[0]], fill=PALETTE[12],
              width=1, joint="curve")
    draw.line(track_mid + [track_mid[0]], fill=PALETTE[13],
              width=1, joint="curve")

    # Bright inner fascia makes both roof edges read as architecture rather
    # than a plain dark crop, while keeping the spectators completely hidden.
    for edge in ((track_near_l, track_near_r),
                 (track_far_r, track_near_r)):
        draw.line(edge, fill=PALETTE[4], width=6)
        draw.line(edge, fill=PALETTE[12], width=2)

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
    # Far/top advertising edge was previously missing. Use an unmistakable
    # red, white, yellow, blue broadcast sequence across the complete rail.
    def far_hoardings(p0, p1, outward, segs, thickness):
        ad_cols = (PALETTE[9], PALETTE[15], PALETTE[6], PALETTE[12])
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
    far_hoardings(far_l, far_r, (0, -5), 12, 3)
    hoardings(far_l, near_l, (-7, 0), 6, 3)      # left touchline
    hoardings(far_r, near_r, (7, 0), 6, 3)       # right touchline

    # Strong double-edged boundary, exactly on the movement polygon.
    boundary = [far_l, far_r, near_r, near_l, far_l]
    draw.line(boundary, fill=PALETTE[4], width=4, joint="curve")
    draw.line(boundary, fill=PALETTE[15], width=2, joint="curve")

    if with_net:
        # Clear centre board: glass-blue panels with a bright top rail, base
        # rail, and visible posts for the standard two-team game.
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
    else:
        # Reuse both pieces of the title screen's fire mark as the open-court
        # crest. It is sheared into the court projection rather than pasted on
        # flat, so it reads as paint on the floor instead of a screen overlay.
        fire_mark = title_fire_floor_mark()
        image.paste(fire_mark, (96, 78), fire_mark)

    return image


def prepare_foreground(include_net=True):
    """Transparent high-priority roof plus optional centre rails/posts."""
    overlay = Image.new("P", (320, 224), 0)
    flat_palette = [channel for colour in PALETTE for channel in colour] + [0] * (768 - 48)
    overlay.putpalette(flat_palette)
    draw = ImageDraw.Draw(overlay)

    # Repeat the visible near/right canopy geometry from prepare_image on the
    # transparent foreground plane. BG_B still carries the same pixels, but
    # these priority tiles make every gameplay sprite pass underneath the
    # stadium roof instead of appearing in front of the architecture.
    def perimeter_point(depth, right=False, lateral=0):
        base = (312 + lateral) if right else (64 - lateral)
        x = base - ((depth - 24) // 2)
        return (x, depth + x // 4)

    track_far_r = perimeter_point(5, True, 18)
    track_near_l = perimeter_point(163, False, 18)
    track_near_r = perimeter_point(163, True, 18)
    draw.polygon([track_near_l, track_near_r, (319, 223), (0, 223)], fill=8)
    draw.polygon([track_far_r, (319, 24), (319, 223), track_near_r], fill=8)
    for offset in (12, 27, 42):
        draw.line((track_near_l[0], track_near_l[1] + offset,
                   track_near_r[0], track_near_r[1] + offset),
                  fill=4, width=3)
        draw.line((track_near_l[0], track_near_l[1] + offset - 1,
                   track_near_r[0], track_near_r[1] + offset - 1),
                  fill=5, width=1)
    for edge in ((track_near_l, track_near_r),
                 (track_far_r, track_near_r)):
        draw.line(edge, fill=4, width=6)
        draw.line(edge, fill=12, width=2)

    if not include_net:
        return overlay

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
    flat_palette = [channel for colour in PALETTE for channel in colour] + [0] * (768 - 48)

    def indexed_image(image):
        result = Image.new("P", image.size)
        result.putpalette(flat_palette)
        result.putdata([nearest_colour(pixel) for pixel in image.getdata()])
        return result

    indexed = indexed_image(prepare_image(with_net=True))
    eliminator_indexed = indexed_image(prepare_image(with_net=False))
    indexed.save(PREVIEW)
    eliminator_preview = eliminator_indexed.copy()
    eliminator_preview.putpalette(
        [channel for colour in ELIMINATOR_PALETTE for channel in colour]
        + [0] * (768 - 48))
    eliminator_preview.save(ELIMINATOR_PREVIEW)
    foreground = prepare_foreground(include_net=True)
    eliminator_foreground = prepare_foreground(include_net=False)

    unique = []
    tile_ids = {}

    def build_tilemap(source):
        tilemap = []
        pixels = source.load()
        for ty in range(28):
            row = []
            for tx in range(40):
                tile = tuple(pixels[tx * 8 + x, ty * 8 + y]
                             for y in range(8) for x in range(8))
                if tile not in tile_ids:
                    tile_ids[tile] = len(unique)
                    unique.append(tile)
                row.append(tile_ids[tile])
            tilemap.append(row)
        return tilemap

    tilemap = build_tilemap(indexed)
    eliminator_tilemap = build_tilemap(eliminator_indexed)

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
    lines.extend(["};", "", "static const u16 eliminator_stadium_tilemap[28][40] = {"])
    for row in eliminator_tilemap:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", ""])

    fg_unique = [tuple([0] * 64)]
    fg_ids = {fg_unique[0]: 0}
    def build_foreground_tilemap(source):
        result = []
        pixels = source.load()
        for ty in range(28):
            row = []
            for tx in range(40):
                tile = tuple(pixels[tx * 8 + x, ty * 8 + y]
                             for y in range(8) for x in range(8))
                if tile not in fg_ids:
                    fg_ids[tile] = len(fg_unique)
                    fg_unique.append(tile)
                row.append(fg_ids[tile])
            result.append(row)
        return result

    fg_map = build_foreground_tilemap(foreground)
    eliminator_fg_map = build_foreground_tilemap(eliminator_foreground)
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
    lines.extend(["};", "",
                  "static const u16 eliminator_stadium_foreground_tilemap[28][40] = {"])
    for row in eliminator_fg_map:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", ""])
    OUTPUT.write_text("\n".join(lines), encoding="ascii")
    print(f"Wrote {len(unique)} shared court tiles + {len(fg_unique)} foreground tiles, "
          f"{PREVIEW.name}, {ELIMINATOR_PREVIEW.name}, and {OUTPUT.name}")


if __name__ == "__main__":
    main()
