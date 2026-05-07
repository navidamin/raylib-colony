"""Pass B — per-zone procedural surface generator.

Replaces / augments the locked procedural pipeline so that the
*Regional View* (which replaces Colony view) shows terrain matching
the selected zone's real characteristics:

  * Fresh young crater zone → sharp dominant central crater + ray
    field hint + dense small surroundings.
  * Old crater zone → eroded shallow central feature + heavily
    cratered older surroundings.
  * Mare zone → flat dark plain with sparse small fresh craters,
    barely-visible wrinkle ridges from the FBM relief.
  * Landing-site zone → biome of the local terrain (basalt mare or
    highland breccia), with a small "landing pad" marker.
  * Polar zone → low sun altitude (long shadows), POLAR tint,
    permanently-shadowed dark patches.
  * Basin zone (SPA) → elevated relief, mixed crater density.

The base pipeline (sample_craters, apply_craters, hillshade,
cast_shadows, colourise, pink_noise) stays exactly as-is. We just
provide:

  * `params_for_zone(zone)` — maps ZoneInfo → procedural params
  * `render_regional_view(zone, ...)` — runs the pipeline with those
    params, plus optionally injects a forced central crater (e.g. for
    Tycho — the named crater dominates the view).

Outputs:
  output/regional_<slug>.png      — per-zone regional render
  output/regional_compare.png     — 3×2 grid of representative zones
"""

from __future__ import annotations

import math
import os
from dataclasses import dataclass

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    ARCHETYPES, ARCHETYPE_ORDER, apply_craters, cast_shadows, colourise,
    Crater, fbm, gaussian_blur, hillshade, pink_noise, sample_craters,
)
from zones_db import ALL_ZONES, ZoneInfo, lookup_by_name

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(PROTOTYPE_DIR, "output")

REGION_PX = 1200            # render resolution
REGION_KM = 100             # ~100 km on a side → 12 m/pixel ish


# ===========================================================================
# Zone → procedural params
# ===========================================================================

@dataclass
class RegionalParams:
    # Crater placement (passed to sample_craters)
    count_small: int
    count_med: int
    count_big: int
    size_scale: float
    size_variance: float
    age_alpha: float
    min_separation: float

    # Crater profile (passed to apply_craters)
    depth_scale: float
    depth_variance: float

    # Lighting (passed to hillshade / cast_shadows)
    sun_altitude_deg: float

    # Background / surface
    fbm_amp: float                # heightmap relief amplitude after compress
    texture_amp: float            # pink-noise texture amp
    texture_blur_sigma: float

    # Biome tint
    archetype: str

    # Forced central feature (for crater zones)
    central_crater_radius_frac: float | None = None  # 0..1 of REGION_PX/2
    central_crater_depth_amp: float = -0.5
    central_crater_age: float = 0.1   # 0=fresh, 1=eroded
    central_crater_central_peak: bool = False


