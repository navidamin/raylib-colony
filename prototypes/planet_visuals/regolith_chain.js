"use strict";
/* ===== CHAIN BEGIN =====================================================
   src/TerrainGen/terrain_synthesis.cpp, in JavaScript.
   ---------------------------------------------------------------------
   Ported function for function, not "inspired by": CropMacro,
   SharpenAdaptive, ValueNoise, Fbm, GrainNoise, Hillshade, CastShadows,
   SprinkleBoulders, TextureModulate, RampColor and the three-level chain
   are the same arithmetic with the same constants. Anything that differs
   is marked NEW and is the point of the bench.

   No DOM below this line: the headless renderer
   (regolith_craters_render.mjs) slices this block out of the file and
   runs it in Node, so what it writes to PNG is what the page draws.
   ===================================================================== */
const TerrainChain = (function(){

const DEG2RAD = Math.PI / 180.0;
const MOON_KM_PER_DEG = 30.32268;              // pi * 1737.4 / 180

/* --- float fields -------------------------------------------------- */

// The exact separable gaussian. O(w*h*sigma), which is why the wide
// blurs do not call it directly.
function GaussianBlurExact(a, w, h, sigma){
  if (sigma <= 0.05) return;
  const radius = Math.ceil(sigma * 3.0);
  const kernel = new Float32Array(2 * radius + 1);
  let norm = 0.0;
  for (let i = -radius; i <= radius; i++){
    const v = Math.exp(-0.5 * (i * i) / (sigma * sigma));
    kernel[i + radius] = v; norm += v;
  }
  for (let i = 0; i < kernel.length; i++) kernel[i] /= norm;
  const tmp = new Float32Array(a.length);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++){
      let acc = 0.0;
      for (let i = -radius; i <= radius; i++){
        let xi = x + i; if (xi < 0) xi = 0; else if (xi > w - 1) xi = w - 1;
        acc += a[y * w + xi] * kernel[i + radius];
      }
      tmp[y * w + x] = acc;
    }
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++){
      let acc = 0.0;
      for (let i = -radius; i <= radius; i++){
        let yi = y + i; if (yi < 0) yi = 0; else if (yi > h - 1) yi = h - 1;
        acc += tmp[yi * w + x] * kernel[i + radius];
      }
      a[y * w + x] = acc;
    }
}

// A wide blur is a low-pass by definition, so decimate, run the real
// kernel small, and bilinear back up. Work falls by about f^3.
function GaussianBlur(a, w, h, sigma){
  if (sigma <= 0.05) return;
  if (sigma < 3.0){ GaussianBlurExact(a, w, h, sigma); return; }
  let f = (sigma / 2.0) | 0;
  while (f > 1 && ((w / f) < 32 || (h / f) < 32)) f--;
  if (f <= 1){ GaussianBlurExact(a, w, h, sigma); return; }
  const dw = (w / f) | 0, dh = (h / f) | 0;
  const small = new Float32Array(dw * dh);
  for (let y = 0; y < dh; y++)
    for (let x = 0; x < dw; x++){
      let acc = 0.0, n = 0;
      for (let j = 0; j < f; j++){
        const sy = y * f + j; if (sy >= h) break;
        for (let i = 0; i < f; i++){
          const sx = x * f + i; if (sx >= w) break;
          acc += a[sy * w + sx]; n++;
        }
      }
      small[y * dw + x] = n > 0 ? acc / n : 0.0;
    }
  GaussianBlurExact(small, dw, dh, sigma / f);
  for (let y = 0; y < h; y++){
    const gy = ((y + 0.5) / f) - 0.5;
    let y0 = Math.floor(gy); const ty = gy - y0;
    let y1 = Math.min(Math.max(y0 + 1, 0), dh - 1);
    y0 = Math.min(Math.max(y0, 0), dh - 1);
    for (let x = 0; x < w; x++){
      const gx = ((x + 0.5) / f) - 0.5;
      let x0 = Math.floor(gx); const tx = gx - x0;
      let x1 = Math.min(Math.max(x0 + 1, 0), dw - 1);
      x0 = Math.min(Math.max(x0, 0), dw - 1);
      const v00 = small[y0 * dw + x0], v10 = small[y0 * dw + x1];
      const v01 = small[y1 * dw + x0], v11 = small[y1 * dw + x1];
      a[y * w + x] = (v00 * (1 - tx) + v10 * tx) * (1 - ty)
                   + (v01 * (1 - tx) + v11 * tx) * ty;
    }
  }
}

/* --- the world frame ------------------------------------------------
   Invented detail is a function of GROUND, not of the window that
   frames it: two windows over the same site have to invent the same
   rock, or ground a player judged changes when they come back to it.
   ------------------------------------------------------------------ */

const NOISE_GRAIN      = 0x9E3779B9;
const NOISE_GRAIN_FINE = 0x85EBCA6B;
const NOISE_UNDULATION = 0xC2B2AE35;
const NOISE_SPECKLE    = 0x27D4EB2F;
const NOISE_BOULDER    = 0xD3A2646C;

function MakeNoiseFrame(latDeg, lonDeg, spanKm, res, salt){
  const latSpanDeg = spanKm / MOON_KM_PER_DEG;
  const c = Math.max(0.2, Math.cos(latDeg * DEG2RAD));
  const lonSpanDeg = latSpanDeg / c;
  return {
    lat0Deg: latDeg + latSpanDeg * 0.5,
    dLatPerPx: -latSpanDeg / res,
    lon0Deg: lonDeg - lonSpanDeg * 0.5,
    dLonPerPx: lonSpanDeg / res,
    kmPerPx: spanKm / res,
    salt: salt >>> 0
  };
}
// The cos belongs to the PIXEL, not to the window: taking it once at
// the centre slides the whole lattice sideways when the window moves.
function FrameWorldKm(f, x, y, out){
  const lat = f.lat0Deg + y * f.dLatPerPx;
  const lon = f.lon0Deg + x * f.dLonPerPx;
  out[1] = lat * MOON_KM_PER_DEG;                          // v
  out[0] = lon * MOON_KM_PER_DEG * Math.cos(lat * DEG2RAD); // u
}
// One value per world lattice cell, 0..1. The same cell hashes the same
// however it is reached, which is the whole point.
function HashCell(cx, cy, salt){
  let h = (Math.imul(cx | 0, 73856093) ^ Math.imul(cy | 0, 19349663)
         ^ Math.imul(salt | 0, 83492791)) >>> 0;
  h = (h ^ (h >>> 16)) >>> 0; h = Math.imul(h, 0x45D9F3B) >>> 0;
  h = (h ^ (h >>> 16)) >>> 0; h = Math.imul(h, 0x45D9F3B) >>> 0;
  h = (h ^ (h >>> 16)) >>> 0;
  return (h >>> 8) * (1.0 / 16777216.0);
}

function ValueNoise(res, scale, frame, octaveSalt){
  scale = Math.max(2, scale | 0);
  const cellKm = Math.max(1e-12, scale * frame.kmPerPx);
  const p = [0, 0];
  let uMin = Infinity, uMax = -Infinity, vMin = Infinity, vMax = -Infinity;
  for (const [cx, cy] of [[0,0],[res,0],[0,res],[res,res]]){
    FrameWorldKm(frame, cx, cy, p);
    uMin = Math.min(uMin, p[0]); uMax = Math.max(uMax, p[0]);
    vMin = Math.min(vMin, p[1]); vMax = Math.max(vMax, p[1]);
  }
  const i0 = Math.floor(uMin / cellKm), j0 = Math.floor(vMin / cellKm);
  const gw = Math.max(2, Math.floor(uMax / cellKm) - i0 + 3);
  const gh = Math.max(2, Math.floor(vMax / cellKm) - j0 + 3);
  const grid = new Float32Array(gw * gh);
  const salt = (frame.salt ^ octaveSalt) >>> 0;
  for (let j = 0; j < gh; j++)
    for (let i = 0; i < gw; i++)
      grid[j * gw + i] = HashCell(i0 + i, j0 + j, salt);

  const rowV = new Float64Array(res), rowCos = new Float64Array(res),
        colLonKm = new Float64Array(res);
  for (let y = 0; y < res; y++){
    const lat = frame.lat0Deg + (y + 0.5) * frame.dLatPerPx;
    rowV[y] = lat * MOON_KM_PER_DEG / cellKm - j0;
    rowCos[y] = Math.cos(lat * DEG2RAD);
  }
  for (let x = 0; x < res; x++)
    colLonKm[x] = (frame.lon0Deg + (x + 0.5) * frame.dLonPerPx)
                  * MOON_KM_PER_DEG / cellKm;

  const out = new Float32Array(res * res);
  for (let y = 0; y < res; y++){
    const gy = rowV[y];
    const jy = Math.min(Math.max(Math.floor(gy), 0), gh - 2);
    const ty = gy - jy, cosLat = rowCos[y];
    for (let x = 0; x < res; x++){
      const gx = colLonKm[x] * cosLat - i0;
      const ix = Math.min(Math.max(Math.floor(gx), 0), gw - 2);
      const tx = gx - ix;
      const r0 = jy * gw + ix, r1 = (jy + 1) * gw + ix;
      const a = grid[r0] + (grid[r0 + 1] - grid[r0]) * tx;
      const b = grid[r1] + (grid[r1 + 1] - grid[r1]) * tx;
      out[y * res + x] = a + (b - a) * ty;
    }
  }
  GaussianBlur(out, res, res, scale * 0.45);
  return out;
}

