/* Dark Plating — palette (§1.2)
   The world is single-theme by choice: nothing inherits from a host theme,
   every colour is painted explicitly. Semantics are load-bearing —
   amber = machine/attention, cyan = instrument/information, hot = heat/damage.
   Do not reuse a semantic colour decoratively.                             */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};

GFX.palette = {
    /* chrome */
    ground : "#070b11",  panel  : "#0d151e",  panel2 : "#111c27",  rule : "#1c2a39",
    text   : "#c9d8e8",  dim    : "#61768a",  dimmer : "#3d4e5e",

    /* semantic */
    am     : "#d9962f",  amLit  : "#f4c66a",
    cy     : "#50e1ff",
    hot    : "#ff5a28",
    good   : "#5fd39a",
    ice    : "#7fd8ee",

    /* §2 the line — style constants, never a per-shape choice */
    OUT    : "#0a0e14",  // the chunky outline, front work
    OUTB   : "#101820",  // softer outline behind back-facing work
    OUT_W  : 2.2,        // at 2x canvas scale
};
})(typeof window !== "undefined" ? window : globalThis);
