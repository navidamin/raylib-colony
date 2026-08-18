// Real-elevation lunar map tool (`lunar_map`).
//
// Renders the actual Moon from the LOLA LDEM_16 elevation model
// (prototypes/planet_visuals/data/lola/ldem_16_uint.tif) — the whole
// near side, or any picked region — as a 3D heightmap mesh with a
// lunar-specific GLSL shading model:
//
//   real DEM heightmap  -> terrain elevation (mesh + height texture)
//   per-pixel normals   -> slope from the height texture, not the mesh
//   explicit sun vector -> harsh light, near-black shadows, no haze
//   WAC albedo crop     -> subtle real gray/brown surface variation
//   multi-scale noise   -> fine regolith variation (kept subtle)
//   curvature term      -> crater rims and floors read stronger
//
// Two styles: `shaded` (photographic hillshade look) and `color`
// (LOLA-style rainbow elevation map, hillshade-modulated).
//
// Standalone tool beside the game: only raylib + LolaDem, no game code.
//
// Usage (from the repo root, so relative asset paths resolve):
//   cmake --build build --target lunar_map
//   tools/lunarmap/lunarmap.sh --nearside
//   tools/lunarmap/lunarmap.sh --pick 43.3,17.3 --span 250   # Aristoteles
//   ./build/src/lunar_map --pick -43.3,-11.4 --tilt          # interactive
//
// Headless (--out) needs the software-GL wrapper the script applies.

#include "raylib.h"
#include "raymath.h"

#include "lola_dem.h"

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
    float ambient = 0.06f;
    int width = 1200;
    int height = 1200;
    int demRes = 0;                // height texture res (0 = auto)
    int meshRes = 256;             // mesh grid resolution (max 256)
    bool tilt = false;             // tilted 3D slab instead of top-down
    std::string outPath;           // empty = interactive window
    std::string demPath = DEFAULT_DEM_PATH;
    std::string wacPath = DEFAULT_WAC_PATH;
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
        << "  --ambient F       ambient light level     (default: 0.06)\n"
        << "  --tilt            tilted 3D slab view instead of top-down\n"
        << "  --size WxH        output resolution       (default: 1200x1200)\n"
        << "  --demres N        height texture pixels   (default: auto)\n"
        << "  --meshres N       mesh grid resolution    (default: 256)\n"
        << "  --out PATH        render PNG and exit (else: interactive)\n"
        << "  --dem PATH        DEM TIFF path\n"
        << "  --help            show this message\n"
        << "\n"
        << "Interactive keys: drag=orbit (tilt)  wheel=zoom  arrows=sun\n"
        << "                  TAB=style  T=tilt  Q/E=exaggeration  S=shot\n";
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
        else if (arg == "--ambient" && hasNext) { options.ambient = (float)std::atof(argv[++i]); }
        else if (arg == "--tilt") { options.tilt = true; }
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
// lunar appearance. Normals are computed per pixel from the height
// texture (finite differences at physical scale), so crater detail
// survives even where the mesh grid is coarser than the DEM.
// ---------------------------------------------------------------------------

static const char* TERRAIN_VS = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 mvp;
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec3 fragWorldPos;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorldPos = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* TERRAIN_FS = R"(
#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D heightMap;   // metres vs reference radius (R32F)
uniform sampler2D albedoMap;   // WAC mosaic crop

uniform vec2 texelSize;        // 1 / height texture resolution
uniform vec2 worldPerTexel;    // km per height texel (x east, y south)
uniform float heightScale;     // world km per metre of elevation
uniform vec3 sunDirection;     // toward the sun, normalised
uniform vec3 sunColor;
uniform float ambient;
uniform float minHeight;       // metres, for the colour ramp
uniform float maxHeight;
uniform int styleMode;         // 0 shaded, 1 colour elevation
uniform float noiseAmp;

