#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <sys/stat.h>

// --- Configuration ---
static const int RENDER_SIZE = 512;
static const int OUTPUT_SIZE = 128;
static const int GLOW_LEVELS = 5;
static const int SIZE_LEVELS = 4;
static const int SHAPES_PER_FAMILY = 5;
static const int NUM_FAMILIES = 4;

static const char* FAMILY_NAMES[] = {"family_a", "family_b", "family_c", "family_d"};
static const char* SHAPE_NAMES[][5] = {
    {"a1_cleaved", "a2_shatter", "a3_wedge", "a4_stacked", "a5_corner"},
    {"b1_crystal", "b2_twin", "b3_needle", "b4_tabular", "b5_druzy"},
    {"c1_cobble", "c2_botryoidal", "c3_concretion", "c4_pebble", "c5_split"},
    {"d1_flagstone", "d2_shale", "d3_crossbed", "d4_laminate", "d5_breccia"},
};

// --- Utility: create directory recursively ---
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

// --- Mesh builders ---
// Each returns a Mesh composed of triangles. All meshes are centered at origin,
// sized roughly to fit in a [-1, 1] bounding box.

struct Tri { Vector3 a, b, c; };

static Mesh TriListToMesh(const std::vector<Tri>& tris)
{
    int vertCount = static_cast<int>(tris.size()) * 3;
    Mesh mesh = {0};
    mesh.vertexCount = vertCount;
    mesh.triangleCount = static_cast<int>(tris.size());
    mesh.vertices = (float*)MemAlloc(vertCount * 3 * sizeof(float));
    mesh.normals = (float*)MemAlloc(vertCount * 3 * sizeof(float));

    for (int i = 0; i < static_cast<int>(tris.size()); i++)
    {
        Vector3 v0 = tris[i].a, v1 = tris[i].b, v2 = tris[i].c;
        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 n = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

        for (int j = 0; j < 3; j++)
        {
            Vector3 v = (j == 0) ? v0 : (j == 1) ? v1 : v2;
            mesh.vertices[(i * 3 + j) * 3 + 0] = v.x;
            mesh.vertices[(i * 3 + j) * 3 + 1] = v.y;
            mesh.vertices[(i * 3 + j) * 3 + 2] = v.z;
            mesh.normals[(i * 3 + j) * 3 + 0] = n.x;
            mesh.normals[(i * 3 + j) * 3 + 1] = n.y;
            mesh.normals[(i * 3 + j) * 3 + 2] = n.z;
        }
    }

    UploadMesh(&mesh, false);
    return mesh;
}

static void AddBox(std::vector<Tri>& tris, Vector3 pos, Vector3 size, float rotY = 0.0f)
{
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    Vector3 corners[8] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz},
    };

    float cosR = cosf(rotY * DEG2RAD), sinR = sinf(rotY * DEG2RAD);
    for (auto& c : corners)
    {
        float x = c.x * cosR - c.z * sinR;
        float z = c.x * sinR + c.z * cosR;
        c.x = x + pos.x;
        c.z = z + pos.z;
        c.y += pos.y;
    }

    int faces[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {1,5,6,2},
        {4,0,3,7}, {3,2,6,7}, {4,5,1,0}
    };
    for (auto& f : faces)
    {
        tris.push_back({corners[f[0]], corners[f[1]], corners[f[2]]});
        tris.push_back({corners[f[0]], corners[f[2]], corners[f[3]]});
    }
}