function Fbm(res, octaves, baseScale, persistence, frame){
  const out = new Float32Array(res * res);
  let amp = 1.0, norm = 0.0, scale = baseScale;
  for (let o = 0; o < octaves; o++){
    const n = ValueNoise(res, scale, frame, Math.imul(0x9E3779B9, o + 1) >>> 0);
    for (let i = 0; i < out.length; i++) out[i] += amp * n[i];
    norm += amp; amp *= persistence; scale = Math.max(2, scale >> 1);
  }
  for (let i = 0; i < out.length; i++) out[i] /= norm;
  return out;
}

function NormalizeField(g){
  let mean = 0.0;
  for (let i = 0; i < g.length; i++) mean += g[i];
  mean /= g.length;
  let varr = 0.0;
  for (let i = 0; i < g.length; i++){ const d = g[i] - mean; varr += d * d; }
  let sd = Math.sqrt(varr / g.length);
  if (sd < 1e-6) sd = 1.0;
  for (let i = 0; i < g.length; i++) g[i] = (g[i] - mean) / sd;
}

// Zero-mean unit-std regolith grain. Plain FBM buries its variance in
// smooth blobs; pink noise carries equal energy per octave down to the
// pixel, so a per-pixel component is mixed in -- without it the ground
// renders flat.
function GrainNoise(res, frame){
  const coarse = Object.assign({}, frame);
  coarse.salt = (frame.salt ^ NOISE_GRAIN) >>> 0;
  const g = Fbm(res, 5, 64, 0.8, coarse);
  NormalizeField(g);
  const fine = new Float32Array(res * res);
  const cellKm = Math.max(1e-12, frame.kmPerPx);
  const saltFine = (frame.salt ^ NOISE_GRAIN_FINE) >>> 0;
  const p = [0, 0];
  for (let y = 0; y < res; y++)
    for (let x = 0; x < res; x++){
      FrameWorldKm(frame, x + 0.5, y + 0.5, p);
      fine[y * res + x] = HashCell(Math.floor(p[0] / cellKm),
                                   Math.floor(p[1] / cellKm), saltFine) - 0.5;
    }
  GaussianBlur(fine, res, res, 0.5);
  NormalizeField(fine);
  for (let i = 0; i < g.length; i++) g[i] = 0.55 * g[i] + 0.75 * fine[i];
  NormalizeField(g);
  return g;
}

/* --- light ----------------------------------------------------------
   Sun azimuth and altitude are hard constants in the C++ (315 NW, 35).
   NEW: levers here, because a crater is read almost entirely by the
   shadow it throws, and a bench that cannot move the sun cannot show
   that.
   ------------------------------------------------------------------ */
function Hillshade(height, res, zFactor, smoothPx, sunAzDeg, sunAltDeg){
  const h = Float32Array.from(height);
  GaussianBlur(h, res, res, smoothPx);
  const az = (360.0 - sunAzDeg + 90.0) * DEG2RAD;
  const alt = sunAltDeg * DEG2RAD;
  const sinAlt = Math.sin(alt), cosAlt = Math.cos(alt);
  const out = new Float32Array(res * res);
  for (let y = 0; y < res; y++){
    const ym = Math.max(0, y - 1), yp = Math.min(res - 1, y + 1);
    for (let x = 0; x < res; x++){
      const xm = Math.max(0, x - 1), xp = Math.min(res - 1, x + 1);
      const dy = (h[yp * res + x] - h[ym * res + x]) * zFactor / (yp - ym);
      const dx = (h[y * res + xp] - h[y * res + xm]) * zFactor / (xp - xm);
      const slope = Math.atan(Math.hypot(dx, dy));
      const aspect = Math.atan2(dy, -dx);
      const v = Math.cos(slope) * sinAlt
              + Math.sin(slope) * cosAlt * Math.cos(az - aspect);
      out[y * res + x] = Math.min(Math.max(v, 0.0), 1.0);
    }
  }
  return out;
}

// Horizon ray-march toward the sun: 1 lit, 0 blocked. This is what puts
// a crater floor in the dark instead of merely shading its far wall.
function CastShadows(height, res, zFactor, maxDistPx, stepPx, sunAzDeg, sunAltDeg, blurPx){
  const az = (360.0 - sunAzDeg + 90.0) * DEG2RAD;
  const sx = Math.cos(az), syImage = -Math.sin(az);
  const tanAlt = Math.tan(sunAltDeg * DEG2RAD);
  const nSteps = (maxDistPx / stepPx) | 0;
  const light = new Float32Array(res * res);
  // The sample offsets are the same for every pixel, so the step loop
  // goes OUTSIDE the pixel loops. Each pass is then a shifted, stride-1
  // read of two rows instead of a diagonal walk that leaves the cache on
  // every sample -- at res 1498 one step of that walk jumped 6 kB.
  //
  // Identical arithmetic, not an approximation: floor(x + o) = x +
  // floor(o) for integer x, and where the two differ from the old
  // truncation -- only at negative coordinates -- both were clamped to
  // the same edge pixel anyway.
  const hz = new Float32Array(res * res);
  for (let i = 0; i < hz.length; i++) hz[i] = height[i] * zFactor;
  const maxBlock = new Float32Array(res * res).fill(-1e9);
  for (let s = 1; s <= nSteps; s++){
    const dist = s * stepPx;
    const dx = Math.floor(sx * dist), dy = Math.floor(syImage * dist);
    const inv = 1.0 / dist;
    // Where the shifted row falls inside the image the inner loop needs
    // no bounds test at all; the two clamped ends are walked separately.
    const x0 = Math.min(res, Math.max(0, -dx));
    const x1 = Math.min(res, Math.max(0, res - dx));
    for (let y = 0; y < res; y++){
      const syp = y + dy < 0 ? 0 : (y + dy > res - 1 ? res - 1 : y + dy);
      const srow = syp * res, drow = y * res;
      for (let x = 0; x < x0; x++){
        const bs = (hz[srow] - hz[drow + x]) * inv;
        if (bs > maxBlock[drow + x]) maxBlock[drow + x] = bs;
      }
      for (let x = x0; x < x1; x++){
        const bs = (hz[srow + x + dx] - hz[drow + x]) * inv;
        if (bs > maxBlock[drow + x]) maxBlock[drow + x] = bs;
      }
      for (let x = x1; x < res; x++){
        const bs = (hz[srow + res - 1] - hz[drow + x]) * inv;
        if (bs > maxBlock[drow + x]) maxBlock[drow + x] = bs;
      }
    }
  }
  const band = tanAlt * 0.35;
  for (let i = 0; i < light.length; i++)
    light[i] = 1.0 - Math.min(Math.max((maxBlock[i] - tanAlt) / band, 0.0), 1.0);
  GaussianBlur(light, res, res, blurPx === undefined ? 0.8 : blurPx);
  for (let i = 0; i < light.length; i++)
    light[i] = Math.min(Math.max(light[i], 0.0), 1.0);
  return light;
}

/* --- the imagery ----------------------------------------------------
   CropMacro, against the embedded block instead of the whole 8192x4096
   mosaic. The texel arithmetic is the global one, so a pixel maps to
   the same ground the C++ would give it; the block is only where those
   texels are read from. `escaped` tells the caller the window has
   walked off the carried imagery.
   ------------------------------------------------------------------ */
function CropMacro(block, latDeg, lonDeg, spanDeg, res, report){
  const W = block.wacW, H = block.wacH;
  const c = Math.max(0.2, Math.cos(latDeg * DEG2RAD));
  const lonSpan = spanDeg / c;
  const lat0 = latDeg - spanDeg / 2.0, lat1 = latDeg + spanDeg / 2.0;
  const lon0 = lonDeg - lonSpan / 2.0, lon1 = lonDeg + lonSpan / 2.0;
  const MARGIN = 2;                      // bilinear needs a neighbour past each edge
  const y0 = Math.floor((90.0 - lat1) / 180.0 * H) - MARGIN;
  const y1 = Math.floor((90.0 - lat0) / 180.0 * H) + 1 + MARGIN;
  const x0 = Math.floor((lon0 + 180.0) / 360.0 * W) - MARGIN;
  const x1 = Math.floor((lon1 + 180.0) / 360.0 * W) + 1 + MARGIN;
  const cw = Math.max(2, x1 - x0), ch = Math.max(2, y1 - y0);
  const crop = new Float32Array(cw * ch);
  let escaped = false;
  for (let y = 0; y < ch; y++){
    let by = y0 + y - block.y0;
    if (by < 0 || by >= block.h){ escaped = true; by = Math.min(Math.max(by, 0), block.h - 1); }
    for (let x = 0; x < cw; x++){
      let bx = x0 + x - block.x0;
      if (bx < 0 || bx >= block.w){ escaped = true; bx = Math.min(Math.max(bx, 0), block.w - 1); }
      crop[y * cw + x] = block.data[by * block.w + bx] * (1.0 / 255.0);
    }
  }
  if (report) report.escaped = escaped;
  GaussianBlur(crop, cw, ch, 0.7);       // denoise JPEG artifacts

  // Sample at each output pixel's OWN lat/lon rather than stretching the
  // block edge to edge: stretching makes pixel->ground depend on where
  // the texels happened to fall, and two windows over the same site then
  // disagree by up to half a texel.
  const out = new Float32Array(res * res);
  const tx0 = (lon0 + 180.0) / 360.0 * W - 0.5 - x0;
  const dtx = (lonSpan / res) / 360.0 * W;
  const ty0 = (90.0 - lat1) / 180.0 * H - 0.5 - y0;
  const dty = (spanDeg / res) / 180.0 * H;
  for (let y = 0; y < res; y++){
    const fy = ty0 + (y + 0.5) * dty;
    const iy = Math.min(Math.max(Math.floor(fy), 0), ch - 2);
    const wy = Math.min(Math.max(fy - iy, 0.0), 1.0);
    for (let x = 0; x < res; x++){
      const fx = tx0 + (x + 0.5) * dtx;
      const ix = Math.min(Math.max(Math.floor(fx), 0), cw - 2);
      const wx = Math.min(Math.max(fx - ix, 0.0), 1.0);
      const r0 = iy * cw + ix, r1 = (iy + 1) * cw + ix;
      const a = crop[r0] + (crop[r0 + 1] - crop[r0]) * wx;
      const b = crop[r1] + (crop[r1 + 1] - crop[r1]) * wx;
      out[y * res + x] = a + (b - a) * wy;
    }
  }
  return out;
}

