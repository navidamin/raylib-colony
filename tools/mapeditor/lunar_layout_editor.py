"""Lunar base layout editor.

The palette on the left IS the sprite sheet. Hover a piece to highlight it,
click to grab it onto the cursor, move to the map on the right, click to
drop. Rotate with Q/E or the wheel, snap to pieces already on the map,
scale 0.5x / 2x.

What changed from v2, and why
-----------------------------
v2 carried a hand-typed table of sprite rectangles and a hand-typed table
of arc angles. Both are now MEASURED FROM THE IMAGE:

* The background level is measured, not assumed. The sheet is drawn dark
  on dark and the inside of a road sits about six levels above the
  background, so a threshold picked by eye either splits every road into
  its two bright rails or swallows the gutters between pieces.

* Sprite cells are found by slicing the sheet at its background gutters
  (an X-Y cut, then a gap merge inside each cell), so every piece on the
  sheet is selectable and its rectangle is exactly the art. The short
  road in the lower left could not be picked in v2 because its typed
  rectangle missed the art. Nothing is typed here, so it cannot happen.

* Each curve's turn is measured from its own pixels, and that measurement
  is its rotation step, so N rotations chain N copies end to end. See
  centreline() and scan_centre() for how, and README.md for how well.

Run
---
    python3 lunar_layout_editor.py [sheet.png]      # lunar_sheet.png by default
    python3 lunar_layout_editor.py sheet.png --atlas atlas.png   # what it found
    python3 lunar_layout_editor.py sheet.png --demo demo.png     # drive it
    python3 lunar_layout_editor.py sheet.png --gutter 30 --threshold 40

Deps: pygame, pillow, numpy
"""

import json
import math
import os
import sys

import numpy as np
import pygame
from PIL import Image

# ---------------------------------------------------------------- config

WINDOW_W, WINDOW_H = 1500, 900
PALETTE_W = 430
BG = (22, 24, 27)
PANEL = (28, 31, 35)
GRID = (38, 42, 46)
WHITE = (225, 228, 232)
GREEN = (90, 255, 145)
YELLOW = (255, 210, 80)
CYAN = (90, 210, 255)
MUTED = (165, 170, 176)

SHEET_CANDIDATES = [
    "lunar_sheet.png",
    "sample_sheet.png",
    "a_clean_flat_game_asset_ui_sprite_sheet_style.png",
    "a_clean_game_asset_style_reference_sheet_on_a_dark.png",
]

BG_MARGIN = 6          # how far above the sheet's background art starts
GUTTER = 22            # px of background that separates two pieces
MIN_AREA = 700         # px; drops specks and compression noise
SNAP_RADIUS = 48       # px; how close two ports must be to snap


# ------------------------------------------------------- sheet analysis

def background_level(rgb):
    """The sheet's background brightness: the most common level in it.

    Worth measuring rather than assuming. On a sheet drawn dark-on-dark
    the inside of a road can sit six levels above the background and no
    more, so a threshold picked by eye either splits every road into its
    two bright rails or swallows the gutters between pieces.
    """
    v = rgb.max(axis=2)
    hist = np.bincount(v.ravel(), minlength=256)
    return int(np.argmax(hist))


def ink_mask(rgb, threshold):
    """True where the sheet has art rather than background."""
    return rgb.max(axis=2) >= threshold


def profile_runs(has_ink, gutter):
    """Runs of ink along one axis, ignoring background gaps < gutter."""
    idx = np.flatnonzero(has_ink)
    if len(idx) == 0:
        return []
    runs = []
    start = prev = idx[0]
    for i in idx[1:]:
        if i - prev > gutter:
            runs.append((start, prev))
            start = i
        prev = i
    runs.append((start, prev))
    return runs


def xy_cut(mask, ox, oy, gutter):
    """Slice a sheet at its widest background gutters, alternating axes.

    Cutting at gutters first is what stops a whole row of pieces from
    chain-merging into one box: the structural gaps between pieces are
    always wider than the gaps inside a piece.
    """
    bands = profile_runs(mask.any(axis=1), gutter)
    if len(bands) > 1:
        out = []
        for a, b in bands:
            out += xy_cut(mask[a:b + 1], ox, oy + a, gutter)
        return out
    cols = profile_runs(mask.any(axis=0), gutter)
    if len(cols) > 1:
        out = []
        for a, b in cols:
            out += xy_cut(mask[:, a:b + 1], ox + a, oy, gutter)
        return out
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return []
    return [(ox + int(xs.min()), oy + int(ys.min()),
             ox + int(xs.max()), oy + int(ys.max()))]


def label_components(mask):
    """Connected components via row runs + union-find. Returns bboxes.

    Row-run labelling rather than per-pixel flood fill: a 1254x1254 sheet
    is 1.5M pixels and per-pixel BFS in Python is slow enough to be felt
    on every launch.
    """
    h, w = mask.shape
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    runs_by_row = []
    next_id = 0
    for y in range(h):
        row = mask[y]
        if not row.any():
            runs_by_row.append([])
            continue
        edges = np.flatnonzero(
            np.diff(np.concatenate(([0], row.view(np.int8), [0]))))
        runs = []
        for i in range(0, len(edges), 2):
            x0, x1 = int(edges[i]), int(edges[i + 1]) - 1
            parent[next_id] = next_id
            runs.append((x0, x1, next_id))
            next_id += 1
        runs_by_row.append(runs)
        if y > 0:                       # 8-connectivity
            for x0, x1, rid in runs:
                for px0, px1, pid in runs_by_row[y - 1]:
                    if px1 >= x0 - 1 and px0 <= x1 + 1:
                        union(pid, rid)

    boxes = {}
    for y, runs in enumerate(runs_by_row):
        for x0, x1, rid in runs:
            r = find(rid)
            if r in boxes:
                bx0, by0, bx1, by1 = boxes[r]
                boxes[r] = (min(bx0, x0), min(by0, y),
                            max(bx1, x1), max(by1, y))
            else:
                boxes[r] = (x0, y, x1, y)
    return list(boxes.values())


def merge_boxes(boxes, gap):
    """Union boxes within `gap` of each other, to fixpoint."""
    boxes = list(boxes)
    changed = True
    while changed:
        changed = False
        out = []
        while boxes:
            x0, y0, x1, y1 = boxes.pop()
            merged = True
            while merged:
                merged = False
                rest = []
                for b in boxes:
                    if (x0 - gap <= b[2] and b[0] <= x1 + gap
                            and y0 - gap <= b[3] and b[1] <= y1 + gap):
                        x0, y0 = min(x0, b[0]), min(y0, b[1])
                        x1, y1 = max(x1, b[2]), max(y1, b[3])
                        merged = changed = True
                    else:
                        rest.append(b)
                boxes = rest
            out.append((x0, y0, x1, y1))
        boxes = out
    return boxes


