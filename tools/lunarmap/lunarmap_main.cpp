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
#include "survey_cursor.h"
#include "survey_hints.h"

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

struct DemoSite
{
    const char* key;
    double pickLat, pickLon;       // level-1 click
    double aimDx, aimDy;           // km east/north of pick the player aims at
    float sunElDeg;                // display sun: grazing at the poles
    double altHoverLat, altHoverLon;   // extra L1 frame hovering elsewhere
                                       // (9e9 = none): shows the highlight
                                       // tracking the mouse, not the target
    // Which region-card row the L1 screenshot shows hovered, with its
    // hint open ("titanium" | "rock" | "psr" | nullptr). In the game the
    // hint appears only while the cursor rests on the row.
    const char* hintKey;
    float psrDistanceKm;               // for the "psr" hint
    // region card (fixed from level 1)
    const char* regionName;
    const char* terrane;
    const char* archetype;
    Color archetypeTint;
    const char* rock;
    float fePct, tiPct, thPpm;     // real values where the data has them
    const char* latitudeNote;
    // one annotation per level: decision, consequence
    const char* note[5][2];
};

static const DemoSite DEMO_SITES[] =
{
    { "imbrium", 32.8, -15.6, 13.0, 13.0, 30.0f, 28.0, 17.5, "titanium", 9e9f,
      "Mare Imbrium", "Procellarum KREEP Terrane", "MARE INDUSTRIAL",
      Color{ 224, 168, 108, 255 },
      "mare basalt", 14.0f, 2.5f, 8.0f,
      "33 N - 14-day nights, strong Earth comms",
      {
        { "Claim Mare Imbrium: PKT mare, Fe 14% / Ti 2.5% / Th 8 ppm, flat basalt.",
          "Metals-and-comms economy. Aluminium must be imported; every site here sleeps 14 days a month." },
        { "Position the playfield deep in the mare interior - no highland shore in reach.",
          "Pure industrial play: maximum flat ground, zero local access to Al/Ca rock." },
        { "Pick a neighbourhood on smooth plain, clear of wrinkle ridges.",
          "Nearly every cell is buildable, so expansion is unconstrained - the choice is cheap here, by design." },
        { "Anchor cell with all eight neighbours flat.",
          "Roads and later sects can go any direction. No terrain tax on growth." },
        { "Footprint on level ground - the verdict is green almost anywhere.",
          "Build allowed. What is UNDER this exact spot - hydrogen, regolith depth - stays unknown until prospected." },
      } },

    { "apennine", 26.10, 3.60, -3.0, 1.0, 24.0f, 9e9, 9e9, "rock", 9e9f,
      "Palus Putredinis", "PKT - Apennine boundary", "MIXED (SHORE)",
      Color{ 150, 200, 150, 255 },
      "mare basalt / highland breccia", 12.0f, 1.8f, 5.0f,
      "26 N - mare meets the Apennine front",
      {
        { "Claim the Imbrium rim where the mare laps the Apennine front (Apollo 15 country).",
          "Both rock types in one playfield: no imports - but nothing at top grade, and mountains eat buildable ground." },
        { "Anchor the playfield ON the shore, not the interior. Same region, different game.",
          "This is the MIXED economy chosen by position alone - the level-2 decision at its clearest." },
        { "Neighbourhood on the mare side with the front in trucking reach.",
          "Flat plain for sects, highland material a short haul east - and slopes cap expansion that way." },
        { "A cell on the plain, rougher ground southward where the front rises.",
          "Expansion prefers north and west - the neighbour glyph shows which sides a road should leave." },
        { "Footprint on the open plain, the Apennine front one neighbourhood south.",
          "Green here - and the closer to the front you push, the sooner the verdict flips." },
      } },

    { "shackleton", -89.7, 110.0, 0.0, 10.5, 4.0f, 9e9, 9e9, "psr", 4.0f,
      "Shackleton rim", "Feldspathic Highlands (polar)", "POLAR VOLATILE",
      Color{ 140, 190, 235, 255 },
      "anorthosite breccia", 5.0f, 0.4f, 1.0f,
      "89 S - PSR floors + near-constant crest sun",
      {
        { "Claim the south polar rim: poor metals, brutal ground - and the only water on the Moon.",
          "The whole economy inverts: sunlight and ice replace iron. Earth comms are marginal at best." },
        { "Playfield straddling the crater rim: permanently shadowed floor and lit crest in one field.",
          "POLAR VOLATILE play. Nearly all other ground in the field is unbuildable slope." },
        { "Neighbourhood along the rim crest, the ice one ridge away.",
          "Short haul to the PSR - but the buildable strip is thin, so growth will be a line, not a disc." },
        { "The one workable crest cell; neighbours fall away into shadow.",
          "Single-file expansion. Every road leaves along the ridge or not at all." },
        { "Footprint on the lit crest, metres from permanent shadow.",
          "Sun for power, ice next door: the knife-edge this strategy exists for. (PSR flag from 1.9 km/px data - coarse until a polar SLDEM crop lands.)" },
      } },
};
static const int DEMO_SITE_COUNT = (int)(sizeof(DEMO_SITES) / sizeof(DEMO_SITES[0]));

// The five questions, one per level (docs/design/site-selection SS2).
static const char* LEVEL_QUESTION[5] =
{
    "WHICH ECONOMY?", "WHICH MIX?", "WHICH NEIGHBOURHOOD?",
    "WHICH CELL?", "WHICH GROUND?"
};
static const char* LEVEL_FILE[5] =
{
    "L1_ECONOMY", "L2_MIX", "L3_NEIGHBOURHOOD", "L4_CELL", "L5_GROUND"
};


struct MapOptions
{
    bool nearside = true;          // full near side unless --pick given
    double pickLat = 0.0;
    double pickLon = 0.0;
    double spanKm = 200.0;         // regional window size (square)
    std::string style = "shaded";  // shaded | color
    float sunAzimuthDeg = 315.0f;  // clockwise from north (NW default)
    float sunElevationDeg = 30.0f;
    // True scale. 2.0 was inflating broad landforms into swollen
    // mounds — doubling every slope before the normals are built
    // exaggerates exactly the low-frequency shapes, which reads as
    // "melted". Raise it deliberately for map-scale legibility.
    float exaggeration = 1.0f;     // vertical scale multiplier
    float detail = 1.0f;           // sub-floor synthesis strength (0 = off)
    std::string interp = "catrom"; // catrom | bspline | lanczos | fractal
    std::string texture = "noise"; // noise | craters
    bool despeckle = false;        // --despeckle to enable
    int demDecim = 1;              // --demdecim N: coarsen overlays
    bool survey = false;           // --survey: site report, no render
    // Placement cursor: the footprint the player is about to commit to.
    // Default 1.5 km ~ the sect core (terrain_synthesis coreRadiusKm),
    // the part that actually has to sit on good ground.
    bool place = false;
    double placeDxKm = 0.0;        // cursor offset east of window centre
    double placeDyKm = 0.0;        // cursor offset north of window centre
    double footprintKm = 1.5;
    // Survey ladder: walk the descent (500 km -> 5 km window) with the
    // cursor aimed at one target, one PNG per level.
    bool ladder = false;
    int demo = -1;                 // --demo NAME: annotated ladder walk
    int maxLevel = 5;              // --maxlevel N: stop the demo after Ln
    // Data-layer test: -1 none, else an index into INSTRUMENTS. --truth
    // renders the field at full resolution instead of on the
    // instrument's measurement grid -- the "what is actually there"
    // control image.
    int layer = -1;
    bool truth = false;
    int layerAlpha = 145;
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
        << "  --exag F          vertical exaggeration   (default: 1.0)\n"
        << "  --detail F        sub-floor synthesis     (default: 1.0, 0=off)\n"
        << "  --texture NAME    noise | craters         (default: noise)\n"
        << "  --despeckle       apply the 3x3 median to overlay crops\n"
        << "  --demdecim N      coarsen overlays Nx (1=59m, 4=237m)\n"
        << "  --survey          print a buildability report, no render\n"
        << "  --place DX,DY     placement cursor, km east/north of centre\n"
        << "  --footprint KM    cursor footprint size    (default: 1.5)\n"
        << "  --ladder          walk the survey descent, one PNG per level\n"
        << "  --demo NAME       annotated descent: imbrium|apennine|shackleton\n"
        << "  --layer N         data layer 0=hydrogen 1=iron 2=rock abundance\n"
        << "  --truth           draw the layer at full resolution, not its grid\n"
        << "  --layeralpha N    layer opacity 0-255      (default: 145)\n"
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
        else if (arg == "--texture" && hasNext) { options.texture = argv[++i]; }
        else if (arg == "--nodespeckle") { options.despeckle = false; }
        else if (arg == "--despeckle") { options.despeckle = true; }
        else if (arg == "--survey") { options.survey = true; }
        else if (arg == "--place" && hasNext)
        {
            options.place = true;
            if (std::sscanf(argv[++i], "%lf,%lf",
                            &options.placeDxKm, &options.placeDyKm) != 2)
            {
                std::cerr << "Bad --place, expected DX,DY in km\n";
                return false;
            }
        }
        else if (arg == "--footprint" && hasNext) { options.footprintKm = std::atof(argv[++i]); }
        else if (arg == "--ladder") { options.ladder = true; }
        else if (arg == "--maxlevel" && hasNext) { options.maxLevel = std::atoi(argv[++i]); }
        else if (arg == "--demo" && hasNext)
        {
            std::string name = argv[++i];
            for (int d = 0; d < DEMO_SITE_COUNT; d++)
            {
                if (name == DEMO_SITES[d].key) options.demo = d;
            }
            if (options.demo < 0)
            {
                std::cerr << "Unknown --demo site: " << name << "\n";
                return false;
            }
            options.ladder = true;
            options.nearside = false;
            options.pickLat = DEMO_SITES[options.demo].pickLat;
            options.pickLon = DEMO_SITES[options.demo].pickLon;
            options.place = true;
            options.placeDxKm = DEMO_SITES[options.demo].aimDx;
            options.placeDyKm = DEMO_SITES[options.demo].aimDy;
            options.sunElevationDeg = DEMO_SITES[options.demo].sunElDeg;
        }
        else if (arg == "--layer" && hasNext) { options.layer = std::atoi(argv[++i]); }
        else if (arg == "--truth") { options.truth = true; }
        else if (arg == "--layeralpha" && hasNext) { options.layerAlpha = std::atoi(argv[++i]); }
        else if (arg == "--demdecim" && hasNext) { options.demDecim = std::atoi(argv[++i]); }
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
uniform float albedoStrength;  // WAC contribution (map scales only)

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

