// The orbital globe. See lunar_globe.h for what and why.

#include "lunar_globe.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{

// ---------------------------------------------------------------------------
// Shader
//
// One quad, one pass. The body is dialect-neutral and a prefix adapts it
// to GLSL 330 or GLSL ES 100, the same split lunarmap_main.cpp uses --
// the web build gets ES 100, where there are no const arrays and
// precision must be declared.
// ---------------------------------------------------------------------------

const char* FS_PREFIX_330 = R"(
#version 330
in vec2 fragTexCoord;
out vec4 outColor;
#define TEX texture
#define FRAG_OUT outColor
)";

const char* FS_PREFIX_100 = R"(
#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec2 fragTexCoord;
#define TEX texture2D
#define FRAG_OUT gl_FragColor
)";

const char* FS_BODY = R"(
uniform sampler2D uAlbedo;     // WAC mosaic, equirectangular, grey
uniform vec2  uResolution;     // viewport, px
uniform vec2  uCentre;         // disc centre, px, y down
uniform float uRadius;         // disc radius, px
uniform vec4  uTurn;           // cos/sin of sub-viewer lat, then lon
uniform vec3  uSunDir;         // toward the sun, in the moon's own frame
uniform float uSunMix;         // 0 flat mosaic .. 1 full terminator
uniform float uLimb;           // limb darkening
uniform float uStars;          // starfield brightness

float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Space is not black: a faint gradient plus sparse stars, so the globe
// sits in something rather than on nothing. Stars are hashed per 3 px
// cell and fixed to the screen -- they are scenery, not a star chart.
vec3 spaceAt(vec2 px)
{
    vec3 base = vec3(0.020, 0.023, 0.040);
    vec2 cell = floor(px / 3.0);
    float h = hash21(cell);
    float s = max(0.0, h - 0.9972) / 0.0028;
    return base + vec3(s * s * uStars);
}

void main()
{
    vec2 px = fragTexCoord * uResolution;
    vec2 d = (px - uCentre) / uRadius;
    d.y = -d.y;                                  // screen y down, world y up
    float dd = length(d);

    vec3 space = spaceAt(px);
    if (dd > 1.0 + 2.0 / uRadius)
    {
        FRAG_OUT = vec4(space, 1.0);
        return;
    }

    // Unproject onto the near hemisphere, then turn the globe back into
    // its own frame: P = Ry(lon0) . Rx(-lat0) . V
    float zn = sqrt(max(0.0, 1.0 - dd * dd));
    float cl = uTurn.x, sl = uTurn.y, co = uTurn.z, so = uTurn.w;
    float y1 =  d.y * cl + zn * sl;
    float z1 = -d.y * sl + zn * cl;
    vec3 p = vec3(d.x * co + z1 * so, y1, -d.x * so + z1 * co);

    float lat = asin(clamp(p.y, -1.0, 1.0));
    float lon = atan(p.x, p.z);
    vec2 uv = vec2(lon * 0.15915494 + 0.5, 0.5 - lat * 0.31830989);
    float g = TEX(uAlbedo, uv).r;

    // The mosaic is already photographic, so the sun is optional: at
    // mix 0 this is exactly the flat disc the bake produced.
    float shade = 1.0;
    if (uSunMix > 0.0)
    {
        float ndl = dot(p, uSunDir);
        float lit = smoothstep(-0.10, 0.20, ndl);
        shade = mix(1.0, 0.055 + 0.945 * lit, uSunMix);
    }

    // Limb darkening does the work of making a flat circle read as a
    // ball. pow() on a clamped value: zn is 0 exactly at the edge.
    float limb = mix(1.0, pow(max(zn, 0.0001), 0.42), uLimb);

    vec3 col = vec3(g) * shade * limb;
    col *= vec3(0.985, 0.99, 1.0);               // the moon is faintly cool

    // One pixel of coverage at the limb, so the silhouette is smooth at
    // any zoom instead of a staircase.
    float aa = 1.0 - smoothstep(1.0 - 1.5 / uRadius, 1.0 + 0.5 / uRadius, dd);
    FRAG_OUT = vec4(mix(space, col, aa), 1.0);
}
)";

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct Globe
{
    bool tried = false;          // load attempted (success or not)
    bool ok = false;
    Shader shader = {};
    Texture2D albedo = {};
    Texture2D white = {};        // 1x1, so the quad carries 0..1 texcoords
    int locAlbedo = -1, locResolution = -1, locCentre = -1, locRadius = -1;
    int locTurn = -1, locSunDir = -1, locSunMix = -1, locLimb = -1, locStars = -1;

    double spinDegPerSec = 2.5;
    double sunLonDeg = -35.0;    // matches the mosaic's own lighting bias
    double sunLatDeg = 8.0;
    float sunMix = 0.0f;         // flat by default: the bake looked right
    float limb = 0.55f;
    float stars = 0.75f;

    bool dragging = false;
    bool movedThisPress = false;   // press has travelled past the threshold
    Vector2 dragFrom = {0, 0};
    Vector2 pressAt = {0, 0};
};

