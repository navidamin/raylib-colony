/* THE ASSEMBLER — first component drawn with the iso layer.
   Subject: the top-left tile of the reference factory sheet. Style: Dark
   Plating (../STYLE.md), so it comes out cooler and harder-edged than the
   reference — this engine has one world and the sheet is not it.

   The whole machine is four stacked volumes on one 1x1 world footprint, plus
   a doorway cut through the front. Nothing here is hand-projected: every
   greeble is placed in the face's own coordinates.                          */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};
const { clamp, lerp } = GFX.core;
const ISO = GFX.iso, PT = GFX.parts, MAT = GFX.mat, L = GFX.line, PAL = GFX.palette;

/* ---------------- the breakdown ----------------
   plinth   a dark oversized slab: the machine is BOLTED DOWN, not floating
   frame    the amber housing band, and the doorway is cut through it
   hood     the plated steel upper body, chamfered, where the light lands
   cap      a dark raised access panel inset on the hood's top face          */
const G = {
    plinth : { x0:-0.055, y0:-0.055, z0: 0.000, x1: 1.055, y1: 1.055, z1: 0.130 },
    frame  : { x0: 0.000, y0: 0.000, z0: 0.130, x1: 1.000, y1: 1.000, z1: 0.470 },
    hood   : { x0: 0.000, y0: 0.000, z0: 0.470, x1: 1.000, y1: 1.000, z1: 0.838 },
    cap    : { x0: 0.196, y0: 0.196, z0: 0.838, x1: 0.822, y1: 0.822, z1: 0.902 },
    /* the doorway, in the frame's own south-face coordinates */
    door   : { x: 0.200, y: 0.048, w: 0.615, h: 0.292, r: 0.110, reveal: 0.022, depth: 0.330 },
};

