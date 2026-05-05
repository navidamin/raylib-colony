// Regolith sample sprite generator
//
// Mathematically synthesizes wireframe sprite renders of regolith-like
// particles for the prospecting sample tray. Replaces tools/crystal_gen as the
// asset pipeline source for sample sprites.
//
// Encoding axes (mirroring ShapeFamily / CrystalVisual in the game):
//   - 4 families:    angular, shard, rounded, slab
//                    (order matches ShapeFamily enum: ANGULAR_CHUNKS,
//                     CRYSTALLINE_SHARDS, ROUNDED_NODULES, LAYERED_SLABS)
//   - 5 templates:   parameter perturbations within a family that vary the
//                    geometry (roughness/frequency/jaggedness/ellipsoid mul)
//                    without crossing the family signature
//   - 4 size levels: render scale (richness)
//   - 5 glow levels: soft white silhouette halo behind the wireframe (confidence)
//
// Sprites are drawn as opaque WHITE wireframes on a transparent background, so
// the game tints them at runtime with the sample's element color.
//
// Output layout (4 x 5 x 4 x 5 = 400 PNGs):
//   <output>/<family>/t<n>/size_<s>_glow_<g>.png
//     n in 1..5, s in 1..4, g in 0..4

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

// ============================================================
// Configuration
// ============================================================

static const int RENDER_SIZE = 768;
static const int OUTPUT_SIZE = 128;
static const int SUBDIVISIONS = 4;          // 4 -> 2562 verts, 5120 tris
static const int NUM_TEMPLATES = 5;
static const int NUM_SIZE_LEVELS = 4;
static const int NUM_GLOW_LEVELS = 5;

// Render scale per size level (matches crystal_gen for consistency)
static const float SIZE_SCALES[NUM_SIZE_LEVELS] = { 0.55f, 0.70f, 0.85f, 1.0f };

struct ParticleFamily
{
    const char* name;
    float roughness;
    float baseFrequency;
    int   octaves;
    float persistence;
    float lacunarity;
    Vector3 ellipsoid;
    float jaggedness;
};

// Order MUST match the in-game ShapeFamily enum so a future loader can index
// by static_cast<int>(visual.shapeFamily):
//   0: ANGULAR_CHUNKS      -> angular
//   1: CRYSTALLINE_SHARDS  -> shard
//   2: ROUNDED_NODULES     -> rounded
//   3: LAYERED_SLABS       -> slab
static const ParticleFamily FAMILIES[] = {
    {"angular", 0.30f, 2.0f, 5, 0.55f, 2.10f, {1.05f, 0.95f, 1.00f}, 0.10f},
    {"shard",   0.12f, 1.6f, 4, 0.55f, 2.10f, {1.80f, 0.60f, 0.80f}, 0.06f},
    {"rounded", 0.18f, 1.4f, 4, 0.50f, 2.05f, {1.00f, 1.00f, 1.00f}, 0.04f},
    {"slab",    0.14f, 1.2f, 4, 0.45f, 2.05f, {1.40f, 0.35f, 1.20f}, 0.05f},
};
static const int NUM_FAMILIES = sizeof(FAMILIES) / sizeof(FAMILIES[0]);

// Per-template parameter multipliers — pushes the same family signature toward
// rougher / smoother / stretched / extra-jagged variants without leaving family.
struct TemplateVariation
{
    float roughnessMul;
    float frequencyMul;
    float jaggednessMul;
    Vector3 ellipsoidMul;
};
static const TemplateVariation TEMPLATES[NUM_TEMPLATES] = {
    { 1.00f, 1.00f, 1.00f, {1.00f, 1.00f, 1.00f} },  // t1 nominal
    { 1.20f, 1.00f, 1.10f, {1.00f, 1.00f, 1.00f} },  // t2 rougher
    { 0.85f, 0.80f, 0.95f, {1.00f, 1.00f, 1.00f} },  // t3 smoother + larger lumps
    { 1.00f, 1.05f, 1.00f, {1.10f, 0.92f, 1.05f} },  // t4 stretched + finer
    { 1.05f, 1.10f, 1.40f, {1.00f, 1.00f, 1.00f} },  // t5 extra jagged
};

// ============================================================
// Filesystem helper
// ============================================================

static void MkdirP(const char* path)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// ============================================================
// 3D gradient (Perlin) noise
// ============================================================