float HeightAt(vec2 uv) { return texture(heightMap, uv).r; }

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
    return mix(mix(Hash(i), Hash(i + vec2(1, 0)), u.x),
               mix(Hash(i + vec2(0, 1)), Hash(i + vec2(1, 1)), u.x), u.y);
}

// LOLA-style elevation ramp (deep blue -> green -> red -> white).
vec3 ElevationRamp(float t)
{
    const vec3 stops[9] = vec3[](
        vec3(0.10, 0.05, 0.35),   // deepest basins
        vec3(0.10, 0.25, 0.75),
        vec3(0.05, 0.55, 0.85),
        vec3(0.10, 0.65, 0.40),
        vec3(0.45, 0.75, 0.20),
        vec3(0.95, 0.85, 0.20),
        vec3(0.95, 0.50, 0.15),
        vec3(0.85, 0.15, 0.10),
        vec3(0.98, 0.96, 0.94));  // highest peaks
    float x = clamp(t, 0.0, 1.0) * 8.0;
    int i = int(min(floor(x), 7.0));
    return mix(stops[i], stops[i + 1], fract(x));
}

void main()
{
    vec2 uv = fragTexCoord;

    // Per-pixel normal from the height texture at physical scale.
    float hL = HeightAt(uv - vec2(texelSize.x, 0.0));
    float hR = HeightAt(uv + vec2(texelSize.x, 0.0));
    float hU = HeightAt(uv - vec2(0.0, texelSize.y));
    float hD = HeightAt(uv + vec2(0.0, texelSize.y));
    float h = HeightAt(uv);
    vec3 normal = normalize(vec3(
        -(hR - hL) * heightScale / (2.0 * worldPerTexel.x),
        1.0,
        -(hD - hU) * heightScale / (2.0 * worldPerTexel.y)));

    // Harsh lunar sunlight: pure Lambert, low ambient, no haze.
    float NdotL = max(dot(normal, sunDirection), 0.0);
    // Deepen the shadow side a touch (s-curve), keeping lit slopes linear.
    float lit = NdotL * NdotL * (3.0 - 2.0 * NdotL);
    lit = mix(NdotL, lit, 0.35);

    // Curvature (Laplacian of height): crater rims catch extra light,
    // floors and hollows settle darker. Kept subtle.
    float lap = (hL + hR + hU + hD - 4.0 * h);
    float curve = clamp(-lap * heightScale * 3.0 / worldPerTexel.x,
                        -0.12, 0.12);

    // Multi-scale regolith variation (subtle, or it reads procedural).
    float largeNoise = Noise(uv * 12.0);
    float mediumNoise = Noise(uv * 60.0);
    float fineNoise = Noise(uv * 280.0);
    float variation = (largeNoise - 0.5) * 0.10
                    + (mediumNoise - 0.5) * 0.05
                    + (fineNoise - 0.5) * 0.03;

    vec3 light = sunColor * lit + vec3(ambient);

    if (styleMode == 1)
    {
        // Colour elevation map, hillshade-modulated for readability.
        float t = (h - minHeight) / max(1.0, maxHeight - minHeight);
        vec3 ramp = ElevationRamp(t);
        float shade = 0.42 + 0.68 * lit + curve;
        finalColor = vec4(ramp * shade, 1.0);
        return;
    }

    // Photographic style: real albedo pulled toward neutral gray,
    // sun-lit, with crater emphasis and regolith grain.
    vec3 albedo = texture(albedoMap, uv).rgb;
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
    finalColor = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Terrain construction
// ---------------------------------------------------------------------------

// World layout: x east (km), z south (km), y up (km * exaggeration /
// 1000 per metre). The window is centred on the origin.
struct TerrainScene
{
    LolaWindow window;
    Mesh mesh = { 0 };
    Model model = { 0 };
    Texture2D heightTex = { 0 };
    Texture2D albedoTex = { 0 };
    Shader shader = { 0 };
    float worldWidthKm = 0.0f;     // east-west extent (real km)
    float worldHeightKm = 0.0f;    // north-south extent (real km)
    // Geometry is normalised so the long side is 200 world units —
    // raylib's default far clip plane (1000) would otherwise swallow a
    // 5458 km near-side map. The shader keeps working in real km: its
    // slope maths only uses the height/ground RATIO, which the uniform
    // scale leaves untouched.
    float worldScale = 1.0f;       // world units per km
    float ScaledW() const { return worldWidthKm * worldScale; }
    float ScaledH() const { return worldHeightKm * worldScale; }
    double lat0 = 0.0, lat1 = 0.0, lon0 = 0.0, lon1 = 0.0;
    int locTexel = 0, locWorldPerTexel = 0, locHeightScale = 0;
    int locSunDir = 0, locSunColor = 0, locAmbient = 0;
    int locMinH = 0, locMaxH = 0, locStyle = 0, locNoiseAmp = 0;
};

// Flat grid mesh; elevation is applied in the vertex data (so the tilt
// view has real relief) while shading reads the height texture.
// worldW/worldH and exaggeration arrive pre-multiplied by the scene's
// worldScale, so vertex heights stay proportional to the ground plane.
static Mesh BuildTerrainMesh(const LolaWindow& window, float worldW,
                             float worldH, float exaggeration, int gridRes)
{
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
            // Non-square windows store rows of `res` samples per row
            // regardless — resolution is the row stride.
            float elev = window.elevationM[(size_t)sy * res + sx];
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

static Texture2D BuildHeightTexture(const LolaWindow& window, int texW,
                                    int texH)
{
    Image img = { 0 };
    img.width = texW;
    img.height = texH;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R32;
    img.data = MemAlloc(texW * texH * sizeof(float));
    // The window is already at (texW, texH) resolution by construction.
    std::memcpy(img.data, window.elevationM.data(),
                (size_t)texW * texH * sizeof(float));
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
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
    float x0 = (float)((lon0 + 180.0) / 360.0) * wac.width;
    float x1 = (float)((lon1 + 180.0) / 360.0) * wac.width;
    float y0 = (float)((90.0 - lat1) / 180.0) * wac.height;
    float y1 = (float)((90.0 - lat0) / 180.0) * wac.height;
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

static bool BuildScene(const MapOptions& options, const LolaDem& dem,
                       TerrainScene& scene)
{
    int texW, texH;
    if (options.nearside)
    {
        scene.lat0 = -90.0; scene.lat1 = 90.0;
        scene.lon0 = -90.0; scene.lon1 = 90.0;
        int res = (options.demRes > 0) ? options.demRes : 2048;
        texW = texH = std::clamp(res, 64, 2880);
        scene.window = dem.WindowDegrees(scene.lat0, scene.lat1,
                                         scene.lon0, scene.lon1, texW, texH);
        // Plate carrée map: both axes in equator-scale km.
        scene.worldWidthKm = (float)(180.0 * LOLA_M_PER_DEG / 1000.0);
        scene.worldHeightKm = scene.worldWidthKm;
    }
    else
    {
        int res = (options.demRes > 0) ? options.demRes : 1024;
        texW = texH = std::clamp(res, 64, 4096);
        scene.window = dem.Window(options.pickLat, options.pickLon,
                                  options.spanKm, texW);
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
    // Window rows are square (resolution x resolution) in both paths.
    scene.window.resolution = texW;

    scene.worldScale = 200.0f /
        std::max(scene.worldWidthKm, scene.worldHeightKm);
    scene.mesh = BuildTerrainMesh(scene.window, scene.ScaledW(),
                                  scene.ScaledH(),
                                  options.exaggeration * scene.worldScale,
                                  options.meshRes);
    scene.model = LoadModelFromMesh(scene.mesh);
    scene.heightTex = BuildHeightTexture(scene.window, texW, texH);
    scene.albedoTex = BuildAlbedoTexture(options, scene.lat0, scene.lat1,
                                         scene.lon0, scene.lon1);

    scene.shader = LoadShaderFromMemory(TERRAIN_VS, TERRAIN_FS);
    // Route raylib's material-map binding to our sampler names: the
    // diffuse slot carries the height map, the specular slot the albedo.
    scene.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
        GetShaderLocation(scene.shader, "heightMap");
    scene.shader.locs[SHADER_LOC_MAP_SPECULAR] =
        GetShaderLocation(scene.shader, "albedoMap");
    scene.model.materials[0].shader = scene.shader;
    scene.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
        scene.heightTex;
    scene.model.materials[0].maps[MATERIAL_MAP_SPECULAR].texture =
        scene.albedoTex;
    scene.locTexel = GetShaderLocation(scene.shader, "texelSize");
    scene.locWorldPerTexel = GetShaderLocation(scene.shader, "worldPerTexel");
    scene.locHeightScale = GetShaderLocation(scene.shader, "heightScale");
    scene.locSunDir = GetShaderLocation(scene.shader, "sunDirection");
    scene.locSunColor = GetShaderLocation(scene.shader, "sunColor");
    scene.locAmbient = GetShaderLocation(scene.shader, "ambient");
    scene.locMinH = GetShaderLocation(scene.shader, "minHeight");
    scene.locMaxH = GetShaderLocation(scene.shader, "maxHeight");
    scene.locStyle = GetShaderLocation(scene.shader, "styleMode");
    scene.locNoiseAmp = GetShaderLocation(scene.shader, "noiseAmp");
    return true;
}

static void ApplyShaderState(const TerrainScene& scene,
                             const MapOptions& options, int styleMode)
{
    float texel[2] = { 1.0f / scene.heightTex.width,
                       1.0f / scene.heightTex.height };
    float wpt[2] = { scene.worldWidthKm / scene.heightTex.width,
                     scene.worldHeightKm / scene.heightTex.height };
    float heightScale = options.exaggeration / 1000.0f;
    Vector3 sun = SunDirection(options.sunAzimuthDeg,
                               options.sunElevationDeg);
    float sunColor[3] = { 1.0f, 0.97f, 0.92f };
    float noiseAmp = 1.0f;
    SetShaderValue(scene.shader, scene.locTexel, texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(scene.shader, scene.locWorldPerTexel, wpt,
                   SHADER_UNIFORM_VEC2);
    SetShaderValue(scene.shader, scene.locHeightScale, &heightScale,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene.shader, scene.locSunDir, &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(scene.shader, scene.locSunColor, sunColor,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(scene.shader, scene.locAmbient, &options.ambient,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene.shader, scene.locMinH, &scene.window.minElevationM,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene.shader, scene.locMaxH, &scene.window.maxElevationM,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene.shader, scene.locStyle, &styleMode,
                   SHADER_UNIFORM_INT);
    SetShaderValue(scene.shader, scene.locNoiseAmp, &noiseAmp,
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
// HUD: title, scale bar, elevation legend
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
    const Color dim = Color{ 160, 160, 160, 255 };

    const char* title = options.nearside
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
    DrawText(TextFormat("%.0f km", niceKm), bx + barPx + 8, by - 6, 14, ink);

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

// ---------------------------------------------------------------------------
// Rendering
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

int main(int argc, char** argv)
{
    MapOptions options;
    if (!ParseArgs(argc, argv, options)) return 1;
    bool headless = !options.outPath.empty();

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(options.width, options.height, "lunar_map - LOLA elevation");

    LolaDem dem;
    if (!dem.Load(options.demPath))
    {
        CloseWindow();
        return 1;
    }

    TerrainScene scene;
    if (!BuildScene(options, dem, scene))
    {
        CloseWindow();
        return 1;
    }
    std::cerr << TextFormat(
        "lunar_map: window %.1f..%.1f km elevation, %d px height texture\n",
        scene.window.minElevationM / 1000.0f,
        scene.window.maxElevationM / 1000.0f, scene.heightTex.width);

    int styleMode = (options.style == "color") ? 1 : 0;
    bool tilt = options.tilt;
    float yawDeg = 180.0f, pitchDeg = 52.0f, zoom = 1.0f;

    if (headless)
    {
        RenderTexture2D target = LoadRenderTexture(options.width,
                                                   options.height);
        BeginTextureMode(target);
        Camera3D camera = tilt ? TiltCamera(scene, yawDeg, pitchDeg, zoom)
                               : TopDownCamera(scene, zoom);
        DrawScene(scene, options, styleMode, camera);
        DrawHud(scene, options, styleMode, options.width, options.height,
                zoom);
        EndTextureMode();

        Image shot = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&shot);
        ExportImage(shot, options.outPath.c_str());
        std::cerr << "lunar_map: wrote " << options.outPath << "\n";
        UnloadImage(shot);
        UnloadRenderTexture(target);
    }
    else
    {
        SetTargetFPS(60);
        MapOptions live = options;
        while (!WindowShouldClose())
        {
            // Sun steering, style/tilt toggles, zoom, orbit.
            float dt = GetFrameTime();
            if (IsKeyDown(KEY_LEFT)) live.sunAzimuthDeg -= 60.0f * dt;
            if (IsKeyDown(KEY_RIGHT)) live.sunAzimuthDeg += 60.0f * dt;
            if (IsKeyDown(KEY_UP))
                live.sunElevationDeg = Clamp(live.sunElevationDeg + 30.0f * dt,
                                             2.0f, 89.0f);
            if (IsKeyDown(KEY_DOWN))
                live.sunElevationDeg = Clamp(live.sunElevationDeg - 30.0f * dt,
                                             2.0f, 89.0f);
            if (IsKeyPressed(KEY_TAB)) styleMode = 1 - styleMode;
            if (IsKeyPressed(KEY_T)) tilt = !tilt;
            if (IsKeyDown(KEY_Q))
                live.exaggeration = Clamp(live.exaggeration - 2.0f * dt,
                                          0.5f, 12.0f);
            if (IsKeyDown(KEY_E))
                live.exaggeration = Clamp(live.exaggeration + 2.0f * dt,
                                          0.5f, 12.0f);
            zoom = Clamp(zoom * (1.0f + GetMouseWheelMove() * 0.1f),
                         0.25f, 12.0f);
            if (tilt && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                Vector2 d = GetMouseDelta();
                yawDeg += d.x * 0.4f;
                pitchDeg = Clamp(pitchDeg - d.y * 0.3f, 10.0f, 88.0f);
            }
            // Exaggeration changes need the mesh rebuilt (vertex heights).
            if (live.exaggeration != options.exaggeration &&
                !IsKeyDown(KEY_Q) && !IsKeyDown(KEY_E))
            {
                options.exaggeration = live.exaggeration;
                UnloadModel(scene.model);
                scene.mesh = BuildTerrainMesh(
                    scene.window, scene.ScaledW(), scene.ScaledH(),
                    options.exaggeration * scene.worldScale,
                    options.meshRes);
                scene.model = LoadModelFromMesh(scene.mesh);
                scene.model.materials[0].shader = scene.shader;
                scene.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
                    scene.heightTex;
            }
            options.sunAzimuthDeg = live.sunAzimuthDeg;
            options.sunElevationDeg = live.sunElevationDeg;

            BeginDrawing();
            Camera3D camera = tilt
                ? TiltCamera(scene, yawDeg, pitchDeg, zoom)
                : TopDownCamera(scene, zoom);
            DrawScene(scene, options, styleMode, camera);
            DrawHud(scene, options, styleMode, GetScreenWidth(),
                    GetScreenHeight(), zoom);
            if (IsKeyPressed(KEY_S)) TakeScreenshot("lunar_map_shot.png");
            EndDrawing();
        }
    }

    UnloadModel(scene.model);
    UnloadTexture(scene.heightTex);
    UnloadTexture(scene.albedoTex);
    UnloadShader(scene.shader);
    CloseWindow();
    return 0;
}
