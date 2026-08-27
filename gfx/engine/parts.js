/* Dark Plating — machine furniture
   The grammar of parts that says "this is an assembly, not a lump" (§4.3),
   expressed in FACE-LOCAL coordinates so the same call works on any face of
   any solid. Every one of these is laid out as if the face were a flat UI
   rectangle; iso.js does the projecting.                                    */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};
const { clamp, lerp, band } = GFX.core;
const L = GFX.line, MAT = GFX.mat, ISO = GFX.iso, PAL = GFX.palette;

/* ---------------- local paths ----------------
   Shapes are generated, never digitised, so the same generator can produce a
   shape and its inset twin — which is how a cut gets a REVEAL (the thickness
   of the plate it is cut through) without polygon offsetting.               */
function rectPath(x, y, w, h) { return [[x,y],[x+w,y],[x+w,y+h],[x,y+h]]; }

/* a doorway: square-shouldered below, round-shouldered above. Two quarter-arcs
   and a flat top, NOT a half-ellipse — the flat between the shoulders is what
   makes it read as a machine's throat rather than a tunnel mouth.
   `inset` shrinks the whole shape evenly, which is how the same generator
   produces a cut and the reveal inside it.                                  */
function archPath(x, y, w, h, r, inset, segs) {
    const i = inset || 0, n = segs || 8;
    x += i; y += i; w -= i*2; h -= i*2;
    r = Math.max(0, Math.min(r - i, w/2, h));
    if (r < 0.001) return rectPath(x, y, w, h);
    const pts = [[x, y + h]];
    for (let k = 0; k <= n; k++) {                           // left shoulder, 180 -> 90
        const a = Math.PI * (1 - 0.5 * k / n);
        pts.push([x + r + Math.cos(a) * r, y + r - Math.sin(a) * r]);
    }
    for (let k = 0; k <= n; k++) {                           // right shoulder, 90 -> 0
        const a = Math.PI * 0.5 * (1 - k / n);
        pts.push([x + w - r + Math.cos(a) * r, y + r - Math.sin(a) * r]);
    }
    pts.push([x + w, y + h]);
    return pts;
}

/* ---------------- the recess ----------------
   A hole with thickness and a room behind it. Three surfaces do all the work:

     reveal   the plate's own cut edge, bright on the lit side  — says the
              wall has thickness, which is what separates a doorway from a
              black sticker;
     tube     the inner walls, banded from the mouth inward     — the floor
              near the mouth catches light, the roof does not;
     back     a flat plane at `depth`, the darkest tone in the piece.

   The back plane is drawn at the projected offset, so it lands up-and-right of
   the mouth on a south face — and the sliver of tube left visible along the
   bottom and left IS the floor and the near side wall. The geometry produces
   them; they are not drawn as separate shapes.                              */
