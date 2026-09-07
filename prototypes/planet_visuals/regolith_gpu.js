"use strict";
/* ===== REGOLITH GPU ====================================================
   The last rung of regolith_chain.js, as fragment shaders.

   Why: profiled, a 1498 px view is about four seconds of JavaScript, and
   it is four seconds of exactly the arithmetic a GPU exists to do --
   nine octaves of value noise, a crater field, a hillshade and a
   sixty-four step horizon march, all per pixel and none of it dependent
   on its neighbours except through one height field. The game already
   knows this: src/TerrainGen/terrain_gpu.cpp runs the shipped chain as
   GLSL 330 and ES 100 passes and picks the GPU when it times faster.
   This is the same move for the bench.

   Two passes, not the game's fused one. The game can afford to
   re-evaluate its height inside the shadow march because its height is
   a few octaves of noise; this chain's is a crater population, and
   evaluating that sixty-four times per pixel would cost more than the
   CPU does. So the height field is written to a float texture first and
   the march reads it back -- one evaluation per pixel instead of
   sixty-five.

   The CPU keeps the rung ladder. Those are cached per place and per
   level and cost seconds only when the level changes; the per-view work
   is what moves here.

   Matches the CPU path where it matters and not bit for bit: the hashes
   are the same (DetailHash01 is integer arithmetic that GLSL's uint
   reproduces exactly), the octaves and the crater lattice are the same,
   but the gaussian is a sampled kernel rather than the CPU's stacked
   boxes. Compare with the GPU chip in the playtest before changing
   anything here.
   ===================================================================== */

