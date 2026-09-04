// GPU terrain synthesis. See terrain_gpu.h for the contract and
// terrain_synthesis.cpp for the chain this reproduces.
//
// Why a second implementation: the CPU chain costs ~0.5 s per cell even
// after the pyramid blur, and the browser build has no worker threads
// to hide that behind, so every cell crossing there froze the page. A
// GPU does the same work in milliseconds, and every target has one --
// phone, browser, desktop -- except a software GL stack, which
// GetTerrainPath() detects and leaves on the CPU.
//
// Fidelity: the same stages in the same order (crop, unsharp, adaptive
// contrast, relief blur, grain, undulation, boulders, site disturbance,
// hillshade, cast shadows, speckle, S-curve, tone ramp), driven by the
// same location seed, so a cell always looks the same. What differs is
// the noise itself: the CPU draws lattices from a xorshift stream, the
// shader hashes lattice coordinates. Each octave is scaled to the CPU's
// measured per-octave deviation (0.135 for its blurred bilinear lattice
// against 0.214 for smoothstep interpolation), so the ground carries
// the same texture statistics without being the same pixels.
//
// Storage: no float textures anywhere, because WebGL1 has none. The
// chain is FUSED -- the height field is never written out. The shading
// pass evaluates height procedurally at the pixel, at its four
// neighbours for the hillshade gradient, and along the sun line for the
// shadow march. Only 8-bit luminance lives between passes, plus the WAC
// crop and one 1x1 texture of site means, both packed 16-bit into two
// 8-bit channels.
//
// Orientation: raylib render targets come out upside down relative to
// images (GL's origin is bottom-left). Every pass here draws a full
// quad and samples its inputs through rtuv(), which flips v, so the
// content stays screen-aligned from pass to pass; the final ramp pass
// samples unflipped, which lands the colour texture in image
// orientation -- exactly what DrawTexturePro expects, so the render
// manager draws CPU and GPU chains the same way.

#include "terrain_gpu.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

// Two dialects, one body. The prefix supplies version, precision and
// the in/out spelling; the body is written against TEX() and OUT.
const char* PREFIX_330 =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 finalColor;\n"
    "#define TEX(s, uv) texture(s, uv)\n"
    "#define OUT finalColor\n";

const char* PREFIX_100 =
    "#version 100\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "#else\n"
    "precision mediump float;\n"
    "precision mediump int;\n"
    "#endif\n"
    "varying vec2 fragTexCoord;\n"
    "#define TEX(s, uv) texture2D(s, uv)\n"
    "#define OUT gl_FragColor\n";

// Shared by every pass: the target size and the flipped sampler.
const char* COMMON = R"GLSL(
uniform float uRes;
vec2 rtuv(vec2 pix) { return vec2(pix.x / uRes, 1.0 - pix.y / uRes); }
)GLSL";

// Hash noise standing in for the CPU's seeded lattices.
const char* NOISE = R"GLSL(
float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
// One octave of value noise on a lattice of s pixels. 0.63 brings a
// smoothstep lattice's deviation (0.214) down to the CPU's blurred
// bilinear one (0.135).
float vnoise(vec2 p, float s, vec2 seed)
{
    vec2 q = p / s;
    vec2 i = floor(q);
    vec2 f = q - i;
    vec2 u = f * f * (3.0 - 2.0 * f);
    // The lattice is global now, so i runs to hundreds of thousands and
    // hash21 loses its spread long before that. Wrap it. 4096 cells is
    // 356 km at the finest lattice and 78,000 km at the coarsest --
    // wider than any window at either end, so nothing repeats on screen.
    vec2 w = mod(i + seed, 4096.0);
    float a = hash21(w);
    float b = hash21(mod(i + vec2(1.0, 0.0) + seed, 4096.0));
    float c = hash21(mod(i + vec2(0.0, 1.0) + seed, 4096.0));
    float d = hash21(mod(i + vec2(1.0, 1.0) + seed, 4096.0));
    float v = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    return 0.5 + (v - 0.5) * 0.63;
}
float fbm(vec2 p, float base, float pers, int oct, vec2 seed)
{
    float amp = 1.0;
    float norm = 0.0;
    float s = base;
    float acc = 0.0;
    for (int o = 0; o < 5; o++)
    {
        if (o >= oct) break;
        acc += amp * vnoise(p, s, seed + vec2(float(o) * 17.0, float(o) * 31.0));
        norm += amp;
        amp *= pers;
        s = max(2.0, floor(s * 0.5));
    }
    return acc / norm;
}
// Zero-mean, unit-deviation regolith grain: the CPU's GrainNoise, with
// its two per-field normalisations replaced by the measured constants
// (fbm 0.06, white 0.2887, mix 0.93).
// The CPU's per-pixel component is white noise blurred at sigma 0.5,
// and its hillshade blurs the height by another 0.6 before taking
// gradients -- about 0.8 px of smoothing in all. The hillshade reads
// the grain's GRADIENT, so unblurred white noise would shade nearly
// twice as hard for the same deviation. A 2 px value-noise lattice has
// that smoothed spectrum at four hashes; 0.135 is one octave's
// deviation, so the result has unit deviation like the CPU's.
float fineNoise(vec2 p, vec2 seed)
{
    return (vnoise(p, 2.0, seed * 0.37 + vec2(3.1, 7.7)) - 0.5) / 0.135;
}
float grain(vec2 p, vec2 seed)
{
    float g = (fbm(p, 64.0, 0.8, 5, seed) - 0.5) / 0.06;
    return (0.55 * g + 0.75 * fineNoise(p, seed)) / 0.93;
}
)GLSL";

// Level 0 macro: the packed 16-bit WAC crop, upsampled bilinearly on
// pixel centres exactly as ResizeBilinear does.
const char* FS_MACRO = R"GLSL(
uniform sampler2D uCrop;
uniform vec2 uCropSize;
// The window inside the block: top-left texel and texel span. Sampling
// by position rather than stretching the block is what keeps the
// imagery a pure function of ground -- see TerrainMacroCrop.
uniform vec4 uCropWin;
float texel(vec2 t)
{
    vec4 c = TEX(uCrop, (t + 0.5) / uCropSize);
    return (floor(c.r * 255.0 + 0.5) * 256.0 + floor(c.g * 255.0 + 0.5)) / 65535.0;
}
void main()
{
    vec2 pix = fragTexCoord * uRes;
    vec2 f = uCropWin.xy + pix * uCropWin.zw / uRes;
    vec2 i0 = clamp(floor(f), vec2(0.0), uCropSize - 1.0);
    vec2 i1 = min(i0 + 1.0, uCropSize - 1.0);
    vec2 t = clamp(f - i0, 0.0, 1.0);
    float top = mix(texel(vec2(i0.x, i0.y)), texel(vec2(i1.x, i0.y)), t.x);
    float bot = mix(texel(vec2(i0.x, i1.y)), texel(vec2(i1.x, i1.y)), t.x);
    float v = mix(top, bot, t.y);
    OUT = vec4(v, v, v, 1.0);
}
)GLSL";