// How far the pointer must travel before a press counts as turning the
// globe rather than pointing at it. A few pixels: enough to absorb the
// shake of a click, small enough that a deliberate drag feels immediate.
const float DRAG_THRESHOLD_PX = 5.0f;

Globe g;

// The mosaic is 8192x4096. Uploading it whole is 33 MB as one channel --
// fine on a desktop GPU and worth it, because at full zoom the globe is
// magnifying 1.3 km/px imagery. WebGL1 only guarantees 2048, and a phone
// should not be asked for 33 MB either, so the web build takes a
// quarter-size mosaic.
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
const int ALBEDO_MAX_WIDTH = 2048;
#else
const int ALBEDO_MAX_WIDTH = 8192;
#endif

bool LoadAlbedo()
{
    Image img = LoadImage("src/assets/planet/wac_global.jpg");
    if (img.data == nullptr)
    {
        TraceLog(LOG_WARNING, "GLOBE: src/assets/planet/wac_global.jpg not found");
        return false;
    }
    // The moon is grey and the shader reads one channel: dropping to
    // 8-bit luminance before upload is a third of the memory for an
    // identical picture.
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    if (img.width > ALBEDO_MAX_WIDTH)
    {
        int w = ALBEDO_MAX_WIDTH;
        int h = std::max(1, img.height * ALBEDO_MAX_WIDTH / img.width);
        ImageResize(&img, w, h);
    }
    g.albedo = LoadTextureFromImage(img);
    int w = img.width, h = img.height;
    UnloadImage(img);
    if (g.albedo.id == 0) return false;

    // Bilinear across the surface; the equirect wraps in longitude, so
    // the seam at +-180 must repeat rather than clamp or it shows as a
    // bright edge when the globe turns past the far side.
    SetTextureFilter(g.albedo, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(g.albedo, TEXTURE_WRAP_REPEAT);
    TraceLog(LOG_INFO, "GLOBE: mosaic %dx%d uploaded (%.1f MB)",
             w, h, (double)w * h / (1024.0 * 1024.0));
    return true;
}

bool Init()
{
    if (g.tried) return g.ok;
    g.tried = true;

    bool es100 = false;
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    es100 = true;
#endif
    std::string fs = std::string(es100 ? FS_PREFIX_100 : FS_PREFIX_330) + FS_BODY;
    g.shader = LoadShaderFromMemory(nullptr, fs.c_str());
    // raylib hands back the DEFAULT shader when compilation fails, so a
    // non-zero id is not proof of success.
    if (g.shader.id == 0 || g.shader.id == rlGetShaderIdDefault())
    {
        TraceLog(LOG_WARNING, "GLOBE: shader failed to build, keeping the flat disc");
        g.shader = {};
        return false;
    }

    if (!LoadAlbedo())
    {
        UnloadShader(g.shader);
        g.shader = {};
        return false;
    }

    Image px = GenImageColor(1, 1, WHITE);
    g.white = LoadTextureFromImage(px);
    UnloadImage(px);

    g.locAlbedo = GetShaderLocation(g.shader, "uAlbedo");
    g.locResolution = GetShaderLocation(g.shader, "uResolution");
    g.locCentre = GetShaderLocation(g.shader, "uCentre");
    g.locRadius = GetShaderLocation(g.shader, "uRadius");
    g.locTurn = GetShaderLocation(g.shader, "uTurn");
    g.locSunDir = GetShaderLocation(g.shader, "uSunDir");
    g.locSunMix = GetShaderLocation(g.shader, "uSunMix");
    g.locLimb = GetShaderLocation(g.shader, "uLimb");
    g.locStars = GetShaderLocation(g.shader, "uStars");

    g.ok = true;
    return true;
}

} // namespace

bool LunarGlobeReady() { return Init(); }