static void AddPrism(std::vector<Tri>& tris, Vector3 pos, float radius, float height, int sides)
{
    float hy = height * 0.5f;
    std::vector<Vector3> top(sides), bot(sides);
    for (int i = 0; i < sides; i++)
    {
        float a = (float)i / sides * 2.0f * PI;
        float x = cosf(a) * radius + pos.x;
        float z = sinf(a) * radius + pos.z;
        top[i] = {x, pos.y + hy, z};
        bot[i] = {x, pos.y - hy, z};
    }

    Vector3 topCenter = {pos.x, pos.y + hy, pos.z};
    Vector3 botCenter = {pos.x, pos.y - hy, pos.z};

    for (int i = 0; i < sides; i++)
    {
        int j = (i + 1) % sides;
        tris.push_back({topCenter, top[i], top[j]});
        tris.push_back({botCenter, bot[j], bot[i]});
        tris.push_back({bot[i], bot[j], top[j]});
        tris.push_back({bot[i], top[j], top[i]});
    }
}

static void AddSphere(std::vector<Tri>& tris, Vector3 pos, float radius, int rings, int slices)
{
    for (int i = 0; i < rings; i++)
    {
        float phi0 = PI * i / rings;
        float phi1 = PI * (i + 1) / rings;
        for (int j = 0; j < slices; j++)
        {
            float theta0 = 2.0f * PI * j / slices;
            float theta1 = 2.0f * PI * (j + 1) / slices;

            Vector3 p00 = {pos.x + radius * sinf(phi0) * cosf(theta0),
                           pos.y + radius * cosf(phi0),
                           pos.z + radius * sinf(phi0) * sinf(theta0)};
            Vector3 p10 = {pos.x + radius * sinf(phi1) * cosf(theta0),
                           pos.y + radius * cosf(phi1),
                           pos.z + radius * sinf(phi1) * sinf(theta0)};
            Vector3 p01 = {pos.x + radius * sinf(phi0) * cosf(theta1),
                           pos.y + radius * cosf(phi0),
                           pos.z + radius * sinf(phi0) * sinf(theta1)};
            Vector3 p11 = {pos.x + radius * sinf(phi1) * cosf(theta1),
                           pos.y + radius * cosf(phi1),
                           pos.z + radius * sinf(phi1) * sinf(theta1)};

            tris.push_back({p00, p10, p11});
            tris.push_back({p00, p11, p01});
        }
    }
}

// ============================================================
// Family A: Angular Chunks (Regolith — surface debris)
// ============================================================

static Mesh GenA1_Cleaved()
{
    std::vector<Tri> t;
    AddBox(t, {-0.3f, 0.0f, 0.0f}, {0.55f, 0.7f, 0.6f}, 5.0f);
    AddBox(t, { 0.3f, 0.05f, 0.05f}, {0.50f, 0.65f, 0.55f}, -3.0f);
    return TriListToMesh(t);
}

static Mesh GenA2_Shatter()
{
    std::vector<Tri> t;
    AddBox(t, {0.0f, 0.0f, 0.0f}, {0.35f, 0.50f, 0.30f}, 15.0f);
    AddBox(t, {0.35f, -0.1f, 0.15f}, {0.30f, 0.40f, 0.25f}, -20.0f);
    AddBox(t, {-0.25f, -0.15f, -0.2f}, {0.28f, 0.35f, 0.28f}, 40.0f);
    AddBox(t, {0.1f, 0.2f, -0.25f}, {0.22f, 0.25f, 0.20f}, -10.0f);
    return TriListToMesh(t);
}

static Mesh GenA3_Wedge()
{
    std::vector<Tri> t;
    // Two opposing triangular wedges
    AddBox(t, {-0.25f, 0.1f, 0.0f}, {0.50f, 0.70f, 0.40f}, 12.0f);
    AddBox(t, { 0.25f, -0.1f, 0.0f}, {0.45f, 0.65f, 0.35f}, -12.0f);
    return TriListToMesh(t);
}

static Mesh GenA4_Stacked()
{
    std::vector<Tri> t;
    AddBox(t, {0.0f, -0.35f, 0.0f}, {0.70f, 0.22f, 0.55f}, 3.0f);
    AddBox(t, {0.05f, -0.10f, 0.02f}, {0.65f, 0.20f, 0.50f}, -2.0f);
    AddBox(t, {-0.03f, 0.12f, -0.03f}, {0.60f, 0.18f, 0.52f}, 5.0f);
    AddBox(t, {0.02f, 0.32f, 0.0f}, {0.55f, 0.16f, 0.48f}, -4.0f);
    return TriListToMesh(t);
}

