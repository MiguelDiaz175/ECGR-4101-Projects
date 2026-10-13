"""Convert hand-drawn pet frames (PNG) into RGB565 C arrays for the STM32.

Usage (run from the project-2-virtual-pet folder):
    python tools/sprite_convert.py

Input:  docs/artwork/masters/<animation>_<n>.png   e.g. idle_1.png, eat_2.png
        Black line art on a white (or transparent) canvas. Every frame should
        use the same canvas size and keep the pet in the same place, because
        the whole canvas is scaled down -- that is what keeps frames aligned.
        Labels written along the bottom of the canvas (below LABEL_CUTOFF)
        are removed automatically.

Output: project-2-stm/Core/Src/pet_sprites.c   (const RGB565 arrays, in flash)
        project-2-stm/Core/Inc/pet_sprites.h
        docs/artwork/preview/<frame>.png         (what the LCD will show)
        docs/artwork/preview/all_frames.png      (contact sheet, 3x zoom)

How a drawing becomes colors:
    dark pixels                      -> OUTLINE
    white area connected to the edge -> background (matches the LCD screen)
    largest enclosed white area      -> BODY (per-animation, see BODY_COLORS)
    other enclosed white areas       -> WHITE (eyes, open mouth, tear)

Requires: Pillow, numpy, scipy  (pip install pillow numpy scipy)
"""

import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage as ndi

# ---- May want to adjust later this section later ----
SPRITE_SIZE = 128          # final width/height on the LCD in pixels
INK_THRESHOLD = 110        # gray level (0-255) below which a pixel is "ink"
LABEL_CUTOFF = 0.845       # anything below this fraction is considered a label and dropped
LINE_COVERAGE = 0.22       # how much ink a final pixel needs to become outline

BACKGROUND = (0, 0, 0)     # this has to match the pet screen background in pet_graphics.c. 
OUTLINE = (40, 30, 60)
WHITE = (255, 255, 255)
DEFAULT_BODY = (255, 217, 102)
BODY_COLORS = {            # optional per-animation body color
    "dead": (200, 215, 240),
}
# ---------

ROOT = Path(__file__).resolve().parent.parent
MASTERS = ROOT / "docs" / "artwork" / "masters"
PREVIEW = ROOT / "docs" / "artwork" / "preview"
OUT_C = ROOT / "project-2-stm" / "Core" / "Src" / "pet_sprites.c"
OUT_H = ROOT / "project-2-stm" / "Core" / "Inc" / "pet_sprites.h"

BG, LINE, BODY, FILL = 0, 1, 2, 3
NAME_RE = re.compile(r"^([a-z]+)_(\d+)$")


def load_classes(path):
    """Label every pixel of the drawing as BG, LINE, BODY or FILL."""
    image = Image.open(path).convert("RGBA")
    flat = Image.new("RGBA", image.size, (255, 255, 255, 255))
    flat.alpha_composite(image)
    gray = np.asarray(flat.convert("L"))

    ink = gray < INK_THRESHOLD
    height, width = ink.shape

    # Drop labels: ink blobs that start near the bottom of the canvas.
    blobs, _ = ndi.label(ink, structure=np.ones((3, 3)))
    for index, region in enumerate(ndi.find_objects(blobs), start=1):
        if region is not None and region[0].start >= LABEL_CUTOFF * height:
            ink[blobs == index] = False

    # Square the canvas (centered) so the sprite is never stretched.
    side = max(height, width)
    pad_y, pad_x = (side - height) // 2, (side - width) // 2
    ink = np.pad(ink, ((pad_y, side - height - pad_y), (pad_x, side - width - pad_x)))

    areas, count = ndi.label(~ink)
    edge = set(np.unique(np.concatenate(
        [areas[0], areas[-1], areas[:, 0], areas[:, -1]]))) - {0}
    sizes = ndi.sum(~ink, areas, range(1, count + 1))
    inside = [i for i in range(1, count + 1) if i not in edge]
    body = max(inside, key=lambda i: sizes[i - 1]) if inside else None

    classes = np.full(ink.shape, BG, np.uint8)
    classes[ink] = LINE
    for i in inside:
        classes[areas == i] = BODY if i == body else FILL
    return classes


