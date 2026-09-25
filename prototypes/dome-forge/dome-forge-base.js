/* DomeForgeBase — roads, lunar ground and the full base assembly (2D).
 *
 *   DomeForgeBase.renderRoads(cfg, W, H, prims, scale)   -> {width,height,data}  road layer (transparent elsewhere)
 *   DomeForgeBase.renderGround(cfg, W, H, scale)         -> {width,height,data}  regolith with craters
 *   DomeForgeBase.renderBase(cfg, DomeForge, opts)       -> {width,height,data}  ground + roads + domes
 *
 * Road primitives: {t:'seg', ax,ay,bx,by, w}  {t:'ring', cx,cy,r, w}  {t:'disc', cx,cy,r}
 * Discs are dome rims: they join the road union so the kerbs flow into the rims, but carry no lane line.
 */
const DomeForgeBase = (function () {
  'use strict';
  // helpers come from the engine so noise, blending and colours match the sprites exactly
  const E = (typeof DomeForge !== 'undefined') ? DomeForge : require('./dome-forge-engine.js');
  const { clamp, lerp, sstep, hashInt, hexToRgb, patchNoise, smin, blit, dirFromAzEl } = E.util;
  const DEG = Math.PI / 180;

  // ---------- defaults (px at a 1254 px base; everything scales with the base size) ----------
  const DEFAULTS = {
    baseSize: 1254,
    // roads
    roadW: 30, roadOuterW: 14, curbW: 5, fillet: 20, filletDome: 16,
    roadColor: '#645e57', roadMottle: 0.08, roadGrain: 0.05,
    curbColor: '#a89d90', curbSeg: 9, curbSegDepth: 0.3, curbBevel: 1.4, curbShine: 0.6, curbShadow: 0.5, curbOutline: 0.7,
    bankW: 5, bankLight: 0.28, bankShadow: 0.5, domeShadowW: 6, domeShadow: 0.7,
    laneOn: true, laneColor: '#8f877a', laneW: 1.4, laneDash: 18, laneGap: 6, laneAlpha: 0.6,
    // ground
    groundColor: '#2c2a26', groundMottle: 0.12, groundGrain: 0.07,
    craterBig: 0.9, craterSmall: 0.85, craterDepth: 2.4, craterRim: 1.5,
    baseView: 'full',
    // layout
    orbit: 340, ringRoadR: 485, centralSize: 475, unitSize: 236, offsetY: -42,
    centralColor: '#8d8d8d', cardinalColor: '#1fb75b', diagonalColor: '#8a8a8a', spokesBeyond: false, domeRoads: true,
    unitSockets: true, centralSockets: true, socketsToCentre: true,   // unit: one loop toward the centre, one toward the ring road; central: one per connector
    previewScale: 0.5,
  };

  // ---------- road field ----------
  // returns [dUnion, tAlong, perp, isDisc, dDisc]: tAlong/perp belong to the nearest road primitive, dDisc is the
  // distance outside the nearest dome disc (for the rim shadow on the asphalt)
  function roadField(x, y, prims, F, FD, out) {
    let d = 1e9, best = 1e9, t = 0, perp = 0, isDisc = 0, dRoad = 1e9, dDisc = 1e9;   // dRoad: nearest road primitive, for the lane/kerb parameters
    for (let i = 0; i < prims.length; i++) {
      const p = prims[i];
      let dp, tt = 0, pp = 0;
      if (p.t === 'seg') {
        const px = x - p.ax, py = y - p.ay;
        const h = (px * p.dx + py * p.dy) / p.len;                 // along
        pp = Math.abs(px * p.dy - py * p.dx) / p.len;              // across
        const a = pp - p.w / 2, b = Math.abs(h - p.len / 2) - p.len / 2;   // flat-ended box
        dp = Math.hypot(Math.max(a, 0), Math.max(b, 0)) + Math.min(Math.max(a, b), 0);
        tt = h;
      } else if (p.t === 'ring') {
        const r = Math.hypot(x - p.cx, y - p.cy);
        pp = Math.abs(r - p.r); dp = pp - p.w / 2; tt = Math.atan2(y - p.cy, x - p.cx) * p.r;
      } else { // disc
        dp = Math.hypot(x - p.cx, y - p.cy) - p.r; pp = 1e9; tt = 0; if (dp < dDisc) dDisc = dp;
      }
      if (p.t !== 'disc') { d = d > 1e8 ? dp : smin(d, dp, F); if (dp < dRoad) { dRoad = dp; t = tt; perp = pp; } }
      else { d = d > 1e8 ? dp : smin(d, dp, FD); }
      if (dp < best) { best = dp; isDisc = p.t === 'disc' ? 1 : 0; }
    }
    out[0] = d; out[1] = t; out[2] = perp; out[3] = isDisc; out[4] = dDisc;
    return out;
  }
  function prepPrims(prims) {
    return prims.map(p => {
      if (p.t !== 'seg') return p;
      const dx = p.bx - p.ax, dy = p.by - p.ay, len = Math.hypot(dx, dy) || 1;
      return Object.assign({}, p, { dx, dy, len });
    });
  }

  function renderRoads(cfg, W, H, prims, scale) {
    cfg = Object.assign({}, DEFAULTS, cfg || {});
    scale = scale || 1;
    const P = prepPrims(prims);
    const data = new Uint8ClampedArray(W * H * 4);
    const curbW = cfg.curbW * scale, F = cfg.fillet * scale, FD = cfg.filletDome * scale;
    const road = hexToRgb(cfg.roadColor), curb = hexToRgb(cfg.curbColor), lane = hexToRgb(cfg.laneColor);
    const L = dirFromAzEl(cfg.lightAz === undefined ? 140 : cfg.lightAz, cfg.lightEl === undefined ? 45 : cfg.lightEl);
    const Lm = Math.hypot(L[0], L[1]) || 1, L2x = L[0] / Lm, L2y = L[1] / Lm;
    const seed = (cfg.seed | 0) + 900;
    const f = [0, 0, 0, 0, 0], fx = [0, 0, 0, 0, 0], fy = [0, 0, 0, 0, 0];
    const aa = 1, e = 0.6;
    const segSp = cfg.curbSeg * scale, laneW = cfg.laneW * scale, dash = cfg.laneDash * scale, gap = cfg.laneGap * scale;
    const bankW = cfg.bankW * scale, shW = cfg.domeShadowW * scale;
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const X = x + 0.5, Y = H - (y + 0.5);          // math coords: y up, so the light matches the sprites
      roadField(X, Y, P, F, FD, f);
      const d = f[0];
      if (d > bankW + aa) continue;
      const o = (y * W + x) * 4;
      // outward normal of the outline (needed for the ridge and the embankment)
      roadField(X + e, Y, P, F, FD, fx); roadField(X, Y + e, P, F, FD, fy);
      let gx = fx[0] - d, gy = fy[0] - d; const gl = Math.hypot(gx, gy) || 1; gx /= gl; gy /= gl;
      const facing = gx * L2x + gy * L2y;              // +1: this edge faces the light
      if (d > aa * 0.5) {
        // the road is a raised slab: a lit slope on the side toward the light, a cast shadow on the far side
        const band = 1 - sstep(0, bankW, d);
        const lit = Math.max(0, facing) * cfg.bankLight, dark = Math.max(0, -facing) * cfg.bankShadow;
        const al = band * (lit + dark);
        const v = lit > dark ? 255 : 0;
        data[o] = v; data[o + 1] = v; data[o + 2] = v; data[o + 3] = clamp(al, 0, 1) * 255;
        continue;
      }
      const cov = 1 - sstep(-aa * 0.5, aa * 0.5, d);
      let R, G, B;
      // mottled asphalt
      const mot = (patchNoise(X, Y, 14 * scale, seed, 0.5) - 0.5) * cfg.roadMottle * 2 + (hashInt(x, y, seed + 2) - 0.5) * cfg.roadGrain * 2;
      const k = 1 + mot;
      R = road[0] * k; G = road[1] * k; B = road[2] * k;
      // lane line, dashed, along the nearest road primitive
      if (cfg.laneOn && f[3] === 0 && d < -curbW - 2 * scale) {
        const on = (((f[1] / (dash + gap)) % 1) + 1) % 1 < dash / (dash + gap) ? 1 : 0;
        const la = (1 - sstep(laneW * 0.5 - 0.6, laneW * 0.5 + 0.6, f[2])) * on * cfg.laneAlpha;
        if (la > 0) { R = lerp(R, lane[0], la); G = lerp(G, lane[1], la); B = lerp(B, lane[2], la); }
      }
      // the dome rims sit above the road and throw a shadow onto it
      if (shW > 0 && f[4] > 0 && f[4] < shW) { const sm = 1 - cfg.domeShadow * (1 - sstep(0, shW, f[4])); R *= sm; G *= sm; B *= sm; }
      // kerb: a rounded ridge along the outline. Its lit face is the outer side on edges that face the
      // light and the inner side on edges that face away, so both kerbs of a street read the same.
      if (d > -curbW - 3 * scale) {
        const u = clamp(-d / curbW, 0, 1);            // 0 at the outer edge, 1 at the asphalt
        const slope = cfg.curbBevel * Math.PI * Math.cos(Math.PI * u) / curbW;
        let nx = slope * gx, ny = slope * gy;
        let segMul = 1;
        if (segSp > 0 && cfg.curbSegDepth > 0) {
          const fr = ((f[1] / segSp) % 1 + 1) % 1, dEdge = Math.min(fr, 1 - fr) * segSp;
          const line = 1 - sstep(0.3 * scale, 1.0 * scale, dEdge);
          segMul = 1 - cfg.curbSegDepth * 0.55 * line;
          const tilt = Math.sin(2 * Math.PI * fr) * cfg.curbSegDepth * 0.25;
          nx += -gy * tilt; ny += gx * tilt;
        }
        const nl = Math.hypot(nx, ny, 1); nx /= nl; ny /= nl; const nz = 1 / nl;
        const ndl = Math.max(0, nx * L[0] + ny * L[1] + nz * L[2]);
        const hx = L[0], hy = L[1], hz = L[2] + 1, hl = Math.hypot(hx, hy, hz);
        const ndh = Math.max(0, (nx * hx + ny * hy + nz * hz) / hl);
        const spec = Math.pow(ndh, 14) * cfg.curbShine;
        const grain = (patchNoise(X + 11, Y - 7, 6 * scale, seed + 5, 0.5) - 0.5) * 0.18 + (hashInt(x, y, seed + 6) - 0.5) * 0.06;
        const kc = (0.42 + 0.7 * ndl + grain) * segMul;
        let cr = curb[0] * kc + spec, cg = curb[1] * kc + spec, cb = curb[2] * kc + spec;
        // inked contour along the outer edge
        const ol = 1 - cfg.curbOutline * (1 - sstep(0.2 * scale, 0.9 * scale, -d));
        cr *= ol; cg *= ol; cb *= ol;
        const ca = sstep(-curbW - 0.5, -curbW + 0.5, d);       // kerb coverage (1 on the kerb, 0 on the asphalt)
        // shadow the kerb throws on the asphalt, only where its inner side faces away from the light
        const sh = 1 - cfg.curbShadow * sstep(-curbW - 3 * scale, -curbW, d) * (1 - ca) * clamp(0.5 + 0.5 * facing, 0, 1);
        R = lerp(R * sh, cr, ca); G = lerp(G * sh, cg, ca); B = lerp(B * sh, cb, ca);
      }
      data[o] = clamp(R, 0, 1) * 255; data[o + 1] = clamp(G, 0, 1) * 255; data[o + 2] = clamp(B, 0, 1) * 255; data[o + 3] = cov * 255;
    }
    return { width: W, height: H, data };
  }

  // ---------- lunar ground ----------
  function renderGround(cfg, W, H, scale) {
    cfg = Object.assign({}, DEFAULTS, cfg || {});
    scale = scale || 1;
    const data = new Uint8ClampedArray(W * H * 4);
    const base = hexToRgb(cfg.groundColor);
    const L = dirFromAzEl(cfg.lightAz === undefined ? 140 : cfg.lightAz, cfg.lightEl === undefined ? 45 : cfg.lightEl);
    const seed = (cfg.seed | 0) + 500;
    const layers = [
      { cell: 130 * scale, prob: cfg.craterBig, rMin: 16 * scale, rMax: 58 * scale, seed: seed + 10 },
      { cell: 46 * scale, prob: cfg.craterSmall, rMin: 3.5 * scale, rMax: 11 * scale, seed: seed + 20 },
      { cell: 16 * scale, prob: cfg.craterSmall * 0.6, rMin: 1.2 * scale, rMax: 3.5 * scale, seed: seed + 30 },
    ];
    const depth = cfg.craterDepth, rimH = cfg.craterRim;
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const X = x + 0.5, Y = H - (y + 0.5);
      // soft large-scale mottling + fine grain
      const mot = (patchNoise(X, Y, 90 * scale, seed, 0.6) - 0.5) * cfg.groundMottle * 2 + (patchNoise(X + 300, Y, 22 * scale, seed + 1, 0.6) - 0.5) * cfg.groundMottle
        + (patchNoise(X - 150, Y + 90, 4 * scale, seed + 3, 0.4) - 0.5) * cfg.groundGrain * 1.5 + (hashInt(x, y, seed + 2) - 0.5) * cfg.groundGrain * 2;
      let nx = 0, ny = 0, ao = 0;
      // craters: bowl with a raised rim; the nearest crater wins in each layer
      for (let li = 0; li < layers.length; li++) {
        const ly = layers[li];
        const gx = Math.floor(X / ly.cell), gy = Math.floor(Y / ly.cell);
        let bu = 1e9, bR = 0, bdx = 0, bdy = 0;
        for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) {
          const cx = gx + i, cy = gy + j;
          if (hashInt(cx, cy, ly.seed) > ly.prob) continue;
          const jx = (cx + hashInt(cx, cy, ly.seed + 1)) * ly.cell, jy = (cy + hashInt(cx, cy, ly.seed + 2)) * ly.cell;
          const Rr = lerp(ly.rMin, ly.rMax, Math.pow(hashInt(cx, cy, ly.seed + 3), 2.2));
          const dx = X - jx, dy = Y - jy, u = Math.hypot(dx, dy) / Rr;
          if (u < bu) { bu = u; bR = Rr; bdx = dx; bdy = dy; }
        }
        if (bu < 1.35) {
          // height profile h(u): bowl −depth·(1−u²) inside, raised rim around u≈1
          const rr = Math.hypot(bdx, bdy) || 1e-6;
          const dirx = bdx / rr, diry = bdy / rr;
          const rimW = 0.18, g = Math.exp(-((bu - 1.02) * (bu - 1.02)) / (2 * rimW * rimW));
          // slope dh/dr (self-similar: independent of crater size): parabolic bowl + gaussian rim
          const s = (bu < 1 ? 0.5 * depth * bu : 0) + 0.08 * rimH * (-(bu - 1.02) / (rimW * rimW)) * g;
          nx += -s * dirx; ny += -s * diry;
          if (bu < 1) ao += 0.3 * (1 - bu * bu) * depth;
        }
      }
      const nl = Math.hypot(nx, ny, 1);
      const ndl = Math.max(0, (nx * L[0] + ny * L[1] + L[2]) / nl);
      const k = (0.45 + 0.75 * ndl) * (1 - ao) * (1 + mot);
      const o = (y * W + x) * 4;
      data[o] = clamp(base[0] * k, 0, 1) * 255; data[o + 1] = clamp(base[1] * k, 0, 1) * 255; data[o + 2] = clamp(base[2] * k, 0, 1) * 255; data[o + 3] = 255;
    }
    return { width: W, height: H, data };
  }

  // ---------- assembly ----------
  // Layout in math coordinates (origin at the centre, y up), all in px at the given scale.
  function layout(cfg, scale) {
    const A = cfg.baseSize * scale, s = A / 1254;
    const c = { x: A / 2, y: A / 2 - (cfg.offsetY || 0) * s };
    const domes = [];
    const centralSize = cfg.centralSize * s, unitSize = cfg.unitSize * s;
    const K = cfg.central, U = cfg.unit;
    const centralRout = (K.domeRadius + K.ringWidth) * centralSize;
    const unitRout = (U.domeRadius + U.ringWidth) * unitSize;
    domes.push({ kind: 'central', x: c.x, y: c.y, size: centralSize, rout: centralRout, angle: 0 });
    const orbit = cfg.orbit * s, ringR = cfg.ringRoadR * s;
    const prims = [];
    for (let i = 0; i < 8; i++) {
      const ang = 90 + 45 * i, ca = Math.cos(ang * DEG), sa = Math.sin(ang * DEG);
      const cardinal = i % 2 === 0;
      const dx = c.x + orbit * ca, dy = c.y + orbit * sa;
      domes.push({ kind: 'unit', x: dx, y: dy, size: unitSize, rout: unitRout, angle: ang, color: cardinal ? cfg.cardinalColor : cfg.diagonalColor });
      if (cfg.domeRoads) {
        prims.push({ t: 'seg', ax: c.x, ay: c.y, bx: dx, by: dy, w: cfg.roadW * s });                 // centre -> dome
        prims.push({ t: 'seg', ax: dx, ay: dy, bx: c.x + ringR * ca, by: c.y + ringR * sa, w: cfg.roadW * s });   // dome -> ring
      }
      if (cardinal && cfg.spokesBeyond) prims.push({ t: 'seg', ax: c.x + ringR * ca, ay: c.y + ringR * sa, bx: c.x + A * ca, by: c.y + A * sa, w: cfg.roadOuterW * s });
    }
    prims.push({ t: 'ring', cx: c.x, cy: c.y, r: ringR, w: cfg.roadW * s });
    for (const d of domes) prims.push({ t: 'disc', cx: d.x, cy: d.y, r: d.rout - 0.5 * s });
    return { A, s, domes, prims };
  }

  function renderBase(cfg, engine, opts) {
    cfg = Object.assign({}, DEFAULTS, cfg || {});
    opts = opts || {};
    const scale = opts.scale || 1;
    const lay = layout(cfg, scale);
    const W = Math.round(lay.A), H = W;
    const ground = renderGround(cfg, W, H, scale * cfg.baseSize / 1254);
    const roads = renderRoads(cfg, W, H, lay.prims, scale * cfg.baseSize / 1254);
    blit(ground, roads, 0, 0);
    if (engine && opts.domes !== false) {
      const cache = opts.cache || {};
      const unitSize = lay.domes.find(d => d.kind === 'unit') ? lay.domes.find(d => d.kind === 'unit').size : cfg.unitSize * lay.s;
      for (const d of lay.domes) {
        const size = Math.max(32, Math.round(d.size));
        const spriteCfg = Object.assign({}, cfg, { bgOn: false });
        if (d.kind === 'unit') {
          spriteCfg.color = d.color;
          spriteCfg.socketOn = cfg.socketOn && cfg.unitSockets;
          // two loops: one where the connector road from the centre plugs in, one toward the ring road
          spriteCfg.unit = Object.assign({}, cfg.unit, { size, socketCount: 2, socketStart: cfg.socketsToCentre ? (d.angle + 180) % 360 : cfg.unit.socketStart });
        } else {
          spriteCfg.color = cfg.centralColor || cfg.color;
          spriteCfg.socketOn = cfg.socketOn && cfg.centralSockets;
          // one loop per connector road, aimed at the unit domes. Socket size is derived from the unit
          // sprite size, so pass the unit size used in this base (scaled) or the loops come out too big.
          spriteCfg.unit = Object.assign({}, cfg.unit, { size: Math.max(32, Math.round(unitSize)) });
          spriteCfg.central = Object.assign({}, cfg.central, { size, socketCount: 8, socketStart: 90 });
        }
        const key = d.kind + ':' + size + ':' + spriteCfg.color + ':' + spriteCfg.socketOn + ':' + (d.kind === 'unit' ? spriteCfg.unit.socketStart : '');
        let img = cache[key];
        if (!img) { img = engine.render(spriteCfg, d.kind); cache[key] = img; }
        // the sprite's dome centre is at (g.cx, g.cy) inside the sprite; we centred on the ring so use the sprite centre
        const cx = img.cx !== undefined ? img.cx : img.width / 2, cy = img.cy !== undefined ? img.cy : img.height / 2;
        blit(ground, img, Math.round(d.x - cx), Math.round(H - d.y - cy));
      }
    }
    return ground;
  }

  return { renderRoads, renderGround, renderBase, layout, DEFAULTS };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = DomeForgeBase;
