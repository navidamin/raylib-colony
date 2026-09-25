/* DomeForge3D — real-time 3D view, ray-marched from signed distance fields in a WebGL fragment shader.
 * Two scenes from the same config: a single dome, or the whole base (ground, roads, nine domes).
 *
 *   const v = DomeForge3D.create(gl);
 *   v.setConfig(cfg, 'unit');                  // single dome ('unit' | 'central')
 *   v.setConfig(cfg, 'base', DomeForgeBase);   // full assembly
 *   v.setCamera({ yaw, pitch, dist }); v.render(w, h);
 */
const DomeForge3D = (function () {
  'use strict';

  // global (per-scene) parameter slots -> uP[]
  const PARAMS = [
    'ambient', 'diffuse', 'shininess', 'specInt', 'glintW', 'glintH', 'glintStrength',
    'rimInt', 'rimPow', 'limbDark', 'limbPow', 'edgeLine',
    'hexS', 'hexRot', 'hexLineRad', 'hexLineDark', 'hexLineLight', 'facetVar', 'facetBevel', 'facetShade',
    'litBase', 'litNear', 'litAmount',
    'frameAmbient', 'frameDiffuse', 'frameSpec', 'frameShine', 'metalEnv', 'edgeLight', 'grain', 'grainScale', 'outline',
    'seed', 'shadow', 'bgOn', 'bgNoise', 'pixAngle', 'lightGlow', 'corner', 'rot',
    'roadW', 'curbW', 'fillet', 'filletDome', 'roadTop', 'groundZ', 'kerbR', 'laneW', 'laneDash', 'laneGap', 'laneOn',
    'craterBig', 'craterSmall', 'craterDepth', 'craterRim', 'groundMottle', 'curbSeg',
    'lightLen', 'lightW', 'lightSig',
  ];
  // per-kind dome geometry slots -> uKindA[] (unit) / uKindB[] (central)
  const KIND = ['ringW', 'Wi', 'Wl', 'T', 'rb', 'innerDrop', 'lipDrop', 'domeR', 'domeDrop', 'hw', 'hh', 'st', 'rc', 'cham', 'fillet', 're', 'rho', 'lightPt', 'lightPsig', 'nSock', 'inset'];
  const NP = PARAMS.length, NK = KIND.length;
  const defines = PARAMS.map((n, i) => `#define P_${n} uP[${i}]`).join('\n');
  const kindSel = KIND.map((n, i) => `  d.${n} = k < 0.5 ? uKindA[${i}] : uKindB[${i}];`).join('\n');
  const kindDecl = KIND.map(n => `float ${n};`).join(' ');

  const VERT = `attribute vec2 aPos; void main(){ gl_Position = vec4(aPos, 0.0, 1.0); }`;

  const FRAG = `
precision highp float;
uniform vec2 uRes;
uniform vec3 uCamPos, uRight, uUp, uFwd;
uniform float uTanFov;
uniform vec3 uKey, uSpecL, uRimL, uEnvUp;
uniform vec3 uMetal, uBg, uLightCol, uRoadCol, uKerbCol, uLaneCol, uGroundCol;
uniform int uDomeN, uSides, uSegN, uGround;
uniform vec4 uDomeP[9];        // x, y, z, scale
uniform vec2 uDomeK[9];        // kind, socket start angle
uniform vec3 uDomeCol[9];
uniform float uSockRho[8];     // primary dome only (polygon rims)
uniform vec4 uSeg[16];         // road segments ax, ay, bx, by
uniform vec4 uRing;            // ring road cx, cy, r, w (w <= 0 -> none)
uniform float uKindA[${NK}], uKindB[${NK}];
uniform int uLightN;
uniform vec4 uLights[32];       // optional rim bars: x, y, orientation, scale
uniform float uP[${NP}];
${defines}

struct Dome { ${kindDecl} };
Dome getDome(float k) {
  Dome d;
${kindSel}
  return d;
}

// ---------- helpers ----------
float hash21(vec2 p, float s) { p = fract(p * vec2(0.3183099, 0.3678794) + s * 0.1173); p += dot(p, p + 19.19); return fract(p.x * p.y); }
float hash31(vec3 p, float s) { p = fract(p * 0.3183099 + s * 0.1173); p += dot(p, p.yzx + 19.19); return fract((p.x + p.y) * p.z); }
float sstep(float a, float b, float x) { return smoothstep(a, b, x); }
float patch3(vec3 p, float cell, float seed, float soft) {
  vec3 g = floor(p / cell);
  float d1 = 1e9, d2 = 1e9, v1 = 0.5, v2 = 0.5;
  for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
    vec3 c = g + vec3(float(i), float(j), float(k));
    vec3 jt = vec3(hash31(c, seed), hash31(c, seed + 1.0), hash31(c, seed + 2.0));
    vec3 q = (c + jt) * cell;
    float d = dot(p - q, p - q);
    float v = hash31(c, seed + 3.0);
    if (d < d1) { d2 = d1; v2 = v1; d1 = d; v1 = v; } else if (d < d2) { d2 = d; v2 = v; }
  }
  float w = smoothstep(0.0, soft, (sqrt(d2) - sqrt(d1)) / cell);
  return mix((v1 + v2) * 0.5, v1, w);
}
float patch2(vec2 p, float cell, float seed, float soft) {
  vec2 g = floor(p / cell);
  float d1 = 1e9, d2 = 1e9, v1 = 0.5, v2 = 0.5;
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
    vec2 c = g + vec2(float(i), float(j));
    vec2 q = (c + vec2(hash21(c, seed), hash21(c, seed + 1.0))) * cell;
    float d = dot(p - q, p - q);
    float v = hash21(c, seed + 2.0);
    if (d < d1) { d2 = d1; v2 = v1; d1 = d; v1 = v; } else if (d < d2) { d2 = d; v2 = v; }
  }
  float w = smoothstep(0.0, soft, (sqrt(d2) - sqrt(d1)) / cell);
  return mix((v1 + v2) * 0.5, v1, w);
}
float sdBox2(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdRoundRect(vec2 p, vec2 b, float rc) { return sdBox2(p, b - rc) - rc; }
float sdChamferRect(vec2 p, vec2 b, float c) { vec2 a = abs(p); return max(max(a.x - b.x, a.y - b.y), (a.x + a.y - (b.x + b.y - c)) * 0.7071068); }
float smin(float a, float b, float k) { if (k <= 0.0) return min(a, b); float h = max(k - abs(a - b), 0.0) / k; return min(a, b) - h * h * k * 0.25; }
float slab(float d2, float z, float zTop, float zBot, float rb) {
  vec2 wv = vec2(d2 + rb, abs(z - (zTop + zBot) * 0.5) - ((zTop - zBot) * 0.5 - rb));
  return min(max(wv.x, wv.y), 0.0) + length(max(wv, 0.0)) - rb;
}

// ---------- one dome (local frame: radius 1, plate top at z = 0) ----------
float outline2(vec2 xy, Dome dm, bool primary) {
  float r = length(xy), R = 1.0 + dm.ringW;
  if (!primary || uSides < 3) return r - R;
  float sector = 6.2831853 / float(uSides), ap = R - P_corner;
  float ang = atan(xy.y, xy.x);
  float base = P_rot + floor((ang - P_rot) / sector + 0.5) * sector;
  float a = ang - base, px = r * cos(a), py = r * sin(a);
  float d;
  if (px <= ap) d = px - ap;
  else { float t = ap * tan(sector * 0.5); float ey = clamp(py, -t, t); d = length(vec2(px - ap, py - ey)); }
  return d - P_corner;
}
float sdGlassD(vec3 p, Dome dm) {
  float s = length(p - vec3(0.0, 0.0, -dm.domeDrop)) - dm.domeR;
  return max(s, -dm.T - p.z);
}
vec3 sockPos(int i, Dome dm, float start, bool primary) {
  float a = start + float(i) * 6.2831853 / max(dm.nSock, 1.0);
  float rho = dm.rho;
  if (primary && uSides >= 3) { for (int k = 0; k < 8; k++) if (k == i) rho = uSockRho[k]; }
  return vec3(cos(a), sin(a), rho);
}
float sdMetalD(vec3 p, Dome dm, float start, bool primary) {
  float rho = length(p.xy), dOut = outline2(p.xy, dm, primary);
  float w = dm.ringW, T = dm.T;
  float m0 = 1.0 + dm.Wi;
  float d = slab(max(m0 - rho, dOut + dm.Wl), p.z, 0.0, -T, dm.rb);
  float i0 = 0.97, i1 = m0 + 0.06 * w;
  d = min(d, slab(max(i0 - rho, rho - i1), p.z, -dm.innerDrop, -T, dm.rb * 0.6));
  if (dm.Wl > 0.0) d = min(d, slab(max(dOut, -dOut - dm.Wl - 0.08 * w), p.z, -dm.lipDrop, -T, dm.rb * 0.5));
  float gw = 0.03 * w;
  float gr = max(abs(rho - (m0 + gw)) - gw, -(p.z + 0.12 * w));
  d = max(d, -gr);
  int n = int(dm.nSock + 0.5);
  for (int i = 0; i < 8; i++) {
    if (i >= n) break;
    vec3 sp = sockPos(i, dm, start, primary);
    vec2 dir = sp.xy, tng = vec2(-dir.y, dir.x);
    vec2 lp = vec2(dot(p.xy, tng), dot(p.xy, dir) - sp.z);
    vec2 b = vec2(dm.hw, dm.hh), bi = vec2(dm.hw - dm.st, dm.hh - dm.st);
    float d2o, d2i;
    if (dm.cham > 0.5) { d2o = sdChamferRect(lp, b, dm.rc); d2i = sdChamferRect(lp, bi, max(0.0, dm.rc - dm.st * 0.6)); }
    else { d2o = sdRoundRect(lp, b, dm.rc); d2i = sdRoundRect(lp, bi, max(0.002, dm.rc - dm.st)); }
    float d2 = max(d2o, -d2i);
    float re = dm.re;
    vec2 wv = vec2(d2 + re, abs(p.z + T * 0.5) - (T * 0.5 - re));
    float dl = min(max(wv.x, wv.y), 0.0) + length(max(wv, 0.0)) - re;
    d = smin(d, dl, dm.fillet);
  }
  return d;
}
float sdLightD(vec3 p, Dome dm, float start, bool primary) {
  if (P_lightGlow <= 0.0 || dm.lightPt <= 0.0) return 1e9;
  float d = 1e9;
  int n = int(dm.nSock + 0.5);
  for (int i = 0; i < 8; i++) {
    if (i >= n) break;
    vec3 sp = sockPos(i, dm, start, primary);
    d = min(d, length(p - vec3(sp.xy * sp.z, -dm.T * 0.45)) - dm.lightPt);
  }
  return d;
}
float lightDist2D(vec2 xy, Dome dm, float start, bool primary) {
  float d = 1e9;
  int n = int(dm.nSock + 0.5);
  for (int i = 0; i < 8; i++) { if (i >= n) break; vec3 sp = sockPos(i, dm, start, primary); d = min(d, length(xy - sp.xy * sp.z)); }
  return d;
}

// ---------- roads ----------
float road2(vec2 xy) {
  float d = 1e9;
  for (int i = 0; i < 16; i++) {
    if (i >= uSegN) break;
    vec4 s = uSeg[i];
    vec2 ab = s.zw - s.xy; float len = max(length(ab), 1e-4); vec2 u = ab / len;
    vec2 q = xy - s.xy; float h = dot(q, u); float pp = abs(q.x * u.y - q.y * u.x);
    vec2 w = vec2(pp - P_roadW * 0.5, abs(h - len * 0.5) - len * 0.5);
    float dp = min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
    d = d > 1e8 ? dp : smin(d, dp, P_fillet);
  }
  if (uRing.w > 0.0) { float dp = abs(length(xy - uRing.xy) - uRing.z) - uRing.w * 0.5; d = d > 1e8 ? dp : smin(d, dp, P_fillet); }
  if (d > 1e8) return d;
  for (int i = 0; i < 9; i++) {
    if (i >= uDomeN) break;
    vec4 dp4 = uDomeP[i];
    float rw = uDomeK[i].x < 0.5 ? uKindA[0] : uKindB[0];
    float dp = length(xy - dp4.xy) - dp4.w * (1.0 + rw);
    d = smin(d, dp, P_filletDome);
  }
  return d;
}
// nearest road centreline: x = across, y = along (for the lane dashes)
vec2 roadLane(vec2 xy) {
  float best = 1e9; vec2 r = vec2(1e9, 0.0);
  for (int i = 0; i < 16; i++) {
    if (i >= uSegN) break;
    vec4 s = uSeg[i];
    vec2 ab = s.zw - s.xy; float len = max(length(ab), 1e-4); vec2 u = ab / len;
    vec2 q = xy - s.xy; float h = dot(q, u); float pp = abs(q.x * u.y - q.y * u.x);
    float dp = max(pp, abs(h - len * 0.5) - len * 0.5);
    if (dp < best) { best = dp; r = vec2(pp, h); }
  }
  if (uRing.w > 0.0) { float rr = length(xy - uRing.xy); float pp = abs(rr - uRing.z); if (pp < best) { best = pp; r = vec2(pp, atan(xy.y - uRing.y, xy.x - uRing.x) * uRing.z); } }
  return r;
}
float sdRoad(vec3 p) {
  if (uSegN == 0 && uRing.w <= 0.0) return 1e9;
  float d2 = road2(p.xy);
  if (d2 > 0.6) return d2;
  float zc = (P_roadTop + P_groundZ - 0.05) * 0.5, hz = (P_roadTop - P_groundZ + 0.05) * 0.5;
  vec2 wv = vec2(d2, abs(p.z - zc) - hz);
  float sl = min(max(wv.x, wv.y), 0.0) + length(max(wv, 0.0));
  float kerb = length(vec2(d2 + P_kerbR, p.z - P_roadTop)) - P_kerbR;
  return min(sl, kerb);
}

// ---------- scene ----------
void nearestDome(vec2 xy, out vec4 P, out vec2 K, out vec3 col) {
  float best = 1e9; P = vec4(0.0, 0.0, 0.0, 1.0); K = vec2(0.0); col = vec3(0.5);
  for (int i = 0; i < 9; i++) {
    if (i >= uDomeN) break;
    float d = length(xy - uDomeP[i].xy) / uDomeP[i].w;
    if (d < best) { best = d; P = uDomeP[i]; K = uDomeK[i]; col = uDomeCol[i]; }
  }
}
float sdScene(vec3 p) {
  vec4 P; vec2 K; vec3 c;
  nearestDome(p.xy, P, K, c);
  Dome dm = getDome(K.x);
  bool primary = uDomeN == 1;
  vec3 pl = (p - P.xyz) / P.w;
  float d = min(min(sdMetalD(pl, dm, K.y, primary), sdGlassD(pl, dm)), sdLightD(pl, dm, K.y, primary)) * P.w;
  d = min(d, sdRoad(p));
  if (uGround == 1) d = min(d, p.z - P_groundZ);
  return d;
}
vec3 calcNormal(vec3 p) {
  vec2 e = vec2(0.0007, -0.0007);
  return normalize(e.xyy * sdScene(p + e.xyy) + e.yyx * sdScene(p + e.yyx) + e.yxy * sdScene(p + e.yxy) + e.xxx * sdScene(p + e.xxx));
}
float calcAO(vec3 p, vec3 n) {
  float o = 0.0, s = 1.0;
  for (int i = 1; i <= 5; i++) { float h = 0.01 * float(i) * float(i); float d = sdScene(p + n * h); o += (h - d) * s; s *= 0.72; }
  return clamp(1.0 - 2.2 * o, 0.0, 1.0);
}
float softShadow(vec3 p, vec3 l) {
  float res = 1.0, t = 0.02;
  for (int i = 0; i < 22; i++) {
    float d = sdScene(p + l * t);
    res = min(res, 9.0 * d / t);
    if (res < 0.005 || t > 4.0) break;
    t += clamp(d, 0.01, 0.2);
  }
  return clamp(res, 0.0, 1.0);
}

// ---------- hex cells ----------
void hexCell(vec2 uv, out float edge, out vec2 centre, out vec2 id) {
  float q = 0.5773503 * uv.x - uv.y / 3.0, r = uv.y * 2.0 / 3.0;
  float rx = floor(q + 0.5), rz = floor(r + 0.5), ry = floor(-q - r + 0.5);
  float dx = abs(rx - q), dy = abs(ry + q + r), dz = abs(rz - r);
  if (dx > dy && dx > dz) rx = -ry - rz; else if (dy <= dz) rz = -rx - ry;
  centre = vec2(1.7320508 * (rx + rz * 0.5), 1.5 * rz);
  vec2 l = uv - centre;
  float h = max(abs(l.x), max(abs(l.x * 0.5 + l.y * 0.8660254), abs(l.x * 0.5 - l.y * 0.8660254)));
  edge = 0.8660254 - h;
  id = vec2(rx, rz);
}

// optional warm bar lights on the rim beside each socket
vec3 rimBars(vec2 xy) {
  vec3 e = vec3(0.0);
  for (int i = 0; i < 32; i++) {
    if (i >= uLightN) break;
    vec4 L = uLights[i];
    vec2 q = xy - L.xy;
    float ca = cos(L.z), sa = sin(L.z);
    vec2 lp = vec2(q.x * ca + q.y * sa, -q.x * sa + q.y * ca) / L.w;
    float d = sdRoundRect(lp, vec2(P_lightLen * 0.5, P_lightW * 0.5), P_lightW * 0.5) * L.w;
    float core = 1.0 - smoothstep(-0.0015, 0.0015, d);
    float dd = max(d, 0.0), sg = P_lightSig * L.w;
    float gl = exp(-dd * dd / (2.0 * sg * sg)) * P_lightGlow;
    e += uLightCol * gl * 0.9 + mix(uLightCol, vec3(1.0), 0.6) * core * 1.6;
  }
  return e;
}

// ---------- materials ----------
vec3 envMetal(vec3 N, vec3 V, float ndl, vec3 base, float grain, float bevel) {
  vec3 L = uKey;
  float ndv = max(dot(N, V), 0.0);
  vec3 Hm = normalize(L + V);
  float spec = pow(max(dot(N, Hm), 0.0), P_frameShine) * P_frameSpec;
  float up = dot(N, uEnvUp);
  float sky = sstep(-0.6, 0.85, up), fl = sstep(-0.25, -0.95, up) * 0.5, fres = pow(1.0 - ndv, 3.0) * 0.3;
  float E = mix(0.72, 0.5 + 0.42 * sky + fl + fres, P_metalEnv);
  float tl = length(N.xy);
  float bev = sstep(0.05, 0.3, tl) * (1.0 - sstep(0.85, 0.97, tl)) * bevel;
  float edge = tl > 1e-4 ? P_edgeLight * max(dot(N.xy / tl, normalize(L.xy + vec2(1e-5))), 0.0) * bev : 0.0;
  float k = P_frameAmbient * E + P_frameDiffuse * ndl + grain * (1.0 - tl * tl);
  return base * k + spec + edge;
}
vec3 shadeMetal(vec3 p, vec3 pl, vec3 N, vec3 V, float ao, float sh, Dome dm, float start, bool primary) {
  float ndl = max(dot(N, uKey), 0.0) * sh;
  float gs = P_grainScale;
  float big = patch3(pl, 0.095 * gs, P_seed, 0.5) - 0.5;
  float mid = patch3(pl + 37.1, 0.047 * gs, P_seed + 40.0, 0.55) - 0.5;
  vec3 col = envMetal(N, V, ndl, uMetal, (big * 1.5 + mid * 0.7) * P_grain * 2.0, 1.0);
  float ndv = max(dot(N, V), 0.0);
  col *= mix(0.55, 1.0, ao);
  col *= 1.0 - P_outline * (1.0 - sstep(0.05, 0.42, ndv));
  if (P_lightGlow > 0.0 && dm.lightPt > 0.0 && dm.nSock > 0.5) {
    float dd = max(lightDist2D(pl.xy, dm, start, primary) - dm.lightPt, 0.0);
    col += uLightCol * exp(-dd * dd / (2.0 * dm.lightPsig * dm.lightPsig)) * P_lightGlow * 0.9;
  }
  if (uLightN > 0 && pl.z > -0.02) col += rimBars(p.xy) * sstep(0.5, 0.85, N.z);
  return col;
}
vec3 shadeGlass(vec3 pl, vec3 N, vec3 V, float t, float ao, float sh, Dome dm, vec3 base, float sc) {
  vec3 L = uKey;
  float ndv = max(dot(N, V), 0.0);
  float ndl = max(dot(N, L), 0.0) * sh;
  float k = P_ambient + P_diffuse * ndl;
  vec3 nl = normalize(pl - vec3(0.0, 0.0, -dm.domeDrop));
  float th = acos(clamp(nl.z, -1.0, 1.0)) * dm.domeR;
  vec2 dir2 = length(nl.xy) > 1e-5 ? normalize(nl.xy) : vec2(1.0, 0.0);
  float cr = cos(P_hexRot), sr = sin(P_hexRot);
  vec2 uv = th * vec2(dir2.x * cr - dir2.y * sr, dir2.x * sr + dir2.y * cr);
  float s = P_hexS;
  float edge; vec2 c, id;
  hexCell(uv / s, edge, c, id);
  float rnd = hash21(id, P_seed), rnd2 = hash21(id, P_seed + 77.0);
  float cellMul = 1.0 + (rnd2 - 0.5) * P_facetVar * 2.0;
  float thc = length(c) * s / dm.domeR;
  vec2 dc = length(c) > 1e-6 ? normalize(c) : vec2(1.0, 0.0);
  dc = vec2(dc.x * cr + dc.y * sr, -dc.x * sr + dc.y * cr);
  vec3 Nc = vec3(sin(thc) * dc, cos(thc));
  vec3 H = normalize(uSpecL + V);
  float cndh = max(dot(Nc, H), 0.0);
  float cndl = max(dot(Nc, L), 0.0) * sh;
  k = mix(k, P_ambient + P_diffuse * cndl, P_facetShade);
  vec3 off = N - Nc; float ol = length(off);
  vec3 offn = ol > 1e-5 ? off / ol : vec3(0.0);
  cellMul += P_facetBevel * dot(offn, L);
  float litP = P_litBase + P_litNear * pow(cndh, P_shininess * 0.35 + 2.0);
  float lit = rnd < litP ? P_litAmount * (0.55 + 0.45 * rnd2) : 0.0;
  float fp = P_pixAngle * t / max(ndv, 0.15) / sc;
  float halfW = max(P_hexLineRad * 0.5, fp * 0.35);
  float line = (1.0 - sstep(halfW - fp * 0.6, halfW + fp * 0.6, edge * s)) * (0.35 + 0.65 * ndv);
  float litSide = max(dot(offn, L), 0.0);
  vec3 col = base * k * cellMul;
  col = mix(col, col * 1.35 + 0.12, lit);
  col *= 1.0 - P_hexLineDark * line * (1.0 - litSide) + P_hexLineLight * line * litSide;
  float spec = pow(max(dot(N, H), 0.0), P_shininess) * P_specInt;
  float specC = pow(cndh, P_shininess) * P_specInt;
  spec = clamp(mix(spec, specC, P_facetShade), 0.0, 1.0);
  vec3 specCol = mix(base, vec3(1.0), 0.62), rimCol = mix(base, vec3(1.0), 0.45);
  col = mix(col, specCol, spec);
  col *= 1.0 - P_limbDark * pow(1.0 - ndv, P_limbPow);
  col *= mix(0.6, 1.0, ao);
  float rim = pow(1.0 - ndv, P_rimPow) * max(dot(N, uRimL), 0.0) * P_rimInt;
  col += rim * rimCol;
  float el = P_edgeLine * sstep(0.3, 0.03, ndv) * (0.25 + 0.75 * max(dot(N, L), 0.0));
  col += el * rimCol;
  vec3 R = reflect(-V, N);
  vec3 Tg = normalize(cross(uSpecL, uUp)), Bg = cross(uSpecL, Tg);
  float gd = abs(dot(R, Tg)) / P_glintW + abs(dot(R, Bg)) / P_glintH;
  float ga = (1.0 - sstep(0.82, 1.06, gd)) * P_glintStrength * step(0.0, dot(R, uSpecL));
  col = mix(col, mix(specCol, vec3(1.0), 0.6), ga);
  return col;
}
vec3 shadeRoad(vec3 p, vec3 N, vec3 V, float ao, float sh, bool kerb) {
  float ndl = max(dot(N, uKey), 0.0) * sh;
  if (kerb) {
    float g = (patch2(p.xy + 11.0, 0.07, P_seed + 5.0, 0.5) - 0.5) * 0.18;
    float segMul = 1.0;
    if (P_curbSeg > 0.0) { vec2 ln = roadLane(p.xy); float fr = fract(ln.y / P_curbSeg); float de = min(fr, 1.0 - fr) * P_curbSeg; segMul = 1.0 - 0.3 * 0.55 * (1.0 - sstep(0.004, 0.012, de)); }
    vec3 col = envMetal(N, V, ndl, uKerbCol, g, 0.4) * segMul;
    col *= mix(0.6, 1.0, ao);
    return col;
  }
  float mot = (patch2(p.xy, 0.18, P_seed + 9.0, 0.5) - 0.5) * 0.16 + (hash21(floor(p.xy * 400.0), P_seed + 2.0) - 0.5) * 0.1;
  vec3 col = uRoadCol * (1.0 + mot) * (0.45 + 0.7 * ndl);
  if (P_laneOn > 0.5) {
    vec2 ln = roadLane(p.xy);
    float per = P_laneDash + P_laneGap;
    float on = fract(ln.y / per) < P_laneDash / per ? 1.0 : 0.0;
    float la = (1.0 - sstep(P_laneW * 0.5 - 0.004, P_laneW * 0.5 + 0.004, ln.x)) * on * 0.6;
    col = mix(col, uLaneCol * (0.5 + 0.6 * ndl), la);
  }
  col *= mix(0.5, 1.0, ao);
  return col;
}
// cratered regolith: craters only bend the normal, the plane stays flat
vec3 groundNormal(vec2 xy, out float ao) {
  vec2 n = vec2(0.0); ao = 0.0;
  for (int li = 0; li < 2; li++) {
    float cell = li == 0 ? 1.7 : 0.55, prob = li == 0 ? P_craterBig : P_craterSmall;
    float rMin = li == 0 ? 0.2 : 0.045, rMax = li == 0 ? 0.72 : 0.14, seed = P_seed + (li == 0 ? 10.0 : 20.0);
    vec2 g = floor(xy / cell);
    float bu = 1e9, bR = 1.0; vec2 bd = vec2(0.0);
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
      vec2 c = g + vec2(float(i), float(j));
      if (hash21(c, seed) > prob) continue;
      vec2 q = (c + vec2(hash21(c, seed + 1.0), hash21(c, seed + 2.0))) * cell;
      float Rr = mix(rMin, rMax, pow(hash21(c, seed + 3.0), 2.2));
      vec2 d = xy - q; float u = length(d) / Rr;
      if (u < bu) { bu = u; bR = Rr; bd = d; }
    }
    if (bu < 1.35) {
      vec2 dir = bd / max(length(bd), 1e-5);
      float rimW = 0.18, gg = exp(-((bu - 1.02) * (bu - 1.02)) / (2.0 * rimW * rimW));
      float s = (bu < 1.0 ? 0.5 * P_craterDepth * bu : 0.0) + 0.08 * P_craterRim * (-(bu - 1.02) / (rimW * rimW)) * gg;
      n += -s * dir;
      if (bu < 1.0) ao += 0.3 * (1.0 - bu * bu) * P_craterDepth;
    }
  }
  return normalize(vec3(n, 1.0));
}
vec3 shadeGround(vec3 p, float sh) {
  float cao;
  vec3 N = groundNormal(p.xy, cao);
  float ndl = max(dot(N, uKey), 0.0) * sh;
  float mot = (patch2(p.xy, 1.1, P_seed, 0.6) - 0.5) * P_groundMottle * 2.0 + (patch2(p.xy + 30.0, 0.28, P_seed + 1.0, 0.6) - 0.5) * P_groundMottle
    + (patch2(p.xy - 15.0, 0.05, P_seed + 3.0, 0.4) - 0.5) * 0.1 + (hash21(floor(p.xy * 300.0), P_seed + 2.0) - 0.5) * 0.14;
  return uGroundCol * (0.45 + 0.75 * ndl) * (1.0 - cao) * (1.0 + mot);
}

void main() {
  vec2 uv = (gl_FragCoord.xy - uRes * 0.5) / uRes.y * 2.0;
  vec3 rd = normalize(uFwd + (uv.x * uRight + uv.y * uUp) * uTanFov);
  vec3 ro = uCamPos;
  vec4 outc = vec4(0.0);
  if (P_bgOn > 0.5) { float n = (hash21(gl_FragCoord.xy, P_seed + 1.0) - 0.5) * P_bgNoise; outc = vec4(uBg + n, 1.0); }
  // the ground plane is infinite: intersect it analytically and only march up to it
  float tG = 1e9;
  if (uGround == 1 && rd.z < -1e-4) tG = (P_groundZ - ro.z) / rd.z;
  float t = 0.0, tEnd = min(80.0, tG);
  bool hit = false;
  for (int i = 0; i < 140; i++) {
    vec3 p = ro + rd * t;
    float d = sdScene(p);
    if (d < 0.0006 * t) { hit = true; break; }
    t += d * 0.9;
    if (t > tEnd) break;
  }
  if (!hit && tG < 1e8) { hit = true; t = tG; }
  if (hit) {
    vec3 p = ro + rd * t;
    vec3 N = calcNormal(p);
    vec3 V = -rd;
    float ao = calcAO(p, N);
    vec4 P; vec2 K; vec3 dcol;
    nearestDome(p.xy, P, K, dcol);
    Dome dm = getDome(K.x);
    bool primary = uDomeN == 1;
    vec3 pl = (p - P.xyz) / P.w;
    float dg = sdGlassD(pl, dm) * P.w, dmt = sdMetalD(pl, dm, K.y, primary) * P.w, dl = sdLightD(pl, dm, K.y, primary) * P.w;
    float dr = sdRoad(p), dgr = uGround == 1 ? p.z - P_groundZ : 1e9;
    float m = min(min(min(dg, dmt), min(dl, dr)), dgr);
    vec3 col;
    if (m == dl) col = mix(uLightCol, vec3(1.0), 0.6) * (1.2 + 0.4 * P_lightGlow);
    else {
      float sh = P_shadow > 0.5 ? softShadow(p + N * 0.012, uKey) : 1.0;
      if (m == dg) col = shadeGlass(pl, N, V, t, ao, sh, dm, dcol, P.w);
      else if (m == dmt) col = shadeMetal(p, pl, N, V, ao, sh, dm, K.y, primary);
      else if (m == dr) {
        float d2 = road2(p.xy);
        float kerb = length(vec2(d2 + P_kerbR, p.z - P_roadTop)) - P_kerbR;
        col = shadeRoad(p, N, V, ao, sh, kerb < 0.004 && d2 > -P_curbW - 0.002);
      } else col = shadeGround(p, sh) * mix(0.5, 1.0, ao);
    }
    outc = vec4(col, 1.0);
  }
  gl_FragColor = vec4(clamp(outc.rgb, 0.0, 1.0), outc.a);
}`;

  function hexToRgb(hex) {
    const m = /^#?([0-9a-f]{6})$/i.exec(String(hex).trim()); if (!m) return [0.5, 0.5, 0.5];
    const n = parseInt(m[1], 16); return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }
  const DEG = Math.PI / 180;

  function create(gl) {
    function compile(type, src) {
      const sh = gl.createShader(type); gl.shaderSource(sh, src); gl.compileShader(sh);
      if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) throw new Error('shader: ' + gl.getShaderInfoLog(sh));
      return sh;
    }
    const prog = gl.createProgram();
    gl.attachShader(prog, compile(gl.VERTEX_SHADER, VERT));
    gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, FRAG));
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error('link: ' + gl.getProgramInfoLog(prog));
    gl.useProgram(prog);
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    const aPos = gl.getAttribLocation(prog, 'aPos');
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);
    const U = {};
    for (const n of ['uRes', 'uCamPos', 'uRight', 'uUp', 'uFwd', 'uTanFov', 'uKey', 'uSpecL', 'uRimL', 'uEnvUp',
      'uMetal', 'uBg', 'uLightCol', 'uRoadCol', 'uKerbCol', 'uLaneCol', 'uGroundCol', 'uDomeN', 'uSides', 'uSegN', 'uGround',
      'uDomeP', 'uDomeK', 'uDomeCol', 'uSockRho', 'uSeg', 'uRing', 'uKindA', 'uKindB', 'uP', 'uLightN', 'uLights'])
      U[n] = gl.getUniformLocation(prog, n) || gl.getUniformLocation(prog, n + '[0]');

    const P = new Float32Array(NP);
    const st = { cfg: null, mode: 'unit', cam: { yaw: -25, pitch: 55, dist: 5.2, fov: 30 }, followLight: true, boundR: 1.4 };
    const set = (name, v) => { P[PARAMS.indexOf(name)] = v; };

    // geometry of one dome kind in its own units (radius 1); sockets keep their absolute size
    function kindParams(cfg, kind, nSockOverride) {
      const K = cfg[kind];
      const Rpx = K.domeRadius * K.size, upx = Rpx / (cfg.unit.size / 256);
      const w = K.ringWidth / K.domeRadius, thick = (cfg.plateThick || 0.45) * w;
      const hh = Math.min(1, Math.max(0.3, cfg.domeHeight || 1)), domeR = (1 + hh * hh) / (2 * hh);
      const shh = cfg.socketH / upx, stt = cfg.socketT / upx;
      const n = cfg.socketOn ? (nSockOverride !== undefined ? nSockOverride : Math.min(8, K.socketCount | 0)) : 0;
      const o = {
        ringW: w, Wi: 0.30 * w, Wl: Math.min(0.6, cfg.outerLip || 0) * w, T: thick,
        rb: Math.min(0.3 * (0.66 * w - (cfg.outerLip || 0) * w) * 0.5, Math.max(0.004, cfg.bevel * 1.1 / Rpx)),
        innerDrop: 0.22 * w, lipDrop: 0.16 * w, domeR, domeDrop: domeR - hh + 0.05,
        hw: cfg.socketW / upx / 2, hh: shh / 2, st: stt, rc: cfg.socketCorner / upx, cham: cfg.socketCorners === 'chamfer' ? 1 : 0,
        fillet: cfg.socketFillet / upx, re: Math.min(thick * 0.3, stt * 0.25),
        rho: 1 + w + shh / 2 - cfg.socketInset / upx,
        lightPt: cfg.socketLights && cfg.lightGlow > 0 ? cfg.lightSize / upx : 0, lightPsig: cfg.lightGlowR * 0.5 / upx,
        nSock: n, inset: cfg.socketInset / upx,
      };
      return { o, Rpx, upx, w };
    }
    function packKind(o) { return new Float32Array(KIND.map(k => o[k])); }

    function setConfig(cfg, mode, base) {
      st.cfg = cfg; st.mode = mode || 'unit';
      const isBase = st.mode === 'base';
      set('ambient', cfg.ambient); set('diffuse', cfg.diffuse); set('shininess', cfg.shininess); set('specInt', cfg.specInt);
      set('glintW', cfg.glintSize * 2.2 * cfg.glintAspect); set('glintH', cfg.glintSize * 2.2 / cfg.glintAspect); set('glintStrength', cfg.glintStrength);
      set('rimInt', cfg.rimInt); set('rimPow', cfg.rimPow); set('limbDark', cfg.limbDark); set('limbPow', cfg.limbPow); set('edgeLine', cfg.edgeLine);
      set('hexS', cfg.hexCells); set('hexRot', cfg.hexRot * DEG);
      set('hexLineDark', cfg.hexLineDark); set('hexLineLight', cfg.hexLineLight); set('facetVar', cfg.facetVar); set('facetBevel', cfg.facetBevel); set('facetShade', cfg.facetShade);
      set('litBase', cfg.litBase); set('litNear', cfg.litNear); set('litAmount', cfg.litAmount);
      set('frameAmbient', cfg.frameAmbient); set('frameDiffuse', cfg.frameDiffuse); set('frameSpec', cfg.frameSpec); set('frameShine', cfg.frameShine);
      set('metalEnv', cfg.metalEnv); set('edgeLight', cfg.edgeLight); set('grain', cfg.grainStyle === 'smooth' ? 0 : cfg.grain); set('grainScale', cfg.grainScale || 1); set('outline', cfg.outline);
      set('seed', (cfg.seed | 0) % 1000); set('shadow', cfg.shadow3d === false ? 0 : 1); set('bgOn', cfg.bgOn ? 1 : 0); set('bgNoise', cfg.bgNoise);
      set('lightGlow', cfg.lightGlow);
      st.followLight = cfg.lightFollowsCamera !== false;
      gl.uniform3fv(U.uMetal, hexToRgb(cfg.frameColor));
      gl.uniform3fv(U.uBg, hexToRgb(cfg.bg));
      gl.uniform3fv(U.uLightCol, hexToRgb(cfg.lightColor || '#ffad55'));

      const domeP = new Float32Array(36), domeK = new Float32Array(18), domeCol = new Float32Array(27);
      const segs = new Float32Array(64); let segN = 0; let ring = [0, 0, 0, 0];
      const rhos = new Float32Array(8);
      let sides = 0, unitKP, centralKP;

      if (!isBase) {
        const kind = st.mode, K = cfg[kind];
        unitKP = centralKP = kindParams(cfg, kind);
        const { o, Rpx, w } = unitKP;
        set('hexLineRad', cfg.hexLine / Rpx);
        sides = (K.sides | 0) >= 3 ? (K.sides | 0) : 0;
        const corner = sides ? Math.min((K.corner || 0) / K.domeRadius, w - 0.01) : 0;
        const rot = sides ? (K.socketStart + (K.socketsAtCorners ? 180 / sides : 0)) * DEG : 0;
        set('corner', corner); set('rot', rot);
        const outline = (x, y) => {
          const r = Math.hypot(x, y), R = 1 + w;
          if (!sides) return r - R;
          const sector = 2 * Math.PI / sides, ap = R - corner, ang = Math.atan2(y, x);
          const b = rot + Math.round((ang - rot) / sector) * sector, a = ang - b;
          const px = r * Math.cos(a), py = r * Math.sin(a);
          let d; if (px <= ap) d = px - ap; else { const tt = ap * Math.tan(sector / 2), ey = Math.max(-tt, Math.min(tt, py)); d = Math.hypot(px - ap, py - ey); }
          return d - corner;
        };
        for (let i = 0; i < o.nSock; i++) {
          const a = (K.socketStart + (360 / o.nSock) * i) * DEG, c = Math.cos(a), sn = Math.sin(a);
          let lo = 1, hi = 2 * (1 + w) + 1;
          for (let k = 0; k < 40; k++) { const m = (lo + hi) / 2; if (outline(m * c, m * sn) < 0) lo = m; else hi = m; }
          rhos[i] = (lo + hi) / 2 + o.hh - o.inset;
        }
        gl.uniform1fv(U.uKindA, packKind(o)); gl.uniform1fv(U.uKindB, packKind(o));
        domeP.set([0, 0, 0, 1], 0); domeK.set([0, K.socketStart * DEG], 0); domeCol.set(hexToRgb(cfg.color), 0);
        gl.uniform1i(U.uDomeN, 1); gl.uniform1i(U.uGround, 0);
        st.boundR = (1 + w) * (sides ? 1 / Math.cos(Math.PI / sides) : 1) + 2 * o.hh + 0.1;
      } else {
        // ---- the whole base, in units of the unit dome radius ----
        const Ru = cfg.unitSize * cfg.unit.domeRadius;            // px per unit
        const lay = base.layout(cfg, 1);
        const c = lay.domes[0];                                   // central sits at the origin
        const A = unitKP = kindParams(cfg, 'unit', cfg.socketOn && cfg.unitSockets ? 2 : 0);
        const B = centralKP = kindParams(cfg, 'central', cfg.socketOn && cfg.centralSockets ? 8 : 0);
        gl.uniform1fv(U.uKindA, packKind(A.o)); gl.uniform1fv(U.uKindB, packKind(B.o));
        set('hexLineRad', cfg.hexLine / A.Rpx);
        set('corner', 0); set('rot', 0);
        const groundZ = -A.o.T - 0.03;
        set('groundZ', groundZ);
        let di = 0;
        for (const d of lay.domes) {
          const isC = d.kind === 'central';
          const scale = isC ? (cfg.central.domeRadius * d.size) / Ru : 1;
          const x = (d.x - c.x) / Ru, y = (d.y - c.y) / Ru;
          const z = groundZ + scale * (isC ? B.o.T : A.o.T);
          domeP.set([x, y, z, scale], di * 4);
          domeK.set([isC ? 1 : 0, (isC ? 90 : (d.angle + 180) % 360) * DEG], di * 2);
          domeCol.set(hexToRgb(isC ? (cfg.centralColor || cfg.color) : d.color), di * 3);
          di++;
        }
        gl.uniform1i(U.uDomeN, di); gl.uniform1i(U.uGround, 1);
        for (const p of lay.prims) {
          if (p.t === 'seg' && segN < 16) { segs.set([(p.ax - c.x) / Ru, (p.ay - c.y) / Ru, (p.bx - c.x) / Ru, (p.by - c.y) / Ru], segN * 4); segN++; }
          else if (p.t === 'ring') ring = [(p.cx - c.x) / Ru, (p.cy - c.y) / Ru, p.r / Ru, p.w / Ru];
        }
        const s = lay.s;
        set('roadW', cfg.roadW * s / Ru); set('curbW', cfg.curbW * s / Ru); set('kerbR', cfg.curbW * s / Ru * 0.5);
        set('fillet', cfg.fillet * s / Ru); set('filletDome', cfg.filletDome * s / Ru);
        set('roadTop', groundZ + (cfg.roadHeight3d || 4) * s / Ru);
        set('laneW', cfg.laneW * s / Ru); set('laneDash', cfg.laneDash * s / Ru); set('laneGap', cfg.laneGap * s / Ru); set('laneOn', cfg.laneOn ? 1 : 0);
        set('curbSeg', cfg.curbSeg * s / Ru);
        set('craterBig', cfg.craterBig); set('craterSmall', cfg.craterSmall); set('craterDepth', cfg.craterDepth); set('craterRim', cfg.craterRim); set('groundMottle', cfg.groundMottle);
        gl.uniform3fv(U.uRoadCol, hexToRgb(cfg.roadColor)); gl.uniform3fv(U.uKerbCol, hexToRgb(cfg.curbColor));
        gl.uniform3fv(U.uLaneCol, hexToRgb(cfg.laneColor)); gl.uniform3fv(U.uGroundCol, hexToRgb(cfg.groundColor));
        st.boundR = (cfg.ringRoadR * s + cfg.roadW * s) / Ru + 0.5;
      }
      // optional rim bars: lightAngle either side of each socket (or one per gap when sockets are crowded)
      const bars = new Float32Array(128); let bn = 0;
      if (cfg.rimLights && cfg.socketOn && cfg.lightGlow > 0) {
        const nDomes = isBase ? Math.min(9, domeP.length / 4) : 1;
        for (let di = 0; di < nDomes; di++) {
          const sc = domeP[di * 4 + 3]; if (sc === 0) break;
          const kind = domeK[di * 2], start = domeK[di * 2 + 1] / DEG;
          const kp = kind < 0.5 ? unitKP : centralKP;
          const n = kp.o.nSock; if (!n) continue;
          const spacing = 360 / n, single = spacing / 2 <= cfg.lightAngle + 4;
          for (let i = 0; i < n; i++) {
            const sa = start + spacing * i;
            const angs = single ? [sa + spacing / 2] : [sa - cfg.lightAngle, sa + cfg.lightAngle];
            for (const ad of angs) {
              if (bn >= 32) break;
              const a = ad * DEG, r0 = (1 + kp.w - kp.w * cfg.lightRadial) * sc;
              bars.set([domeP[di * 4] + r0 * Math.cos(a), domeP[di * 4 + 1] + r0 * Math.sin(a), a + Math.PI / 2, sc], bn * 4); bn++;
            }
          }
        }
        set('lightLen', cfg.lightLen / unitKP.upx); set('lightW', cfg.lightW / unitKP.upx); set('lightSig', cfg.lightGlowR * 0.45 / unitKP.upx);
      }
      gl.uniform1i(U.uLightN, bn); gl.uniform4fv(U.uLights, bars);
      gl.uniform1i(U.uSides, sides);
      gl.uniform1fv(U.uSockRho, rhos);
      gl.uniform4fv(U.uDomeP, domeP); gl.uniform2fv(U.uDomeK, domeK); gl.uniform3fv(U.uDomeCol, domeCol);
      gl.uniform1i(U.uSegN, segN); gl.uniform4fv(U.uSeg, segs); gl.uniform4fv(U.uRing, ring);
    }
    function setCamera(c) { Object.assign(st.cam, c); }

    function render(w, h) {
      const cfg = st.cfg, cam = st.cam;
      gl.viewport(0, 0, w, h);
      gl.uniform2f(U.uRes, w, h);
      const yaw = cam.yaw * DEG, pitch = Math.max(-89, Math.min(89, cam.pitch)) * DEG;
      const tgt = [0, 0, st.mode === 'base' ? -0.05 : 0.05];
      const pos = [tgt[0] + cam.dist * Math.cos(pitch) * Math.sin(yaw), tgt[1] - cam.dist * Math.cos(pitch) * Math.cos(yaw), tgt[2] + cam.dist * Math.sin(pitch)];
      const fwd = norm([tgt[0] - pos[0], tgt[1] - pos[1], tgt[2] - pos[2]]);
      const wu = Math.abs(fwd[2]) > 0.999 ? [0, 1, 0] : [0, 0, 1];
      const right = norm(cross(fwd, wu)), up = cross(right, fwd);
      gl.uniform3fv(U.uCamPos, pos); gl.uniform3fv(U.uRight, right); gl.uniform3fv(U.uUp, up); gl.uniform3fv(U.uFwd, fwd);
      const tanF = Math.tan(cam.fov * DEG / 2);
      gl.uniform1f(U.uTanFov, tanF);
      set('pixAngle', 2 * tanF / h);
      const R = st.followLight ? right : [1, 0, 0], Up = st.followLight ? up : [0, 1, 0], Tw = st.followLight ? fwd.map(v => -v) : [0, 0, 1];
      const toW = v => norm([v[0] * R[0] + v[1] * Up[0] + v[2] * Tw[0], v[0] * R[1] + v[1] * Up[1] + v[2] * Tw[1], v[0] * R[2] + v[1] * Up[2] + v[2] * Tw[2]]);
      const dirAE = (az, el) => [Math.cos(az * DEG) * Math.cos(el * DEG), Math.sin(az * DEG) * Math.cos(el * DEG), Math.sin(el * DEG)];
      const Hv = norm([cfg.hlX, cfg.hlY, Math.sqrt(Math.max(0.02, 1 - cfg.hlX * cfg.hlX - cfg.hlY * cfg.hlY))]);
      const Lsv = [2 * Hv[2] * Hv[0], 2 * Hv[2] * Hv[1], 2 * Hv[2] * Hv[2] - 1];
      gl.uniform3fv(U.uKey, toW(dirAE(cfg.lightAz, cfg.lightEl)));
      gl.uniform3fv(U.uSpecL, toW(Lsv));
      gl.uniform3fv(U.uRimL, toW(dirAE(cfg.rimAz, 10)));
      gl.uniform3fv(U.uEnvUp, toW([0, 1, 0]));
      gl.uniform1fv(U.uP, P);
      gl.clearColor(0, 0, 0, 0); gl.clear(gl.COLOR_BUFFER_BIT);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
    }
    function norm(v) { const l = Math.hypot(v[0], v[1], v[2]) || 1; return [v[0] / l, v[1] / l, v[2] / l]; }
    function cross(a, b) { return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]; }
    return { setConfig, setCamera, render, state: st };
  }

  return { create, PARAMS };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = DomeForge3D;