def params_for_zone(zone: ZoneInfo) -> RegionalParams:
    """Pick reasonable procedural params for the zone's characteristics."""

    # --- defaults: gentle mare-like, locked-pipeline style ----------------
    p = RegionalParams(
        count_small=20, count_med=8, count_big=1,
        size_scale=1.2, size_variance=0.5,
        age_alpha=4.0, min_separation=1.35,
        depth_scale=1.0, depth_variance=1.0,
        sun_altitude_deg=35.0,
        fbm_amp=0.30,
        texture_amp=0.03, texture_blur_sigma=1.5,
        archetype="MARE_INDUSTRIAL",
    )

    # --- type-specific overrides ------------------------------------------
    if zone.feature_type == "mare":
        # Flat, low relief, sparse small craters, mare tint
        p.count_small = 14
        p.count_med = 4
        p.count_big = 0
        p.size_scale = 0.9
        p.depth_scale = 0.6
        p.fbm_amp = 0.18
        p.archetype = "MARE_INDUSTRIAL"

    elif zone.feature_type == "crater":
        # Forced big crater at the centre. Sharpness depends on age.
        is_young = zone.age_ga is not None and zone.age_ga < 1.0
        is_old = zone.age_ga is not None and zone.age_ga > 3.0
        # Surrounding terrain biased to highland (most named craters
        # in our list are in highland regions, except a few mare ones).
        local_archetype = ("HIGHLAND_CONSTRUCTION"
                            if "highland" in (zone.terrain or "").lower()
                            or "anorthos" in (zone.dominant_rock or "").lower()
                            or zone.feature_type == "crater"
                            else "MARE_INDUSTRIAL")
        p.archetype = local_archetype

        # Surrounding small craters
        p.count_small = 25 if is_young else 35
        p.count_med = 6
        p.count_big = 0   # the forced central crater IS the big one
        p.size_scale = 0.7  # smaller surrounding craters
        p.fbm_amp = 0.12
        p.age_alpha = 2.0 if is_young else 4.5
        p.depth_scale = 1.3 if is_young else 0.7
        p.depth_variance = 0.8

        # Central crater geometry
        p.central_crater_radius_frac = 0.42
        if is_young:
            p.central_crater_depth_amp = -0.95
            p.central_crater_age = 0.05
            p.central_crater_central_peak = zone.diameter_km > 60
        elif is_old:
            p.central_crater_depth_amp = -0.45
            p.central_crater_age = 0.85    # heavily eroded
            p.central_crater_central_peak = False
        else:
            p.central_crater_depth_amp = -0.70
            p.central_crater_age = 0.40
            p.central_crater_central_peak = zone.diameter_km > 80

    elif zone.feature_type == "landing":
        # Use surrounding biome — most landings are in mare
        is_mare = "basalt" in (zone.dominant_rock or "").lower() \
                   or "mare" in (zone.terrain or "").lower()
        p.archetype = "MARE_INDUSTRIAL" if is_mare else "HIGHLAND_CONSTRUCTION"
        p.count_small = 16 if is_mare else 30
        p.count_med = 3 if is_mare else 8
        p.count_big = 0
        p.size_scale = 0.8 if is_mare else 0.9
        p.depth_scale = 0.5 if is_mare else 0.9
        p.fbm_amp = 0.10 if is_mare else 0.20

    elif zone.feature_type == "basin":
        # Mega-basin like SPA. Higher relief, mixed crater density.
        p.archetype = "MIXED"
        p.count_small = 35
        p.count_med = 10
        p.count_big = 2
        p.size_scale = 1.3
        p.fbm_amp = 0.40   # much higher relief
        p.depth_variance = 1.4

    # --- modifiers from zone properties -----------------------------------
    if zone.permanently_shadowed:
        p.archetype = "POLAR_VOLATILE"
        p.sun_altitude_deg = 6.0    # very low sun for long shadows
    elif abs(zone.lat) > 70:
        # High-latitude — shallow sun, polar tint blend
        p.sun_altitude_deg = 18.0
        if zone.feature_type != "crater":
            p.archetype = "POLAR_VOLATILE"

    # KREEP regions get warm tint (would be applied via archetype tint —
    # we'd extend ARCHETYPES if we had a richer palette; for now the
    # composition_notes are visible in the info panel).
    if zone.thorium_ppm is not None and zone.thorium_ppm > 10:
        # Procellarum / Aristarchus territory — KREEP biome
        if p.archetype not in ("POLAR_VOLATILE",):
            p.archetype = "KREEP_SCIENTIFIC"

    return p


# ===========================================================================
# Buildability mask
# ===========================================================================

