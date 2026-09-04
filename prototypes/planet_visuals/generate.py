"""
Procedural moon surface prototype.

Pipeline (each stage emits a debug PNG so we can see what it contributes):

    1. Multi-octave value-noise heightmap          -> stage1_heightmap.png
    2. Mare basins (low-freq depression mask)      -> stage2_mare.png
    3. Parametric crater field (age-stratified)    -> stage3_craters.png
    4. Hillshade (Lambertian, NW sun)              -> stage4_hillshade.png
    5. Archetype tinting (MARE / HIGHLAND / POLAR  -> stage5_archetypes.png
       / KREEP / LAVA_TUBE / MIXED)
    6. Decals + composite                          -> stage6_final.png

Then a few presentation outputs:

    planet_full.png       1600x1600 final result, the whole 20x20 grid
    archetype_tiles.png   one 256-px sample per archetype, side-by-side
    detail_zoom.png       4x zoom on a crater cluster
    comparison.png        old random-tile look vs new procedural look

Run:    python3 generate.py
Output: ./output/*.png
"""

from __future__ import annotations

import math
import os
from dataclasses import dataclass

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

PLANET_GRID = 20            # game's PLANET_SIZE
PX_PER_CELL = 80            # render resolution per game cell
SIZE = PLANET_GRID * PX_PER_CELL   # 1600 px
SEED = 12345

OUT = os.path.join(os.path.dirname(__file__), "output")
os.makedirs(OUT, exist_ok=True)


# ---------------------------------------------------------------------------
# Math helpers
# ---------------------------------------------------------------------------

def gaussian_blur(arr, sigma):
    """Separable 1D Gaussian blur on a float32 array. Used in float space to
    avoid the uint8 quantization contour-line artifacts that PIL gives us."""
    if sigma <= 0:
        return arr
    h, w = arr.shape
    # np.convolve(mode='same') returns max(M, N) — i.e. when the kernel
    # is wider than the signal, the output gets padded up. Cap radius
    # to half the smaller axis so output shape is preserved.
    radius = max(1, int(sigma * 3))
    radius = min(radius, h // 2 - 1, w // 2 - 1)
    if radius < 1:
        return arr
    x = np.arange(-radius, radius + 1, dtype=np.float32)
    k = np.exp(-(x ** 2) / (2.0 * sigma * sigma))
    k /= k.sum()
    out = arr.astype(np.float32, copy=False)
    out = np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 1, out)
    out = np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 0, out)
    return out


# ---------------------------------------------------------------------------
# Archetype palette (matches game_enums.h SiteArchetype)
# ---------------------------------------------------------------------------

@dataclass
class Archetype:
    name: str
    base: tuple        # (r,g,b) average ground tone
    shadow: tuple      # darker variant for low ground / floors
    high: tuple        # brighter variant for ridges
    accent: tuple      # signature accent (ice glint, KREEP glow, void, etc.)
    contrast: float    # 0..1, how much hillshade modulates albedo

# Tones tuned to look like real lunar imagery: low chroma, mostly grayscale
# with subtle hue shifts. All `base` colours kept in a tight 140..170
# brightness range so no biome region reads as a shadow patch when
# rendered at full-planet zoom. Biomes are distinguished by tint, not
# by big brightness jumps.
ARCHETYPES = {
    "MARE_INDUSTRIAL":     Archetype("Mare (basalt)",
                                     (138, 134, 128),
                                     ( 80,  78,  74),
                                     (170, 165, 156),
                                     ( 90,  86,  80), 0.45),
    "HIGHLAND_CONSTRUCTION": Archetype("Highland (anorthosite)",
                                     (162, 158, 150),
                                     (110, 106,  98),
                                     (200, 196, 188),
                                     (190, 188, 180), 0.55),
    "POLAR_VOLATILE":      Archetype("Polar (ice frost)",
                                     (168, 174, 184),
                                     (115, 122, 134),
                                     (210, 218, 230),
                                     (220, 234, 246), 0.65),
    "KREEP_SCIENTIFIC":    Archetype("KREEP (thorium-rich)",
                                     (158, 144, 130),
                                     (100,  90,  80),
                                     (190, 174, 156),
                                     (200, 150, 110), 0.50),
    "LAVA_TUBE":           Archetype("Lava tube collapse",
                                     (148, 142, 134),
                                     ( 70,  66,  60),
                                     (180, 172, 162),
                                     (  6,   4,   4), 0.45),
    "MIXED":               Archetype("Mixed terrain",
                                     (152, 146, 138),
                                     (100,  94,  86),
                                     (188, 180, 168),
                                     (140, 132, 120), 0.55),
}


# ---------------------------------------------------------------------------
# Step 1 — heightmap (value-noise FBM, fully vectorised)
# ---------------------------------------------------------------------------