void DrawLunarGlobe(int screenWidth, int screenHeight)
{
    if (!Init()) return;

    const OrbitalCamera& cam = GetOrbitalCamera();
    float lat0 = (float)(cam.subLatDeg * DEG2RAD);
    float lon0 = (float)(cam.subLonDeg * DEG2RAD);
    float turn[4] = { std::cos(lat0), std::sin(lat0),
                      std::cos(lon0), std::sin(lon0) };

    // The sun is a direction in the moon's own frame, so it stays put on
    // the surface as the globe turns -- which is the whole point of
    // having a terminator rather than a screen-space vignette.
    float sunLat = (float)(g.sunLatDeg * DEG2RAD);
    float sunLon = (float)(g.sunLonDeg * DEG2RAD);
    float sun[3] = { std::cos(sunLat) * std::sin(sunLon),
                     std::sin(sunLat),
                     std::cos(sunLat) * std::cos(sunLon) };

    float resolution[2] = { (float)screenWidth, (float)screenHeight };
    float centre[2] = { screenWidth * 0.5f, screenHeight * 0.5f };
    float radius = (float)OrbitalDiscRadiusPx(screenWidth, screenHeight);

    BeginShaderMode(g.shader);
    if (g.locAlbedo >= 0) SetShaderValueTexture(g.shader, g.locAlbedo, g.albedo);
    if (g.locResolution >= 0) SetShaderValue(g.shader, g.locResolution, resolution, SHADER_UNIFORM_VEC2);
    if (g.locCentre >= 0) SetShaderValue(g.shader, g.locCentre, centre, SHADER_UNIFORM_VEC2);
    if (g.locRadius >= 0) SetShaderValue(g.shader, g.locRadius, &radius, SHADER_UNIFORM_FLOAT);
    if (g.locTurn >= 0) SetShaderValue(g.shader, g.locTurn, turn, SHADER_UNIFORM_VEC4);
    if (g.locSunDir >= 0) SetShaderValue(g.shader, g.locSunDir, sun, SHADER_UNIFORM_VEC3);
    if (g.locSunMix >= 0) SetShaderValue(g.shader, g.locSunMix, &g.sunMix, SHADER_UNIFORM_FLOAT);
    if (g.locLimb >= 0) SetShaderValue(g.shader, g.locLimb, &g.limb, SHADER_UNIFORM_FLOAT);
    if (g.locStars >= 0) SetShaderValue(g.shader, g.locStars, &g.stars, SHADER_UNIFORM_FLOAT);

    // Through a 1x1 texture, not DrawRectangle: a plain rectangle is
    // textured from a few texels of the font atlas, so fragTexCoord
    // barely moves across it and the whole quad samples one point.
    DrawTexturePro(g.white, Rectangle{0, 0, 1, 1},
                   Rectangle{0, 0, (float)screenWidth, (float)screenHeight},
                   Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
}

bool UpdateLunarGlobeInput(int screenWidth, int screenHeight, float dtSeconds)
{
    OrbitalCamera cam = GetOrbitalCamera();
    double radius = OrbitalDiscRadiusPx(screenWidth, screenHeight);
    if (radius <= 0.0) return false;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        // The wheel stops where the mosaic does; only a descent flight
        // is allowed deeper, and it ends by handing over to the DEM.
        cam.zoom = std::clamp(cam.zoom * std::pow(1.18, (double)wheel),
                              ORBITAL_ZOOM_MIN, ORBITAL_ZOOM_USER_MAX);
    }

    Vector2 m = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        g.dragging = true;
        g.dragFrom = m;
        g.pressAt = m;
        g.movedThisPress = false;    // cleared here, so it survives the release
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) g.dragging = false;

    bool spun = false;
    if (g.dragging)
    {
        double dx = m.x - g.dragFrom.x;
        double dy = m.y - g.dragFrom.y;
        if (std::hypot(m.x - g.pressAt.x, m.y - g.pressAt.y) > DRAG_THRESHOLD_PX)
            g.movedThisPress = true;
        if (dx != 0.0 || dy != 0.0)
        {
            // Dragging by one radius turns the globe a quarter turn,
            // which keeps the surface moving with the pointer at the
            // centre without running away near the limb.
            cam.subLonDeg -= dx / radius * 90.0;
            cam.subLatDeg += dy / radius * 90.0;
            g.dragFrom = m;
            spun = true;
        }
    }
    else if (g.spinDegPerSec != 0.0)
    {
        cam.subLonDeg += g.spinDegPerSec * (double)dtSeconds;
    }

    SetOrbitalCamera(cam);
    return spun;
}

bool LunarGlobeWasDragged() { return g.movedThisPress; }

void SetLunarGlobeSpin(double degreesPerSecond) { g.spinDegPerSec = degreesPerSecond; }

void SetLunarGlobeSun(double sunLonDeg, double sunLatDeg, float mix)
{
    g.sunLonDeg = sunLonDeg;
    g.sunLatDeg = sunLatDeg;
    g.sunMix = std::clamp(mix, 0.0f, 1.0f);
}

void UnloadLunarGlobe()
{
    if (!g.tried) return;
    if (g.albedo.id != 0) UnloadTexture(g.albedo);
    if (g.white.id != 0) UnloadTexture(g.white);
    if (g.ok) UnloadShader(g.shader);
    g = Globe{};
}
