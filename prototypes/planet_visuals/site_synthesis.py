"""Site synthesis — conditioned procedural detail below the WAC floor.

The direction (2026-08-13): real imagery is the *macro truth*, procedural
generation is *detail amplification*. The player zooms anywhere on the
orbital moon; at the deepest zoom (~90 km site window) the real LROC WAC
mosaic only has ~68x68 native pixels — a blur. This prototype replaces
that blur with generated detail that:

  1. is CONDITIONED on the real pixels — macro forms (Copernicus' rim,
     mare/highland boundaries) stay exactly where the real moon has them,
     because the real crop is the low-frequency base of the output;
  2. only invents detail BELOW the source resolution floor (~1.3 km/px):
     small craters, regolith texture, crisp shadows — things the real
     data cannot resolve but that must exist at this scale;
  3. is DETERMINISTIC BY LOCATION — the RNG is seeded from quantised
     lat/lon, so zooming to the same place twice gives the same ground,
     with no stored assets;
  4. is styled as game pixel art — rendered at a low internal resolution
     with a fixed lunar palette, upscaled nearest-neighbour.

Outputs (in output/):
  site_synthesis_compare.png     real blurry vs three style variants
  site_synthesis_drilldown.png   the 4-panel zoom strip with the site
                                 panel synthesized instead of blurry
  site_synthesis_locations.png   three different sites (crater / mare /
                                 highland) real vs synthesized — shows
                                 the conditioning adapting to terrain

Run:  python3 site_synthesis.py
"""

from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (apply_craters, cast_shadows, fbm, gaussian_blur,
                      hillshade, pink_noise, sample_craters)
from multi_zoom import WAC_PATH, crop_equirect_region, latlon_to_disc
from wrap_to_sphere import wrap_to_sphere

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(PROTOTYPE_DIR, "output")

# Internal synthesis resolution. At a 91 km site window this is ~300 m
# per synthesized pixel; displayed at 3x nearest-neighbour = 900 px.
SYNTH_RES = 300
DISPLAY_RES = 900

# Site window, matching multi_zoom's panel 3 (site zoom).
SITE_SPAN_DEG = 3.0


# --- Determinism ----------------------------------------------------------

def location_seed(lat_deg: float, lon_deg: float) -> int:
    """Stable seed from quantised lat/lon. Same place -> same ground,
    forever, with no stored assets. Quantise to 0.01 deg (~300 m) so
    float drift can't change the seed."""
    qlat = int(round((lat_deg + 90.0) * 100))
    qlon = int(round((lon_deg + 180.0) * 100))
    # splitmix-style integer hash, stable across platforms
    x = (qlat * 73856093) ^ (qlon * 19349663)
    x &= 0xFFFFFFFF
    x = (x ^ (x >> 16)) * 0x45D9F3B & 0xFFFFFFFF
    x = (x ^ (x >> 16)) * 0x45D9F3B & 0xFFFFFFFF
    return x ^ (x >> 16)


# --- Conditioned synthesis ------------------------------------------------

def crop_native(wac: np.ndarray, lat_centre: float, lon_centre: float,
                span_deg: float) -> np.ndarray:
    """Crop the equirect WAC to a centred lat/lon window at NATIVE
    resolution (no resize) and return grayscale float 0..1."""
    h_src, w_src = wac.shape[:2]
    lat0 = lat_centre - span_deg / 2
    lat1 = lat_centre + span_deg / 2
    lon0 = lon_centre - span_deg / 2
    lon1 = lon_centre + span_deg / 2
    y0 = max(0, int((90.0 - lat1) / 180.0 * h_src))
    y1 = min(h_src, int((90.0 - lat0) / 180.0 * h_src))
    x0 = max(0, int((lon0 + 180.0) / 360.0 * w_src))
    x1 = min(w_src, int((lon1 + 180.0) / 360.0 * w_src))
    crop = wac[y0:y1, x0:x1]
    gray = crop.astype(np.float32).mean(axis=2) / 255.0
    return gray

