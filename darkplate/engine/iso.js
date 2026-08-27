/* Dark Plating — iso solids
   THE EXTENSION. Everything the drill draws is a rect or a swept cylinder in
   screen space; there was no way to say "a plated volume seen from above".
   This adds three things:

     1. a parameterised axonometric projection with named presets,
     2. the CHAMFERED PLATED VOLUME — the box primitive of this style,
     3. FACE-LOCAL COORDINATES: draw any 2D shape (an arch, a vent, a bolt
        pattern, a decal) in a face's own plane and have it project correctly.

   (3) is what makes the volume useful. Without it every greeble has to be
   hand-projected, which is how iso art turns into a pile of magic numbers.  */
(function (root) {
"use strict";
const GFX = root.GFX = root.GFX || {};
const { clamp, lerp, band } = GFX.core;
const L = GFX.line, MAT = GFX.mat, PAL = GFX.palette;

/* ---------------- projection ----------------
   Axis vectors are screen displacement per ONE world unit, so foreshortening
   is baked into the preset rather than applied afterwards.                  */
const PRESETS = {
    /* Measured off the reference sprite sheet: its top face is a parallelogram
       with edge vectors (54,17) and (-30,23) source px. The two ground axes
       are deliberately UNEQUAL — this is a rotated three-quarter view, not a
       corner-on isometric, which is what gives one wide front face to put a
       door in and one narrow face beside it. A symmetric iso would split the
       front evenly and lose that. */
    studio34: { x: [ 1.000, 0.315], y: [-0.556, 0.426], z: [0, -1] },
    /* classic 2:1 game isometric, corner-on and symmetric */
    iso21:    { x: [ 1.000, 0.500], y: [-1.000, 0.500], z: [0, -1] },
    /* the shallow ratio the game's block model already uses (ProsBlockGeom,
       TY = TX*0.28) — for anything that has to sit beside it */
    shallow:  { x: [ 1.000, 0.280], y: [-1.000, 0.280], z: [0, -1] },
};

/* A view maps world (x,y,z) to screen. +x runs right-and-down, +y runs
   left-and-down, +z is up; so the visible faces of a box are always
   EAST (x1), SOUTH (y1) and TOP (z1), and depth increases with x+y. */
function view(opt) {
    const o = opt || {};
    const a = PRESETS[o.preset || "studio34"];
    const s = o.scale || 100, ox = o.ox || 0, oy = o.oy || 0;

    /* The scale the style's pixel constants were tuned at. Outline weight,
       bevel rules and stud sizes are all px numbers picked against a hero-sized
       sprite; drawn unchanged at 32 px they are most of the sprite. */
    const REF = 300;

    const V = {
        preset: o.preset || "studio34", scale: s, ox, oy,
        ax: a.x, ay: a.y, az: a.z,
        /* line weight for this scale — never below hairline, never fatter than
           the constant it came from */
        lw: base => Math.max(0.7, Math.min(base, base * s / REF)),
        /* px on screen for a world length — the honest question to ask before
           drawing a detail */
        px: len => len * s,
        /* detail budget: 1 at hero scale, falling with the sprite. A component
           consults this and drops greebles rather than rendering them into
           mud. An icon is not a small picture of a hero asset; it is a
           different drawing of the same object, and this is the dial. */
        lod: s / REF,
        p(x, y, z) {
            return [ox + s * (x * a.x[0] + y * a.y[0] + z * a.z[0]),
                    oy + s * (x * a.x[1] + y * a.y[1] + z * a.z[1])];
        },
        /* painter depth — larger is nearer the camera */
        depth(x, y, z) { return x + y + z * 0.5; },
    };

    /* ---------------- face-local coordinates ----------------
       Returns a mapper for one face of a box. Local (u,v) are WORLD UNITS
       measured from the face's top-left as it appears on screen: u runs right
       along the face, v runs DOWN it. So a shape laid out like a UI rectangle
       lands the right way up on the right face, in the right place, with no
       per-face special-casing at the call site.                             */
    V.on = function (face, b) {
        let org, du, dv, w, h;
        if (face === "south") {                       // y = y1, the wide front
            org = [b.x0, b.y1, b.z1]; du = [1, 0, 0]; dv = [0, 0, -1];
            w = b.x1 - b.x0; h = b.z1 - b.z0;
        } else if (face === "east") {                 // x = x1, the narrow front
            org = [b.x1, b.y1, b.z1]; du = [0, -1, 0]; dv = [0, 0, -1];
            w = b.y1 - b.y0; h = b.z1 - b.z0;
        } else if (face === "north") {
            org = [b.x1, b.y0, b.z1]; du = [-1, 0, 0]; dv = [0, 0, -1];
            w = b.x1 - b.x0; h = b.z1 - b.z0;
        } else if (face === "west") {
            org = [b.x0, b.y0, b.z1]; du = [0, 1, 0]; dv = [0, 0, -1];
            w = b.y1 - b.y0; h = b.z1 - b.z0;
        } else {                                      // top: z = z1
            org = [b.x0, b.y0, b.z1]; du = [1, 0, 0]; dv = [0, 1, 0];
            w = b.x1 - b.x0; h = b.y1 - b.y0;
        }
        const m = (u, v, lift) => V.p(org[0] + du[0]*u + dv[0]*v + (lift||0)*NORMAL[face][0],
                                      org[1] + du[1]*u + dv[1]*v + (lift||0)*NORMAL[face][1],
                                      org[2] + du[2]*u + dv[2]*v + (lift||0)*NORMAL[face][2]);
        m.w = w; m.h = h; m.face = face;
        m.n  = (a, c, lift) => m(a * w, c * h, lift);       // normalised 0..1
        m.pts = (list, lift) => list.map(q => m(q[0], q[1], lift));

        /* Depth-compensated placement. A point pushed `lift` into the face does
           not stay where you put it: on a south face it slides right and up by
           the projection of the normal. Lay out a recess interior in the
           mouth's own coordinates and the top of it silently leaves the
           opening — which is how a ram ends up clipped to nothing and looks
           like a missing draw call.

           m.at(u,v,lift) solves the 2x2 system for the (du,dv) that cancels the
           normal's screen displacement, so (u,v) means the same place on screen
           at any depth. m.vShift(lift) is just the vertical half of it, for
           when you want real parallax but need to know where the usable band
           of the opening starts.                                             */
        const S = d => [ (d[0]*V.ax[0] + d[1]*V.ay[0] + d[2]*V.az[0]),
                         (d[0]*V.ax[1] + d[1]*V.ay[1] + d[2]*V.az[1]) ];
        const su = S(du), sv = S(dv), sn = S(NORMAL[face]);
        const det = su[0]*sv[1] - su[1]*sv[0];
        m.at = (u, v, lift) => {
            const l = lift || 0;
            if (!l || Math.abs(det) < 1e-9) return m(u, v, l);
            const bx = -l*sn[0], by = -l*sn[1];
            return m(u + ( bx*sv[1] - by*sv[0]) / det,
                     v + (-bx*su[1] + by*su[0]) / det, l);
        };
        m.vShift = lift => (Math.abs(det) < 1e-9 ? 0
            : ((-(-lift*sn[0])*su[1] + (-lift*sn[1])*su[0]) / det));
        return m;
    };
    return V;
}
/* outward normal per face, in world units — `lift` in a face mapper moves a
   decal off the surface (a raised rim) or into it (a recess). */
const NORMAL = { top:[0,0,1], south:[0,1,0], east:[1,0,0], north:[0,-1,0], west:[-1,0,0], bottom:[0,0,-1] };

/* ---------------- face shading ----------------
   The light is implied upper-left-front, the same one that puts steel()'s
   specular band off-centre left (§4.2). Nothing casts a computed shadow;
   these are the shades a face CARRIES.                                      */
const FACE = { top: 0.74, south: 0.60, east: 0.26, north: 0.30, west: 0.66, bottom: 0.10 };
/* a chamfer crowns the face below it and always catches more light */
const RIM  = { north: 0.92, west: 0.96, south: 0.84, east: 0.44 };

/* ---------------- the plated volume ----------------
   b     {x0,y0,z0,x1,y1,z1}
   opt   mat, heat, chamfer, shades{}, bands (sub-bands per face), skip[]

   Drawn in two passes over its own polygons — flood, then faces — so the
   assembly reads as one machine (§2). Faces get a small number of flat
   sub-bands running along the face's OWN axis: a flat quad has no curvature
   to band, but without that variation a big face reads as dead vinyl. Three
   bands is enough; the eye reads plate, not gradient.                       */
function boxPolys(V, b, c) {
    const cx = c || 0, zc = b.z1 - cx;
    const inset = [[b.x0+cx, b.y0+cx], [b.x1-cx, b.y0+cx], [b.x1-cx, b.y1-cx], [b.x0+cx, b.y1-cx]];
    const full  = [[b.x0, b.y0], [b.x1, b.y0], [b.x1, b.y1], [b.x0, b.y1]];
    const P = {};
    P.top = inset.map(q => V.p(q[0], q[1], b.z1));
    /* four chamfer bands, corner-shared, so they tile with no gaps */
    const CH = ["north", "east", "south", "west"];
    P.chamfer = {};
    for (let i = 0; i < 4; i++) {
        const j = (i + 1) & 3;
        P.chamfer[CH[i]] = [ V.p(inset[i][0], inset[i][1], b.z1),
                             V.p(inset[j][0], inset[j][1], b.z1),
                             V.p(full[j][0],  full[j][1],  zc),
                             V.p(full[i][0],  full[i][1],  zc) ];
    }
    P.south = [V.p(b.x0,b.y1,zc), V.p(b.x1,b.y1,zc), V.p(b.x1,b.y1,b.z0), V.p(b.x0,b.y1,b.z0)];
    P.east  = [V.p(b.x1,b.y1,zc), V.p(b.x1,b.y0,zc), V.p(b.x1,b.y0,b.z0), V.p(b.x1,b.y1,b.z0)];
    P.zc = zc;
    return P;
}

function solid(ctx, V, b, opt) {
    const o = opt || {}, P = boxPolys(V, b, o.chamfer);
    const skip = o.skip || [], has = f => skip.indexOf(f) < 0;
    /* the flood pass covers exactly the faces that will be painted — a solid
       stacked under another one skips its top, or the line would show through
       the piece above it. */
    const chK = o.chamferKeys || ["north", "west", "south", "east"];
    const polys = [];
    if (has("south")) polys.push(P.south);
    if (has("east"))  polys.push(P.east);
    if (has("chamfer")) for (const k of chK) polys.push(P.chamfer[k]);
    if (has("top")) polys.push(P.top);
    if (o.flood !== false) L.flood(ctx, polys, { color: o.out || PAL.OUT, w: o.outW || V.lw(PAL.OUT_W) });
    paintFaces(ctx, V, b, P, o);
    return P;
}

function paintFaces(ctx, V, b, P, o) {
    const mat = o.mat || "plating", heat = o.heat || 0;
    const sh = Object.assign({}, FACE, o.shades);
    const rim = Object.assign({}, RIM, o.rims);
    const skip = o.skip || [];
    const has = f => skip.indexOf(f) < 0;

    if (has("east"))  face(ctx, V, b, P.east,  "east",  sh.east,  mat, heat, o);
    if (has("south")) face(ctx, V, b, P.south, "south", sh.south, mat, heat, o);
    /* The back chamfers (north, west) are the rim you see ABOVE the top face.
       On a volume that is being repainted as a near lip they are occluded by
       whatever stands on it, so a caller can name only the chamfers it wants. */
    for (const k of (o.chamferKeys || ["north", "west", "south", "east"])) {
        if (!P.chamfer[k] || !has("chamfer")) continue;
        L.fill(ctx, P.chamfer[k], MAT.tone(mat, rim[k], heat * 0.8));
    }
    if (has("top")) face(ctx, V, b, P.top, "top", sh.top, mat, heat, o);
}

/* one face: banded along its own axis, then its edge rules. */
function face(ctx, V, b, poly, name, shade, mat, heat, o) {
    /* A band boundary is a straight line on screen, so on the TOP face it cuts
       across the parallelogram at an angle and reads as a fold in the plate
       rather than a change of tone. Big top faces want one flat tone and let
       the chamfer rim do the tonal work; upright faces can take three.       */
    const per = (o && o.faceBands) || {};
    const bands = per[name] !== undefined ? per[name] : ((o && o.bands) || 3);
    if (bands <= 1) {
        L.fill(ctx, poly, MAT.tone(mat, shade, heat));
    } else {
        /* band direction = the face's own u axis, taken from its polygon so it
           follows the projection instead of the screen. */
        const p0 = poly[0], p1 = poly[1];
        const tones = [];
        const spread = (o && o.spread) !== undefined ? o.spread : 0.10;
        for (let i = 0; i < bands; i++) {
            const t = bands === 1 ? 0.5 : i / (bands - 1);
            tones.push([1 / bands, band(clamp(shade + spread * (0.5 - t) * 2, 0, 1))]);
        }
        L.fill(ctx, poly, MAT.bandsAlong(ctx, p0, p1, tones, { mat, heat }));
    }
    /* §4.3 the bevel, generalised: bright rule along the face's top edge, dark
       rule along its bottom edge. This is what makes a plate sit ON something
       rather than float in front of it. */
    if (!o || o.bevel !== false) {
        const w = V.lw((o && o.bevelW) || 1.7);
        if (name !== "top") {
            L.rule(ctx, poly[0], poly[1], "rgba(255,255,255,.20)", w);
            L.rule(ctx, poly[3], poly[2], "rgba(0,0,0,.38)", w);
        }
    }
}

/* ---------------- fitting ----------------
   Sprite work is "put this volume in that rectangle", not "pick a scale and
   hope". Projects the eight corners of a world bounds box, then solves for the
   scale and origin that centre it in `rect` with `pad` px of margin.        */
function fit(presetName, b, rect, pad) {
    const a = PRESETS[presetName] || PRESETS.studio34, m = pad || 0;
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const x of [b.x0, b.x1]) for (const y of [b.y0, b.y1]) for (const z of [b.z0, b.z1]) {
        const sx = x*a.x[0] + y*a.y[0] + z*a.z[0];
        const sy = x*a.x[1] + y*a.y[1] + z*a.z[1];
        x0 = Math.min(x0, sx); x1 = Math.max(x1, sx);
        y0 = Math.min(y0, sy); y1 = Math.max(y1, sy);
    }
    const s = Math.min((rect.w - m*2) / (x1 - x0), (rect.h - m*2) / (y1 - y0));
    return view({ preset: presetName, scale: s,
                  ox: rect.x + (rect.w - (x1 - x0) * s) / 2 - x0 * s,
                  oy: rect.y + (rect.h - (y1 - y0) * s) / 2 - y0 * s });
}

GFX.iso = { PRESETS, view, fit, solid, boxPolys, paintFaces, face, FACE, RIM, NORMAL };
})(typeof window !== "undefined" ? window : globalThis);
