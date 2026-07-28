"""Composite only sharpened title-letter pixels over the untouched source.

The image edit supplies cleaner typography, but this mask deliberately rejects
every change outside the two existing wordmarks so the stadium artwork, players,
balls, lighting, globe and fire remain byte-for-byte identical to v2.
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / "assets" / "title_source_v2.png"
EDIT = ROOT / "assets" / "title_text_sharpened_edit.png"
OUTPUT = ROOT / "assets" / "title_source_v3.png"
MASK_OUTPUT = ROOT / "assets" / "title_text_mask_v3.png"


def gold_pixels(image: Image.Image) -> Image.Image:
    rgb = np.asarray(image.convert("RGB"), dtype=np.int16)
    red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    selected = ((red > 150) & (green > 65) & (blue < 180) &
                (red > blue + 45) & (green > blue + 20))
    return Image.fromarray((selected * 255).astype(np.uint8), "L")


def main() -> None:
    original = Image.open(ORIGINAL).convert("RGB")
    edited = Image.open(EDIT).convert("RGB")
    if edited.size != original.size:
        edited = edited.resize(original.size, Image.Resampling.LANCZOS)

    width, height = original.size
    sx, sy = width / 1448.0, height / 1086.0
    region = Image.new("L", original.size, 0)
    draw = ImageDraw.Draw(region)
    # Tight quadrilaterals around the existing slanted MICRO RETRO and
    # DODGEBALL faces; neither reaches the athletes or stadium scenery.
    draw.polygon([(int(x * sx), int(y * sy)) for x, y in
                  ((430, 220), (1065, 158), (1080, 326), (425, 405))], fill=255)
    draw.polygon([(int(x * sx), int(y * sy)) for x, y in
                  ((300, 360), (1210, 226), (1225, 500), (270, 718))], fill=255)

    old_gold = gold_pixels(original)
    new_gold = gold_pixels(edited)
    letter_mask = Image.fromarray(np.maximum(np.asarray(old_gold),
                                              np.asarray(new_gold)), "L")
    letter_mask = Image.composite(letter_mask, Image.new("L", original.size, 0),
                                  region)
    # Include the immediate dark outline and pale inner keyline, but nothing
    # beyond roughly 2.5 output pixels at the final 320x224 resolution.
    letter_mask = letter_mask.filter(ImageFilter.MaxFilter(25))
    letter_mask = Image.composite(letter_mask, Image.new("L", original.size, 0),
                                  region)

    result = Image.composite(edited, original, letter_mask)
    outside = np.asarray(letter_mask) == 0
    if not np.array_equal(np.asarray(result)[outside], np.asarray(original)[outside]):
        raise RuntimeError("A non-lettering background pixel changed")
    result.save(OUTPUT)
    letter_mask.save(MASK_OUTPUT)
    print(f"Wrote {OUTPUT}")
    print(f"Wrote {MASK_OUTPUT}")


if __name__ == "__main__":
    main()
