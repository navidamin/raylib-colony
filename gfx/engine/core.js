/* Dark Plating — core
   Numbers, tone, determinism. Everything else in the engine sits on this.

   Reference for the rules quoted here: docs/design/graphics/dark-plating.md
   Extracted from docs/design/subsurface/prototypes/drill-rig.html, where all
   of it lived as file-scope globals bound to one canvas.                    */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};

/* ---------------- numbers ---------------- */
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const lerp  = (a, b, t) => a + (b - a) * t;
const inv   = (a, b, v) => (b === a ? 0 : clamp((v - a) / (b - a), 0, 1));

/* ---------------- §3.1 tone is stepped, never smooth ----------------
   Every computed shade passes through a ladder before it becomes a colour.
   Surfaces get 7 steps; highlights and glints are deliberately coarser, so a
   glint never reads as a soft ramp.                                        */
const quant = (v, steps) => Math.round(clamp(v, 0, 1) * steps) / steps;
const band  = v => quant(v, 7);
const glint = v => quant(v, 3);

/* §3.2 back-facing work is REMAPPED, not multiplied. Compressing the range
   keeps the far side legible but unmistakably behind, and stops it glowing
   white-hot when a state tint pushes every shade up. */
const dim = (v, front) => (front ? v : 0.15 + v * 0.44);

/* ---------------- §5 determinism ----------------
   The drill kept one module-global `grainSeed` and reset it by hand before
   each pass (sky 3, strata 7, borehole 29) — forgetting a reset made the
   ground shimmer between frames. A Rng is a closure instead, so a pass owns
   its stream and cannot disturb anyone else's.
   Math.random() is only ever for genuinely transient particles.            */
function Rng(seed) {
    let s = (seed | 0) || 1;
    const next = () => (s = (s * 16807) % 2147483647) / 2147483647;
    next.range  = (a, b) => a + next() * (b - a);
    next.int    = n => Math.floor(next() * n);
    next.pick   = arr => arr[Math.floor(next() * arr.length)];
    next.sign   = () => (next() < 0.5 ? -1 : 1);
    next.reseed = k => { s = (k | 0) || 1; };
    return next;
}

/* ---------------- colour ---------------- */
function hexToRgb(h) {
    const v = parseInt(h.slice(1), 16);
    return h.length === 4
        ? [((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17]
        : [(v >> 16) & 255, (v >> 8) & 255, v & 255];
}
const rgbToHex = c =>
    "#" + c.map(v => clamp(Math.round(v), 0, 255).toString(16).padStart(2, "0")).join("");
const rgbStr   = c => `rgb(${c[0] | 0},${c[1] | 0},${c[2] | 0})`;
const mixRgb   = (a, b, t) => [lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)];
const alpha    = (c, a) => `rgba(${c[0] | 0},${c[1] | 0},${c[2] | 0},${a})`;

GFX.core = { clamp, lerp, inv, quant, band, glint, dim, Rng,
             hexToRgb, rgbToHex, rgbStr, mixRgb, alpha };
})(typeof window !== "undefined" ? window : globalThis);