    vec4 shade = TEX(shadeMap, uv - 0.5 * texelSize);
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
    // The WAC mosaic is 1.33 km/px: below ~20 km windows it supplies
    // fewer than 15 texels across the frame, so stretching it paints
    // huge soft tonal blobs unrelated to the ground — the single
    // biggest source of the "expressionist" wash. Faded out close in.
    vec3 albedo = TEX(albedoMap, uv).rgb;
    float gray = dot(albedo, vec3(0.299, 0.587, 0.114));
    // WAC brightness already encodes its own sun; use it gently.
    vec3 surface = mix(vec3(0.62), mix(vec3(gray), albedo, 0.55),
                       0.75 * albedoStrength);
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
    double nativeKm = 0.0;         // finest data actually feeding this window
    float worldWidthKm = 0.0f;     // east-west extent (real km)
    float worldHeightKm = 0.0f;    // north-south extent (real km)
    float worldScale = 1.0f;       // world units per km
    double lat0 = 0.0, lat1 = 0.0, lon0 = 0.0, lon1 = 0.0;
    int locTexel = 0, locSunDir = 0, locSunColor = 0;
    int locAmbient = 0, locStyle = 0, locCurve = 0, locAlbedoStr = 0;

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
            // Half-offset quad difference, NOT a central difference:
            // (h[x+1]-h[x-1])/2 has MTF sinc(2f), which is exactly ZERO
            // at the 2-texel wavelength — the finest synthesized octave
            // was being deleted on arrival. The quad gradient sits at
            // the texel corner (MTF 0.637 there) and the shader
            // compensates with a half-texel lookup offset.
            int xq = std::min(res - 1, x + 1);
            int yq = std::min(res - 1, y + 1);
            float h00 = e[(size_t)y * res + x];
            float h10 = e[(size_t)y * res + xq];
            float h01 = e[(size_t)yq * res + x];
            float h11 = e[(size_t)yq * res + xq];
            float gx = ((h10 + h11) - (h00 + h01)) * 0.5f / dxM *
                       exaggeration;
            float gy = ((h01 + h11) - (h00 + h10)) * 0.5f / dyM *
                       exaggeration;
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
        // The shading texture must OUT-resolve the frame: at 1024 the
        // GPU stretched it ~1.2x onto a 1200 px screen and the normal
        // differencing low-passed another 2x — a permanent soft-focus
        // no interpolant could beat. 2048 puts both below one screen
        // pixel.
        int res = (options.demRes > 0) ? options.demRes : 2048;
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
    scene.nativeKm = options.nearside
        ? LOLA_M_PER_DEG / (dem.Width() / 360.0) / 1000.0
        : dem.NativeKmAt(options.pickLat, options.pickLon);
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
    scene.locAlbedoStr = GetShaderLocation(scene.shader, "albedoStrength");

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
    // WAC carries real regional albedo only while many of its 1.33 km
    // texels span the frame: full at >= 100 km, gone by 20 km.
    float albedoStrength = scene.nearside
        ? 1.0f
        : Clamp((scene.worldWidthKm - 20.0f) / 80.0f, 0.0f, 1.0f);
    SetShaderValue(scene.shader, scene.locAlbedoStr, &albedoStrength,
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

    // Name the dataset actually feeding THIS window, not a fixed
    // string: a regional pick on an SLDEM2015 crop is 59 m data, and
    // labelling it LDEM_16 (1.9 km) misreports the tool's own input.
    const char* source = (scene.nativeKm < 0.5)
        ? TextFormat("SLDEM2015 %.0f m", scene.nativeKm * 1000.0)
        : TextFormat("LOLA LDEM_16 %.1f km", scene.nativeKm);
    const char* title = scene.nearside
        ? TextFormat("MOON - NEAR SIDE  |  %s (real elevation)", source)
        : TextFormat("MOON  %.2f%c  %.2f%c  |  %.0f km window  |  %s",
                     std::fabs(scene.window.latDeg),
                     (scene.window.latDeg >= 0.0) ? 'N' : 'S',
                     std::fabs(scene.window.lonDeg),
                     (scene.window.lonDeg >= 0.0) ? 'E' : 'W',
                     options.spanKm, source);
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

// ---------------------------------------------------------------------------
// Placement cursor
//
// The footprint the player is about to commit to, judged against the
// real DEM underneath it and drawn green or red. The readout names the
// specific limit that failed rather than just refusing.
// ---------------------------------------------------------------------------

struct PlacementVerdict
{
    bool allowed = false;
    const char* reason = "";
};

static PlacementVerdict JudgeSite(const TerrainBuildability& b)
{
    PlacementVerdict v;
    if (b.meanSlopeDeg > 8.0f) { v.reason = "TOO STEEP (mean slope)"; return v; }
    if (b.maxSlopeDeg > 25.0f) { v.reason = "TOO STEEP (local face)"; return v; }
    if (b.roughnessM > 40.0f)  { v.reason = "GROUND TOO BROKEN";      return v; }
    if (b.reliefM > 400.0f)    { v.reason = "RELIEF TOO GREAT";       return v; }
    if (b.isPsr)               { v.reason = "PERMANENT SHADOW";       return v; }
    v.allowed = true;
    v.reason = "SITE OK - BUILD ALLOWED";
    return v;
}

static void DrawPlacementCursor(const MapOptions& options, const LolaDem& dem,
                                int screenW, int screenH)
{
    const double kmPerDeg = LOLA_M_PER_DEG / 1000.0;
    double lat = options.pickLat + options.placeDyKm / kmPerDeg;
    double cosLat = std::max(0.05, std::cos(options.pickLat * DEG2RAD));
    double lon = options.pickLon + options.placeDxKm / (kmPerDeg * cosLat);

    TerrainBuildability b = dem.EvaluateSite(lat, lon, options.footprintKm,
                                             30.0);
    PlacementVerdict v = JudgeSite(b);
    Color tint = v.allowed ? Color{ 60, 235, 120, 255 }
                           : Color{ 255, 70, 70, 255 };

    // World km -> screen: the top-down camera maps the whole span onto
    // the frame, north up.
    float pxPerKm = screenH / (float)options.spanKm;
    float cx = screenW * 0.5f + (float)options.placeDxKm * pxPerKm;
    float cy = screenH * 0.5f - (float)options.placeDyKm * pxPerKm;
    float half = (float)(options.footprintKm * 0.5) * pxPerKm;

    Rectangle r = { cx - half, cy - half, half * 2.0f, half * 2.0f };
    DrawRectangleRec(r, Color{ tint.r, tint.g, tint.b, 38 });
    DrawRectangleLinesEx(r, 3.0f, tint);
    float t = half * 0.35f;
    for (int i = 0; i < 4; i++)
    {
        float ox = (i & 1) ? r.x + r.width : r.x;
        float oy = (i & 2) ? r.y + r.height : r.y;
        float sx = (i & 1) ? -1.0f : 1.0f;
        float sy = (i & 2) ? -1.0f : 1.0f;
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox + sx * t, oy }, 5.0f, tint);
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox, oy + sy * t }, 5.0f, tint);
    }
    DrawCircleV(Vector2{ cx, cy }, 3.0f, tint);

    // Keep the readout on the opposite side to the cursor, so the panel
    // never covers the footprint it is describing.
    int pw = 348, ph = 138;
    int px = (cx < screenW * 0.5f) ? screenW - pw - 16 : 16;
    int py = 78;
    Color dim = Color{ 210, 210, 210, 255 };
    DrawRectangle(px, py, pw, ph, Color{ 12, 12, 16, 215 });
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw,
                                    (float)ph }, 2.0f, tint);
    DrawText(v.reason, px + 12, py + 10, 19, tint);
    DrawText(TextFormat("footprint    %.1f km", options.footprintKm),
             px + 12, py + 38, 15, dim);
    DrawText(TextFormat("slope mean   %.2f deg  (max 8)", b.meanSlopeDeg),
             px + 12, py + 58, 15, b.meanSlopeDeg > 8.0f ? tint : dim);
    DrawText(TextFormat("slope peak   %.2f deg  (max 25)", b.maxSlopeDeg),
             px + 12, py + 77, 15, b.maxSlopeDeg > 25.0f ? tint : dim);
    DrawText(TextFormat("roughness    %.1f m    (max 40)", b.roughnessM),
             px + 12, py + 96, 15, b.roughnessM > 40.0f ? tint : dim);
    DrawText(TextFormat("relief       %.0f m    (max 400)", b.reliefM),
             px + 12, py + 115, 15, b.reliefM > 400.0f ? tint : dim);
}

// ---------------------------------------------------------------------------
// Data layers and their instruments
//
// The test behind docs/design/site-selection §4.3: a quantity is only as
// sharp as the instrument that measured it, and zooming is a camera move,
// not an instrument change. Each layer is drawn on ITS OWN measurement
// grid, so terrain keeps resolving while the neutron layer turns into
// 45 km blocks and finally into one flat block filling the frame.
//
// The field itself is synthetic -- the real composition maps are not in
// this repo -- but that is the point of the test: it lets the sub-floor
// structure be dialled deliberately, which is the open question the
// design flags (does the generator put anything below the floor?).
// ---------------------------------------------------------------------------

struct SurveyInstrument
{
    const char* name;
    const char* quantity;
    const char* unit;
    double footprintKm;
    float lo, hi;           // display range
};

// Footprints are order-of-magnitude, as the design says: a neutron
// spectrometer sees tens of km, a gamma-ray spectrometer not much better,
// a thermal radiometer a couple of hundred metres.
static const SurveyInstrument INSTRUMENTS[3] =
{
    { "NEUTRON", "hydrogen",       "ppm",  45.0,   0.0f, 500.0f },
    { "GAMMA",   "iron",           "wt%",  30.0,   3.0f,  22.0f },
    { "DIVINER", "rock abundance", "%",     0.2,   0.0f,  12.0f },
};
static const int LAYER_COUNT = 3;