static Mesh GenA5_Corner()
{
    std::vector<Tri> t;
    AddBox(t, {0.0f, 0.0f, 0.0f}, {0.75f, 0.75f, 0.65f}, 0.0f);
    AddBox(t, {0.35f, 0.35f, 0.3f}, {0.30f, 0.30f, 0.25f}, 25.0f);
    return TriListToMesh(t);
}

// ============================================================
// Family B: Crystalline Shards (Intact Bedrock — deep minerals)
// ============================================================

static Mesh GenB1_Crystal()
{
    std::vector<Tri> t;
    AddPrism(t, {0.0f, 0.0f, 0.0f}, 0.30f, 1.2f, 6);
    // Small chip at base
    AddBox(t, {0.25f, -0.55f, 0.1f}, {0.15f, 0.12f, 0.10f}, 30.0f);
    return TriListToMesh(t);
}

static Mesh GenB2_Twin()
{
    std::vector<Tri> t;
    AddPrism(t, {-0.15f, 0.05f, 0.0f}, 0.22f, 1.0f, 6);
    AddPrism(t, { 0.18f, -0.05f, 0.08f}, 0.20f, 0.85f, 6);
    return TriListToMesh(t);
}

static Mesh GenB3_Needle()
{
    std::vector<Tri> t;
    AddPrism(t, {0.0f, 0.0f, 0.0f}, 0.10f, 1.1f, 5);
    AddPrism(t, {0.18f, 0.1f, 0.12f}, 0.08f, 0.9f, 5);
    AddPrism(t, {-0.15f, -0.05f, -0.1f}, 0.09f, 0.8f, 5);
    AddPrism(t, {0.08f, -0.15f, -0.15f}, 0.07f, 0.7f, 5);
    return TriListToMesh(t);
}

static Mesh GenB4_Tabular()
{
    std::vector<Tri> t;
    AddPrism(t, {0.0f, 0.0f, 0.0f}, 0.55f, 0.20f, 6);
    return TriListToMesh(t);
}

static Mesh GenB5_Druzy()
{
    std::vector<Tri> t;
    // Flat base
    AddBox(t, {0.0f, -0.25f, 0.0f}, {0.8f, 0.15f, 0.6f}, 0.0f);
    // Small crystals on top
    for (int i = 0; i < 9; i++)
    {
        float cx = (i % 3 - 1) * 0.22f;
        float cz = (i / 3 - 1) * 0.16f;
        float h = 0.15f + (i * 37 % 7) * 0.04f;
        AddPrism(t, {cx, -0.15f + h * 0.5f, cz}, 0.06f, h, 5);
    }
    return TriListToMesh(t);
}

// ============================================================
// Family C: Rounded Nodules (Megaregolith — compacted rubble)
// ============================================================

static Mesh GenC1_Cobble()
{
    std::vector<Tri> t;
    AddSphere(t, {0.0f, 0.0f, 0.0f}, 0.55f, 8, 10);
    AddBox(t, {0.4f, -0.3f, 0.2f}, {0.15f, 0.10f, 0.12f}, 20.0f);
    return TriListToMesh(t);
}

static Mesh GenC2_Botryoidal()
{
    std::vector<Tri> t;
    AddSphere(t, {0.0f, 0.0f, 0.0f}, 0.35f, 7, 9);
    AddSphere(t, {0.28f, 0.08f, 0.0f}, 0.28f, 7, 9);
    AddSphere(t, {-0.20f, 0.15f, 0.18f}, 0.25f, 7, 9);
    AddSphere(t, {0.10f, -0.20f, -0.15f}, 0.22f, 7, 9);
    return TriListToMesh(t);
}