function draw(ctx, V, st) {
    const s = st || {};
    const work  = clamp(s.work  || 0, 0, 1);
    const heat  = clamp(s.heat  || 0, 0, 1);
    const t     = s.t || 0;

    /* Detail budget (V.lod: 1 at hero scale). The icon strip is what forced
       this: at 48 px the studs, louvres and the press in the throat all landed
       on top of each other and the machine turned to mud, while the outline —
       a px constant — ate the amber band. An icon is not a small picture of a
       hero asset; it is a different drawing of the same object. Two thresholds
       are enough. */
    const fine = V.lod >= 0.50;          /* studs, louvres, the press */
    const mid  = V.lod >= 0.18;          /* rails, plinth notches, cap window */

    /* §4.5 heat is a FIELD around the working point, not a global multiplier.
       The throat is where the work happens, so the glow climbs from there and
       is nearly gone by the cap. Feeding one flat scalar to every material —
       the first pass did — turns the whole machine tan, which is CG lighting
       wearing a stepped-tone costume. */
    const heatAt = z => heat * Math.exp(-((z - 0.26) * (z - 0.26)) / (2 * 0.34 * 0.34));
    const hFrame = heatAt(0.29) * 0.55;
    const hHood  = heatAt(0.65) * 0.40;
    const hCap   = heatAt(0.87) * 0.30;

    /* ---- 1. plinth ---- */
    plinth(ctx, V, false);
    /* One recessed strip per visible face. The plinth stops being a brick and
       becomes two corner feet with a gap between them — subtraction, not four
       more boxes. */
    if (mid) {
        PT.panel(ctx, V, G.plinth, "south", 0.315, 0.028, 0.470, 0.074,
                 { mat: "dark", shade: 0.17, lift: -0.014, outW: 1.0 });
        PT.panel(ctx, V, G.plinth, "east",  0.315, 0.028, 0.470, 0.074,
                 { mat: "dark", shade: 0.08, lift: -0.014, outW: 1.0 });
    }

    /* ---- 2. amber frame band ----
       No top face and no chamfer: the hood sits directly on it, and a line
       under a part that is covered would print through the piece above. */
    ISO.solid(ctx, V, G.frame, {
        mat: "amber", heat: hFrame, bands: 3, spread: 0.038, skip: ["top", "chamfer"],
        shades: { south: 0.60, east: 0.30 },
    });

    /* ---- 3. the doorway ----
       Cut through the frame's south face. The reveal is the amber plate's own
       thickness, painted in plating steel: the frame is a skin over a steel
       machine, and a cut is where you find that out.                        */
    const d = G.door;
    PT.recess(ctx, V, G.frame, "south",
        PT.archPath(d.x, d.y, d.w, d.h, d.r, 0),
        PT.archPath(d.x, d.y, d.w, d.h, d.r, d.reveal),
        { depth: d.depth, mat: "dark", revealMat: "plating", heat: 0,
          reveal0: 0.60, reveal1: 0.30, reveal2: 0.12,
          floor: 0.20, floorMat: "plating", wall: 0.14, dark: 0.03, back: 0.01,
          glow: work * 0.48, glowR: 0.28, glowY: 0.86 });

    /* the work in the throat: a press that strokes as the cycle runs */
    if (fine && work > 0.02) throatWork(ctx, V, work, t, heat);

    /* ---- 3b. the plinth's near lip, repainted ----
       The plinth overhangs the body on the front two sides, so its front faces
       are NEARER the camera than the doorway is — and the doorway is a hole,
       which means anything painted before it shows through. Painter order is
       not "bottom to top" for an overhanging base; it is back to front, and
       the base is both. So the deck goes down first and the lip comes back
       over the cut afterwards. */
    plinth(ctx, V, true);

    /* ---- 4. frame furniture ----
       Two actuator rails on the door posts. They are the reason the frame
       reads as a machine's front and not a painted stripe. */
    const slide = 0.5 + 0.5 * Math.sin(t * 2.1) * work;
    if (mid) {
        PT.rail(ctx, V, G.frame, "south", 0.088, 0.085, 0.042, 0.198, { t: slide, heat: hFrame });
        PT.rail(ctx, V, G.frame, "south", 0.872, 0.085, 0.042, 0.198, { t: 1 - slide, heat: hFrame });
        PT.rail(ctx, V, G.frame, "east",  0.108, 0.095, 0.040, 0.182, { t: slide, heat: hFrame, channelShade: 0.14 });
    }

    /* ---- 5. plated hood ---- */
    ISO.solid(ctx, V, G.hood, {
        mat: "plating", heat: hHood, chamfer: 0.055, bands: 3, spread: 0.030, faceBands: { top: 1 },
        shades: { top: 0.74, south: 0.62, east: 0.26 },
    });
    /* the seam where hood meets frame — an under-edge line grounds the plate */
    seam(ctx, V);

    if (mid) PT.louvres(ctx, V, G.hood, "east", 0.215, 0.135, 0.500, 0.112, 3, { heat: hHood, shade: 0.10 });
    if (fine) {
        PT.studs(ctx, V, G.hood, "south",
                 [[0.055, 0.080], [0.055, 0.300], [0.945, 0.080], [0.945, 0.300]],
                 { r: 0.019, shade: 0.62, heat: hHood });
        PT.studs(ctx, V, G.hood, "east", [[0.062, 0.080], [0.062, 0.300]],
                 { r: 0.018, shade: 0.40, heat: hHood });
    }

    /* status lamp: cyan idle, amber driven, hot over-driven (§6.3) */
    const lampCol = heat > 0.72 ? PAL.hot : (work > 0.15 ? PAL.amLit : PAL.cy);
    const pulse = 0.55 + 0.45 * Math.sin(t * 4.4);
    PT.lamp(ctx, V, G.hood, "south", 0.500, 0.298, 0.021, lampCol,
            { bloom: (0.35 + 0.5 * work) * pulse,
              bloomCol: heat > 0.72 ? "rgba(255,120,50,.5)" : (work > 0.15 ? "rgba(255,200,90,.45)" : "rgba(120,230,255,.40)") });

    /* ---- 6. access cap ---- */
    ISO.solid(ctx, V, G.cap, {
        mat: "dark", heat: hCap, chamfer: 0.018, bands: 2, spread: 0.04, faceBands: { top: 1 },
        shades: { top: 0.60, south: 0.38, east: 0.18 },
        rims:   { north: 0.74, west: 0.78, south: 0.66, east: 0.30 },
    });
    /* a recessed window in the cap, so the top face is not a dead lid */
    if (mid) PT.panel(ctx, V, G.cap, "top", 0.098, 0.098, 0.430, 0.430,
             { mat: "dark", shade: 0.28, lift: -0.006, outW: 1.2 });
    if (fine) PT.studs(ctx, V, G.cap, "top",
             [[0.052, 0.052], [0.574, 0.052], [0.052, 0.574], [0.574, 0.574]],
             { r: 0.017, shade: 0.52, mat: "plating" });

    /* ---- 7. heat atmosphere (§4.5) ---- */
    if (heat > 0.45) {
        const c = V.p(0.5, 1.0, 0.30), rad = V.scale * 0.95;
        const g = ctx.createRadialGradient(c[0], c[1], 0, c[0], c[1], rad);
        g.addColorStop(0, `rgba(255,110,30,${0.30 * (heat - 0.45) / 0.55})`);
        g.addColorStop(1, "rgba(255,110,30,0)");
        ctx.fillStyle = g; ctx.fillRect(c[0] - rad, c[1] - rad, rad * 2, rad * 2);
    }
}