def downscale(classes):
    """Shrink to SPRITE_SIZE, keeping thin lines visible."""
    coverage = []
    for value in (BG, LINE, BODY, FILL):
        mask = Image.fromarray((classes == value).astype(np.uint8) * 255)
        mask = mask.resize((SPRITE_SIZE, SPRITE_SIZE), Image.BOX)
        coverage.append(np.asarray(mask, np.float32) / 255.0)
    coverage = np.stack(coverage)

    no_line = coverage.copy()
    no_line[LINE] = 0
    small = np.argmax(no_line, axis=0).astype(np.uint8)
    small[coverage[LINE] >= LINE_COVERAGE] = LINE
    return small


def to_rgb(small, animation):
    palette = {BG: BACKGROUND, LINE: OUTLINE, FILL: WHITE,
               BODY: BODY_COLORS.get(animation, DEFAULT_BODY)}
    rgb = np.zeros(small.shape + (3,), np.uint8)
    for value, color in palette.items():
        rgb[small == value] = color
    return rgb


def rgb565(rgb):
    r = rgb[..., 0].astype(np.uint16) >> 3
    g = rgb[..., 1].astype(np.uint16) >> 2
    b = rgb[..., 2].astype(np.uint16) >> 3
    return (r << 11) | (g << 5) | b


def main():
    masters = sorted(MASTERS.glob("*.png"))
    frames = []
    for path in masters:
        match = NAME_RE.match(path.stem)
        if not match:
            print(f"skipping {path.name}: name it like idle_1.png")
            continue
        animation = match.group(1)
        rgb = to_rgb(downscale(load_classes(path)), animation)
        frames.append((path.stem, rgb))
        print(f"converted {path.name}")

    if not frames:
        sys.exit(f"no frames found in {MASTERS}")

    PREVIEW.mkdir(parents=True, exist_ok=True)
    zoom, pad, columns = 3, 8, 3
    rows = (len(frames) + columns - 1) // columns
    cell = SPRITE_SIZE * zoom + pad
    sheet = Image.new("RGB", (columns * cell + pad, rows * cell + pad), (60, 60, 60))

    lines_h = [
        "/* Generated by tools/sprite_convert.py -- do not edit by hand. */",
        "#ifndef PET_SPRITES_H", "#define PET_SPRITES_H", "",
        "#include <stdint.h>", "",
        f"#define PET_SPRITE_W {SPRITE_SIZE}U",
        f"#define PET_SPRITE_H {SPRITE_SIZE}U", "",
    ]
    lines_c = [
        "/* Generated by tools/sprite_convert.py from docs/artwork/masters.",
        " * Do not edit by hand: edit the PNGs and re-run the script. */",
        '#include "pet_sprites.h"', "",
    ]

    for i, (stem, rgb) in enumerate(frames):
        image = Image.fromarray(rgb)
        image.save(PREVIEW / f"{stem}.png")
        sheet.paste(image.resize((SPRITE_SIZE * zoom,) * 2, Image.NEAREST),
                    (pad + (i % columns) * cell, pad + (i // columns) * cell))

        symbol = "PET_SPR_" + stem.upper()
        pixels = rgb565(rgb).flatten()
        lines_h.append(f"extern const uint16_t {symbol}[PET_SPRITE_W * PET_SPRITE_H];")
        lines_c.append(f"const uint16_t {symbol}[PET_SPRITE_W * PET_SPRITE_H] = {{")
        for start in range(0, len(pixels), 16):
            chunk = ", ".join(f"0x{p:04X}" for p in pixels[start:start + 16])
            lines_c.append(f"    {chunk},")
        lines_c += ["};", ""]

    lines_h += ["", "#endif", ""]
    sheet.save(PREVIEW / "all_frames.png")
    OUT_H.write_text("\n".join(lines_h), encoding="ascii")
    OUT_C.write_text("\n".join(lines_c), encoding="ascii")

    kb = len(frames) * SPRITE_SIZE * SPRITE_SIZE * 2 / 1024
    print(f"wrote {len(frames)} frames ({kb:.0f} KB of flash) to {OUT_C.name} / {OUT_H.name}")


if __name__ == "__main__":
    main()