def synthesize_site(wac: np.ndarray, lat_deg: float, lon_deg: float,
                    span_deg: float = SITE_SPAN_DEG,
                    synth_res: int = SYNTH_RES):
    """Generate the detailed site luminance field, conditioned on the
    real WAC crop. Returns (lum 0..1 float array, macro 0..1 float array)
    at synth_res."""
    rng = np.random.default_rng(location_seed(lat_deg, lon_deg))
    shape = (synth_res, synth_res)

    # -- Macro truth: the real crop, upsampled smoothly. This is the
    #    low-frequency base — every big form in the output is real.
    #    Denoise at NATIVE resolution first: the 8K WAC is a JPEG, and
    #    its block/ringing artifacts otherwise get amplified into a
    #    visible ripple by the upsample + unsharp below.
    native = crop_native(wac, lat_deg, lon_deg, span_deg)
    native = gaussian_blur(native, 0.7)
    macro = np.asarray(
        Image.fromarray(native, mode="F").resize((synth_res, synth_res),
                                                 Image.BICUBIC),
        dtype=np.float32)

    # Unsharp mask recovers some edge contrast the upsample smeared,
    # without inventing anything.
    blur = gaussian_blur(macro, 5.0)
    macro_sharp = np.clip(macro + 0.40 * (macro - blur), 0.0, 1.0)

    # Adaptive contrast: expand the macro's tonal range so real forms
    # stay dominant after detail modulation — but around the crop's OWN
    # midpoint with capped gain. A full-range stretch turned quiet dark
    # maria into blotchy mid-grey; a mare must stay a dark calm plain.
    p_lo, p_hi = np.percentile(macro_sharp, [2.0, 98.0])
    spread = max(p_hi - p_lo, 1e-4)
    gain = min(2.2, max(1.0, 0.60 / spread))
    mid = 0.5 * (p_hi + p_lo)
    macro_sharp = np.clip(mid + (macro_sharp - mid) * gain, 0.0, 1.0)

    # -- Conditioning field: bright highland terrain is densely cratered
    #    and rough; dark mare is sparse and smooth. Derived from the
    #    real pixels, so the synthesis adapts to where the player zoomed.
    density = np.clip((macro - 0.22) / 0.45, 0.15, 1.0)
    roughness = 0.45 + 0.55 * density

    # -- Detail heightmap: only features BELOW the source floor.
    #    Source floor ~1.3 km/px -> craters under ~4 km diameter are
    #    invisible in the real data. At ~300 m per synth pixel that is
    #    a radius cap of ~7 px. size_scale=0.16 puts the crater buckets
    #    at roughly r 1.3..17 px (0.4..5 km dia); the few biggest sit at
    #    the floor boundary where real data fades out.
    craters = sample_craters(shape, rng,
                             count_small=70, count_med=16, count_big=3,
                             min_separation=1.25,
                             size_scale=0.22, size_variance=0.8)
    kept = []
    for c in craters:
        cy = min(synth_res - 1, max(0, int(c.cy)))
        cx = min(synth_res - 1, max(0, int(c.cx)))
        if rng.random() < density[cy, cx]:
            kept.append(c)

    height = np.zeros(shape, dtype=np.float32)
    apply_craters(height, kept, rng, depth_variance=1.0, depth_scale=1.0)

    # Regolith grain between craters — pink noise matches natural
    # rough-surface statistics. Quiet: it is texture, not terrain.
    height += 0.004 * pink_noise(shape, rng) * roughness
    # Gentle undulation so flat stretches are not billiard-table flat.
    height += 0.02 * (fbm(shape, 3, 64, 0.5, rng) - 0.5) * roughness

    # -- Shade the detail relief. Normalised so flat ground multiplies
    #    by 1.0 — the real macro brightness passes through untouched
    #    where we added nothing. The detail is a *modulation* of the
    #    real macro, never its replacement: mostly within +/-25%, with
    #    cast shadows allowed to go deep inside fresh craters.
    z = 110.0
    hs = hillshade(height, z_factor=z, smooth_px=0.6)
    flat_ref = float(hillshade(np.zeros(shape, dtype=np.float32),
                               z_factor=z, smooth_px=0.0)[0, 0])
    rel = np.clip(hs / max(flat_ref, 1e-4), 0.0, 1.5)
    light = cast_shadows(height, z_factor=z,
                         max_distance_px=18.0, step_px=1.5)

    lum = macro_sharp * (0.75 + 0.25 * rel) * (0.35 + 0.65 * light)

    # Faint fine albedo speckle (ray dust, boulders at sub-pixel scale).
    speckle = fbm(shape, 2, 4, 0.5, rng)
    lum *= 1.0 + 0.04 * (speckle - 0.5) * roughness

    return np.clip(lum, 0.0, 1.0), macro


# --- Styling --------------------------------------------------------------

def lunar_palette(n: int = 14):
    """Fixed game palette: dark cool shadows -> warm bright regolith.
    A ramp, so quantised output keeps its tonal ordering."""
    lo = np.array([16, 17, 24], dtype=np.float32)      # shadow: cool
    mid = np.array([108, 105, 102], dtype=np.float32)  # regolith grey
    hi = np.array([236, 232, 220], dtype=np.float32)   # sunlit: warm
    cols = []
    for i in range(n):
        t = i / (n - 1)
        if t < 0.5:
            c = lo + (mid - lo) * (t / 0.5)
        else:
            c = mid + (hi - mid) * ((t - 0.5) / 0.5)
        cols.append(tuple(int(v) for v in c))
    return cols


