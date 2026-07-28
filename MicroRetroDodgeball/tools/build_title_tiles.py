"""Convert the authored title illustration into Mega Drive tile data.

The title is a scene-local VRAM bank: it temporarily overlays the large UI
font region, which scene_menu restores before drawing either team selector.
"""

from pathlib import Path
from PIL import Image, ImageEnhance, ImageFilter, ImageDraw
import numpy as np


def _kmeans(x, k, iters=60, seed=7):
    """Lloyd's k-means (numpy only, no sklearn dependency). Returns per-sample
    labels and the cluster centres."""
    rng = np.random.default_rng(seed)
    centres = x[rng.choice(len(x), k, replace=False)].astype(np.float64).copy()
    x2 = (x * x).sum(1)
    labels = np.zeros(len(x), dtype=np.int64)
    for _ in range(iters):
        d = x2[:, None] + (centres * centres).sum(1)[None, :] - 2.0 * x @ centres.T
        new = d.argmin(1)
        if (new == labels).all():
            labels = new
            break
        labels = new
        for j in range(k):
            m = labels == j
            if m.any():
                centres[j] = x[m].mean(0)
            else:
                centres[j] = x[rng.integers(len(x))]
    return labels, centres

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "title_source_v3.png"
TEXT_MASK = ROOT / "assets" / "title_text_mask_v3.png"
PREVIEW = ROOT / "assets" / "title_screen_v2_preview.png"
OUTPUT = ROOT / "src" / "title_tiles.inc"

WIDTH, HEIGHT = 320, 224
TILES_X, TILES_Y = WIDTH // 8, HEIGHT // 8
MAX_TILES = 379  # title bank + 8 prompt tiles stays below the 0xB000 map

PROMPT_FONT = {
    "<": ("00001", "00010", "00100", "01000", "00100", "00010", "00001"),
    ">": ("10000", "01000", "00100", "00010", "00100", "01000", "10000"),
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
}


