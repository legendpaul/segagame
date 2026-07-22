"""Build 32x32 Genesis player tiles from the generated isometric sheet.

The source contains four separated poses on transparency. This script
finds the four largest connected components, fits each into a bottom-
aligned 32x32 canvas, maps reds into the team-swappable kit ramp and all
other pixels into the fixed skin/hair ramp, then writes C tile data in
the VDP's column-major order plus a nearest-neighbour QA preview.
"""

from collections import deque
from pathlib import Path
import colorsys

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "player_isometric_sheet.png"
BACK_SOURCE = ROOT / "assets" / "player_isometric_back_sheet_v2.png"
RUN_SOURCE = ROOT / "assets" / "player_run_cycle_v2.png"
RUN_PASS_SOURCE = ROOT / "assets" / "player_run_pass_v1.png"
ACTION_SOURCE = ROOT / "assets" / "player_action_sheet_v1.png"
OUTPUT = ROOT / "src" / "player_isometric_tiles.inc"
PREVIEW = ROOT / "assets" / "player_isometric_preview.png"

# Must stay in sync with pal_team_red[] in sprites_data.c.
PALETTE = {
    1: (0xE1, 0xA4, 0x8E), 2: (0xCF, 0x82, 0x88),
    3: (0xD2, 0x77, 0x5A), 4: (0xBA, 0x70, 0x61),
    5: (0xB8, 0x46, 0x4F), 6: (0xA8, 0x40, 0x48),
    7: (0x9F, 0x3C, 0x44), 8: (0xCA, 0x5B, 0x18),
    9: (0x87, 0x50, 0x51), 10: (0x95, 0x3C, 0x20),
    11: (0x6A, 0x28, 0x22), 12: (0x95, 0x36, 0x3E),
    13: (0x73, 0x2C, 0x32), 14: (0x4E, 0x1D, 0x21),
    15: (0x00, 0x00, 0x00),
}
KIT = (2, 5, 6, 7, 12, 13, 14)
FIXED = (1, 3, 4, 8, 9, 10, 11, 15)
POSES = ("stand", "run", "throw", "catch")


def remove_green_key(image):
    """Turn the authored rear sheet's bright-green matte transparent."""
    result = image.convert("RGBA")
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            r, g, b, a = pixels[x, y]
            if g > 110 and g > r * 3 // 2 and g > b * 3 // 2:
                pixels[x, y] = (r, g, b, 0)
    return result


def components(alpha: Image.Image, limit=4):
    w, h = alpha.size
    mask = alpha.point(lambda a: 255 if a >= 80 else 0)
    px = mask.load()
    seen = set()
    found = []
    for y in range(h):
        for x in range(w):
            if not px[x, y] or (x, y) in seen:
                continue
            q = deque([(x, y)])
            seen.add((x, y))
            points = []
            while q:
                cx, cy = q.popleft()
                points.append((cx, cy))
                for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                    if 0 <= nx < w and 0 <= ny < h and px[nx, ny] and (nx, ny) not in seen:
                        seen.add((nx, ny))
                        q.append((nx, ny))
            if len(points) > 500:
                xs = [p[0] for p in points]
                ys = [p[1] for p in points]
                found.append((len(points), (min(xs), min(ys), max(xs) + 1, max(ys) + 1)))
    return sorted(found, reverse=True)[:limit]


def nearest(rgb, slots):
    return min(slots, key=lambda idx: sum((rgb[i] - PALETTE[idx][i]) ** 2 for i in range(3)))


def index_pixel(rgba):
    r, g, b, a = rgba
    if a < 80:
        return 0
    if max(r, g, b) < 34:
        return 15
    hue, sat, val = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    hue_deg = hue * 360
    # Generated uniforms are true red; orange/yellow skin sits safely
    # outside this narrow hue window and remains team-independent.
    is_kit = sat > 0.48 and val > 0.14 and (hue_deg < 13 or hue_deg > 347)
    # Remove the generated white balls: the runtime ball sprite is drawn
    # separately and must not be baked into THROW/CATCH artwork.
    if sat < 0.18 and val > 0.62:
        return 0
    return nearest((r, g, b), KIT if is_kit else FIXED)


def encode_tiles(canvas):
    tiles = []
    for tx in range(4):
        for ty in range(4):
            rows = []
            for y in range(8):
                value = 0
                for x in range(8):
                    value = (value << 4) | canvas[ty * 8 + y][tx * 8 + x]
                rows.append(value)
            tiles.append(rows)
    return tiles