def buildability_mask(heightmap: np.ndarray, craters,
                       crater_rim_factor: float = 1.05,
                       slope_threshold_deg: float = 18.0,
                       z_factor: float = 75.0) -> np.ndarray:
    """1.0 = buildable, 0.0 = forbidden.

    Two rules:
      1. Inside any crater's rim (distance < `crater_rim_factor * r`)
         is unbuildable — bowl walls + debris + sometimes permanent
         shadow.
      2. Anywhere the local slope exceeds `slope_threshold_deg` is
         unbuildable — too steep for surface infrastructure. Catches
         small unlisted craters AND any FBM-induced ridges.
    """
    h, w = heightmap.shape
    mask = np.ones((h, w), dtype=np.float32)

    # 1. Crater rim exclusion — vectorized per crater on a tight bbox.
    for c in craters:
        # bbox extends slightly past the rim for the soft falloff
        margin = c.r * crater_rim_factor
        x0 = max(0, int(c.cx - margin))
        x1 = min(w, int(c.cx + margin) + 1)
        y0 = max(0, int(c.cy - margin))
        y1 = min(h, int(c.cy + margin) + 1)
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        d = np.sqrt((xx - c.cx) ** 2 + (yy - c.cy) ** 2)
        # Soft inner-edge: already 1.0 outside the rim, taper to 0
        # over the last 5% of radius.
        inside = d < c.r * crater_rim_factor
        # Smoothstep falloff from rim to interior over ~10% of r
        falloff_band = max(2.0, c.r * 0.10)
        t = np.clip((c.r * crater_rim_factor - d) / falloff_band, 0.0, 1.0)
        local = 1.0 - t * t * (3.0 - 2.0 * t)   # smoothstep
        mask[y0:y1, x0:x1] = np.minimum(mask[y0:y1, x0:x1],
                                         np.where(inside, local, 1.0))

    # 2. Slope cutoff — applied to a smoothed heightmap so the
    #    pink-noise texture (sub-meter regolith roughness) doesn't
    #    flag every pixel as steep. We want terrain-scale slopes,
    #    not micro-texture.
    smoothed = gaussian_blur(heightmap, 3.5)
    dy, dx = np.gradient(smoothed * z_factor)
    slope_rad = np.arctan(np.hypot(dx, dy))
    slope_deg = np.degrees(slope_rad)
    too_steep = slope_deg > slope_threshold_deg
    mask = np.where(too_steep, 0.0, mask)

    return mask


def overlay_unbuildable(rgb: np.ndarray, mask: np.ndarray,
                         tint_strength: float = 0.30) -> np.ndarray:
    """Blend a subtle warm-red tint over unbuildable pixels so the
    player can see at a glance where they can't build."""
    red = np.array([200.0, 70.0, 60.0], dtype=np.float32)
    forbidden = (1.0 - mask)[..., None]
    out = rgb.astype(np.float32) * (1.0 - tint_strength * forbidden) \
          + red * tint_strength * forbidden
    return np.clip(out, 0, 255).astype(np.uint8)


def save_mask_png(mask: np.ndarray, path: str):
    """Save a 1-channel buildability mask PNG (white = buildable,
    black = forbidden) for the C++ side to load."""
    arr = np.clip(mask * 255, 0, 255).astype(np.uint8)
    Image.fromarray(arr, mode="L").save(path)




def _make_central_crater(p: RegionalParams, region_px: int) -> Crater | None:
    if p.central_crater_radius_frac is None:
        return None
    r = (region_px / 2) * p.central_crater_radius_frac
    return Crater(
        cx=region_px / 2,
        cy=region_px / 2,
        r=r,
        age=p.central_crater_age,
        has_peak=p.central_crater_central_peak,
    )