def apply_ramp(lum: np.ndarray) -> np.ndarray:
    """Continuous version of the palette ramp (no quantisation)."""
    n = 256
    lut = np.zeros((n, 3), dtype=np.float32)
    pal = lunar_palette(n)
    for i, c in enumerate(pal):
        lut[i] = c
    idx = np.clip((lum * (n - 1)).astype(np.int32), 0, n - 1)
    return lut[idx].astype(np.uint8)


def style_realistic(lum: np.ndarray) -> Image.Image:
    """Continuous tones through the lunar ramp, smooth upscale."""
    rgb = apply_ramp(lum)
    return Image.fromarray(rgb).resize((DISPLAY_RES, DISPLAY_RES),
                                       Image.LANCZOS)


def _quantise(lum: np.ndarray, n_tones: int, dither: bool) -> Image.Image:
    pal = lunar_palette(n_tones)
    # Build a PIL palette image for quantize()
    pal_img = Image.new("P", (1, 1))
    flat = []
    for c in pal:
        flat.extend(c)
    flat.extend([0, 0, 0] * (256 - n_tones))
    pal_img.putpalette(flat)
    rgb = Image.fromarray(apply_ramp(lum))
    d = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    q = rgb.quantize(palette=pal_img, dither=d)
    return q.convert("RGB").resize((DISPLAY_RES, DISPLAY_RES),
                                   Image.NEAREST)


def style_pixel(lum: np.ndarray) -> Image.Image:
    """Game pixel art: 14-tone palette, hard edges, nearest upscale."""
    return _quantise(lum, 14, dither=False)


def style_pixel_dither(lum: np.ndarray) -> Image.Image:
    """Same palette with error-diffusion dithering — softer gradients
    at the cost of a grainier surface."""
    return _quantise(lum, 14, dither=True)


def real_blurry(wac: np.ndarray, lat_deg: float, lon_deg: float,
                span_deg: float = SITE_SPAN_DEG) -> Image.Image:
    """What the game shows today: the native crop blown up smoothly.
    This is multi_zoom's panel 4."""
    return crop_equirect_region(wac, lat_deg, lon_deg,
                                lat_span=span_deg, lon_span=span_deg,
                                output_w=DISPLAY_RES, output_h=DISPLAY_RES)


# --- Composition helpers --------------------------------------------------

def _fonts():
    try:
        title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
        cap = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
    except OSError:
        title = ImageFont.load_default()
        cap = ImageFont.load_default()
    return title, cap