def slice_sheet(mask, gutter=GUTTER):
    """Every piece on the sheet, as a tight bounding box."""
    cells = xy_cut(mask, 0, 0, gutter)
    boxes = []
    for x0, y0, x1, y1 in cells:
        sub = mask[y0:y1 + 1, x0:x1 + 1]
        parts = merge_boxes(label_components(sub), gutter)
        for bx0, by0, bx1, by1 in parts:
            boxes.append((int(x0 + bx0), int(y0 + by0),
                          int(x0 + bx1), int(y0 + by1)))
    boxes = [b for b in boxes
             if (b[2] - b[0] + 1) * (b[3] - b[1] + 1) >= MIN_AREA]
    boxes.sort(key=lambda b: (round(b[1] / 60.0), b[0]))
    return boxes


def pca_frame(pts):
    """Mean point plus the piece's long axis and its perpendicular."""
    mean = pts.mean(axis=0)
    _, _, vt = np.linalg.svd(pts - mean, full_matrices=False)
    axis = vt[0]
    perp = np.array([-axis[1], axis[0]])
    return mean, axis, perp


def bin_means(key, val, nbins, min_count=4):
    """Mean of val in equal-width bins of key. Returns keys, vals, counts."""
    lo, hi = float(key.min()), float(key.max())
    if hi - lo < 1e-6:
        return np.empty(0), np.empty(0), np.empty(0)
    idx = np.clip(((key - lo) / (hi - lo) * nbins).astype(int), 0, nbins - 1)
    ks, vs, ns = [], [], []
    for b in range(nbins):
        sel = idx == b
        n = int(sel.sum())
        if n >= min_count:
            ks.append(float(key[sel].mean()))
            vs.append(float(val[sel].mean()))
            ns.append(n)
    return np.array(ks), np.array(vs), np.array(ns)


def centreline(pts):
    """The centre of the ink along the piece, and the frame it lives in.

    A filled band is a bad thing to fit a circle to directly: over a
    short arc the band's own thickness swamps the few pixels of sagitta
    and an algebraic fit collapses onto the blob's centroid. So the
    centreline comes first — bin the ink along the piece's long axis and
    take the mean of each bin.

    The ends are dropped. Where the piece is cut off, a bin holds only
    the corner of the band, so its mean sits off the centreline by
    several pixels: more than the whole signal on a shallow curve, and
    enough on its own to flatten the fit into a straight line.

    Returns (mid point, unit normal at it, chord length, sagitta) or None.
    """
    mean, axis, perp = pca_frame(pts)
    rel = pts - mean
    u, v = rel @ axis, rel @ perp
    bu, bv, bn = bin_means(u, v, 48)
    if len(bu) < 10:
        return None
    lo, hi = float(u.min()), float(u.max())
    inset = 0.10 * (hi - lo)
    keep = (bn >= 0.55 * np.median(bn)) & (bu > lo + inset) & (bu < hi - inset)
    if keep.sum() < 8:
        return None
    bu, bv = bu[keep], bv[keep]

    poly = np.polyfit(bu, bv, 2)
    chord = float(bu.max() - bu.min())
    sagitta = abs(poly[0]) * chord * chord / 4.0
    if sagitta < 0.8:
        return None                     # straight within measurement noise
    um = float(bu.mean())
    slope = 2.0 * poly[0] * um + poly[1]
    mid = mean + um * axis + float(np.polyval(poly, um)) * perp
    n = np.array([-slope, 1.0]) / math.hypot(slope, 1.0)
    normal = n[0] * axis + n[1] * perp
    if poly[0] < 0:
        normal = -normal
    guess = (1.0 + slope * slope) ** 1.5 / (2.0 * abs(poly[0]))
    return mid, normal, chord, guess


def radial_flatness(pts, centre, nbins=36):
    """How annular the ink looks from a candidate centre.

    Seen from the true centre of a curved road, the mean distance to the
    ink is the same at every angle. That is the whole test: sweep the
    angles, take the mean radius in each, and measure how much it drifts.
    """
    rel = pts - centre
    ang = np.degrees(np.arctan2(rel[:, 1], rel[:, 0])) % 360.0
    rad = np.hypot(rel[:, 0], rel[:, 1])
    s = np.sort(ang)
    gaps = np.diff(np.concatenate([s, [s[0] + 360.0]]))
    start = float(s[(int(np.argmax(gaps)) + 1) % len(s)])
    rot = (ang - start) % 360.0
    ba, br, bn = bin_means(rot, rad, nbins)
    if len(br) < 6:
        return 1e9, start, 0.0
    trim = slice(1, -1) if len(br) > 6 else slice(None)
    return float(br[trim].std()), start, float(rot.max())


def scan_centre(pts, mid, normal, chord):
    """Sweep the centre along the normal and keep the most annular one.

    The circle's centre has to lie on the normal to the centreline, so
    the search is really one-dimensional: walk out along it and see
    which distance makes the ink look most like part of a ring. Sweeping
    beats trusting the parabola's own radius, which is only reliable
    while the curve is shallow — on a piece that turns far enough for
    the chord-wise binning to bias the centreline, the parabola can be
    out by a factor of two and a local search never recovers.
    """
    best = None
    for sign in (1.0, -1.0):
        for k in range(80):
            radius = 0.25 * chord * (80.0) ** (k / 79.0)
            cand = mid + sign * normal * radius
            score, _, _ = radial_flatness(pts, cand)
            if best is None or score < best[0]:
                best = (score, cand, radius)
    return best[1], best[2]


def refine_centre(pts, centre, radius):
    """Pattern search for the centre that makes the ink most annular.

    Cheap, derivative-free and stable: try eight directions, keep any
    improvement, halve the step when none helps. Beats another algebraic
    circle fit here because the ink is a filled band, not a thin curve.
    """
    centre = np.array(centre, dtype=float)
    best, _, _ = radial_flatness(pts, centre)
    step = max(2.0, 0.25 * radius)
    while step > 0.4:
        moved = False
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1),
                       (0.7, 0.7), (-0.7, 0.7), (0.7, -0.7), (-0.7, -0.7)):
            cand = centre + np.array([dx, dy]) * step
            score, _, _ = radial_flatness(pts, cand)
            if score < best:
                best, centre, moved = score, cand, True
        if not moved:
            step *= 0.5
    _, start, span = radial_flatness(pts, centre)
    rel = pts - centre
    return centre, float(np.hypot(rel[:, 0], rel[:, 1]).mean()), start, span