def render_regional_view(zone: ZoneInfo, output_path: str,
                          region_px: int = REGION_PX,
                          seed: int = 12345) -> Image.Image:
    """Render a regional procedural view for the given zone."""
    p = params_for_zone(zone)
    rng = np.random.default_rng(seed ^ (hash(zone.name) & 0xFFFF))

    shape = (region_px, region_px)

    # Heightmap: gentle FBM, amp scaled per zone
    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * p.fbm_amp

    # Crater placement — sample random craters, prepend the forced
    # central crater if any.
    craters = sample_craters(
        shape, rng,
        count_small=p.count_small,
        count_med=p.count_med,
        count_big=p.count_big,
        min_separation=p.min_separation,
        age_alpha=p.age_alpha,
        size_scale=p.size_scale,
        size_variance=p.size_variance,
    )
    central = _make_central_crater(p, region_px)
    if central is not None:
        # Remove any random craters that overlap the central one's
        # rim — they'd look like accidental inside-the-bowl impacts.
        cleaned = [central]
        for c in craters:
            d = math.hypot(c.cx - central.cx, c.cy - central.cy)
            if d > (central.r + c.r) * 1.10:
                cleaned.append(c)
        craters = cleaned

    h_with = apply_craters(height.copy(), craters, rng,
                            depth_variance=p.depth_variance,
                            depth_scale=p.depth_scale)

    # If the central crater is depth-tuned per-zone, we have to apply
    # it manually after — sample_craters/apply_craters apply default
    # depth_amp. The cleanest way is to override by re-applying the
    # central crater alone at its custom depth.
    if central is not None and p.central_crater_radius_frac:
        # Strip out the central one's contribution and re-stamp it
        # with the per-zone depth amplitude. Simpler: subtract its
        # default-depth contribution then add the custom one.
        # Implementation below uses a fresh override.
        h_central_only = np.zeros(shape, dtype=np.float32)
        # apply_craters with default scale to get the "default" carving
        h_with_default = apply_craters(
            np.zeros(shape, dtype=np.float32), [central],
            np.random.default_rng(0),
            depth_variance=1.0, depth_scale=1.0)
        # And with the per-zone scale to get the desired carving
        target_scale = abs(p.central_crater_depth_amp) / 0.42  # vs apply_craters baseline
        h_with_tuned = apply_craters(
            np.zeros(shape, dtype=np.float32), [central],
            np.random.default_rng(0),
            depth_variance=1.0, depth_scale=target_scale)
        # Replace
        h_with = h_with - h_with_default + h_with_tuned

    # Surface texture (locked: pink noise + sigma 1.5 blur)
    h_with = h_with + p.texture_amp * gaussian_blur(
        pink_noise(shape, rng), p.texture_blur_sigma)

    # Hillshade + cast shadows at the per-zone sun altitude
    sh = hillshade(h_with, altitude_deg=p.sun_altitude_deg, z_factor=75.0,
                    smooth_px=1.0)
    cast = cast_shadows(h_with, altitude_deg=p.sun_altitude_deg,
                         z_factor=75.0)

    # Biome tint — single archetype across the whole region.
    arch_idx = ARCHETYPE_ORDER.index(p.archetype)
    arch_pix = np.full(shape, arch_idx, dtype=np.int32)

    rgb, _ = colourise(h_with, np.zeros_like(height), sh, arch_pix, rng,
                        cast_mask=cast, albedo_height=height)

    # Buildability mask: forbid crater interiors + steep slopes.
    mask = buildability_mask(h_with, craters,
                              slope_threshold_deg=18.0)
    rgb_with_overlay = overlay_unbuildable(rgb, mask)

    # Save the mask as a sibling PNG so the C++ side can consume it
    # directly at sect-placement time (white = ok, black = forbidden).
    mask_path = os.path.splitext(output_path)[0] + "_buildable.png"
    save_mask_png(mask, mask_path)

    img = Image.fromarray(rgb_with_overlay).convert("RGB")
    _annotate(img, zone, p, mask)
    img.save(output_path)
    print(f"  wrote {output_path}  (mask: {mask_path})")
    return img