// Unsharp + adaptive contrast around the crop's own midpoint. The gain
// is capped: maria must stay dark, calm plains.
function SharpenAdaptive(macro, res){
  const k = res / 300.0;
  const blur = Float32Array.from(macro);
  GaussianBlur(blur, res, res, 5.0 * k);
  for (let i = 0; i < macro.length; i++)
    macro[i] = Math.min(Math.max(macro[i] + 0.40 * (macro[i] - blur[i]), 0.0), 1.0);
  const sorted = Float32Array.from(macro).sort();
  const pLo = sorted[(sorted.length * 0.02) | 0];
  const pHi = sorted[(sorted.length * 0.98) | 0];
  const spread = Math.max(pHi - pLo, 1e-4);
  const gain = Math.min(2.2, Math.max(1.0, 0.60 / spread));
  const mid = 0.5 * (pHi + pLo);
  for (let i = 0; i < macro.length; i++)
    macro[i] = Math.min(Math.max(mid + (macro[i] - mid) * gain, 0.0), 1.0);
}

// One boulder per world cell, on a hashed pixel inside it -- the only
// scheme that puts a boulder on the same rock twice.
function SprinkleBoulders(height, res, frame, count, amp){
  if (count <= 0) return;
  const cellPx = Math.max(2, Math.round(res / Math.sqrt(count)));
  const cellKm = Math.max(1e-12, cellPx * frame.kmPerPx);
  const p = [0, 0];
  let uMin = Infinity, uMax = -Infinity, vMin = Infinity, vMax = -Infinity;
  for (const [cx, cy] of [[0,0],[res,0],[0,res],[res,res]]){
    FrameWorldKm(frame, cx, cy, p);
    uMin = Math.min(uMin, p[0]); uMax = Math.max(uMax, p[0]);
    vMin = Math.min(vMin, p[1]); vMax = Math.max(vMax, p[1]);
  }
  const i0 = Math.floor(uMin / cellKm), j0 = Math.floor(vMin / cellKm);
  const i1 = Math.floor(uMax / cellKm) + 1, j1 = Math.floor(vMax / cellKm) + 1;
  const salt = (frame.salt ^ NOISE_BOULDER) >>> 0;
  for (let cj = j0; cj <= j1; cj++)
    for (let ci = i0; ci <= i1; ci++){
      const u = (ci + HashCell(ci, cj, salt)) * cellKm;
      const v = (cj + HashCell(ci, cj, (salt ^ 0x5BD1E995) >>> 0)) * cellKm;
      const lat = v / MOON_KM_PER_DEG;
      const cosLat = Math.cos(lat * DEG2RAD);
      if (Math.abs(cosLat) < 1e-6) continue;
      const lon = u / (MOON_KM_PER_DEG * cosLat);
      const y = Math.round((lat - frame.lat0Deg) / frame.dLatPerPx - 0.5);
      const x = Math.round((lon - frame.lon0Deg) / frame.dLonPerPx - 0.5);
      if (x < 1 || y < 1 || x >= res - 1 || y >= res - 1) continue;
      const a = amp * (0.4 + HashCell(ci, cj, (salt ^ 0x27220A95) >>> 0));
      height[y * res + x] += a;
      if (HashCell(ci, cj, (salt ^ 0x165667B1) >>> 0) < 0.5)
        height[y * res + x + 1] += a * 0.6;
      if (HashCell(ci, cj, (salt ^ 0x9E3779B1) >>> 0) < 0.3)
        height[(y + 1) * res + x] += a * 0.5;
    }
}

/* --- NEW: the craters ------------------------------------------------
   The Layer Block's Bowl(), which is

       floor = r < 1 ? -(1 - r*r) : 0
       rim   = rimHeight * exp(-(r-1)^2 / (2 * 0.18^2))

   -- a parabolic excavation with a gaussian ring on it -- put into this
   chain's height field. Three things had to be added for it to mean
   anything on real ground:

   1. SCALE. The block bench's crater is a fraction of the block and its
      depth is in the block's own metres. Here a crater is an object on
      the moon: a diameter in km and a position in km east/south of the
      region centre, turned into lat/lon once. Every level then projects
      it through its own frame, so ONE crater is the same rock at
      100 km, at 25 km and at 5 km -- which is the property the whole
      chain is built around and the reason this is not just a decal.

   2. DEPTH FROM DIAMETER. d/D 0.03 (ancient, gardened flat) to 0.20
      (fresh) -- the same law lola_dem.cpp's DetailCraterField uses, so
      the two synthesizers do not disagree about how deep a 2 km crater
      is. `fresh` picks the point on that line and also scales the rim,
      because an old crater has lost its rim, not just its depth.

   3. A FLAT FLOOR. Real lunar craters are flat-floored with a smooth
      wall and a barely-raised rim (prototypes/planet_visuals/README.md,
      cross-checked against LRO imagery); the pure paraboloid is a
      cone-ish dish. floorFlat 0 IS the Layer Block bowl, exactly; turn
      it up and the same expression walks to the measured profile.

   Deepest-wins for the excavation, rims accumulate: without that a
   saturated field digs runaway pits wherever two craters overlap.
   -------------------------------------------------------------------- */

// The rim, and the apron that continues it outward.
//
// These used to be two terms: a gaussian crest plus, only for r > 1, an
// ejecta skirt worth 0.35 of the rim. That skirt STARTED at 0.35 -- a
// step, right at r = 1 -- and a hillshade differentiates, so every
// crater in the population got a drawn one-pixel circle around it. The
// two are one function here: gaussian into the bowl, and outside it a
// smoothstep falloff that is 1 at the crest and 0 at the apron's edge,
// with zero slope at both ends. Continuous in value AND slope, which is
// what the light actually reads.
const EJECTA_SHARE = 0.35;
function RimRise(t, invW, ejectaKm){
  const g = Math.exp(-t * t * invW);
  if (t <= 0.0 || ejectaKm <= 0.001) return g;
  const x = Math.min(1.0, t / ejectaKm);
  const sf = 1.0 - x * x * (3.0 - 2.0 * x);
  return (1.0 - EJECTA_SHARE) * g + EJECTA_SHARE * sf;
}

// Bowl profile in units of the crater's own depth, r in radii.
//
// The Layer Block's -(1 - r^2) is zero AT the rim but not FLAT at it: it
// arrives with a slope of -2/(1-flat), so the bowl meets the plain in a
// crease. On a block bench drawn at one scale that is invisible; on a
// saturated population under a hillshade every crater gets a drawn black
// circle around it, which is what sent the first deep-zoom renders
// wrong. The cosine dish has zero slope at both ends -- flat at the
// floor, tangent at the rim -- and is the profile lola_dem.cpp already
// uses for the same reason. Both are here; the parabola is what the
// Layer Block does, and seeing where it fails is worth a chip.
function CraterProfile(r, flat, cosine){
  if (r >= 1.0) return 0.0;
  if (r <= flat) return -1.0;
  const u = (r - flat) / Math.max(1e-4, 1.0 - flat);
  return cosine ? -0.5 * (1.0 + Math.cos(Math.PI * u)) : -(1.0 - u * u);
}

// Craters carry km positions; this pins them to the moon once.
function CraterLatLon(c, originLat, originLon){
  const lat = originLat - c.sKm / MOON_KM_PER_DEG;
  const lon = originLon + c.eKm / (MOON_KM_PER_DEG * Math.cos(lat * DEG2RAD));
  return [lat, lon];
}