def encode_far(canvas):
    source = Image.new("P", (32, 32))
    source.putdata([value for row in canvas for value in row])
    reduced = source.resize((24, 24), Image.Resampling.NEAREST)
    far = [list(reduced.crop((0, y, 24, y + 1)).getdata()) for y in range(24)]
    tiles = []
    for tx in range(3):
        for ty in range(3):
            rows = []
            for y in range(8):
                value = 0
                for x in range(8):
                    value = (value << 4) | far[ty * 8 + y][tx * 8 + x]
                rows.append(value)
            tiles.append(rows)
    return tiles


def build_canvases(image, pose_names=POSES):
    boxes = sorted((box for _, box in components(image.getchannel("A"), len(pose_names))),
                   key=lambda b: b[0])
    if len(boxes) != len(pose_names):
        raise SystemExit(f"Expected {len(pose_names)} player components, found {len(boxes)}")

    pose_canvases = {}
    for pose, box in zip(pose_names, boxes):
        sprite = image.crop(box)
        sw, sh = sprite.size
        scale = min(30 / sw, 32 / sh)
        nw, nh = max(1, round(sw * scale)), max(1, round(sh * scale))
        sprite = sprite.resize((nw, nh), Image.Resampling.NEAREST)
        ox, oy = (32 - nw) // 2, 32 - nh
        canvas = [[0] * 32 for _ in range(32)]
        for y in range(nh):
            for x in range(nw):
                canvas[oy + y][ox + x] = index_pixel(sprite.getpixel((x, y)))
        pose_canvases[pose] = canvas
    return pose_canvases


def main():
    front = build_canvases(Image.open(SOURCE).convert("RGBA"))
    back = build_canvases(remove_green_key(Image.open(BACK_SOURCE)))
    runs = build_canvases(remove_green_key(Image.open(RUN_SOURCE)),
                          ("front_run", "front_run_alt", "back_run", "back_run_alt"))
    run_passes = build_canvases(remove_green_key(Image.open(RUN_PASS_SOURCE)),
                                ("front_run_pass", "back_run_pass"))
    actions = build_canvases(remove_green_key(Image.open(ACTION_SOURCE)),
                             ("front_hit", "front_fall", "front_celebrate",
                              "back_hit", "back_fall", "back_celebrate"))
    front["run"] = runs["front_run"]
    front["run_pass"] = run_passes["front_run_pass"]
    front["run_alt"] = runs["front_run_alt"]
    back["run"] = runs["back_run"]
    back["run_pass"] = run_passes["back_run_pass"]
    back["run_alt"] = runs["back_run_alt"]
    for pose in ("hit", "fall", "celebrate"):
        front[pose] = actions[f"front_{pose}"]
        back[pose] = actions[f"back_{pose}"]

    output_poses = ("stand", "run", "run_pass", "run_alt", "throw", "catch",
                    "hit", "fall", "celebrate")
    preview = Image.new("RGB", (len(output_poses) * 40, 2 * 40), (24, 40, 72))
    for row, canvases in enumerate((front, back)):
        for col, pose in enumerate(output_poses):
            rendered = Image.new("RGB", (32, 32), (24, 40, 72))
            rp = rendered.load()
            for y in range(32):
                for x in range(32):
                    if canvases[pose][y][x]:
                        rp[x, y] = PALETTE[canvases[pose][y][x]]
            preview.paste(rendered, (4 + col * 40, 4 + row * 40))

    lines = ["/* Generated by tools/build_isometric_sprites.py. */", ""]
    for direction, canvases in (("front", front), ("back", back)):
        for pose in output_poses:
            c_name = "pickup" if pose == "catch" else pose
            lines.append(f"static const u32 tile_iso_{direction}_{c_name}[16][8] = {{")
            for rows in encode_tiles(canvases[pose]):
                values = ", ".join(f"0x{value:08x}" for value in rows)
                lines.append(f"    {{ {values} }},")
            lines.extend(["};", ""])
    for direction, canvases in (("front", front), ("back", back)):
        for pose in output_poses:
            c_name = "pickup" if pose == "catch" else pose
            lines.append(f"static const u32 tile_iso_far_{direction}_{c_name}[9][8] = {{")
            for rows in encode_far(canvases[pose]):
                values = ", ".join(f"0x{value:08x}" for value in rows)
                lines.append(f"    {{ {values} }},")
            lines.extend(["};", ""])
    OUTPUT.write_text("\n".join(lines), encoding="ascii")
    preview.resize((800, 320), Image.Resampling.NEAREST).save(PREVIEW)
    print(f"Wrote {OUTPUT}")
    print(f"Wrote {PREVIEW}")


if __name__ == "__main__":
    main()
