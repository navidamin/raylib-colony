/* Dark Plating — the line (§2)
   The single strongest style marker: every silhouette sits on a heavy
   near-black line. Widths are style constants, not per-shape choices — that
   is what makes separate parts read as one machine.                        */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};
const P = GFX.palette;

/* Build a path from a list of screen points. */
function trace(ctx, pts) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.closePath();
}

/* §2 ONE FLOOD PASS, THEN FACES.
   Outline a shape by filling its whole silhouette, slightly inflated, before
   painting anything over it. Stroking each face individually puts black
   ribbing across the interior — the mistake that made the drill's first
   thread pass look welded together.

   Inflation is stroke-then-fill with a round join, so no boolean union is
   needed: pass every polygon of an assembly through here first, and the seams
   between them are painted over by their own faces. Only the outer boundary
   survives, which is the line you wanted.                                  */
function flood(ctx, polys, opt) {
    const o = opt || {};
    ctx.save();
    ctx.fillStyle = ctx.strokeStyle = o.color || P.OUT;
    ctx.lineWidth = (o.w || P.OUT_W) * 2;
    ctx.lineJoin = ctx.lineCap = "round";
    for (const pts of polys) { if (pts.length < 3) continue; trace(ctx, pts); ctx.stroke(); ctx.fill(); }
    ctx.restore();
}

/* Fill one polygon; optionally with a hole (even-odd), which is how a cut —
   an arch, a port, a window — is taken out of a face without redrawing the
   face as three separate pieces. */
function fill(ctx, pts, style, hole) {
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.closePath();
    if (hole && hole.length > 2) {
        ctx.moveTo(hole[0][0], hole[0][1]);
        for (let i = 1; i < hole.length; i++) ctx.lineTo(hole[i][0], hole[i][1]);
        ctx.closePath();
    }
    ctx.fillStyle = style;
    ctx.fill("evenodd");
    ctx.restore();
}

/* An edge rule: the thin bright or dark line that grounds a plate against its
   neighbour. §4.3's housing bevel, reduced to its one primitive — a stroked
   segment in the plate's own plane. Bright above, dark below; never a blur. */
function rule(ctx, a, b, style, w) {
    ctx.save();
    ctx.strokeStyle = style; ctx.lineWidth = w || 1.6; ctx.lineCap = "butt";
    ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
    ctx.restore();
}

/* Stroke an open polyline (arch inner edges, fracture lines, leaders). */
function stroke(ctx, pts, style, w, closed) {
    ctx.save();
    ctx.strokeStyle = style; ctx.lineWidth = w || 1.6;
    ctx.lineJoin = ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    if (closed) ctx.closePath();
    ctx.stroke();
    ctx.restore();
}

GFX.line = { trace, flood, fill, rule, stroke };
})(typeof window !== "undefined" ? window : globalThis);
