// Real-elevation lunar map tool (`lunar_map`).
//
// Renders the actual Moon from the LOLA LDEM_16 elevation model
// (prototypes/planet_visuals/data/lola/ldem_16_uint.tif) — the whole
// near side, or any picked region — as a 3D heightmap mesh with a
// lunar-specific shading model:
//
//   real DEM heightmap  -> terrain elevation (mesh + shading textures)
//   per-pixel normals   -> precomputed from the DEM at physical scale
//   explicit sun vector -> harsh light, near-black shadows, no haze
//   WAC albedo crop     -> subtle real gray/brown surface variation
//   multi-scale noise   -> fine regolith variation (kept subtle)
//   curvature term      -> crater rims and floors read stronger
//
// Two styles: `shaded` (photographic hillshade look) and `color`
// (LOLA-style rainbow elevation map, hillshade-modulated).
//
// Standalone tool beside the game: only raylib + LolaDem, no game code.
// Builds for the desktop (native + headless PNG export) and for the
// web (PLATFORM=Web; deployed to GitHub Pages at /lunarmap/). The
// shading avoids float textures and ships the fragment shader in both
// GLSL 330 and GLSL ES 100 dialects, so the same look survives WebGL1:
// normals + curvature live in an RGBA8 texture computed on the CPU,
// and the colour ramp reads a 16-bit split-channel height texture.
//
// Usage (from the repo root, so relative asset paths resolve):
//   cmake --build build --target lunar_map
//   tools/lunarmap/lunarmap.sh --nearside
//   tools/lunarmap/lunarmap.sh --pick 43.3,17.3 --span 250   # Aristoteles
//   ./build/src/lunar_map --pick -43.3,-11.4 --tilt          # interactive
//
// Headless (--out) needs the software-GL wrapper the script applies.
//
// Interactive: on the near-side map, a click (or tap) dives into a
// 200 km regional window at that spot; BACK returns. On-screen buttons
// cover style, view, sun and exaggeration, so a phone can drive it.

#include "raylib.h"
#include "raymath.h"

#include "lola_dem.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static const char* DEFAULT_DEM_PATH =
    "prototypes/planet_visuals/data/lola/ldem_16_uint.tif";
static const char* DEFAULT_WAC_PATH = "src/assets/planet/wac_global.jpg";

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct MapOptions
{
    bool nearside = true;          // full near side unless --pick given
    double pickLat = 0.0;
    double pickLon = 0.0;
    double spanKm = 200.0;         // regional window size (square)
    std::string style = "shaded";  // shaded | color
    float sunAzimuthDeg = 315.0f;  // clockwise from north (NW default)
    float sunElevationDeg = 30.0f;
    float exaggeration = 2.0f;     // vertical scale multiplier
    float detail = 1.0f;           // sub-floor synthesis strength (0 = off)
    std::string interp = "catrom"; // catrom | bspline | lanczos | fractal
    float ambient = 0.06f;
    int width = 1200;
    int height = 1200;
    int demRes = 0;                // shading texture res (0 = auto)
    int meshRes = 256;             // mesh grid resolution (max 256)
    bool tilt = false;             // tilted 3D slab instead of top-down
    std::string outPath;           // empty = interactive window
    std::string demPath = DEFAULT_DEM_PATH;
    std::string wacPath = DEFAULT_WAC_PATH;
    bool webShader = false;        // force the GLSL ES 100 shader (debug)
    float orbitYawDeg = 180.0f;    // tilt-view camera angles (--orbit)
    float orbitPitchDeg = 52.0f;
};

static void PrintUsage()
{
    std::cout
        << "Usage: lunar_map [options]\n"
        << "\n"
        << "  --nearside        whole near side map (default)\n"
        << "  --pick LAT,LON    regional window centred on real coordinates\n"
        << "  --span KM         regional window size    (default: 200)\n"
        << "  --style NAME      shaded | color          (default: shaded)\n"
        << "  --sun AZ,EL       sun azimuth/elevation   (default: 315,30)\n"
        << "  --exag F          vertical exaggeration   (default: 2.0)\n"
        << "  --detail F        sub-floor synthesis     (default: 1.0, 0=off)\n"
        << "  --ambient F       ambient light level     (default: 0.06)\n"
        << "  --tilt            tilted 3D slab view instead of top-down\n"
        << "  --orbit YAW,PITCH tilt camera angles (default: 180,52)\n"
        << "  --size WxH        output resolution       (default: 1200x1200)\n"
        << "  --demres N        shading texture pixels  (default: auto)\n"
        << "  --meshres N       mesh grid resolution    (default: 256)\n"
        << "  --out PATH        render PNG and exit (else: interactive)\n"
        << "  --dem PATH        DEM TIFF path\n"
        << "  --webshader       use the GLSL ES 100 shader on desktop\n"
        << "  --help            show this message\n"
        << "\n"
        << "Interactive: click the near-side map to dive into a 200 km\n"
        << "window; on-screen buttons or keys (TAB style, T tilt, arrows\n"
        << "sun, Q/E exaggeration, wheel zoom, drag orbit in tilt).\n";
}