static float LayerHash(int x, int y, int seed)
{
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + seed * 1442695040888963407ull);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFFFF) / (float)0xFFFFFF;
}

// Value noise on a km grid, smoothstep-interpolated.
static float LayerNoise(double xKm, double yKm, double cellKm, int seed)
{
    double gx = xKm / cellKm, gy = yKm / cellKm;
    int x0 = (int)std::floor(gx), y0 = (int)std::floor(gy);
    float fx = (float)(gx - x0), fy = (float)(gy - y0);
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = LayerHash(x0, y0, seed), b = LayerHash(x0 + 1, y0, seed);
    float c = LayerHash(x0, y0 + 1, seed), d = LayerHash(x0 + 1, y0 + 1, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// Global km coordinates, so the field and its measurement grid are
// anchored to the moon rather than to the window.
static void LayerGlobalKm(double latDeg, double lonDeg, double* xKm, double* yKm)
{
    double cosLat = std::cos(latDeg * DEG2RAD);
    if (cosLat < 0.05) cosLat = 0.05;
    *xKm = lonDeg * 30.32268 * cosLat;
    *yKm = latDeg * 30.32268;
}

// 0..1. The spectra differ on purpose:
//   hydrogen -- most of its power at 4-15 km, well under the 45 km
//               footprint, and made patchy because cold traps are sparse
//   iron     -- basin-scale, so a 30 km footprint captures it nearly
//               perfectly and the band stays narrow
//   rock     -- fine, and its instrument is fine too: fully resolvable
static float CompositionField01(int layer, double latDeg, double lonDeg)
{
    double x, y;
    LayerGlobalKm(latDeg, lonDeg, &x, &y);
    float v;
    if (layer == 0)
    {
        v = 0.30f * LayerNoise(x, y, 300.0, 11)
          + 0.18f * LayerNoise(x, y,  60.0, 12)
          + 0.28f * LayerNoise(x, y,  13.0, 13)
          + 0.24f * LayerNoise(x, y,   4.5, 14);
        v = powf(Clamp(v, 0.0f, 1.0f), 2.3f);   // sparse, high-contrast
    }
    else if (layer == 1)
    {
        v = 0.55f * LayerNoise(x, y, 420.0, 21)
          + 0.31f * LayerNoise(x, y, 130.0, 22)
          + 0.14f * LayerNoise(x, y,  34.0, 23);
    }
    else
    {
        v = 0.45f * LayerNoise(x, y, 40.0, 31)
          + 0.33f * LayerNoise(x, y,  2.0, 32)
          + 0.22f * LayerNoise(x, y,  0.4, 33);
        v = powf(Clamp(v, 0.0f, 1.0f), 1.7f);
    }
    return Clamp(v, 0.0f, 1.0f);
}

// Mean of the field over a square window, sampled on a grid.
static float FieldMean(int layer, double latDeg, double lonDeg,
                       double sizeKm, int samples)
{
    double cosLat = std::cos(latDeg * DEG2RAD);
    if (cosLat < 0.05) cosLat = 0.05;
    double sum = 0.0;
    for (int j = 0; j < samples; j++)
    {
        for (int i = 0; i < samples; i++)
        {
            double dx = ((i + 0.5) / samples - 0.5) * sizeKm;
            double dy = ((j + 0.5) / samples - 0.5) * sizeKm;
            sum += CompositionField01(layer, latDeg + dy / 30.32268,
                                      lonDeg + dx / (30.32268 * cosLat));
        }
    }
    return (float)(sum / (samples * samples));
}

// The band the design asks for: the standard deviation of CURSOR-SIZED
// patches inside the INSTRUMENT footprint. It widens on its own as the
// cursor shrinks, because the player is asking a sharper question of the
// same measurement -- no invented noise anywhere.
static float FieldBand(int layer, double latDeg, double lonDeg,
                       double footprintKm, double cursorKm)
{
    // One rule, both directions. The band is the spread of the SMALLER
    // of (cursor, instrument footprint) sampled across the LARGER:
    //
    //   cursor < footprint -> patches of cursor size across the
    //       footprint. Widens as the cursor shrinks: the player is
    //       asking a sharper question of the same measurement.
    //   cursor > footprint -> patches of footprint size across the
    //       cursor. Narrows as the cursor shrinks: the instrument
    //       resolves the ground and the question is closing in on it.
    //
    // Terrain rows therefore close while resource rows open, with no
    // special case and nothing injected.
    double patchKm = (cursorKm < footprintKm) ? cursorKm : footprintKm;
    double spreadKm = (cursorKm < footprintKm) ? footprintKm : cursorKm;
    if (patchKm <= 0.0 || spreadKm <= patchKm) return 0.0f;

    const int patches = 9;
    double cosLat = std::cos(latDeg * DEG2RAD);
    if (cosLat < 0.05) cosLat = 0.05;

    double sum = 0.0, sum2 = 0.0;
    int n = 0;
    for (int j = 0; j < patches; j++)
    {
        for (int i = 0; i < patches; i++)
        {
            double dx = ((i + 0.5) / patches - 0.5) * (spreadKm - patchKm);
            double dy = ((j + 0.5) / patches - 0.5) * (spreadKm - patchKm);
            float m = FieldMean(layer, latDeg + dy / 30.32268,
                                lonDeg + dx / (30.32268 * cosLat),
                                patchKm, 3);
            sum += m; sum2 += (double)m * m; n++;
        }
    }
    if (n < 2) return 0.0f;
    double mean = sum / n;
    double var = sum2 / n - mean * mean;
    return (float)std::sqrt(var > 0.0 ? var : 0.0);
}

static Color LayerColor(int layer, float v01)
{
    v01 = Clamp(v01, 0.0f, 1.0f);
    if (layer == 0)          // hydrogen: cold blue -> cyan -> white
    {
        // Stretched to the DATA's range, not to the absolute scale --
        // the field sits around 0.1-0.3 because cold traps are sparse, so
        // a 0..1 ramp puts the whole map in its dark end where
        // neighbouring cells are indistinguishable. Standard practice for
        // any data viewer, and it changes only the colour, not the value.
        float t = powf(Clamp(v01 * 2.6f, 0.0f, 1.0f), 0.78f);
        return Color{ (unsigned char)(20 + 215 * t * t),
                      (unsigned char)(45 + 195 * t),
                      (unsigned char)(95 + 160 * powf(t, 0.6f)), 255 };
    }
    if (layer == 1)          // iron: dark -> orange
        return Color{ (unsigned char)(40 + 205 * powf(v01, 0.7f)),
                      (unsigned char)(30 + 130 * v01),
                      (unsigned char)(35 + 30 * v01), 255 };
    return Color{ (unsigned char)(60 + 190 * v01),      // rock: grey -> yellow
                  (unsigned char)(60 + 175 * v01),
                  (unsigned char)(70 + 25 * v01), 255 };
}

// Draw one layer on its own measurement grid, anchored globally so the
// blocks belong to the moon and do not swim when the window moves.
// gridKm <= 0 renders the field at full resolution -- the "what is
// actually there" control image.
static void DrawDataLayer(int layer, double gridKm, const SurveyCursor& cursor,
                          const SurveyViewport& viewport, unsigned char alpha)
{
    bool truth = (gridKm <= 0.0);
    if (truth) gridKm = cursor.windowSpanKm / 200.0;

    double cx, cy;
    LayerGlobalKm(cursor.windowLatDeg, cursor.windowLonDeg, &cx, &cy);
    double cosLat = std::cos(cursor.windowLatDeg * DEG2RAD);
    if (cosLat < 0.05) cosLat = 0.05;

    double half = cursor.windowSpanKm * 0.5;
    long i0 = (long)std::floor((cx - half) / gridKm);
    long i1 = (long)std::floor((cx + half) / gridKm);
    long j0 = (long)std::floor((cy - half) / gridKm);
    long j1 = (long)std::floor((cy + half) / gridKm);

    float pxPerKm = SurveyPixelsPerKm(viewport, cursor.windowSpanKm);
    float cellPx = (float)gridKm * pxPerKm;

    for (long j = j0; j <= j1; j++)
    {
        for (long i = i0; i <= i1; i++)
        {
            double gxc = (i + 0.5) * gridKm;
            double gyc = (j + 0.5) * gridKm;
            double lat = gyc / 30.32268;
            double lon = gxc / (30.32268 * cosLat);
            // The measured value of a cell is the field averaged over the
            // instrument's footprint, not a point sample: that averaging
            // IS the resolution limit.
            float v = truth ? CompositionField01(layer, lat, lon)
                            : FieldMean(layer, lat, lon, gridKm, 5);
            float sx, sy;
            SurveyOffsetKmToScreen(viewport, cursor.windowSpanKm,
                                   gxc - cx, gyc - cy, &sx, &sy);
            Color c = LayerColor(layer, v);
            c.a = alpha;
            DrawRectangleRec(Rectangle{ sx - cellPx * 0.5f - 0.5f,
                                        sy - cellPx * 0.5f - 0.5f,
                                        cellPx + 1.0f, cellPx + 1.0f }, c);
        }
    }
}

// The instrument footprint as a ring around the cursor -- drawn only
// where it is LARGER than the cursor, which is exactly when the number
// has stopped sharpening. Seeing a 45 km ring around a 1.5 km base is
// the whole argument in one glance.
static void DrawFootprintRing(double footprintKm, const SurveyCursor& cursor,
                              const SurveyViewport& viewport,
                              const char* instrument, Color tint)
{
    if (footprintKm <= cursor.footprintKm) return;
    float pxPerKm = SurveyPixelsPerKm(viewport, cursor.windowSpanKm);
    Rectangle r = SurveyCursorRect(cursor, viewport);
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
    float radius = (float)(footprintKm * 0.5) * pxPerKm;

    // Past a couple of levels the footprint is wider than the whole
    // view, so the ring falls off the frame -- and its absence would
    // read as "no limit here", the opposite of the truth. Say it on the
    // frame edge instead.
    if (footprintKm > cursor.windowSpanKm)
    {
        Rectangle f = { viewport.x + 5.0f, viewport.y + 5.0f,
                        viewport.width - 10.0f, viewport.height - 10.0f };
        float dash = 16.0f;
        for (float x = f.x; x < f.x + f.width; x += dash * 2.0f)
        {
            float w = fminf(dash, f.x + f.width - x);
            DrawRectangleRec(Rectangle{ x, f.y, w, 3.0f }, tint);
            DrawRectangleRec(Rectangle{ x, f.y + f.height - 3.0f, w, 3.0f }, tint);
        }
        for (float y = f.y; y < f.y + f.height; y += dash * 2.0f)
        {
            float h = fminf(dash, f.y + f.height - y);
            DrawRectangleRec(Rectangle{ f.x, y, 3.0f, h }, tint);
            DrawRectangleRec(Rectangle{ f.x + f.width - 3.0f, y, 3.0f, h }, tint);
        }
        const char* msg = TextFormat("%s FOOTPRINT %.3g km  -  WIDER THAN THIS VIEW",
                                     instrument, footprintKm);
        int tw = MeasureText(msg, 15);
        float bx = f.x + (f.width - tw) * 0.5f - 10.0f;
        float by = f.y + f.height - 34.0f;
        DrawRectangle((int)bx, (int)by, tw + 20, 24, Color{ 12, 12, 16, 215 });
        DrawText(msg, (int)bx + 10, (int)by + 5, 15, tint);
        return;
    }

    for (int k = 0; k < 96; k++)
    {
        float a0 = (float)k / 96.0f * 2.0f * PI;
        float a1 = (float)(k + 1) / 96.0f * 2.0f * PI;
        if ((k % 3) == 2) continue;      // dashed
        DrawLineEx(Vector2{ cx + cosf(a0) * radius, cy + sinf(a0) * radius },
                   Vector2{ cx + cosf(a1) * radius, cy + sinf(a1) * radius },
                   2.5f, tint);
    }
}

// ---------------------------------------------------------------------------
// The survey cursor (src/TerrainGen/survey_cursor.*)
//
// At every zoom the cursor is the footprint of the level BELOW, so it is
// always "the thing you are about to enter". Levels 1-4 are navigation
// and draw neutral -- nothing is being judged yet; the verdict colouring
// belongs to the site level, where a base is actually being placed
// (DrawPlacementCursor above). Design:
// docs/design/site-selection/site-selection-master-design.md
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Demo sites for the annotated descent (--demo)
//
// Three locations exercising the three main strategies. Region identity
// values are real: Fe/Ti/Th from src/assets/planet/zones.json where the
// entry carries them, terranes from Jolliff, Gillis & Haskin (2000).
// The annotation strip is a TEST overlay, not game UI -- it narrates the
// decision each level asks and its probable consequence.
// ---------------------------------------------------------------------------

// Ground statistics over a sub-square of the real window: what the
// level cards report. Everything measured, nothing synthetic.
struct GroundStats
{
    float meanSlope = 0.0f;
    float maxSlope = 0.0f;
    float buildableFrac = 0.0f;    // slope under the 8 deg gate
    float reliefM = 0.0f;
};

static GroundStats CursorGroundStats(const LolaWindow& window,
                                     double offXKm, double offYKm,
                                     double sizeKm)
{
    GroundStats g;
    int res = window.resolution;
    if (res <= 0 || window.spanKm <= 0.0) return g;
    double half = sizeKm / window.spanKm * res * 0.5;
    int cx = (int)(res * 0.5 + offXKm / window.spanKm * res);
    int cy = (int)(res * 0.5 - offYKm / window.spanKm * res);
    int x0 = std::max(0, (int)(cx - half)), x1 = std::min(res - 1, (int)(cx + half));
    int y0 = std::max(0, (int)(cy - half)), y1 = std::min(res - 1, (int)(cy + half));
    if (x1 <= x0 || y1 <= y0) return g;
    double sum = 0.0; int n = 0, buildable = 0;
    float lo = 1e9f, hi = -1e9f;
    for (int y = y0; y <= y1; y++)
    {
        for (int x = x0; x <= x1; x++)
        {
            float s = window.slopeDeg[y * res + x];
            sum += s; n++;
            if (s > g.maxSlope) g.maxSlope = s;
            if (s < 8.0f) buildable++;
            float e = window.elevationM[y * res + x];
            if (e < lo) lo = e;
            if (e > hi) hi = e;
        }
    }
    g.meanSlope = (float)(sum / n);
    g.buildableFrac = (float)buildable / (float)n;
    g.reliefM = hi - lo;
    return g;
}

// ---------------------------------------------------------------------------
// The two cards + annotation strip: this IS the intended first-stage
// site-selection UI (the annotation strip excepted, which is test-only).
// ---------------------------------------------------------------------------

// Greedy word wrap for tooltip bodies. Returns lines drawn.
static int DrawWrappedText(const char* text, int x, int y, int width,
                           int fontSize, Color color)
{
    char word[64], line[256];
    line[0] = '\0';
    int lines = 0;
    const char* c = text;
    while (*c)
    {
        int wl = 0;
        while (*c && *c != ' ' && wl < 63) word[wl++] = *c++;
        word[wl] = '\0';
        while (*c == ' ') c++;
        char trial[256];
        std::snprintf(trial, sizeof(trial), "%s%s%s", line,
                      line[0] ? " " : "", word);
        if (MeasureText(trial, fontSize) > width && line[0])
        {
            DrawText(line, x, y + lines * (fontSize + 5), fontSize, color);
            lines++;
            std::snprintf(line, sizeof(line), "%s", word);
        }
        else
        {
            std::snprintf(line, sizeof(line), "%s", trial);
        }
    }
    if (line[0])
    {
        DrawText(line, x, y + lines * (fontSize + 5), fontSize, color);
        lines++;
    }
    return lines;
}

// A dotted underline marks a row as hoverable -- the game's affordance
// for "there is a hint here".
static void DrawHintUnderline(int x, int y, int w, Color color)
{
    for (int i = 0; i < w; i += 5)
    {
        DrawRectangle(x + i, y, 2, 1, color);
    }
}

// The hint itself: a tooltip beside the card, connected to its row.
// Game UI, not a test overlay -- in play it appears while the cursor
// rests on the row and vanishes when it leaves.
static SurveyHintResult g_pendingHint = { nullptr, nullptr };
static int g_pendingHintRowY = -1;
static int g_pendingHintRight = 0;

static void DrawHintTooltip(const SurveyHintResult& hint, int rowX,
                            int rowY, int cardRight, Color tint)
{
    if (!hint.title) return;
    int w = 306, x = cardRight + 14, pad = 12;
    // measure by dry run: wrap at 13px over w-2*pad
    // (cheap: count via a second pass after drawing background sized
    // generously, so draw title+body onto a measured panel)
    // First compute lines without drawing: reuse DrawWrappedText on an
    // offscreen y with transparent colour is wasteful; instead estimate
    // then draw box behind using the returned count.
    // Simplest correct order: draw into a measured box -- draw text
    // last, box first, so run the wrap once with BLANK to count.
    int lines = DrawWrappedText(hint.text, -4000, -4000, w - 2 * pad, 13,
                                BLANK);
    int h = 34 + lines * 18 + pad;
    DrawLineEx(Vector2{ (float)(rowX), (float)rowY + 7.0f },
               Vector2{ (float)x, (float)rowY + 7.0f }, 1.0f,
               Color{ tint.r, tint.g, tint.b, 130 });
    DrawRectangle(x, rowY - 10, w, h, Color{ 16, 17, 22, 236 });
    DrawRectangleLinesEx(Rectangle{ (float)x, (float)(rowY - 10), (float)w,
                                    (float)h }, 1.0f, tint);
    DrawText(hint.title, x + pad, rowY - 10 + 10, 14, tint);
    DrawWrappedText(hint.text, x + pad, rowY - 10 + 32, w - 2 * pad, 13,
                    Color{ 208, 212, 222, 255 });
}

static void DrawMiniBar(int x, int y, int w, float frac, Color tint)
{
    DrawRectangle(x, y, w, 6, Color{ 34, 36, 42, 255 });
    DrawRectangle(x, y, (int)(w * Clamp(frac, 0.0f, 1.0f)), 6,
                  Color{ tint.r, tint.g, tint.b, 200 });
}

// The frozen region card. Drawn IDENTICALLY at every level -- the whole
// design in one visual fact: this panel never changes below level 1.
static void DrawRegionCard(const DemoSite& site, int level, int px, int py,
                           const char* hoverKey = nullptr)
{
    int hintRowY = -1;
    SurveyHintResult hint = { nullptr, nullptr };
    int pw = 336, ph = 252;
    Color line = Color{ 120, 150, 205, 255 };
    Color dim = Color{ 205, 210, 220, 255 };
    Color faint = Color{ 128, 134, 146, 255 };
    DrawRectangle(px, py, pw, ph, Color{ 12, 12, 16, 220 });
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw,
                                    (float)ph }, 2.0f, line);
    DrawText(level == 0 ? "REGION - CLAIMING" : "REGION - FIXED AT LEVEL 1",
             px + 12, py + 10, 15, line);
    DrawText(site.regionName, px + 12, py + 32, 25, WHITE);
    DrawText(TextFormat("%s  ·  %s", site.terrane, site.rock),
             px + 12, py + 62, 13, faint);
    DrawHintUnderline(px + 12, py + 77,
                      MeasureText(TextFormat("%s  ·  %s", site.terrane,
                                             site.rock), 13),
                      Color{ 120, 150, 205, 140 });
    if (hoverKey && std::strcmp(hoverKey, "rock") == 0)
    {
        hintRowY = py + 62;
        hint = GetRockHint(site.rock);
    }

    // archetype chip
    int chipW = MeasureText(site.archetype, 14) + 16;
    DrawRectangle(px + 12, py + 82, chipW, 22,
                  Color{ site.archetypeTint.r, site.archetypeTint.g,
                         site.archetypeTint.b, 40 });
    DrawRectangleLinesEx(Rectangle{ (float)(px + 12), (float)(py + 82),
                                    (float)chipW, 22.0f }, 1.0f,
                         site.archetypeTint);
    DrawText(site.archetype, px + 20, py + 86, 14, site.archetypeTint);

    int rowY = py + 116;
    DrawText("iron",     px + 12, rowY, 14, dim);
    DrawHintUnderline(px + 12, rowY + 15, MeasureText("iron", 14),
                      Color{ 120, 150, 205, 140 });
    DrawText(TextFormat("%.1f wt%%", site.fePct), px + pw - 88, rowY, 14, dim);
    DrawMiniBar(px + 12, rowY + 17, pw - 24, site.fePct / 22.0f, line);
    if (hoverKey && std::strcmp(hoverKey, "iron") == 0)
    {
        hintRowY = rowY;
        hint = GetSurveyHint("iron", site.fePct);
    }
    rowY += 32;
    DrawText("titanium", px + 12, rowY, 14, dim);
    DrawHintUnderline(px + 12, rowY + 15, MeasureText("titanium", 14),
                      Color{ 120, 150, 205, 140 });
    DrawText(TextFormat("%.1f wt%%", site.tiPct), px + pw - 88, rowY, 14, dim);
    DrawMiniBar(px + 12, rowY + 17, pw - 24, site.tiPct / 13.0f, line);
    if (hoverKey && std::strcmp(hoverKey, "titanium") == 0)
    {
        hintRowY = rowY;
        hint = GetSurveyHint("titanium", site.tiPct);
    }
    rowY += 32;
    DrawText("thorium",  px + 12, rowY, 14, dim);
    DrawHintUnderline(px + 12, rowY + 15, MeasureText("thorium", 14),
                      Color{ 120, 150, 205, 140 });
    DrawText(TextFormat("%.1f ppm", site.thPpm), px + pw - 88, rowY, 14, dim);
    DrawMiniBar(px + 12, rowY + 17, pw - 24, site.thPpm / 12.0f, line);
    if (hoverKey && std::strcmp(hoverKey, "thorium") == 0)
    {
        hintRowY = rowY;
        hint = GetSurveyHint("thorium", site.thPpm);
    }
    rowY += 34;
    DrawText(site.latitudeNote, px + 12, rowY, 13, faint);
    DrawHintUnderline(px + 12, rowY + 15,
                      MeasureText(site.latitudeNote, 13),
                      Color{ 120, 150, 205, 140 });
    if (hoverKey && std::strcmp(hoverKey, "psr") == 0)
    {
        hintRowY = rowY;
        hint = GetPsrHint(site.psrDistanceKm);
    }
    if (hoverKey && std::strcmp(hoverKey, "illumination") == 0)
    {
        hintRowY = rowY;
        hint = GetSurveyHint("illumination", 85.0f);
    }
    DrawText("orbital survey - one value per region, never refines",
             px + 12, py + ph - 20, 12, Color{ 96, 104, 118, 255 });

    if (hintRowY >= 0)
    {
        // Queued, not drawn: the tooltip must sit on top of every
        // card, so the render pass flushes it last.
        g_pendingHint = hint;
        g_pendingHintRowY = hintRowY;
        g_pendingHintRight = px + pw;
    }
}