// Levels 1 and 2: the centre crop of the level above, bilinear.
const char* FS_CROP = R"GLSL(
uniform sampler2D uSrc;
uniform vec2 uLoFrac;
void main()
{
    vec2 pix = fragTexCoord * uRes;
    vec2 src = uLoFrac.x + pix * uLoFrac.y;
    float v = TEX(uSrc, rtuv(src)).r;
    OUT = vec4(v, v, v, 1.0);
}
)GLSL";

// Box-average f x f source texels into one: the pyramid's decimation.
const char* FS_DOWN = R"GLSL(
uniform sampler2D uSrc;
uniform float uSrcRes;
uniform float uF;
void main()
{
    vec2 sp = floor(fragTexCoord * uRes) * uF;
    float acc = 0.0;
    float n = 0.0;
    for (int j = 0; j < 8; j++)
    {
        if (float(j) >= uF) break;
        for (int i = 0; i < 8; i++)
        {
            if (float(i) >= uF) break;
            vec2 q = sp + vec2(float(i), float(j)) + 0.5;
            if (q.x < uSrcRes && q.y < uSrcRes)
            {
                acc += TEX(uSrc, vec2(q.x / uSrcRes, 1.0 - q.y / uSrcRes)).r;
                n += 1.0;
            }
        }
    }
    float v = (n > 0.0) ? acc / n : 0.0;
    OUT = vec4(v, v, v, 1.0);
}
)GLSL";

// One separable Gaussian pass, edges clamped like the CPU's.
const char* FS_BLUR = R"GLSL(
uniform sampler2D uSrc;
uniform vec2 uDir;
uniform float uKern[19];
uniform int uRadius;
void main()
{
    vec2 pix = fragTexCoord * uRes;
    float acc = 0.0;
    for (int i = -9; i <= 9; i++)
    {
        if (i < -uRadius || i > uRadius) continue;
        vec2 q = clamp(pix + uDir * float(i), vec2(0.5), vec2(uRes - 0.5));
        acc += TEX(uSrc, rtuv(q)).r * uKern[i + 9];
    }
    OUT = vec4(acc, acc, acc, 1.0);
}
)GLSL";

// Unsharp mask, plus the adaptive contrast gain on level 0.
const char* FS_SHARPEN = R"GLSL(
uniform sampler2D uSrc;
uniform sampler2D uBlur;
uniform vec3 uGainMid;
void main()
{
    vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    float m = TEX(uSrc, uv).r;
    float b = TEX(uBlur, uv).r;
    m = clamp(m + 0.40 * (m - b), 0.0, 1.0);
    if (uGainMid.z > 0.5)
        m = clamp(uGainMid.y + (m - uGainMid.y) * uGainMid.x, 0.0, 1.0);
    OUT = vec4(m, m, m, 1.0);
}
)GLSL";

// Everything the shading and the site-mean passes share: the site's
// footprint, the tone-levelled macro, the relief, and the fused height.
const char* HEIGHT = R"GLSL(
uniform sampler2D uMacro;
uniform sampler2D uRelief;
uniform float uAmp;
uniform float uK;
uniform vec4 uTune;        // grain, undulation, formRelief, -
uniform vec4 uSite;        // cx, cy, workedR, outerR (px); outerR <= 0: none
uniform vec4 uSiteAmp;     // toneLevel, levelAmount, undulationAmp, roughAmp
uniform vec4 uSpots[9];    // x, y, r (px), amp
uniform int uSpotCount;
uniform vec2 uBoulder;     // cell size (px, 0 = none), amp
uniform vec2 uSeedU;
uniform vec2 uSeedG;
uniform vec2 uSeedL;
uniform vec2 uSeedF;
uniform vec2 uSeedB;
// Where a pixel sits on the moon. Noise reads world(pix), never pix, so
// the lattice is pinned to the ground and the same site invents the same
// detail from any window that frames it.
//
// This is lola_dem's frame and terrain_synthesis.cpp's FrameWorldKm: u
// is the east-west arc from the prime meridian at the pixel's OWN
// latitude, v the north-south arc from the equator, both divided by the
// level's km/px so a lattice of s still means s pixels. The cos belongs
// to the pixel: taking it once per window makes the east-west scale
// depend on where the window is, which slid the lattice 7 px at the
// sect level. Bounded by half the moon's circumference over kmPerPx --
// about 560k at 5 km, well inside exact integer range for a float.
uniform vec4 uFrame;      // lat0, dLat/px, lon0, dLon/px  (degrees)
uniform float uKmPerPx;
const float MOON_KM_DEG = 30.32268;
vec2 world(vec2 pix)
{
    float lat = uFrame.x + (pix.y + 0.5) * uFrame.y;
    float lon = uFrame.z + (pix.x + 0.5) * uFrame.w;
    return vec2(lon * MOON_KM_DEG * cos(radians(lat)),
                lat * MOON_KM_DEG) / uKmPerPx;
}
// The exact inverse: v gives the latitude, and the latitude gives the
// cosine that u needs. Only the boulders need it, to put a cell's rock
// back on the pixel it belongs to.
vec2 worldToPix(vec2 w)
{
    float lat = w.y * uKmPerPx / MOON_KM_DEG;
    float c = cos(radians(lat));
    float lon = (w.x * uKmPerPx) / (MOON_KM_DEG * (abs(c) < 1e-6 ? 1e-6 : c));
    return vec2((lon - uFrame.z) / uFrame.w - 0.5,
                (lat - uFrame.x) / uFrame.y - 0.5);
}

