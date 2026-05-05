// Regolith particle shape generator
//
// Mathematically synthesizes irregular regolith-like 3D particles by displacing
// an icosphere with multi-octave Perlin noise. The technique mirrors the
// spherical-harmonic-radial representation used by NIST (Garboczi et al.) for
// real CT-scanned particles, but trades scientific exactness for runtime speed
// and infinite seeded variety.
//
// For each family the generator produces:
//   - PNG renders: wireframe (matches the CT-scan reference look) and lit solid
//   - OBJ mesh exports for downstream use in the game or external tools
//
// Output layout:
//   <output>/<family>/sample_<n>.obj
//   <output>/<family>/sample_<n>_wire.png
//   <output>/<family>/sample_<n>_lit.png

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
static const int OUTPUT_SIZE = 256;
static const int SUBDIVISIONS = 4;          // 4 -> 2562 verts, 5120 tris
static const int SAMPLES_PER_FAMILY = 6;

struct ParticleFamily
{
    const char* name;
    float roughness;        // displacement amplitude (fraction of unit radius)
    float baseFrequency;    // base spatial frequency of fbm
    int   octaves;          // octaves of fbm
    float persistence;      // amplitude falloff per octave
    float lacunarity;       // frequency growth per octave
    Vector3 ellipsoid;      // global non-uniform stretch applied after displacement
    float jaggedness;       // extra high-frequency detail amplitude
};

static const ParticleFamily FAMILIES[] = {
    // Smooth rounded — matches the left blob in the CT-scan reference
    {"rounded",   0.18f, 1.4f, 4, 0.50f, 2.05f, {1.00f, 1.00f, 1.00f}, 0.04f},
    // Elongated lozenge — matches the right blob in the CT-scan reference
    {"elongated", 0.16f, 1.6f, 4, 0.50f, 2.05f, {1.55f, 0.70f, 0.85f}, 0.04f},
    // Heavily jagged angular fragment
    {"jagged",    0.30f, 2.0f, 5, 0.55f, 2.10f, {1.05f, 0.95f, 1.00f}, 0.10f},
    // Subtly pebbly, near-spherical
    {"pebbly",    0.12f, 1.0f, 3, 0.45f, 2.00f, {1.05f, 0.95f, 1.00f}, 0.02f},
};
static const int NUM_FAMILIES = sizeof(FAMILIES) / sizeof(FAMILIES[0]);

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