def value_noise(shape, scale, rng):
    """Single-octave smooth noise.

    PIL mode 'F' for the bicubic upsample, then a numpy Gaussian blur in
    float space. uint8 quantization here would give concentric contour
    artifacts under hillshading.
    """
    h, w = shape
    gh = max(2, h // scale + 2)
    gw = max(2, w // scale + 2)
    grid = rng.random((gh, gw)).astype(np.float32)
    img = Image.fromarray(grid, mode="F")
    img = img.resize((w + scale * 2, h + scale * 2), Image.BICUBIC)
    arr = np.asarray(img, dtype=np.float32)[scale:scale + h, scale:scale + w]
    arr = gaussian_blur(arr, scale * 0.45)
    return np.clip(arr, 0.0, 1.0)


def pink_noise(shape, rng, *, exponent=1.0):
    """1/f^exponent noise via FFT spectrum shaping. Returns a zero-mean
    unit-variance float field. Pink (exponent=1) is the spectrum that
    matches natural rough-surface statistics — used here to add a fine
    regolith texture between craters.
    """
    h, w = shape
    white = rng.standard_normal(shape).astype(np.float32)
    spec = np.fft.fft2(white)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    filt = 1.0 / (f ** exponent)
    filt[0, 0] = 0.0
    out = np.real(np.fft.ifft2(spec * filt)).astype(np.float32)
    out -= out.mean()
    s = out.std()
    if s > 1e-6:
        out /= s
    return out


def fbm(shape, octaves, base_scale, persistence, rng):
    """Fractal Brownian motion: stack value-noise octaves."""
    out = np.zeros(shape, dtype=np.float32)
    amp = 1.0
    norm = 0.0
    scale = base_scale
    for _ in range(octaves):
        out += amp * value_noise(shape, scale, rng)
        norm += amp
        amp *= persistence
        scale = max(2, scale // 2)
    out /= norm
    return out


# ---------------------------------------------------------------------------
# Step 2 — mare basins (a few large low-freq dark depressions)
# ---------------------------------------------------------------------------

def mare_field(shape, rng, n=3):
    """Big flat dark basins. The floor should read as a *plain*, not a smooth
    bowl — so we use a smoothstepped indicator with a sharp interior."""
    h, w = shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    mask = np.zeros(shape, dtype=np.float32)
    for _ in range(n):
        cx = rng.uniform(0.2, 0.8) * w
        cy = rng.uniform(0.2, 0.8) * h
        rx = rng.uniform(0.18, 0.32) * w
        ry = rng.uniform(0.18, 0.32) * h
        ang = rng.uniform(0.0, math.pi)
        cs, sn = math.cos(ang), math.sin(ang)
        x = (xx - cx) * cs + (yy - cy) * sn
        y = -(xx - cx) * sn + (yy - cy) * cs
        d = np.sqrt((x / rx) ** 2 + (y / ry) ** 2)
        # smoothstep transition: flat 1.0 inside, soft ramp 0.85..1.05
        t = np.clip((1.05 - d) / 0.20, 0.0, 1.0)
        falloff = t * t * (3.0 - 2.0 * t)
        mask = np.maximum(mask, falloff)
    # break the perfect ellipse outline with low-freq jitter
    jitter = fbm(shape, 4, 64, 0.5, rng)
    mask *= 0.85 + 0.15 * jitter
    return mask


# ---------------------------------------------------------------------------
# Step 3 — parametric crater field
# ---------------------------------------------------------------------------

@dataclass
class Crater:
    cx: float
    cy: float
    r: float          # rim radius in pixels
    age: float        # 0=fresh (sharp rim, central peak), 1=eroded
    has_peak: bool


def sample_craters(shape, rng, count_small=17, count_med=7, count_big=1,
                   min_separation=1.35, age_alpha=4.0,
                   size_scale=1.5, size_variance=0.5):
    """Place primary craters with mostly-random placement (uniform
    coverage, no clumps), then inject a small number of secondary
    craters around each big primary.

    The user-visible distribution we want is: uniform density across the
    whole planet, with subtle clusters of small secondaries hanging off
    a few big primaries. Bridson-style growth made the clusters too
    obvious — most placements now go to a fully random location, with
    only a small fraction (~15%) drawn near an existing crater. That's
    enough to fill in any genuinely empty spot without producing a
    blob structure.
    """
    h, w = shape

    primary_sizes = []
    for n, rmin, rmax in [(count_big, 42, 110),
                          (count_med, 18, 42),
                          (count_small, 8, 18)]:
        center = 0.5 * (rmin + rmax)
        half_range = 0.5 * (rmax - rmin)
        for _ in range(n):
            # size_variance scales the spread around the bucket centre.
            # 0 -> every crater in this bucket is the bucket centre size.
            # 1 -> current uniform[rmin, rmax] behaviour.
            # >1 -> wider spread, can exceed the bucket boundaries.
            u = float(rng.uniform(-1.0, 1.0)) * size_variance
            r = (center + half_range * u) * size_scale
            primary_sizes.append(max(2.0, r))
    primary_sizes.sort(reverse=True)  # big first claims space

    placed: list[Crater] = []

    def fits(x, y, r):
        if not (0 <= x < w and 0 <= y < h):
            return False
        for c in placed:
            if (c.cx - x) ** 2 + (c.cy - y) ** 2 \
                    < ((c.r + r) * min_separation) ** 2:
                return False
        return True

    def try_place(x, y, r, age, has_peak):
        if fits(x, y, r):
            placed.append(Crater(cx=x, cy=y, r=r,
                                 age=age, has_peak=has_peak))
            return True
        return False

    # --- pass 1: primaries (mostly uniform random, light Bridson rescue)
    if primary_sizes:
        r0 = primary_sizes[0]
        for _ in range(100):
            if try_place(float(rng.uniform(0, w)),
                          float(rng.uniform(0, h)),
                          r0,
                          float(rng.beta(age_alpha, 1.5)),
                          bool(rng.random() < 0.25)):
                break

    dropped_primary = 0
    for r in primary_sizes[1:]:
        success = False
        for _ in range(80):
            # 85% global random, 15% Bridson-style near an existing
            # crater (only used as a rescue if random keeps failing).
            if placed and rng.random() < 0.15:
                seed = placed[int(rng.integers(0, len(placed)))]
                d_min = (seed.r + r) * min_separation
                d_max = d_min * 2.0
                d = float(rng.uniform(d_min, d_max))
                ang = float(rng.uniform(0, math.tau))
                x = seed.cx + d * math.cos(ang)
                y = seed.cy + d * math.sin(ang)
            else:
                x = float(rng.uniform(0, w))
                y = float(rng.uniform(0, h))
            if try_place(x, y, r,
                          float(rng.beta(age_alpha, 1.5)),
                          bool(rng.random() < 0.25)):
                success = True
                break
        if not success:
            dropped_primary += 1

    n_primary = len(placed)

    # --- pass 2: secondaries — fewer, only around the largest primaries
    primaries_snapshot = list(placed)
    dropped_secondary = 0
    for p in primaries_snapshot:
        if p.r < 55:                    # only the very largest spawn rays
            continue
        n_secondaries = int(rng.integers(1, 4))     # 1..3
        ray_angles = (float(rng.uniform(0, math.tau)),
                      float(rng.uniform(0, math.tau)))
        for _ in range(n_secondaries):
            sr = float(rng.uniform(4.0, 9.0))
            success = False
            for _ in range(20):
                if rng.random() < 0.55:
                    base = ray_angles[int(rng.integers(0, 2))]
                    ang = base + float(rng.normal(0.0, 0.30))
                else:
                    ang = float(rng.uniform(0, math.tau))
                d = p.r * float(rng.uniform(1.8, 3.5))
                x = p.cx + d * math.cos(ang)
                y = p.cy + d * math.sin(ang)
                age = float(rng.beta(1.5, 2.8))
                if try_place(x, y, sr, age, False):
                    success = True
                    break
            if not success:
                dropped_secondary += 1

    n_secondary = len(placed) - n_primary
    print(f"  placed {n_primary} primaries + {n_secondary} secondaries"
          f" (dropped {dropped_primary} primaries, "
          f"{dropped_secondary} secondaries)")
    return placed


def apply_craters(height, craters, rng=None,
                  depth_variance=1.0, depth_scale=1.0):
    # Defaults locked from the v4 dispersion sweep — sparse mare-like
    # density with mid-deep craters and tight size buckets.
    """Carve craters with the geometry real lunar craters actually have:
    flat floor + smooth wall + barely-raised rim.

    Cross-checked against LRO/Apollo imagery:
      * Depth/diameter ratio is ~1:5 for simple bowls, ~1:20 for large
        flat-floored complex craters. We pick per crater.
      * Rim height is only ~3-5% of crater depth, not the dominant feature.
        (Old code had a rim 43% as tall as the bowl was deep — wrong.)
      * The dominant visual feature is the *cast shadow*, which is
        handled separately in `cast_shadows()`. Carving here is just
        about getting the geometry right.
    """
    if rng is None:
        rng = np.random.default_rng(0)
    h, w = height.shape
    # Ejecta blanket disabled — was visually reading as crater-overlap
    # at the user's preferred zoom. Carving stops at the rim (1.05 r).
    extent = 1.10
    for c in craters:
        x0 = max(0, int(c.cx - c.r * extent))
        x1 = min(w, int(c.cx + c.r * extent) + 1)
        y0 = max(0, int(c.cy - c.r * extent))
        y1 = min(h, int(c.cy + c.r * extent) + 1)
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        dx = xx - c.cx
        dy = yy - c.cy
        d = np.sqrt(dx * dx + dy * dy) / c.r
        ang = np.arctan2(dy, dx)

        a1, a2, a3 = rng.uniform(0, math.tau, 3)
        irreg = (0.04 * np.sin(2 * ang + a1)
                 + 0.03 * np.sin(3 * ang + a2)
                 + 0.02 * np.sin(5 * ang + a3))
        d = d * (1.0 + irreg * (0.5 + 0.5 * c.age))

        # Per-crater depth variation. Wide jitter range so some craters
        # are shallow flat dishes and some are deep enough that their
        # floors fall into cast shadow — the variety the user asked for.
        # Larger craters trend shallower (filled / complex morphology).
        # Per-crater depth, fully independent of radius — every crater
        # picks a depth from the same Beta distribution, so big craters
        # have the same chance of being deep as small ones. (Old code
        # had a size_factor and per-size jitter floors that compressed
        # big craters into the shallow band, which is why no deep large
        # craters appeared.)
        # depth_variance scales the spread of depths around 1.0. At 0.0
        # all craters get the same depth (no variance). At 1.0 we get
        # the original 0.15..1.75 range. Higher values widen the spread.
        # Clip below at 0.05 so high variance doesn't flip craters into
        # raised bumps.
        depth_jitter = max(0.05, 1.0 + (float(rng.beta(2.2, 2.2)) * 1.6 - 0.80) * depth_variance)
        sharp = 1.0 - c.age * 0.7
        depth_amp = -0.42 * sharp * depth_jitter * depth_scale
        # Rim is *small* — only ~5% of depth amplitude.
        rim_amp = 0.05 * sharp * abs(depth_amp)

        # Profile zones, tuned to match real lunar simple-crater morphology:
        #   d in [0, 0.70]:    flat floor (talus-filled bottom; wider than
        #                      a textbook "bowl" — matches LRO photos)
        #   d in [0.70, 0.95]: wall, with a steepness curve that's nearly
        #                      vertical near the top and gradual at the
        #                      bottom (angle-of-repose talus). Power
        #                      profile h = depth * (1 - u^p), p=3..5.
        #   d in [0.95, 1.05]: tiny rim
        #   d in [1.05, 1.8]:  subtle ejecta
        floor_mask = d < 0.70
        wall_mask = (d >= 0.70) & (d < 0.95)
        rim_mask = (d >= 0.95) & (d < 1.05)

        delta = np.zeros_like(d, dtype=np.float32)
        delta[floor_mask] = depth_amp
        # Wall: vertical-near-top profile. Bigger / deeper craters get a
        # higher exponent (more vertical upper wall) — real-world simple
        # bowl craters are deeper *and* steeper at the top.
        wall_p = 3.0 + min(2.0, abs(depth_amp) * 4.0)
        wu = (d[wall_mask] - 0.70) / 0.25
        delta[wall_mask] = depth_amp * (1.0 - wu ** wall_p)
        rim_d = d[rim_mask]
        delta[rim_mask] = rim_amp * np.exp(-((rim_d - 1.00) / 0.05) ** 2)

        # (Central peaks intentionally omitted: a bare Gaussian dome on
        # a flat floor reads as an unwanted bump, not a real complex-
        # crater central peak. Re-add later with proper talus geometry
        # if/when complex craters become a feature.)

        height[y0:y1, x0:x1] += delta
    return height


# ---------------------------------------------------------------------------
# Step 4 — hillshade
# ---------------------------------------------------------------------------

def cast_shadows(height, azimuth_deg=315.0, altitude_deg=35.0, z_factor=75.0,
                 max_distance_px=70.0, step_px=1.5):
    """Cheap horizon ray-march toward the sun. Returns a per-pixel float
    mask: 1.0 = lit, 0.0 = fully blocked from the sun by terrain.

    For each pixel, march along the sun direction in plan view; at each
    step, ask whether the elevation there exceeds what a straight ray
    from the pixel at the sun's altitude would have. If yes at any step,
    the pixel is in cast shadow.

    This is what makes deep crater floors go nearly black — the inner
    wall on the sun side blocks the floor. Lambertian self-shading alone
    can't produce that effect.
    """
    h, w = height.shape
    sun_az_math = math.radians(360.0 - azimuth_deg + 90.0)
    # We march FROM each pixel TOWARD the sun. In math angles +x is east
    # and +y is north, but image y increases southward, so flip y.
    sx = math.cos(sun_az_math)
    sy_image = -math.sin(sun_az_math)
    tan_alt = math.tan(math.radians(altitude_deg))

    h_scaled = (height * z_factor).astype(np.float32)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)

    # Track the maximum BLOCKING SLOPE along the sun ray, not just a
    # binary "in shadow" flag. A pixel whose max blocking slope just
    # barely exceeds the sun altitude should be only partially shadowed
    # (penumbra-like). This is what gives the continuous range of crater
    # depths the user wanted instead of three discrete tiers.
    max_block = np.full((h, w), -1e9, dtype=np.float32)
    n_steps = int(max_distance_px / step_px)
    for s in range(1, n_steps + 1):
        dist = s * step_px
        sample_x = np.clip(xx + sx * dist, 0, w - 1).astype(np.int32)
        sample_y = np.clip(yy + sy_image * dist, 0, h - 1).astype(np.int32)
        h_sample = h_scaled[sample_y, sample_x]
        block_slope = (h_sample - h_scaled) / dist
        max_block = np.maximum(max_block, block_slope)

    # Smooth transition: at max_block == tan_alt we're at the shadow
    # edge; anything `band` above that is full shadow, anything `band`
    # below is full sun. Linear ramp across that band gives the gradient.
    band = tan_alt * 0.35
    shadow_amount = np.clip((max_block - tan_alt) / band, 0.0, 1.0)
    light = 1.0 - shadow_amount
    light = gaussian_blur(light, 0.8)
    return np.clip(light, 0.0, 1.0)


def hillshade(height, azimuth_deg=315.0, altitude_deg=35.0, z_factor=75.0,
              smooth_px=1.0):
    """Lambertian shading. azimuth=315 -> sun from NW. Returns 0..1.

    `smooth_px` pre-blurs the heightmap so micro-noise doesn't dominate the
    gradient. Lunar terrain at game scale should read as gentle relief, not
    crumpled foil.
    """
    h = gaussian_blur(height, smooth_px) if smooth_px > 0 else height
    az = math.radians(360.0 - azimuth_deg + 90.0)
    alt = math.radians(altitude_deg)
    # np.gradient on a 2D array returns (gradient along axis 0, axis 1) =
    # (∂h/∂y_image_south, ∂h/∂x_east). Standard hillshade aspect (the
    # downhill compass direction) in math-radian form is atan2(dy, -dx).
    # The previous atan2(-dx, dy) was 90° rotated — it shaded craters as
    # if they were hills lit from the SW.
    dy, dx = np.gradient(h * z_factor)
    slope = np.arctan(np.hypot(dx, dy))
    aspect = np.arctan2(dy, -dx)
    shaded = (np.sin(alt) * np.cos(slope)
              + np.cos(alt) * np.sin(slope) * np.cos(az - aspect))
    return np.clip(shaded, 0.0, 1.0)


# ---------------------------------------------------------------------------
# Step 5 — archetype mask (coarse Voronoi-ish over the 20x20 grid)
# ---------------------------------------------------------------------------

ARCHETYPE_ORDER = list(ARCHETYPES.keys())  # stable index <-> name

def assign_archetype_grid(rng):
    """Return (PLANET_GRID, PLANET_GRID) ints into ARCHETYPE_ORDER.

    Noise-field biome assignment with hard constraints (calibrated in
    biome_compare.py — see that script for visual A/B against the
    earlier Voronoi version).

      * MARE in the top 20% of an FBM noise field. Low-frequency noise
        gives connected mare blobs that look like real basin geology
        (Mare Imbrium, Mare Crisium etc. — connected, not scattered).
      * POLAR forced at top-2 / bottom-2 rows of the grid.
      * KREEP at 1-2 random radial hotspots (~5-10 cells each).
      * LAVA TUBE at 1-3 randomly scattered isolated cells.
      * MIXED auto-assigned in the ±0.04 band around the mare threshold
        — that's the mare-highland transition zone.
      * HIGHLAND is the default fill for everything else.

    Resulting proportions per planet roughly: mare 10%, highland 50%,
    polar 20%, kreep 2%, lava 0.5%, mixed 15% (boundaries).
    """
    g = PLANET_GRID
    shape = (g, g)

    mare_strength = fbm(shape, 3, 4, 0.5, rng)
    mare_thresh = float(np.percentile(mare_strength, 80))

    yy = np.arange(g).reshape(-1, 1).repeat(g, axis=1)
    polar_mask = (yy < 2) | (yy >= g - 2)

    kreep_mask = np.zeros(shape, dtype=bool)
    n_hotspots = int(rng.integers(1, 3))
    for _ in range(n_hotspots):
        cx = int(rng.integers(3, g - 3))
        cy = int(rng.integers(3, g - 3))
        radius = float(rng.uniform(1.5, 2.5))
        ys, xs = np.mgrid[0:g, 0:g]
        d = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
        kreep_mask |= (d < radius)

    lava_mask = np.zeros(shape, dtype=bool)
    n_lava = int(rng.integers(1, 4))
    for _ in range(n_lava):
        x = int(rng.integers(0, g))
        y = int(rng.integers(0, g))
        lava_mask[y, x] = True

    mixed_mask = (
        (mare_strength > mare_thresh - 0.04)
        & (mare_strength < mare_thresh + 0.04)
    )

    grid = np.full(shape, ARCHETYPE_ORDER.index("HIGHLAND_CONSTRUCTION"),
                    dtype=np.int32)
    grid[mare_strength > mare_thresh] = ARCHETYPE_ORDER.index("MARE_INDUSTRIAL")
    overridable = ~(polar_mask | kreep_mask | lava_mask)
    grid[mixed_mask & overridable] = ARCHETYPE_ORDER.index("MIXED")
    grid[kreep_mask] = ARCHETYPE_ORDER.index("KREEP_SCIENTIFIC")
    grid[polar_mask] = ARCHETYPE_ORDER.index("POLAR_VOLATILE")
    grid[lava_mask] = ARCHETYPE_ORDER.index("LAVA_TUBE")
    return grid


def archetype_pixel_map(arch_grid):
    """Upsample arch_grid to per-pixel labels with smooth-ish boundaries."""
    h = w = SIZE
    # nearest-neighbour upsample via numpy
    py = np.repeat(np.arange(PLANET_GRID), PX_PER_CELL)
    px = np.repeat(np.arange(PLANET_GRID), PX_PER_CELL)
    return arch_grid[py[:, None], px[None, :]]


def colourise(height, mare_mask, hillsh, arch_pix, rng, cast_mask=None,
              albedo_height=None):
    """Apply per-pixel archetype colour, then modulate with hillshade and
    cast shadows. We deliberately do NOT blend the archetype's shadow/base/
    high colours by elevation — that produced large soft circular dark
    patches wherever the FBM heightmap dipped, even when those dips were
    not actually crater features.
    """
    h, w = height.shape

    # Per-pixel base archetype colour (Gaussian-blurred boundaries).
    base = np.zeros((h, w, 3), dtype=np.float32)
    contrast = np.zeros((h, w), dtype=np.float32)
    for i, name in enumerate(ARCHETYPE_ORDER):
        a = ARCHETYPES[name]
        m = (arch_pix == i)
        base[m] = a.base
        contrast[m] = a.contrast

    blur_px = int(PX_PER_CELL * 0.9)
    base = np.asarray(Image.fromarray(base.astype(np.uint8))
                      .filter(ImageFilter.GaussianBlur(blur_px)),
                      dtype=np.float32)
    contrast_blur = np.asarray(
        Image.fromarray((contrast * 255).astype(np.uint8))
        .filter(ImageFilter.GaussianBlur(blur_px)), dtype=np.float32) / 255.0

    albedo = base

    # hillshade modulates albedo
    sh = hillsh[..., None]
    contrast_arr = contrast_blur[..., None]
    shaded = albedo * (1.0 - contrast_arr + contrast_arr * (0.35 + 1.30 * sh))

    # Crater-floor ambient occlusion: darken any pixel that sits below
    # the FBM baseline (i.e. is inside a crater bowl) proportional to
    # its depth. Even when not in cast shadow, a crater floor gets less
    # ambient / scattered light than the open surrounding plain. This
    # is the "sense of depth" effect — a crater bottom should always
    # read a bit darker than the surrounding soil.
    if albedo_height is not None:
        depth_below = np.clip(albedo_height - height, 0.0, 1.5)
        ao = 1.0 - np.clip(depth_below * 0.85, 0.0, 0.45)
        shaded = shaded * ao[..., None]

    # Cast shadows: pixels the sun can't directly reach get darkened to
    # ~22% of their lit value. Not pure black — real lunar shadows still
    # receive some scattered earthlight / albedo bounce. 0.22 reads as
    # "deep shadow" without becoming a featureless hole.
    if cast_mask is not None:
        cm = cast_mask[..., None]
        shaded = shaded * (0.22 + 0.78 * cm)

    rgb = np.clip(shaded, 0, 255)
    return rgb.astype(np.uint8), albedo.astype(np.uint8)


# ---------------------------------------------------------------------------
# Step 6 — decals (ice glints in polar, KREEP glow, lava-tube voids)
# ---------------------------------------------------------------------------

def add_decals(rgb, arch_pix, height, rng):
    img = Image.fromarray(rgb)
    draw = ImageDraw.Draw(img, "RGBA")
    h, w = arch_pix.shape

    # small voids for lava-tube cells: pits with dark interior + faint rim
    lava_idx = ARCHETYPE_ORDER.index("LAVA_TUBE")
    polar_idx = ARCHETYPE_ORDER.index("POLAR_VOLATILE")
    kreep_idx = ARCHETYPE_ORDER.index("KREEP_SCIENTIFIC")

    # sparse sample
    n = 800
    xs = rng.integers(0, w, n)
    ys = rng.integers(0, h, n)
    for x, y in zip(xs, ys):
        a = arch_pix[y, x]
        if a == lava_idx and rng.random() < 0.04:
            # lava-tube skylight: a small rosette of 3..6 collapse pits
            cluster = rng.integers(3, 7)
            for _ in range(cluster):
                ox = x + int(rng.normal(0, 6))
                oy = y + int(rng.normal(0, 6))
                if not (0 <= ox < w and 0 <= oy < h):
                    continue
                if arch_pix[oy, ox] != lava_idx:
                    continue
                r = rng.integers(2, 5)
                draw.ellipse([ox - r, oy - r, ox + r, oy + r],
                             fill=(6, 4, 4, 230))
                draw.ellipse([ox - r - 1, oy - r - 1,
                              ox + r + 1, oy + r + 1],
                             outline=(50, 46, 42, 160), width=1)
        elif a == polar_idx and rng.random() < 0.12:
            r = rng.integers(1, 3)
            draw.ellipse([x - r, y - r, x + r, y + r],
                         fill=(240, 250, 255, 170))
        elif a == kreep_idx and rng.random() < 0.025:
            r = rng.integers(10, 24)
            for k, alpha in [(r, 14), (r * 0.6, 22), (r * 0.3, 38)]:
                draw.ellipse([x - k, y - k, x + k, y + k],
                             fill=(220, 140, 90, alpha))

    return np.asarray(img)


# ---------------------------------------------------------------------------
# Helpers for saving and labelling
# ---------------------------------------------------------------------------

def save_gray(arr, path):
    a = (arr - arr.min()) / max(1e-6, arr.max() - arr.min())
    Image.fromarray((a * 255).astype(np.uint8), mode="L").save(path)
    print(f"  wrote {path}")


def save_rgb(arr, path):
    Image.fromarray(arr.astype(np.uint8)).save(path)
    print(f"  wrote {path}")


def label_image(img, text, font_size=22):
    """Add a small caption at top-left."""
    out = img.copy()
    draw = ImageDraw.Draw(out, "RGBA")
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
    except OSError:
        font = ImageFont.load_default()
    pad = 6
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.rectangle([0, 0, tw + pad * 2, th + pad * 2], fill=(0, 0, 0, 180))
    draw.text((pad, pad), text, fill=(255, 255, 255, 255), font=font)
    return out


# ---------------------------------------------------------------------------
# Build pipeline
# ---------------------------------------------------------------------------

def build_planet(seed=SEED):
    rng = np.random.default_rng(seed)
    shape = (SIZE, SIZE)

    print("[1/6] heightmap (FBM)...")
    # Very gentle relief — just enough to keep the surface from looking
    # like a perfectly flat sheet. With craters as the only "real"
    # features, even modest FBM amplitude reads as ugly soft circular
    # dark patches under hillshade. We compress the FBM range to ~0.25
    # of its raw amplitude so the surface stays close to flat.
    base = fbm(shape, octaves=5, base_scale=384, persistence=0.5, rng=rng)
    detail = fbm(shape, octaves=3, base_scale=128, persistence=0.5, rng=rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * 0.30
    save_gray(height, os.path.join(OUT, "stage1_heightmap.png"))

    print("[2/6] mare basins (disabled — read as circular dark patches)...")
    # We still emit the mask so the pipeline diagram has a slot, but we
    # don't apply any height or albedo effect. User feedback: the soft
    # circular basins read as ugly cast shadows on the surface.
    mare = mare_field(shape, rng, n=3) * 0.0
    height_with_mare = height
    save_gray(mare_field(shape, rng, n=3), os.path.join(OUT, "stage2_mare.png"))

    print("[3/6] crater field...")
    craters = sample_craters(shape, rng)
    height_with_craters = apply_craters(height_with_mare.copy(), craters, rng)
    # Pink (1/f) noise regolith texture, amp 0.03 with sigma=1.5 blur
    # — softer than raw pink at the same amp, gives the "v3 pink + blur"
    # look from the texture-comparison sheet.
    height_with_craters = height_with_craters + 0.03 * gaussian_blur(
        pink_noise(shape, rng), 1.5)
    save_gray(height_with_craters, os.path.join(OUT, "stage3_craters.png"))

    print("[4/6] hillshade + cast shadows...")
    sh = hillshade(height_with_craters)
    cast = cast_shadows(height_with_craters)
    save_gray(sh, os.path.join(OUT, "stage4_hillshade.png"))
    save_gray(cast, os.path.join(OUT, "stage4b_cast_shadows.png"))

    print("[5/6] archetype tinting...")
    arch_grid = assign_archetype_grid(rng)
    arch_pix = archetype_pixel_map(arch_grid)
    rgb, albedo = colourise(height_with_craters, mare, sh, arch_pix, rng,
                            cast_mask=cast, albedo_height=height)
    save_rgb(albedo, os.path.join(OUT, "stage5_albedo.png"))
    save_rgb(rgb, os.path.join(OUT, "stage5_archetypes.png"))

    print("[6/6] decals + dust...")
    final = add_decals(rgb, arch_pix, height_with_craters, rng)
    # Regolith dust grain: very fine noise modulating brightness +/- 4%.
    # Adds tactile feel without going through hillshade so no contour bands.
    grain = (rng.random((SIZE, SIZE)).astype(np.float32) - 0.5) * 0.10
    grain_blur = np.asarray(
        Image.fromarray(((grain + 0.5) * 255).astype(np.uint8))
        .filter(ImageFilter.GaussianBlur(0.6)),
        dtype=np.float32) / 255.0 - 0.5
    final = np.clip(final.astype(np.float32) * (1.0 + grain_blur[..., None] * 0.18),
                    0, 255).astype(np.uint8)
    save_rgb(final, os.path.join(OUT, "stage6_final.png"))

    return {
        "height": height_with_craters,
        "mare": mare,
        "hillshade": sh,
        "arch_grid": arch_grid,
        "arch_pix": arch_pix,
        "albedo": albedo,
        "rgb": rgb,
        "final": final,
    }


# ---------------------------------------------------------------------------
# Presentation outputs
# ---------------------------------------------------------------------------

def render_archetype_tiles():
    """Render one 320-px sample per archetype, using the same pipeline forced
    to a single archetype. Saved as a horizontal strip with labels."""
    tile_px = 320
    rng = np.random.default_rng(SEED + 7)
    tiles = []
    for arch_name in ARCHETYPE_ORDER:
        local_rng = np.random.default_rng(rng.integers(0, 2**31))
        h = fbm((tile_px, tile_px), 6, 64, 0.55, local_rng)
        h += 0.3 * fbm((tile_px, tile_px), 4, 16, 0.6, local_rng)
        mare = mare_field((tile_px, tile_px), local_rng, n=1) * 0.4
        h_with = h - 0.3 * mare
        # archetype tile is 320x320 = 1/25 of full planet area, but we
        # want it to look populated — 6/2/1 gives a richer preview.
        h_with = apply_craters(h_with, sample_craters(
            (tile_px, tile_px), local_rng,
            count_small=6, count_med=2, count_big=1), local_rng)
        h_with = h_with + 0.03 * gaussian_blur(
            pink_noise((tile_px, tile_px), local_rng), 1.5)
        sh = hillshade(h_with, z_factor=35.0, smooth_px=1.0)
        cast = cast_shadows(h_with, z_factor=35.0, max_distance_px=40.0)
        arch_idx = ARCHETYPE_ORDER.index(arch_name)
        arch_pix_local = np.full((tile_px, tile_px), arch_idx, dtype=np.int32)
        rgb, _ = colourise(h_with, mare, sh, arch_pix_local, local_rng,
                           cast_mask=cast, albedo_height=h)
        rgb = add_decals(rgb, arch_pix_local, h_with, local_rng)
        img = Image.fromarray(rgb)
        img = label_image(img, ARCHETYPES[arch_name].name, font_size=18)
        tiles.append(img)

    strip_w = tile_px * len(tiles) + (len(tiles) - 1) * 6
    strip = Image.new("RGB", (strip_w, tile_px), (12, 12, 14))
    x = 0
    for t in tiles:
        strip.paste(t, (x, 0))
        x += tile_px + 6
    out = os.path.join(OUT, "archetype_tiles.png")
    strip.save(out)
    print(f"  wrote {out}")


def render_detail_zoom(planet_data):
    """Crop a 400x400 region from the final, upscale 2x for clarity."""
    final = Image.fromarray(planet_data["final"])
    x0, y0 = 700, 500     # area chosen empirically to land on craters
    crop = final.crop((x0, y0, x0 + 400, y0 + 400))
    crop = crop.resize((800, 800), Image.LANCZOS)
    crop = label_image(crop, "Detail zoom (2x)", font_size=22)
    out = os.path.join(OUT, "detail_zoom.png")
    crop.save(out)
    print(f"  wrote {out}")


def render_old_baseline():
    """Reproduce the existing in-game look (3 random tiles stamped)."""
    asset_dir = os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "..", "src", "assets"))
    tiles = [Image.open(os.path.join(asset_dir, f"moonsurface_tile{i}.png"))
             .convert("RGB") for i in (1, 2, 3)]
    img = Image.new("RGB", (SIZE, SIZE), (60, 60, 60))
    rng = np.random.default_rng(SEED)
    tw, th = tiles[0].size
    for y in range(0, SIZE, th):
        for x in range(0, SIZE, tw):
            img.paste(tiles[int(rng.integers(0, 3))], (x, y))
    out = os.path.join(OUT, "old_baseline.png")
    img.save(out)
    print(f"  wrote {out}")
    return img


def render_comparison(old_img, new_arr):
    """Side-by-side at 800px each."""
    panel = 800
    composite = Image.new("RGB", (panel * 2 + 8, panel + 40), (12, 12, 14))
    a = old_img.resize((panel, panel), Image.LANCZOS)
    b = Image.fromarray(new_arr).resize((panel, panel), Image.LANCZOS)
    a = label_image(a, "BEFORE  (current 3-tile shuffle)")
    b = label_image(b, "AFTER  (procedural + archetype-tinted)")
    composite.paste(a, (0, 40))
    composite.paste(b, (panel + 8, 40))
    draw = ImageDraw.Draw(composite)
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    draw.text((10, 8), "raylib-colony  --  planet visual prototype",
              fill=(230, 230, 230), font=font)
    out = os.path.join(OUT, "comparison.png")
    composite.save(out)
    print(f"  wrote {out}")


def render_stages_composite():
    """2x3 grid showing each pipeline step on the same seed."""
    stages = [
        ("stage1_heightmap.png", "1. heightmap (FBM)"),
        ("stage2_mare.png",      "2. mare basins"),
        ("stage3_craters.png",   "3. craters carved"),
        ("stage4_hillshade.png", "4. hillshade"),
        ("stage5_archetypes.png","5. archetype tints"),
        ("stage6_final.png",     "6. + decals + dust"),
    ]
    cell = 540
    pad = 6
    cols, rows = 3, 2
    W = cols * cell + (cols + 1) * pad
    H = rows * cell + (rows + 1) * pad
    canvas = Image.new("RGB", (W, H), (16, 16, 18))
    for i, (name, label) in enumerate(stages):
        path = os.path.join(OUT, name)
        if not os.path.exists(path):
            continue
        im = Image.open(path).convert("RGB").resize((cell, cell), Image.LANCZOS)
        im = label_image(im, label, font_size=20)
        cx = pad + (i % cols) * (cell + pad)
        cy = pad + (i // cols) * (cell + pad)
        canvas.paste(im, (cx, cy))
    out = os.path.join(OUT, "pipeline_stages.png")
    canvas.save(out)
    print(f"  wrote {out}")


def render_seed_variants():
    """Render 4 small planets with different seeds to show variety."""
    panel = 480
    seeds = [SEED, SEED + 1, SEED + 2, SEED + 3]
    composite = Image.new("RGB", (panel * 2 + 8, panel * 2 + 8), (16, 16, 18))
    for i, s in enumerate(seeds):
        rng = np.random.default_rng(s)
        small_size = 800
        shape = (small_size, small_size)
        base = fbm(shape, 5, 192, 0.5, rng)
        detail = fbm(shape, 3, 64, 0.5, rng)
        height = 0.92 * base + 0.08 * detail
        mare = mare_field(shape, rng, n=3)
        h2 = height - 0.20 * mare
        # seed-variants canvas is 800x800 = 1/4 of full planet area.
        h2 = apply_craters(h2, sample_craters(
            shape, rng,
            count_small=4, count_med=2, count_big=1), rng)
        h2 = h2 + 0.03 * gaussian_blur(pink_noise(shape, rng), 1.5)
        sh = hillshade(h2, z_factor=35.0, smooth_px=1.0)
        cast = cast_shadows(h2, z_factor=35.0, max_distance_px=40.0)
        ag = assign_archetype_grid(rng)
        py = np.repeat(np.arange(PLANET_GRID), small_size // PLANET_GRID)
        px = np.repeat(np.arange(PLANET_GRID), small_size // PLANET_GRID)
        arch_pix = ag[py[:, None], px[None, :]]
        rgb, _ = colourise(h2, mare, sh, arch_pix, rng, cast_mask=cast,
                           albedo_height=height)
        rgb = add_decals(rgb, arch_pix, h2, rng)
        im = Image.fromarray(rgb).resize((panel, panel), Image.LANCZOS)
        im = label_image(im, f"seed {s}", font_size=18)
        x = (i % 2) * (panel + 8)
        y = (i // 2) * (panel + 8)
        composite.paste(im, (x, y))
    out = os.path.join(OUT, "seed_variants.png")
    composite.save(out)
    print(f"  wrote {out}")


def render_archetype_grid_overlay(planet_data):
    """Overlay archetype labels on the final planet to show the biome layout."""
    img = Image.fromarray(planet_data["final"]).copy()
    draw = ImageDraw.Draw(img, "RGBA")
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 11)
    except OSError:
        font = ImageFont.load_default()
    arch_grid = planet_data["arch_grid"]
    for gy in range(PLANET_GRID):
        for gx in range(PLANET_GRID):
            label = ARCHETYPE_ORDER[arch_grid[gy, gx]]
            short = {"MARE_INDUSTRIAL": "MARE",
                     "HIGHLAND_CONSTRUCTION": "HIGH",
                     "POLAR_VOLATILE": "POLAR",
                     "KREEP_SCIENTIFIC": "KREEP",
                     "LAVA_TUBE": "LAVA",
                     "MIXED": "MIX"}[label]
            x = gx * PX_PER_CELL + 4
            y = gy * PX_PER_CELL + 4
            draw.rectangle([x, y, x + 38, y + 14], fill=(0, 0, 0, 140))
            draw.text((x + 2, y), short, fill=(255, 255, 255, 220), font=font)
    out = os.path.join(OUT, "planet_with_archetypes.png")
    img.save(out)
    print(f"  wrote {out}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("== building planet ==")
    data = build_planet()
    # Save the pretty version under a nicer name too
    Image.fromarray(data["final"]).save(os.path.join(OUT, "planet_full.png"))
    print(f"  wrote {os.path.join(OUT, 'planet_full.png')}")

    print("\n== archetype tile strip ==")
    render_archetype_tiles()

    print("\n== detail zoom ==")
    render_detail_zoom(data)

    print("\n== old baseline reference ==")
    old = render_old_baseline()

    print("\n== before/after ==")
    render_comparison(old, data["final"])

    print("\n== archetype overlay ==")
    render_archetype_grid_overlay(data)

    print("\n== pipeline stages composite ==")
    render_stages_composite()

    print("\n== seed variants ==")
    render_seed_variants()

    print("\nDone. Outputs in", OUT)


if __name__ == "__main__":
    main()