float siteW(vec2 p)
{
    if (uSite.w <= 0.0) return 0.0;
    float d = distance(p, uSite.xy);
    if (d <= uSite.z) return 1.0;
    if (d >= uSite.w) return 0.0;
    float t = 1.0 - (d - uSite.z) / max(1e-3, uSite.w - uSite.z);
    return t * t * (3.0 - 2.0 * t);
}
float macroAt(vec2 pix) { return TEX(uMacro, rtuv(pix)).r; }
float toneAt(vec2 pix, float w, float tone)
{
    return mix(macroAt(pix), tone, uSiteAmp.x * w);
}
float roughAt(float m)
{
    return 0.45 + 0.55 * clamp((m - 0.22) / 0.45, 0.15, 1.0);
}
// Levelling the macro before the relief blur commutes with the blur to
// within the site's fade width, so it is applied to the blurred value.
float reliefAt(vec2 pix, float w, float tone)
{
    float r = mix(TEX(uRelief, rtuv(pix)).r, tone, uSiteAmp.x * w);
    return (r - 0.5) * 0.13 * uTune.z;
}
// One boulder per cell, on a hashed pixel; sometimes it also lifts the
// pixel to its right or below, like SprinkleBoulders.
float boulderAt(vec2 pix)
{
    if (uBoulder.x <= 0.0) return 0.0;
    vec2 ip = floor(pix);
    float h = 0.0;
    for (int n = 0; n < 3; n++)
    {
        vec2 off = (n == 1) ? vec2(1.0, 0.0) : ((n == 2) ? vec2(0.0, 1.0) : vec2(0.0));
        vec2 target = ip - off;
        // The cell a boulder belongs to is a patch of moon, so it is
        // chosen in world pixels; only the comparison comes back to
        // local ones. Both sides are integers under 2^24, so the trip
        // out and back is exact.
        vec2 wtarget = world(target);
        vec2 cell = floor(wtarget / uBoulder.x);
        vec2 sd = mod(cell, 4096.0) * 7.13 + uSeedB;
        vec2 bw = (cell + vec2(hash21(sd), hash21(sd + 11.7))) * uBoulder.x;
        vec2 bp = floor(worldToPix(bw) + 0.5);
        if (all(equal(bp, target)))
        {
            float a = uBoulder.y * (0.4 + hash21(sd + 23.1));
            if (n == 0) h += a;
            else if (n == 1) { if (hash21(sd + 5.5) < 0.5) h += 0.6 * a; }
            else { if (hash21(sd + 9.9) < 0.3) h += 0.5 * a; }
        }
    }
    return h;
}
// The height field at a pixel. grainAmp scales the regolith grain:
// 1 for the height itself, 0 for the far shadow march (grain is a
// 1-2 px texture, cannot cast a long shadow, and costs a 5-octave
// fbm), and 0.62 for the hillshade stencil -- the CPU blurs its height
// 0.6 px before differencing, which takes the grain's gradient from
// 0.80 to 0.46 while this lattice's sits at 0.74; the same ratio, the
// same shading.
float heightAt(vec2 pix, float tone, float hmean, float grainAmp)
{
    float w = siteW(pix);
    float m = toneAt(pix, w, tone);
    float rough = roughAt(m);
    float h = reliefAt(pix, w, tone);
    h += 0.02 * uAmp * uTune.y
         * (fbm(world(pix), 64.0 * uK, 0.5, 3, uSeedU) - 0.5) * rough;
    if (grainAmp > 0.0)
        h += 0.004 * uAmp * uTune.x * grain(world(pix), uSeedG)
             * rough * grainAmp;
    h += boulderAt(pix);
    if (w > 0.0)
    {
        h = mix(h, hmean, uSiteAmp.y * w);
        float lumps = fbm(world(pix), max(4.0, floor(uRes / 12.0)),
                          0.55, 3, uSeedL);
        float domeW = 0.0;
        float spotH = 0.0;
        for (int i = 0; i < 9; i++)
        {
            if (i >= uSpotCount) break;
            vec4 sp = uSpots[i];
            float d = distance(pix, sp.xy) / max(1.0, sp.z);
            if (d < 1.0)
            {
                float t = 1.0 - d;
                float ww = t * t * (3.0 - 2.0 * t);
                domeW = max(domeW, ww);
                spotH += sp.w * ww;
            }
        }
        h += uSiteAmp.z * (lumps - 0.5) * 2.0 * w;
        if (grainAmp > 0.0)
            h += uSiteAmp.w * grain(world(pix), uSeedF)
                 * (0.35 * w + 0.65 * domeW) * grainAmp;
        h += spotH;
    }
    return h;
}
)GLSL";

// The site's weighted means of tone and height over a 64x64 grid, into
// one packed 16-bit texel. Replaces the CPU's two reductions.
const char* FS_MEANS = R"GLSL(
void main()
{
    float step = uRes / 64.0;
    float tsum = 0.0;
    float wsum = 0.0;
    for (int j = 0; j < 64; j++)
    {
        for (int i = 0; i < 64; i++)
        {
            vec2 p = (vec2(float(i), float(j)) + 0.5) * step;
            float w = siteW(p);
            if (w <= 0.0) continue;
            tsum += macroAt(p) * w;
            wsum += w;
        }
    }
    float tone = (wsum > 0.0) ? tsum / wsum : 0.5;
    float hsum = 0.0;
    wsum = 0.0;
    for (int j = 0; j < 64; j++)
    {
        for (int i = 0; i < 64; i++)
        {
            vec2 p = (vec2(float(i), float(j)) + 0.5) * step;
            float w = siteW(p);
            if (w <= 0.0) continue;
            float m = toneAt(p, w, tone);
            float h = reliefAt(p, w, tone)
                    + 0.02 * uAmp * uTune.y
                      * (fbm(world(p), 64.0 * uK, 0.5, 3, uSeedU) - 0.5)
                        * roughAt(m);
            hsum += h * w;
            wsum += w;
        }
    }
    float hm = (wsum > 0.0) ? hsum / wsum : 0.0;
    float tq = floor(clamp(tone, 0.0, 1.0) * 65535.0 + 0.5);
    float hq = floor(clamp(hm * 0.5 + 0.5, 0.0, 1.0) * 65535.0 + 0.5);
    OUT = vec4(floor(tq / 256.0) / 255.0, mod(tq, 256.0) / 255.0,
               floor(hq / 256.0) / 255.0, mod(hq, 256.0) / 255.0);
}
)GLSL";