static float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float LerpF(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float PerlinNoise3D(Vector3 p, uint32_t seed)
{
    int xi = static_cast<int>(floorf(p.x));
    int yi = static_cast<int>(floorf(p.y));
    int zi = static_cast<int>(floorf(p.z));
    float xf = p.x - xi;
    float yf = p.y - yi;
    float zf = p.z - zi;

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
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;
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
    std::vector<int> tris;  // 3 indices per triangle
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

    // Per-particle noise field offset so two particles of the same family look distinct
    float ox = static_cast<float>(Hash3D(0, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    float oy = static_cast<float>(Hash3D(1, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    float oz = static_cast<float>(Hash3D(2, 0, 0, seed) & 0xFFFF) / 65535.0f * 100.0f;
    Vector3 noiseOffset = { ox, oy, oz };

    for (auto& v : m.verts)
    {
        Vector3 dir = v;  // unit-sphere vertex == its outward normal
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

// Convert the index-based IcoMesh to a flat-shaded raylib Mesh (per-face normals).
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
// OBJ exporter
// ============================================================

static void ExportOBJ(const IcoMesh& m, const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f) { printf("  WARN: could not write %s\n", path); return; }
    fprintf(f, "# regolith_gen synthetic particle\n");
    fprintf(f, "# verts: %zu  tris: %zu\n", m.verts.size(), m.tris.size() / 3);
    for (const auto& v : m.verts) fprintf(f, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
    for (size_t i = 0; i < m.tris.size(); i += 3)
        fprintf(f, "f %d %d %d\n", m.tris[i] + 1, m.tris[i + 1] + 1, m.tris[i + 2] + 1);
    fclose(f);
}

// ============================================================
// Lit shader (matches crystal_gen style)
// ============================================================

static const char* VS_LIT =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "out vec3 fragNormal;\n"
    "out vec3 fragPos;\n"
    "void main() {\n"
    "    fragNormal = mat3(matModel) * vertexNormal;\n"
    "    fragPos = (matModel * vec4(vertexPosition, 1.0)).xyz;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* FS_LIT =
    "#version 330\n"
    "in vec3 fragNormal;\n"
    "in vec3 fragPos;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec3 norm = normalize(fragNormal);\n"
    "    vec3 keyLight = normalize(vec3(0.5, 0.8, 0.3));\n"
    "    vec3 fillLight = normalize(vec3(-0.4, 0.3, -0.5));\n"
    "    vec3 viewDir = normalize(vec3(2.0, 2.5, 2.0) - fragPos);\n"
    "    float ambient = 0.30;\n"
    "    float diffKey  = max(dot(norm, keyLight),  0.0);\n"
    "    float diffFill = max(dot(norm, fillLight), 0.0);\n"
    "    vec3 halfDir = normalize(keyLight + viewDir);\n"
    "    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);\n"
    "    float lighting = ambient + diffKey * 0.55 + diffFill * 0.20 + spec * 0.30;\n"
    "    finalColor = vec4(colDiffuse.rgb * clamp(lighting, 0.0, 1.4), colDiffuse.a);\n"
    "}\n";

// ============================================================
// Rendering passes
// ============================================================

// Wireframe render — emulates the CT-scan reference image: white background,
// blue thin-line wire mesh.
static void RenderWireframe(RenderTexture2D target, Mesh mesh, Camera3D camera)
{
    BeginTextureMode(target);
    ClearBackground(WHITE);
    BeginMode3D(camera);

    rlEnableWireMode();
    Material wireMat = LoadMaterialDefault();
    wireMat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 70, 140, 230, 255 };
    DrawMesh(mesh, wireMat, MatrixIdentity());
    rlDisableWireMode();

    EndMode3D();
    EndTextureMode();
}

static void RenderLit(RenderTexture2D target, Mesh mesh, Camera3D camera, Shader shader)
{
    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);

    Material mat = LoadMaterialDefault();
    mat.shader = shader;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 175, 165, 150, 255 };  // regolith gray-tan
    DrawMesh(mesh, mat, MatrixIdentity());

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

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv)
{
    const char* outputDir = "../../src/assets/sprites/regolith";
    uint32_t baseSeed = 1337;
    if (argc > 1) outputDir = argv[1];
    if (argc > 2) baseSeed = static_cast<uint32_t>(strtoul(argv[2], nullptr, 10));

    printf("Regolith Particle Generator\n");
    printf("Output dir: %s\n", outputDir);
    printf("Base seed:  %u\n", baseSeed);
    printf("Families:   %d   Samples/family: %d\n", NUM_FAMILIES, SAMPLES_PER_FAMILY);

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(RENDER_SIZE, RENDER_SIZE, "Regolith Generator");
    SetTargetFPS(60);

    Shader litShader = LoadShaderFromMemory(VS_LIT, FS_LIT);
    if (litShader.id == 0)
    {
        printf("WARN: lit shader failed; using default\n");
        litShader = LoadShaderFromMemory(NULL, NULL);
    }

    Camera3D camera = {0};
    camera.position = { 2.4f, 1.8f, 2.4f };
    camera.target   = { 0.0f, 0.0f, 0.0f };
    camera.up       = { 0.0f, 1.0f, 0.0f };
    camera.fovy     = 32.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(RENDER_SIZE, RENDER_SIZE);

    int totalSamples = 0;
    for (int f = 0; f < NUM_FAMILIES; f++)
    {
        const ParticleFamily& fam = FAMILIES[f];
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/%s", outputDir, fam.name);
        MkdirP(dir);
        printf("\nFamily: %-10s  rough=%.2f  ellipsoid=(%.2f,%.2f,%.2f)\n",
               fam.name, fam.roughness, fam.ellipsoid.x, fam.ellipsoid.y, fam.ellipsoid.z);

        for (int s = 0; s < SAMPLES_PER_FAMILY; s++)
        {
            uint32_t seed = baseSeed + static_cast<uint32_t>(f) * 10007u + static_cast<uint32_t>(s) * 97u;
            IcoMesh ico = GenerateRegolith(fam, seed);
            Mesh mesh = IcoToRaylibMesh(ico);

            char objPath[640], wirePath[640], litPath[640];
            snprintf(objPath,  sizeof(objPath),  "%s/sample_%d.obj",       dir, s + 1);
            snprintf(wirePath, sizeof(wirePath), "%s/sample_%d_wire.png",  dir, s + 1);
            snprintf(litPath,  sizeof(litPath),  "%s/sample_%d_lit.png",   dir, s + 1);

            ExportOBJ(ico, objPath);

            RenderWireframe(target, mesh, camera);
            SaveResized(target, wirePath, OUTPUT_SIZE);

            RenderLit(target, mesh, camera, litShader);
            SaveResized(target, litPath, OUTPUT_SIZE);

            UnloadMesh(mesh);
            totalSamples++;
            printf("  sample %d: seed=%u  verts=%zu tris=%zu\n",
                   s + 1, seed, ico.verts.size(), ico.tris.size() / 3);
        }
    }

    UnloadRenderTexture(target);
    UnloadShader(litShader);
    CloseWindow();

    printf("\nDone. Generated %d samples across %d families.\n", totalSamples, NUM_FAMILIES);
    return 0;
}
