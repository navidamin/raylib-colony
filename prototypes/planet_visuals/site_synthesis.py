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

from generate import cast_shadows, fbm, gaussian_blur, hillshade, pink_noise
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


# --- Real scale -----------------------------------------------------------

MOON_RADIUS_KM = 1737.4
KM_PER_DEG = math.pi * MOON_RADIUS_KM / 180.0     # 30.323 km per degree


def window_km(lat_deg: float, span_deg: float) -> float:
    """N-S extent of a lat-span window in km (also the E-W extent, since
    all crops here widen lon_span by 1/cos(lat) to stay square in km)."""
    return span_deg * KM_PER_DEG


def lon_span_for(lat_deg: float, lat_span_deg: float) -> float:
    """Longitude span that makes the window square in kilometres at
    this latitude. Without this, windows are square in DEGREES and the
    ground is squashed E-W by cos(lat) — 27% at Tycho's latitude."""
    c = max(0.2, math.cos(math.radians(lat_deg)))
    return lat_span_deg / c


def print_scale_table(synth_res: int = SYNTH_RES):
    """The real physical scale of every zoom level."""
    print(f"  Moon radius {MOON_RADIUS_KM} km -> "
          f"{KM_PER_DEG:.3f} km per degree")
    rows = [
        ("Orbital",      None,  "hemisphere, ~3,476 km disc"),
        ("Continental",  50.0,  None),
        ("Regional",     10.0,  None),
        ("Site",         3.0,   None),
        ("Local",        1.0,   None),
        ("Close",        1.0 / 3.0, None),
    ]
    for name, span, note in rows:
        if span is None:
            print(f"  {name:12s} {note}")
            continue
        km = window_km(0.0, span)
        m_px = km * 1000.0 / synth_res
        print(f"  {name:12s} {span:6.3f} deg  ->  {km:7.2f} km window"
              f"   ({m_px:7.1f} m/px at {synth_res}px)")


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
    """Crop the equirect WAC to a centred window at NATIVE resolution
    (no resize), grayscale float 0..1. lat_span = span_deg; lon span is
    widened by 1/cos(lat) so the window is SQUARE IN KILOMETRES."""
    h_src, w_src = wac.shape[:2]
    lon_span = lon_span_for(lat_centre, span_deg)
    lat0 = lat_centre - span_deg / 2
    lat1 = lat_centre + span_deg / 2
    lon0 = lon_centre - lon_span / 2
    lon1 = lon_centre + lon_span / 2
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
    macro_sharp = np.clip(mid + (macro_sharp - mid) * gain,
                          0.0, 1.0).astype(np.float32)

    # -- Conditioning field: bright highland terrain is densely cratered
    #    and rough; dark mare is sparse and smooth. Derived from the
    #    real pixels, so the synthesis adapts to where the player zoomed.
    density = np.clip((macro - 0.22) / 0.45, 0.15, 1.0)
    roughness = 0.45 + 0.55 * density

    lum = _texture_modulate(macro_sharp, rng)
    return lum, macro