// Where a crater lands in this level's pixels, and how big it is there.
function CraterPlace(c, frame, originLat, originLon){
  const [lat, lon] = CraterLatLon(c, originLat, originLon);
  return {
    px: (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5,
    py: (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5,
    rPx: (c.dKm * 0.5) / frame.kmPerPx
  };
}

// Carve into the height field, in chain units. heightScaleM is the
// chain's own: the hillshade multiplies height by z = 110 and
// differences it per pixel, so one unit is 110 pixel-widths of rise.
// Converting through it is what makes a 400 m crater 400 m deep at
// every level instead of three different depths.
function CarveCraters(height, res, frame, craters, P, spanKm, originLat, originLon){
  const heightScaleM = 110.0 * (spanKm * 1000.0 / res);
  const unitsPerM = 1.0 / heightScaleM;
  const bowl = new Float32Array(res * res);      // metres, deepest wins
  const relief = new Float32Array(res * res);    // metres, rims accumulate
  // Albedo, kept SEPARATE from the height. A fresh crater is brighter
  // than its surroundings -- that is most of why Plinius reads at all in
  // the mosaic -- but if the brightening went into the macro the
  // formRelief step would read it back as high ground and lift the
  // crater into a mesa. So it is carried alongside and multiplied into
  // the luminance at the very end, after the light.
  const alb = new Float32Array(res * res).fill(1.0);
  const rOuter = 1.0 + Math.max(3.0 * P.rimWidth, P.ejecta);
  let drawn = 0;
  for (const c of craters){
    if (c.off) continue;
    const pl = CraterPlace(c, frame, originLat, originLon);
    const R = pl.rPx * P.sizeScale;
    if (R < 0.35) continue;                      // finer than a pixel: nothing to draw
    const x0 = Math.max(0, Math.floor(pl.px - R * rOuter));
    const x1 = Math.min(res - 1, Math.ceil(pl.px + R * rOuter));
    const y0 = Math.max(0, Math.floor(pl.py - R * rOuter));
    const y1 = Math.min(res - 1, Math.ceil(pl.py + R * rOuter));
    if (x1 < x0 || y1 < y0) continue;
    drawn++;
    const dRatio = P.dMin + (P.dMax - P.dMin) * c.fresh;
    const depthM = c.dKm * 1000.0 * dRatio * P.depth * P.sizeScale;
    const rimM = depthM * P.rim * c.fresh;
    const invW = 1.0 / (2.0 * P.rimWidth * P.rimWidth);
    const albK = P.albedo * c.fresh;
    for (let y = y0; y <= y1; y++){
      const dy = (y - pl.py) / R;
      for (let x = x0; x <= x1; x++){
        const dx = (x - pl.px) / R;
        const r = Math.sqrt(dx * dx + dy * dy);
        if (r >= rOuter) continue;
        const k = y * res + x;
        if (r < 1.0){
          const b = CraterProfile(r, P.floorFlat, P.cosineBowl) * depthM;
          if (P.deepest){ if (b < bowl[k]) bowl[k] = b; }
          else bowl[k] += b;
        }
        if (rimM > 0.001) relief[k] += rimM * RimRise(r - 1.0, invW, P.ejecta);
        if (albK > 0.0005){
          // Dimmest on the floor, brightest at the rim, fading out
          // through the apron -- and smooth across r = 1, where a
          // discontinuity would print a ring the same way the ejecta
          // step did.
          let w;
          if (r < 1.0) w = 0.35 + 0.65 * r * r;
          else if (P.ejecta > 0.001){
            const x = Math.min(1.0, (r - 1.0) / P.ejecta);
            w = 1.0 - x * x * (3.0 - 2.0 * x);
          } else w = 0.0;
          alb[k] *= 1.0 + albK * w;
        }
      }
    }
  }
  for (let i = 0; i < height.length; i++)
    height[i] += (bowl[i] + relief[i]) * unitsPerM;
  return { count: drawn, alb };
}

/* --- NEW: everything below the mosaic's resolution -------------------
   The mosaic resolves nothing under ~1.33 km per texel. Above that floor
   the chain has data; under it, it has to invent -- and the shipped
   chain invents in PIXELS. Its grain sits on a 64-pixel lattice and its
   undulation on another, so both are a fixed fraction of whatever window
   is being drawn: zoom in and the undulation you were looking at
   disappears and a new one at the new pixel size takes its place. The
   ground changes as you approach it. At the widths the game actually
   draws (100 / 25 / 5 km, one per view) that never showed; on a
   continuous zoom it is the whole problem.

   So everything here is in WORLD wavelengths instead. Octaves run from
   the mosaic's own floor down to about three pixels, each pinned to the
   moon and faded in as it becomes resolvable. Zooming then only ADDS
   detail: what you could already see stays exactly where it was, and
   what appears was always there, too fine to draw. That is the
   continuity, and it is the same construction lola_dem.cpp's
   SynthesizeDetail uses below the DEM's floor -- the two synthesizers
   now invent sub-resolution ground the same way.
   -------------------------------------------------------------------- */

const FLOOR_KM = 1.3325;         // one WAC texel at the equator
const SUB_MAX_OCTAVES = 16;

// lola_dem.cpp's DetailHash / DetailNoise, so the same world point
// quantises into the same lattice cell in both synthesizers.
function DetailHash01(x, y, salt){
  let h = (Math.imul(x | 0, 0x8da6b343) ^ Math.imul(y | 0, 0xd8163841)
         ^ Math.imul(salt | 0, 0xcb1ab31f)) >>> 0;
  h = (h ^ (h >>> 13)) >>> 0; h = Math.imul(h, 0x9e3779b1) >>> 0;
  h = (h ^ (h >>> 16)) >>> 0;
  return (h & 0xFFFFFF) / 16777215.0;
}
// Smoothstepped value noise on a lattice of waveKm. -1..1.
function DetailNoise(u, v, waveKm, salt){
  const gu = u / waveKm, gv = v / waveKm;
  const x0 = Math.floor(gu), y0 = Math.floor(gv);
  let fx = gu - x0, fy = gv - y0;
  fx = fx * fx * (3.0 - 2.0 * fx); fy = fy * fy * (3.0 - 2.0 * fy);
  const n00 = DetailHash01(x0, y0, salt), n10 = DetailHash01(x0 + 1, y0, salt);
  const n01 = DetailHash01(x0, y0 + 1, salt), n11 = DetailHash01(x0 + 1, y0 + 1, salt);
  const top = n00 + (n10 - n00) * fx, bot = n01 + (n11 - n01) * fx;
  return (top + (bot - top) * fy) * 2.0 - 1.0;
}

// Every pixel's world position, once. u needs the cosine of ITS OWN row,
// so latitude and its cosine are per row and longitude-in-km per column.
function WorldGrid(frame, res){
  const vRow = new Float64Array(res), cosRow = new Float64Array(res),
        lonKm = new Float64Array(res);
  for (let y = 0; y < res; y++){
    const lat = frame.lat0Deg + (y + 0.5) * frame.dLatPerPx;
    vRow[y] = lat * MOON_KM_PER_DEG;
    cosRow[y] = Math.cos(lat * DEG2RAD);
  }
  for (let x = 0; x < res; x++)
    lonKm[x] = (frame.lon0Deg + (x + 0.5) * frame.dLonPerPx) * MOON_KM_PER_DEG;
  return { vRow, cosRow, lonKm };
}

// 0 where the mosaic still resolves this wavelength, 1 well below it.
function SubFade(lambdaKm){
  return Math.min(Math.max(Math.log2(FLOOR_KM * 2.0 / lambdaKm) / 1.5, 0.0), 1.0);
}

// The fractal residual: the ground's own roughness continued downward.
// Relief at a wavelength is a fraction of that wavelength, which is what
// makes it scale-free -- the same rule at 1 km and at 3 m.
function SubFloorNoise(outM, res, W, kmPerPx, roughFrac, roughMask){
  let lambda = FLOOR_KM * 2.0;
  for (let o = 0; o < SUB_MAX_OCTAVES && lambda >= 3.0 * kmPerPx; o++, lambda *= 0.5){
    const w = SubFade(lambda);
    if (w <= 0.001) continue;
    const ampM = w * roughFrac * lambda * 1000.0;
    const salt = (0x51 + o * 2654435761) | 0;
    for (let y = 0; y < res; y++){
      const v = W.vRow[y], c = W.cosRow[y], row = y * res;
      for (let x = 0; x < res; x++){
        const u = W.lonKm[x] * c;
        outM[row + x] += ampM * roughMask[row + x] * DetailNoise(u, v, lambda, salt);
      }
    }
  }
}

// The impact population, band by band: sizes halving from the mosaic's
// floor down to about three pixels, each band a jittered world lattice.
// Same Bowl(), same d/D law and the same deepest-wins rule as the named
// craters above -- a saturated field digs runaway pits without it.
function CraterPopulation(outM, res, frame, spanKm, P){
  const kmPerPx = spanKm / res;
  const p = [0, 0];
  let uMin = Infinity, uMax = -Infinity, vMin = Infinity, vMax = -Infinity;
  for (const [cx, cy] of [[0,0],[res,0],[0,res],[res,res]]){
    FrameWorldKm(frame, cx, cy, p);
    uMin = Math.min(uMin, p[0]); uMax = Math.max(uMax, p[0]);
    vMin = Math.min(vMin, p[1]); vMax = Math.max(vMax, p[1]);
  }
  const bowl = new Float32Array(res * res);
  const rOuter = 1.0 + Math.max(3.0 * P.rimWidth, P.ejecta);
  let placed = 0;
  let diamKm = FLOOR_KM * 1.4;
  const bandFloor = P.popPx || 2.5;
  for (let b = 0; b < SUB_MAX_OCTAVES && diamKm >= bandFloor * kmPerPx; b++, diamKm *= 0.5){
    const w = SubFade(diamKm);
    if (w <= 0.001) continue;
    const cellKm = diamKm / 0.55;
    const salt = (0xC7A7E5 + b * 7919) | 0;
    const i0 = Math.floor(uMin / cellKm) - 1, i1 = Math.floor(uMax / cellKm) + 1;
    const j0 = Math.floor(vMin / cellKm) - 1, j1 = Math.floor(vMax / cellKm) + 1;
    bowl.fill(0);
    for (let cj = j0; cj <= j1; cj++){
      for (let ci = i0; ci <= i1; ci++){
        // Crater fields cluster: a slow density modulation leaves some
        // patches busy and some nearly clean, instead of bubble wrap.
        const cluster = 0.5 + 0.5 * DetailNoise((ci + 0.5) * cellKm, (cj + 0.5) * cellKm,
                                                cellKm * 11.0, (salt + 900) | 0);
        if (DetailHash01(ci, cj, salt) > P.popDensity * cluster) continue;
        const u = (ci + 0.12 + 0.76 * DetailHash01(ci, cj, (salt + 1) | 0)) * cellKm;
        const v = (cj + 0.12 + 0.76 * DetailHash01(ci, cj, (salt + 2) | 0)) * cellKm;
        const lat = v / MOON_KM_PER_DEG;
        const cosLat = Math.cos(lat * DEG2RAD);
        if (Math.abs(cosLat) < 1e-6) continue;
        const lon = u / (MOON_KM_PER_DEG * cosLat);
        const py = (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5;
        const px = (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5;
        const dKm = cellKm * (0.30 + 0.70 * DetailHash01(ci, cj, (salt + 3) | 0));
        const R = (dKm * 0.5) / kmPerPx;
        if (R < 0.9) continue;
        const x0 = Math.max(0, Math.floor(px - R * rOuter));
        const x1 = Math.min(res - 1, Math.ceil(px + R * rOuter));
        const y0 = Math.max(0, Math.floor(py - R * rOuter));
        const y1 = Math.min(res - 1, Math.ceil(py + R * rOuter));
        if (x1 < x0 || y1 < y0) continue;
        placed++;
        // Most craters are ancient: age^3 keeps the fresh, sharp, rimmed
        // ones rare, which is what a gardened surface looks like.
        const age = DetailHash01(ci, cj, (salt + 4) | 0);
        const fresh = age * age * age;
        const depthM = w * P.popDepth * dKm * 1000.0 *
                       (P.dMin + (P.dMax - P.dMin) * fresh);
        const rimM = depthM * P.rim * fresh;
        const invW = 1.0 / (2.0 * P.rimWidth * P.rimWidth);
        for (let y = y0; y <= y1; y++){
          const dy = (y - py) / R, row = y * res;
          for (let x = x0; x <= x1; x++){
            const dx = (x - px) / R;
            const r = Math.sqrt(dx * dx + dy * dy);
            if (r >= rOuter) continue;
            const k = row + x;
            if (r < 1.0){
              const bv = CraterProfile(r, P.floorFlat, P.cosineBowl) * depthM;
              if (bv < bowl[k]) bowl[k] = bv;
            }
            if (rimM > 0.001) outM[k] += rimM * RimRise(r - 1.0, invW, P.ejecta);
          }
        }
      }
    }
    for (let i = 0; i < outM.length; i++) outM[i] += bowl[i];
  }
  return placed;
}

// Clasts: the scattered angular grit BuildRegolith paints into the Layer
// Block's regolith as little bright specks with a lit lower-right edge.
//
// It paints them, because a texture tile has no light of its own. Here
// they are RELIEF -- small domes -- so the same sun that shades the
// craters gives each one its lit face and its shadow, and they are on
// the same world lattice as everything else: bands halving from a few
// tens of metres down to the pixel, so zooming turns a speck into a rock
// and finds new specks under it.
function ClastBands(outM, res, frame, spanKm, P){
  const kmPerPx = spanKm / res;
  const p = [0, 0];
  let uMin = Infinity, uMax = -Infinity, vMin = Infinity, vMax = -Infinity;
  for (const [cx, cy] of [[0,0],[res,0],[0,res],[res,res]]){
    FrameWorldKm(frame, cx, cy, p);
    uMin = Math.min(uMin, p[0]); uMax = Math.max(uMax, p[0]);
    vMin = Math.min(vMin, p[1]); vMax = Math.max(vMax, p[1]);
  }
  let placed = 0;
  let diamKm = 0.045;                       // 45 m: a big rock, and rare
  const clastFloor = P.clastPx || 2.2;
  for (let b = 0; b < 12 && diamKm >= clastFloor * kmPerPx; b++, diamKm *= 0.5){
    const cellKm = diamKm / 0.42;
    const salt = (0x5EED17 + b * 26417) | 0;
    // Bigger rocks are rarer, by about the same power law the craters use.
    const occ = Math.min(0.9, P.clastDensity * (0.35 + 0.65 * b / 4.0));
    const i0 = Math.floor(uMin / cellKm) - 1, i1 = Math.floor(uMax / cellKm) + 1;
    const j0 = Math.floor(vMin / cellKm) - 1, j1 = Math.floor(vMax / cellKm) + 1;
    for (let cj = j0; cj <= j1; cj++){
      for (let ci = i0; ci <= i1; ci++){
        if (DetailHash01(ci, cj, salt) > occ) continue;
        const u = (ci + 0.15 + 0.70 * DetailHash01(ci, cj, (salt + 1) | 0)) * cellKm;
        const v = (cj + 0.15 + 0.70 * DetailHash01(ci, cj, (salt + 2) | 0)) * cellKm;
        const lat = v / MOON_KM_PER_DEG;
        const cosLat = Math.cos(lat * DEG2RAD);
        if (Math.abs(cosLat) < 1e-6) continue;
        const lon = u / (MOON_KM_PER_DEG * cosLat);
        const py = (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5;
        const px = (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5;
        const dKm = diamKm * (0.55 + 0.75 * DetailHash01(ci, cj, (salt + 3) | 0));
        const R = (dKm * 0.5) / kmPerPx;
        if (R < 0.7) continue;
        const x0 = Math.max(0, Math.floor(px - R)), x1 = Math.min(res - 1, Math.ceil(px + R));
        const y0 = Math.max(0, Math.floor(py - R)), y1 = Math.min(res - 1, Math.ceil(py + R));
        if (x1 < x0 || y1 < y0) continue;
        placed++;
        // A rock sits ON the ground: a third of its width proud of it.
        const hM = P.clasts * dKm * 1000.0 * 0.33 *
                   (0.6 + 0.8 * DetailHash01(ci, cj, (salt + 4) | 0));
        for (let y = y0; y <= y1; y++){
          const dy = (y - py) / R, row = y * res;
          for (let x = x0; x <= x1; x++){
            const dx = (x - px) / R;
            const q = 1.0 - dx * dx - dy * dy;
            if (q > 0.0) outM[row + x] += hM * Math.sqrt(q);
          }
        }
      }
    }
  }
  return placed;
}

// Relief the mosaic cannot carry, in chain units, added to the height
// field where the engine's pixel-anchored grain and undulation used to go.
function SubFloorRelief(height, res, frame, spanKm, tune, density){
  const kmPerPx = spanKm / res;
  const heightScaleM = 110.0 * (spanKm * 1000.0 / res);
  const W = WorldGrid(frame, res);
  const accM = new Float32Array(res * res);
  const roughMask = new Float32Array(res * res);
  for (let i = 0; i < roughMask.length; i++)
    roughMask[i] = 0.45 + 0.55 * density[i];    // bright ground is rough ground
  if (tune.subRough > 0.0)
    SubFloorNoise(accM, res, W, kmPerPx, tune.subRough, roughMask);
  if (tune.clasts > 0.001)
    ClastBands(accM, res, frame, spanKm, tune);
  let craters = 0;
  if (tune.subCraters > 0.001)
    craters = CraterPopulation(accM, res, frame, spanKm, {
      popDensity: tune.popDensity, popDepth: tune.subCraters,
      dMin: tune.dMin, dMax: tune.dMax, rim: tune.rim,
      rimWidth: tune.rimWidth, ejecta: tune.ejecta, floorFlat: tune.floorFlat,
      popPx: tune.popPx, cosineBowl: tune.cosineBowl
    });
  // The last octave is the pixel itself, and nothing world-anchored can
  // live there: this is the grit the previous zoom could not show.
  const grit = new Float32Array(res * res);
  const cellKm = Math.max(1e-12, kmPerPx);
  if (tune.subGrit > 0.0){
    for (let y = 0; y < res; y++){
      const v = W.vRow[y], c = W.cosRow[y], row = y * res;
      for (let x = 0; x < res; x++)
        grit[row + x] = DetailHash01(Math.floor(W.lonKm[x] * c / cellKm),
                                     Math.floor(v / cellKm), 0x6A09E667) - 0.5;
    }
  }
  // Crisp keeps the finest term at one pixel: blurring it is what makes a
  // hard-pixel upscale look like blocky mush rather than like grain.
  GaussianBlur(grit, res, res, tune.crisp ? 0.0 : 0.55);
  const gritM = tune.subGrit * 1.4 * kmPerPx * 1000.0;   // ~1.4 px of relief
  for (let i = 0; i < height.length; i++)
    height[i] += (accM[i] + gritM * grit[i] * roughMask[i]) / heightScaleM;
  return craters;
}

// The regolith's own tone: broad mottled patches over fine grit, the
// character BuildRegolith gives the Layer Block's top stratum, in world
// wavelengths so it too only gains detail as you come down. Albedo only
// -- fed into the macro it would be read back as relief.
function SubFloorMottle(res, frame, spanKm, tune){
  const kmPerPx = spanKm / res;
  const W = WorldGrid(frame, res);
  const out = new Float32Array(res * res);
  // Four octaves, not everything down to the pixel: mottling is a broad
  // tone, the fine end of it is invisible under the grit, and measured at
  // res 640 the extra octaves cost more than the crater population does.
  const MOTTLE_OCTAVES = 4;
  let lambda = FLOOR_KM * 0.7, amp = 1.0, norm = 0.0;
  for (let o = 0; o < MOTTLE_OCTAVES && lambda >= 4.0 * kmPerPx; o++, lambda *= 0.5){
    const w = SubFade(lambda * 2.0) * amp;
    norm += amp;
    if (w > 0.001){
      const salt = (0x2545F491 + o * 40503) | 0;
      for (let y = 0; y < res; y++){
        const v = W.vRow[y], c = W.cosRow[y], row = y * res;
        for (let x = 0; x < res; x++)
          out[row + x] += w * DetailNoise(W.lonKm[x] * c, v, lambda, salt);
      }
    }
    amp *= 0.62;
  }
  if (norm > 0) for (let i = 0; i < out.length; i++) out[i] *= tune.subMottle / norm;
  return out;
}

/* --- surface layers and light --------------------------------------- */

// The chain's own TextureModulate, with one line added: the craters go
// into the height field after the grain and the boulders and BEFORE the
// sun, so they are lit and shadowed by the same hillshade and the same
// ray-march as every other landform. A crater pasted on afterwards is a
// decal; a crater carved here is ground.
function TextureModulate(macro, res, frame, amp, tune, boulderCount, carve, lastRung){
  const k = res / 300.0;
  const density = new Float32Array(res * res);
  for (let i = 0; i < density.length; i++)
    density[i] = Math.min(Math.max((macro[i] - 0.22) / 0.45, 0.15), 1.0);

  // Height field: the smoothed macro as a relief proxy -- the imagery's
  // own form re-read as topography -- plus grain and undulation.
  const height = Float32Array.from(macro);
  // The macro as a relief proxy. Smoothed hard by default so the imagery's
  // own noise does not become terrain; smoothed less when the picture is
  // going to be shown at its own resolution and can afford the detail.
  GaussianBlur(height, res, res, (tune.crisp ? 1.2 : 2.5) * k);
  for (let i = 0; i < height.length; i++)
    height[i] = (height[i] - 0.5) * 0.13 * tune.formRelief;

  // Two ways to invent what the mosaic cannot carry. The engine's own is
  // anchored to the frame; the world-anchored one below it is anchored to
  // the moon. The chip switches between them so the difference can be
  // seen rather than argued.
  const spanKm = frame.kmPerPx * res;
  let popCraters = 0;
  // ...and only to the rung being LOOKED at, for the reason spelled out
  // on DEFAULT_CRATER.everyLevel: relief carved at rung after rung is
  // shaded once per rung. The population is world-anchored and
  // deterministic, so the last rung regenerates exactly the craters the
  // wider rung had -- the compounding is the only thing lost.
  if (tune.subFloor && lastRung){
    popCraters = SubFloorRelief(height, res, frame, spanKm, tune, density);
  } else if (!tune.subFloor){
    const grain = GrainNoise(res, frame);
    const undulFrame = Object.assign({}, frame);
    undulFrame.salt = (frame.salt ^ NOISE_UNDULATION) >>> 0;
    const undul = Fbm(res, tune.octaves, Math.max(2, (tune.featureScale * k) | 0),
                      0.5, undulFrame);
    for (let i = 0; i < height.length; i++){
      const rough = 0.45 + 0.55 * density[i];
      height[i] += 0.004 * amp * tune.grain * grain[i] * rough;
      height[i] += 0.02 * amp * tune.undulation * (undul[i] - 0.5) * rough;
    }
  }
  // The clasts in the sub-floor path are the same idea done properly, so
  // the engine's boulder sprinkle stands down when it is running.
  if (boulderCount > 0 && !(tune.subFloor && lastRung))
    SprinkleBoulders(height, res, frame, (boulderCount * tune.boulders) | 0,
                     0.010 * tune.boulderAmp);

  let craters = 0, craterAlb = null;
  if (carve){
    const c = carve(height, res, frame);
    craters = c.count; craterAlb = c.alb;
  }

  const z = 110.0;
  const hs = Hillshade(height, res, z, tune.crisp ? 0.0 : 0.6, tune.sunAz, tune.sunAlt);
  const flatRef = Math.sin(tune.sunAlt * DEG2RAD);
  // Every detail layer is switchable, so what each one costs and what it
  // is worth can be looked at rather than argued about. The march is the
  // expensive one and had no lever until now.
  const light = tune.shadows === 0
    ? new Float32Array(res * res).fill(1.0)
    : CastShadows(height, res, z, 22.0 * k, 1.5, tune.sunAz, tune.sunAlt,
                  tune.crisp ? 0.25 : 0.8);

  const speckleFrame = Object.assign({}, frame);
  speckleFrame.salt = (frame.salt ^ NOISE_SPECKLE) >>> 0;
  const worldTone = tune.subFloor && lastRung;
  const speckle = worldTone
    ? (tune.subMottle > 0.0 ? SubFloorMottle(res, frame, spanKm, tune)
                            : new Float32Array(res * res))
    : Fbm(res, 2, 4, 0.5, speckleFrame);
  const speckMid = worldTone ? 0.0 : 0.5;
  const speckGain = worldTone ? 1.0 : 0.04 * tune.speckle;
  for (let i = 0; i < macro.length; i++){
    const rel = Math.min(Math.max(hs[i] / flatRef, 0.0), 1.6);
    const rough = 0.45 + 0.55 * density[i];
    let lum = macro[i] * ((1.0 - tune.relWeight) + tune.relWeight * rel)
                       * ((1.0 - tune.lightWeight) + tune.lightWeight * light[i]);
    if (craterAlb) lum *= craterAlb[i];
    lum *= 1.0 + speckGain * Math.min(amp, 1.6) * (speckle[i] - speckMid) * rough;
    lum = Math.min(Math.max(lum, 0.0), 1.0);
    const s = lum * lum * (3.0 - 2.0 * lum);          // gentle S-curve
    let out = Math.min(Math.max(s * tune.sCurve + lum * (1.0 - tune.sCurve), 0.0), 1.0);
    // Quantised tone. Chunky pixels over continuous shading read as a
    // rendering accident; the same pixels over banded shading read as a
    // decision. Off by default -- this is a look, not a correction.
    if (tune.bands >= 2) out = Math.round(out * tune.bands) / tune.bands;
    macro[i] = out;
  }
  return { height, light, hs, craters, popCraters };
}

// Lunar tone ramp: cool shadow -> regolith grey -> warm sunlit.
function RampColor(t, out, o){
  const lo = [16, 17, 24], mid = [108, 105, 102], hi = [236, 232, 220];
  if (t < 0.5){
    const u = t / 0.5;
    for (let k = 0; k < 3; k++) out[o + k] = lo[k] + (mid[k] - lo[k]) * u;
  } else {
    const u = (t - 0.5) / 0.5;
    for (let k = 0; k < 3; k++) out[o + k] = mid[k] + (hi[k] - mid[k]) * u;
  }
  out[o + 3] = 255;
}

/* --- the chain -------------------------------------------------------
   100 -> 25 -> 5 km. Level 0 is the WAC macro crop; every level after it
   is the centre crop of the level above's OUTPUT, which is what
   registers them to each other and makes zooming continuous rather than
   a cut to a different scene.
   ------------------------------------------------------------------ */
function GenerateChain(o){
  const res = o.res, spans = o.spans, tune = o.tune;
  const report = {};
  const t0 = (typeof performance !== "undefined" ? performance.now() : Date.now());
  const spansDeg = spans.map(s => s / MOON_KM_PER_DEG);

  let lum = CropMacro(o.block, o.lat, o.lon, spansDeg[0], res, report);
  SharpenAdaptive(lum, res);

  const levels = [];
  const carveFor = (lvl) => {
    if (!o.craters || !o.craterParams.on) return null;
    if (!o.craterParams.everyLevel && lvl !== spans.length - 1) return null;
    return (height, r, frame) =>
      CarveCraters(height, r, frame, o.craters, o.craterParams, spans[lvl],
                   o.originLat, o.originLon);
  };

  const emit = (lvl, fields) => {
    const rgba = new Uint8ClampedArray(res * res * 4);
    for (let i = 0; i < res * res; i++) RampColor(lum[i], rgba, i * 4);
    levels.push({
      spanKm: spans[lvl], rgba,
      lum: Float32Array.from(lum), height: fields.height, light: fields.light,
      craters: fields.craters, popCraters: fields.popCraters,
      kmPerPx: spans[lvl] / res,
      heightScaleM: 110.0 * (spans[lvl] * 1000.0 / res),
      frame: MakeNoiseFrame(o.lat, o.lon, spans[lvl], res, 0)
    });
  };

  let f = TextureModulate(lum, res, MakeNoiseFrame(o.lat, o.lon, spans[0], res, 0),
                          1.0, tune, 0, carveFor(0), spans.length === 1);
  emit(0, f);

  for (let lvl = 1; lvl < spans.length; lvl++){
    const k = res / 300.0;
    // Crop to the level's exact span, not to whole pixels of the level
    // above: rounding made a level cover 4.98 km where it claimed 5, and
    // the imagery then ran at a scale the noise lattice did not share.
    const frac = spansDeg[lvl] / spansDeg[lvl - 1];
    const half = frac * res / 2.0;
    const lo = res / 2.0 - half, step = 2.0 * half / res;
    const next = new Float32Array(res * res);
    for (let y = 0; y < res; y++){
      const sy = lo + (y + 0.5) * step - 0.5;
      const iy = Math.min(Math.max(Math.floor(sy), 0), res - 2);
      const ty = Math.min(Math.max(sy - iy, 0.0), 1.0);
      for (let x = 0; x < res; x++){
        const sx = lo + (x + 0.5) * step - 0.5;
        const ix = Math.min(Math.max(Math.floor(sx), 0), res - 2);
        const tx = Math.min(Math.max(sx - ix, 0.0), 1.0);
        const r0 = iy * res + ix, r1 = (iy + 1) * res + ix;
        const a = lum[r0] + (lum[r0 + 1] - lum[r0]) * tx;
        const b = lum[r1] + (lum[r1 + 1] - lum[r1]) * tx;
        next[y * res + x] = a + (b - a) * ty;
      }
    }
    lum = next;
    GaussianBlur(lum, res, res, 0.6 * k);
    const blur = Float32Array.from(lum);
    GaussianBlur(blur, res, res, 5.0 * k);
    for (let i = 0; i < lum.length; i++)
      lum[i] = Math.min(Math.max(lum[i] + 0.40 * (lum[i] - blur[i]), 0.0), 1.0);
    // The engine only ever runs this at its 5 km level, where 120*k*k
    // boulders over 5 km is 5.44 per km^2. A bench that zooms has to say
    // which of those two numbers it meant: per WINDOW, and the rocks
    // change every time you zoom; per km^2, and they are the same rocks
    // seen closer. Per km^2 -- and at 5 km it is the engine's own count.
    const boulderBase = (spans[lvl] <= 5.0 + 1e-3)
      ? Math.round(120 * k * k / 25.0 * spans[lvl] * spans[lvl]) : 0;
    f = TextureModulate(lum, res,
                        MakeNoiseFrame(o.lat, o.lon, spans[lvl], res,
                                       Math.imul(0x9E3779B9, lvl) >>> 0),
                        1.0 + 0.7 * lvl, tune, boulderBase, carveFor(lvl),
                        lvl === spans.length - 1);
    emit(lvl, f);
  }
  report.ms = (typeof performance !== "undefined" ? performance.now() : Date.now()) - t0;
  return { levels, report };
}

/* --- NEW: the zoom ladder ------------------------------------------
   TerrainChainSpansForWindow (terrain_synthesis.cpp) gives an arbitrary
   window two steps: the 100 km macro, then one crop straight to the
   window. The game's own ladder is three -- 100 / 25 / 5 -- and the
   difference is visible, because every step re-sharpens and re-lights
   what the step above produced. So the bench carries both and lets them
   be compared, rather than picking one and calling it the answer.
   ------------------------------------------------------------------ */
const MACRO_KM = 100.0;              // where the WAC crop is taken
const LADDER_MAX_LEVELS = 6;

function ChainSpans(spanKm, stepped){
  if (spanKm >= MACRO_KM) return [spanKm];   // nothing above it to crop from
  if (!stepped) return [MACRO_KM, spanKm];   // the C++ instrument ladder
  // Stepped: no step wider than 5x, which lands on 100 / 22.4 / 5 at the
  // game's sect span -- the game's own ladder, near enough to compare.
  const total = MACRO_KM / spanKm;
  const n = Math.min(LADDER_MAX_LEVELS - 1,
                     Math.max(1, Math.ceil(Math.log(total) / Math.log(5))));
  const ratio = Math.pow(total, 1 / n);
  const out = [MACRO_KM];
  for (let i = 1; i < n; i++) out.push(MACRO_KM / Math.pow(ratio, i));
  out.push(spanKm);
  return out;
}

/* --- NEW: the mosaic itself ----------------------------------------
   What the synthesizer was given, with nothing done to it. Returned as
   the whole texels covering the window plus where the window sits
   inside them, so a consumer can draw the REAL texel grid instead of a
   resampled picture of it -- at 5 km that grid is under four texels
   across, and seeing that is the point of putting the two side by side.
   ------------------------------------------------------------------ */
function SourceTexels(block, latDeg, lonDeg, spanDeg){
  const W = block.wacW, H = block.wacH;
  const c = Math.max(0.2, Math.cos(latDeg * DEG2RAD));
  const lonSpan = spanDeg / c;
  const lat0 = latDeg - spanDeg / 2.0, lat1 = latDeg + spanDeg / 2.0;
  const lon0 = lonDeg - lonSpan / 2.0, lon1 = lonDeg + lonSpan / 2.0;
  const gx0 = (lon0 + 180.0) / 360.0 * W, gx1 = (lon1 + 180.0) / 360.0 * W;
  const gy0 = (90.0 - lat1) / 180.0 * H,  gy1 = (90.0 - lat0) / 180.0 * H;
  const x0 = Math.floor(gx0) - 1, y0 = Math.floor(gy0) - 1;
  const w = Math.ceil(gx1) - x0 + 1, h = Math.ceil(gy1) - y0 + 1;
  const data = new Uint8Array(w * h);
  let escaped = false;
  for (let y = 0; y < h; y++){
    let by = y0 + y - block.y0;
    if (by < 0 || by >= block.h){ escaped = true; by = Math.min(Math.max(by, 0), block.h - 1); }
    for (let x = 0; x < w; x++){
      let bx = x0 + x - block.x0;
      if (bx < 0 || bx >= block.w){ escaped = true; bx = Math.min(Math.max(bx, 0), block.w - 1); }
      data[y * w + x] = block.data[by * block.w + bx];
    }
  }
  return { data, w, h, escaped,
           // the window, in texels of this little image
           sx: gx0 - x0, sy: gy0 - y0, sw: gx1 - gx0, sh: gy1 - gy0,
           kmPerTexel: 180.0 / H * MOON_KM_PER_DEG };
}

/* --- NEW: a chain you can zoom ---------------------------------------
   A bench renders one window and stops. A zoom asks for a new one every
   few frames, and walking the whole ladder from the 100 km macro each
   time is most of the work thrown away: the wide rungs do not depend on
   how far in you are. They are crops, and crops of the same ground from
   the same centre are the same crops.

   So the wide rungs are built once and kept, and only the rung you are
   LOOKING at is rebuilt -- which is also the only rung that carries the
   sub-floor detail, so the split falls exactly where the work is. On the
   measurements that took a regeneration from ~500 ms for the full ladder
   to ~190 ms at res 448: fast enough to regenerate while a zoom is still
   moving, which is what makes the ground sharpen instead of stretch.

   The cache is keyed on the centre. Panning invalidates it; zooming does
   not, which is the case a zoom cares about.
   -------------------------------------------------------------------- */
const LIVE_RUNGS = [100.0, 25.0, 5.0, 1.25, 0.3125];

// One rung from the one above it, at the level's exact span rather than
// at whole pixels of the level above -- and not necessarily from its
// centre. The offset is what lets a view pan inside the cached pyramid
// instead of rebuilding it: a rung's pixels are geographically
// registered, so a pixel offset IS a ground offset.
function CropRung(lum, res, fromSpanKm, toSpanKm, offXKm, offYKm, crisp, srcRes){
  const src = srcRes || res;
  const k = res / 300.0;
  const frac = toSpanKm / fromSpanKm;
  const half = frac * src / 2.0;
  const ox = (offXKm || 0) / fromSpanKm * src;
  const oy = (offYKm || 0) / fromSpanKm * src;
  const loX = src / 2.0 - half + ox, loY = src / 2.0 - half + oy;
  const step = 2.0 * half / res;
  const next = new Float32Array(res * res);
  for (let y = 0; y < res; y++){
    const sy = loY + (y + 0.5) * step - 0.5;
    const iy = Math.min(Math.max(Math.floor(sy), 0), src - 2);
    const ty = Math.min(Math.max(sy - iy, 0.0), 1.0);
    for (let x = 0; x < res; x++){
      const sx = loX + (x + 0.5) * step - 0.5;
      const ix = Math.min(Math.max(Math.floor(sx), 0), src - 2);
      const tx = Math.min(Math.max(sx - ix, 0.0), 1.0);
      const r0 = iy * src + ix, r1 = (iy + 1) * src + ix;
      const a = lum[r0] + (lum[r0 + 1] - lum[r0]) * tx;
      const b = lum[r1] + (lum[r1 + 1] - lum[r1]) * tx;
      next[y * res + x] = a + (b - a) * ty;
    }
  }
  GaussianBlur(next, res, res, crisp ? 0.0 : 0.6 * k);
  const blur = Float32Array.from(next);
  GaussianBlur(blur, res, res, 5.0 * k);
  for (let i = 0; i < next.length; i++)
    next[i] = Math.min(Math.max(next[i] + 0.40 * (next[i] - blur[i]), 0.0), 1.0);
  return next;
}

// o.rungRes: the CACHED rungs may be built coarser than the view is.
// They are intermediate macros -- cropped, resampled and then modulated
// again with fresh sub-floor detail at the view's own resolution -- so
// most of their pixels are thrown away by the crop. Measured at a 2400
// view: building them at 1200 instead costs RMS 1.0-1.8/255 (max 16,
// and the difference image is structureless) and saves, from a cold
// chain, 20.8 -> 13.2 s at 25 km, 28.6 -> 14.6 at 5 km, and 46.4 ->
// 18.4 at 1 km, where four rungs have to be walked. That last one is
// the wait after a zoom to a new level, and it is most of it.
function MakeLiveChain(o){
  const res = o.res, rungRes = o.rungRes || o.res, t0 = (typeof performance !== "undefined" ? performance.now() : Date.now());
  const spansDeg0 = LIVE_RUNGS[0] / MOON_KM_PER_DEG;
  const report = {};
  const rungs = [];
  let built = -1;

  // Built on demand, not all at once. A phone waiting for five rungs
  // before its first frame is waiting for two it may never ask for --
  // the deep ones only matter once somebody zooms that far.
  function EnsureRung(i){
    if (built >= i) return;
    if (built < 0){
      let lum = CropMacro(o.block, o.lat, o.lon, spansDeg0, rungRes, report);
      SharpenAdaptive(lum, rungRes);
      // Cached rungs are intermediate BY CONSTRUCTION -- lastRung false --
      // so none of them carries sub-floor detail into the macro the live
      // rung inherits. Same rule the bench found: relief carved at rung
      // after rung is shaded once per rung.
      TextureModulate(lum, rungRes, MakeNoiseFrame(o.lat, o.lon, LIVE_RUNGS[0], rungRes, 0),
                      1.0, o.tune, 0, null, false);
      rungs[0] = { spanKm: LIVE_RUNGS[0], lum };
      built = 0;
    }
    for (let j = built + 1; j <= i; j++){
      const lum = CropRung(rungs[j - 1].lum, rungRes, LIVE_RUNGS[j - 1], LIVE_RUNGS[j],
                           0, 0, o.tune.crisp);
      TextureModulate(lum, rungRes,
                      MakeNoiseFrame(o.lat, o.lon, LIVE_RUNGS[j], rungRes,
                                     Math.imul(0x9E3779B9, j) >>> 0),
                      1.0 + 0.7 * j, o.tune, 0, null, false);
      rungs[j] = { spanKm: LIVE_RUNGS[j], lum };
      built = j;
    }
  }
  EnsureRung(0);
  const buildMs = (typeof performance !== "undefined" ? performance.now() : Date.now()) - t0;

  // The window you are looking at, built from the deepest cached rung
  // that still CONTAINS it -- which, once the window can be off-centre,
  // depends on where it is as well as how wide it is. Panning then costs
  // one rung like zooming does, until it walks off the 100 km macro.
  // Which rung a window comes off, and which level it counts as. Split
  // out because a tile built in pieces has to force every piece to the
  // WHOLE tile's answer: the level seeds the noise and scales the
  // amplitude, so a piece that picked its own would not join up.
  function PickLevel(spanKm, offXKm, offYKm){
    const reach = Math.max(Math.abs(offXKm || 0), Math.abs(offYKm || 0)) + spanKm / 2.0;
    let base = 0;
    for (let i = 0; i < LIVE_RUNGS.length; i++)
      if (LIVE_RUNGS[i] / 2.0 >= reach - 1e-9) base = i;
    return { base, lvl: base + (spanKm < LIVE_RUNGS[base] - 1e-9 ? 1 : 0) };
  }

  // Everything up to the point where the window has a macro and a
  // frame, which is where the CPU and the GPU paths part company: the
  // rung ladder and the crop stay here, the per-pixel synthesis is what
  // regolith_gpu.js takes over.
  // The rungs are built with the chain's own tune and carry no sub-floor
  // detail -- they are never the last rung -- so the VIEW's tune can be
  // overridden per call without any of them going stale. That is what
  // makes a detail toggle instant instead of a rebuild.
  function TuneFor(over){ return over ? Object.assign({}, o.tune, over) : o.tune; }

  function Prepare(spanKm, offXKm, offYKm, force){
    const ox = offXKm || 0, oy = offYKm || 0;
    const pick = PickLevel(spanKm, ox, oy);
    const base = force && force.base !== undefined ? force.base : pick.base;
    const lvl = force && force.lvl !== undefined ? force.lvl : pick.lvl;
    EnsureRung(base);
    // Whether to crop is geometry, not level: a forced level must not be
    // able to decide that a narrower window is the whole rung.
    const rungKm = rungs[base].spanKm;
    const lum = (spanKm >= rungKm - 1e-9 && rungRes === res)
      ? Float32Array.from(rungs[base].lum)
      : CropRung(rungs[base].lum, res, rungKm, spanKm, ox, oy, o.tune.crisp, rungRes);
    // The frame belongs to the window, not to the cache, or the craters
    // would stay behind while the imagery moved.
    const vLat = o.lat - oy / MOON_KM_PER_DEG;
    const vLon = o.lon + ox / (MOON_KM_PER_DEG * Math.cos(vLat * DEG2RAD));
    const frame = MakeNoiseFrame(vLat, vLon, spanKm, res,
                                 Math.imul(0x9E3779B9, lvl) >>> 0);
    return { lum, frame, lvl, base, spanKm, res, ox, oy, lat: vLat, lon: vLon,
             fromRungKm: rungKm, kmPerPx: spanKm / res,
             heightScaleM: 110.0 * (spanKm * 1000.0 / res) };
  }

  function View(spanKm, offXKm, offYKm, force, tuneOver){
    const t1 = (typeof performance !== "undefined" ? performance.now() : Date.now());
    const pre = Prepare(spanKm, offXKm, offYKm, force);
    const l = pre.lum, lvl = pre.lvl, ox = pre.ox, oy = pre.oy;
    const vLat = pre.lat, vLon = pre.lon;
    const k = res / 300.0;
    const boulderBase = (spanKm <= 5.0 + 1e-3)
      ? Math.round(120 * k * k / 25.0 * spanKm * spanKm) : 0;
    const carve = (o.craterParams && o.craterParams.on && o.craters)
      ? (h, r, fr) => CarveCraters(h, r, fr, o.craters, o.craterParams, spanKm,
                                   o.originLat, o.originLon)
      : null;
    const f = TextureModulate(l, res, pre.frame,
                              1.0 + 0.7 * lvl, TuneFor(tuneOver), boulderBase, carve, true);
    const rgba = new Uint8ClampedArray(res * res * 4);
    for (let i = 0; i < res * res; i++) RampColor(l[i], rgba, i * 4);
    return {
      spanKm, res, rgba, lum: l, height: f.height, light: f.light,
      offXKm: ox, offYKm: oy, lat: vLat, lon: vLon,
      craters: f.craters, popCraters: f.popCraters,
      fromRungKm: pre.fromRungKm,
      kmPerPx: spanKm / res,
      heightScaleM: 110.0 * (spanKm * 1000.0 / res),
      ms: (typeof performance !== "undefined" ? performance.now() : Date.now()) - t1
    };
  }
  return { View, Prepare, PickLevel, rungs, EnsureRung, buildMs, escaped: !!report.escaped,
           lat: o.lat, lon: o.lon, res };
}

/* --- the craters this bench ships with ------------------------------
   A couple of big and a handful of small, in km east / km south of the
   region's crater origin. The two big ones are the size of real named
   craters and carry a 100 km frame; the small ones live entirely below
   the WAC's ~1.33 km/texel floor, which is where inventing them is
   honest -- they cannot contradict data that does not resolve them --
   and they are the only landforms a 5 km frame has of its own.
   ------------------------------------------------------------------ */
const CRATERS = [
  { name: "BRAVO",   eKm:   8.20, sKm:   7.00, dKm: 9.60, fresh: 0.80 },
  { name: "ALFA",    eKm: -19.00, sKm:  20.50, dKm: 5.20, fresh: 0.45 },
  { name: "CHARLIE", eKm:   3.30, sKm:  -5.60, dKm: 1.85, fresh: 0.90 },
  { name: "DELTA",   eKm:  -1.05, sKm:   0.75, dKm: 0.92, fresh: 0.95 },
  { name: "ECHO",    eKm:   1.25, sKm:  -1.10, dKm: 0.46, fresh: 0.55 },
  { name: "FOXTROT", eKm:  -0.55, sKm:  -1.85, dKm: 0.26, fresh: 0.70 },
  { name: "GOLF",    eKm:   0.175, sKm:  0.135, dKm: 0.110, fresh: 0.85 },
  { name: "HOTEL",   eKm:  -0.085, sKm: -0.105, dKm: 0.055, fresh: 0.60 }
];

const DEFAULT_TUNE = {
  grain: 1.0, undulation: 1.0, boulders: 1.0, boulderAmp: 1.0,
  formRelief: 1.0, relWeight: 0.38, lightWeight: 0.55, speckle: 1.0,
  sCurve: 0.20, featureScale: 64, octaves: 3, sunAz: 315, sunAlt: 35,
  // The world-anchored sub-floor layers. subFloor 0 is the shipped chain,
  // exactly; 1 replaces its grain and undulation with octaves in world
  // wavelengths plus an impact population, so zooming adds detail instead
  // of exchanging it.
  // Both off: the chain behaves exactly as it did. crisp drops the
  // softening steps that only make sense when a picture is going to be
  // resampled anyway; bands quantises the tone.
  crisp: 0, bands: 0, shadows: 1, popPx: 2.5, clastPx: 2.2,
  subFloor: 1, subRough: 0.045, subGrit: 1.8, subCraters: 1.15,
  popDensity: 0.75, subMottle: 0.20, cosineBowl: 1,
  clasts: 1.0, clastDensity: 0.30,
  dMin: 0.03, dMax: 0.20, rim: 0.34, rimWidth: 0.16, ejecta: 0.55,
  floorFlat: 0.30
};
const DEFAULT_CRATER = {
  // everyLevel is OFF for a reason worth stating: each rung's macro is
  // the LIT output of the rung above, and formRelief reads that shading
  // back as height. Carve the same crater at four rungs of a deep ladder
  // and its wall is shaded four times -- measured on a mare crater, peak
  // luminance 196/255 against 50 for the same crater carved once. The
  // craters are world-anchored, so the rung being looked at regenerates
  // all of them anyway. Turn it on to see the compounding.
  on: 1, everyLevel: 0, deepest: 1,
  depth: 1.0, dMin: 0.03, dMax: 0.20, rim: 0.34, rimWidth: 0.16,
  floorFlat: 0.30, ejecta: 0.55, albedo: 0.30, sizeScale: 1.0, cosineBowl: 1
};

return { GenerateChain, MakeLiveChain, LIVE_RUNGS,
         CarveCraters, CraterProfile, CraterPlace, CraterLatLon,
         MakeNoiseFrame, RampColor, ChainSpans, SourceTexels,
         MOON_KM_PER_DEG, DEG2RAD, MACRO_KM, LADDER_MAX_LEVELS,
         CRATERS, DEFAULT_TUNE, DEFAULT_CRATER };
})();
/* ===== CHAIN END ===================================================== */