static bool ParseArgs(int argc, char** argv, MapOptions& options)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        bool hasNext = (i + 1) < argc;

        if (arg == "--help" || arg == "-h") { PrintUsage(); return false; }
        else if (arg == "--nearside") { options.nearside = true; }
        else if (arg == "--pick" && hasNext)
        {
            options.nearside = false;
            if (std::sscanf(argv[++i], "%lf,%lf",
                            &options.pickLat, &options.pickLon) != 2)
            {
                std::cerr << "Bad --pick, expected LAT,LON\n";
                return false;
            }
        }
        else if (arg == "--span" && hasNext) { options.spanKm = std::atof(argv[++i]); }
        else if (arg == "--style" && hasNext) { options.style = argv[++i]; }
        else if (arg == "--sun" && hasNext)
        {
            if (std::sscanf(argv[++i], "%f,%f", &options.sunAzimuthDeg,
                            &options.sunElevationDeg) != 2)
            {
                std::cerr << "Bad --sun, expected AZ,EL\n";
                return false;
            }
        }
        else if (arg == "--exag" && hasNext) { options.exaggeration = (float)std::atof(argv[++i]); }
        else if (arg == "--detail" && hasNext) { options.detail = (float)std::atof(argv[++i]); }
        else if (arg == "--interp" && hasNext) { options.interp = argv[++i]; }
        else if (arg == "--ambient" && hasNext) { options.ambient = (float)std::atof(argv[++i]); }
        else if (arg == "--tilt") { options.tilt = true; }
        else if (arg == "--orbit" && hasNext)
        {
            options.tilt = true;
            if (std::sscanf(argv[++i], "%f,%f", &options.orbitYawDeg,
                            &options.orbitPitchDeg) != 2)
            {
                std::cerr << "Bad --orbit, expected YAW,PITCH\n";
                return false;
            }
            options.orbitPitchDeg = Clamp(options.orbitPitchDeg,
                                          10.0f, 88.0f);
        }
        else if (arg == "--size" && hasNext)
        {
            if (std::sscanf(argv[++i], "%dx%d",
                            &options.width, &options.height) != 2)
            {
                std::cerr << "Bad --size, expected WxH\n";
                return false;
            }
        }
        else if (arg == "--demres" && hasNext) { options.demRes = std::atoi(argv[++i]); }
        else if (arg == "--meshres" && hasNext) { options.meshRes = std::atoi(argv[++i]); }
        else if (arg == "--out" && hasNext) { options.outPath = argv[++i]; }
        else if (arg == "--dem" && hasNext) { options.demPath = argv[++i]; }
        else if (arg == "--webshader") { options.webShader = true; }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage();
            return false;
        }
    }
    if (options.style != "shaded" && options.style != "color")
    {
        std::cerr << "Bad --style, expected shaded|color\n";
        return false;
    }
    options.meshRes = std::clamp(options.meshRes, 8, 256);
    return true;
}

// ---------------------------------------------------------------------------
// Shaders
//
// The heightmap controls geometry; the fragment shader controls the
// lunar appearance. Per-pixel normals and the curvature term are
// precomputed on the CPU into shadeMap (RGBA8: xyz normal, w
// curvature), so no float textures are needed and WebGL1 works. The
// colour ramp reads heightMap (RGBA8: R hi / G lo byte of normalised
// elevation, NEAREST-filtered, manual bilinear in the shader). The
// shader body is dialect-neutral; a prefix adapts it to GLSL 330 or
// GLSL ES 100.
// ---------------------------------------------------------------------------

static const char* VS_330 = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 mvp;
out vec2 fragTexCoord;
void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* VS_100 = R"(
#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
uniform mat4 mvp;
varying vec2 fragTexCoord;
void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* FS_PREFIX_330 = R"(
#version 330
in vec2 fragTexCoord;
out vec4 outColor;
#define TEX texture
#define FRAG_OUT outColor
)";

static const char* FS_PREFIX_100 = R"(
#version 100
precision highp float;
varying vec2 fragTexCoord;
#define TEX texture2D
#define FRAG_OUT gl_FragColor
)";

static const char* FS_BODY = R"(
uniform sampler2D shadeMap;    // xyz: normal, w: curvature
uniform sampler2D heightMap;   // RG: 16-bit normalised elevation
uniform sampler2D albedoMap;   // WAC mosaic crop

uniform vec2 texelSize;        // 1 / heightMap resolution
uniform vec3 sunDirection;     // toward the sun, normalised
uniform vec3 sunColor;
uniform float ambient;
uniform float styleFlag;       // 0 shaded, 1 colour elevation
uniform float curveStrength;   // crater-rim emphasis (map scales only)