def compose_strip(panels, captions, title, output_path, panel_w=DISPLAY_RES):
    pad = 10
    title_h = 60
    cap_h = 30
    total_w = len(panels) * panel_w + (len(panels) - 1) * pad
    total_h = title_h + panel_w + cap_h
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    font_title, font_cap = _fonts()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18), title, fill=(220, 220, 220), font=font_title)
    for i, (p, cap) in enumerate(zip(panels, captions)):
        x = i * (panel_w + pad)
        canvas.paste(p.convert("RGB").resize((panel_w, panel_w)),
                     (x, title_h))
        cw = draw.textlength(cap, font=font_cap)
        draw.text((x + (panel_w - cw) // 2, title_h + panel_w + 6),
                  cap, fill=(230, 230, 230), font=font_cap)
    canvas.save(output_path)
    print(f"  wrote {output_path}")


# --- Outputs --------------------------------------------------------------

def render_compare(wac, lat, lon, name):
    lum, _ = synthesize_site(wac, lat, lon)
    panels = [real_blurry(wac, lat, lon),
              style_realistic(lum),
              style_pixel(lum),
              style_pixel_dither(lum)]
    captions = ["Real WAC (today: blurry)",
                "Synthesized — continuous",
                "Synthesized — pixel art 14-tone",
                "Synthesized — pixel art + dither"]
    compose_strip(panels, captions,
                  f"Site synthesis — {name} ({lat:+.1f}, {lon:+.1f}), "
                  f"~90 km window. Macro forms from real WAC, detail "
                  f"below the 1.3 km/px floor is generated.",
                  os.path.join(OUT, "site_synthesis_compare.png"))


def render_drilldown(wac, lat, lon, style_fn=style_pixel):
    """The multi_zoom 4-panel strip, with the site panel synthesized."""
    panel_w = DISPLAY_RES
    panels = []
    captions = []

    orb_path = os.path.join(OUT, "_synth_tmp_orbital.png")
    wrap_to_sphere(WAC_PATH, output_size=panel_w, output_path=orb_path,
                   extent="globe", camera_lon_deg=0.0,
                   apply_limb_darkening=False)
    img = Image.open(orb_path).convert("RGBA")
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    px, py, _ = latlon_to_disc(lat, lon, panel_w)
    box = panel_w * 0.18 / 2
    draw.rectangle([px - box, py - box, px + box, py + box],
                   outline=(255, 200, 100, 240), width=3)
    panels.append(Image.alpha_composite(img, overlay))
    captions.append("Orbital  (~3,500 km)  — real")
    os.remove(orb_path)

    for span, boxfrac, cap in [(50, 0.20, "Continental (~1,500 km) — real"),
                               (10, 0.30, "Regional (~300 km) — real")]:
        p = crop_equirect_region(wac, lat, lon, lat_span=span,
                                 lon_span=span,
                                 output_w=panel_w, output_h=panel_w)
        p = p.convert("RGBA")
        overlay = Image.new("RGBA", p.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        b = panel_w * boxfrac / 2
        draw.rectangle([panel_w / 2 - b, panel_w / 2 - b,
                        panel_w / 2 + b, panel_w / 2 + b],
                       outline=(255, 200, 100, 240), width=3)
        panels.append(Image.alpha_composite(p, overlay))
        captions.append(cap)

    lum, _ = synthesize_site(wac, lat, lon)
    panels.append(style_fn(lum).convert("RGBA"))
    captions.append("Site (~90 km) — SYNTHESIZED")

    compose_strip(panels, captions,
                  "Drill-down, Copernicus. Panels 1-3 real WAC; panel 4 "
                  "generated: real macro + procedural detail, seeded by "
                  "location.",
                  os.path.join(OUT, "site_synthesis_drilldown.png"))


def render_locations(wac, style_fn=style_pixel):
    """Three terrain types, real vs synthesized — the conditioning must
    visibly adapt (mare smooth/sparse, highland rough/dense)."""
    sites = [("Copernicus (crater)", 9.6, -20.0),
             ("Mare Imbrium (mare)", 32.8, -15.6),
             ("Tycho highlands", -43.3, -11.4)]
    half = DISPLAY_RES // 2
    panels = []
    captions = []
    for name, lat, lon in sites:
        lum, _ = synthesize_site(wac, lat, lon)
        real = real_blurry(wac, lat, lon).resize((half, half), Image.LANCZOS)
        synth = style_fn(lum).resize((half, half), Image.NEAREST)
        pair = Image.new("RGB", (half, DISPLAY_RES), (10, 12, 18))
        pair.paste(real.convert("RGB"), (0, 0))
        pair.paste(synth.convert("RGB"), (0, half))
        panels.append(pair)
        captions.append(f"{name}  (top: real, bottom: synth)")

    pad = 10
    title_h = 60
    cap_h = 30
    total_w = len(panels) * half + (len(panels) - 1) * pad
    total_h = title_h + DISPLAY_RES + cap_h
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    font_title, font_cap = _fonts()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18),
              "Zoom anywhere — same pipeline, three terrains. Synthesis "
              "is conditioned on the real pixels at each site.",
              fill=(220, 220, 220), font=font_title)
    for i, (p, cap) in enumerate(zip(panels, captions)):
        x = i * (half + pad)
        canvas.paste(p, (x, title_h))
        cw = draw.textlength(cap, font=font_cap)
        draw.text((x + max(0, (half - cw) // 2), title_h + DISPLAY_RES + 6),
                  cap, fill=(230, 230, 230), font=font_cap)
    path = os.path.join(OUT, "site_synthesis_locations.png")
    canvas.save(path)
    print(f"  wrote {path}")


def check_determinism(wac, lat=9.6, lon=-20.0):
    a, _ = synthesize_site(wac, lat, lon)
    b, _ = synthesize_site(wac, lat, lon)
    same = np.array_equal(a, b)
    print(f"  determinism check ({lat}, {lon}): "
          f"{'IDENTICAL' if same else 'MISMATCH — BUG'}")
    return same


def main():
    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    print("== determinism ==")
    check_determinism(wac)
    print("== compare strip (Copernicus) ==")
    render_compare(wac, 9.6, -20.0, "Copernicus")
    print("== drill-down with synthesized site panel ==")
    render_drilldown(wac, 9.6, -20.0)
    print("== three-terrain conditioning ==")
    render_locations(wac)


if __name__ == "__main__":
    main()