// The fused relight: TextureModulate's hillshade, cast shadows, speckle
// and S-curve in one pass, on a height field that is never stored.
const char* FS_FUSED = R"GLSL(
uniform sampler2D uMeans;
uniform float uShadowSteps;
uniform vec2 uSeedS;
vec2 unpackMeans()
{
    vec4 m = TEX(uMeans, vec2(0.5, 0.5));
    float tone = (floor(m.r * 255.0 + 0.5) * 256.0 + floor(m.g * 255.0 + 0.5)) / 65535.0;
    float hm = (floor(m.b * 255.0 + 0.5) * 256.0 + floor(m.a * 255.0 + 0.5)) / 65535.0;
    return vec2(tone, hm * 2.0 - 1.0);
}
void main()
{
    vec2 pix = fragTexCoord * uRes;
    vec2 mm = unpackMeans();
    float tone = mm.x;
    float hmean = mm.y;
    float w = siteW(pix);
    float m = toneAt(pix, w, tone);
    float rough = roughAt(m);

    // Lambertian hillshade from central differences, sun NW at 35 deg.
    const float z = 110.0;
    const float HS_GRAIN = 0.62;
    float hHere = heightAt(pix, tone, hmean, 1.0);
    float hL = heightAt(pix - vec2(1.0, 0.0), tone, hmean, HS_GRAIN);
    float hR = heightAt(pix + vec2(1.0, 0.0), tone, hmean, HS_GRAIN);
    float hU = heightAt(pix - vec2(0.0, 1.0), tone, hmean, HS_GRAIN);
    float hD = heightAt(pix + vec2(0.0, 1.0), tone, hmean, HS_GRAIN);
    float dy = (hD - hU) * z * 0.5;
    float dx = (hR - hL) * z * 0.5;
    float slope = atan(length(vec2(dx, dy)));
    float aspect = atan(dy, -dx);
    float az = radians(360.0 - 315.0 + 90.0);
    float alt = radians(35.0);
    float hs = clamp(cos(slope) * sin(alt)
                     + sin(slope) * cos(alt) * cos(az - aspect), 0.0, 1.0);
    float rel = clamp(hs / sin(alt), 0.0, 1.6);

    // Horizon march toward the sun: grain for the first steps (it makes
    // the pixel-scale micro-shadows), the smooth terms beyond.
    float tanAlt = tan(alt);
    vec2 sdir = vec2(cos(az), -sin(az));
    float hz = hHere * z;
    float maxBlock = -1e9;
    for (int s = 1; s <= 64; s++)
    {
        if (float(s) > uShadowSteps) break;
        float dist = float(s) * 1.5;
        vec2 q = clamp(pix + sdir * dist, vec2(0.0), vec2(uRes - 1.0));
        float hb = heightAt(q, tone, hmean, (s <= 3) ? 1.0 : 0.0) * z;
        maxBlock = max(maxBlock, (hb - hz) / dist);
    }
    float light = 1.0 - clamp((maxBlock - tanAlt) / (tanAlt * 0.35), 0.0, 1.0);

    float speckle = fbm(world(pix), 4.0, 0.5, 2, uSeedS);
    float lum = m * (0.62 + 0.38 * rel) * (0.45 + 0.55 * light);
    lum *= 1.0 + 0.04 * min(uAmp, 1.6) * (speckle - 0.5) * rough;
    lum = clamp(lum, 0.0, 1.0);
    float sc = lum * lum * (3.0 - 2.0 * lum);
    lum = clamp(sc * 0.20 + lum * 0.80, 0.0, 1.0);
    OUT = vec4(lum, lum, lum, 1.0);
}
)GLSL";

// Lunar tone ramp, sampled unflipped so the colour lands in image
// orientation (see the header comment).
const char* FS_RAMP = R"GLSL(
uniform sampler2D uSrc;
void main()
{
    float t = TEX(uSrc, fragTexCoord).r;
    vec3 lo = vec3(16.0, 17.0, 24.0) / 255.0;
    vec3 mid = vec3(108.0, 105.0, 102.0) / 255.0;
    vec3 hi = vec3(236.0, 232.0, 220.0) / 255.0;
    vec3 c = (t < 0.5) ? mix(lo, mid, t / 0.5) : mix(mid, hi, (t - 0.5) / 0.5);
    OUT = vec4(c, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// GL objects
// ---------------------------------------------------------------------------

struct SmallSet
{
    int size = 0;
    RenderTexture2D rt[3] = {};
};

struct Gpu
{
    bool init = false;
    bool ok = false;
    Shader macroSh = {}, cropSh = {}, downSh = {}, blurSh = {};
    Shader sharpenSh = {}, meansSh = {}, fusedSh = {}, rampSh = {};
    int res = 0;
    // Full-resolution scratch: macro, blur temp, sharpened macro, blur
    // destination, and the two luminance targets the levels ping-pong.
    RenderTexture2D A = {}, B = {}, C = {}, D = {}, L0 = {}, L1 = {};
    SmallSet small[3];
    RenderTexture2D means = {};
    Texture2D cropTex = {};
    Texture2D white = {};      // 1x1: a quad drawn through it gets 0..1 texcoords
    std::unordered_map<std::string, int> locs;
};

Gpu G;

bool UseEs100()
{
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    return true;
#else
    int v = rlGetVersion();
    return v == RL_OPENGL_ES_20 || v == RL_OPENGL_ES_30;
#endif
}

Shader Build(const char* const* parts, int count)
{
    std::string fs = UseEs100() ? PREFIX_100 : PREFIX_330;
    for (int i = 0; i < count; i++) fs += parts[i];
    Shader sh = LoadShaderFromMemory(nullptr, fs.c_str());
    return sh;
}

bool ShaderOk(Shader sh)
{
    // raylib hands back the default shader when compilation fails.
    return sh.id != 0 && sh.id != rlGetShaderIdDefault();
}

int Loc(Shader sh, const char* name)
{
    std::string key = std::to_string(sh.id) + ":" + name;
    auto it = G.locs.find(key);
    if (it != G.locs.end()) return it->second;
    int loc = rlGetLocationUniform(sh.id, name);
    if (loc < 0)
    {
        // Arrays are "name[0]" on GLSL ES 1.00.
        std::string alt = std::string(name) + "[0]";
        loc = rlGetLocationUniform(sh.id, alt.c_str());
    }
    G.locs[key] = loc;
    return loc;
}

void SetF(Shader sh, const char* n, float v)
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValue(sh, l, &v, SHADER_UNIFORM_FLOAT);
}
void SetI(Shader sh, const char* n, int v)
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValue(sh, l, &v, SHADER_UNIFORM_INT);
}
void SetV2(Shader sh, const char* n, float x, float y)
{
    float v[2] = {x, y};
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValue(sh, l, v, SHADER_UNIFORM_VEC2);
}
void SetV3(Shader sh, const char* n, float x, float y, float z)
{
    float v[3] = {x, y, z};
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValue(sh, l, v, SHADER_UNIFORM_VEC3);
}
void SetV4(Shader sh, const char* n, const float v[4])
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValue(sh, l, v, SHADER_UNIFORM_VEC4);
}
void SetV4Array(Shader sh, const char* n, const float* v, int count)
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValueV(sh, l, v, SHADER_UNIFORM_VEC4, count);
}
void SetFArray(Shader sh, const char* n, const float* v, int count)
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValueV(sh, l, v, SHADER_UNIFORM_FLOAT, count);
}
void SetTex(Shader sh, const char* n, Texture2D t)
{
    int l = Loc(sh, n);
    if (l >= 0) SetShaderValueTexture(sh, l, t);
}