def _texture_modulate(macro_sharp: np.ndarray,
                      rng: np.random.Generator,
                      amp: float = 1.0) -> np.ndarray:
    """Add surface texture to a macro base: regolith grain and gentle
    undulation, shaded and applied as a quiet modulation. NO invented
    craters (user decision 2026-08-13) — every form in the base passes
    through; this only makes the ground feel like ground."""
    shape = macro_sharp.shape

    # Rough terrain (bright highlands) gets more grain than maria.
    density = np.clip((macro_sharp - 0.22) / 0.45, 0.15, 1.0)
    roughness = 0.45 + 0.55 * density

    # RELIEF FROM THE REAL FORMS (anti-matte): the upsampled source has
    # flat, washed-out lighting, so a pure albedo modulation inherits
    # that matte look. Treat the macro luminance as a height proxy
    # (bright = raised, dark = sunk) and RELIGHT it with a directional
    # sun — the big shapes come back with crisp lit/shadow sides like
    # the native-resolution real imagery has.
    form_relief = (gaussian_blur(macro_sharp, 2.5) - 0.5) * 0.13

    height = form_relief.astype(np.float32)
    # Regolith grain — pink noise matches natural rough-surface
    # statistics. Quiet: it is texture, not terrain.
    height += 0.004 * amp * pink_noise(shape, rng) * roughness
    # Gentle undulation so flat stretches are not billiard-table flat.
    height += 0.02 * amp * (fbm(shape, 3, 64, 0.5, rng) - 0.5) * roughness

    # Shade the combined relief. Normalised so flat ground multiplies
    # by 1.0; the directional term now carries much more of the image.
    z = 110.0
    hs = hillshade(height, z_factor=z, smooth_px=0.6)
    flat_ref = float(hillshade(np.zeros(shape, dtype=np.float32),
                               z_factor=z, smooth_px=0.0)[0, 0])
    rel = np.clip(hs / max(flat_ref, 1e-4), 0.0, 1.6)
    light = cast_shadows(height, z_factor=z,
                         max_distance_px=22.0, step_px=1.5)

    lum = macro_sharp * (0.62 + 0.38 * rel) * (0.45 + 0.55 * light)

    # Faint fine albedo speckle (dust, sub-pixel boulders).
    speckle = fbm(shape, 2, 4, 0.5, rng)
    lum *= 1.0 + 0.04 * min(amp, 1.6) * (speckle - 0.5) * roughness

    # Gentle S-curve: deepen shadows, keep highlights — kills the
    # residual haze without clipping.
    lum = np.clip(lum, 0.0, 1.0)
    lum = lum * lum * (3.0 - 2.0 * lum) * 0.20 + lum * 0.80

    return np.clip(lum, 0.0, 1.0).astype(np.float32)


def synthesize_site_chain(wac: np.ndarray, lat_deg: float, lon_deg: float,
                          extra_levels: int = 2,
                          synth_res: int = SYNTH_RES):
    """Progressive deep zoom below the site window. Level 0 is the
    normal site synthesis from the real WAC (~90 km). Each deeper
    level takes the CENTRE THIRD of the previous level's output as its
    macro truth — real forms keep flowing down, and each level's
    texture becomes the next level's structure — then re-sharpens and
    adds grain at the finer scale. 3x zoom per level: ~90 -> ~30 ->
    ~10 km. Deterministic: each level salts the location seed.

    Returns a list of luminance arrays, coarsest first.
    """
    lum, _ = synthesize_site(wac, lat_deg, lon_deg, synth_res=synth_res)
    levels = [lum]
    third = synth_res // 3
    lo = (synth_res - third) // 2
    hi = lo + third
    for lvl in range(1, extra_levels + 1):
        rng = np.random.default_rng(
            (location_seed(lat_deg, lon_deg) ^ (0x9E3779B9 * lvl))
            & 0xFFFFFFFF)
        # The dtype/contiguity here is load-bearing: PIL mode "F" reads
        # the raw buffer as 4-byte floats, so a float64 array or a
        # strided slice view renders as garbage (binarized blobs).
        base = np.ascontiguousarray(levels[-1][lo:hi, lo:hi],
                                    dtype=np.float32)
        base = np.asarray(
            Image.fromarray(base, mode="F").resize((synth_res, synth_res),
                                                   Image.BICUBIC),
            dtype=np.float32)
        # Mild denoise then unsharp — same recipe as level 0, gentler
        # gain since contrast was already established upstream.
        base = gaussian_blur(base, 0.6)
        blur = gaussian_blur(base, 5.0)
        base = np.clip(base + 0.40 * (base - blur),
                       0.0, 1.0).astype(np.float32)
        # Grain grows with depth: the macro gets smoother each level,
        # so without this the deep panels read as fog.
        levels.append(_texture_modulate(base, rng, amp=1.0 + 0.7 * lvl))
    return levels


# --- Stylized pixel art ---------------------------------------------------
#
# The user's call (2026-08-13): precision in reproducing the surface is
# NOT the goal — smoothness and attractiveness are. So this path does
# not simulate lighting at all. The real crop only supplies the big
# tonal shapes; craters are drawn the way a pixel artist draws them:
# clean disc, dark shadow crescent on the sun side, lit crescent
# opposite, done. Chunky pixels, one small handsome palette.