static void DrawPendingHintTooltip()
{
    if (g_pendingHintRowY < 0) return;
    DrawHintTooltip(g_pendingHint, g_pendingHintRight, g_pendingHintRowY,
                    g_pendingHintRight, Color{ 150, 190, 255, 255 });
    g_pendingHintRowY = -1;
    g_pendingHint.title = nullptr;
}

// The per-level question card: the level's own measured geometry.
static void DrawLevelCard(const DemoSite& site, int level,
                          const GroundStats& g, const GroundStats* cells,
                          const TerrainBuildability* siteB,
                          const PlacementVerdict* verdict,
                          int px, int py, int pw)
{
    int ph = (level == 4) ? 236 : (level == 3 ? 208 : 164);
    Color line = (level == 4 && verdict)
        ? (verdict->allowed ? Color{ 60, 235, 120, 255 }
                            : Color{ 255, 70, 70, 255 })
        : Color{ 232, 238, 255, 255 };
    Color dim = Color{ 205, 210, 220, 255 };
    Color faint = Color{ 128, 134, 146, 255 };
    DrawRectangle(px, py, pw, ph, Color{ 12, 12, 16, 220 });
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw,
                                    (float)ph }, 2.0f, line);
    DrawText(TextFormat("LEVEL %d / 5", level + 1), px + 12, py + 10, 15, faint);
    DrawText(LEVEL_QUESTION[level], px + 12, py + 30, 21, line);

    int rowY = py + 64;
    if (level == 0)
    {
        DrawText("terrane, rock and latitude decide the", px + 12, rowY, 14, dim);
        DrawText("economy. Chemistry locks HERE - the",   px + 12, rowY + 19, 14, dim);
        DrawText("region card never changes below this.", px + 12, rowY + 38, 14, dim);
        DrawText("click = claim region + anchor playfield", px + 12, rowY + 62, 13, faint);
        return;
    }

    if (level <= 3)
    {
        const char* subject = (level == 1) ? "playfield" :
                              (level == 2) ? "neighbourhood" : "cell";
        DrawText(TextFormat("%s mean slope", subject), px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.1f deg", g.meanSlope), px + pw - 92, rowY, 14, dim);
        rowY += 21;
        DrawText("buildable ground", px + 12, rowY, 14, dim);
        Color bTint = g.buildableFrac > 0.7f ? Color{ 120, 220, 140, 255 }
                    : g.buildableFrac > 0.3f ? Color{ 235, 195, 110, 255 }
                                             : Color{ 240, 120, 100, 255 };
        DrawText(TextFormat("%.0f %%", g.buildableFrac * 100.0f),
                 px + pw - 92, rowY, 14, bTint);
        rowY += 21;
        DrawText("relief", px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.0f m", g.reliefM), px + pw - 92, rowY, 14, dim);
        rowY += 25;
    }
    else if (siteB)
    {
        // Same source as the verdict below it, or the panel argues
        // with itself.
        DrawText("mean slope", px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.1f deg", siteB->meanSlopeDeg),
                 px + pw - 92, rowY, 14,
                 siteB->meanSlopeDeg > 8.0f ? Color{ 240, 120, 100, 255 } : dim);
        rowY += 21;
        DrawText("peak slope", px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.1f deg", siteB->maxSlopeDeg),
                 px + pw - 92, rowY, 14,
                 siteB->maxSlopeDeg > 25.0f ? Color{ 240, 120, 100, 255 } : dim);
        rowY += 21;
        DrawText("roughness", px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.0f m", siteB->roughnessM),
                 px + pw - 92, rowY, 14,
                 siteB->roughnessM > 40.0f ? Color{ 240, 120, 100, 255 } : dim);
        rowY += 21;
        DrawText("relief", px + 12, rowY, 14, dim);
        DrawText(TextFormat("%.0f m", siteB->reliefM),
                 px + pw - 92, rowY, 14, dim);
        rowY += 25;
    }

    if (level == 3 && cells)
    {
        // 3x3 neighbour-buildability glyph: expansion room made visible
        // before the cell is committed.
        DrawText("expansion room (neighbour cells)", px + 12, rowY, 13, faint);
        int gx = px + 12, gy = rowY + 20, cell = 18;
        for (int j = 0; j < 3; j++)
        {
            for (int i = 0; i < 3; i++)
            {
                const GroundStats& c = cells[j * 3 + i];
                Color cc = (c.buildableFrac < 0.0f) ? Color{ 50, 52, 60, 255 }
                    : c.buildableFrac > 0.7f ? Color{ 70, 170, 100, 255 }
                    : c.buildableFrac > 0.3f ? Color{ 190, 155, 80, 255 }
                                             : Color{ 185, 80, 70, 255 };
                DrawRectangle(gx + i * (cell + 3), gy + j * (cell + 3),
                              cell, cell, cc);
            }
        }
        DrawRectangleLinesEx(Rectangle{ (float)(gx + cell + 3),
                                        (float)(gy + cell + 3),
                                        (float)cell, (float)cell },
                             2.0f, WHITE);
    }

    if (level == 4 && siteB && verdict)
    {
        DrawText(TextFormat("illumination   %.0f %%",
                            siteB->illumination * 100.0f),
                 px + 12, rowY, 14, dim);
        rowY += 21;
        DrawText(TextFormat("perm. shadow   %s", siteB->isPsr ? "YES" : "no"),
                 px + 12, rowY, 14,
                 siteB->isPsr ? Color{ 150, 190, 255, 255 } : dim);
        rowY += 21;
        DrawText(TextFormat("earth link     %.0f %%",
                            siteB->earthVisibility * 100.0f),
                 px + 12, rowY, 14, dim);
        rowY += 27;
        DrawText(verdict->reason, px + 12, rowY, 16, line);
    }
}

// Test-only annotation strip. Deliberately NOT styled like the cards, so
// it cannot be mistaken for game UI.
static void DrawTestNote(const DemoSite& site, int level,
                         int screenW, int screenH)
{
    Color amber = Color{ 240, 195, 110, 255 };
    int h = 66, y = screenH - h;
    DrawRectangle(0, y, screenW, h, Color{ 26, 20, 8, 232 });
    DrawRectangle(0, y, screenW, 2, amber);
    DrawText("TEST ANNOTATION", 14, y + 8, 12, amber);
    DrawText(TextFormat("DECISION     %s", site.note[level][0]),
             14, y + 24, 14, Color{ 235, 225, 205, 255 });
    DrawText(TextFormat("CONSEQUENCE  %s", site.note[level][1]),
             14, y + 44, 14, Color{ 185, 175, 155, 255 });
}

// The rect the window span is drawn into. The top-down camera's fovy is
// the vertical world extent, so the span maps onto the screen HEIGHT --
// this is the centred square that span occupies.
static SurveyViewport LadderViewport(int screenW, int screenH)
{
    SurveyViewport viewport;
    viewport.x = (screenW - screenH) * 0.5f;
    viewport.y = 0.0f;
    viewport.width = (float)screenH;
    viewport.height = (float)screenH;
    return viewport;
}

static void DrawSurveyCursorNav(const SurveyCursor& cursor,
                                const SurveyViewport& viewport,
                                int screenW, int screenH)
{
    const SurveyLevelDef* ladder = GetSurveyLadder();
    Color tint = Color{ 232, 238, 255, 255 };
    Rectangle r = SurveyCursorRect(cursor, viewport);

    DrawRectangleRec(r, Color{ tint.r, tint.g, tint.b, 26 });
    DrawRectangleLinesEx(r, 2.0f, Color{ tint.r, tint.g, tint.b, 180 });

    float t = r.width * 0.5f * 0.35f;
    for (int i = 0; i < 4; i++)
    {
        float ox = (i & 1) ? r.x + r.width : r.x;
        float oy = (i & 2) ? r.y + r.height : r.y;
        float sx = (i & 1) ? -1.0f : 1.0f;
        float sy = (i & 2) ? -1.0f : 1.0f;
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox + sx * t, oy }, 4.0f, tint);
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox, oy + sy * t }, 4.0f, tint);
    }
    float cx = r.x + r.width * 0.5f;
    float cy = r.y + r.height * 0.5f;
    DrawCircleV(Vector2{ cx, cy }, 3.0f, tint);

    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(cursor, &lat, &lon);

    // Readout on the far side of the frame, so the panel never covers
    // the ground it is describing.
    int pw = 396, ph = 258;
    int px = (cx < screenW * 0.5f) ? screenW - pw - 16 : 16;
    int py = 78;    // below the HUD title, clear of the scale bar
    Color dim = Color{ 205, 210, 220, 255 };
    Color faint = Color{ 128, 134, 146, 255 };
    DrawRectangle(px, py, pw, ph, Color{ 12, 12, 16, 218 });
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw,
                                    (float)ph }, 2.0f,
                         Color{ tint.r, tint.g, tint.b, 150 });
    DrawText(TextFormat("LEVEL %d  %s", cursor.level + 1,
                        ladder[cursor.level].name), px + 12, py + 10, 19, tint);

    // What this level is FOR. The instrument floors mean the levels ask
    // different questions, not the same question at five resolutions.
    bool regionLevel = (cursor.level <= 2);
    DrawText(regionLevel ? "WHICH REGION?" : "WHICH GROUND?",
             px + pw - 12 - MeasureText(regionLevel ? "WHICH REGION?"
                                                    : "WHICH GROUND?", 15),
             py + 14, 15, Color{ 150, 190, 255, 255 });
    DrawText(TextFormat("%.0f km window   cursor %.1f km   %+.3f %+.3f",
                        cursor.windowSpanKm, cursor.footprintKm, lat, lon),
             px + 12, py + 36, 14, faint);

    // One row per instrument: value, band, and the footprint it was
    // measured over. A row whose instrument is coarser than the cursor
    // has stopped sharpening -- it is drawn dimmed and says so.
    int rowY = py + 62;
    for (int i = 0; i < LAYER_COUNT; i++)
    {
        const SurveyInstrument& ins = INSTRUMENTS[i];
        double window = (ins.footprintKm > cursor.footprintKm)
                        ? ins.footprintKm : cursor.footprintKm;
        float v01 = FieldMean(i, lat, lon, window, 7);
        float b01 = FieldBand(i, lat, lon, ins.footprintKm, cursor.footprintKm);
        float value = ins.lo + (ins.hi - ins.lo) * v01;
        float band = (ins.hi - ins.lo) * b01;
        bool frozen = (ins.footprintKm > cursor.footprintKm);
        Color rowTint = frozen ? faint : dim;

        DrawText(TextFormat("%-14s %s", ins.quantity,
                            frozen ? "" : ""), px + 12, rowY, 15, rowTint);
        int dec = ((ins.hi - ins.lo) < 40.0f) ? 1 : 0;
        DrawText(TextFormat("%.*f +- %.*f %s", dec, value, dec, band, ins.unit),
                 px + 150, rowY, 15, frozen ? Color{ 255, 190, 120, 255 }
                                            : Color{ 150, 230, 170, 255 });
        DrawText(TextFormat("%s %s", ins.name,
                            frozen ? TextFormat("%.3g km avg", ins.footprintKm)
                                   : "resolved"),
                 px + 12, rowY + 18, 12, rowTint);

        // The band as a bar, so the widening is visible rather than read.
        float barW = (float)pw - 24.0f;
        float bx = (float)px + 12.0f, by = (float)rowY + 34.0f;
        DrawRectangle((int)bx, (int)by, (int)barW, 7, Color{ 34, 36, 42, 255 });
        float mid = Clamp(v01, 0.0f, 1.0f);
        float halfBand = Clamp(b01, 0.0f, 0.5f);
        DrawRectangle((int)(bx + (mid - halfBand) * barW), (int)by,
                      (int)(2.0f * halfBand * barW + 1.0f), 7,
                      frozen ? Color{ 190, 130, 60, 210 }
                             : Color{ 70, 150, 95, 210 });
        DrawRectangle((int)(bx + mid * barW) - 1, (int)by - 2, 3, 11,
                      Color{ 235, 240, 255, 255 });
        rowY += 62;
    }
}

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