static Mesh GenC3_Concretion()
{
    std::vector<Tri> t;
    AddSphere(t, {0.0f, 0.0f, 0.0f}, 0.50f, 8, 10);
    // Shell fragments
    AddBox(t, {0.35f, 0.25f, 0.1f}, {0.20f, 0.04f, 0.18f}, 15.0f);
    AddBox(t, {-0.3f, -0.25f, -0.15f}, {0.18f, 0.04f, 0.15f}, -25.0f);
    return TriListToMesh(t);
}

static Mesh GenC4_Pebble()
{
    std::vector<Tri> t;
    AddSphere(t, {-0.25f, -0.1f, 0.0f}, 0.25f, 6, 8);
    AddSphere(t, { 0.20f,  0.0f, 0.15f}, 0.20f, 6, 8);
    AddSphere(t, { 0.0f, -0.15f, -0.2f}, 0.18f, 6, 8);
    AddSphere(t, { 0.30f, -0.15f, -0.1f}, 0.15f, 6, 8);
    AddSphere(t, {-0.15f,  0.15f, 0.2f}, 0.17f, 6, 8);
    return TriListToMesh(t);
}

static Mesh GenC5_Split()
{
    std::vector<Tri> t;
    // Two halves of an oval
    AddSphere(t, {-0.18f, 0.0f, 0.0f}, 0.42f, 7, 9);
    AddSphere(t, { 0.18f, 0.0f, 0.0f}, 0.42f, 7, 9);
    // Crystalline interior visible between halves
    AddPrism(t, {0.0f, 0.0f, 0.0f}, 0.12f, 0.50f, 6);
    return TriListToMesh(t);
}

// ============================================================
// Family D: Layered Slabs (Fractured Bedrock — sheared layers)
// ============================================================

static Mesh GenD1_Flagstone()
{
    std::vector<Tri> t;
    AddBox(t, {-0.05f, 0.22f, 0.0f}, {0.70f, 0.08f, 0.55f}, 2.0f);
    AddBox(t, { 0.05f, 0.08f, 0.03f}, {0.75f, 0.08f, 0.50f}, -1.0f);
    AddBox(t, {-0.02f, -0.06f, -0.02f}, {0.72f, 0.08f, 0.52f}, 3.0f);
    AddBox(t, { 0.03f, -0.20f, 0.0f}, {0.68f, 0.08f, 0.48f}, -2.0f);
    return TriListToMesh(t);
}

static Mesh GenD2_Shale()
{
    std::vector<Tri> t;
    // Thin slabs fanning apart
    for (int i = 0; i < 5; i++)
    {
        float yy = (i - 2) * 0.12f;
        float rot = (i - 2) * 8.0f;
        float xoff = (i - 2) * 0.04f;
        AddBox(t, {xoff, yy, 0.0f}, {0.65f - i * 0.03f, 0.05f, 0.45f}, rot);
    }
    return TriListToMesh(t);
}

static Mesh GenD3_CrossBed()
{
    std::vector<Tri> t;
    // Diagonal slabs
    AddBox(t, {0.0f, 0.15f, 0.0f}, {0.60f, 0.06f, 0.45f}, 15.0f);
    AddBox(t, {0.0f, -0.05f, 0.0f}, {0.55f, 0.06f, 0.42f}, -15.0f);
    AddBox(t, {0.0f, -0.25f, 0.0f}, {0.58f, 0.06f, 0.40f}, 10.0f);
    // Perpendicular pieces
    AddBox(t, {0.0f, 0.05f, 0.0f}, {0.06f, 0.50f, 0.40f}, 0.0f);
    return TriListToMesh(t);
}