def quantise_span(span, tol=None):
    """Pull a measured span onto an exact divisor of 360 if it is close.

    The artist drew, say, 45 degrees; the pixels measure 45.4, and eight
    rotations of 45.4 overshoot by 3 degrees so the chain visibly fails
    to close. The tolerance is proportional because a shallow curve in a
    small box carries only a few pixels of sagitta and cannot be measured
    as tightly as a deep one.
    """
    if tol is None:
        tol = max(1.2, 0.09 * span)
    best, err = span, tol
    for n in range(2, 49):
        cand = 360.0 / n
        d = abs(span - cand)
        if d < err:
            best, err = cand, d
    return best


def fit_circle(pts):
    """Kasa algebraic circle fit through points that lie on a circle."""
    a = np.column_stack([pts[:, 0], pts[:, 1], np.ones(len(pts))])
    b = pts[:, 0] ** 2 + pts[:, 1] ** 2
    sol, *_ = np.linalg.lstsq(a, b, rcond=None)
    cx, cy = sol[0] / 2.0, sol[1] / 2.0
    return np.array([cx, cy]), math.sqrt(max(1e-6, sol[2] + cx * cx + cy * cy))


def analyse_dome(pts, w, h, nbins=180, lug=1.06):
    """A dome's rim, and the sockets sticking out of it.

    The sockets are where roads actually plug in, so they are found
    rather than assumed: sweep the angles, take the furthest ink in each,
    and anything reaching past the rim by six percent is a lug. A unit
    dome has exactly one, and a road snapped to it turns the dome so that
    socket faces the road.

    The centre comes from a circle fit to the rim, not the centroid: a
    single lug on one side drags the centroid toward it, and then the
    opposite side of the rim reads as another lug.
    """
    centre = pts.mean(axis=0)
    rim = 0.25 * (w + h)
    for _ in range(3):
        rel = pts - centre
        rad = np.hypot(rel[:, 0], rel[:, 1])
        ang = np.degrees(np.arctan2(rel[:, 1], rel[:, 0])) % 360.0
        idx = np.clip((ang / 360.0 * nbins).astype(int), 0, nbins - 1)
        reach = np.array([rad[idx == b].max() if (idx == b).any() else 0.0
                          for b in range(nbins)])
        rim = float(np.median(reach[reach > 0]))
        on_rim = pts[(rad > 0.93 * rim) & (rad < 1.03 * rim)]
        if len(on_rim) < 24:
            break
        centre, _ = fit_circle(on_rim)

    # Re-measure from the settled centre: the lug test compares reach
    # against the rim, so both have to be seen from the same place.
    rel = pts - centre
    rad = np.hypot(rel[:, 0], rel[:, 1])
    ang = np.degrees(np.arctan2(rel[:, 1], rel[:, 0])) % 360.0
    idx = np.clip((ang / 360.0 * nbins).astype(int), 0, nbins - 1)
    reach = np.array([rad[idx == b].max() if (idx == b).any() else 0.0
                      for b in range(nbins)])
    rim = float(np.median(reach[reach > 0]))

    out = reach > rim * lug
    runs, b = [], 0
    while b < nbins:
        if out[b]:
            start = b
            while b < nbins and out[b]:
                b += 1
            runs.append((start, b - 1))
        else:
            b += 1
    if len(runs) > 1 and out[0] and out[nbins - 1]:     # a lug across 0 deg
        runs = [(runs[-1][0] - nbins, runs[0][1])] + runs[1:-1]
    runs = [r for r in runs if 2 <= (r[1] - r[0] + 1) <= 45]

    ports, socket_w = [], 0.0
    for start, end in runs:
        mid = math.radians(((start + end) / 2.0 % nbins) * 360.0 / nbins)
        span = [i % nbins for i in range(start, end + 1)]
        r = float(reach[span].max())
        # How wide the socket is where a road would meet it - the number
        # that says whether a road plugged in here overhangs.
        socket_w = max(socket_w, 2.0 * r * math.sin(
            math.radians(len(span) * 360.0 / nbins / 2.0)))
        # The port sits on the RIM, not on the lug's outer face. A road
        # brought here lands its rail ends on the rim and its own socket
        # then lies over the dome's lug, which is how the art is drawn to
        # go together - butting the two outer faces instead leaves the
        # two sockets nose to nose with the road stopping short.
        ports.append(((centre[0] + rim * math.cos(mid),
                       centre[1] + rim * math.sin(mid)),
                      (math.cos(mid), math.sin(mid))))
    if not ports:                       # a dome with no visible socket
        for k in range(8):
            a = math.radians(k * 45.0)
            ports.append(((centre[0] + rim * math.cos(a),
                           centre[1] + rim * math.sin(a)),
                          (math.cos(a), math.sin(a))))
    return centre, rim, ports, socket_w