// ---------------------------------------------------------------------------
// --ladder: render the descent, one PNG per level, cursor aimed at one
// fixed target. Each image's cursor frames exactly the ground the next
// image shows -- which is the whole claim the ladder makes.
//
// Levels 2-5 only: level 1 is the projected orbital disc, which lives in
// the game's render path (OrbitalPickToLatLon), not in this instrument.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Level-1 region highlighting: the mechanism itself.
//
// The disc is a MAP OF REGIONS, not ground with a crosshair. Terranes
// are colour-washed; the named features are outlined; whatever is under
// the cursor lights up with its name on a chip. No cursor rectangle at
// this level -- the lit region IS the selection.
//
// Feature circles carry real centres and radii from
// src/assets/planet/zones.json. The PKT boundary is traced approximately
// from Jolliff, Gillis & Haskin (2000) Fig. 1 -- test-grade, not
// survey-grade; the game will classify from its own composition once the
// generator is inverted.
// ---------------------------------------------------------------------------

struct DiscFeature
{
    const char* name;
    double lat, lon, radiusKm;
};

static const DiscFeature DISC_FEATURES[] =
{
    { "Oceanus Procellarum", 18.4, -57.4, 1296 },
    { "Mare Frigoris", 55.0, 0.0, 723 },
    { "Mare Imbrium", 32.8, -15.6, 573 },
    { "Mare Fecunditatis", -7.8, 51.3, 454 },
    { "Mare Tranquillitatis", 8.5, 31.4, 436 },
    { "Mare Nubium", -21.3, -16.5, 358 },
    { "Mare Serenitatis", 28.0, 17.5, 354 },
    { "Mare Crisium", 17.0, 59.1, 278 },
    { "Mare Humorum", -24.4, -38.6, 194 },
    { "Mare Cognitum", -10.0, -23.1, 175 },
    { "Mare Nectaris", -15.2, 35.3, 170 },
    { "Sinus Medii", 2.4, 1.7, 144 },
    { "Sinus Iridum", 44.1, -31.5, 124 },
    { "Mare Vaporum", 13.3, 3.6, 122 },
    { "Clavius", -58.4, -14.4, 116 },
    { "Ptolemaeus", -9.3, -1.9, 76 },
    { "Copernicus", 9.6, -20.1, 47 },
    { "Tycho", -43.3, -11.4, 43 },
    { "Plato", 51.6, -9.4, 50 },
};
static const int DISC_FEATURE_COUNT =
    (int)(sizeof(DISC_FEATURES) / sizeof(DISC_FEATURES[0]));