PIX_RES = 150          # internal pixel grid; 150 * 6 = 900 display
PIX_TONES = 9          # palette depth


def pixel_palette(n: int = PIX_TONES):
    """Cool-shadow -> warm-light lunar ramp, tuned for charm over
    accuracy. Index 0 is the deepest shadow tone."""
    stops = [
        (26, 28, 44),      # deep shadow, blue-violet
        (52, 54, 74),
        (84, 86, 106),
        (117, 119, 136),
        (148, 149, 162),
        (176, 176, 185),
        (201, 200, 204),
        (223, 221, 218),
        (244, 241, 231),   # sunlit, warm
    ]
    if n == len(stops):
        return stops
    out = []
    for i in range(n):
        t = i / (n - 1) * (len(stops) - 1)
        a = int(t)
        b = min(a + 1, len(stops) - 1)
        f = t - a
        out.append(tuple(int(stops[a][k] + (stops[b][k] - stops[a][k]) * f)
                         for k in range(3)))
    return out


def synthesize_pixelart(wac: np.ndarray, lat_deg: float, lon_deg: float,
                        span_deg: float = SITE_SPAN_DEG,
                        pix_res: int = PIX_RES):
    """Stylized site view. Returns a (pix_res, pix_res) int array of
    palette indices."""
    rng = np.random.default_rng(location_seed(lat_deg, lon_deg))
    shape = (pix_res, pix_res)

    # Macro: real crop, denoised and SMOOTHED HARD — we only want the
    # big shapes (mare edge, main crater bowl, ray brightness).
    native = crop_native(wac, lat_deg, lon_deg, span_deg)
    native = gaussian_blur(native, 0.8)
    macro = np.asarray(
        Image.fromarray(native, mode="F").resize((pix_res, pix_res),
                                                 Image.BICUBIC),
        dtype=np.float32)
    # Light smoothing only — with no invented craters, the real forms
    # carry the whole picture; heavy blur turned them into amoebas.
    macro = gaussian_blur(macro, 0.7)

    # Gentle adaptive expansion around the crop's own midpoint.
    p_lo, p_hi = np.percentile(macro, [3.0, 97.0])
    spread = max(p_hi - p_lo, 1e-4)
    gain = min(2.0, max(1.0, 0.55 / spread))
    mid = 0.5 * (p_hi + p_lo)
    macro = np.clip(mid + (macro - mid) * gain, 0.02, 0.98)

    # Base tone field in continuous palette units.
    tone = macro * (PIX_TONES - 1)

    # Soft large-scale undulation so plains aren't one flat tone —
    # smooth, not noisy.
    tone += 1.1 * (fbm(shape, 3, 56, 0.5, rng) - 0.5)

    # Invented craters REMOVED (user decision 2026-08-13): every form
    # in the output now comes from the real crop's big shapes. The
    # stylization is purely tonal.

    # Ordered 2x2 dither at tone boundaries — classic pixel-art
    # blending, keeps gradients smooth without extra palette entries.
    bayer = np.array([[0.0, 0.5], [0.75, 0.25]], dtype=np.float32) - 0.375
    dither = np.tile(bayer, (pix_res // 2 + 1, pix_res // 2 + 1))
    tone += 0.30 * dither[:pix_res, :pix_res]

    idx = np.clip(np.round(tone), 0, PIX_TONES - 1).astype(np.int32)
    return idx


def style_pixelart_v2(idx: np.ndarray) -> Image.Image:
    pal = np.array(pixel_palette(), dtype=np.uint8)
    rgb = pal[idx]
    return Image.fromarray(rgb).resize((DISPLAY_RES, DISPLAY_RES),
                                       Image.NEAREST)


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
    This is multi_zoom's panel 4 (square-in-km window)."""
    return crop_equirect_region(wac, lat_deg, lon_deg,
                                lat_span=span_deg,
                                lon_span=lon_span_for(lat_deg, span_deg),
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

def pixelart_site(wac, lat, lon):
    """The chosen style: stylized pixel art, smooth and attractive
    over precise (user decision 2026-08-13; the photo-real path above
    is kept for reference only)."""
    return style_pixelart_v2(synthesize_pixelart(wac, lat, lon))


def render_compare(wac, lat, lon, name):
    lum, _ = synthesize_site(wac, lat, lon)
    panels = [real_blurry(wac, lat, lon),
              pixelart_site(wac, lat, lon),
              style_realistic(lum)]
    captions = ["Real WAC (today: blurry)",
                "Stylized pixel art (chosen direction)",
                "Photo-real synthesis (rejected, for reference)"]
    compose_strip(panels, captions,
                  f"Site synthesis — {name} ({lat:+.1f}, {lon:+.1f}), "
                  f"~90 km window. Real pixels guide the big shapes; "
                  f"the style is game pixel art.",
                  os.path.join(OUT, "site_synthesis_compare.png"))


def render_drilldown(wac, lat, lon, site_fn=pixelart_site):
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

    panels.append(site_fn(wac, lat, lon).convert("RGBA"))
    captions.append("Site (~90 km) — SYNTHESIZED PIXEL ART")

    compose_strip(panels, captions,
                  "Drill-down, Copernicus. Panels 1-3 real WAC; panel 4 "
                  "generated: real macro + procedural detail, seeded by "
                  "location.",
                  os.path.join(OUT, "site_synthesis_drilldown.png"))


def render_locations(wac, site_fn=pixelart_site):
    """Three terrain types, real vs synthesized — the conditioning must
    visibly adapt (mare smooth/sparse, highland rough/dense)."""
    sites = [("Copernicus (crater)", 9.6, -20.0),
             ("Mare Imbrium (mare)", 32.8, -15.6),
             ("Tycho highlands", -43.3, -11.4)]
    half = DISPLAY_RES // 2
    panels = []
    captions = []
    for name, lat, lon in sites:
        real = real_blurry(wac, lat, lon).resize((half, half), Image.LANCZOS)
        synth = site_fn(wac, lat, lon).resize((half, half), Image.NEAREST)
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


def render_deep_zoom(wac, sites, panel=620,
                     output_name="site_synthesis_deepzoom.png"):
    """For each site: regional real (~300 km) -> site (~90 km) ->
    local (~30 km) -> close (~10 km), the last three photo-real
    amplified, each deeper panel the centre third of the previous.
    One row per site."""
    captions = [f"Regional ({window_km(0, 10):.0f} km) — real",
                f"Site ({window_km(0, 3):.1f} km) — amplified",
                f"Local ({window_km(0, 1):.2f} km) — amplified",
                f"Close ({window_km(0, 1 / 3):.2f} km, "
                f"{window_km(0, 1 / 3) * 1000 / SYNTH_RES:.0f} m/px)"
                " — amplified"]
    n_cols = len(captions)
    pad = 10
    title_h = 60
    cap_h = 26
    name_h = 30
    row_h = name_h + panel + cap_h
    total_w = n_cols * panel + (n_cols - 1) * pad
    total_h = title_h + len(sites) * (row_h + pad)
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    font_title, font_cap = _fonts()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18),
              "Deep zoom, photo-real — two levels below the site "
              "window. Each panel is the centre third of the previous; "
              "no invented craters, real forms amplified all the way "
              "down.",
              fill=(220, 220, 220), font=font_title)

    for row, (name, lat, lon) in enumerate(sites):
        y0 = title_h + row * (row_h + pad)
        draw.text((8, y0 + 4), f"{name}  ({lat:+.1f}, {lon:+.1f})",
                  fill=(255, 200, 100), font=font_cap)
        y = y0 + name_h

        levels = synthesize_site_chain(wac, lat, lon, extra_levels=2)
        panels = []
        # Regional real context panel
        reg = crop_equirect_region(wac, lat, lon, lat_span=10,
                                   lon_span=lon_span_for(lat, 10.0),
                                   output_w=panel, output_h=panel)
        panels.append(reg.convert("RGB"))
        for lum in levels:
            img = Image.fromarray(apply_ramp(lum))
            panels.append(img.resize((panel, panel), Image.LANCZOS))

        for i, p in enumerate(panels):
            x = i * (panel + pad)
            p = p.convert("RGB")
            # Centre-third zoom box on every panel that has a deeper one
            if i < n_cols - 1:
                pd = ImageDraw.Draw(p)
                frac = 0.30 if i == 0 else (1.0 / 3.0)
                b = panel * frac / 2
                pd.rectangle([panel / 2 - b, panel / 2 - b,
                              panel / 2 + b, panel / 2 + b],
                             outline=(255, 200, 100), width=3)
            canvas.paste(p, (x, y))
            if row == len(sites) - 1:
                cw = draw.textlength(captions[i], font=font_cap)
                draw.text((x + (panel - cw) // 2, y + panel + 4),
                          captions[i], fill=(230, 230, 230), font=font_cap)

    path = os.path.join(OUT, output_name)
    canvas.save(path)
    print(f"  wrote {path}")


def render_random_locations(wac, n_sites=6, panel=440, seed=20260813):
    """n_sites random near-side locations, each rendered three ways:
    real (blurry), photo-real amplified, stylized pixel art — all with
    NO invented craters. Grid: columns = sites, rows = styles."""
    rng = np.random.default_rng(seed)
    sites = []
    while len(sites) < n_sites:
        lat = float(rng.uniform(-55.0, 55.0))
        lon = float(rng.uniform(-85.0, 85.0))
        sites.append((lat, lon))

    rows = []
    row_labels = ["Real WAC (blurry)",
                  "Amplified — photo-real",
                  "Amplified — pixel art"]
    real_row = []
    photo_row = []
    pixel_row = []
    for lat, lon in sites:
        real_row.append(real_blurry(wac, lat, lon).resize((panel, panel),
                                                          Image.LANCZOS))
        lum, _ = synthesize_site(wac, lat, lon)
        photo_row.append(style_realistic(lum).resize((panel, panel),
                                                     Image.LANCZOS))
        pixel_row.append(pixelart_site(wac, lat, lon).resize(
            (panel, panel), Image.NEAREST))
    rows = [real_row, photo_row, pixel_row]

    pad = 8
    title_h = 60
    cap_h = 26
    label_w = 46
    total_w = label_w + n_sites * panel + (n_sites - 1) * pad
    total_h = title_h + 3 * (panel + pad) + cap_h
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    font_title, font_cap = _fonts()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18),
              "Random locations, no invented craters — every form is "
              "real. Rows: real / photo-real amplified / pixel art.",
              fill=(220, 220, 220), font=font_title)
    for r, (row, label) in enumerate(zip(rows, row_labels)):
        y = title_h + r * (panel + pad)
        # Vertical row label
        lbl = Image.new("RGB", (panel, label_w), (10, 12, 18))
        ld = ImageDraw.Draw(lbl)
        ld.text((8, 12), label, fill=(200, 200, 210), font=font_cap)
        canvas.paste(lbl.rotate(90, expand=True), (0, y))
        for i, p in enumerate(row):
            x = label_w + i * (panel + pad)
            canvas.paste(p.convert("RGB"), (x, y))
    # Site captions under the last row
    y = title_h + 3 * (panel + pad) - pad + 4
    for i, (lat, lon) in enumerate(sites):
        cap = f"({lat:+.1f}, {lon:+.1f})"
        x = label_w + i * (panel + pad)
        cw = draw.textlength(cap, font=font_cap)
        draw.text((x + (panel - cw) // 2, y), cap,
                  fill=(230, 230, 230), font=font_cap)
    path = os.path.join(OUT, "site_synthesis_random.png")
    canvas.save(path)
    print(f"  wrote {path}")
    return sites


def check_determinism(wac, lat=9.6, lon=-20.0):
    a = synthesize_pixelart(wac, lat, lon)
    b = synthesize_pixelart(wac, lat, lon)
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