/* The plinth, drawn twice: once whole, then once as the near lip only. `lip`
   skips the deck (which would cover the frame standing on it) and repaints
   just the overhanging band outside the body's footprint. */
function plinth(ctx, V, lip) {
    const b = G.plinth, c = 0.026;
    const opt = { mat: "dark", chamfer: c, bands: 2, spread: 0.045, faceBands: { top: 1 },
                  shades: { top: 0.46, south: 0.34, east: 0.14 },
                  rims:   { north: 0.60, west: 0.64, south: 0.52, east: 0.24 } };
    if (lip) { opt.skip = ["top"]; opt.chamferKeys = ["south", "east"]; }
    ISO.solid(ctx, V, b, opt);
    if (!lip) return;
    /* the strip of deck that sticks out past the body, front and right */
    const z = b.z1, deck = MAT.tone("dark", 0.46, 0);
    L.fill(ctx, [V.p(b.x0 + c, 1, z), V.p(b.x1 - c, 1, z),
                 V.p(b.x1 - c, b.y1 - c, z), V.p(b.x0 + c, b.y1 - c, z)], deck);
    L.fill(ctx, [V.p(1, b.y0 + c, z), V.p(b.x1 - c, b.y0 + c, z),
                 V.p(b.x1 - c, 1, z), V.p(1, 1, z)], deck);
    /* where the body meets the deck: a hard line, so the machine is bolted
       down rather than resting in a puddle of its own colour */
    L.rule(ctx, V.p(0, 1, z), V.p(1, 1, z), "rgba(0,0,0,.6)", V.lw(2.0));
    L.rule(ctx, V.p(1, 1, z), V.p(1, 0, z), "rgba(0,0,0,.6)", V.lw(2.0));
}

/* the seam: one dark rule along the top of the amber band, one bright rule
   under the hood's bottom edge. Two lines, and the two volumes stop being one
   extruded block. */
function seam(ctx, V) {
    const z = G.hood.z0;
    L.rule(ctx, V.p(0, 1, z), V.p(1, 1, z), "rgba(0,0,0,.55)", V.lw(2.0));
    L.rule(ctx, V.p(1, 1, z), V.p(1, 0, z), "rgba(0,0,0,.55)", V.lw(2.0));
    L.rule(ctx, V.p(0, 1, z + 0.012), V.p(1, 1, z + 0.012), "rgba(255,255,255,.13)", V.lw(1.4));
}