// PKT outline, lat/lon vertices. Everything else on the near side is
// FHT for this instrument; SPA is essentially a far-side terrane.
static const double PKT_POLY[][2] =
{
    { 52, -72 }, { 57, -45 }, { 52, -20 }, { 47, -2 }, { 38, 8 },
    { 28, 17 }, { 18, 14 }, { 8, 10 }, { -2, 7 }, { -12, 2 },
    { -22, -6 }, { -30, -18 }, { -32, -33 }, { -26, -48 },
    { -14, -60 }, { -2, -70 }, { 12, -78 }, { 28, -80 }, { 42, -79 },
};
static const int PKT_POLY_COUNT =
    (int)(sizeof(PKT_POLY) / sizeof(PKT_POLY[0]));

static bool InPkt(double lat, double lon)
{
    bool inside = false;
    for (int i = 0, j = PKT_POLY_COUNT - 1; i < PKT_POLY_COUNT; j = i++)
    {
        double yi = PKT_POLY[i][0], xi = PKT_POLY[i][1];
        double yj = PKT_POLY[j][0], xj = PKT_POLY[j][1];
        if (((yi > lat) != (yj > lat)) &&
            (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi))
        {
            inside = !inside;
        }
    }
    return inside;
}

static double FeatureDistKm(const DiscFeature& f, double lat, double lon)
{
    double la1 = lat * DEG2RAD, la2 = f.lat * DEG2RAD;
    double c = std::sin(la1) * std::sin(la2) +
               std::cos(la1) * std::cos(la2) *
               std::cos((lon - f.lon) * DEG2RAD);
    return std::acos(Clamp((float)c, -1.0f, 1.0f)) * LOLA_MOON_RADIUS_M / 1000.0;
}

// Smallest named feature containing the point, or -1.
static int FeatureAt(double lat, double lon)
{
    int best = -1;
    double bestR = 1e18;
    for (int i = 0; i < DISC_FEATURE_COUNT; i++)
    {
        if (FeatureDistKm(DISC_FEATURES[i], lat, lon) <=
            DISC_FEATURES[i].radiusKm && DISC_FEATURES[i].radiusKm < bestR)
        {
            best = i;
            bestR = DISC_FEATURES[i].radiusKm;
        }
    }
    return best;
}

// The near-side map is plate carree, square: lon -90..90 across the
// screen, lat -90..90 down it.
static void DiscToScreen(double lat, double lon, int w, int h,
                         float* x, float* y)
{
    *x = (float)(w * 0.5 + lon / 180.0 * h);
    *y = (float)(h * 0.5 - lat / 180.0 * h);
}

// The terrane is a LABEL, not a layer.
//
// It was drawn as a colour wash over the whole disc. Three things were
// wrong with that, and they compound:
//   - It never provides a selection. 87% of the PKT's area already sits
//     inside a named feature, and the 3% of the near side that is PKT
//     with no feature is not worth a map layer.
//   - Its boundary is 19 hand-placed vertices traced off a figure. Drawn
//     as a filled province it claims a precision the data does not have.
//   - It competes with the real imagery, which is this project's whole
//     distinguishing asset -- and the imagery is what a hover highlight
//     has to sit on top of to read.
//
// So the terrane survives as text: the second line of the hover chip and
// of the region card, and the fallback name for unnamed ground. The one
// thing it uniquely carries -- thorium, the only genuinely terrane-scale
// quantity -- is on the card as a number with a hint.
//
// The consequence for hover, which is the point: nothing terrane-sized
// can highlight any more. Only the feature under the cursor lights up,
// because only the feature under the cursor is a thing you can pick.