RenderTexture2D MakeTarget(int w, int h, bool bilinear)
{
    RenderTexture2D rt = LoadRenderTexture(w, h);
    SetTextureFilter(rt.texture, bilinear ? TEXTURE_FILTER_BILINEAR
                                          : TEXTURE_FILTER_POINT);
    SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
    return rt;
}

void FreeTarget(RenderTexture2D& rt)
{
    if (rt.id != 0) UnloadRenderTexture(rt);
    rt = {};
}

bool InitGpu()
{
    if (G.init) return G.ok;
    G.init = true;

    const char* macro[] = {COMMON, FS_MACRO};
    const char* crop[] = {COMMON, FS_CROP};
    const char* down[] = {COMMON, FS_DOWN};
    const char* blur[] = {COMMON, FS_BLUR};
    const char* sharpen[] = {COMMON, FS_SHARPEN};
    const char* means[] = {COMMON, NOISE, HEIGHT, FS_MEANS};
    const char* fused[] = {COMMON, NOISE, HEIGHT, FS_FUSED};
    const char* ramp[] = {COMMON, FS_RAMP};
    G.macroSh = Build(macro, 2);
    G.cropSh = Build(crop, 2);
    G.downSh = Build(down, 2);
    G.blurSh = Build(blur, 2);
    G.sharpenSh = Build(sharpen, 2);
    G.meansSh = Build(means, 4);
    G.fusedSh = Build(fused, 4);
    G.rampSh = Build(ramp, 2);
    G.ok = ShaderOk(G.macroSh) && ShaderOk(G.cropSh) && ShaderOk(G.downSh)
        && ShaderOk(G.blurSh) && ShaderOk(G.sharpenSh) && ShaderOk(G.meansSh)
        && ShaderOk(G.fusedSh) && ShaderOk(G.rampSh);
    if (!G.ok)
    {
        TraceLog(LOG_WARNING, "TERRAIN: GPU shaders failed to build, CPU path");
        return false;
    }
    G.means = MakeTarget(1, 1, false);
    Image px = GenImageColor(1, 1, WHITE);
    G.white = LoadTextureFromImage(px);
    UnloadImage(px);
    return true;
}

bool EnsureScratch(int res)
{
    if (G.res == res) return true;
    FreeTarget(G.A); FreeTarget(G.B); FreeTarget(G.C);
    FreeTarget(G.D); FreeTarget(G.L0); FreeTarget(G.L1);
    for (auto& s : G.small) { for (auto& r : s.rt) FreeTarget(r); s.size = 0; }
    G.A = MakeTarget(res, res, true);
    G.B = MakeTarget(res, res, true);
    G.C = MakeTarget(res, res, true);
    G.D = MakeTarget(res, res, true);
    G.L0 = MakeTarget(res, res, true);
    G.L1 = MakeTarget(res, res, true);
    G.res = res;
    return G.A.id != 0 && G.L1.id != 0;
}

SmallSet& SmallFor(int size)
{
    for (auto& s : G.small) if (s.size == size) return s;
    for (auto& s : G.small)
    {
        if (s.size == 0)
        {
            for (auto& r : s.rt) r = MakeTarget(size, size, true);
            s.size = size;
            return s;
        }
    }
    SmallSet& s = G.small[0];
    for (auto& r : s.rt) FreeTarget(r);
    for (auto& r : s.rt) r = MakeTarget(size, size, true);
    s.size = size;
    return s;
}