const TerrainGPU = (function(){

const VS = `#version 300 es
in vec2 aPos;
void main(){ gl_Position = vec4(aPos, 0.0, 1.0); }`;

// Shared by every fragment stage. Pixel indices are image-space: row 0
// is the top row of the tile, the same row 0 the CPU writes.
const COMMON = `#version 300 es
precision highp float;
precision highp int;
out vec4 outC;
uniform float uRes;
uniform vec4 uFrame;          // lat0Deg, dLatPerPx, lon0Deg, dLonPerPx
// Framebuffer row 0 is the BOTTOM. Every pass works in image space --
// row 0 is the top, the row the CPU writes first -- and the textures are
// uploaded row-0-first, so the two agree everywhere except at the very
// end, where the pass that draws to the canvas has to turn over.
uniform float uFlipY;
const float MOON_KM_PER_DEG = 30.32268;
const float DEG2RAD = 0.017453292519943295;
const float FLOOR_KM = 1.3325;
vec2 ipix(){
  float y = floor(gl_FragCoord.y);
  return vec2(floor(gl_FragCoord.x), uFlipY > 0.5 ? (uRes - 1.0 - y) : y);
}
vec2 uvOf(vec2 p){ return (p + 0.5) / uRes; }

// lola_dem.cpp's DetailHash / DetailNoise. Math.imul is a 32-bit
// multiply keeping the low word, which is what uint does here, so the
// lattice quantises identically on both paths.
float dhash(int x, int y, uint salt){
  uint h = uint(x) * 0x8da6b343u ^ uint(y) * 0xd8163841u ^ salt * 0xcb1ab31fu;
  h = h ^ (h >> 13u); h = h * 0x9e3779b1u; h = h ^ (h >> 16u);
  return float(h & 0xFFFFFFu) / 16777215.0;
}
float dnoise(float u, float v, float waveKm, uint salt){
  float gu = u / waveKm, gv = v / waveKm;
  float fx0 = floor(gu), fy0 = floor(gv);
  int x0 = int(fx0), y0 = int(fy0);
  float fx = gu - fx0, fy = gv - fy0;
  fx = fx * fx * (3.0 - 2.0 * fx); fy = fy * fy * (3.0 - 2.0 * fy);
  float n00 = dhash(x0, y0, salt),     n10 = dhash(x0 + 1, y0, salt);
  float n01 = dhash(x0, y0 + 1, salt), n11 = dhash(x0 + 1, y0 + 1, salt);
  return mix(mix(n00, n10, fx), mix(n01, n11, fx), fy) * 2.0 - 1.0;
}
float subfade(float lambdaKm){
  return clamp(log2(FLOOR_KM * 2.0 / lambdaKm) / 1.5, 0.0, 1.0);
}
// The world position of a pixel. The cosine belongs to the pixel's own
// row, exactly as WorldGrid does it.
vec2 worldKm(vec2 p){
  float lat = uFrame.x + (p.y + 0.5) * uFrame.y;
  float lon = uFrame.z + (p.x + 0.5) * uFrame.w;
  return vec2(lon * MOON_KM_PER_DEG * cos(lat * DEG2RAD), lat * MOON_KM_PER_DEG);
}
`;

// Separable gaussian. The CPU stacks box passes for wide sigmas; this
// samples the kernel, which is the same field to well under a grey level
// and one pass either way.
const FS_BLUR = COMMON + `
uniform sampler2D uSrc;
uniform vec2 uDir;
uniform float uSigma;
void main(){
  vec2 p = ipix();
  if (uSigma <= 0.05){ outC = vec4(texture(uSrc, uvOf(p)).r); return; }
  float inv = 1.0 / (2.0 * uSigma * uSigma);
  int rad = int(min(48.0, ceil(uSigma * 3.0)));
  float s = 0.0, w = 0.0;
  for (int i = -48; i <= 48; i++){
    if (i < -rad) continue;
    if (i > rad) break;
    float k = exp(-float(i * i) * inv);
    vec2 q = clamp(p + uDir * float(i), vec2(0.0), vec2(uRes - 1.0));
    s += k * texture(uSrc, uvOf(q)).r; w += k;
  }
  outC = vec4(s / w);
}`;

// The height field, in chain units: the blurred macro as a relief proxy,
// plus everything the mosaic cannot carry.
const FS_HEIGHT = COMMON + `
uniform sampler2D uMacro;     // the cropped rung
uniform sampler2D uBase;      // the same, blurred, as the relief proxy
uniform float uKmPerPx;
uniform float uHeightScaleM;
uniform float uFormRelief;
uniform vec4 uSub;            // subRough, subGrit, subCraters, popDensity
uniform vec4 uPop;            // dMin, dMax, rim, rimWidth
uniform vec4 uPop2;           // ejecta, floorFlat, cosineBowl, popDepth
uniform vec2 uClast;          // clasts, clastDensity
uniform vec4 uNamedA[8];      // px, py, R, depthM
uniform vec4 uNamedB[8];      // rimM, 0, 0, 0
uniform int  uNamedN;

float bowlProf(float r, float flatR, float cosine){
  if (r >= 1.0) return 0.0;
  if (r <= flatR) return -1.0;
  float u = (r - flatR) / max(1e-4, 1.0 - flatR);
  return cosine > 0.5 ? -0.5 * (1.0 + cos(3.14159265358979 * u)) : -(1.0 - u * u);
}
float rimRise(float t, float invW, float ejectaKm){
  float g = exp(-t * t * invW);
  if (t <= 0.0 || ejectaKm <= 0.001) return g;
  float x = min(1.0, t / ejectaKm);
  float sf = 1.0 - x * x * (3.0 - 2.0 * x);
  return 0.65 * g + 0.35 * sf;
}
// Where a world point lands in this tile's pixels. The inverse of
// worldKm, which the splatting loops need because they work from a
// lattice cell back to the pixel.
vec2 pixOfWorld(vec2 w){
  float lat = w.y / MOON_KM_PER_DEG;
  float c = cos(lat * DEG2RAD);
  float lon = w.x / (MOON_KM_PER_DEG * max(1e-6, abs(c)) * sign(c));
  return vec2((lon - uFrame.z) / uFrame.w - 0.5, (lat - uFrame.x) / uFrame.y - 0.5);
}

void main(){
  vec2 p = ipix();
  vec2 w = worldKm(p);
  float macro = texture(uMacro, uvOf(p)).r;
  float density = clamp((macro - 0.22) / 0.45, 0.15, 1.0);
  float rough = 0.45 + 0.55 * density;

  float h = (texture(uBase, uvOf(p)).r - 0.5) * 0.13 * uFormRelief;
  float accM = 0.0;

  // The fractal residual, world wavelengths from the mosaic's floor down
  // to about three pixels.
  float lambda = FLOOR_KM * 2.0;
  for (int o = 0; o < 16; o++){
    if (lambda < 3.0 * uKmPerPx) break;
    float fw = subfade(lambda);
    if (fw > 0.001){
      uint salt = 0x51u + uint(o) * 2654435761u;
      accM += fw * uSub.x * lambda * 1000.0 * rough * dnoise(w.x, w.y, lambda, salt);
    }
    lambda *= 0.5;
  }

  // Clasts: little domes on a world lattice. A clast reaches at most a
  // third of a cell, so the three by three neighbourhood is the whole
  // search -- that inversion is what makes the splat loops into a
  // per-pixel gather.
  if (uClast.x > 0.001){
    float diamKm = 0.045;
    for (int b = 0; b < 12; b++){
      if (diamKm < 2.2 * uKmPerPx) break;
      float cellKm = diamKm / 0.42;
      uint salt = uint(0x5EED17 + b * 26417);
      float occ = min(0.9, uClast.y * (0.35 + 0.65 * float(b) / 4.0));
      int ci0 = int(floor(w.x / cellKm)), cj0 = int(floor(w.y / cellKm));
      for (int dj = -1; dj <= 1; dj++){
        for (int di = -1; di <= 1; di++){
          int ci = ci0 + di, cj = cj0 + dj;
          if (dhash(ci, cj, salt) > occ) continue;
          float cu = (float(ci) + 0.15 + 0.70 * dhash(ci, cj, salt + 1u)) * cellKm;
          float cv = (float(cj) + 0.15 + 0.70 * dhash(ci, cj, salt + 2u)) * cellKm;
          vec2 cp = pixOfWorld(vec2(cu, cv));
          float dKm = diamKm * (0.55 + 0.75 * dhash(ci, cj, salt + 3u));
          float R = (dKm * 0.5) / uKmPerPx;
          if (R < 0.7) continue;
          vec2 d = (p - cp) / R;
          float q = 1.0 - dot(d, d);
          if (q > 0.0)
            accM += uClast.x * dKm * 1000.0 * 0.33
                  * (0.6 + 0.8 * dhash(ci, cj, salt + 4u)) * sqrt(q);
        }
      }
      diamKm *= 0.5;
    }
  }

  // The impact population. Deepest bowl wins WITHIN a band, rims
  // accumulate across all of them -- the same rule the CPU uses, which
  // is why a saturated field does not dig runaway pits.
  if (uSub.z > 0.001){
    float rOuter = 1.0 + max(3.0 * uPop.w, uPop2.x);
    float invW = 1.0 / (2.0 * uPop.w * uPop.w);
    float diamKm = FLOOR_KM * 1.4;
    for (int b = 0; b < 16; b++){
      if (diamKm < 2.5 * uKmPerPx) break;
      float fw = subfade(diamKm);
      if (fw > 0.001){
        float cellKm = diamKm / 0.55;
        uint salt = uint(0xC7A7E5 + b * 7919);
        int ci0 = int(floor(w.x / cellKm)), cj0 = int(floor(w.y / cellKm));
        float bowl = 0.0;
        for (int dj = -1; dj <= 1; dj++){
          for (int di = -1; di <= 1; di++){
            int ci = ci0 + di, cj = cj0 + dj;
            float cluster = 0.5 + 0.5 * dnoise((float(ci) + 0.5) * cellKm,
                                               (float(cj) + 0.5) * cellKm,
                                               cellKm * 11.0, salt + 900u);
            if (dhash(ci, cj, salt) > uSub.w * cluster) continue;
            float cu = (float(ci) + 0.12 + 0.76 * dhash(ci, cj, salt + 1u)) * cellKm;
            float cv = (float(cj) + 0.12 + 0.76 * dhash(ci, cj, salt + 2u)) * cellKm;
            vec2 cp = pixOfWorld(vec2(cu, cv));
            float dKm = cellKm * (0.30 + 0.70 * dhash(ci, cj, salt + 3u));
            float R = (dKm * 0.5) / uKmPerPx;
            if (R < 0.9) continue;
            float r = length(p - cp) / R;
            if (r >= rOuter) continue;
            float age = dhash(ci, cj, salt + 4u);
            float fresh = age * age * age;
            float depthM = fw * uPop2.w * dKm * 1000.0
                         * (uPop.x + (uPop.y - uPop.x) * fresh);
            float rimM = depthM * uPop.z * fresh;
            if (r < 1.0)
              bowl = min(bowl, bowlProf(r, uPop2.y, uPop2.z) * depthM);
            if (rimM > 0.001) accM += rimM * rimRise(r - 1.0, invW, uPop2.x);
          }
        }
        accM += bowl;
      }
      diamKm *= 0.5;
    }
  }

  // The pixel's own grit: the one term that cannot be world-anchored,
  // because the pixel is where the world ends.
  float cellKm = max(1e-12, uKmPerPx);
  float grit = dhash(int(floor(w.x / cellKm)), int(floor(w.y / cellKm)), 0x6A09E667u) - 0.5;
  accM += uSub.y * 1.4 * uKmPerPx * 1000.0 * grit * rough;

  // The named craters, carved in metres like everything else.
  float rOuterN = 1.0 + max(3.0 * uPop.w, uPop2.x);
  float invWN = 1.0 / (2.0 * uPop.w * uPop.w);
  float namedBowl = 0.0;
  for (int i = 0; i < 8; i++){
    if (i >= uNamedN) break;
    vec4 a = uNamedA[i];
    float r = length(p - a.xy) / a.z;
    if (r >= rOuterN) continue;
    if (r < 1.0) namedBowl = min(namedBowl, bowlProf(r, uPop2.y, uPop2.z) * a.w);
    if (uNamedB[i].x > 0.001) accM += uNamedB[i].x * rimRise(r - 1.0, invWN, uPop2.x);
  }
  accM += namedBowl;

  outC = vec4(h + accM / uHeightScaleM);
}`;

// Light and tone: the hillshade, the horizon march, the mottle, the ramp.
const FS_SHADE = COMMON + `
uniform sampler2D uMacro;
uniform sampler2D uHeight;
uniform float uKmPerPx;
uniform vec4 uLight;          // relWeight, lightWeight, sCurve, subMottle
uniform vec2 uSun;            // azDeg, altDeg
uniform float uShadowSteps;
uniform float uBands;
uniform vec4 uNamedA[8];
uniform vec2 uNamedC[8];      // albK, R
uniform int  uNamedN;
uniform vec4 uPop;
uniform vec4 uPop2;

float hAt(vec2 p){
  return texture(uHeight, uvOf(clamp(p, vec2(0.0), vec2(uRes - 1.0)))).r;
}
void main(){
  vec2 p = ipix();
  vec2 w = worldKm(p);
  float macro = texture(uMacro, uvOf(p)).r;
  float density = clamp((macro - 0.22) / 0.45, 0.15, 1.0);
  float rough = 0.45 + 0.55 * density;
  const float z = 110.0;

  float az = radians(360.0 - uSun.x + 90.0);
  float alt = radians(uSun.y);
  float sinAlt = sin(alt), cosAlt = cos(alt);

  // Hillshade from central differences, exactly the CPU's: the divisor
  // is the pixel distance actually spanned, which is 1 at a clamped edge.
  float ym = max(0.0, p.y - 1.0), yp = min(uRes - 1.0, p.y + 1.0);
  float xm = max(0.0, p.x - 1.0), xp = min(uRes - 1.0, p.x + 1.0);
  float dy = (hAt(vec2(p.x, yp)) - hAt(vec2(p.x, ym))) * z / (yp - ym);
  float dx = (hAt(vec2(xp, p.y)) - hAt(vec2(xm, p.y))) * z / (xp - xm);
  float slope = atan(length(vec2(dx, dy)));
  float aspect = atan(dy, -dx);
  float hs = clamp(cos(slope) * sinAlt + sin(slope) * cosAlt * cos(az - aspect), 0.0, 1.0);
  float rel = clamp(hs / sinAlt, 0.0, 1.6);

  // Horizon march toward the sun.
  float tanAlt = tan(alt);
  vec2 sdir = vec2(cos(az), -sin(az));
  float hz = hAt(p) * z;
  float maxBlock = -1e9;
  for (int s = 1; s <= 128; s++){
    if (float(s) > uShadowSteps) break;
    float dist = float(s) * 1.5;
    maxBlock = max(maxBlock, (hAt(floor(p + sdir * dist)) * z - hz) / dist);
  }
  float light = 1.0 - clamp((maxBlock - tanAlt) / (tanAlt * 0.35), 0.0, 1.0);

  // The regolith's own tone, four octaves of it, world-anchored.
  float mott = 0.0, amp = 1.0, norm = 0.0, lambda = FLOOR_KM * 0.7;
  for (int o = 0; o < 4; o++){
    if (lambda < 4.0 * uKmPerPx) break;
    float fw = subfade(lambda * 2.0) * amp;
    norm += amp;
    if (fw > 0.001) mott += fw * dnoise(w.x, w.y, lambda, uint(0x2545F491 + o * 40503));
    amp *= 0.62; lambda *= 0.5;
  }
  if (norm > 0.0) mott *= uLight.w / norm;

  // Named-crater albedo, kept out of the height so formRelief cannot
  // read a bright floor back as high ground.
  float alb = 1.0;
  for (int i = 0; i < 8; i++){
    if (i >= uNamedN) break;
    float r = length(p - uNamedA[i].xy) / uNamedA[i].z;
    if (r < 1.0) alb *= 1.0 + uNamedC[i].x * (1.0 - r * r);
  }

  float lum = macro * ((1.0 - uLight.x) + uLight.x * rel)
                    * ((1.0 - uLight.y) + uLight.y * light);
  lum *= alb;
  lum *= 1.0 + mott * rough;
  lum = clamp(lum, 0.0, 1.0);
  float sc = lum * lum * (3.0 - 2.0 * lum);
  lum = clamp(sc * uLight.z + lum * (1.0 - uLight.z), 0.0, 1.0);
  if (uBands >= 2.0) lum = floor(lum * uBands + 0.5) / uBands;

  // The lunar tone ramp.
  vec3 lo = vec3(16.0, 17.0, 24.0) / 255.0;
  vec3 mid = vec3(108.0, 105.0, 102.0) / 255.0;
  vec3 hi = vec3(236.0, 232.0, 220.0) / 255.0;
  vec3 col = lum < 0.5 ? mix(lo, mid, lum / 0.5) : mix(mid, hi, (lum - 0.5) / 0.5);
  outC = vec4(col, 1.0);
}`;

function Compile(gl, type, src){
  const s = gl.createShader(type);
  gl.shaderSource(s, src); gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS))
    throw new Error(gl.getShaderInfoLog(s) + "\n" + src.split("\n").slice(0, 4).join("\n"));
  return s;
}
function Program(gl, fs){
  const p = gl.createProgram();
  gl.attachShader(p, Compile(gl, gl.VERTEX_SHADER, VS));
  gl.attachShader(p, Compile(gl, gl.FRAGMENT_SHADER, fs));
  gl.bindAttribLocation(p, 0, "aPos");
  gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
  return p;
}