static void DrawDiscFeatureOutlines(int w, int h, int hoverFeature,
                                    Color hoverTint)
{
    for (int i = 0; i < DISC_FEATURE_COUNT; i++)
    {
        const DiscFeature& f = DISC_FEATURES[i];
        if (std::fabs(f.lat) > 80.0) continue;
        float cx = 0.0f, cy = 0.0f;
        DiscToScreen(f.lat, f.lon, w, h, &cx, &cy);
        double rDeg = f.radiusKm / (LOLA_M_PER_DEG / 1000.0);
        float ry = (float)(rDeg / 180.0 * h);
        float rx = (float)(ry / Clamp((float)std::cos(f.lat * DEG2RAD),
                                      0.2f, 1.0f));
        if (i == hoverFeature)
        {
            // Fill only the feature under the cursor. This is the one
            // shape on the disc that is both real data and a thing the
            // player can pick, so it is the only thing allowed to lift
            // off the imagery.
            DrawEllipse((int)cx, (int)cy, rx, ry,
                        Color{ hoverTint.r, hoverTint.g, hoverTint.b, 58 });
            DrawEllipseLines((int)cx, (int)cy, rx, ry, hoverTint);
            DrawEllipseLines((int)cx, (int)cy, rx + 1.0f, ry + 1.0f, hoverTint);
            DrawEllipseLines((int)cx, (int)cy, rx + 2.0f, ry + 2.0f,
                             Color{ hoverTint.r, hoverTint.g, hoverTint.b, 120 });
        }
        else
        {
            DrawEllipseLines((int)cx, (int)cy, rx, ry,
                             Color{ 255, 255, 255, 70 });
        }
    }
}

// The hover chip: what is under the cursor, named. This is the whole
// answer at level 1 -- no numbers, no rectangle.
static void DrawHoverChip(float mx, float my, const char* name,
                          const char* sub, Color tint, int screenW)
{
    int tw = std::max(MeasureText(name, 19), MeasureText(sub, 12));
    int bw = tw + 26, bh = 46;
    float bx = mx + 22.0f, by = my - 12.0f;
    if (bx + bw > screenW - 8) bx = mx - bw - 22.0f;
    DrawRectangle((int)bx, (int)by, bw, bh, Color{ 12, 12, 16, 226 });
    DrawRectangleLinesEx(Rectangle{ bx, by, (float)bw, (float)bh }, 2.0f, tint);
    DrawText(name, (int)bx + 12, (int)by + 6, 19, WHITE);
    DrawText(sub, (int)bx + 12, (int)by + 28, 12,
             Color{ 175, 180, 192, 255 });
}

// Named-region boundaries at the window levels: arcs of the real
// feature circles crossing the view, over unmodified imagery -- the
// "boundaries only below orbital" rule.
static void DrawFeatureArcsInWindow(double cLat, double cLon, double spanKm,
                                    int w, int h)
{
    float pxPerKm = (float)h / (float)spanKm;
    double kmPerDeg = LOLA_M_PER_DEG / 1000.0;
    double cosC = Clamp((float)std::cos(cLat * DEG2RAD), 0.05f, 1.0f);
    const char* insideName = nullptr;
    for (int i = 0; i < DISC_FEATURE_COUNT; i++)
    {
        const DiscFeature& f = DISC_FEATURES[i];
        double dist = FeatureDistKm(f, cLat, cLon);
        if (dist > f.radiusKm + spanKm) continue;       // far outside
        if (dist < f.radiusKm - spanKm)                 // deep inside
        {
            if (!insideName) insideName = f.name;
            continue;
        }
        double rDeg = f.radiusKm / kmPerDeg;
        Vector2 prev = { 0 };
        bool havePrev = false;
        Vector2 best = { 0 };
        float bestD = 1e9f;
        for (int k = 0; k <= 720; k++)
        {
            double t = k / 720.0 * 2.0 * PI;
            double la = f.lat + rDeg * std::cos(t);
            double lo = f.lon + rDeg * std::sin(t) /
                        Clamp((float)std::cos(la * DEG2RAD), 0.2f, 1.0f);
            float x = (float)(w * 0.5 + (lo - cLon) * kmPerDeg * cosC * pxPerKm);
            float y = (float)(h * 0.5 - (la - cLat) * kmPerDeg * pxPerKm);
            Vector2 pt = { x, y };
            bool on = x > -w && x < 2 * w && y > -h && y < 2 * h;
            if (on && havePrev && Vector2Distance(prev, pt) < 60.0f)
            {
                DrawLineEx(prev, pt, 2.0f, Color{ 240, 244, 255, 150 });
            }
            prev = pt;
            havePrev = on;
            float dC = Vector2Distance(pt, Vector2{ w * 0.5f, h * 0.5f });
            if (on && x > 20 && x < w - 20 && y > 80 && y < h - 90 && dC < bestD)
            {
                bestD = dC;
                best = pt;
            }
        }
        if (bestD < h * 0.7f)
        {
            DrawText(f.name, (int)best.x + 8, (int)best.y + 6, 15,
                     Color{ 240, 244, 255, 200 });
        }
    }
    if (insideName)
    {
        int tw = MeasureText(insideName, 17);
        DrawText(insideName, (w - tw) / 2, 58, 17,
                 Color{ 255, 255, 255, 110 });
    }
}

static void DrawLadderCursor(const SurveyCursor& cursor,
                             const SurveyViewport& viewport, Color tint)
{
    Rectangle r = SurveyCursorRect(cursor, viewport);
    DrawRectangleRec(r, Color{ tint.r, tint.g, tint.b, 30 });
    DrawRectangleLinesEx(r, 2.5f, tint);
    float t = r.width * 0.5f * 0.35f;
    for (int i = 0; i < 4; i++)
    {
        float ox = (i & 1) ? r.x + r.width : r.x;
        float oy = (i & 2) ? r.y + r.height : r.y;
        float sx = (i & 1) ? -1.0f : 1.0f;
        float sy = (i & 2) ? -1.0f : 1.0f;
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox + sx * t, oy }, 4.0f, tint);
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox, oy + sy * t }, 4.0f, tint);
    }
    DrawCircleV(Vector2{ r.x + r.width * 0.5f, r.y + r.height * 0.5f },
                3.0f, tint);
}