function recess(ctx, V, b, faceName, path, inner, opt) {
    const o = opt || {}, m = V.on(faceName, b);
    const depth = o.depth !== undefined ? o.depth : 0.3;
    const mat = o.mat || "dark", heat = o.heat || 0;
    const outer = m.pts(path), inPts = m.pts(inner || path);

    /* 1. reveal ring — lit across the top-left shoulder and falling away to
       nothing on the right. A reveal of even brightness all the way round is
       the tell that it was drawn as an outline instead of a surface: the far
       side of a cut edge does not face the light. */
    if (inner) {
        const g = MAT.bandsAlong(ctx, outer[1], outer[outer.length - 1],
            [[0.34, o.reveal0 !== undefined ? o.reveal0 : 0.58],
             [0.33, o.reveal1 !== undefined ? o.reveal1 : 0.30],
             [0.33, o.reveal2 !== undefined ? o.reveal2 : 0.13]],
            { mat: o.revealMat || "plating", heat });
        L.fill(ctx, outer, g, inPts);
    }
    /* 2. the cavity, clipped to the mouth.
       The first pass filled it with one dark gradient, and everything placed
       inside then floated in soup — a hole, not a room. A recess needs its
       surfaces named: the camera looks down and from the left, so of the six
       inner faces exactly three face it — the BACK, the FLOOR, and the inner
       side wall on the LEFT. The other three are turned away and are simply
       the darkest tone in the piece.

       The path must start at its bottom-left and end at its bottom-right (both
       generators here do), so its first and last points give the sill and its
       first two give the left jamb, and the floor and wall fall out of the
       geometry instead of being drawn as separate art.                       */
    const q = inner || path, n = q.length;
    ctx.save();
    L.trace(ctx, inPts); ctx.clip();
    L.fill(ctx, inPts, MAT.tone(mat, o.dark !== undefined ? o.dark : 0.04, heat));
    const back = q.map(v => m(v[0], v[1], -depth));
    L.fill(ctx, back, MAT.tone(mat, o.back !== undefined ? o.back : 0.02, heat));
    /* floor: sill edge -> back, banded so it darkens with depth */
    const sill = [m(q[0][0], q[0][1]), m(q[n-1][0], q[n-1][1]),
                  m(q[n-1][0], q[n-1][1], -depth), m(q[0][0], q[0][1], -depth)];
    /* The trailing bands are RELATIVE to the mouth shade. Hardcoding them made
       a dark floor get lighter with depth — a lit back wall in a machine with
       no light in it, and the tell was a pale wedge in an idle throat. */
    const fl = o.floor !== undefined ? o.floor : 0.30;
    L.fill(ctx, sill, MAT.bandsAlong(ctx, sill[0], sill[3],
        [[0.34, fl], [0.33, fl * 0.62], [0.33, fl * 0.34]],
        { mat: o.floorMat || mat, heat }));
    /* left jamb wall */
    const jamb = [m(q[1][0], q[1][1]), m(q[0][0], q[0][1]),
                  m(q[0][0], q[0][1], -depth), m(q[1][0], q[1][1], -depth)];
    const wl = o.wall !== undefined ? o.wall : 0.16;
    L.fill(ctx, jamb, MAT.bandsAlong(ctx, jamb[0], jamb[3],
        [[0.5, wl], [0.5, wl * 0.5]], { mat, heat }));
    L.rule(ctx, sill[0], sill[3], "rgba(255,255,255,.10)", V.lw(1.3));
    /* an interior light source is atmosphere, the one overlay the style allows */
    if (o.glow > 0.001) {
        const c = m(m.w * 0.5, m.h * (o.glowY || 0.80), -depth * 0.45), rad = V.scale * (o.glowR || 0.42);
        const gr = ctx.createRadialGradient(c[0], c[1], 0, c[0], c[1], rad);
        gr.addColorStop(0, `rgba(255,150,48,${0.40 * o.glow})`);
        gr.addColorStop(0.55, `rgba(255,96,26,${0.13 * o.glow})`);
        gr.addColorStop(1, "rgba(255,80,20,0)");
        ctx.fillStyle = gr; ctx.fillRect(c[0] - rad, c[1] - rad, rad * 2, rad * 2);
    }
    ctx.restore();
    /* 4. the mouth keeps the heavy line — a cut is a silhouette too (§2) */
    L.stroke(ctx, outer, o.out || PAL.OUT, o.outW || V.lw(PAL.OUT_W * 0.85), true);
    return { outer, inPts, back };
}

/* ---------------- an inset or raised plate on a face ----------------
   `lift` > 0 stands it proud, < 0 sinks it. Raised plates get the §4.3 bevel:
   white top, white left, black bottom, black right. */
function panel(ctx, V, b, faceName, x, y, w, h, opt) {
    const o = opt || {}, m = V.on(faceName, b), lift = o.lift || 0;
    const pts = m.pts(rectPath(x, y, w, h), lift);
    if (o.out !== false) L.flood(ctx, [pts], { color: o.out || PAL.OUT, w: V.lw(o.outW || PAL.OUT_W * 0.7) });
    L.fill(ctx, pts, MAT.tone(o.mat || "dark", o.shade !== undefined ? o.shade : 0.30, o.heat || 0));
    if (o.bevel !== false) {
        L.rule(ctx, pts[0], pts[1], lift >= 0 ? "rgba(255,255,255,.26)" : "rgba(0,0,0,.42)", V.lw(o.bevelW || 1.8));
        L.rule(ctx, pts[3], pts[2], lift >= 0 ? "rgba(0,0,0,.40)" : "rgba(255,255,255,.16)", V.lw(o.bevelW || 1.8));
    }
    return pts;
}