/* what the machine is doing, seen through the doorway: a platen that steps up
   through the cycle and glows at the top of its stroke. Drawn INSIDE the
   recess volume so the doorway's own geometry occludes it. */
function throatWork(ctx, V, work, t, heat) {
    const d = G.door, m = V.on("south", G.frame);
    const cyc = (t * 0.55) % 1;
    const stroke = 0.5 - 0.5 * Math.cos(cyc * Math.PI * 2);   // 0 up, 1 pressed

    /* Pushing into a south face by p moves the point RIGHT and UP on screen by
       -p*ay. Laying the interior out at the mouth's own coordinates therefore
       puts it that far off centre — under the arch's right shoulder, where it
       gets clipped. Undo the projection's own shift, in the face's units. */
    const push = -0.20, drift = push * V.ay[0] / V.ax[0];
    const bx = d.x + 0.072 - drift, bw = d.w - 0.144;

    /* Everything stacks off the sill, so nothing is ever drawn below the floor.
       The ceiling is not the arch's top: at depth `push` the whole interior
       rides up by m.vShift, so the usable band starts that far down. Ignoring
       it is what silently clipped the ram out of the frame. */
    const sillV = d.y + d.h, ceilV = d.y + m.vShift(push) + 0.020;
    const bedH = 0.040, bedV = sillV - bedH;
    const wpH  = 0.046, wpV  = bedV - wpH;
    const ramTop = ceilV;
    const ramBot = lerp(ramTop + 0.040, wpV - 0.004, stroke);

    ctx.save();
    L.trace(ctx, m.pts(PT.archPath(d.x, d.y, d.w, d.h, d.r, d.reveal))); ctx.clip();
    /* Three parts, no more. Anything inside a throat this size reads as a
       silhouette against the dark, so a fourth shape is not detail, it is
       noise: bed, work, ram — and you can tell what the machine does.       */
    const bed = m.pts(PT.rectPath(bx - 0.034, bedV, bw + 0.068, bedH), push);
    L.fill(ctx, bed, MAT.tone("plating", 0.28, 0));
    L.rule(ctx, bed[0], bed[1], "rgba(255,255,255,.22)", V.lw(1.6));
    L.stroke(ctx, bed, PAL.OUT, V.lw(1.3), true);
    /* the workpiece: hottest at the bottom of the stroke, because that is when
       it is being worked. State reaches it through the ember ladder, not a
       tint laid over the top (§3.3). */
    const wp = m.pts(PT.rectPath(bx + bw*0.20, wpV, bw*0.60, wpH), push + 0.02);
    L.fill(ctx, wp, MAT.tone("ember", 0.20 + 0.55 * stroke * work, 0));
    L.rule(ctx, wp[0], wp[1], `rgba(255,225,175,${0.18 + 0.46 * stroke})`, V.lw(1.5));
    L.stroke(ctx, wp, PAL.OUT, V.lw(1.3), true);
    /* the ram: a plunger that extends out of the roof and lands on the work */
    const ram = m.pts(PT.rectPath(bx + bw*0.34, ramTop, bw*0.32, ramBot - ramTop), push + 0.03);
    L.fill(ctx, ram, MAT.tone("plating", 0.36, heat * 0.35));
    L.rule(ctx, ram[3], ram[2], "rgba(0,0,0,.55)", V.lw(1.8));
    L.rule(ctx, ram[0], ram[1], "rgba(255,255,255,.20)", V.lw(1.4));
    L.stroke(ctx, ram, PAL.OUT, V.lw(1.3), true);
    ctx.restore();
}

GFX.assembler = { draw, G };
})(typeof window !== "undefined" ? window : globalThis);
