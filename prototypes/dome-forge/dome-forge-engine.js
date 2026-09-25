/* DomeForge engine — procedural lunar dome sprite renderer (2D).
 * Pure pixel math on an RGBA buffer: no DOM needed, so it runs in the browser or in node.
 * Everything is driven by a plain config object (see DEFAULTS); the same object also drives
 * the base assembly (dome-forge-base.js) and the 3D viewer (dome-forge-3d.js).
 *
 *   const out = DomeForge.render(cfg, 'unit');      // {width,height,data,cx,cy}
 *   const sheet = DomeForge.renderSheet(cfg);       // full sprite sheet
 *   DomeForge.toCanvas(out, canvas, cfg.pixelSize); // browser helper
 *   DomeForge.util                                  // shared helpers (noise, blending, colours)
 */
const DomeForge = (function () {
  'use strict';

  // ---------- small helpers ----------
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
  const lerp = (a, b, t) => a + (b - a) * t;
  const sstep = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
  const DEG = Math.PI / 180;
  const SQ3 = Math.sqrt(3);

  function hashInt(x, y, s) {
    let h = Math.imul(x | 0, 0x27d4eb2d) ^ Math.imul(y | 0, 0x165667b1) ^ Math.imul((s | 0) + 0x5bd1e995, 0x9e3779b1);
    h ^= h >>> 15; h = Math.imul(h, 0x2c1b3c6d);
    h ^= h >>> 12; h = Math.imul(h, 0x297a2d39);
    h ^= h >>> 15;
    return (h >>> 0) / 4294967296;
  }
  function hexToRgb(hex) {
    const m = /^#?([0-9a-f]{6})$/i.exec(String(hex).trim());
    if (!m) return [0.5, 0.5, 0.5];
    const n = parseInt(m[1], 16);
    return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }
  function norm3(x, y, z) { const l = Math.hypot(x, y, z) || 1; return [x / l, y / l, z / l]; }
  function dirFromAzEl(az, el) {
    const ca = Math.cos(az * DEG), sa = Math.sin(az * DEG), ce = Math.cos(el * DEG), se = Math.sin(el * DEG);
    return norm3(ca * ce, sa * ce, se);
  }

  // ---------- defaults ----------
  const LIGHTING_PRESETS = {
    A: { lightAz: 140, lightEl: 45, ambient: 0.50, diffuse: 0.55, hlX: -0.02, hlY: 0.55, shininess: 26, specInt: 0.75, glintSize: 0.05, glintStrength: 0.95, litBase: 0.02, litNear: 0.35, rimInt: 0.30, edgeShadow: 0.3, limbDark: 0.42 },
    B: { lightAz: 130, lightEl: 50, ambient: 0.52, diffuse: 0.52, hlX: 0.0,  hlY: 0.50, shininess: 14, specInt: 0.8, glintSize: 0.065, glintStrength: 0.9,  litBase: 0.03, litNear: 0.55, rimInt: 0.36, edgeShadow: 0.28, limbDark: 0.4 },
    C: { lightAz: 125, lightEl: 55, ambient: 0.55, diffuse: 0.50, hlX: 0.0,  hlY: 0.52, shininess: 7,  specInt: 0.9, glintSize: 0.055, glintStrength: 0.95, litBase: 0.04, litNear: 0.7,  rimInt: 0.42, edgeShadow: 0.25, limbDark: 0.36 },
    D: { lightAz: 145, lightEl: 35, ambient: 0.42, diffuse: 0.55, hlX: -0.04, hlY: 0.58, shininess: 40, specInt: 0.65, glintSize: 0.045, glintStrength: 0.85, litBase: 0.015, litNear: 0.22, rimInt: 0.22, edgeShadow: 0.35, limbDark: 0.5 },
  };

  const DEFAULTS = {
    // ---- geometry per sprite kind (fractions of that sprite's size) ----
    unit: { size: 256, domeRadius: 0.33, ringWidth: 0.07, socketCount: 1, socketStart: 270 },
    central: { size: 560, domeRadius: 0.295, ringWidth: 0.068, sides: 0, corner: 0.085, socketsAtCorners: false, socketCount: 8, socketStart: 90 },

    // ---- glass ----
    color: '#1fb75b',
    lightAz: 140, lightEl: 45,      // diffuse key light (math angles: 0 right, 90 up)
    ambient: 0.5, diffuse: 0.55,
    hlX: -0.02, hlY: 0.55,          // specular highlight position, fraction of dome radius (+y = up)
    shininess: 22, specInt: 0.8, specWhite: 0.62,
    glintSize: 0.05, glintAspect: 1.0, glintStrength: 0.95,
    rimAz: 315, rimInt: 0.4, rimPow: 3.5,
    limbDark: 0.42, limbPow: 2.0,     // darkening as the surface turns away from the viewer
    edgeShadow: 0.3, edgeShadowW: 0.25, edgeLine: 0.3,

    // ---- hex cells ----
    hexCells: 0.09,                 // cell circumradius as fraction of dome radius (at the centre)
    hexCurve: 1.3,                  // how fast cells squash from centre to edge (1 = true sphere, >1 exaggerated)
    hexLens: 0.5,                   // perspective: 0 = orthographic, 1 = camera almost touching the dome
    facetShade: 0.35,               // shade each cell as a flat facet (disco-ball tilt) blended with smooth shading
    hexRot: 0, hexLine: 0.9, hexLineDark: 0.16, hexLineLight: 0.22,
    facetVar: 0.07, facetBevel: 0.08,
    litBase: 0.02, litNear: 0.35, litAmount: 0.35,

    // ---- frame (metal ring) ----
    frameColor: '#a4a4a4', frameAmbient: 0.55, frameDiffuse: 0.5, frameSpec: 0.8, frameShine: 18,
    frameProfile: 'plate', bevel: 2.2, metalEnv: 0.8, edgeLight: 0.45, outline: 0.7,
    outerLip: 0.2,                   // narrow stepped flange around the periphery, as a fraction of ring width
    grainStyle: 'mottled', grain: 0.11, grainScale: 1.0,   // mottled = patches of different greys
    segments: 0, segDepth: 0.5,      // optional knurling of the outer band: link spacing in px (0 = off)

    // ---- socket (px at unit size 256; scales with the unit sprite) ----
    socketOn: true, socketW: 34, socketH: 24, socketT: 7.5, socketCorner: 5, socketCorners: 'chamfer', hollowDark: 0.8,
    socketInset: 3.5, socketFillet: 5, // how far the loop is pulled into the rim, and the fillet radius where they merge
    // socket lights (px at unit size 256): a tiny point light in each cavity with a small halo,
    // plus optional bar lights on the rim either side of each socket
    socketLights: true, lightColor: '#ffad55', lightGlow: 0.9, lightGlowR: 5, lightSize: 1.5,
    rimLights: false, lightAngle: 36, lightRadial: 0.5, lightLen: 12, lightW: 3,

    // ---- output ----
    pixelSize: 1, ssaa: 2, levels: 0,
    bgOn: true, bg: '#2b2b2b', bgNoise: 0.02, seed: 7,

    // ---- 3D view ----
    domeHeight: 0.9, plateThick: 0.45, fov3d: 30, shadow3d: true, lightFollowsCamera: true, autoRotate: false,

    // ---- sheet ----
    sheetColors: ['#1db052', '#8a8a8a'],
    sheetVariants: 'ABCD',          // letters of LIGHTING_PRESETS, or '' for current lighting only
    sheetGap: 18, sheetMargin: 24,
  };

  const PROFILES = {
    plate: {     // flat outer band with bevelled edges, dark seam, narrower inner band, dark gap to the glass
      innerFrac: 0.34,
      outer: [{ w: 0.18, k: 'up', a: 1 }, { w: 0.50, k: 'flat' }, { w: 0.18, k: 'down', a: 1 }, { w: 0.14, k: 'gap' }],
      inner: [{ w: 0.22, k: 'gap' }, { w: 0.34, k: 'up', a: 1 }, { w: 0.32, k: 'flat' }, { w: 0.12, k: 'down', a: 0.6 }],
    },
    classic: {   // knurled outer tube, thin groove, recessed plate, smooth inner tube, dark gap to the glass
      innerFrac: 0.30,
      outer: [{ w: 0.58, k: 'ridge', a: 1.0, seg: true }, { w: 0.10, k: 'groove', a: 0.5 }, { w: 0.32, k: 'flat', o: 0.2 }],
      inner: [{ w: 0.24, k: 'gap' }, { w: 0.64, k: 'ridge', a: 0.9 }, { w: 0.12, k: 'flat', o: 0.2 }],
    },
    simple: {
      innerFrac: 0.22,
      outer: [{ w: 0.16, k: 'up', a: 1 }, { w: 0.84, k: 'flat' }],
      inner: [{ w: 0.25, k: 'gap' }, { w: 0.75, k: 'flat' }],
    },
    heavy: {     // two outer tubes with a channel between, wide plate, inner tube
      innerFrac: 0.34,
      outer: [{ w: 0.30, k: 'ridge', a: 1.1, seg: true }, { w: 0.10, k: 'groove', a: 0.6 }, { w: 0.24, k: 'ridge', a: 0.8 }, { w: 0.36, k: 'flat', o: 0.16 }],
      inner: [{ w: 0.2, k: 'gap' }, { w: 0.6, k: 'ridge', a: 1.0 }, { w: 0.2, k: 'flat', o: 0.16 }],
    },
    chamfer: {   // the earlier look: chamfered lip, groove, flat plate
      innerFrac: 0.40,
      outer: [{ w: 0.10, k: 'up', a: 1 }, { w: 0.14, k: 'flat' }, { w: 0.15, k: 'groove', a: 0.9 }, { w: 0.61, k: 'flat' }],
      inner: [{ w: 0.14, k: 'gap' }, { w: 0.46, k: 'ridge', a: 1 }, { w: 0.40, k: 'flat' }],
    },
  };

  const LIP = [{ w: 0.30, k: 'up', a: 0.8 }, { w: 0.42, k: 'flat', o: 0.05 }, { w: 0.28, k: 'seam' }];

  function evalProfile(segs, t, Wpx) {
    let acc = 0;
    for (let i = 0; i < segs.length; i++) {
      const s = segs[i];
      if (t < acc + s.w || i === segs.length - 1) {
        const u = clamp((t - acc) / s.w, 0, 1), wpx = Math.max(0.5, s.w * Wpx);
        const o = s.o || 0, sg = s.seg ? 1 : 0;
        switch (s.k) {
          case 'up': return [s.a / wpx, o, 0, sg];
          case 'down': return [-s.a / wpx, o, 0, sg];
          case 'ridge': { const sn = Math.sin(Math.PI * u); return [s.a * Math.PI * Math.cos(Math.PI * u) / wpx, o + 0.28 * (1 - sn) * (1 - sn), 0, sg]; }
          case 'groove': return [-s.a * Math.PI * Math.cos(Math.PI * u) / wpx, o + 0.55 * Math.sin(Math.PI * u), 0, sg];
          case 'gap': return [0, 0.75, 1, 0];
          case 'seam': return [0, 0.6, 0, 0];
          default: return [0, o, 0, sg];
        }
      }
      acc += s.w;
    }
    return [0, 0, 0, 0];
  }

  // ---------- geometry ----------
  function roundedRectSDF(x, y, hw, hh, rc) {
    const qx = Math.abs(x) - hw + rc, qy = Math.abs(y) - hh + rc;
    return Math.hypot(Math.max(qx, 0), Math.max(qy, 0)) + Math.min(Math.max(qx, qy), 0) - rc;
  }
  function chamferRectSDF(x, y, hw, hh, c) {   // rectangle with cut corners (octagon)
    const ax = Math.abs(x), ay = Math.abs(y);
    return Math.max(ax - hw, ay - hh, (ax + ay - (hw + hh - c)) * 0.7071068);
  }
  // smooth minimum: two shapes merge with a concave fillet of radius ~k instead of a sharp crease
  function smin(a, b, k) {
    if (k <= 0) return Math.min(a, b);
    const h = Math.max(k - Math.abs(a - b), 0) / k;
    return Math.min(a, b) - h * h * k * 0.25;
  }
  // the rim outline on its own: a circle, or a regular polygon with heavily rounded corners
  function ringSDF(X, Y, g) {
    const r = Math.hypot(X, Y);
    if (g.sides < 3) return r - g.Rout;
    const sector = 2 * Math.PI / g.sides, ap = g.Rout - g.corner;
    const ang = Math.atan2(Y, X);
    const base = g.rot + Math.round((ang - g.rot) / sector) * sector;   // nearest edge normal
    const a = ang - base, px = r * Math.cos(a), py = r * Math.sin(a);
    let d;
    if (px <= ap) d = px - ap;
    else { const t = ap * Math.tan(sector / 2), ey = clamp(py, -t, t); d = Math.hypot(px - ap, py - ey); }
    return d - g.corner;
  }
  function boundaryDistance(g, angDeg) {
    const c = Math.cos(angDeg * DEG), s = Math.sin(angDeg * DEG);
    let lo = g.Rd, hi = g.Rout * 2;
    for (let i = 0; i < 40; i++) { const mid = (lo + hi) / 2; if (ringSDF(mid * c, mid * s, g) < 0) lo = mid; else hi = mid; }
    return (lo + hi) / 2;
  }
  // The metal is one piece: the rim merged with every socket loop. This is the signed
  // distance to that merged outline (negative inside). o receives [union, ring, loops].
  function metalSDF(X, Y, g, o) {
    const dr = ringSDF(X, Y, g);
    let dl = 1e9;
    for (let i = 0; i < g.sockets.length; i++) {
      const so = g.sockets[i];
      const ly = X * so.c + Y * so.s - so.rc, lx = -X * so.s + Y * so.c;
      const d = g.sk.shape(lx, ly, g.sk.w / 2, g.sk.h / 2, g.sk.rc);
      if (d < dl) dl = d;
    }
    const du = dl < 1e8 ? smin(dr, dl, g.fillet) : dr;
    if (o) { o[0] = du; o[1] = dr; o[2] = dl; }
    return du;
  }
  // signed distance to the nearest socket hollow (negative inside the hole)
  function hollowSDF(X, Y, g) {
    let dh = 1e9;
    for (let i = 0; i < g.sockets.length; i++) {
      const so = g.sockets[i];
      const ly = X * so.c + Y * so.s - so.rc, lx = -X * so.s + Y * so.c;
      const d = g.sk.shape(lx, ly, g.sk.hw, g.sk.hh, g.sk.hrc);
      if (d < dh) dh = d;
    }
    return dh;
  }
  function gradient(fn, X, Y, g, out) {
    const e = 0.35;
    const gx = fn(X + e, Y, g) - fn(X - e, Y, g), gy = fn(X, Y + e, g) - fn(X, Y - e, g);
    const l = Math.hypot(gx, gy) || 1;
    out[0] = gx / l; out[1] = gy / l; return out;
  }

  // pointy-top hex grid, circumradius 1. Returns cell id, centre, local offset, and distance to the nearest edge.
  function hexCell(u, v, o) {
    const q = (SQ3 / 3) * u - v / 3, r = (2 / 3) * v;
    let rx = Math.round(q), rz = Math.round(r), ry = Math.round(-q - r);
    const dx = Math.abs(rx - q), dy = Math.abs(ry + q + r), dz = Math.abs(rz - r);
    if (dx > dy && dx > dz) rx = -ry - rz; else if (dy <= dz) rz = -rx - ry;
    const cu = SQ3 * (rx + rz / 2), cv = 1.5 * rz;
    const lu = u - cu, lv = v - cv;
    const h = Math.max(Math.abs(lu), Math.abs(lu * 0.5 + lv * 0.8660254), Math.abs(lu * 0.5 - lv * 0.8660254));
    o.q = rx; o.r = rz; o.cu = cu; o.cv = cv; o.lu = lu; o.lv = lv; o.edge = 0.8660254 - h;
    return o;
  }

  // ---------- scene setup ----------
  function buildScene(cfg, kind) {
    const K = cfg[kind] || cfg.unit;
    const S = K.size, sc = S / 256, us = cfg.unit.size / 256;   // sc: this sprite's scale, us: socket scale
    const Rd = K.domeRadius * S, ringW = K.ringWidth * S;
    const sk = {
      w: cfg.socketW * us, h: cfg.socketH * us, t: cfg.socketT * us, rc: cfg.socketCorner * us,
      shape: cfg.socketCorners === 'chamfer' ? chamferRectSDF : roundedRectSDF,
    };
    sk.hw = sk.w / 2 - sk.t; sk.hh = sk.h / 2 - sk.t;
    sk.hrc = cfg.socketCorners === 'chamfer' ? Math.max(0, sk.rc - sk.t * 0.6) : Math.max(0.5, sk.rc - sk.t);
    const sides = (K.sides | 0) >= 3 ? (K.sides | 0) : 0;
    const g = { kind, S, sc, us, Rd, ringW, Rout: Rd + ringW, sk, sockets: [], fillet: cfg.socketFillet * us, sides, corner: 0, rot: 0 };
    if (sides) {
      // Rout is the apothem (distance to the flat edges); corners are rounded with radius `corner`
      g.corner = Math.min((K.corner || 0) * S, g.Rout - Rd - 1);
      g.rot = (K.socketStart + (K.socketsAtCorners ? 180 / sides : 0)) * DEG;   // sockets sit on the flat edges (or at the corners)
    }
    const n = cfg.socketOn ? (K.socketCount | 0) : 0;
    for (let i = 0; i < n; i++) {
      const ang = K.socketStart + (360 / n) * i;
      // loop centre: hangs off the rim, pulled inward by socketInset so the two merge into one piece
      const rc = boundaryDistance(g, ang) + sk.h / 2 - cfg.socketInset * us;
      g.sockets.push({ ang, rc, c: Math.cos(ang * DEG), s: Math.sin(ang * DEG) });
    }
    // socket lights: two bars on the rim, lightAngle degrees either side of each socket
    g.lights = [];
    if (n && cfg.rimLights && cfg.lightGlow > 0) {
      // when sockets are packed so tightly that neighbouring pairs would collide, put one bar in each gap instead
      const spacing = 360 / n, single = spacing / 2 <= cfg.lightAngle + 4;
      const angles = [];
      for (const so of g.sockets) { if (single) angles.push(so.ang + spacing / 2); else angles.push(so.ang - cfg.lightAngle, so.ang + cfg.lightAngle); }
      for (const a of angles) g.lights.push({ c: Math.cos(a * DEG), s: Math.sin(a * DEG), r0: boundaryDistance(g, a) - ringW * cfg.lightRadial });
    }
    g.lt = { len: cfg.lightLen * us, w: cfg.lightW * us, sig: cfg.lightGlowR * us * 0.45, reach: cfg.lightGlowR * us * 2.2, pt: cfg.lightSize * us, psig: cfg.lightGlowR * us * 0.5 };
    // bounds -> centre so everything fits
    const ext = sides ? g.Rout / Math.cos(Math.PI / sides) : g.Rout;
    let minX = -ext, maxX = ext, minY = -ext, maxY = ext;
    for (const so of g.sockets) {
      const reach = so.rc + sk.h / 2, half = sk.w / 2;
      const px = so.c * reach, py = so.s * reach;
      minX = Math.min(minX, px - half); maxX = Math.max(maxX, px + half);
      minY = Math.min(minY, py - half); maxY = Math.max(maxY, py + half);
    }
    g.cx = S / 2 - (minX + maxX) / 2;
    g.cy = S / 2 + (minY + maxY) / 2;   // screen y is down; math y is up

    // lighting
    const L = dirFromAzEl(cfg.lightAz, cfg.lightEl);
    const hz = Math.sqrt(Math.max(0.02, 1 - cfg.hlX * cfg.hlX - cfg.hlY * cfg.hlY));
    const H = norm3(cfg.hlX, cfg.hlY, hz);
    const Rdir = dirFromAzEl(cfg.rimAz, 10);
    const Hm = norm3(L[0], L[1], L[2] + 1);
    const L2 = Math.hypot(L[0], L[1]) || 1;
    const base = hexToRgb(cfg.color), frameBase = hexToRgb(cfg.frameColor);
    const lightCol = hexToRgb(cfg.lightColor), lightCore = lightCol.map(v => lerp(v, 1, 0.6));
    const specCol = [lerp(base[0], 1, cfg.specWhite), lerp(base[1], 1, cfg.specWhite), lerp(base[2], 1, cfg.specWhite)];
    const rimCol = [lerp(base[0], 1, 0.45), lerp(base[1], 1, 0.45), lerp(base[2], 1, 0.45)];
    const prof = PROFILES[cfg.frameProfile] || PROFILES.classic;
    return {
      g, cfg, L, H, Hm, Rdir, L2x: L[0] / L2, L2y: L[1] / L2, base, frameBase, lightCol, lightCore, specCol, rimCol, prof,
      Wi: ringW * prof.innerFrac, Wo: ringW * (1 - prof.innerFrac), Wh: Math.max(1.2 * us, sk.t * 0.42),
      bevel: cfg.bevel * sc, hexLinePx: cfg.hexLine * Math.sqrt(sc), s: cfg.hexCells,
      glintW: cfg.glintSize * Rd * cfg.glintAspect, glintH: cfg.glintSize * Rd / cfg.glintAspect,
      lensD: cfg.hexLens > 0.001 ? 1.15 + 12 * (1 - cfg.hexLens) * (1 - cfg.hexLens) : 0,
      thMax: cfg.hexLens > 0.001 ? Math.acos(1 / (1.15 + 12 * (1 - cfg.hexLens) * (1 - cfg.hexLens))) : Math.PI / 2,
      curve: Math.max(0.3, cfg.hexCurve || 1),
      seed: cfg.seed | 0,
      hexCos: Math.cos(cfg.hexRot * DEG), hexSin: Math.sin(cfg.hexRot * DEG),
    };
  }

  // ---------- shading ----------
  // Metal = key light + a fake studio environment (bright sky above, a floor bounce below,
  // a little fresnel on grazing normals) + a tight specular streak.
  function shadeMetal(nx, ny, nz, sc, base, grain, out, sx, sy, sz) {
    const L = sc.L, H = sc.Hm, c = sc.cfg;
    if (sx === undefined) { sx = nx; sy = ny; sz = nz; }      // normal used for the specular streak
    const ndl = Math.max(0, nx * L[0] + ny * L[1] + nz * L[2]);
    const ndh = Math.max(0, sx * H[0] + sy * H[1] + sz * H[2]);
    const sky = sstep(-0.6, 0.85, ny);
    const floor = sstep(-0.25, -0.95, ny) * 0.5;
    const fres = Math.pow(1 - nz, 3) * 0.3;
    const E = lerp(0.72, 0.45 + 0.55 * sky + floor + fres, c.metalEnv);
    // bevels that face the light get a bright painted edge
    const tl = Math.hypot(nx, ny);
    const edge = tl > 1e-4 ? c.edgeLight * Math.max(0, (nx * sc.L2x + ny * sc.L2y) / tl) * Math.min(1, tl * 1.8) : 0;
    const spec = Math.pow(ndh, c.frameShine) * c.frameSpec + edge;
    const k = c.frameAmbient * E + c.frameDiffuse * ndl + grain * nz * nz;
    out[0] = base[0] * k + spec; out[1] = base[1] * k + spec; out[2] = base[2] * k + spec;
    return out;
  }
  // surface texture, in logical px so it stays the same size at any pixelSize
  // jittered-grid patch noise: every point belongs to the nearest of a set of scattered seeds,
  // and each seed carries its own random shade -> irregular patches of different greys
  function patchNoise(x, y, cell, seed, soft) {
    const gx = Math.floor(x / cell), gy = Math.floor(y / cell);
    let d1 = 1e9, d2 = 1e9, v1 = 0.5, v2 = 0.5;
    for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) {
      const cx = gx + i, cy = gy + j;
      const jx = (cx + hashInt(cx, cy, seed)) * cell, jy = (cy + hashInt(cx, cy, seed + 1)) * cell;
      const d = (x - jx) * (x - jx) + (y - jy) * (y - jy);
      if (d < d1) { d2 = d1; v2 = v1; d1 = d; v1 = hashInt(cx, cy, seed + 2); }
      else if (d < d2) { d2 = d; v2 = hashInt(cx, cy, seed + 2); }
    }
    // soften the border between two patches
    const w = sstep(0, soft, (Math.sqrt(d2) - Math.sqrt(d1)) / cell);
    return lerp((v1 + v2) * 0.5, v1, w);
  }
  function metalGrain(lx, ly, r, ang, sc, seed) {
    const c = sc.cfg, g = c.grain;
    if (g <= 0 || c.grainStyle === 'smooth') return 0;
    const x = lx | 0, y = ly | 0;
    if (c.grainStyle === 'mottled') {
      const k = (c.grainScale || 1) * sc.g.sc;
      const big = patchNoise(lx, ly, 8 * k, seed, 0.5) - 0.5;
      const mid = patchNoise(lx + 37, ly - 11, 4 * k, seed + 40, 0.55) - 0.5;
      const fine = hashInt(x, y, seed + 8) - 0.5;
      return (big * 1.5 + mid * 0.7 + fine * 0.06) * g * 2;
    }
    if (c.grainStyle === 'brushed') {
      return ((hashInt(Math.round(r * 1.5), Math.round(ang * 40), seed) - 0.5) * 0.7 + (hashInt(x * 3, y * 3, seed + 6) - 0.5) * 0.3) * g * 2;
    }
    const n = (hashInt(x, y, seed) - 0.5) * 0.45 + (hashInt(x >> 1, y >> 1, seed + 1) - 0.5) * 0.35 + (hashInt(x >> 2, y >> 2, seed + 2) - 0.5) * 0.2;
    const speck = hashInt(x, y, seed + 3) < 0.045 ? -0.6 : 0;
    return (n + speck) * g * 2;
  }

  const _cell = {};

  // screen radius (0..1) -> "surface distance" from the dome centre, in radians of a unit sphere.
  // Orthographic sphere: asin(rr). With a lens (camera at distance d radii) the visible cap is
  // smaller and the squash toward the edge is steeper. hexCurve then exaggerates the whole thing.
  function surfAngle(rr, sc) {
    rr = Math.min(rr, 1);
    let th;
    if (sc.lensD > 0) {
      const d = sc.lensD, k = rr / Math.sqrt(d * d - 1);
      th = Math.asin(Math.min(1, k * d / Math.sqrt(1 + k * k))) - Math.atan(k);
    } else th = Math.asin(rr);
    return sc.curve === 1 ? th : sc.thMax * Math.pow(Math.max(0, th) / sc.thMax, sc.curve);
  }
  // inverse: surface distance -> screen radius
  function screenRadius(th, sc) {
    if (th >= sc.thMax) return 1;
    let t = sc.curve === 1 ? th : sc.thMax * Math.pow(th / sc.thMax, 1 / sc.curve);
    if (sc.lensD > 0) { const d = sc.lensD; return Math.min(1, Math.sin(t) / (d - Math.cos(t)) * Math.sqrt(d * d - 1)); }
    return Math.sin(t);
  }

  function shadeGlass(X, Y, sc, out) {
    const { g, cfg, L, H, base } = sc, Rd = g.Rd;
    const nx = X / Rd, ny = Y / Rd;
    const rr2 = nx * nx + ny * ny, rr = Math.sqrt(rr2);
    const nz = Math.sqrt(Math.max(0, 1 - rr2));
    const ndl = Math.max(0, nx * L[0] + ny * L[1] + nz * L[2]);
    let k = cfg.ambient + cfg.diffuse * ndl;

    // hex cells mapped onto the curved surface: cells keep their size on the surface, so on
    // screen they squash more and more from the centre toward the edge
    const theta = surfAngle(rr, sc);
    const f = rr > 1e-6 ? theta / rr : (sc.lensD > 0 ? surfAngle(0.001, sc) / 0.001 : 1);
    let u = nx * f, v = ny * f;
    if (sc.hexSin !== 0) { const tu = u * sc.hexCos - v * sc.hexSin; v = u * sc.hexSin + v * sc.hexCos; u = tu; }
    const s = sc.s;
    const cell = hexCell(u / s, v / s, _cell);
    const rnd = hashInt(cell.q, cell.r, sc.seed), rnd2 = hashInt(cell.q, cell.r, sc.seed + 77);
    let cellMul = 1 + (rnd2 - 0.5) * cfg.facetVar * 2;
    cellMul += cfg.facetBevel * (cell.lu * L[0] + cell.lv * L[1]) / 0.866;

    // the cell centre's true normal (through the inverse mapping) drives facet shading and lit facets
    let lit = 0, specC = -1;
    const cr = Math.hypot(cell.cu, cell.cv), ct = cr * s;
    if (ct < sc.thMax) {
      const rrc = screenRadius(ct, sc);
      const cdir = cr > 1e-9 ? rrc / cr : 0;
      let cnx = cell.cu * cdir, cny = cell.cv * cdir;
      if (sc.hexSin !== 0) { const tx = cnx * sc.hexCos + cny * sc.hexSin; cny = -cnx * sc.hexSin + cny * sc.hexCos; cnx = tx; }
      const cnz = Math.sqrt(Math.max(0, 1 - cnx * cnx - cny * cny));
      const cndh = Math.max(0, cnx * H[0] + cny * H[1] + cnz * H[2]);
      const p = cfg.litBase + cfg.litNear * Math.pow(cndh, cfg.shininess * 0.35 + 2);
      if (rnd < p) lit = cfg.litAmount * (0.55 + 0.45 * rnd2);
      if (cfg.facetShade > 0) {
        // shade as a flat tile tilted like the sphere at its centre
        const cndl = Math.max(0, cnx * L[0] + cny * L[1] + cnz * L[2]);
        k = lerp(k, cfg.ambient + cfg.diffuse * cndl, cfg.facetShade);
        specC = Math.pow(cndh, cfg.shininess) * cfg.specInt;
      }
    }

    // cell outline: embossed (lighter on the side of the cell that faces the light, darker opposite),
    // thinning and fading toward the limb where the cells squash into slivers
    const fp = Math.min((surfAngle(rr + 0.002, sc) - surfAngle(Math.max(0, rr - 0.002), sc)) / 0.004, 6) / Rd;
    const halfW = Math.max(sc.hexLinePx / Rd * 0.5, fp * 0.35);
    const line = (1 - sstep(halfW - fp * 0.6, halfW + fp * 0.6, cell.edge * s)) * (0.35 + 0.65 * nz);
    const cl = Math.hypot(cell.lu, cell.lv) || 1;
    const litSide = Math.max(0, (cell.lu * L[0] + cell.lv * L[1]) / cl);

    let r = base[0] * k * cellMul, gg = base[1] * k * cellMul, b = base[2] * k * cellMul;
    if (lit > 0) { r = lerp(r, r * 1.35 + 0.12, lit); gg = lerp(gg, gg * 1.35 + 0.12, lit); b = lerp(b, b * 1.35 + 0.12, lit); }
    const ld = 1 - cfg.hexLineDark * line * (1 - litSide) + cfg.hexLineLight * line * litSide; r *= ld; gg *= ld; b *= ld;

    // broad specular glow
    const ndh = Math.max(0, nx * H[0] + ny * H[1] + nz * H[2]);
    let spec = Math.pow(ndh, cfg.shininess) * cfg.specInt;
    if (specC >= 0) spec = lerp(spec, specC, cfg.facetShade);
    spec = clamp(spec, 0, 1);
    r = lerp(r, sc.specCol[0], spec); gg = lerp(gg, sc.specCol[1], spec); b = lerp(b, sc.specCol[2], spec);

    // limb darkening: the surface turns away from the viewer toward the edge, so it goes dark
    // smoothly from the centre outward, which is what makes the dome read as convex
    const limb = 1 - cfg.limbDark * Math.pow(1 - nz, cfg.limbPow);
    r *= limb; gg *= limb; b *= limb;

    // contact shadow of the ring lip on the lit side
    const facing = rr > 1e-6 ? 0.5 + 0.5 * (nx * sc.L2x + ny * sc.L2y) / rr : 0.5;
    const band = sstep(1 - cfg.edgeShadowW, 1, rr);
    const shade = 1 - cfg.edgeShadow * band * facing;
    r *= shade; gg *= shade; b *= shade;

    // rim / reflected light on the far limb (added after the darkening so it stays bright)
    const Rr = sc.Rdir;
    const rim = Math.pow(1 - nz, cfg.rimPow) * Math.max(0, nx * Rr[0] + ny * Rr[1]) * cfg.rimInt;
    r += rim * sc.rimCol[0]; gg += rim * sc.rimCol[1]; b += rim * sc.rimCol[2];

    // thin bright glass lip, strongest where it faces the light
    const edgePx = (1 - rr) * Rd;
    const el = Math.exp(-((edgePx - 0.9) * (edgePx - 0.9)) / 0.7) * cfg.edgeLine * (0.25 + 0.75 * facing);
    r += el * sc.rimCol[0]; gg += el * sc.rimCol[1]; b += el * sc.rimCol[2];

    // the diamond glint at the highlight
    const gx = Math.abs(X - cfg.hlX * Rd) / sc.glintW, gy = Math.abs(Y - cfg.hlY * Rd) / sc.glintH;
    const ga = (1 - sstep(0.82, 1.06, gx + gy)) * cfg.glintStrength;
    if (ga > 0) { const w0 = sc.specCol; r = lerp(r, lerp(w0[0], 1, 0.6), ga); gg = lerp(gg, lerp(w0[1], 1, 0.6), ga); b = lerp(b, lerp(w0[2], 1, 0.6), ga); }

    out[0] = r; out[1] = gg; out[2] = b;
    return out;
  }

  const HOLLOW = [{ w: 0.34, k: 'gap' }, { w: 0.66, k: 'up', a: 0.8 }];
  const _n = [0, 0], _m = [0, 0, 0];

  function shadeFrame(X, Y, lx, ly, r, dH, sc, out) {
    const { g, cfg, prof } = sc;
    metalSDF(X, Y, g, _m);
    const dOut = -_m[0], dIn = r - g.Rd, ang = Math.atan2(Y, X);
    let slope = 0, occ = 0, gap = 0, seg = 0, dirx = 0, diry = 0;
    if (dOut < sc.Wo && dOut <= dH) {
      // outer edge of the merged piece: the lip only where the ring outline dominates, fading out on the loops
      const n = gradient(metalSDF, X, Y, g, _n); dirx = n[0]; diry = n[1];
      const Wl = Math.min(sc.Wo * 0.6, cfg.outerLip * g.ringW);
      const lipMix = Wl > 0 ? sstep(-1.5 * g.us, 2.5 * g.us, _m[2] - _m[1]) : 0;
      const pr = evalProfile(prof.outer, dOut / sc.Wo, sc.Wo);
      slope = pr[0]; occ = pr[1]; gap = pr[2]; seg = pr[3];
      if (lipMix > 0) {
        const pl = (dOut < Wl) ? evalProfile(LIP, dOut / Wl, Wl) : evalProfile(prof.outer, (dOut - Wl) / (sc.Wo - Wl), sc.Wo - Wl);
        slope = lerp(slope, pl[0], lipMix); occ = lerp(occ, pl[1], lipMix); gap = Math.max(gap * (1 - lipMix), pl[2] * lipMix);
      }
    } else if (dH < sc.Wh) {
      // rim of a socket hollow
      const n = gradient(hollowSDF, X, Y, g, _n); dirx = n[0]; diry = n[1];
      const p = evalProfile(HOLLOW, dH / sc.Wh, sc.Wh); slope = -p[0]; occ = p[1]; gap = p[2];
    } else if (dIn < sc.Wi) {
      const p = evalProfile(prof.inner, dIn / sc.Wi, sc.Wi); slope = -p[0]; occ = p[1]; gap = p[2]; dirx = X / (r || 1); diry = Y / (r || 1);
    }
    const sl = slope * sc.bevel;
    const bl = Math.hypot(sl, 1), bx = sl * dirx / bl, by = sl * diry / bl, bz = 1 / bl;
    let nx = bx, ny = by, nz = bz, segMul = 1;
    // knurling: the outer tube is cut into short links, each slightly pillowed, with a dark divider between
    if (seg && cfg.segments > 0 && cfg.segDepth > 0) {
      const sp = cfg.segments * g.sc, arc = ang * g.Rout;
      const f = arc / sp - Math.floor(arc / sp);
      const dEdge = Math.min(f, 1 - f) * sp;
      const line = 1 - sstep(0.25 * g.sc, 0.95 * g.sc, dEdge);
      segMul = 1 - cfg.segDepth * 0.5 * line;
      const tilt = Math.sin(2 * Math.PI * f) * cfg.segDepth * 0.28;
      const tx = bx - diry * tilt, ty = by + dirx * tilt, tl = Math.hypot(tx, ty, bz);
      nx = tx / tl; ny = ty / tl; nz = bz / tl;
    }
    const grain = metalGrain(lx, ly, r, ang, sc, sc.seed + 3);
    shadeMetal(nx, ny, nz, sc, sc.frameBase, grain, out, bx, by, bz);
    const dark = (1 - occ) * segMul;
    out[0] *= dark; out[1] *= dark; out[2] *= dark;
    if (gap) { const gm = 1 - 0.55 * gap; out[0] *= gm; out[1] *= gm; out[2] *= gm; }
    // inked contour along the outer outline and around the hollows
    const ol = 1 - cfg.outline * (1 - sstep(0.3 * g.sc, 1.0 * g.sc, Math.min(dOut, dH)));
    out[0] *= ol; out[1] *= ol; out[2] *= ol;
    return out;
  }

  // ---------- main render ----------
  function render(cfg, kind) {
    cfg = Object.assign({}, DEFAULTS, cfg || {});
    kind = kind || 'unit';
    const sc = buildScene(cfg, kind);
    const g = sc.g, S = g.S;
    const ps = Math.max(1, cfg.pixelSize | 0), ss = Math.max(1, Math.min(4, cfg.ssaa | 0));
    const W = Math.ceil(S / ps), Hh = W;
    const data = new Uint8ClampedArray(W * Hh * 4);
    const bg = hexToRgb(cfg.bg);
    const aa = ps;                       // one output pixel, in logical px
    const col = [0, 0, 0], tmp = [0, 0, 0];
    const hasSock = g.sockets.length > 0;
    const inv = 1 / (ss * ss);

    for (let py = 0; py < Hh; py++) {
      for (let px = 0; px < W; px++) {
        let ar = 0, ag = 0, ab = 0, aA = 0;
        for (let sy = 0; sy < ss; sy++) for (let sx = 0; sx < ss; sx++) {
          const lx = (px + (sx + 0.5) / ss) * ps, ly = (py + (sy + 0.5) / ss) * ps;
          const X = lx - g.cx, Y = g.cy - ly;
          const r = Math.hypot(X, Y);

          // background
          let R = 0, G = 0, B = 0, A = 0;
          if (cfg.bgOn) {
            const n = (hashInt(lx | 0, ly | 0, sc.seed + 1) - 0.5) * cfg.bgNoise;
            R = bg[0] + n; G = bg[1] + n; B = bg[2] + n; A = 1;
          }

          // metal: ring and socket loops are one merged piece, with the hollows cut out
          const dU = metalSDF(X, Y, g, null);
          const dH = hasSock ? hollowSDF(X, Y, g) : 1e9;
          if (hasSock && cfg.hollowDark > 0) {
            const hol = (1 - sstep(-aa * 0.5, aa * 0.5, dH)) * cfg.hollowDark;
            if (hol > 0) {
              const ao = 0.05 + 0.07 * sstep(0, -3 * g.us, dH);   // shadowed hole, darkest at its edge
              R = lerp(R, ao, hol); G = lerp(G, ao, hol); B = lerp(B, ao, hol); A = A + hol * (1 - A);
            }
          }
          const fa = (1 - sstep(-aa * 0.5, aa * 0.5, dU)) * sstep(g.Rd - aa * 0.5, g.Rd + aa * 0.5, r) * sstep(-aa * 0.5, aa * 0.5, dH);
          if (fa > 0) {
            shadeFrame(X, Y, lx, ly, r, dH, sc, tmp);
            R = lerp(R, tmp[0], fa); G = lerp(G, tmp[1], fa); B = lerp(B, tmp[2], fa); A = A + fa * (1 - A);
          }

          // glass dome
          const ga = 1 - sstep(g.Rd - aa * 0.5, g.Rd + aa * 0.5, r);
          if (ga > 0) {
            const rcl = Math.min(r, g.Rd - 0.01);
            const k = r > 0 ? rcl / r : 1;
            shadeGlass(X * k, Y * k, sc, col);
            R = lerp(R, col[0], ga); G = lerp(G, col[1], ga); B = lerp(B, col[2], ga); A = A + ga * (1 - A);
          }

          // socket lights: a tiny point light in the middle of each cavity with a small halo
          if (hasSock && cfg.socketLights && cfg.lightGlow > 0) {
            const lt = g.lt, reach = lt.psig * 3 + lt.pt;
            for (let i = 0; i < g.sockets.length; i++) {
              const so = g.sockets[i];
              const dx = X - so.c * so.rc, dy = Y - so.s * so.rc;
              if (dx > reach || dx < -reach || dy > reach || dy < -reach) continue;
              const d = Math.hypot(dx, dy);
              const core = 1 - sstep(lt.pt - aa * 0.5, lt.pt + aa * 0.5, d);
              const dd = Math.max(d - lt.pt, 0), gl = Math.exp(-dd * dd / (2 * lt.psig * lt.psig)) * cfg.lightGlow;
              R += sc.lightCol[0] * gl * 0.9; G += sc.lightCol[1] * gl * 0.9; B += sc.lightCol[2] * gl * 0.9;
              A = A + gl * 0.8 * (1 - A);
              R = lerp(R, sc.lightCore[0], core); G = lerp(G, sc.lightCore[1], core); B = lerp(B, sc.lightCore[2], core);
              A = A + core * (1 - A);
            }
          }
          // optional rim bars beside each socket
          if (g.lights.length) {
            const lt = g.lt;
            for (let i = 0; i < g.lights.length; i++) {
              const li = g.lights[i];
              const ly = X * li.c + Y * li.s - li.r0;           // radial offset from the bar
              if (ly > lt.reach || ly < -lt.reach) continue;
              const lx = -X * li.s + Y * li.c;                  // along the rim
              if (lx > lt.reach + lt.len || lx < -lt.reach - lt.len) continue;
              const d = roundedRectSDF(lx, ly, lt.len / 2, lt.w / 2, lt.w / 2);
              const core = 1 - sstep(-aa * 0.5, aa * 0.5, d);
              const dd = Math.max(d, 0), gl = Math.exp(-dd * dd / (2 * lt.sig * lt.sig)) * cfg.lightGlow;
              R += sc.lightCol[0] * gl * 0.9; G += sc.lightCol[1] * gl * 0.9; B += sc.lightCol[2] * gl * 0.9;
              A = A + gl * 0.85 * (1 - A);
              R = lerp(R, sc.lightCore[0], core); G = lerp(G, sc.lightCore[1], core); B = lerp(B, sc.lightCore[2], core);
              A = A + core * (1 - A);
            }
          }

          ar += R * A; ag += G * A; ab += B * A; aA += A;   // premultiplied accumulate
        }
        const o = (py * W + px) * 4;
        if (aA > 0) {
          let R = ar / aA, G = ag / aA, B = ab / aA;
          if (cfg.levels > 1) { const q = cfg.levels - 1; R = Math.round(R * q) / q; G = Math.round(G * q) / q; B = Math.round(B * q) / q; }
          data[o] = clamp(R, 0, 1) * 255; data[o + 1] = clamp(G, 0, 1) * 255; data[o + 2] = clamp(B, 0, 1) * 255; data[o + 3] = clamp(aA * inv, 0, 1) * 255;
        }
      }
    }
    return { width: W, height: Hh, data, pixelSize: ps, size: S, cx: g.cx / ps, cy: g.cy / ps };
  }

  // ---------- sheet ----------
  function blit(dst, src, x0, y0) {
    for (let y = 0; y < src.height; y++) {
      const dy = y0 + y; if (dy < 0 || dy >= dst.height) continue;
      for (let x = 0; x < src.width; x++) {
        const dx = x0 + x; if (dx < 0 || dx >= dst.width) continue;
        const si = (y * src.width + x) * 4, di = (dy * dst.width + dx) * 4;
        const a = src.data[si + 3] / 255; if (a <= 0) continue;
        const ia = 1 - a;
        dst.data[di] = src.data[si] * a + dst.data[di] * ia;
        dst.data[di + 1] = src.data[si + 1] * a + dst.data[di + 1] * ia;
        dst.data[di + 2] = src.data[si + 2] * a + dst.data[di + 2] * ia;
        dst.data[di + 3] = Math.min(255, dst.data[di + 3] + a * 255);
      }
    }
  }

  function renderSheet(cfg) {
    cfg = Object.assign({}, DEFAULTS, cfg || {});
    const ps = Math.max(1, cfg.pixelSize | 0);
    const variants = (cfg.sheetVariants || '').split('').filter(v => LIGHTING_PRESETS[v]);
    const lightings = variants.length ? variants.map(v => LIGHTING_PRESETS[v]) : [{}];
    const colors = (cfg.sheetColors && cfg.sheetColors.length) ? cfg.sheetColors : [cfg.color];
    const spriteCfg = Object.assign({}, cfg, { bgOn: false });
    const central = render(spriteCfg, 'central');
    const rows = colors.map(c => lightings.map(l => render(Object.assign({}, spriteCfg, l, { color: c }), 'unit')));
    const gap = Math.round(cfg.sheetGap / ps), m = Math.round(cfg.sheetMargin / ps);
    const uw = rows[0][0].width, uh = rows[0][0].height, cols = lightings.length;
    const rowW = cols * uw + (cols - 1) * gap;
    const W = Math.max(rowW, central.width) + 2 * m;
    const Hh = m + central.height + gap + colors.length * (uh + gap) - gap + m;
    const out = { width: W, height: Hh, data: new Uint8ClampedArray(W * Hh * 4), pixelSize: ps };
    if (cfg.bgOn) {
      const bg = hexToRgb(cfg.bg);
      for (let y = 0; y < Hh; y++) for (let x = 0; x < W; x++) {
        const n = (hashInt(x, y, (cfg.seed | 0) + 1) - 0.5) * cfg.bgNoise, i = (y * W + x) * 4;
        out.data[i] = clamp(bg[0] + n, 0, 1) * 255; out.data[i + 1] = clamp(bg[1] + n, 0, 1) * 255; out.data[i + 2] = clamp(bg[2] + n, 0, 1) * 255; out.data[i + 3] = 255;
      }
    }
    blit(out, central, Math.round((W - central.width) / 2), m);
    let y = m + central.height + gap;
    const x0 = Math.round((W - rowW) / 2);
    for (const row of rows) { row.forEach((sp, i) => blit(out, sp, x0 + i * (uw + gap), y)); y += uh + gap; }
    return out;
  }

  // ---------- browser helper ----------
  function toCanvas(img, canvas, pixelSize) {
    const ps = pixelSize || img.pixelSize || 1;
    const tmp = document.createElement('canvas');
    tmp.width = img.width; tmp.height = img.height;
    tmp.getContext('2d').putImageData(new ImageData(img.data, img.width, img.height), 0, 0);
    canvas.width = img.width * ps; canvas.height = img.height * ps;
    const ctx = canvas.getContext('2d');
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(tmp, 0, 0, canvas.width, canvas.height);
    return canvas;
  }

  // shared helpers, reused by the base module so the noise and blending match exactly
  const util = { clamp, lerp, sstep, hashInt, hexToRgb, patchNoise, smin, roundedRectSDF, blit, dirFromAzEl };
  return { render, renderSheet, toCanvas, DEFAULTS, LIGHTING_PRESETS, PROFILES, util };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = DomeForge;