// Hash-based value noise — cheap regolith variation, several scales.
float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float Noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(Hash(i), Hash(i + vec2(1.0, 0.0)), u.x),
               mix(Hash(i + vec2(0.0, 1.0)),
                   Hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float DecodeHeight(vec2 uv)
{
    vec4 c = TEX(heightMap, uv);
    return c.r * (255.0 * 256.0 / 65535.0) + c.g * (255.0 / 65535.0);
}

// Manual bilinear over the NEAREST-filtered split-channel height:
// hardware filtering would blend the hi/lo bytes independently.
float SmoothHeight(vec2 uv)
{
    vec2 st = uv / texelSize - 0.5;
    vec2 f = fract(st);
    vec2 base = (floor(st) + 0.5) * texelSize;
    float h00 = DecodeHeight(base);
    float h10 = DecodeHeight(base + vec2(texelSize.x, 0.0));
    float h01 = DecodeHeight(base + vec2(0.0, texelSize.y));
    float h11 = DecodeHeight(base + texelSize);
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

// LOLA-style elevation ramp (deep blue -> green -> red -> white),
// built as a mix cascade so GLSL ES 100 (no const arrays) can run it.
vec3 ElevationRamp(float t)
{
    float x = clamp(t, 0.0, 1.0) * 8.0;
    vec3 c = vec3(0.10, 0.05, 0.35);
    c = mix(c, vec3(0.10, 0.25, 0.75), clamp(x, 0.0, 1.0));
    c = mix(c, vec3(0.05, 0.55, 0.85), clamp(x - 1.0, 0.0, 1.0));
    c = mix(c, vec3(0.10, 0.65, 0.40), clamp(x - 2.0, 0.0, 1.0));
    c = mix(c, vec3(0.45, 0.75, 0.20), clamp(x - 3.0, 0.0, 1.0));
    c = mix(c, vec3(0.95, 0.85, 0.20), clamp(x - 4.0, 0.0, 1.0));
    c = mix(c, vec3(0.95, 0.50, 0.15), clamp(x - 5.0, 0.0, 1.0));
    c = mix(c, vec3(0.85, 0.15, 0.10), clamp(x - 6.0, 0.0, 1.0));
    c = mix(c, vec3(0.98, 0.96, 0.94), clamp(x - 7.0, 0.0, 1.0));
    return c;
}

void main()
{
    vec2 uv = fragTexCoord;

    vec4 shade = TEX(shadeMap, uv);
    vec3 normal = normalize(shade.xyz * 2.0 - 1.0);
    // Curvature emphasis helps landform legibility at map scales but
    // reads as a sourceless glow around rims in close-ups — real
    // photographs shade directionally only. Faded out when zoomed in.
    float curve = (shade.w * 2.0 - 1.0) * 0.12 * curveStrength;

    // Harsh lunar sunlight: pure Lambert, low ambient, no haze.
    float NdotL = max(dot(normal, sunDirection), 0.0);
    // Deepen the shadow side a touch (s-curve), keeping lit slopes linear.
    float lit = NdotL * NdotL * (3.0 - 2.0 * NdotL);
    lit = mix(NdotL, lit, 0.35);

    // Multi-scale regolith variation (subtle, or it reads procedural).
    float largeNoise = Noise(uv * 12.0);
    float mediumNoise = Noise(uv * 60.0);
    float fineNoise = Noise(uv * 280.0);
    float variation = (largeNoise - 0.5) * 0.10
                    + (mediumNoise - 0.5) * 0.05
                    + (fineNoise - 0.5) * 0.03;

    vec3 light = sunColor * lit + vec3(ambient);

    if (styleFlag > 0.5)
    {
        // Colour elevation map, hillshade-modulated for readability.
        vec3 ramp = ElevationRamp(SmoothHeight(uv));
        float rampShade = 0.42 + 0.68 * lit + curve;
        FRAG_OUT = vec4(ramp * rampShade, 1.0);
        return;
    }

    // Photographic style: real albedo pulled toward neutral gray,
    // sun-lit, with crater emphasis and regolith grain.
    vec3 albedo = TEX(albedoMap, uv).rgb;
    float gray = dot(albedo, vec3(0.299, 0.587, 0.114));
    // WAC brightness already encodes its own sun; use it gently.
    vec3 surface = mix(vec3(0.62), mix(vec3(gray), albedo, 0.55), 0.75);
    // Regolith grain fades with the light: noise on a night side only
    // reads as streaking, not texture.
    surface *= 1.0 + variation * (0.25 + 0.75 * lit);
    surface *= 1.0 + curve * 1.6;

    vec3 color = surface * light;
    // Slight tonal lift so shadow detail is not pure black on screens.
    color = pow(clamp(color, 0.0, 1.0), vec3(0.92));
    FRAG_OUT = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Terrain construction
// ---------------------------------------------------------------------------

// World layout: x east, z south, y up. The window is centred on the
// origin, and geometry is normalised so the long side is 200 world
// units — raylib's default far clip plane (1000) would otherwise
// swallow a 5458 km near-side map. Physical correctness is preserved
// because the normals are precomputed from real metre gradients.
struct TerrainScene
{
    LolaWindow window;
    Mesh mesh = { 0 };
    Model model = { 0 };
    Texture2D shadeTex = { 0 };
    Texture2D heightTex = { 0 };
    Texture2D albedoTex = { 0 };
    Shader shader = { 0 };
    bool nearside = true;
    float worldWidthKm = 0.0f;     // east-west extent (real km)
    float worldHeightKm = 0.0f;    // north-south extent (real km)
    float worldScale = 1.0f;       // world units per km
    double lat0 = 0.0, lat1 = 0.0, lon0 = 0.0, lon1 = 0.0;
    int locTexel = 0, locSunDir = 0, locSunColor = 0;
    int locAmbient = 0, locStyle = 0, locCurve = 0;

    float ScaledW() const { return worldWidthKm * worldScale; }
    float ScaledH() const { return worldHeightKm * worldScale; }
};

// worldW/worldH and exaggeration arrive pre-multiplied by the scene's
// worldScale, so vertex heights stay proportional to the ground plane.
// Heights are centred on the window's mid elevation: at small spans the
// world scale magnifies absolute elevations (a -2.7 km site in a 1 km
// window would sit 1080 units below the origin, outside the far clip).
static Mesh BuildTerrainMesh(const LolaWindow& window, float worldW,
                             float worldH, float exaggeration, int gridRes)
{
    float centerElevM = (window.minElevationM + window.maxElevationM) / 2.0f;
    int nx = gridRes, nz = gridRes;
    Mesh mesh = { 0 };
    mesh.vertexCount = nx * nz;
    mesh.triangleCount = (nx - 1) * (nz - 1) * 2;
    mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.normals = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.indices = (unsigned short*)MemAlloc(
        mesh.triangleCount * 3 * sizeof(unsigned short));

    float kmPerMetre = exaggeration / 1000.0f;
    int res = window.resolution;
    for (int j = 0; j < nz; j++)
    {
        float v = (float)j / (nz - 1);
        for (int i = 0; i < nx; i++)
        {
            float u = (float)i / (nx - 1);
            int sx = std::min(res - 1, (int)(u * (res - 1) + 0.5f));
            int sy = std::min(res - 1, (int)(v * (res - 1) + 0.5f));
            float elev = window.elevationM[(size_t)sy * res + sx]
                         - centerElevM;
            int k = j * nx + i;
            mesh.vertices[k * 3 + 0] = (u - 0.5f) * worldW;
            mesh.vertices[k * 3 + 1] = elev * kmPerMetre;
            mesh.vertices[k * 3 + 2] = (v - 0.5f) * worldH;
            mesh.texcoords[k * 2 + 0] = u;
            mesh.texcoords[k * 2 + 1] = v;
            mesh.normals[k * 3 + 0] = 0.0f;
            mesh.normals[k * 3 + 1] = 1.0f;
            mesh.normals[k * 3 + 2] = 0.0f;
        }
    }
    int t = 0;
    for (int j = 0; j < nz - 1; j++)
    {
        for (int i = 0; i < nx - 1; i++)
        {
            unsigned short a = (unsigned short)(j * nx + i);
            unsigned short b = (unsigned short)(a + 1);
            unsigned short c = (unsigned short)(a + nx);
            unsigned short d = (unsigned short)(c + 1);
            mesh.indices[t++] = a; mesh.indices[t++] = c; mesh.indices[t++] = b;
            mesh.indices[t++] = b; mesh.indices[t++] = c; mesh.indices[t++] = d;
        }
    }
    UploadMesh(&mesh, false);
    return mesh;
}

// RGBA8 shading texture: xyz = surface normal at physical scale (with
// vertical exaggeration folded in), w = curvature (crater rim/floor
// emphasis), both straight from the DEM window on the CPU.
static Texture2D BuildShadeTexture(const LolaWindow& window,
                                   float worldWidthKm, float worldHeightKm,
                                   float exaggeration)
{
    int res = window.resolution;
    float dxM = worldWidthKm * 1000.0f / res;
    float dyM = worldHeightKm * 1000.0f / res;

    Image img = { 0 };
    img.width = res;
    img.height = res;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    unsigned char* px = (unsigned char*)MemAlloc((size_t)res * res * 4);
    img.data = px;

    const std::vector<float>& e = window.elevationM;
    for (int y = 0; y < res; y++)
    {
        int ym = std::max(0, y - 1), yp = std::min(res - 1, y + 1);
        for (int x = 0; x < res; x++)
        {
            int xm = std::max(0, x - 1), xp = std::min(res - 1, x + 1);
            float h = e[(size_t)y * res + x];
            float hL = e[(size_t)y * res + xm];
            float hR = e[(size_t)y * res + xp];
            float hU = e[(size_t)ym * res + x];
            float hD = e[(size_t)yp * res + x];
            float gx = (hR - hL) / (dxM * (xp - xm)) * exaggeration;
            float gy = (hD - hU) / (dyM * (yp - ym)) * exaggeration;
            Vector3 n = Vector3Normalize(Vector3{ -gx, 1.0f, -gy });
            float lap = hL + hR + hU + hD - 4.0f * h;
            float curve = Clamp(-lap * 3.0f * exaggeration / dxM,
                                -0.12f, 0.12f);
            size_t k = ((size_t)y * res + x) * 4;
            px[k + 0] = (unsigned char)((n.x * 0.5f + 0.5f) * 255.0f);
            px[k + 1] = (unsigned char)((n.y * 0.5f + 0.5f) * 255.0f);
            px[k + 2] = (unsigned char)((n.z * 0.5f + 0.5f) * 255.0f);
            px[k + 3] = (unsigned char)((curve / 0.12f * 0.5f + 0.5f)
                                        * 255.0f);
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    return tex;
}

// 16-bit normalised elevation split across R (hi) and G (lo). Must be
// NEAREST-filtered — the shader does its own bilinear.
static Texture2D BuildHeightTexture(const LolaWindow& window)
{
    int res = window.resolution;
    float range = std::max(1.0f,
                           window.maxElevationM - window.minElevationM);
    Image img = { 0 };
    img.width = res;
    img.height = res;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    unsigned char* px = (unsigned char*)MemAlloc((size_t)res * res * 4);
    img.data = px;
    for (size_t i = 0; i < (size_t)res * res; i++)
    {
        float t = (window.elevationM[i] - window.minElevationM) / range;
        int q = std::clamp((int)(t * 65535.0f + 0.5f), 0, 65535);
        px[i * 4 + 0] = (unsigned char)(q >> 8);
        px[i * 4 + 1] = (unsigned char)(q & 0xFF);
        px[i * 4 + 2] = 0;
        px[i * 4 + 3] = 255;
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    return tex;
}

// Crop the WAC mosaic to the window's lat/lon rectangle. Above ~75
// degrees latitude the equirect mosaic is orbit-seam streaks, not
// ground: polar windows get the neutral fallback instead.
static Texture2D BuildAlbedoTexture(const MapOptions& options,
                                    double lat0, double lat1,
                                    double lon0, double lon1)
{
    double midLat = (lat0 + lat1) / 2.0;
    Image wac = (std::fabs(midLat) <= 75.0)
        ? LoadImage(options.wacPath.c_str())
        : Image{ 0 };
    if (wac.data == nullptr)
    {
        // Neutral gray fallback: the shader pulls toward gray anyway.
        Image flat = GenImageColor(4, 4, Color{ 158, 156, 152, 255 });
        Texture2D tex = LoadTextureFromImage(flat);
        UnloadImage(flat);
        return tex;
    }
    double x0 = (lon0 + 180.0) / 360.0 * wac.width;
    double x1 = (lon1 + 180.0) / 360.0 * wac.width;
    double y0 = (90.0 - lat1) / 180.0 * wac.height;
    double y1 = (90.0 - lat0) / 180.0 * wac.height;
    // Integer rect: raylib's ImageCrop overruns its allocation by a
    // row/column when handed fractional coordinates.
    int cx = std::clamp((int)std::floor(x0), 0, wac.width - 2);
    int cy = std::clamp((int)std::floor(y0), 0, wac.height - 2);
    int cwidth = std::clamp((int)std::ceil(x1 - x0), 1, wac.width - cx);
    int cheight = std::clamp((int)std::ceil(y1 - y0), 1, wac.height - cy);
    Rectangle crop = { (float)cx, (float)cy, (float)cwidth, (float)cheight };
    ImageCrop(&wac, crop);
    int maxSide = 2048;
    if (wac.width > maxSide || wac.height > maxSide)
    {
        float s = (float)maxSide / std::max(wac.width, wac.height);
        ImageResize(&wac, (int)(wac.width * s), (int)(wac.height * s));
    }
    Texture2D tex = LoadTextureFromImage(wac);
    UnloadImage(wac);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    return tex;
}

static Vector3 SunDirection(float azimuthDeg, float elevationDeg)
{
    // Azimuth clockwise from north; north is -Z in world space.
    float az = azimuthDeg * DEG2RAD;
    float el = elevationDeg * DEG2RAD;
    return Vector3Normalize(Vector3{
        sinf(az) * cosf(el), sinf(el), -cosf(az) * cosf(el) });
}

static void UnloadSceneGpu(TerrainScene& scene)
{
    if (scene.model.meshCount > 0) UnloadModel(scene.model);
    if (scene.shadeTex.id > 0) UnloadTexture(scene.shadeTex);
    if (scene.heightTex.id > 0) UnloadTexture(scene.heightTex);
    if (scene.albedoTex.id > 0) UnloadTexture(scene.albedoTex);
    if (scene.shader.id > 0) UnloadShader(scene.shader);
    scene.model = Model{ 0 };
    scene.shadeTex = scene.heightTex = scene.albedoTex = Texture2D{ 0 };
    scene.shader = Shader{ 0 };
}

// (Re)build mesh + shading textures from the already-extracted window.
// Split out so an exaggeration change does not re-query the DEM.
static void BuildSceneGeometry(TerrainScene& scene,
                               const MapOptions& options)
{
    if (scene.model.meshCount > 0) UnloadModel(scene.model);
    if (scene.shadeTex.id > 0) UnloadTexture(scene.shadeTex);
    scene.mesh = BuildTerrainMesh(scene.window, scene.ScaledW(),
                                  scene.ScaledH(),
                                  options.exaggeration * scene.worldScale,
                                  options.meshRes);
    scene.model = LoadModelFromMesh(scene.mesh);
    scene.shadeTex = BuildShadeTexture(scene.window, scene.worldWidthKm,
                                       scene.worldHeightKm,
                                       options.exaggeration);
    scene.model.materials[0].shader = scene.shader;
    scene.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
        scene.shadeTex;
    scene.model.materials[0].maps[MATERIAL_MAP_SPECULAR].texture =
        scene.heightTex;
    scene.model.materials[0].maps[MATERIAL_MAP_NORMAL].texture =
        scene.albedoTex;
}

static bool BuildScene(const MapOptions& options, const LolaDem& dem,
                       TerrainScene& scene)
{
    UnloadSceneGpu(scene);
    scene.nearside = options.nearside;

    int texRes;
    if (options.nearside)
    {
        scene.lat0 = -90.0; scene.lat1 = 90.0;
        scene.lon0 = -90.0; scene.lon1 = 90.0;
        int res = (options.demRes > 0) ? options.demRes : 2048;
        texRes = std::clamp(res, 64, 2880);
        scene.window = dem.WindowDegrees(scene.lat0, scene.lat1,
                                         scene.lon0, scene.lon1,
                                         texRes, texRes);
        // Plate carrée map: both axes in equator-scale km.
        scene.worldWidthKm = (float)(180.0 * LOLA_M_PER_DEG / 1000.0);
        scene.worldHeightKm = scene.worldWidthKm;
    }
    else
    {
        int res = (options.demRes > 0) ? options.demRes : 1024;
        texRes = std::clamp(res, 64, 4096);
        scene.window = dem.Window(options.pickLat, options.pickLon,
                                  options.spanKm, texRes, options.detail);
        double spanDeg = options.spanKm * 1000.0 / LOLA_M_PER_DEG;
        double c = std::max(0.2, std::cos(options.pickLat * DEG2RAD));
        scene.lat0 = options.pickLat - spanDeg / 2.0;
        scene.lat1 = options.pickLat + spanDeg / 2.0;
        scene.lon0 = options.pickLon - spanDeg / c / 2.0;
        scene.lon1 = options.pickLon + spanDeg / c / 2.0;
        scene.worldWidthKm = (float)options.spanKm;
        scene.worldHeightKm = (float)options.spanKm;
    }
    if (scene.window.elevationM.empty())
    {
        std::cerr << "DEM window extraction failed\n";
        return false;
    }
    scene.window.resolution = texRes;
    scene.worldScale = 200.0f /
        std::max(scene.worldWidthKm, scene.worldHeightKm);

    scene.heightTex = BuildHeightTexture(scene.window);
    scene.albedoTex = BuildAlbedoTexture(options, scene.lat0, scene.lat1,
                                         scene.lon0, scene.lon1);

#if defined(PLATFORM_WEB)
    bool es100 = true;
#else
    bool es100 = options.webShader;
#endif
    std::string fs = std::string(es100 ? FS_PREFIX_100 : FS_PREFIX_330)
                     + FS_BODY;
    scene.shader = LoadShaderFromMemory(es100 ? VS_100 : VS_330,
                                        fs.c_str());
    // Route raylib's material-map binding to our sampler names: the
    // diffuse slot carries the shading map, specular the height ramp,
    // normal the albedo.
    scene.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
        GetShaderLocation(scene.shader, "shadeMap");
    scene.shader.locs[SHADER_LOC_MAP_SPECULAR] =
        GetShaderLocation(scene.shader, "heightMap");
    scene.shader.locs[SHADER_LOC_MAP_NORMAL] =
        GetShaderLocation(scene.shader, "albedoMap");
    scene.locTexel = GetShaderLocation(scene.shader, "texelSize");
    scene.locSunDir = GetShaderLocation(scene.shader, "sunDirection");
    scene.locSunColor = GetShaderLocation(scene.shader, "sunColor");
    scene.locAmbient = GetShaderLocation(scene.shader, "ambient");
    scene.locStyle = GetShaderLocation(scene.shader, "styleFlag");
    scene.locCurve = GetShaderLocation(scene.shader, "curveStrength");

    BuildSceneGeometry(scene, options);
    return true;
}

static void ApplyShaderState(const TerrainScene& scene,
                             const MapOptions& options, int styleMode)
{
    float texel[2] = { 1.0f / scene.heightTex.width,
                       1.0f / scene.heightTex.height };
    Vector3 sun = SunDirection(options.sunAzimuthDeg,
                               options.sunElevationDeg);
    float sunColor[3] = { 1.0f, 0.97f, 0.92f };
    float styleFlag = (float)styleMode;
    SetShaderValue(scene.shader, scene.locTexel, texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(scene.shader, scene.locSunDir, &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(scene.shader, scene.locSunColor, sunColor,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(scene.shader, scene.locAmbient, &options.ambient,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene.shader, scene.locStyle, &styleFlag,
                   SHADER_UNIFORM_FLOAT);
    // Full rim emphasis at >= 100 km windows, none below 20 km.
    float curveStrength = scene.nearside
        ? 1.0f
        : Clamp((scene.worldWidthKm - 20.0f) / 80.0f, 0.0f, 1.0f);
    SetShaderValue(scene.shader, scene.locCurve, &curveStrength,
                   SHADER_UNIFORM_FLOAT);
}

// ---------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------

static Camera3D TopDownCamera(const TerrainScene& scene, float zoom)
{
    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, scene.ScaledW() * 2.0f, 0.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 0.0f, -1.0f };    // north up
    camera.projection = CAMERA_ORTHOGRAPHIC;
    camera.fovy = scene.ScaledH() / zoom;        // vertical world extent
    return camera;
}

static Camera3D TiltCamera(const TerrainScene& scene, float yawDeg,
                           float pitchDeg, float zoom)
{
    Camera3D camera = { 0 };
    float dist = scene.ScaledW() * 1.15f / zoom;
    float yaw = yawDeg * DEG2RAD;
    float pitch = pitchDeg * DEG2RAD;
    camera.position = Vector3{
        dist * cosf(pitch) * sinf(yaw),
        dist * sinf(pitch),
        dist * cosf(pitch) * cosf(yaw) };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.projection = CAMERA_PERSPECTIVE;
    camera.fovy = 45.0f;
    return camera;
}

// ---------------------------------------------------------------------------
// HUD: title, scale bar, elevation legend, touch buttons
// ---------------------------------------------------------------------------

static Color RampColor(float t)
{
    const Color stops[9] = {
        { 26, 13, 89, 255 }, { 26, 64, 191, 255 }, { 13, 140, 217, 255 },
        { 26, 166, 102, 255 }, { 115, 191, 51, 255 }, { 242, 217, 51, 255 },
        { 242, 128, 38, 255 }, { 217, 38, 26, 255 }, { 250, 245, 240, 255 } };
    t = Clamp(t, 0.0f, 1.0f) * 8.0f;
    int i = std::min((int)t, 7);
    float f = t - i;
    return Color{
        (unsigned char)(stops[i].r + (stops[i + 1].r - stops[i].r) * f),
        (unsigned char)(stops[i].g + (stops[i + 1].g - stops[i].g) * f),
        (unsigned char)(stops[i].b + (stops[i + 1].b - stops[i].b) * f),
        255 };
}

static void DrawHud(const TerrainScene& scene, const MapOptions& options,
                    int styleMode, int screenW, int screenH, float zoom)
{
    const Color ink = Color{ 235, 235, 235, 255 };
    const Color dim = Color{ 170, 170, 170, 255 };

    const char* title = scene.nearside
        ? "MOON - NEAR SIDE  |  LOLA LDEM_16 (real elevation)"
        : TextFormat("MOON  %.2f%c  %.2f%c  |  %.0f km window  |  LOLA LDEM_16",
                     std::fabs(scene.window.latDeg),
                     (scene.window.latDeg >= 0.0) ? 'N' : 'S',
                     std::fabs(scene.window.lonDeg),
                     (scene.window.lonDeg >= 0.0) ? 'E' : 'W',
                     options.spanKm);
    DrawText(title, 14, 12, 18, ink);
    DrawText(TextFormat("sun az %.0f  el %.0f   exag x%.1f   %s",
                        options.sunAzimuthDeg, options.sunElevationDeg,
                        options.exaggeration,
                        (styleMode == 1) ? "COLOR ELEVATION" : "SHADED RELIEF"),
             14, 34, 14, dim);
    if (scene.nearside && options.outPath.empty())
    {
        DrawText("click / tap the map to dive into a 200 km window",
                 14, 52, 14, dim);
    }

    // Scale bar (bottom left) — round to a tidy km length.
    float kmPerPx = scene.worldHeightKm / (screenH * zoom);
    float targetKm = kmPerPx * screenW * 0.2f;
    float niceKm = powf(10.0f, floorf(log10f(targetKm)));
    if (targetKm / niceKm >= 5.0f) niceKm *= 5.0f;
    else if (targetKm / niceKm >= 2.0f) niceKm *= 2.0f;
    int barPx = (int)(niceKm / kmPerPx);
    int bx = 14, by = screenH - 30;
    DrawRectangle(bx, by, barPx, 4, ink);
    DrawRectangle(bx, by - 4, 2, 12, ink);
    DrawRectangle(bx + barPx - 2, by - 4, 2, 12, ink);
    const char* barLabel = (niceKm >= 1.0f)
        ? TextFormat("%.0f km", niceKm)
        : TextFormat("%.0f m", niceKm * 1000.0f);
    DrawText(barLabel, bx + barPx + 8, by - 6, 14, ink);

    // Elevation legend (colour style): gradient strip, km labels.
    if (styleMode == 1)
    {
        int lw = 18, lh = 220;
        int lx = screenW - 74, ly = (screenH - lh) / 2;
        for (int i = 0; i < lh; i++)
        {
            float t = 1.0f - (float)i / (lh - 1);
            DrawRectangle(lx, ly + i, lw, 1, RampColor(t));
        }
        DrawRectangleLines(lx - 1, ly - 1, lw + 2, lh + 2, dim);
        DrawText(TextFormat("%+.1f km", scene.window.maxElevationM / 1000.0f),
                 lx + lw + 5, ly - 4, 13, ink);
        DrawText(TextFormat("%+.1f km", scene.window.minElevationM / 1000.0f),
                 lx + lw + 5, ly + lh - 8, 13, ink);
        DrawText("elev", lx - 2, ly + lh + 8, 13, dim);
    }
}

// Touch-friendly buttons along the bottom-right. Returns the index of
// the button released this frame, or -1. Also reports whether the
// pointer is currently over any button (so map clicks can ignore it).
static int DrawButtons(const std::vector<const char*>& labels,
                       int screenW, int screenH, bool* pointerOnUi)
{
    const int h = 34, pad = 10, gap = 8;
    int x = screenW - pad;
    int clicked = -1;
    Vector2 mouse = GetMousePosition();
    if (pointerOnUi) *pointerOnUi = false;
    for (int i = (int)labels.size() - 1; i >= 0; i--)
    {
        int w = MeasureText(labels[i], 15) + 22;
        x -= w;
        Rectangle r = { (float)x, (float)(screenH - pad - h),
                        (float)w, (float)h };
        bool over = CheckCollisionPointRec(mouse, r);
        if (over && pointerOnUi) *pointerOnUi = true;
        DrawRectangleRec(r, Color{ 25, 25, 30, 210 });
        DrawRectangleLinesEx(r, 1.0f,
                             over ? Color{ 230, 230, 230, 255 }
                                  : Color{ 120, 120, 130, 255 });
        DrawText(labels[i], (int)r.x + 11, (int)r.y + 9, 15,
                 Color{ 225, 225, 225, 255 });
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = i;
        x -= gap;
    }
    return clicked;
}

// Invert the top-down near-side map: screen pixel -> lat/lon.
static bool ScreenToLatLon(const TerrainScene& scene, float zoom,
                           int screenW, int screenH, Vector2 p,
                           double* latDeg, double* lonDeg)
{
    float fovy = scene.ScaledH() / zoom;                  // world units
    float worldPerPx = fovy / screenH;
    float wx = (p.x - screenW / 2.0f) * worldPerPx;       // east
    float wz = (p.y - screenH / 2.0f) * worldPerPx;       // south
    double xKm = wx / scene.worldScale;
    double zKm = wz / scene.worldScale;
    double kmPerDeg = LOLA_M_PER_DEG / 1000.0;
    double lon = xKm / kmPerDeg;                          // plate carrée
    double lat = -zKm / kmPerDeg;
    if (std::fabs(lat) > 89.0 || std::fabs(lon) > 90.0) return false;
    *latDeg = lat;
    *lonDeg = lon;
    return true;
}

// ---------------------------------------------------------------------------
// Rendering + interactive loop
// ---------------------------------------------------------------------------

static void DrawScene(TerrainScene& scene, const MapOptions& options,
                      int styleMode, const Camera3D& camera)
{
    ApplyShaderState(scene, options, styleMode);
    ClearBackground(BLACK);    // space: no atmosphere, no haze
    BeginMode3D(camera);
    DrawModel(scene.model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    EndMode3D();
}

struct AppState
{
    MapOptions options;
    LolaDem dem;
    TerrainScene scene;
    int styleMode = 0;
    bool tilt = false;
    float yawDeg = 180.0f;
    float pitchDeg = 52.0f;
    float zoom = 1.0f;
    float pendingExag = 2.0f;
    Vector2 pressPos = { 0.0f, 0.0f };
    bool pressOnUi = false;
};

static void UpdateFrame(void* arg)
{
    AppState& app = *(AppState*)arg;
    MapOptions& options = app.options;
    float dt = GetFrameTime();

    // Keyboard (desktop).
    if (IsKeyDown(KEY_LEFT)) options.sunAzimuthDeg -= 60.0f * dt;
    if (IsKeyDown(KEY_RIGHT)) options.sunAzimuthDeg += 60.0f * dt;
    if (IsKeyDown(KEY_UP))
        options.sunElevationDeg = Clamp(options.sunElevationDeg + 30.0f * dt,
                                        2.0f, 89.0f);
    if (IsKeyDown(KEY_DOWN))
        options.sunElevationDeg = Clamp(options.sunElevationDeg - 30.0f * dt,
                                        2.0f, 89.0f);
    if (IsKeyPressed(KEY_TAB)) app.styleMode = 1 - app.styleMode;
    if (IsKeyPressed(KEY_T)) app.tilt = !app.tilt;
    if (IsKeyDown(KEY_Q))
        app.pendingExag = Clamp(app.pendingExag - 2.0f * dt, 0.5f, 12.0f);
    if (IsKeyDown(KEY_E))
        app.pendingExag = Clamp(app.pendingExag + 2.0f * dt, 0.5f, 12.0f);

    app.zoom = Clamp(app.zoom * (1.0f + GetMouseWheelMove() * 0.1f),
                     0.25f, 12.0f);
    if (app.tilt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !app.pressOnUi)
    {
        Vector2 d = GetMouseDelta();
        app.yawDeg += d.x * 0.4f;
        app.pitchDeg = Clamp(app.pitchDeg - d.y * 0.3f, 10.0f, 88.0f);
    }

    // Deferred exaggeration rebuild (once keys/buttons settle).
    if (app.pendingExag != options.exaggeration &&
        !IsKeyDown(KEY_Q) && !IsKeyDown(KEY_E))
    {
        options.exaggeration = app.pendingExag;
        BuildSceneGeometry(app.scene, options);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        app.pressPos = GetMousePosition();
    }

    int screenW = GetScreenWidth(), screenH = GetScreenHeight();

    BeginDrawing();
    Camera3D camera = app.tilt
        ? TiltCamera(app.scene, app.yawDeg, app.pitchDeg, app.zoom)
        : TopDownCamera(app.scene, app.zoom);
    DrawScene(app.scene, options, app.styleMode, camera);
    DrawHud(app.scene, options, app.styleMode, screenW, screenH, app.zoom);

    std::vector<const char*> labels = {
        (app.styleMode == 1) ? "SHADED" : "COLOR",
        app.tilt ? "MAP" : "3D",
        "SUN +45",
        "EXAG -",
        "EXAG +",
    };
    bool hasBack = !app.scene.nearside;
    if (hasBack) labels.insert(labels.begin(), "BACK");
    bool pointerOnUi = false;
    int clicked = DrawButtons(labels, screenW, screenH, &pointerOnUi);
    app.pressOnUi = pointerOnUi;

#if !defined(PLATFORM_WEB)
    if (IsKeyPressed(KEY_S)) TakeScreenshot("lunar_map_shot.png");
#endif
    EndDrawing();

    int base = hasBack ? 1 : 0;
    if (clicked >= 0)
    {
        if (hasBack && clicked == 0)
        {
            options.nearside = true;
            BuildScene(options, app.dem, app.scene);
            app.zoom = 1.0f;
        }
        else if (clicked == base + 0) { app.styleMode = 1 - app.styleMode; }
        else if (clicked == base + 1) { app.tilt = !app.tilt; }
        else if (clicked == base + 2)
        {
            options.sunAzimuthDeg += 45.0f;
            if (options.sunAzimuthDeg >= 360.0f)
                options.sunAzimuthDeg -= 360.0f;
        }
        else if (clicked == base + 3)
        {
            app.pendingExag = Clamp(app.pendingExag - 0.5f, 0.5f, 12.0f);
            options.exaggeration = app.pendingExag;
            BuildSceneGeometry(app.scene, options);
        }
        else if (clicked == base + 4)
        {
            app.pendingExag = Clamp(app.pendingExag + 0.5f, 0.5f, 12.0f);
            options.exaggeration = app.pendingExag;
            BuildSceneGeometry(app.scene, options);
        }
    }
    else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !pointerOnUi &&
             app.scene.nearside && !app.tilt)
    {
        // A short click (not a drag) on the near-side map dives into a
        // regional window at the clicked coordinates.
        Vector2 p = GetMousePosition();
        if (Vector2Distance(p, app.pressPos) < 8.0f)
        {
            double lat, lon;
            if (ScreenToLatLon(app.scene, app.zoom, screenW, screenH, p,
                               &lat, &lon))
            {
                options.nearside = false;
                options.pickLat = lat;
                options.pickLon = lon;
                options.spanKm = 200.0;
                BuildScene(options, app.dem, app.scene);
                app.zoom = 1.0f;
            }
        }
    }
}

int main(int argc, char** argv)
{
    static AppState app;
    if (!ParseArgs(argc, argv, app.options)) return 1;
    bool headless = !app.options.outPath.empty();

#if defined(PLATFORM_WEB)
    app.options.width = 960;
    app.options.height = 960;
#endif

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(app.options.width, app.options.height,
               "lunar_map - LOLA elevation");

    if (app.options.interp == "bspline")
        LolaSetInterpolation(LolaInterp::BSPLINE);
    else if (app.options.interp == "lanczos")
        LolaSetInterpolation(LolaInterp::LANCZOS);
    else if (app.options.interp == "fractal")
        LolaSetInterpolation(LolaInterp::FRACTAL);
    else
        LolaSetInterpolation(LolaInterp::CATROM);

    if (!app.dem.Load(app.options.demPath))
    {
        CloseWindow();
        return 1;
    }
    // High-resolution SLDEM2015 crops (fetched by the fetch-dem
    // workflow) refine any window that lands on them.
    {
        std::string lolaDir = app.options.demPath.substr(
            0, app.options.demPath.find_last_of("/\\"));
        int n = app.dem.LoadOverlays(lolaDir);
        if (n > 0) std::cerr << "lunar_map: " << n
                             << " high-res overlay(s) active\n";
    }
    if (!BuildScene(app.options, app.dem, app.scene))
    {
        CloseWindow();
        return 1;
    }
    app.styleMode = (app.options.style == "color") ? 1 : 0;
    app.tilt = app.options.tilt;
    app.yawDeg = app.options.orbitYawDeg;
    app.pitchDeg = app.options.orbitPitchDeg;
    app.pendingExag = app.options.exaggeration;
    std::cerr << TextFormat(
        "lunar_map: window %.1f..%.1f km elevation, %d px shading texture\n",
        app.scene.window.minElevationM / 1000.0f,
        app.scene.window.maxElevationM / 1000.0f,
        app.scene.shadeTex.width);

    if (headless)
    {
        RenderTexture2D target = LoadRenderTexture(app.options.width,
                                                   app.options.height);
        BeginTextureMode(target);
        Camera3D camera = app.tilt
            ? TiltCamera(app.scene, app.yawDeg, app.pitchDeg, app.zoom)
            : TopDownCamera(app.scene, app.zoom);
        DrawScene(app.scene, app.options, app.styleMode, camera);
        DrawHud(app.scene, app.options, app.styleMode, app.options.width,
                app.options.height, app.zoom);
        EndTextureMode();

        Image shot = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&shot);
        ExportImage(shot, app.options.outPath.c_str());
        std::cerr << "lunar_map: wrote " << app.options.outPath << "\n";
        UnloadImage(shot);
        UnloadRenderTexture(target);
    }
    else
    {
#if defined(PLATFORM_WEB)
        emscripten_set_main_loop_arg(UpdateFrame, &app, 0, 1);
#else
        SetTargetFPS(60);
        while (!WindowShouldClose())
        {
            UpdateFrame(&app);
        }
#endif
    }

    UnloadSceneGpu(app.scene);
    CloseWindow();
    return 0;
}