static int RenderLadder(AppState& app)
{
    MapOptions opts = app.options;
    opts.nearside = false;
    const DemoSite* demo = (opts.demo >= 0) ? &DEMO_SITES[opts.demo] : nullptr;

    // The target the player is aiming at, as real coordinates: --place
    // gives its offset from --pick, otherwise a default off-centre spot
    // so the cursor is visibly tracking rather than parked in the middle.
    double targetDxKm = opts.place ? opts.placeDxKm : 74.0;
    double targetDyKm = opts.place ? opts.placeDyKm : -52.0;
    SurveyCursor seed = MakeSurveyCursor(1, opts.pickLat, opts.pickLon);
    seed.offsetXKm = targetDxKm;
    seed.offsetYKm = targetDyKm;
    double targetLat = 0.0, targetLon = 0.0;
    SurveyCursorLatLon(seed, &targetLat, &targetLon);

    SurveyDescent descent = MakeSurveyDescent(opts.pickLat, opts.pickLon);
    descent.levels[1] = MakeSurveyCursor(1, opts.pickLat, opts.pickLon);
    descent.depth = 2;

    std::string stem = opts.outPath;
    size_t dot = stem.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? ".png" : stem.substr(dot);
    if (dot != std::string::npos) stem = stem.substr(0, dot);

    SurveyViewport viewport = LadderViewport(opts.width, opts.height);
    const SurveyLevelDef* table = GetSurveyLadder();
    int written = 0;

    // ---- Level 1: the orbital pick, rendered as the near-side map ----
    // The disc is a map of regions: terranes colour-washed, named
    // features outlined, and whatever is under the cursor LIT UP with
    // its name -- the highlight is the selection mechanism, so there is
    // no cursor rectangle at this level. An optional extra frame hovers
    // somewhere else first, to show the highlight tracking the mouse.
    if (demo)
    {
        MapOptions l1 = opts;
        l1.nearside = true;
        // The map keeps a map sun: the demo's grazing polar sun is for
        // the surface windows, and under it the whole disc goes black.
        l1.sunElevationDeg = 30.0f;
        if (BuildScene(l1, app.dem, app.scene))
        {
            int passes = (demo->altHoverLat < 1e8) ? 2 : 1;
            for (int pass = 0; pass < passes; pass++)
            {
                bool alt = (passes == 2 && pass == 0);
                double hLat = alt ? demo->altHoverLat : opts.pickLat;
                double hLon = alt ? demo->altHoverLon : opts.pickLon;
                int hover = FeatureAt(hLat, hLon);
                bool hoverPkt = InPkt(hLat, hLon);
                Color tint = alt ? Color{ 235, 240, 255, 255 }
                                 : demo->archetypeTint;

                RenderTexture2D target = LoadRenderTexture(opts.width,
                                                           opts.height);
                BeginTextureMode(target);
                Camera3D camera = TopDownCamera(app.scene, 1.0f);
                DrawScene(app.scene, l1, app.styleMode, camera);
                DrawDiscFeatureOutlines(opts.width, opts.height, hover, tint);
                DrawHud(app.scene, l1, app.styleMode, opts.width,
                        opts.height, 1.0f);

                // Cursor position on the map (clamped with an arrow for
                // polar picks rather than drawn at a false latitude).
                float mx = 0.0f, my = 0.0f;
                DiscToScreen(hLat, hLon, opts.width, opts.height, &mx, &my);
                // A pick past the map edge (|lon| > 90 on plate carree)
                // gets clamped with an arrow, not drawn at a false spot.
                if (mx > opts.width - 44.0f)
                {
                    float ax = opts.width - 40.0f;
                    DrawLineEx(Vector2{ ax - 28.0f, my }, Vector2{ ax, my },
                               3.0f, tint);
                    DrawLineEx(Vector2{ ax - 10.0f, my - 8.0f },
                               Vector2{ ax, my }, 3.0f, tint);
                    DrawLineEx(Vector2{ ax - 10.0f, my + 8.0f },
                               Vector2{ ax, my }, 3.0f, tint);
                    mx = ax - 44.0f;
                }
                bool clamped = (my > opts.height - 96.0f);
                if (clamped)
                {
                    float ay = opts.height - 100.0f;
                    DrawLineEx(Vector2{ mx, ay - 28.0f }, Vector2{ mx, ay },
                               3.0f, tint);
                    DrawLineEx(Vector2{ mx - 8.0f, ay - 10.0f },
                               Vector2{ mx, ay }, 3.0f, tint);
                    DrawLineEx(Vector2{ mx + 8.0f, ay - 10.0f },
                               Vector2{ mx, ay }, 3.0f, tint);
                    my = ay - 44.0f;
                }
                DrawCircleLinesV(Vector2{ mx, my }, 6.0f, tint);
                DrawLineEx(Vector2{ mx - 16.0f, my }, Vector2{ mx - 7.0f, my },
                           2.0f, tint);
                DrawLineEx(Vector2{ mx + 7.0f, my }, Vector2{ mx + 16.0f, my },
                           2.0f, tint);
                DrawLineEx(Vector2{ mx, my - 16.0f }, Vector2{ mx, my - 7.0f },
                           2.0f, tint);
                DrawLineEx(Vector2{ mx, my + 7.0f }, Vector2{ mx, my + 16.0f },
                           2.0f, tint);

                if (alt)
                {
                    const char* hn = (hover >= 0)
                        ? DISC_FEATURES[hover].name
                        : (hoverPkt ? "Procellarum KREEP Terrane"
                                    : "Feldspathic Highlands");
                    DrawHoverChip(mx, my, hn,
                                  hoverPkt ? "PKT  ·  hover to inspect"
                                           : "FHT  ·  hover to inspect",
                                  tint, opts.width);
                }
                else
                {
                    if (!demo->hintKey)
                    {
                        DrawHoverChip(mx, my, demo->regionName,
                                      demo->terrane, tint, opts.width);
                    }
                    DrawRegionCard(*demo, 0, 16, 64, demo->hintKey);
                }
                DrawLevelCard(*demo, 0, GroundStats(), nullptr, nullptr,
                              nullptr, opts.width - 352, 64, 336);
                DrawPendingHintTooltip();
                if (alt)
                {
                    Color amber = Color{ 240, 195, 110, 255 };
                    int nh = 66, ny = opts.height - nh;
                    DrawRectangle(0, ny, opts.width, nh,
                                  Color{ 26, 20, 8, 232 });
                    DrawRectangle(0, ny, opts.width, 2, amber);
                    DrawText("TEST ANNOTATION", 14, ny + 8, 12, amber);
                    DrawText("DECISION     None yet - the cursor is elsewhere. The region under it lights up and names itself.",
                             14, ny + 24, 14, Color{ 235, 225, 205, 255 });
                    DrawText("CONSEQUENCE  The highlight IS the selection: no rectangle, no numbers, just lit ground with a name.",
                             14, ny + 44, 14, Color{ 185, 175, 155, 255 });
                }
                else
                {
                    DrawTestNote(*demo, 0, opts.width, opts.height);
                }
                EndTextureMode();

                Image shot = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&shot);
                std::string path = alt
                    ? std::string(TextFormat("%s_L1_HOVER%s", stem.c_str(),
                                             ext.c_str()))
                    : std::string(TextFormat("%s_%s%s", stem.c_str(),
                                             LEVEL_FILE[0], ext.c_str()));
                ExportImage(shot, path.c_str());
                std::cerr << "lunar_map: wrote " << path << "\n";
                UnloadImage(shot);
                UnloadRenderTexture(target);
                written++;
            }
        }
    }

    for (;;)
    {
        SurveyCursor* cursor = SurveyCurrent(&descent);

        // Aim the cursor at the target through the same path the mouse
        // takes: ground -> km -> screen -> track. If the helpers
        // disagree anywhere, the cursor lands off the target and the
        // render shows it.
        double dx = 0.0, dy = 0.0;
        SurveyLatLonToOffsetKm(*cursor, targetLat, targetLon, &dx, &dy);
        float mouseX = 0.0f, mouseY = 0.0f;
        SurveyOffsetKmToScreen(viewport, cursor->windowSpanKm, dx, dy,
                               &mouseX, &mouseY);
        SurveyCursorTrack(cursor, viewport, mouseX, mouseY);

        opts.pickLat = cursor->windowLatDeg;
        opts.pickLon = cursor->windowLonDeg;
        opts.spanKm = cursor->windowSpanKm;

        // BuildScene releases the previous level's GPU resources itself.
        if (!BuildScene(opts, app.dem, app.scene)) return 1;

        // The level's own measured geometry, from the real window.
        int level = cursor->level;    // 1..4 -> L2..L5
        GroundStats g = CursorGroundStats(app.scene.window,
                                          cursor->offsetXKm,
                                          cursor->offsetYKm,
                                          cursor->footprintKm);
        GroundStats cells[9];
        if (level == 3)
        {
            // Neighbour cells of the candidate 5 km cell. Sampled from a
            // fresh 15 km window centred on the CELL, not from the display
            // window: the game's grid continues past the view edge, so a
            // corner cell's neighbours are real ground, not blanks.
            double cLat = 0.0, cLon = 0.0;
            SurveyCursorLatLon(*cursor, &cLat, &cLon);
            LolaWindow neigh = app.dem.Window(cLat, cLon, 15.0, 240,
                                              opts.detail);
            for (int j = 0; j < 3; j++)
            {
                for (int i = 0; i < 3; i++)
                {
                    cells[j * 3 + i] = CursorGroundStats(neigh,
                                                         (i - 1) * 5.0,
                                                         (1 - j) * 5.0, 5.0);
                }
            }
        }
        TerrainBuildability siteB;
        PlacementVerdict verdict;
        bool haveVerdict = false;
        if (level == 4)
        {
            double cLat = 0.0, cLon = 0.0;
            SurveyCursorLatLon(*cursor, &cLat, &cLon);
            siteB = app.dem.EvaluateSite(cLat, cLon, cursor->footprintKm, 30.0);
            verdict = JudgeSite(siteB);
            haveVerdict = true;
        }

        RenderTexture2D target = LoadRenderTexture(opts.width, opts.height);
        BeginTextureMode(target);
        Camera3D camera = TopDownCamera(app.scene, 1.0f);
        DrawScene(app.scene, opts, app.styleMode, camera);
        DrawHud(app.scene, opts, app.styleMode, opts.width, opts.height, 1.0f);

        if (demo)
        {
            if (level == 1)
            {
                DrawFeatureArcsInWindow(opts.pickLat, opts.pickLon,
                                        opts.spanKm, opts.width, opts.height);
            }
            Color tint = haveVerdict
                ? (verdict.allowed ? Color{ 60, 235, 120, 255 }
                                   : Color{ 255, 70, 70, 255 })
                : Color{ 232, 238, 255, 255 };
            DrawLadderCursor(*cursor, viewport, tint);
            DrawRegionCard(*demo, level, 16, 64);
            DrawLevelCard(*demo, level, g, level == 3 ? cells : nullptr,
                          haveVerdict ? &siteB : nullptr,
                          haveVerdict ? &verdict : nullptr,
                          opts.width - 352, 64, 336);
            DrawTestNote(*demo, level, opts.width, opts.height);
        }
        else
        {
            DrawSurveyCursorNav(*cursor, viewport, opts.width, opts.height);
        }
        EndTextureMode();

        Image shot = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&shot);
        std::string path = demo
            ? std::string(TextFormat("%s_%s%s", stem.c_str(),
                                     LEVEL_FILE[level], ext.c_str()))
            : std::string(TextFormat("%s_%d_%s%s%s", stem.c_str(),
                                     cursor->level + 1,
                                     table[cursor->level].name,
                                     opts.truth ? "_truth" : "", ext.c_str()));
        ExportImage(shot, path.c_str());
        std::cerr << "lunar_map: wrote " << path << "\n";
        UnloadImage(shot);
        UnloadRenderTexture(target);
        written++;

        if (demo && cursor->level + 1 >= opts.maxLevel) break;
        if (!SurveyDescend(&descent)) break;
    }
    return written > 0 ? 0 : 1;
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

    LolaSetDespeckle(app.options.despeckle);
    LolaSetOverlayDecimation(app.options.demDecim);
    LolaSetTextureMode(app.options.texture == "craters"
                       ? LolaTexture::CRATERS : LolaTexture::NOISE);
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

    if (app.options.survey)
    {
        TerrainBuildability b = app.dem.EvaluateSite(
            app.options.pickLat, app.options.pickLon, app.options.spanKm);
        std::printf("SITE  %.3f %.3f   footprint %.1f km\n",
                    app.options.pickLat, app.options.pickLon,
                    app.options.spanKm);
        std::printf("  elevation      %8.0f m\n", b.elevationM);
        std::printf("  slope mean/max %8.2f / %.2f deg\n",
                    b.meanSlopeDeg, b.maxSlopeDeg);
        std::printf("  relief         %8.0f m\n", b.reliefM);
        std::printf("  roughness      %8.1f m (RMS off best-fit plane)\n",
                    b.roughnessM);
        std::printf("  illumination   %8.1f %%%s\n", b.illumination * 100.0f,
                    b.isPsr ? "   *** PSR: permanently shadowed ***" : "");
        std::printf("  longest night  %8.1f Earth days\n",
                    b.longestNightDays);
        std::printf("  earth visible  %8.1f %% of libration\n",
                    b.earthVisibility * 100.0f);
        std::printf("  open sky       %8.1f %%\n", b.skyFraction * 100.0f);
        std::printf("  BUILD SCORE    %8.2f\n", b.buildScore);
        CloseWindow();
        return 0;
    }

    if (app.options.ladder)
    {
        if (app.options.outPath.empty())
        {
            std::cerr << "--ladder needs --out PATH (one PNG per level)\n";
            CloseWindow();
            return 1;
        }
        app.styleMode = (app.options.style == "color") ? 1 : 0;
        int rc = RenderLadder(app);
        UnloadSceneGpu(app.scene);
        CloseWindow();
        return rc;
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
        if (app.options.place)
        {
            DrawPlacementCursor(app.options, app.dem, app.options.width,
                                app.options.height);
        }
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