static Mesh GenD4_Laminate()
{
    std::vector<Tri> t;
    for (int i = 0; i < 7; i++)
    {
        float yy = (i - 3) * 0.09f;
        AddBox(t, {0.0f, yy, 0.0f}, {0.70f, 0.04f, 0.50f}, 0.0f);
    }
    return TriListToMesh(t);
}

static Mesh GenD5_Breccia()
{
    std::vector<Tri> t;
    // Main slab
    AddBox(t, {0.0f, 0.0f, 0.0f}, {0.75f, 0.15f, 0.55f}, 0.0f);
    // Angular fragment inclusions
    AddBox(t, {-0.15f, 0.0f, 0.1f}, {0.12f, 0.10f, 0.10f}, 35.0f);
    AddBox(t, { 0.20f, 0.0f, -0.08f}, {0.10f, 0.08f, 0.12f}, -20.0f);
    AddBox(t, { 0.0f, 0.0f, -0.15f}, {0.08f, 0.12f, 0.08f}, 50.0f);
    AddBox(t, {-0.25f, 0.0f, -0.12f}, {0.09f, 0.09f, 0.07f}, 15.0f);
    return TriListToMesh(t);
}

// ============================================================
// Generator dispatch table
// ============================================================

typedef Mesh (*MeshGenFunc)();

static MeshGenFunc GENERATORS[NUM_FAMILIES][SHAPES_PER_FAMILY] = {
    {GenA1_Cleaved, GenA2_Shatter, GenA3_Wedge, GenA4_Stacked, GenA5_Corner},
    {GenB1_Crystal, GenB2_Twin, GenB3_Needle, GenB4_Tabular, GenB5_Druzy},
    {GenC1_Cobble, GenC2_Botryoidal, GenC3_Concretion, GenC4_Pebble, GenC5_Split},
    {GenD1_Flagstone, GenD2_Shale, GenD3_CrossBed, GenD4_Laminate, GenD5_Breccia},
};

// ============================================================
// Lighting shader (embedded GLSL 330)
// ============================================================

static const char* VS_LIGHTING =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "out vec3 fragNormal;\n"
    "out vec3 fragPos;\n"
    "void main() {\n"
    "    fragNormal = vertexNormal;\n"
    "    fragPos = vertexPosition;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* FS_LIGHTING =
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
    "\n"
    "    float ambient = 0.25;\n"
    "\n"
    "    float diffKey = max(dot(norm, keyLight), 0.0);\n"
    "    float diffFill = max(dot(norm, fillLight), 0.0);\n"
    "\n"
    "    vec3 halfDir = normalize(keyLight + viewDir);\n"
    "    float spec = pow(max(dot(norm, halfDir), 0.0), 64.0);\n"
    "\n"
    "    float rim = 1.0 - max(dot(norm, viewDir), 0.0);\n"
    "    rim = pow(rim, 3.0) * 0.3;\n"
    "\n"
    "    float lighting = ambient + diffKey * 0.55 + diffFill * 0.15 + spec * 0.45 + rim;\n"
    "    finalColor = vec4(colDiffuse.rgb * clamp(lighting, 0.0, 1.4), colDiffuse.a);\n"
    "}\n";

// ============================================================
// Rendering
// ============================================================

