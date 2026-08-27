/* Dark Plating — materials (§4)
   Two models live here, and the difference is the whole point:

   1. steel(shade, heat) — the drill's CONTINUOUS ramp, verbatim. Correct for
      TURNED parts, where banding comes from curvature across the part and the
      quantization happens in steelBands' paired gradient stops.

   2. LADDERS — an ordered list of plate tones, dark to bright. A shade picks a
      rung. Correct for FLAT VOLUMES, where there is no curvature to band and
      a two-point RGB lerp cannot hold saturation: lerping #241704 -> #f8d78c
      passes through a desaturated tan, so an amber housing shaded that way
      goes muddy in its midtones. A ladder puts the hue where you want it at
      every rung, and quantization stops being an afterthought — it is the
      data structure. Extends §4.4's "clone steel(), name it, add it to the
      table" to materials a linear ramp cannot express.

   Heat enters through the material itself (§3.3), never as an overlay.      */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};
const { clamp, lerp, quant, hexToRgb, rgbStr, mixRgb } = GFX.core;

/* ---------------- the continuous ramp (turned parts) ---------------- */
function steel(shade, heat) {
    const base = [lerp(96, 238, shade), lerp(104, 244, shade), lerp(118, 252, shade)];
    return rgbStr(applyHeat(base, shade, heat));
}
/* the glow ramp runs black-red -> orange -> near-white as shade rises, so hot
   BRIGHT metal whitens while hot DARK metal stays ember-red. */
function applyHeat(base, shade, heat) {
    if (!heat) return base;
    const glow = [255, lerp(55, 165, shade), 25];
    return mixRgb(base, glow, clamp(heat * 1.15, 0, 1));
}

/* ---------------- the ladders (flat volumes) ---------------- */
const LADDERS = {};
function register(name, hexes) { LADDERS[name] = hexes.map(hexToRgb); return name; }

/* cool blue-biased grey — b runs ahead of r at every rung. That bias is the
   plating's colour identity (§4.1), kept from steel()'s base ramp. The floor
   is darker than steel()'s (96,104,118): a turned rod never shows a face
   fully turned from the light, a box does, and without a real shadow rung
   every volume reads flat. */
register("plating", ["#1b2128","#28313b","#37424e","#495764","#5e6d7c",
                     "#778796","#93a1ae","#b3bfc9","#d9e0e7"]);
/* saturated amber — blue stays near zero through the midtones, which is what
   a linear lerp to a pale highlight cannot do. */
register("amber",   ["#241704","#3d2606","#5b3808","#7d4d09","#a1640b",
                     "#c5800f","#d9962f","#edb551","#f8d78c"]);
/* charcoal — plinths, caps, recessed panels; cool, never neutral. */
register("dark",    ["#080b0f","#0e1319","#151c23","#1d252e","#262f39",
                     "#303b46","#3c4854","#4a5764","#5a6875"]);
/* ember — a lit throat, a furnace mouth, molten work. */
register("ember",   ["#2b0c03","#501705","#7a2408","#a5350a","#cc4a10",
                     "#e8681c","#f78c33","#ffb35e","#ffe0a8"]);
/* volatiles — ice is --ice, never cyan; cyan is information (§5). */
register("ice",     ["#0d2027","#153440","#1d4a59","#276275","#347e93",
                     "#489db3","#68bcd0","#93d5e4","#c2ecf4"]);

/* shade 0..1 -> a rung. The snap IS the style; there is no interpolation
   between rungs on purpose. */
function rung(name, shade) {
    const L = LADDERS[name] || LADDERS.plating;
    return L[Math.round(clamp(shade, 0, 1) * (L.length - 1))];
}
function tone(name, shade, heat) {
    return rgbStr(applyHeat(rung(name, shade), shade, heat || 0));
}

/* ---------------- banded fills ----------------
   steelBands() in the drill could only run along screen x
   (createLinearGradient(x0,0,x1,0)). A face of an iso solid runs along its own
   axis, at whatever angle the projection gives it, so the direction has to be
   a parameter. Paired stops keep every band flat — a plate of tone, not a ramp. */
function bandsAlong(ctx, p0, p1, tones, opt) {
    const o = opt || {}, name = o.mat || "plating", heat = o.heat || 0;
    const g = ctx.createLinearGradient(p0[0], p0[1], p1[0], p1[1]);
    let t = 0;
    for (const [span, shade] of tones) {
        const c = name === "steel" ? steel(shade, heat) : tone(name, shade, heat);
        g.addColorStop(t, c);
        t = Math.min(1, t + span);
        g.addColorStop(t, c);
    }
    return g;
}
/* the drill's five-band cylinder profiles (§4.2), kept verbatim so a rod drawn
   with this engine is the same rod. dark edge -> bright hot-spot band
   off-centre left -> mid -> darker -> dark edge; the off-centre specular is
   the implied upper-left light. */
const PROFILE = {
    rod  : [[0.15,0.11],[0.17,0.98],[0.21,0.58],[0.27,0.30],[0.20,0.07]],
    joint: [[0.15,0.16],[0.18,0.94],[0.22,0.56],[0.26,0.28],[0.19,0.10]],
    chuck: [[0.17,0.06],[0.16,0.62],[0.22,0.34],[0.26,0.18],[0.19,0.04]],
};

GFX.mat = { steel, tone, rung, register, bandsAlong, PROFILE, LADDERS, applyHeat };
})(typeof window !== "undefined" ? window : globalThis);