static uint32_t Hash3D(int x, int y, int z, uint32_t seed)
{
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(z) * 0x27D4EB2Fu;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

static Vector3 GradientAt(int x, int y, int z, uint32_t seed)
{
    static const Vector3 G[12] = {
        { 1.0f,  1.0f,  0.0f}, {-1.0f,  1.0f,  0.0f}, { 1.0f, -1.0f,  0.0f}, {-1.0f, -1.0f,  0.0f},
        { 1.0f,  0.0f,  1.0f}, {-1.0f,  0.0f,  1.0f}, { 1.0f,  0.0f, -1.0f}, {-1.0f,  0.0f, -1.0f},
        { 0.0f,  1.0f,  1.0f}, { 0.0f, -1.0f,  1.0f}, { 0.0f,  1.0f, -1.0f}, { 0.0f, -1.0f, -1.0f},
    };
    return G[Hash3D(x, y, z, seed) % 12];
}

static float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
static float LerpF(float a, float b, float t) { return a + (b - a) * t; }

static float PerlinNoise3D(Vector3 p, uint32_t seed)
{
    int xi = static_cast<int>(floorf(p.x));
    int yi = static_cast<int>(floorf(p.y));
    int zi = static_cast<int>(floorf(p.z));
    float xf = p.x - xi, yf = p.y - yi, zf = p.z - zi;
    float u = Fade(xf), v = Fade(yf), w = Fade(zf);

    Vector3 d000 = { xf,        yf,        zf        };
    Vector3 d100 = { xf - 1.0f, yf,        zf        };
    Vector3 d010 = { xf,        yf - 1.0f, zf        };
    Vector3 d110 = { xf - 1.0f, yf - 1.0f, zf        };
    Vector3 d001 = { xf,        yf,        zf - 1.0f };
    Vector3 d101 = { xf - 1.0f, yf,        zf - 1.0f };
    Vector3 d011 = { xf,        yf - 1.0f, zf - 1.0f };
    Vector3 d111 = { xf - 1.0f, yf - 1.0f, zf - 1.0f };

    float n000 = Vector3DotProduct(GradientAt(xi    , yi    , zi    , seed), d000);
    float n100 = Vector3DotProduct(GradientAt(xi + 1, yi    , zi    , seed), d100);
    float n010 = Vector3DotProduct(GradientAt(xi    , yi + 1, zi    , seed), d010);
    float n110 = Vector3DotProduct(GradientAt(xi + 1, yi + 1, zi    , seed), d110);
    float n001 = Vector3DotProduct(GradientAt(xi    , yi    , zi + 1, seed), d001);
    float n101 = Vector3DotProduct(GradientAt(xi + 1, yi    , zi + 1, seed), d101);
    float n011 = Vector3DotProduct(GradientAt(xi    , yi + 1, zi + 1, seed), d011);
    float n111 = Vector3DotProduct(GradientAt(xi + 1, yi + 1, zi + 1, seed), d111);

    float x00 = LerpF(n000, n100, u);
    float x10 = LerpF(n010, n110, u);
    float x01 = LerpF(n001, n101, u);
    float x11 = LerpF(n011, n111, u);
    float y0  = LerpF(x00, x10, v);
    float y1  = LerpF(x01, x11, v);
    return LerpF(y0, y1, w);
}

static float Fbm3D(Vector3 p, int octaves, float persistence, float lacunarity, uint32_t seed)
{
    float total = 0.0f, amplitude = 1.0f, frequency = 1.0f, maxValue = 0.0f;
    for (int i = 0; i < octaves; i++)
    {
        Vector3 sp = Vector3Scale(p, frequency);
        total += PerlinNoise3D(sp, seed + static_cast<uint32_t>(i) * 0x68E31DA4u) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxValue;
}

// ============================================================
// Icosphere construction
// ============================================================

struct IcoMesh
{
    std::vector<Vector3> verts;
    std::vector<int> tris;
};

static int GetMidpoint(IcoMesh& m, std::unordered_map<uint64_t, int>& cache, int a, int b)
{
    uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | static_cast<uint64_t>(std::max(a, b));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Vector3 mid = Vector3Normalize(Vector3Scale(Vector3Add(m.verts[a], m.verts[b]), 0.5f));
    int idx = static_cast<int>(m.verts.size());
    m.verts.push_back(mid);
    cache[key] = idx;
    return idx;
}

static IcoMesh BuildIcosphere(int subdivisions)
{
    const float t = (1.0f + sqrtf(5.0f)) * 0.5f;
    IcoMesh m;
    m.verts = {
        Vector3Normalize({-1.0f,  t,    0.0f }), Vector3Normalize({ 1.0f,  t,    0.0f }),
        Vector3Normalize({-1.0f, -t,    0.0f }), Vector3Normalize({ 1.0f, -t,    0.0f }),
        Vector3Normalize({ 0.0f, -1.0f,  t   }), Vector3Normalize({ 0.0f,  1.0f,  t   }),
        Vector3Normalize({ 0.0f, -1.0f, -t   }), Vector3Normalize({ 0.0f,  1.0f, -t   }),
        Vector3Normalize({ t,    0.0f, -1.0f }), Vector3Normalize({ t,    0.0f,  1.0f }),
        Vector3Normalize({-t,    0.0f, -1.0f }), Vector3Normalize({-t,    0.0f,  1.0f }),
    };
    m.tris = {
         0, 11,  5,   0,  5,  1,   0,  1,  7,   0,  7, 10,   0, 10, 11,
         1,  5,  9,   5, 11,  4,  11, 10,  2,  10,  7,  6,   7,  1,  8,
         3,  9,  4,   3,  4,  2,   3,  2,  6,   3,  6,  8,   3,  8,  9,
         4,  9,  5,   2,  4, 11,   6,  2, 10,   8,  6,  7,   9,  8,  1,
    };

    for (int s = 0; s < subdivisions; s++)
    {
        std::unordered_map<uint64_t, int> cache;
        std::vector<int> next;
        next.reserve(m.tris.size() * 4);
        for (size_t i = 0; i < m.tris.size(); i += 3)
        {
            int v0 = m.tris[i], v1 = m.tris[i + 1], v2 = m.tris[i + 2];
            int a = GetMidpoint(m, cache, v0, v1);
            int b = GetMidpoint(m, cache, v1, v2);
            int c = GetMidpoint(m, cache, v2, v0);
            next.insert(next.end(), { v0,  a,  c });
            next.insert(next.end(), { v1,  b,  a });
            next.insert(next.end(), { v2,  c,  b });
            next.insert(next.end(), {  a,  b,  c });
        }
        m.tris = std::move(next);
    }
    return m;
}

// ============================================================
// Regolith deformation
// ============================================================

static IcoMesh GenerateRegolith(const ParticleFamily& fam, uint32_t seed)
{
    IcoMesh m = BuildIcosphere(SUBDIVISIONS);

    float ox = static_cast<float>(Hash3D(0, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    float oy = static_cast<float>(Hash3D(1, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    float oz = static_cast<float>(Hash3D(2, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    Vector3 noiseOffset = { ox, oy, oz };

    for (auto& v : m.verts)
    {
        Vector3 dir = v;
        Vector3 lowPos = Vector3Add(Vector3Scale(dir, fam.baseFrequency), noiseOffset);
        float disp = Fbm3D(lowPos, fam.octaves, fam.persistence, fam.lacunarity, seed);
        Vector3 hiPos = Vector3Add(Vector3Scale(dir, fam.baseFrequency * 6.0f), noiseOffset);
        disp += PerlinNoise3D(hiPos, seed ^ 0xDEADBEEFu) * fam.jaggedness;

        float r = 1.0f + disp * fam.roughness;
        Vector3 displaced = Vector3Scale(dir, r);
        displaced.x *= fam.ellipsoid.x;
        displaced.y *= fam.ellipsoid.y;
        displaced.z *= fam.ellipsoid.z;
        v = displaced;
    }
    return m;
}

static Mesh IcoToRaylibMesh(const IcoMesh& src)
{
    int triCount = static_cast<int>(src.tris.size()) / 3;
    Mesh mesh = {0};
    mesh.vertexCount = triCount * 3;
    mesh.triangleCount = triCount;
    mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals  = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));

    for (int i = 0; i < triCount; i++)
    {
        Vector3 v0 = src.verts[src.tris[i * 3 + 0]];
        Vector3 v1 = src.verts[src.tris[i * 3 + 1]];
        Vector3 v2 = src.verts[src.tris[i * 3 + 2]];
        Vector3 n = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(v1, v0), Vector3Subtract(v2, v0)));
        for (int j = 0; j < 3; j++)
        {
            Vector3 v = (j == 0) ? v0 : (j == 1) ? v1 : v2;
            mesh.vertices[(i * 3 + j) * 3 + 0] = v.x;
            mesh.vertices[(i * 3 + j) * 3 + 1] = v.y;
            mesh.vertices[(i * 3 + j) * 3 + 2] = v.z;
            mesh.normals [(i * 3 + j) * 3 + 0] = n.x;
            mesh.normals [(i * 3 + j) * 3 + 1] = n.y;
            mesh.normals [(i * 3 + j) * 3 + 2] = n.z;
        }
    }
    UploadMesh(&mesh, false);
    return mesh;
}

// ============================================================
// Sprite render: white wireframe + soft white silhouette halo
// ============================================================

static void RenderSprite(RenderTexture2D target, Mesh mesh, Camera3D camera,
                          float sizeScale, int glowLevel)
{
    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);

    rlDisableDepthTest();

    // Halo passes — filled mesh in low-alpha white at progressively larger
    // scales build up a soft silhouette glow behind the wireframe.
    if (glowLevel > 0)
    {
        Material haloMat = LoadMaterialDefault();
        for (int p = 0; p < glowLevel; p++)
        {
            float haloScale = sizeScale * (1.0f + (p + 1) * 0.05f);
            int alpha = 60 - p * 10;
            if (alpha < 15) alpha = 15;
            haloMat.maps[MATERIAL_MAP_DIFFUSE].color =
                (Color){ 255, 255, 255, static_cast<unsigned char>(alpha) };
            DrawMesh(mesh, haloMat, MatrixScale(haloScale, haloScale, haloScale));
        }
    }

    // Main wireframe — opaque white so runtime tinting fully colorizes the lines.
    rlEnableWireMode();
    Material wireMat = LoadMaterialDefault();
    wireMat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 255, 255, 255, 255 };
    DrawMesh(mesh, wireMat, MatrixScale(sizeScale, sizeScale, sizeScale));
    rlDisableWireMode();

    rlEnableDepthTest();

    EndMode3D();
    EndTextureMode();
}

static void SaveResized(RenderTexture2D target, const char* path, int outSize)
{
    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img);
    ImageResize(&img, outSize, outSize);
    ExportImage(img, path);
    UnloadImage(img);
}

// Apply per-template parameter perturbations to a base family.
static ParticleFamily ApplyTemplate(const ParticleFamily& base, const TemplateVariation& tv)
{
    ParticleFamily f = base;
    f.roughness     *= tv.roughnessMul;
    f.baseFrequency *= tv.frequencyMul;
    f.jaggedness    *= tv.jaggednessMul;
    f.ellipsoid.x   *= tv.ellipsoidMul.x;
    f.ellipsoid.y   *= tv.ellipsoidMul.y;
    f.ellipsoid.z   *= tv.ellipsoidMul.z;
    return f;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv)
{
    const char* outputDir = "../../src/assets/sprites/samples";
    uint32_t baseSeed = 1337;
    if (argc > 1) outputDir = argv[1];
    if (argc > 2) baseSeed = static_cast<uint32_t>(strtoul(argv[2], nullptr, 10));

    int totalSprites = NUM_FAMILIES * NUM_TEMPLATES * NUM_SIZE_LEVELS * NUM_GLOW_LEVELS;

    printf("Regolith Sample Sprite Generator\n");
    printf("Output dir: %s\n", outputDir);
    printf("Base seed:  %u\n", baseSeed);
    printf("Layout:     %d families x %d templates x %d sizes x %d glows = %d sprites\n",
           NUM_FAMILIES, NUM_TEMPLATES, NUM_SIZE_LEVELS, NUM_GLOW_LEVELS, totalSprites);

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(RENDER_SIZE, RENDER_SIZE, "Regolith Sprite Gen");
    SetTargetFPS(60);

    Camera3D camera = {0};
    camera.position = { 2.4f, 1.8f, 2.4f };
    camera.target   = { 0.0f, 0.0f, 0.0f };
    camera.up       = { 0.0f, 1.0f, 0.0f };
    camera.fovy     = 32.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(RENDER_SIZE, RENDER_SIZE);

    int written = 0;
    for (int f = 0; f < NUM_FAMILIES; f++)
    {
        const ParticleFamily& base = FAMILIES[f];
        printf("\nFamily: %-8s  base ellipsoid=(%.2f,%.2f,%.2f) rough=%.2f\n",
               base.name, base.ellipsoid.x, base.ellipsoid.y, base.ellipsoid.z, base.roughness);

        for (int t = 0; t < NUM_TEMPLATES; t++)
        {
            ParticleFamily fam = ApplyTemplate(base, TEMPLATES[t]);
            uint32_t seed = baseSeed
                          + static_cast<uint32_t>(f) * 10007u
                          + static_cast<uint32_t>(t) * 97u;

            IcoMesh ico = GenerateRegolith(fam, seed);
            Mesh mesh = IcoToRaylibMesh(ico);

            char tdir[640];
            snprintf(tdir, sizeof(tdir), "%s/%s/t%d", outputDir, base.name, t + 1);
            MkdirP(tdir);
            printf("  t%d  seed=%u  rough=%.2f  freq=%.2f  jag=%.2f  ell=(%.2f,%.2f,%.2f)\n",
                   t + 1, seed, fam.roughness, fam.baseFrequency, fam.jaggedness,
                   fam.ellipsoid.x, fam.ellipsoid.y, fam.ellipsoid.z);

            for (int s = 0; s < NUM_SIZE_LEVELS; s++)
            {
                for (int g = 0; g < NUM_GLOW_LEVELS; g++)
                {
                    char path[768];
                    snprintf(path, sizeof(path), "%s/size_%d_glow_%d.png",
                             tdir, s + 1, g);
                    RenderSprite(target, mesh, camera, SIZE_SCALES[s], g);
                    SaveResized(target, path, OUTPUT_SIZE);
                    written++;
                }
            }
            UnloadMesh(mesh);
        }
    }

    UnloadRenderTexture(target);
    CloseWindow();

    printf("\nDone. Wrote %d / %d sprites.\n", written, totalSprites);
    return 0;
}