def crop_and_reduce(image: Image.Image) -> Image.Image:
    image = image.convert("RGB")
    wanted_height = image.width * HEIGHT // WIDTH
    if wanted_height <= image.height:
        top = (image.height - wanted_height) // 2
        image = image.crop((0, top, image.width, top + wanted_height))
    else:
        wanted_width = image.height * WIDTH // HEIGHT
        left = (image.width - wanted_width) // 2
        image = image.crop((left, 0, left + wanted_width, image.height))

    # Readability pass (2026-07-23): sharpen the busy background (fire, players,
    # stadium look crisper) BUT keep the logo lettering smooth. A strong
    # unsharp turned the thin logo strokes into unreadable speckle once
    # quantized, so over the central logo area we restore the soft BOX
    # downscale, which preserves the fine text far better. Everything else
    # gets the LANCZOS + unsharp treatment.
    plain = image.resize((WIDTH, HEIGHT), Image.Resampling.BOX)
    sharp = image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    sharp = sharp.filter(ImageFilter.UnsharpMask(radius=1.8, percent=170, threshold=1))
    mask = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(mask).rounded_rectangle([60, 38, 262, 132], radius=20, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(9))
    image = Image.composite(plain, sharp, mask)   # plain where mask=white (logo)
    indexed = image.quantize(colors=16, method=Image.Quantize.MEDIANCUT,
                             dither=Image.Dither.NONE)

    # Snap the chosen colours to the Genesis' 3-bit-per-channel gamut, then
    # remap pixels to the closest snapped colour. This preview is therefore
    # representative of the real CRAM result, not a 24-bit approximation.
    raw_palette = indexed.getpalette()[:48]
    palette = []
    for i in range(16):
        rgb = raw_palette[i * 3:i * 3 + 3]
        palette.append(tuple(round(c * 7 / 255) * 255 // 7 for c in rgb))

    pixels = list(indexed.getdata())
    # Genesis uses palette entry 0 as the backdrop/transparent colour.
    # Force the darkest selected navy into that slot and remap the image;
    # leaving a gold there created bright PAL border bars on real output.
    darkest = min(range(16), key=lambda i: sum(palette[i]))
    if darkest != 0:
        palette[0], palette[darkest] = palette[darkest], palette[0]
        pixels = [darkest if p == 0 else 0 if p == darkest else p for p in pixels]
    # Preserve the same indices while exposing the snapped RGB palette.
    reduced = Image.new("P", (WIDTH, HEIGHT))
    flat = [c for rgb in palette for c in rgb] + [0] * (768 - 48)
    reduced.putpalette(flat)
    reduced.putdata(pixels)
    return reduced


def tile_bytes(image: Image.Image, tx: int, ty: int) -> bytes:
    return bytes(image.crop((tx * 8, ty * 8, tx * 8 + 8, ty * 8 + 8)).getdata())


def encode_rows(tile: bytes) -> list[int]:
    rows = []
    for y in range(8):
        value = 0
        for x in range(8):
            value = (value << 4) | tile[y * 8 + x]
        rows.append(value)
    return rows


def prompt_tile(rows: tuple[str, ...], face: int, shadow: int) -> bytes:
    # Opaque navy cell: BG_A text tiles replace the panel tiles underneath,
    # so transparency here would reveal the busy title illustration on BG_B.
    pixels = [shadow] * 64
    for y, row in enumerate(rows):
        for x, bit in enumerate(row):
            if bit != "1":
                continue
            gx = x + 1
            for ox, oy in ((-1, 0), (1, 0), (0, 1), (1, 1)):
                dx, dy = gx + ox, y + oy
                if 0 <= dx < 8 and 0 <= dy < 8:
                    pixels[dy * 8 + dx] = shadow
    for y, row in enumerate(rows):
        for x, bit in enumerate(row):
            if bit == "1":
                pixels[y * 8 + x + 1] = face
    return bytes(pixels)


image = crop_and_reduce(Image.open(SOURCE))
palette = image.getpalette()[:48]
palette_rgb = np.array(palette, dtype=np.float32).reshape(16, 3)

# The source contains nearly one unique tile per screen position. Cluster
# whole 8x8 colour tiles—not individual pixels—into a hardware-sized visual
# vocabulary. This retains the logo edges and athlete silhouettes far better
# than reducing the entire picture to a very low source resolution.
tile_vectors = []
for ty in range(TILES_Y):
    for tx in range(TILES_X):
        indices = np.frombuffer(tile_bytes(image, tx, ty), dtype=np.uint8)
        tile_vectors.append(palette_rgb[indices].reshape(-1))
tile_vectors = np.asarray(tile_vectors, dtype=np.float32)
cluster_count = min(360, len(tile_vectors))
cluster_labels, cluster_centres = _kmeans(tile_vectors, cluster_count)
centres = cluster_centres.reshape(cluster_count, 64, 3)
distance = ((centres[:, :, None, :] - palette_rgb[None, None, :, :]) ** 2).sum(3)
centre_tiles = [bytes(row) for row in distance.argmin(2).astype(np.uint8)]

unique: list[bytes] = []
lookup: dict[bytes, int] = {}
cluster_to_tile = []
for tile in centre_tiles:
    if tile not in lookup:
        lookup[tile] = len(unique)
        unique.append(tile)
    cluster_to_tile.append(lookup[tile])

tilemap = []
reconstructed = Image.new("P", (WIDTH, HEIGHT))
reconstructed.putpalette(palette + [0] * (768 - 48))
for ty in range(TILES_Y):
    row = []
    for tx in range(TILES_X):
        label = cluster_labels[ty * TILES_X + tx]
        tile_index = cluster_to_tile[label]
        row.append(tile_index)
        tile = Image.new("P", (8, 8))
        tile.putpalette(reconstructed.getpalette())
        tile.putdata(unique[tile_index])
        reconstructed.paste(tile, (tx * 8, ty * 8))
    tilemap.append(row)

# The wordmark gets a separate, unclustered transparent layer. The 360-tile
# background compressor is excellent for the stadium but turns narrow letter
# strokes into noisy patches. Reusing the exact reduced source pixels here
# keeps the background tilemap unchanged while restoring solid faces and a
# continuous outline on the console.
text_mask = Image.open(TEXT_MASK).convert("L").resize(
    (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
mask_pixels = np.asarray(text_mask, dtype=np.uint8) >= 48
source_pixels = np.asarray(image, dtype=np.uint8)
dark_nonzero = min(range(1, 16), key=lambda i: sum(palette_rgb[i]))
overlay_pixels = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)
overlay_pixels[mask_pixels] = source_pixels[mask_pixels]
# Palette entry zero is transparent on BG_A. Preserve near-black outline
# pixels by moving only masked zeroes to the darkest opaque title colour.
overlay_pixels[mask_pixels & (overlay_pixels == 0)] = dark_nonzero

overlay_unique: list[bytes] = []
overlay_lookup: dict[bytes, int] = {}
overlay_tilemap = []
for ty in range(TILES_Y):
    row = []
    for tx in range(TILES_X):
        tile = bytes(overlay_pixels[ty * 8:ty * 8 + 8,
                                   tx * 8:tx * 8 + 8].reshape(-1))
        if not any(tile):
            row.append(0)
            continue
        if tile not in overlay_lookup:
            overlay_lookup[tile] = len(overlay_unique)
            overlay_unique.append(tile)
        row.append(overlay_lookup[tile] + 1)  # zero means transparent tile
    overlay_tilemap.append(row)

# The boot logo and flag banks are scene-local and free while the title is
# visible. The menu restores its flag art before it can be shown again.
if len(overlay_unique) > 282:
    raise SystemExit(f"Title text requires {len(overlay_unique)} overlay tiles; maximum is 282")

overlay_preview = Image.fromarray(overlay_pixels, "P")
overlay_preview.putpalette(palette + [0] * (768 - 48))
reconstructed.paste(overlay_preview, (0, 0), text_mask.point(lambda p: 255 if p >= 48 else 0))
reconstructed.convert("RGB").save(PREVIEW)

if len(unique) > MAX_TILES:
    raise SystemExit(f"Title requires {len(unique)} tiles; maximum is {MAX_TILES}")

palette_rgb = [tuple(palette[i * 3:i * 3 + 3]) for i in range(16)]
prompt_face = min(range(1, 16), key=lambda i: sum(
    (palette_rgb[i][c] - (255, 190, 32)[c]) ** 2 for c in range(3)))
prompt_shadow = min(range(1, 16), key=lambda i: sum(
    (palette_rgb[i][c] - (0, 10, 40)[c]) ** 2 for c in range(3)))
prompt_edge = min(range(1, 16), key=lambda i: sum(
    (palette_rgb[i][c] - (64, 216, 255)[c]) ** 2 for c in range(3)))
prompt_order = "<>AEPRST"
prompt_glyphs = [prompt_tile(PROMPT_FONT[ch], prompt_face, prompt_shadow)
                 for ch in prompt_order]
# Three-row broadcast plate behind PRESS START: a solid dark field and a
# cyan highlight edge which is vertically flipped for the lower bevel.
prompt_panel = bytes([prompt_shadow] * 64)
prompt_edge_tile = bytes([prompt_edge] * 8 + [prompt_shadow] * 56)
prompt_tiles = prompt_glyphs + [prompt_panel, prompt_edge_tile]
lines = [
    "/* Generated by tools/build_title_tiles.py from assets/title_source_v2.png. */",
    f"#define TITLE_ART_TILE_COUNT {len(unique)}",
    "static const u16 title_art_palette[16] = {",
]
for i in range(16):
    r, g, b = palette[i * 3:i * 3 + 3]
    lines.append(f"    RGB24_TO_VDPCOLOR(0x{r:02x}{g:02x}{b:02x}),")
lines.extend(["};", "static const u32 title_art_tiles[TITLE_ART_TILE_COUNT][8] = {"])
for tile in unique:
    rows = encode_rows(tile)
    lines.append("    { " + ", ".join(f"0x{v:08x}" for v in rows) + " },")
lines.extend([
    "};",
    f"#define TITLE_PROMPT_GLYPH_COUNT {len(prompt_glyphs)}",
    f"#define TITLE_PROMPT_TILE_COUNT {len(prompt_tiles)}",
    f"#define TITLE_PROMPT_PANEL {len(prompt_glyphs)}",
    f"#define TITLE_PROMPT_EDGE {len(prompt_glyphs) + 1}",
    f'static const char title_prompt_order[] = "{prompt_order}";',
    "static const u32 title_prompt_tiles[TITLE_PROMPT_TILE_COUNT][8] = {",
])
for tile in prompt_tiles:
    rows = encode_rows(tile)
    lines.append("    { " + ", ".join(f"0x{v:08x}" for v in rows) + " },")
lines.extend(["};", "static const u16 title_art_tilemap[28][40] = {"])
for row in tilemap:
    lines.append("    { " + ", ".join(str(v) for v in row) + " },")
lines.extend(["};", f"#define TITLE_TEXT_TILE_COUNT {len(overlay_unique)}",
              "static const u32 title_text_tiles[TITLE_TEXT_TILE_COUNT][8] = {"])
for tile in overlay_unique:
    rows = encode_rows(tile)
    lines.append("    { " + ", ".join(f"0x{v:08x}" for v in rows) + " },")
lines.extend(["};", "static const u16 title_text_tilemap[28][40] = {"])
for row in overlay_tilemap:
    lines.append("    { " + ", ".join(str(v) for v in row) + " },")
lines.append("};")
OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")

print(f"Wrote {OUTPUT} ({len(unique)} unique tiles)")
print(f"Title text overlay: {len(overlay_unique)} unique tiles")
print(f"Wrote {PREVIEW}")