def analyse_shape(mask, box):
    """Recover a piece's geometry from its pixels.

    Returns its kind (arc / road / dome), the measured arc span, and the
    connection ports in sprite-local coordinates with the outgoing
    heading at each. Everything the editor needs to rotate and snap a
    piece is derived here rather than declared.
    """
    x0, y0, x1, y1 = box
    sub = mask[y0:y1 + 1, x0:x1 + 1]
    ys, xs = np.nonzero(sub)
    if len(xs) < 40:
        return None
    w, h = x1 - x0 + 1, y1 - y0 + 1
    pts = np.column_stack([xs.astype(float), ys.astype(float)])

    # A dome is a compact blob that fills its box. It gets ports around
    # its rim, so roads can be run into it from any of eight headings.
    fill = len(xs) / float(w * h)
    if fill > 0.55 and max(w, h) < 1.5 * min(w, h):
        centre, rim, ports, socket_w = analyse_dome(pts, w, h)
        return {"kind": "dome", "arc": False, "span": 0.0, "measured": 0.0,
                "radius": rim, "width": socket_w,
                "centre": (float(centre[0]), float(centre[1])),
                "ports": ports, "size": (w, h)}

    curved = centreline(pts)
    if curved is not None:
        mid, normal, chord, guess = curved
        sample = (pts if len(pts) <= 4000
                  else pts[np.linspace(0, len(pts) - 1, 4000).astype(int)])
        # Two starting points, because neither is reliable alone: the
        # parabola's own radius is good while the curve is shallow and
        # useless once it turns far enough to bias the binning, and the
        # sweep is the other way round. Refine from both, keep whichever
        # ends up looking more like part of a ring.
        cands = [scan_centre(sample, mid, normal, chord),
                 (mid + normal * guess, guess)]
        best = None
        for c0, r0 in cands:
            centre, radius, start, span = refine_centre(sample, c0, r0)
            score, _, _ = radial_flatness(sample, centre)
            if best is None or score < best[0]:
                best = (score, centre, radius, start, span)
        _, centre, radius, start, span = best
        if 2.0 < span < 300.0:
            ports = []
            for a, sign in ((start, -1.0), (start + span, 1.0)):
                t = math.radians(a + sign * 90.0)
                ports.append(((centre[0] + radius * math.cos(math.radians(a)),
                               centre[1] + radius * math.sin(math.radians(a))),
                              (math.cos(t), math.sin(t))))
            return {"kind": "arc", "arc": True, "span": quantise_span(span),
                    "measured": span, "radius": float(radius), "width": 0.0,
                    "centre": (float(centre[0]), float(centre[1])),
                    "ports": ports, "size": (w, h)}

    # Straight piece: principal axis, then the centreline point at each end.
    mean, axis, perp = pca_frame(pts)
    proj = (pts - mean) @ axis
    lo, hi = float(proj.min()), float(proj.max())
    # Deck width, measured across the middle third so the end sockets -
    # which stand proud of the rails - do not inflate it. This is the
    # number that says whether two straight pieces butt together cleanly.
    across = (pts - mean) @ perp
    middle = np.abs(proj) < 0.17 * (hi - lo)
    width = (float(across[middle].max() - across[middle].min())
             if middle.sum() > 20 else float(across.max() - across.min()))

    # Where the rails stop. A socketed piece carries a bracket at each
    # end that stands proud of the deck, and the rails end at the inside
    # of it. That is the point which has to land on a dome's rim, so the
    # bracket ends up lying over the dome's lug rather than nose to nose
    # with it. A plain road has no bracket and its rails run to the end.
    nb = max(8, int(hi - lo))
    slot = np.clip(((proj - lo) / (hi - lo) * nb).astype(int), 0, nb - 1)
    reach = np.zeros(nb)
    for b in range(nb):
        sel = slot == b
        if sel.any():
            # Extent, not twice the largest offset: the piece's centroid
            # is not always on its centreline, and doubling from an
            # off-centre origin invents height that is not there.
            reach[b] = float(across[sel].max() - across[sel].min())
    proud = reach > width * 1.06

    def bracket_end(flags):
        """How far in the end bracket reaches, in columns.

        Walk inward from the first proud column, tolerating short gaps -
        a bracket's rounded corners and its hollow middle both drop below
        full height for a few columns, and the outermost columns of all
        are the corner radius, so the walk cannot start at the very end.
        Nothing proud near the end means there is no bracket, which is
        the case for the plain roads; without that check a single noisy
        column halfway along would be read as one and trim the piece by
        a third.
        """
        head = np.flatnonzero(flags[:max(10, len(flags) // 10)])
        if not len(head):
            return 0
        last, gap = int(head[0]), 0
        for i in range(int(head[0]), len(flags)):
            if flags[i]:
                last, gap = i, 0
            else:
                gap += 1
                if gap > 5:
                    break
        return last + 1

    span = max(4, nb // 4)
    rail_lo = lo + bracket_end(proud[:span]) * (hi - lo) / nb
    rail_hi = hi - bracket_end(proud[::-1][:span]) * (hi - lo) / nb

    ports = []
    for target, sign in ((rail_lo, -1.0), (rail_hi, 1.0)):
        # Exactly on the rail end along the piece, centred across it.
        # Averaging the nearby ink instead would pull the port a few
        # pixels inboard, and those pixels are the overlap.
        sel = np.abs(proj - target) < max(2.0, 0.04 * (hi - lo))
        off = float(across[sel].mean()) if sel.any() else 0.0
        p = mean + target * axis + off * perp
        d = axis * sign
        ports.append(((float(p[0]), float(p[1])),
                      (float(d[0]), float(d[1]))))
    return {"kind": "road", "arc": False, "span": 0.0, "measured": 0.0,
            "radius": 0.0, "width": width, "centre": None, "ports": ports,
            "size": (w, h)}


def crop_sprite(sheet_img, box, bg):
    """Cut the sprite out and fade the background to transparent.

    A ramp rather than a hard cut: on a dark-on-dark sheet the darkest
    parts of a road are only a few levels above the background, and
    clipping them leaves the piece full of holes.
    """
    x0, y0, x1, y1 = box
    crop = sheet_img.crop((x0, y0, x1 + 1, y1 + 1)).convert("RGBA")
    arr = np.array(crop).astype(np.int32)
    v = arr[:, :, :3].max(axis=2)
    arr[:, :, 3] = np.clip((v - bg - 1) * 255 // 5, 0, 255)
    return pygame.image.frombuffer(
        np.ascontiguousarray(arr.astype(np.uint8)).tobytes(),
        crop.size, "RGBA").convert_alpha()


class Sprite:
    """One pickable piece, entirely derived from the sheet."""

    def __init__(self, index, box, shape, surface):
        self.index = index
        self.box = box
        self.shape = shape
        self.surface = surface
        self.is_arc = shape["arc"]
        self.span = shape["span"]
        # A curve's rotation quantum is its own arc span, so chaining N
        # copies closes exactly. Everything else rotates in plain 15
        # degree steps by hand, but snaps to whatever angle the joint
        # actually needs - a dome has to be able to turn its socket to
        # face the road, whatever heading the road arrived on.
        self.rot_step = self.span if (self.is_arc and self.span > 1.0) else 15.0
        self.snap_free = not self.is_arc
        w, h = surface.get_size()
        self.centre = (w / 2.0, h / 2.0)
        self.ports = shape["ports"]

    @property
    def label(self):
        w, h = self.surface.get_size()
        if self.is_arc:
            return f"arc {self.span:g} deg  r{self.shape['radius']:.0f}"
        if self.shape["kind"] == "road":
            return f"road {w}x{h} deck {self.shape['width']:.0f}"
        return (f"dome {w}x{h} r{self.shape['radius']:.0f} "
                f"socket {self.shape['width']:.0f}")


def report_atlas(sprites, path):
    print(f"{os.path.basename(path)}: detected {len(sprites)} sprites")
    for s in sprites:
        if s.is_arc:
            extra = (f"   measured {s.shape['measured']:.2f} deg -> step "
                     f"{s.rot_step:g}")
        else:
            extra = f"   {len(s.ports)} port(s)"
        print(f"  [{s.index:2d}] {s.label:22s} box={s.box}{extra}")


def load_span_overrides(sheet_path):
    """Hand-corrected arc spans, if the user has written any down.

    The measurement is good to about a degree and a half, which resolves
    the coarse divisors of 360 but not the fine ones - below roughly 25
    degrees, 24 / 22.5 / 20 / 18 sit closer together than that. A file
    named after the sheet, e.g. `lunar_sheet.spans.json` holding
    {"12": 45, "16": 45}, overrides those sprites by index.
    """
    path = os.path.splitext(sheet_path)[0] + ".spans.json"
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        data = json.load(f)
    print(f"span overrides from {os.path.basename(path)}: {data}")
    return {int(k): float(v) for k, v in data.items()}


def build_atlas(path, gutter=GUTTER, threshold=None):
    img = Image.open(path).convert("RGB")
    rgb = np.array(img)
    bg = background_level(rgb)
    if threshold is None:
        threshold = bg + BG_MARGIN
    print(f"background level {bg}, ink threshold {threshold}")
    mask = ink_mask(rgb, threshold)
    overrides = load_span_overrides(path)
    sprites = []
    for box in slice_sheet(mask, gutter):
        shape = analyse_shape(mask, box)
        if shape is None:
            continue
        sprite = Sprite(len(sprites), box, shape, crop_sprite(img, box, bg))
        if sprite.index in overrides and sprite.is_arc:
            sprite.span = sprite.rot_step = overrides[sprite.index]
        sprites.append(sprite)
    return img, sprites


def write_atlas_debug(img, sprites, path):
    """Render the sheet with every detected box drawn on it."""
    from PIL import ImageDraw
    out = img.convert("RGB").copy()
    d = ImageDraw.Draw(out)
    for s in sprites:
        x0, y0, x1, y1 = s.box
        col = (90, 255, 145) if s.is_arc else (255, 210, 80)
        d.rectangle([x0, y0, x1, y1], outline=col, width=3)
        d.text((x0 + 4, y0 + 4), f"{s.index}:{s.label}", fill=col)
    out.save(path)
    print("wrote", path)


# ------------------------------------------------------------- placement

class Placed:
    def __init__(self, sprite, pos, angle=0.0, scale=1.0):
        self.sprite = sprite
        self.pos = [float(pos[0]), float(pos[1])]
        self.angle = angle
        self.scale = scale

    def image(self):
        return pygame.transform.rotozoom(self.sprite.surface, -self.angle,
                                         self.scale)

    def rect(self):
        r = self.image().get_rect()
        r.center = (int(self.pos[0]), int(self.pos[1]))
        return r

    def draw(self, screen, tint=None):
        img = self.image()
        if tint:
            img = img.copy()
            img.fill(tint, special_flags=pygame.BLEND_RGBA_ADD)
        screen.blit(img, self.rect())

    def rotate(self, direction):
        self.angle = (self.angle + direction * self.sprite.rot_step) % 360.0

    def world_ports(self):
        """Ports in map coordinates, with their outgoing headings."""
        out = []
        cx, cy = self.sprite.centre
        rad = math.radians(self.angle)
        ca, sa = math.cos(rad), math.sin(rad)
        for (px, py), (dx, dy) in self.sprite.ports:
            lx, ly = (px - cx) * self.scale, (py - cy) * self.scale
            out.append(((self.pos[0] + lx * ca - ly * sa,
                         self.pos[1] + lx * sa + ly * ca),
                        (dx * ca - dy * sa, dx * sa + dy * ca)))
        return out


def free_ports(obj, others, tol=4.0):
    """The ports of obj that nothing else is already joined to."""
    taken = [p for o in others if o is not obj for p, _ in o.world_ports()]
    out = []
    for i, (p, h) in enumerate(obj.world_ports()):
        if not any(math.hypot(p[0] - q[0], p[1] - q[1]) < tol for q in taken):
            out.append((i, p, h))
    return out


def snap_to_neighbours(obj, others, radius=SNAP_RADIUS):
    """Align a piece to the nearest free port of an already placed one.

    Rotates so the two headings oppose (the pieces run head to tail),
    then translates so the ports coincide. Rotation is quantised to the
    piece's own step, which is what makes chained curves line up instead
    of drifting by a fraction of a degree each time. Ports that already
    have something joined to them are skipped, so a growing chain keeps
    extending from its open end instead of doubling back on itself.
    """
    best = None
    for other in others:
        if other is obj:
            continue
        for _, tp, th in free_ports(other, [o for o in others if o is not obj]):
            for i, (mp, _) in enumerate(obj.world_ports()):
                d = math.hypot(tp[0] - mp[0], tp[1] - mp[1])
                if d < radius and (best is None or d < best[0]):
                    best = (d, i, tp, th)
    if best is None:
        return False

    _, port_i, target_pos, target_head = best
    my_head = obj.world_ports()[port_i][1]
    want = math.degrees(math.atan2(-target_head[1], -target_head[0]))
    have = math.degrees(math.atan2(my_head[1], my_head[0]))
    delta = (want - have + 180.0) % 360.0 - 180.0
    if obj.sprite.snap_free:
        obj.angle = (obj.angle + delta) % 360.0
    else:
        step = obj.sprite.rot_step
        obj.angle = (obj.angle + round(delta / step) * step) % 360.0

    mp = obj.world_ports()[port_i][0]
    obj.pos[0] += target_pos[0] - mp[0]
    obj.pos[1] += target_pos[1] - mp[1]
    return True


# ------------------------------------------------------------------- ui

def draw_grid(screen, rect):
    for x in range(rect.left, rect.right, 40):
        pygame.draw.line(screen, GRID, (x, rect.top), (x, rect.bottom))
    for y in range(rect.top, rect.bottom, 40):
        pygame.draw.line(screen, GRID, (rect.left, y), (rect.right, y))


def button(screen, rect, label, font, on=False):
    pygame.draw.rect(screen, (52, 58, 66) if on else (38, 42, 48), rect,
                     border_radius=4)
    pygame.draw.rect(screen, (95, 105, 120), rect, 1, border_radius=4)
    t = font.render(label, True, WHITE)
    screen.blit(t, t.get_rect(center=rect.center))


def find_sheet(argv):
    for a in argv[1:]:
        if not a.startswith("-") and os.path.exists(a):
            return a
    here = os.path.dirname(os.path.abspath(__file__))
    for name in SHEET_CANDIDATES:
        for base in (os.getcwd(), here):
            p = os.path.join(base, name)
            if os.path.exists(p):
                return p
    raise SystemExit(
        "No sprite sheet found. Pass one as an argument, or drop it beside\n"
        "the script as one of: " + ", ".join(SHEET_CANDIDATES))


def save_layout(placed, path="layout.json"):
    data = [{"sprite": p.sprite.index, "box": p.sprite.box,
             "pos": p.pos, "angle": p.angle, "scale": p.scale}
            for p in placed]
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"saved {len(data)} pieces to {path}")


class Editor:
    """The whole editor. Split out so a script can drive it headlessly."""

    def __init__(self, sheet_path, gutter=GUTTER, threshold=None):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
        pygame.display.set_caption("Lunar Base Layout Editor v3")
        self.font = pygame.font.SysFont("dejavusans", 15)
        self.small = pygame.font.SysFont("dejavusans", 13)
        self.clock = pygame.time.Clock()

        self.sheet_img, self.sprites = build_atlas(sheet_path, gutter,
                                                   threshold)
        report_atlas(self.sprites, sheet_path)

        sw, sh = self.sheet_img.size
        self.pal_scale = min((PALETTE_W - 24) / sw, (WINDOW_H - 230) / sh)
        surf = pygame.image.frombuffer(
            self.sheet_img.tobytes(), (sw, sh), "RGB").convert()
        self.pal_surf = pygame.transform.smoothscale(
            surf, (int(sw * self.pal_scale), int(sh * self.pal_scale)))
        self.pal_pos = (12, 150)

        self.map_rect = pygame.Rect(PALETTE_W, 0, WINDOW_W - PALETTE_W,
                                    WINDOW_H)
        self.placed = []
        self.carried = None      # sprite riding the cursor
        self.selected = None     # placed piece under edit
        self.dragging = None
        self.snap_on = True
        self.show_ports = False
        self.place_scale = 1.0
        self.mouse = (0, 0)
        self.sim_mouse = None    # set by a script to drive headlessly

        self.btn_half = pygame.Rect(12, 60, 80, 30)
        self.btn_double = pygame.Rect(100, 60, 80, 30)
        self.btn_snap = pygame.Rect(188, 60, 92, 30)
        self.btn_ports = pygame.Rect(288, 60, 92, 30)
        self.btn_clear = pygame.Rect(12, 100, 120, 28)
        self.btn_save = pygame.Rect(140, 100, 110, 28)

    # -------------------------------------------------------- interaction

    @property
    def target(self):
        return self.dragging or self.carried or self.selected

    def palette_hit(self, mx, my):
        """Which sprite is under the cursor, in sheet coordinates."""
        px = (mx - self.pal_pos[0]) / self.pal_scale
        py = (my - self.pal_pos[1]) / self.pal_scale
        for s in self.sprites:
            x0, y0, x1, y1 = s.box
            if x0 <= px <= x1 and y0 <= py <= y1:
                return s
        return None

    def scale_by(self, factor):
        if self.target:
            self.target.scale = min(8.0, max(0.05, self.target.scale * factor))
        else:
            self.place_scale = min(8.0, max(0.05, self.place_scale * factor))

    def click(self, mx, my):
        for rect, action in ((self.btn_half, lambda: self.scale_by(0.5)),
                             (self.btn_double, lambda: self.scale_by(2.0)),
                             (self.btn_snap, self.toggle_snap),
                             (self.btn_ports, self.toggle_ports),
                             (self.btn_clear, self.clear_map),
                             (self.btn_save, lambda: save_layout(self.placed))):
            if rect.collidepoint(mx, my):
                action()
                return
        if mx < PALETTE_W:
            hit = self.palette_hit(mx, my)
            if hit:
                self.carried = Placed(hit, (mx, my), scale=self.place_scale)
                self.selected = None
            return
        if self.carried:
            self.carried.pos = [float(mx), float(my)]
            if self.snap_on:
                snap_to_neighbours(self.carried, self.placed)
            self.placed.append(self.carried)
            self.selected = self.carried
            self.carried = None
            return
        self.selected = None
        for obj in reversed(self.placed):
            if obj.rect().collidepoint(mx, my):
                self.selected = self.dragging = obj
                break

    def toggle_snap(self):
        self.snap_on = not self.snap_on

    def toggle_ports(self):
        self.show_ports = not self.show_ports

    def clear_map(self):
        self.placed.clear()
        self.selected = None

    def handle(self, event):
        mx, my = self.mouse
        if event.type == pygame.QUIT:
            return False
        if event.type == pygame.KEYDOWN:
            t = self.target
            if event.key == pygame.K_ESCAPE:
                if self.carried:
                    self.carried = None
                else:
                    self.selected = None
            elif event.key in (pygame.K_e, pygame.K_RIGHT):
                if t: t.rotate(1)
            elif event.key in (pygame.K_q, pygame.K_LEFT):
                if t: t.rotate(-1)
            elif event.key == pygame.K_g:
                self.toggle_snap()
            elif event.key == pygame.K_p:
                self.toggle_ports()
            elif event.key in (pygame.K_DELETE, pygame.K_BACKSPACE):
                if self.selected in self.placed:
                    self.placed.remove(self.selected)
                    self.selected = None
            elif event.key == pygame.K_LEFTBRACKET:
                self.scale_by(0.5)
            elif event.key == pygame.K_RIGHTBRACKET:
                self.scale_by(2.0)
        elif event.type == pygame.MOUSEWHEEL:
            if self.target:
                self.target.rotate(1 if event.y > 0 else -1)
        elif event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 3:
                self.carried = self.selected = None
            elif event.button == 1:
                self.click(mx, my)
        elif event.type == pygame.MOUSEBUTTONUP:
            if event.button == 1 and self.dragging:
                if self.snap_on:
                    snap_to_neighbours(self.dragging, self.placed)
                self.dragging = None
        elif event.type == pygame.MOUSEMOTION:
            if self.dragging:
                self.dragging.pos = [float(mx), float(my)]
        return True

    # --------------------------------------------------------------- draw

    def draw(self):
        mx, my = self.mouse
        screen = self.screen
        screen.fill(BG)
        pygame.draw.rect(screen, PANEL, (0, 0, PALETTE_W, WINDOW_H))
        draw_grid(screen, self.map_rect)

        for obj in self.placed:
            obj.draw(screen, (30, 30, 30) if obj is self.selected else None)
        if self.selected:
            pygame.draw.rect(screen, CYAN, self.selected.rect(), 1)

        if self.show_ports:
            for obj in self.placed:
                for p, hdg in obj.world_ports():
                    pygame.draw.circle(screen, YELLOW,
                                       (int(p[0]), int(p[1])), 4)
                    pygame.draw.line(screen, YELLOW, p,
                                     (p[0] + hdg[0] * 20, p[1] + hdg[1] * 20), 2)

        if self.carried:
            ghost = self.carried.image()
            ghost.set_alpha(180)
            screen.blit(ghost, self.carried.rect())

        screen.blit(self.pal_surf, self.pal_pos)
        hovered = self.palette_hit(mx, my) if mx < PALETTE_W else None
        for s, col in ((hovered, GREEN),
                       (self.carried.sprite if self.carried else None, YELLOW)):
            if not s:
                continue
            x0, y0, x1, y1 = s.box
            pygame.draw.rect(screen, col, pygame.Rect(
                self.pal_pos[0] + x0 * self.pal_scale,
                self.pal_pos[1] + y0 * self.pal_scale,
                (x1 - x0) * self.pal_scale, (y1 - y0) * self.pal_scale), 2)

        screen.blit(self.font.render("LUNAR LAYOUT EDITOR", True, WHITE),
                    (12, 12))
        screen.blit(self.small.render(
            f"{len(self.sprites)} sprites detected from the sheet   "
            f"new-piece scale {self.place_scale:g}x", True, MUTED), (12, 34))
        button(screen, self.btn_half, "scale 0.5x", self.small)
        button(screen, self.btn_double, "scale 2x", self.small)
        button(screen, self.btn_snap,
               "snap: " + ("on" if self.snap_on else "off"),
               self.small, self.snap_on)
        button(screen, self.btn_ports,
               "ports: " + ("on" if self.show_ports else "off"),
               self.small, self.show_ports)
        button(screen, self.btn_clear, "clear map", self.small)
        button(screen, self.btn_save, "save layout", self.small)

        info_y = WINDOW_H - 56
        if hovered:
            screen.blit(self.small.render(
                f"hover: {hovered.label}   rotation step "
                f"{hovered.rot_step:g} deg", True, GREEN), (12, info_y))
        elif self.target:
            t = self.target
            screen.blit(self.small.render(
                f"{t.sprite.label}   angle {t.angle:g}   scale {t.scale:g}x",
                True, CYAN), (12, info_y))
        screen.blit(self.small.render(
            "click palette to pick  -  click map to drop  -  Q/E or wheel "
            "rotate  -  G snap  -  P ports  -  Del remove  -  right-click "
            "cancel", True, MUTED), (PALETTE_W + 12, WINDOW_H - 26))

    def run(self):
        running = True
        while running:
            self.mouse = pygame.mouse.get_pos()
            for event in pygame.event.get():
                running = self.handle(event) and running
            if self.carried:
                self.carried.pos = list(self.mouse)
            self.draw()
            pygame.display.flip()
            self.clock.tick(60)
        pygame.quit()

    # Headless driving: move the cursor, click, press keys, take a shot.
    def move(self, x, y):
        self.mouse = (x, y)
        if self.carried:
            self.carried.pos = [float(x), float(y)]

    def press(self, button=1):
        self.handle(pygame.event.Event(pygame.MOUSEBUTTONDOWN,
                                       button=button, pos=self.mouse))

    def release(self, button=1):
        self.handle(pygame.event.Event(pygame.MOUSEBUTTONUP,
                                       button=button, pos=self.mouse))

    def key(self, k):
        self.handle(pygame.event.Event(pygame.KEYDOWN, key=k))

    def palette_point(self, sprite):
        """Where a sprite sits on screen in the palette."""
        x0, y0, x1, y1 = sprite.box
        return (int(self.pal_pos[0] + (x0 + x1) / 2 * self.pal_scale),
                int(self.pal_pos[1] + (y0 + y1) / 2 * self.pal_scale))

    def pick_and_drop(self, sprite, at):
        """Palette click, then map click — the same path a player takes."""
        self.move(*self.palette_point(sprite))
        self.press(); self.release()
        self.move(int(at[0]), int(at[1]))
        self.press(); self.release()
        return self.placed[-1]

    def shot(self, path):
        self.draw()
        pygame.display.flip()
        pygame.image.save(self.screen, path)
        print("wrote", path)


def aim(obj, direction):
    """Turn a piece so one of its ports faces the given direction."""
    best = max(obj.world_ports(),
               key=lambda p: p[1][0] * direction[0] + p[1][1] * direction[1])
    want = math.degrees(math.atan2(direction[1], direction[0]))
    have = math.degrees(math.atan2(best[1][1], best[1][0]))
    obj.angle = (obj.angle + (want - have + 180.0) % 360.0 - 180.0) % 360.0
    return obj


def attach(ed, sprite, anchor, scale, toward=None):
    """Drop a piece onto a free port of an already placed one.

    `toward` picks which free port to leave from when the anchor has
    several - a big dome has eight sockets and only one of them is the
    way the chain is heading.
    """
    ed.place_scale = scale
    open_ports = free_ports(anchor, ed.placed)
    if toward is not None:
        open_ports.sort(key=lambda p: -(p[2][0] * toward[0] + p[2][1] * toward[1]))
    target = open_ports[0][1]
    probe = Placed(sprite, (0.0, 0.0), scale=scale)
    off = min(probe.world_ports(), key=lambda p: p[0][0])[0]
    return ed.pick_and_drop(sprite, (target[0] - off[0], target[1] - off[1]))


def chain_ring(ed, arc, at, scale):
    """Drop copies of one curve, letting each snap to the last."""
    ed.place_scale = scale
    n = max(2, int(round(360.0 / arc.span)))
    ring = [ed.pick_and_drop(arc, at)]
    for _ in range(n - 1):
        target = free_ports(ring[-1], ring[:-1])[-1][1]
        probe = Placed(arc, (0.0, 0.0), scale=scale)
        off = probe.world_ports()[0][0]
        ed.pick_and_drop(arc, (target[0] - off[0], target[1] - off[1]))
        ring.append(ed.placed[-1])
    gap = min(math.hypot(a[0] - b[0], a[1] - b[1])
              for a, _ in ring[-1].world_ports()
              for b, _ in ring[0].world_ports())
    return n, gap


def demo(sheet, out, gutter=GUTTER, threshold=None):
    """Drive the editor headlessly and prove the two reported problems.

    1. Every piece on the sheet is pickable, the small road included. In
       v2 its typed rectangle missed the art and it could not be
       selected.
    2. Each curve is chained by its own measured span until it closes a
       ring. If the spans were asserted rather than measured, the ends
       would not meet.
    """
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    ed = Editor(sheet, gutter, threshold)

    roads = [s for s in ed.sprites if s.shape["kind"] == "road"]
    arcs = [s for s in ed.sprites if s.is_arc]
    domes = sorted((s for s in ed.sprites if s.shape["kind"] == "dome"),
                   key=lambda s: -s.surface.get_width())
    small = min(roads, key=lambda s: s.surface.get_width())
    print(f"\nsmall road is sprite [{small.index}] {small.label} — picking it")

    # A base: the big dome, a road out of every second rim port, and a
    # unit dome snapped onto the far end of each road.
    ed.place_scale = 0.42
    core = ed.pick_and_drop(domes[0], (760, 420))
    for i, (_, p, _) in enumerate(free_ports(core, ed.placed)):
        if i % 2:
            continue
        probe = Placed(small, (0.0, 0.0), scale=ed.place_scale)
        off = probe.world_ports()[0][0]
        road = ed.pick_and_drop(small, (p[0] - off[0], p[1] - off[1]))
        end = free_ports(road, ed.placed[:-1])[-1][1]
        unit = domes[-1]
        probe = Placed(unit, (0.0, 0.0), scale=ed.place_scale)
        off = probe.world_ports()[0][0]
        ed.pick_and_drop(unit, (end[0] - off[0], end[1] - off[1]))
    print(f"  {len(ed.placed)} pieces snapped into a base")
    ed.selected = None
    ed.shot(out)

    # Then one ring per curve, each chained by its own measured step.
    ed.clear_map()
    print("\nchaining every curve into a ring:")
    spots = [(640, 230), (960, 230), (1250, 230), (740, 640), (1130, 640)]
    for arc, at in zip(arcs, spots):
        scale = min(0.5, 95.0 / max(1.0, arc.shape["radius"]))
        n, gap = chain_ring(ed, arc, at, scale)
        print(f"  [{arc.index:2d}] measured {arc.shape['measured']:6.2f} deg "
              f"-> step {arc.rot_step:8g}  x{n} ring closes to {gap:.1f} px")
    ed.selected = None
    ed.show_ports = False
    rings = out.replace(".png", "_rings.png")
    ed.shot(rings)
    pygame.quit()


def fit_test(sheet, out, gutter=GUTTER, threshold=None):
    """Butt the connector against the plain roads and see if it fits.

    The connector is the piece with a socket at each end; the plain roads
    have none. Whether they can be run together is a question about the
    art, so the answer is measured (deck widths) and drawn (three chains
    a joint at a time), not asserted.
    """
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    ed = Editor(sheet, gutter, threshold)

    roads = sorted((s for s in ed.sprites if s.shape["kind"] == "road"),
                   key=lambda s: -s.surface.get_width())
    domes = sorted((s for s in ed.sprites if s.shape["kind"] == "dome"),
                   key=lambda s: -s.surface.get_width())
    connector, long_road, short_road = roads[0], roads[1], roads[2]
    big, unit = domes[0], domes[-1]

    print("\ndeck widths, measured across the middle of each piece:")
    for r in (connector, long_road, short_road):
        print(f"  [{r.index:2d}] {r.label}")
    decks = [r.shape["width"] for r in roads]
    print(f"  spread {min(decks):.0f}-{max(decks):.0f} px "
          f"({(max(decks) - min(decks)) / max(decks) * 100:.0f}% mismatch)")
    print(f"  big dome socket  {big.shape['width']:.0f} px")
    print(f"  unit dome socket {unit.shape['width']:.0f} px")

    east = (1.0, 0.0)
    scale = 0.42
    rows = [
        ("unit - connector - unit", [unit, connector, unit]),
        ("big  - connector - unit", [big, connector, unit]),
        ("unit - plain road - unit", [unit, long_road, unit]),
    ]
    y = 190
    for title, seq in rows:
        ed.place_scale = scale
        head = aim(ed.pick_and_drop(seq[0], (560, y)), east)
        prev = head
        for piece in seq[1:]:
            prev = attach(ed, piece, prev, scale, toward=east)
        print(f"  {title}: {len(seq)} pieces, "
              f"ends at x={prev.pos[0]:.0f}")
        y += 260
    ed.selected = None
    ed.shot(out)

    # A close look at the joint, where any mismatch in deck width shows.
    # A close look at the first joint, where the connector's socket lies
    # over the dome's and the rails meet the rim.
    surf = pygame.image.load(out)
    zoom = pygame.Surface((200, 100))
    zoom.blit(surf, (0, 0), pygame.Rect(515, 140, 200, 100))
    zoom = pygame.transform.scale(zoom, (1200, 600))
    joint = out.replace(".png", "_joint.png")
    pygame.image.save(zoom, joint)
    print("wrote", joint)
    pygame.quit()


def main(argv):
    def opt(name, cast=int, default=None):
        return cast(argv[argv.index(name) + 1]) if name in argv else default

    gutter = opt("--gutter", int, GUTTER)
    threshold = opt("--threshold", int, None)
    flagged = set()
    for name in ("--gutter", "--threshold", "--atlas", "--demo", "--fit"):
        if name in argv:
            flagged.add(argv[argv.index(name) + 1])
    sheet = find_sheet([a for a in argv if a not in flagged])

    if "--fit" in argv:
        fit_test(sheet, opt("--fit", str), gutter, threshold)
        return
    if "--demo" in argv:
        demo(sheet, opt("--demo", str), gutter, threshold)
        return
    if "--atlas" in argv:
        os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
        pygame.init()
        pygame.display.set_mode((64, 64))
        img = Image.open(sheet).convert("RGB")
        _, sprites = build_atlas(sheet, gutter, threshold)
        report_atlas(sprites, sheet)
        write_atlas_debug(img, sprites, opt("--atlas", str))
        return
    Editor(sheet, gutter, threshold).run()


if __name__ == "__main__":
    main(sys.argv)