/* ---------------- bolts ----------------
   Small, bright, and always in fours or pairs: a bolt pattern is what tells
   the eye a plate is FASTENED to the thing behind it. */
function studs(ctx, V, b, faceName, list, opt) {
    const o = opt || {}, m = V.on(faceName, b), r = o.r || 0.035;
    for (const [x, y] of list) {
        const q = m.pts(rectPath(x - r, y - r, r*2, r*2), o.lift || 0.004);
        L.fill(ctx, q, PAL.OUT);
        const s = m.pts(rectPath(x - r*0.62, y - r*0.62, r*1.24, r*1.24), o.lift || 0.004);
        L.fill(ctx, s, o.col || MAT.tone(o.mat || "plating", o.shade !== undefined ? o.shade : 0.80, o.heat || 0));
    }
}

/* ---------------- vents ----------------
   Dark slots with a 1.6 px inner shadow line under each (§4.3). */
function louvres(ctx, V, b, faceName, x, y, w, h, n, opt) {
    const o = opt || {}, m = V.on(faceName, b), gap = h / n;
    for (let i = 0; i < n; i++) {
        const yy = y + i * gap, hh = gap * 0.58;
        L.fill(ctx, m.pts(rectPath(x, yy, w, hh)), MAT.tone(o.mat || "dark", o.shade !== undefined ? o.shade : 0.16, o.heat || 0));
        const a = m(x, yy + hh * 0.62), c = m(x + w, yy + hh * 0.62);
        L.rule(ctx, a, c, "rgba(0,0,0,.5)", V.lw(1.6));
        const d = m(x, yy), e = m(x + w, yy);
        L.rule(ctx, d, e, "rgba(255,255,255,.10)", V.lw(1.2));
    }
}

/* ---------------- a rail / actuator strip ----------------
   The dark inset channel with a bright slider in it. Reads as a moving part
   at any size, which is why the reference sprite puts one on each front post. */
function rail(ctx, V, b, faceName, x, y, w, h, opt) {
    const o = opt || {}, m = V.on(faceName, b), t = clamp(o.t !== undefined ? o.t : 0.35, 0, 1);
    L.fill(ctx, m.pts(rectPath(x, y, w, h)), MAT.tone(o.channel || "dark", o.channelShade !== undefined ? o.channelShade : 0.26, 0));
    L.rule(ctx, m(x, y), m(x + w, y), "rgba(0,0,0,.55)", V.lw(1.5));
    L.rule(ctx, m(x + w, y), m(x + w, y + h), "rgba(255,255,255,.10)", V.lw(1.2));
    const sh = h * 0.17, sy = y + (h - sh) * (1 - t);
    L.fill(ctx, m.pts(rectPath(x - w*0.14, sy, w*1.28, sh)), MAT.tone(o.mat || "plating", 0.62, o.heat || 0));
    L.rule(ctx, m(x - w*0.14, sy), m(x + w*1.14, sy), "rgba(255,255,255,.26)", V.lw(1.3));
}

/* ---------------- status lamp ----------------
   §6.3: the lamp reads machine state semantically — cyan idle, amber driven,
   hot over-driven. It is the one place a semantic colour is allowed to sit on
   a machine, because it is literally information. */
function lamp(ctx, V, b, faceName, x, y, r, col, opt) {
    const o = opt || {}, m = V.on(faceName, b);
    L.fill(ctx, m.pts(rectPath(x - r*1.5, y - r*1.5, r*3, r*3), 0.004), PAL.OUT);
    L.fill(ctx, m.pts(rectPath(x - r, y - r, r*2, r*2), 0.006), col);
    if (o.bloom > 0.001) {
        const c = m(x, y, 0.006), rad = r * V.scale * 3.4;
        const g = ctx.createRadialGradient(c[0], c[1], 0, c[0], c[1], rad);
        g.addColorStop(0, o.bloomCol || "rgba(120,230,255,.42)");
        g.addColorStop(1, "rgba(0,0,0,0)");
        ctx.save(); ctx.globalAlpha = clamp(o.bloom, 0, 1);
        ctx.fillStyle = g; ctx.fillRect(c[0] - rad, c[1] - rad, rad*2, rad*2); ctx.restore();
    }
}

GFX.parts = { rectPath, archPath, recess, panel, studs, louvres, rail, lamp };
})(typeof window !== "undefined" ? window : globalThis);