def _annotate(img: Image.Image, zone: ZoneInfo, p: RegionalParams,
               mask: np.ndarray | None = None):
    """Title bar + parameter caption."""
    draw = ImageDraw.Draw(img, "RGBA")
    try:
        font_t = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24)
        font_s = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13)
    except OSError:
        font_t = ImageFont.load_default()
        font_s = ImageFont.load_default()

    # Top-left: zone name + type
    draw.rectangle([0, 0, img.width, 56], fill=(0, 0, 0, 140))
    type_label = {"mare": "MARE", "crater": "CRATER",
                   "landing": "LANDING SITE", "basin": "BASIN"}.get(
        zone.feature_type, zone.feature_type.upper())
    draw.text((16, 8), zone.name, fill=(255, 255, 255), font=font_t)
    draw.text((16, 36), f"{type_label}  ·  Ø {zone.diameter_km:,.0f} km"
              + (f"  ·  {zone.age_ga} Ga" if zone.age_ga else ""),
              fill=(180, 200, 220), font=font_s)

    # Top-right: buildable-area stat
    if mask is not None:
        buildable_frac = float(mask.mean())
        stat = f"buildable: {buildable_frac * 100:.0f}%"
        tw = draw.textlength(stat, font=font_s)
        draw.text((img.width - tw - 16, 38), stat,
                  fill=(180, 220, 200), font=font_s)

    # Bottom-left: procedural params summary
    spec = (f"density={p.count_small + p.count_med + p.count_big}  "
            f"size×{p.size_scale:.1f}  "
            f"depth×{p.depth_scale:.2f}  "
            f"sun {p.sun_altitude_deg:.0f}°  "
            f"biome {p.archetype.split('_')[0]}")
    draw.rectangle([0, img.height - 28, img.width, img.height], fill=(0, 0, 0, 140))
    draw.text((16, img.height - 22), spec,
              fill=(180, 220, 200), font=font_s)

    # Bottom-right: legend for the unbuildable overlay
    legend = "■ no-build (crater / steep)"
    tw = draw.textlength(legend, font=font_s)
    # red square + label
    draw.rectangle([img.width - tw - 32, img.height - 22,
                     img.width - tw - 22, img.height - 12],
                    fill=(200, 70, 60))
    draw.text((img.width - tw - 16, img.height - 22), legend,
              fill=(220, 200, 200), font=font_s)


# ===========================================================================
# Comparison sheet
# ===========================================================================

def render_comparison():
    """3×2 sheet showing six representative zones."""
    examples = ["Tycho", "Copernicus", "Mare Imbrium",
                "Apollo 11", "Shackleton", "South Pole–Aitken basin"]
    panels = []
    for name in examples:
        z = lookup_by_name(name)
        if z is None:
            print(f"  [skip] no zone named {name!r}")
            continue
        slug = (name.lower().replace(" ", "_")
                  .replace("'", "").replace("–", "-"))
        out = os.path.join(OUT, f"regional_{slug}.png")
        panels.append(render_regional_view(z, out))

    cols, rows = 3, 2
    pad = 12
    title_h = 60
    panel_w, panel_h = panels[0].size
    out_w = cols * panel_w + (cols - 1) * pad
    out_h = title_h + rows * panel_h + (rows - 1) * pad
    canvas = Image.new("RGB", (out_w, out_h), (10, 12, 18))
    draw = ImageDraw.Draw(canvas)
    try:
        ftitle = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
    except OSError:
        ftitle = ImageFont.load_default()
    draw.text((20, 16),
              "Regional View  —  procedural surfaces tuned per real zone, "
              "red = no-build (crater interior or steep slope)",
              fill=(220, 220, 220), font=ftitle)
    for i, p in enumerate(panels):
        c = i % cols
        r = i // cols
        x = c * (panel_w + pad)
        y = title_h + r * (panel_h + pad)
        canvas.paste(p, (x, y))
    out_path = os.path.join(OUT, "regional_compare.png")
    canvas.save(out_path)
    print(f"  wrote {out_path}")


def main():
    print(f"== Pass B: per-zone procedural regional views ==")
    render_comparison()


if __name__ == "__main__":
    main()