static void RenderCrystal(RenderTexture2D target, Mesh mesh, Camera3D camera,
                           Shader shader, float scale, int glowLevel)
{
    BeginTextureMode(target);
    ClearBackground(BLANK);

    BeginMode3D(camera);

    unsigned char baseGray = 155;
    unsigned char glowBoost = static_cast<unsigned char>(glowLevel * 22);
    unsigned char r = static_cast<unsigned char>(std::min(255, baseGray + glowBoost));
    unsigned char g = static_cast<unsigned char>(std::min(255, baseGray + glowBoost));
    unsigned char b = static_cast<unsigned char>(std::min(255, baseGray + glowBoost + glowLevel * 10));
    Color crystalColor = {r, g, b, 255};

    Material mat = LoadMaterialDefault();
    mat.shader = shader;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = crystalColor;

    Matrix transform = MatrixScale(scale, scale, scale);
    DrawMesh(mesh, mat, transform);

    if (glowLevel > 0)
    {
        float glowScale = scale * (1.0f + glowLevel * 0.04f);
        unsigned char glowAlpha = static_cast<unsigned char>(20 + glowLevel * 18);
        Color glowColor = {210, 225, 255, glowAlpha};
        Material glowMat = LoadMaterialDefault();
        glowMat.shader = shader;
        glowMat.maps[MATERIAL_MAP_DIFFUSE].color = glowColor;
        Matrix glowTransform = MatrixScale(glowScale, glowScale, glowScale);
        DrawMesh(mesh, glowMat, glowTransform);
    }

    EndMode3D();
    EndTextureMode();
}

static Image DownscaleImage(Image src, int targetSize)
{
    Image resized = ImageCopy(src);
    ImageResize(&resized, targetSize, targetSize);
    return resized;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv)
{
    const char* outputDir = "../../src/assets/sprites/samples";
    if (argc > 1) outputDir = argv[1];

    printf("Crystal Sprite Generator\n");
    printf("Output: %s\n", outputDir);
    printf("Rendering %d families x %d shapes x %d sizes x %d glow levels = %d sprites\n",
           NUM_FAMILIES, SHAPES_PER_FAMILY, SIZE_LEVELS, GLOW_LEVELS,
           NUM_FAMILIES * SHAPES_PER_FAMILY * SIZE_LEVELS * GLOW_LEVELS);

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(RENDER_SIZE, RENDER_SIZE, "Crystal Generator");
    SetTargetFPS(60);

    Shader lightShader = LoadShaderFromMemory(VS_LIGHTING, FS_LIGHTING);
    if (lightShader.id == 0)
    {
        printf("ERROR: Failed to load lighting shader, falling back to default\n");
        lightShader = LoadShaderFromMemory(NULL, NULL);
    }

    Camera3D camera = {0};
    camera.position = {2.2f, 2.8f, 2.2f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 28.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(RENDER_SIZE, RENDER_SIZE);

    float sizeScales[] = {0.55f, 0.70f, 0.85f, 1.0f};

    int totalSprites = 0;

    for (int family = 0; family < NUM_FAMILIES; family++)
    {
        for (int shape = 0; shape < SHAPES_PER_FAMILY; shape++)
        {
            printf("  Generating %s/%s...\n", FAMILY_NAMES[family], SHAPE_NAMES[family][shape]);

            Mesh mesh = GENERATORS[family][shape]();

            for (int sizeLevel = 0; sizeLevel < SIZE_LEVELS; sizeLevel++)
            {
                for (int glow = 0; glow < GLOW_LEVELS; glow++)
                {
                    float scale = sizeScales[sizeLevel];
                    RenderCrystal(target, mesh, camera, lightShader, scale, glow);

                    // Read from render texture (flipped vertically by OpenGL)
                    Image img = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&img);

                    // Downscale to output size
                    Image output = DownscaleImage(img, OUTPUT_SIZE);

                    // Build output path
                    char dir[512], filepath[512];
                    snprintf(dir, sizeof(dir), "%s/%s/%s",
                             outputDir, FAMILY_NAMES[family], SHAPE_NAMES[family][shape]);
                    MkdirP(dir);

                    snprintf(filepath, sizeof(filepath), "%s/size_%d_glow_%d.png",
                             dir, sizeLevel + 1, glow);

                    ExportImage(output, filepath);
                    UnloadImage(output);
                    UnloadImage(img);

                    totalSprites++;
                }
            }

            UnloadMesh(mesh);
        }
    }

    UnloadRenderTexture(target);
    UnloadShader(lightShader);
    CloseWindow();

    printf("\nDone! Generated %d sprites.\n", totalSprites);
    return 0;
}
