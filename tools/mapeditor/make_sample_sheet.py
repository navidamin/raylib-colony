"""Build a stand-in sprite sheet, so the editor can be tested without art.

The real sheet is not in the repo. This draws one with the same layout —
two large domes, two rows of unit domes, a connector, a long road, a short
road in the lower left, and five curves — and PRINTS THE TRUE ARC SPANS it
drew. The editor is then asked to measure them back from the pixels, which
is the only honest way to check that the measurement works.

Roads get a dashed centre line drawn as detached segments, because that is
what breaks naive component labelling: without the gap merge each dash
becomes its own unpickable sliver.

    python3 make_sample_sheet.py [out.png]
"""

import json
import math
import sys

from PIL import Image, ImageDraw

W, H = 1254, 1370
BG = (18, 18, 20)

ROAD = (150, 152, 158)
ROAD_EDGE = (96, 99, 106)
DASH = (232, 226, 190)
DOME_ON = (74, 210, 128)
DOME_OFF = (78, 84, 92)
RIM = (168, 174, 184)
DECK = (58, 62, 70)

ARC_START = 200.0       # every curve is drawn from the same heading
ARC_THICK = 44

# Curves: true span in degrees, radius, and where the piece's bounding box
# goes on the sheet. The spans are a mix of exact divisors of 360 and one
# that is not (22.5 is; 50 is not), so span quantising is exercised both
# ways.
ARCS = [
    dict(span=15.0, radius=340, x=545, y=1020),
    dict(span=30.0, radius=210, x=960, y=1020),
    dict(span=22.5, radius=260, x=350, y=1180),
    dict(span=50.0, radius=190, x=700, y=1180),
    dict(span=60.0, radius=130, x=1010, y=1180),
]


def dome(d, cx, cy, r, on):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=RIM)
    d.ellipse([cx - r + 7, cy - r + 7, cx + r - 7, cy + r - 7],
              fill=DOME_ON if on else DOME_OFF)
    for i in range(5):
        a = 0.9 * r * (i - 2) / 2.5
        d.rectangle([cx + a - 9, cy - r * 0.45, cx + a + 9, cy + r * 0.45],
                    fill=DECK)


def straight(d, x, y, length, thickness, grapplers=False):
    """A straight road piece, horizontal, top-left at (x, y)."""
    d.rectangle([x, y, x + length, y + thickness], fill=ROAD)
    d.rectangle([x, y, x + length, y + 5], fill=ROAD_EDGE)
    d.rectangle([x, y + thickness - 5, x + length, y + thickness],
                fill=ROAD_EDGE)
    cy = y + thickness // 2
    for dx in range(14, length - 26, 34):          # detached centre dashes
        d.rectangle([x + dx, cy - 4, x + dx + 18, cy + 4], fill=DASH)
    if grapplers:
        for gx in (x + 6, x + length - 28):
            d.rectangle([gx, y - 12, gx + 22, y + thickness + 12], fill=RIM)
    return (x, y - (12 if grapplers else 0),
            x + length, y + thickness + (12 if grapplers else 0))


def arc_bbox(radius, span, thickness):
    """Where the ink of an arc lands, relative to its circle centre."""
    xs, ys = [], []
    for i in range(200):
        a = math.radians(ARC_START + span * i / 199.0)
        for rr in (radius - thickness / 2.0, radius + thickness / 2.0):
            xs.append(rr * math.cos(a))
            ys.append(rr * math.sin(a))
    return min(xs), min(ys), max(xs), max(ys)


def arc(d, x, y, radius, span, thickness):
    """Draw an arc whose bounding box has its top-left at (x, y)."""
    bx0, by0, bx1, by1 = arc_bbox(radius, span, thickness)
    cx, cy = x - bx0, y - by0
    # PIL draws an arc's width inward from the bounding circle, so the
    # bounding radius is pushed out by half the width to leave the band
    # centred on `radius`.
    ro = radius + thickness / 2.0
    d.arc([cx - ro, cy - ro, cx + ro, cy + ro],
          ARC_START, ARC_START + span, fill=ROAD_EDGE, width=thickness)
    ri = radius + (thickness - 12) / 2.0
    d.arc([cx - ri, cy - ri, cx + ri, cy + ri],
          ARC_START, ARC_START + span, fill=ROAD, width=thickness - 12)
    n = max(2, int(span / 4.0))
    for i in range(n):                              # detached centre dashes
        a = math.radians(ARC_START + span * (i + 0.5) / n)
        px, py = cx + radius * math.cos(a), cy + radius * math.sin(a)
        d.ellipse([px - 4, py - 4, px + 4, py + 4], fill=DASH)
    return (x, y, x + (bx1 - bx0), y + (by1 - by0))


def check_gaps(boxes, gutter=24):
    """Every piece must be separated by more than the slicer's gutter."""
    bad = []
    for i, a in enumerate(boxes):
        for b in boxes[i + 1:]:
            dx = max(0, max(a[0], b[0]) - min(a[2], b[2]))
            dy = max(0, max(a[1], b[1]) - min(a[3], b[3]))
            if dx <= gutter and dy <= gutter:
                bad.append((a, b, dx, dy))
    return bad


def build(path):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    boxes = []

    # Two large central domes, on and off.
    for cx, on in ((410, True), (800, False)):
        dome(d, cx, 200, 175, on)
        boxes.append((cx - 175, 25, cx + 175, 375))

    # Two rows of unit domes.
    for i in range(4):
        for cy, on in ((520, True), (730, False)):
            dome(d, 285 + i * 205, cy, 82, on)
            boxes.append((203 + i * 205, cy - 82, 367 + i * 205, cy + 82))

    # Connector with end grapplers, then the two plain roads.
    boxes.append(straight(d, 130, 870, 545, 100, grapplers=True))
    boxes.append(straight(d, 105, 1010, 390, 95))
    boxes.append(straight(d, 105, 1180, 215, 92))     # the small one

    truth = []
    for a in ARCS:
        boxes.append(arc(d, a["x"], a["y"], a["radius"], a["span"],
                         ARC_THICK))
        truth.append(a["span"])

    bad = check_gaps(boxes)
    for a, b, dx, dy in bad:
        print(f"WARNING pieces too close: {a} {b} gap {dx},{dy}")

    img.save(path)
    print(f"wrote {path}   true arc spans: {truth}")
    with open(path + ".truth.json", "w") as f:
        json.dump({"arcs": truth}, f)
    return truth


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "sample_sheet.png")