// Draw one full-target quad through a shader. The quad goes through a
// 1x1 texture so its texcoords run 0..1 -- DrawRectangle would texture
// it with a few texels of the font atlas and fragTexCoord would barely
// move. Blending off: the means pass writes real data into alpha, and
// nothing here composites.
template <typename Bind>
void Pass(Shader sh, RenderTexture2D& dst, Bind bind)
{
    BeginTextureMode(dst);
    rlDisableColorBlend();
    BeginShaderMode(sh);
    SetF(sh, "uRes", (float)dst.texture.width);
    bind();
    DrawTexturePro(G.white, Rectangle{0, 0, 1, 1},
                   Rectangle{0, 0, (float)dst.texture.width, (float)dst.texture.height},
                   Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();
}

void Blur1D(RenderTexture2D& src, RenderTexture2D& dst, float sigma, bool horizontal)
{
    int radius = std::min(9, (int)std::ceil(sigma * 3.0f));
    float kern[19] = {0};
    float norm = 0.0f;
    for (int i = -radius; i <= radius; i++)
    {
        float v = std::exp(-0.5f * (i * i) / (sigma * sigma));
        kern[i + 9] = v;
        norm += v;
    }
    for (float& k : kern) k /= norm;
    Pass(G.blurSh, dst, [&]() {
        SetTex(G.blurSh, "uSrc", src.texture);
        SetV2(G.blurSh, "uDir", horizontal ? 1.0f : 0.0f, horizontal ? 0.0f : 1.0f);
        SetFArray(G.blurSh, "uKern", kern, 19);
        SetI(G.blurSh, "uRadius", radius);
    });
}

// Gaussian blur of a full-res target, the CPU's way: wide blurs run on
// a decimated copy (a wide blur is a low-pass filter, so nothing is
// lost), narrow ones exactly. Returns the target holding the result --
// a small one to be sampled bilinearly, or `dst` (which may be `src`).
RenderTexture2D& Blur(RenderTexture2D& src, RenderTexture2D& dst,
                      RenderTexture2D& tmp, float sigma)
{
    int res = src.texture.width;
    int f = 1;
    if (sigma >= 3.0f)
    {
        f = (int)(sigma / 2.0f);
        while (f > 1 && res / f < 32) f--;
        if (f > 8) f = 8;
    }
    if (f <= 1)
    {
        Blur1D(src, tmp, sigma, true);
        Blur1D(tmp, dst, sigma, false);
        return dst;
    }
    SmallSet& s = SmallFor(res / f);
    Pass(G.downSh, s.rt[0], [&]() {
        SetTex(G.downSh, "uSrc", src.texture);
        SetF(G.downSh, "uSrcRes", (float)res);
        SetF(G.downSh, "uF", (float)f);
    });
    Blur1D(s.rt[0], s.rt[1], sigma / (float)f, true);
    Blur1D(s.rt[1], s.rt[2], sigma / (float)f, false);
    return s.rt[2];
}

// GL state the passes disturb. EndTextureMode always returns to the
// screen, so a caller inside its own render texture or a camera would
// otherwise lose both.
struct SavedGl
{
    Matrix proj, view;
    unsigned int fbo;
    int w, h;
};

SavedGl SaveGl()
{
    rlDrawRenderBatchActive();
    SavedGl s;
    s.proj = rlGetMatrixProjection();
    s.view = rlGetMatrixModelview();
    s.fbo = rlGetActiveFramebuffer();
    s.w = rlGetFramebufferWidth();
    s.h = rlGetFramebufferHeight();
    return s;
}

void RestoreGl(const SavedGl& s)
{
    rlDrawRenderBatchActive();
    if (s.fbo != 0) rlEnableFramebuffer(s.fbo);
    else rlDisableFramebuffer();
    rlViewport(0, 0, s.w, s.h);
    rlSetFramebufferWidth(s.w);
    rlSetFramebufferHeight(s.h);
    rlSetMatrixProjection(s.proj);
    rlSetMatrixModelview(s.view);
}

// Seeds: the CPU's uint32 becomes lattice offsets, kept small so the
// float hash stays precise.
void SeedVec(unsigned int seed, float purpose, float* out)
{
    out[0] = (float)(seed & 0x3FFu) * 0.731f + purpose;
    out[1] = (float)((seed >> 10) & 0x3FFu) * 0.517f + purpose * 1.7f;
}

float Hash01(unsigned int x)
{
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return (x >> 8) * (1.0f / 16777216.0f);
}

struct SiteUniforms
{
    bool enabled = false;
    float site[4] = {0, 0, 0, 0};
    float amp[4] = {0, 0, 0, 0};
    int spotCount = 0;
    float spots[9 * 4] = {0};
};

SiteUniforms BuildSite(const TerrainSiteDisturbance* site, int res,
                       float pxPerKm, unsigned int seed, int lvl)
{
    SiteUniforms u;
    if (!site || !site->enabled || !IsSiteDisturbanceEnabled() || pxPerKm <= 0.0f)
        return u;
    float cx = res * 0.5f, cy = res * 0.5f;
    float workedR = site->workedRadiusKm * pxPerKm;
    float outerR = (site->workedRadiusKm + site->fadeKm) * pxPerKm;
    if (outerR < 2.0f) return u;             // site smaller than a pixel
    u.enabled = true;
    u.site[0] = cx; u.site[1] = cy; u.site[2] = workedR; u.site[3] = outerR;
    u.amp[0] = site->toneLevelAmount; u.amp[1] = site->levelAmount;
    u.amp[2] = site->undulationAmp; u.amp[3] = site->roughAmp;
    unsigned int s = seed ^ (0x9E3779B9u * (unsigned int)lvl) ^ 0x51ED270Bu;
    int n = 0;
    u.spots[n * 4 + 0] = cx; u.spots[n * 4 + 1] = cy;
    u.spots[n * 4 + 2] = site->coreRadiusKm * pxPerKm;
    u.spots[n * 4 + 3] = site->spotAmp * (Hash01(s) - 0.5f) * 2.0f;
    n++;
    int domes = std::min(site->domeCount, 8);
    for (int i = 0; i < domes; i++)
    {
        float ang = (90.0f - i * (360.0f / site->domeCount)) * DEG2RAD;
        float ring = site->ringRadiusKm * pxPerKm;
        u.spots[n * 4 + 0] = cx + ring * std::cos(ang);
        u.spots[n * 4 + 1] = cy - ring * std::sin(ang);
        u.spots[n * 4 + 2] = site->domeWorkKm * pxPerKm;
        u.spots[n * 4 + 3] = site->spotAmp * (Hash01(s + 17u * (i + 1)) - 0.5f) * 2.0f;
        n++;
    }
    u.spotCount = n;
    return u;
}

void BindHeight(Shader sh, RenderTexture2D& macro, RenderTexture2D& relief,
                float amp, float k, const SiteUniforms& su, float boulderCell,
                float boulderAmp, unsigned int salt,
                const float frame[4], float kmPerPx)
{
    SetTex(sh, "uMacro", macro.texture);
    SetTex(sh, "uRelief", relief.texture);
    SetF(sh, "uAmp", amp);
    SetF(sh, "uK", k);
    TerrainTuning tune;
    float t[4] = {tune.grain, tune.undulation, tune.formRelief, 0.0f};
    SetV4(sh, "uTune", t);
    SetV4(sh, "uSite", su.site);
    SetV4(sh, "uSiteAmp", su.amp);
    SetV4Array(sh, "uSpots", su.spots, 9);
    SetI(sh, "uSpotCount", su.spotCount);
    SetV2(sh, "uBoulder", boulderCell, boulderAmp);
    SetV4(sh, "uFrame", frame);
    SetF(sh, "uKmPerPx", kmPerPx);
    // The salt separates LAYERS and levels, nothing else. Where the
    // ground is no longer enters here: it enters through uWorldPx, which
    // is the whole point -- a seed keyed to the window would put
    // different detail on the same rock the moment the window moved.
    float v[2];
    SeedVec(salt, 3.0f, v);  SetV2(sh, "uSeedU", v[0], v[1]);
    SeedVec(salt, 11.0f, v); SetV2(sh, "uSeedG", v[0], v[1]);
    SeedVec(salt, 19.0f, v); SetV2(sh, "uSeedL", v[0], v[1]);
    SeedVec(salt, 29.0f, v); SetV2(sh, "uSeedF", v[0], v[1]);
    SeedVec(salt, 41.0f, v); SetV2(sh, "uSeedB", v[0], v[1]);
}

// ---------------------------------------------------------------------------
// Path selection
// ---------------------------------------------------------------------------

int g_path = -1;
int g_gpuRes = 1024;
std::string g_pathWhy;

// One 512 px chain, timed to completion (the read-back is what forces
// the GPU to finish). A discrete GPU does it in a couple of
// milliseconds, an integrated one in ten or so, llvmpipe in over a
// hundred. Call it once untimed first: the mosaic decode and the
// shader compile are one-time costs, not the per-cell cost this
// decision is about.
double ProbeMs()
{
    TerrainGpuChain chain;
    double t0 = GetTime();
    if (!GenerateTerrainChainGPU(TERRAIN_ANCHOR_LAT, TERRAIN_ANCHOR_LON, 512,
                                 &chain, nullptr))
        return 1e9;
    void* px = rlReadTexturePixels(chain.color[2].texture.id, 512, 512,
                                   chain.color[2].texture.format);
    double ms = (GetTime() - t0) * 1000.0;
    if (px) RL_FREE(px);
    UnloadTerrainGpuChain(&chain);
    return ms;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TerrainPath GetTerrainPath()
{
    if (g_path >= 0) return (TerrainPath)g_path;

    const char* env = std::getenv("COLONY_TERRAIN");
    if (env && (std::strcmp(env, "cpu") == 0 || std::strcmp(env, "CPU") == 0))
    {
        g_path = TERRAIN_PATH_CPU;
        g_pathWhy = "COLONY_TERRAIN=cpu";
    }
    else if (env && (std::strcmp(env, "gpu") == 0 || std::strcmp(env, "GPU") == 0))
    {
        g_path = InitGpu() ? TERRAIN_PATH_GPU : TERRAIN_PATH_CPU;
        g_pathWhy = InitGpu() ? "COLONY_TERRAIN=gpu" : "COLONY_TERRAIN=gpu, but shaders failed";
    }
    else
    {
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
        g_path = InitGpu() ? TERRAIN_PATH_GPU : TERRAIN_PATH_CPU;
        g_pathWhy = "browser: no worker threads, WebGL is a real GPU";
#else
        if (!InitGpu())
        {
            g_path = TERRAIN_PATH_CPU;
            g_pathWhy = "shaders failed to build";
        }
        else
        {
            ProbeMs();                       // warm: mosaic, shaders, targets
            double ms = ProbeMs();
            // Tiers: a chain that costs a frame or less can be built at
            // 1024 (four times the work) and still prefetch one
            // neighbour per frame without a visible hitch; a modest GPU
            // stays at 512; anything slower is a rasterizer on the CPU,
            // where the threaded path wins.
            if (ms <= 12.0) { g_path = TERRAIN_PATH_GPU; g_gpuRes = 1024; }
            else if (ms <= 40.0) { g_path = TERRAIN_PATH_GPU; g_gpuRes = 512; }
            else g_path = TERRAIN_PATH_CPU;
            g_pathWhy = TextFormat("512 px chain in %.1f ms", ms);
        }
#endif
    }
    TraceLog(LOG_INFO, "TERRAIN: %s path (%s), %d px",
             GetTerrainPathName(), g_pathWhy.c_str(), GetTerrainPathResolution());
    return (TerrainPath)g_path;
}

const char* GetTerrainPathName()
{
    return (GetTerrainPath() == TERRAIN_PATH_GPU) ? "GPU" : "CPU";
}

int GetTerrainPathResolution()
{
    if (GetTerrainPath() == TERRAIN_PATH_CPU) return 512;
    // COLONY_TERRAIN_RES overrides the GPU resolution (256..2048), for
    // trying a phone's budget on a desktop or a desktop's on llvmpipe.
    const char* env = std::getenv("COLONY_TERRAIN_RES");
    if (env)
    {
        int r = std::atoi(env);
        if (r >= 256 && r <= 2048) return r;
    }
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    return 512;
#else
    return g_gpuRes;
#endif
}

bool GenerateTerrainChainGPU(double latDeg, double lonDeg, int res,
                             TerrainGpuChain* out,
                             const TerrainSiteDisturbance* site,
                             const TerrainChainSpans* spans)
{
    if (!out || res < 64) return false;
    if (!InitGpu() || !EnsureScratch(res)) return false;

    TerrainChainSpans game;
    const TerrainChainSpans& ladder = spans ? *spans : game;
    const int levelCount = std::clamp(ladder.count, 1,
                                      TERRAIN_CHAIN_MAX_LEVELS);
    out->levels = levelCount;

    TerrainMacroCrop crop;
    if (!GetTerrainMacroCrop(latDeg, lonDeg, &crop, ladder.km[0]))
    {
        // No mosaic: the CPU path's flat grey, so the views still draw.
        for (int i = 0; i < levelCount; i++)
        {
            out->color[i] = MakeTarget(res, res, true);
            BeginTextureMode(out->color[i]);
            ClearBackground(Color{40, 40, 48, 255});
            EndTextureMode();
        }
        return true;
    }

    double t0 = GetTime();
    SavedGl saved = SaveGl();

    if (G.cropTex.id != 0) UnloadTexture(G.cropTex);
    G.cropTex = LoadTextureFromImage(crop.image);
    SetTextureFilter(G.cropTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(G.cropTex, TEXTURE_WRAP_CLAMP);
    int cropW = crop.image.width, cropH = crop.image.height;
    const float cropWin[4] = {crop.originX, crop.originY,
                              crop.spanX, crop.spanY};
    UnloadImage(crop.image);

    // Level geometry, as GenerateChainInternal has it.
    const float k = res / 300.0f;
    const float* levelSpanKm = ladder.km;
    double spanDeg[TERRAIN_CHAIN_MAX_LEVELS] = {};
    for (int i = 0; i < levelCount; i++)
        spanDeg[i] = levelSpanKm[i] / MOON_KM_PER_DEG;
    const float SECT_SITE_SCALE = 0.63f;
    TerrainSiteDisturbance sectSite;
    const TerrainSiteDisturbance* siteFor[TERRAIN_CHAIN_MAX_LEVELS] =
        {site, site, site};
    if (site)
    {
        sectSite = *site;
        sectSite.ringRadiusKm *= SECT_SITE_SCALE;
        sectSite.coreRadiusKm *= SECT_SITE_SCALE;
        sectSite.domeWorkKm *= SECT_SITE_SCALE;
        sectSite.workedRadiusKm *= SECT_SITE_SCALE;
        sectSite.fadeKm *= SECT_SITE_SCALE;
        for (int i = 0; i < levelCount; i++)
            if (levelSpanKm[i] <= 5.0f + 1e-3f) siteFor[i] = &sectSite;
    }
    TerrainTuning tune;

    for (int i = 0; i < levelCount; i++)
        out->color[i] = MakeTarget(res, res, true);

    RenderTexture2D* lumPrev = &G.L0;
    RenderTexture2D* lumCur = &G.L1;
    for (int lvl = 0; lvl < levelCount; lvl++)
    {
        // 1. The macro: WAC crop upsampled, or the level above's centre.
        if (lvl == 0)
        {
            Pass(G.macroSh, G.A, [&]() {
                SetTex(G.macroSh, "uCrop", G.cropTex);
                SetV2(G.macroSh, "uCropSize", (float)cropW, (float)cropH);
                float w[4] = {cropWin[0], cropWin[1], cropWin[2], cropWin[3]};
                SetV4(G.macroSh, "uCropWin", w);
            });
        }
        else
        {
            // Exact bounds, as GenerateChainInternal has them: rounding
            // to whole pixels of the level above made a level cover
            // 4.98 km where it claimed 5.
            double frac = spanDeg[lvl] / spanDeg[lvl - 1];
            double half = frac * res / 2.0;
            Pass(G.cropSh, G.A, [&]() {
                SetTex(G.cropSh, "uSrc", lumPrev->texture);
                SetV2(G.cropSh, "uLoFrac", (float)(res / 2.0 - half),
                      (float)(2.0 * half / res));
            });
            Blur(G.A, G.A, G.B, 0.6f * k);
        }

        // 2. Unsharp mask (plus the adaptive contrast on level 0).
        RenderTexture2D& wide = Blur(G.A, G.D, G.B, 5.0f * k);
        Pass(G.sharpenSh, G.C, [&]() {
            SetTex(G.sharpenSh, "uSrc", G.A.texture);
            SetTex(G.sharpenSh, "uBlur", wide.texture);
            if (lvl == 0) SetV3(G.sharpenSh, "uGainMid", crop.gain, crop.mid, 1.0f);
            else SetV3(G.sharpenSh, "uGainMid", 1.0f, 0.5f, 0.0f);
        });

        // 3. The relief proxy: the macro smoothed.
        RenderTexture2D& relief = Blur(G.C, G.D, G.B, 2.5f * k);

        // 4. Site geometry and its means; boulders on the sect level.
        float pxPerKm = (float)res / levelSpanKm[lvl];
        unsigned int lvlSalt = 0x9E3779B9u * (unsigned int)(lvl + 1);

        // This level's geographic extent, the same one MakeNoiseFrame
        // builds for the CPU path and the same CropMacro cuts: spanKm
        // north-south, widened by 1/cos so the window is square on the
        // ground. The shader turns it into a per-pixel world position.
        const double D2R = 3.14159265358979323846 / 180.0;
        double latSpanDeg = levelSpanKm[lvl] / MOON_KM_PER_DEG;
        double lonSpanDeg = latSpanDeg
                          / std::max(0.2, std::cos(latDeg * D2R));
        float frame[4] = {
            (float)(latDeg + latSpanDeg * 0.5),
            (float)(-latSpanDeg / (double)res),
            (float)(lonDeg - lonSpanDeg * 0.5),
            (float)(lonSpanDeg / (double)res)
        };
        float kmPerPx = (float)(levelSpanKm[lvl] / (double)res);

        SiteUniforms su = BuildSite(siteFor[lvl], res, pxPerKm, lvlSalt, lvl);
        float boulderCell = 0.0f, boulderAmp = 0.0f;
        if (levelSpanKm[lvl] <= 5.0f + 1e-3f)
        {
            int count = (int)(120 * k * k * tune.boulders);
            if (count > 0)
            {
                boulderCell = std::max(2.0f, (float)res / std::sqrt((float)count));
                boulderAmp = 0.010f * tune.boulderAmp;
            }
        }
        float amp = 1.0f + 0.7f * lvl;
        if (su.enabled)
        {
            Pass(G.meansSh, G.means, [&]() {
                SetF(G.meansSh, "uRes", (float)res);   // the grid spans the macro, not the 1x1 target
                BindHeight(G.meansSh, G.C, relief, amp, k, su,
                           boulderCell, boulderAmp, lvlSalt,
                           frame, kmPerPx);
            });
        }

        // 5. The fused relight.
        Pass(G.fusedSh, *lumCur, [&]() {
            BindHeight(G.fusedSh, G.C, relief, amp, k, su,
                       boulderCell, boulderAmp, lvlSalt,
                       frame, kmPerPx);
            SetTex(G.fusedSh, "uMeans", G.means.texture);
            SetF(G.fusedSh, "uShadowSteps", (float)(int)(22.0f * k / 1.5f));
            float v[2];
            SeedVec(lvlSalt, 53.0f, v);
            SetV2(G.fusedSh, "uSeedS", v[0], v[1]);
        });

        // 6. Colour.
        Pass(G.rampSh, out->color[lvl], [&]() {
            SetTex(G.rampSh, "uSrc", lumCur->texture);
        });

        std::swap(lumPrev, lumCur);
    }

    RestoreGl(saved);
    TraceLog(LOG_INFO, "TERRAIN: GPU chain at (%.3f, %.3f) %d px submitted in %.1f ms",
             latDeg, lonDeg, res, (GetTime() - t0) * 1000.0);
    return true;
}

void UnloadTerrainGpuChain(TerrainGpuChain* chain)
{
    if (!chain) return;
    // Every slot, not just the real ones: a chain reused for a shorter
    // ladder would otherwise leak the targets the longer one made.
    for (int i = 0; i < TERRAIN_CHAIN_MAX_LEVELS; i++)
        FreeTarget(chain->color[i]);
    chain->levels = TERRAIN_CHAIN_MAX_LEVELS;
}

void UnloadTerrainGpu()
{
    if (!G.init) return;
    FreeTarget(G.A); FreeTarget(G.B); FreeTarget(G.C);
    FreeTarget(G.D); FreeTarget(G.L0); FreeTarget(G.L1);
    for (auto& s : G.small) { for (auto& r : s.rt) FreeTarget(r); s.size = 0; }
    FreeTarget(G.means);
    if (G.cropTex.id != 0) { UnloadTexture(G.cropTex); G.cropTex = {}; }
    if (G.white.id != 0) { UnloadTexture(G.white); G.white = {}; }
    if (G.ok)
    {
        UnloadShader(G.macroSh); UnloadShader(G.cropSh); UnloadShader(G.downSh);
        UnloadShader(G.blurSh); UnloadShader(G.sharpenSh); UnloadShader(G.meansSh);
        UnloadShader(G.fusedSh); UnloadShader(G.rampSh);
    }
    G.locs.clear();
    G.res = 0;
    G.init = false;
    G.ok = false;
}
