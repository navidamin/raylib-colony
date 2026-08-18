"""Lunar base layout editor.

The palette on the left IS the sprite sheet. Hover a piece to highlight it,
click to grab it onto the cursor, move to the map on the right, click to
drop. Rotate with Q/E or the wheel, snap to pieces already on the map,
scale 0.5x / 2x.

What changed from v2, and why
-----------------------------
v2 carried a hand-typed table of sprite rectangles and a hand-typed table
of arc angles. Both are now MEASURED FROM THE IMAGE:

* Sprite cells are found by slicing the sheet at its background gutters
  (an X-Y cut, then a gap merge inside each cell), so every piece on the
  sheet is selectable and its rectangle is exactly the art. The short
  road in the lower left could not be picked in v2 because its typed
  rectangle missed the art. Nothing is typed here, so it cannot happen.
  The gap merge is what keeps a road drawn with a dashed centre line one
  pickable object instead of a row of loose dashes.

* Arc geometry is recovered by fitting a circle to each curved piece:
  centre, radius, angular span and the two endpoints all come from the
  pixels. A curve's rotation step is its own measured span, so N
  rotations chain N copies exactly end to end, whatever the artist drew.
  A span that lands within a degree of an exact divisor of 360 is
  snapped to it, so the chain closes rather than drifting.

Run
---
    python3 lunar_layout_editor.py [sheet.png]
    python3 lunar_layout_editor.py sheet.png --atlas atlas.png   # what it found
    python3 lunar_layout_editor.py sheet.png --gutter 30         # tune slicing

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

BG_THRESHOLD = 52      # pixels darker than this are sheet background
GUTTER = 22            # px of background that separates two pieces
MIN_AREA = 700         # px; drops specks and compression noise
SNAP_RADIUS = 48       # px; how close two ports must be to snap


# ------------------------------------------------------- sheet analysis

def ink_mask(rgb):
    """True where the sheet has art rather than dark background."""
    return rgb.max(axis=2) >= BG_THRESHOLD


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


def centreline_curvature(pts):
    """Curvature of a road piece, from the centreline of its ink.

    A filled band is a terrible thing to fit a circle to directly: over a
    short arc the band's own thickness swamps the few pixels of sagitta,
    and an algebraic fit collapses onto the blob's centroid. So the
    centreline is recovered first — bin the ink along the piece's long
    axis and take the mean of each bin — and a parabola is fitted to
    that. Averaging a few hundred pixels per bin puts the centreline well
    inside a pixel, which is what makes a 3 px sagitta measurable.

    The ends are dropped before fitting. Where the piece is cut off, a
    bin holds only the corner of the band, so its mean sits off the
    centreline by several pixels — more than the whole signal on a
    shallow curve, and enough on its own to flatten the fit into a
    straight line.

    Returns (centre_xy, radius) or None if the piece is straight.
    """
    mean, axis, perp = pca_frame(pts)
    rel = pts - mean
    u, v = rel @ axis, rel @ perp
    bu, bv, bn = bin_means(u, v, 48)
    if len(bu) < 10:
        return None
    keep = bn >= 0.55 * np.median(bn)
    lo, hi = float(u.min()), float(u.max())
    inset = 0.10 * (hi - lo)
    keep &= (bu > lo + inset) & (bu < hi - inset)
    if keep.sum() < 8:
        return None
    bu, bv = bu[keep], bv[keep]

    a, b, c = np.polyfit(bu, bv, 2)
    if abs(a) < 1e-9:
        return None
    um = float(bu.mean())
    slope = 2.0 * a * um + b
    radius = (1.0 + slope * slope) ** 1.5 / (2.0 * abs(a))
    chord = float(bu.max() - bu.min())
    sagitta = abs(a) * chord * chord / 4.0
    if sagitta < 0.8 or radius > 25.0 * chord:
        return None                     # straight within measurement noise
    n = np.array([-slope, 1.0]) / math.hypot(slope, 1.0) * np.sign(a)
    cu = um + n[0] * radius
    cv = (a * um * um + b * um + c) + n[1] * radius
    return mean + cu * axis + cv * perp, radius


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


def refine_centre(pts, centre, radius):
    """Pattern search for the centre that makes the ink most annular.

    Cheap, derivative-free and stable: try eight directions, keep any
    improvement, halve the step when none helps. Beats another algebraic
    circle fit here because the ink is a filled band, not a thin curve.
    """
    if len(pts) > 4000:
        pts = pts[np.linspace(0, len(pts) - 1, 4000).astype(int)]
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
        cx, cy = w / 2.0, h / 2.0
        r = 0.25 * (w + h)
        ports = []
        for k in range(8):
            a = math.radians(k * 45.0)
            ports.append(((cx + r * math.cos(a), cy + r * math.sin(a)),
                          (math.cos(a), math.sin(a))))
        return {"kind": "dome", "arc": False, "span": 0.0, "measured": 0.0,
                "radius": r, "centre": (cx, cy), "ports": ports,
                "size": (w, h)}

    curved = centreline_curvature(pts)
    if curved is not None:
        centre, radius, start, span = refine_centre(pts, curved[0], curved[1])
        if 2.0 < span < 300.0:
            ports = []
            for a, sign in ((start, -1.0), (start + span, 1.0)):
                t = math.radians(a + sign * 90.0)
                ports.append(((centre[0] + radius * math.cos(math.radians(a)),
                               centre[1] + radius * math.sin(math.radians(a))),
                              (math.cos(t), math.sin(t))))
            return {"kind": "arc", "arc": True, "span": quantise_span(span),
                    "measured": span, "radius": float(radius),
                    "centre": (float(centre[0]), float(centre[1])),
                    "ports": ports, "size": (w, h)}

    # Straight piece: principal axis, then the centreline point at each end.
    mean, axis, _ = pca_frame(pts)
    proj = (pts - mean) @ axis
    lo, hi = float(proj.min()), float(proj.max())
    ports = []
    for target, sign in ((lo, -1.0), (hi, 1.0)):
        sel = np.abs(proj - target) < max(2.0, 0.04 * (hi - lo))
        p = pts[sel].mean(axis=0)
        d = axis * sign
        ports.append(((float(p[0]), float(p[1])),
                      (float(d[0]), float(d[1]))))
    return {"kind": "road", "arc": False, "span": 0.0, "measured": 0.0,
            "radius": 0.0, "centre": None, "ports": ports, "size": (w, h)}


def crop_sprite(sheet_img, box):
    """Cut the sprite out and knock the dark background transparent."""
    x0, y0, x1, y1 = box
    crop = sheet_img.crop((x0, y0, x1 + 1, y1 + 1)).convert("RGBA")
    arr = np.array(crop)
    arr[arr[:, :, :3].max(axis=2) < BG_THRESHOLD, 3] = 0
    return pygame.image.frombuffer(
        np.ascontiguousarray(arr).tobytes(), crop.size, "RGBA").convert_alpha()


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
        # copies closes exactly. Straight pieces get a plain 15 degrees.
        self.rot_step = self.span if (self.is_arc and self.span > 1.0) else 15.0
        w, h = surface.get_size()
        self.centre = (w / 2.0, h / 2.0)
        self.ports = shape["ports"]

    @property
    def label(self):
        w, h = self.surface.get_size()
        if self.is_arc:
            return f"arc {self.span:g} deg  r{self.shape['radius']:.0f}"
        return f"{self.shape['kind']} {w}x{h}"


def report_atlas(sprites, path):
    print(f"{os.path.basename(path)}: detected {len(sprites)} sprites")
    for s in sprites:
        extra = (f"   measured {s.shape['measured']:.2f} deg -> step "
                 f"{s.rot_step:g}" if s.is_arc else "")
        print(f"  [{s.index:2d}] {s.label:22s} box={s.box}{extra}")


def build_atlas(path, gutter=GUTTER):
    img = Image.open(path).convert("RGB")
    mask = ink_mask(np.array(img))
    sprites = []
    for box in slice_sheet(mask, gutter):
        shape = analyse_shape(mask, box)
        if shape is not None:
            sprites.append(Sprite(len(sprites), box, shape,
                                  crop_sprite(img, box)))
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

    def __init__(self, sheet_path, gutter=GUTTER):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
        pygame.display.set_caption("Lunar Base Layout Editor v3")
        self.font = pygame.font.SysFont("dejavusans", 15)
        self.small = pygame.font.SysFont("dejavusans", 13)
        self.clock = pygame.time.Clock()

        self.sheet_img, self.sprites = build_atlas(sheet_path, gutter)
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


def demo(sheet, out, gutter=GUTTER):
    """Drive the editor headlessly and prove the two reported problems.

    1. The small road is picked from the palette and dropped. In v2 its
       typed rectangle missed the art and it could not be selected.
    2. A curve is chained by its own measured span until it closes a
       ring. If the span were asserted rather than measured, the ends
       would not meet.
    """
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    ed = Editor(sheet, gutter)
    ed.show_ports = True

    roads = [s for s in ed.sprites if s.shape["kind"] == "road"]
    arcs = [s for s in ed.sprites if s.is_arc]
    domes = [s for s in ed.sprites if s.shape["kind"] == "dome"]
    small = min(roads, key=lambda s: s.surface.get_width())
    print(f"\nsmall road is sprite [{small.index}] {small.label} — picking it")

    ed.place_scale = 0.5
    core = ed.pick_and_drop(domes[0], (760, 260))
    # Roads dropped near the dome's rim ports snap onto them.
    for i, (_, p, _) in enumerate(free_ports(core, ed.placed)):
        if i % 2:
            continue
        probe = Placed(small, (0.0, 0.0), scale=ed.place_scale)
        off = probe.world_ports()[0][0]
        ed.pick_and_drop(small, (p[0] - off[0], p[1] - off[1]))
    print(f"  {len(ed.placed) - 1} copies of it snapped onto the dome rim")

    # Chain one curve until it closes. The count comes from the measured
    # span, so a wrong measurement shows up as a ring that does not shut.
    arc = max(arcs, key=lambda s: s.span)
    n = int(round(360.0 / arc.span))
    print(f"\nchaining sprite [{arc.index}] {arc.label}: "
          f"measured {arc.shape['measured']:.2f} deg, step {arc.rot_step:g}, "
          f"{n} to close 360")
    ring = []
    first = ed.pick_and_drop(arc, (700, 600))
    ring.append(first)
    for _ in range(n - 1):
        open_ports = free_ports(ring[-1], ring[:-1])
        target = open_ports[-1][1]
        probe = Placed(arc, (0.0, 0.0), scale=ed.place_scale)
        off = probe.world_ports()[0][0]
        ed.pick_and_drop(arc, (target[0] - off[0], target[1] - off[1]))
        ring.append(ed.placed[-1])
    gap = min(math.hypot(a[0] - b[0], a[1] - b[1])
              for a, _ in ring[-1].world_ports()
              for b, _ in ring[0].world_ports())
    print(f"  ring of {n}: the two loose ends meet within {gap:.1f} px "
          f"(piece radius {arc.shape['radius'] * ed.place_scale:.0f} px)")

    ed.selected = None
    ed.shot(out)
    pygame.quit()


def main(argv):
    gutter = GUTTER
    if "--gutter" in argv:
        gutter = int(argv[argv.index("--gutter") + 1])
    sheet = find_sheet([a for a in argv if a != str(gutter)])
    if "--demo" in argv:
        demo(sheet, argv[argv.index("--demo") + 1], gutter)
        return
    if "--atlas" in argv:
        os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
        pygame.init()
        pygame.display.set_mode((64, 64))
        img = Image.open(sheet).convert("RGB")
        _, sprites = build_atlas(sheet, gutter)
        report_atlas(sprites, sheet)
        write_atlas_debug(img, sprites, argv[argv.index("--atlas") + 1])
        return
    Editor(sheet, gutter).run()


if __name__ == "__main__":
    main(sys.argv)