function Create(){
  let canvas, gl;
  try {
    canvas = document.createElement("canvas");
    gl = canvas.getContext("webgl2", { antialias: false, depth: false,
                                       preserveDrawingBuffer: true, alpha: false });
  } catch (e){ return null; }
  // Float render targets are the whole design: the height field has to
  // survive from one pass to the next with more than eight bits.
  if (!gl || !gl.getExtension("EXT_color_buffer_float")) return null;

  let progs;
  try {
    progs = { blur: Program(gl, FS_BLUR), height: Program(gl, FS_HEIGHT),
              shade: Program(gl, FS_SHADE) };
  } catch (e){ return { error: String(e) }; }

  const quad = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, quad);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1,-1, 3,-1, -1,3]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

  const fbo = gl.createFramebuffer();
  let tex = {}, texRes = 0;
  function Ensure(res){
    if (texRes === res) return;
    for (const k in tex) gl.deleteTexture(tex[k]);
    tex = {};
    for (const k of ["macro", "a", "b", "height"]){
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      gl.texStorage2D(gl.TEXTURE_2D, 1, gl.R32F, res, res);
      for (const pn of [gl.TEXTURE_MIN_FILTER, gl.TEXTURE_MAG_FILTER])
        gl.texParameteri(gl.TEXTURE_2D, pn, gl.NEAREST);
      for (const pn of [gl.TEXTURE_WRAP_S, gl.TEXTURE_WRAP_T])
        gl.texParameteri(gl.TEXTURE_2D, pn, gl.CLAMP_TO_EDGE);
      tex[k] = t;
    }
    texRes = res;
  }
  const U = (p, n) => gl.getUniformLocation(p, n);
  function Common(p, o, flip){
    gl.uniform1f(U(p, "uRes"), o.res);
    gl.uniform1f(U(p, "uFlipY"), flip ? 1 : 0);
    gl.uniform4f(U(p, "uFrame"), o.frame.lat0Deg, o.frame.dLatPerPx,
                 o.frame.lon0Deg, o.frame.dLonPerPx);
  }

  // The named craters land in this tile's pixels on the CPU: eight of
  // them, and the arithmetic is the frame's, not the shader's.
  function NamedUniforms(o){
    const A = [], B = [], C = [];
    const P = o.craterParams, K = 30.32268, D2R = Math.PI / 180.0;
    let n = 0;
    for (const c of (o.craters || [])){
      if (c.off || n >= 8) continue;
      const lat = o.originLat - c.sKm / K;
      const lon = o.originLon + c.eKm / (K * Math.cos(lat * D2R));
      const px = (lon - o.frame.lon0Deg) / o.frame.dLonPerPx - 0.5;
      const py = (lat - o.frame.lat0Deg) / o.frame.dLatPerPx - 0.5;
      const R = ((c.dKm * 0.5) / o.frame.kmPerPx) * P.sizeScale;
      if (R < 0.35) continue;
      const depthM = c.dKm * 1000.0 * (P.dMin + (P.dMax - P.dMin) * c.fresh)
                   * P.depth * P.sizeScale;
      A.push(px, py, R, depthM);
      B.push(depthM * P.rim * c.fresh, 0, 0, 0);
      C.push(P.albedo * c.fresh, R);
      n++;
    }
    while (A.length < 32){ A.push(0,0,1,0); B.push(0,0,0,0); C.push(0,1); }
    return { A: new Float32Array(A), B: new Float32Array(B),
             C: new Float32Array(C), n };
  }

  function Draw(target){
    if (target){
      gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, target, 0);
    } else {
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    }
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  function Bind(p, name, t, unit){
    gl.activeTexture(gl.TEXTURE0 + unit);
    gl.bindTexture(gl.TEXTURE_2D, t);
    gl.uniform1i(U(p, name), unit);
  }

  // o: { res, macro (Float32Array res*res), frame, spanKm, tune,
  //      craters, craterParams, originLat, originLon }
  function Render(o){
    const t0 = performance.now();
    const res = o.res, k = res / 300.0, T = o.tune;
    Ensure(res);
    canvas.width = canvas.height = res;
    gl.viewport(0, 0, res, res);

    gl.bindTexture(gl.TEXTURE_2D, tex.macro);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, res, res, gl.RED, gl.FLOAT, o.macro);

    // The relief proxy: the macro, smoothed, so the imagery's own noise
    // does not become terrain.
    const sigma = (T.crisp ? 1.2 : 2.5) * k;
    gl.useProgram(progs.blur); Common(progs.blur, o);
    gl.uniform1f(U(progs.blur, "uSigma"), sigma);
    gl.uniform2f(U(progs.blur, "uDir"), 1, 0);
    Bind(progs.blur, "uSrc", tex.macro, 0); Draw(tex.a);
    gl.uniform2f(U(progs.blur, "uDir"), 0, 1);
    Bind(progs.blur, "uSrc", tex.a, 0); Draw(tex.b);

    const nm = NamedUniforms(o);
    const P = o.craterParams;

    gl.useProgram(progs.height); Common(progs.height, o);
    gl.uniform1f(U(progs.height, "uKmPerPx"), o.frame.kmPerPx);
    gl.uniform1f(U(progs.height, "uHeightScaleM"), 110.0 * (o.spanKm * 1000.0 / res));
    gl.uniform1f(U(progs.height, "uFormRelief"), T.formRelief);
    gl.uniform4f(U(progs.height, "uSub"), T.subRough, T.subGrit, T.subCraters, T.popDensity);
    gl.uniform4f(U(progs.height, "uPop"), T.dMin, T.dMax, T.rim, T.rimWidth);
    gl.uniform4f(U(progs.height, "uPop2"), T.ejecta, T.floorFlat, T.cosineBowl, T.subCraters);
    gl.uniform2f(U(progs.height, "uClast"), T.clasts, T.clastDensity);
    gl.uniform4fv(U(progs.height, "uNamedA"), nm.A);
    gl.uniform4fv(U(progs.height, "uNamedB"), nm.B);
    gl.uniform1i(U(progs.height, "uNamedN"), nm.n);
    Bind(progs.height, "uMacro", tex.macro, 0);
    Bind(progs.height, "uBase", tex.b, 1);
    Draw(tex.height);

    gl.useProgram(progs.shade); Common(progs.shade, o, true);
    gl.uniform1f(U(progs.shade, "uKmPerPx"), o.frame.kmPerPx);
    gl.uniform4f(U(progs.shade, "uLight"), T.relWeight, T.lightWeight, T.sCurve, T.subMottle);
    gl.uniform2f(U(progs.shade, "uSun"), T.sunAz, T.sunAlt);
    gl.uniform1f(U(progs.shade, "uShadowSteps"), Math.min(128, Math.floor(22.0 * k / 1.5)));
    gl.uniform1f(U(progs.shade, "uBands"), T.bands || 0);
    gl.uniform4fv(U(progs.shade, "uNamedA"), nm.A);
    gl.uniform2fv(U(progs.shade, "uNamedC"), nm.C);
    gl.uniform1i(U(progs.shade, "uNamedN"), nm.n);
    gl.uniform4f(U(progs.shade, "uPop"), T.dMin, T.dMax, T.rim, T.rimWidth);
    gl.uniform4f(U(progs.shade, "uPop2"), T.ejecta, T.floorFlat, T.cosineBowl, T.subCraters);
    Bind(progs.shade, "uMacro", tex.macro, 0);
    Bind(progs.shade, "uHeight", tex.height, 1);
    Draw(null);

    // finish() alone does not stop the clock here: ANGLE will happily
    // return from it with the work still queued, and the first honest
    // measurement of this came back at five milliseconds on a software
    // rasteriser, which is not a thing. A one pixel read forces the
    // pipeline to drain.
    const sync = new Uint8Array(4);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sync);
    gl.finish();
    return { canvas, ms: performance.now() - t0 };
  }

  return { Render, gl, canvas,
           renderer: (() => { const d = gl.getExtension("WEBGL_debug_renderer_info");
             return d ? gl.getParameter(d.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER); })() };
}

return { Create };
})();
