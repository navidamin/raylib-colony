// Sample sprite render harness
//
// End-to-end test of the sprite pipeline that powers the prospecting tray.
// Loads the same sprite assets the game does, then renders three layouts to
// a single PNG using the *same* raylib calls (LoadTexture, SetTextureFilter,
// DrawTexturePro with a tint Color) that RenderManager::DrawSampleSprite uses
// in production. The output verifies, in one image, that:
//
//   1. Sprites load from src/assets/sprites/samples/<family>/t<n>/...
//   2. Tinting via DrawTexturePro colorizes the white wireframe
//   3. The 5 glow levels visibly progress
//   4. The 5 templates within a family are distinguishable
//   5. The 4 families have distinct silhouettes
//   6. A realistic mock tray (mixed family/template/size/glow/color) renders
//      all sample slots correctly with state markers (PROCESSING / COMPLETED)
//
// Output: tools/sample_render_test/output.png

#include "raylib.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// Configuration
// ============================================================

static const int CANVAS_W = 1280;
static const int CANVAS_H = 1100;

// Background matches the game's prospecting panel dark background.
static const Color CANVAS_BG = { 18, 22, 32, 255 };
static const Color SLOT_BG   = { 30, 30, 50, 255 };
static const Color BORDER_LO = { 70, 80, 100, 255 };
static const Color BORDER_HI = { 130, 150, 180, 255 };
static const Color BORDER_PROCESSING = { 220, 165, 60, 255 };  // PROS_MSG_ALERT-ish
static const Color BORDER_COMPLETED  = { 80, 200, 120, 255 };  // EXT_ACCENT_GREEN-ish
static const Color TEXT_LIGHT = { 220, 225, 235, 255 };
static const Color TEXT_DIM   = { 150, 160, 175, 255 };

// Family / template sprite cache, indexed exactly the way the game does:
// sprites[family][template][sizeLevelIdx][glowLevel].
// family order matches the in-game ShapeFamily enum:
//   0 ANGULAR_CHUNKS    -> angular
//   1 CRYSTALLINE_SHARDS -> shard
//   2 ROUNDED_NODULES   -> rounded
//   3 LAYERED_SLABS     -> slab
static const char* FAMILY_NAMES[4] = { "angular", "shard", "rounded", "slab" };
static Texture2D sprites[4][5][4][5];

// Element colour table mirrors src/Prospecting/prospecting_types.cpp::GetElementColor.
struct ElementSwatch
{
    const char* label;
    Color color;
};
static const ElementSwatch ELEMENTS[] = {
    { "Fe",    { 181,  70,  60, 255 } },
    { "Ti",    { 160, 176, 192, 255 } },
    { "Si",    { 212, 168,  80, 255 } },
    { "Al",    { 192, 192, 200, 255 } },
    { "Ca",    { 232, 220, 192, 255 } },
    { "WATER", {  68, 136, 204, 255 } },
    { "H2",    { 136, 204, 238, 255 } },
    { "O2",    {  85, 170, 153, 255 } },
};
static const int NUM_ELEMENTS = sizeof(ELEMENTS) / sizeof(ELEMENTS[0]);

// ============================================================
// Sprite cache load — mirrors RenderManager::LoadSampleSprites
// ============================================================

static int LoadAllSprites(const char* baseDir)
{
    int loaded = 0;
    int missing = 0;
    char path[512];
    for (int f = 0; f < 4; f++)
    {
        for (int t = 0; t < 5; t++)
        {
            for (int s = 0; s < 4; s++)
            {
                for (int g = 0; g < 5; g++)
                {
                    snprintf(path, sizeof(path),
                             "%s/%s/t%d/size_%d_glow_%d.png",
                             baseDir, FAMILY_NAMES[f], t + 1, s + 1, g);
                    Texture2D tex = LoadTexture(path);
                    if (tex.id != 0)
                    {
                        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                        loaded++;
                    }
                    else
                    {
                        missing++;
                        if (missing < 4)
                            printf("  MISS: %s\n", path);
                    }
                    sprites[f][t][s][g] = tex;
                }
            }
        }
    }
    if (missing > 0)
        printf("Loaded %d sprites, %d missing\n", loaded, missing);
    else
        printf("Loaded all %d sprites\n", loaded);
    return loaded;
}

static void UnloadAllSprites()
{
    for (int f = 0; f < 4; f++)
        for (int t = 0; t < 5; t++)
            for (int s = 0; s < 4; s++)
                for (int g = 0; g < 5; g++)
                    if (sprites[f][t][s][g].id != 0)
                        UnloadTexture(sprites[f][t][s][g]);
}

// Identical signature/body to RenderManager::DrawSampleSprite — verifies the
// production code path renders correctly into a render texture.
static void DrawSampleSprite(int family, int templateIdx, int sizeLevelIdx,
                              int glowLevel, Color tint, Rectangle slot)
{
    family       = std::clamp(family,       0, 3);
    templateIdx  = std::clamp(templateIdx,  0, 4);
    sizeLevelIdx = std::clamp(sizeLevelIdx, 0, 3);
    glowLevel    = std::clamp(glowLevel,    0, 4);
    Texture2D tex = sprites[family][templateIdx][sizeLevelIdx][glowLevel];
    if (tex.id == 0) return;
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    DrawTexturePro(tex, src, slot, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
}

// Mirror of GetGlowLevel for the harness.
static int HarnessGlowLevel(float confidence)
{
    confidence = std::clamp(confidence, 0.0f, 1.0f);
    if (confidence <= 0.20f) return 0;
    if (confidence <= 0.40f) return 1;
    if (confidence <= 0.60f) return 2;
    if (confidence <= 0.80f) return 3;
    return 4;
}
static int HarnessSizeLevelIdx(float richness)
{
    if (richness < 0.25f) return 0;
    if (richness < 0.50f) return 1;
    if (richness < 0.75f) return 2;
    return 3;
}

// ============================================================
// Layout sections
// ============================================================

static void DrawTrayCell(Rectangle slot, int family, int tpl, int sizeIdx,
                          int glow, Color tint, int state /* 0=in_tray 1=processing 2=completed */)
{
    DrawRectangleRec(slot, SLOT_BG);
    DrawSampleSprite(family, tpl, sizeIdx, glow, tint, slot);
    if (state == 1)
        DrawRectangleLinesEx(slot, 2.0f, BORDER_PROCESSING);
    else if (state == 2)
        DrawRectangleLinesEx(slot, 2.0f, BORDER_COMPLETED);
    else
        DrawRectangleLinesEx(slot, 1.0f, BORDER_LO);
}

// Section 1 — confidence ramp: same family/template/size, glow 0..4, single tint.
static void DrawConfidenceRamp(int x, int y)
{
    DrawText("CONFIDENCE RAMP   live glow recompute as confidence rises",
             x, y, 18, TEXT_LIGHT);
    DrawText("(family=rounded, template=t1, size=4, tint=H2)",
             x, y + 22, 12, TEXT_DIM);

    const int slot = 96;
    const int gap = 10;
    int sx = x;
    int sy = y + 50;

    // Five slots showing increasing confidence.
    float confidences[5] = { 0.05f, 0.30f, 0.50f, 0.70f, 0.95f };
    const char* labels[5]   = { "5%", "30%", "50%", "70%", "95%" };
    for (int i = 0; i < 5; i++)
    {
        Rectangle s = { (float)(sx + i * (slot + gap)), (float)sy, (float)slot, (float)slot };
        int g = HarnessGlowLevel(confidences[i]);
        DrawTrayCell(s, /*family=*/2, /*tpl=*/0, /*sizeIdx=*/3, g,
                     ELEMENTS[6].color /* H2 light blue */, /*state=*/0);
        char buf[32];
        snprintf(buf, sizeof(buf), "%s -> g%d", labels[i], g);
        DrawText(buf, (int)s.x + 4, (int)(s.y + s.height + 4), 12, TEXT_DIM);
    }
}

// Section 2 — family x template grid: every (family, template) pair at glow 2.
static void DrawFamilyTemplateGrid(int x, int y)
{
    DrawText("FAMILY x TEMPLATE GRID   4 families x 5 templates, glow=2, neutral white",
             x, y, 18, TEXT_LIGHT);
    DrawText("rows: angular / shard / rounded / slab",
             x, y + 22, 12, TEXT_DIM);

    const int slot = 96;
    const int gap = 10;
    int sx = x + 80;  // leave room for row labels
    int sy = y + 50;

    const Color WHITE_TINT = { 255, 255, 255, 255 };
    for (int f = 0; f < 4; f++)
    {
        DrawText(FAMILY_NAMES[f], x, sy + f * (slot + gap) + slot / 2 - 6, 14, TEXT_DIM);
        for (int t = 0; t < 5; t++)
        {
            Rectangle s = { (float)(sx + t * (slot + gap)),
                            (float)(sy + f * (slot + gap)),
                            (float)slot, (float)slot };
            DrawTrayCell(s, f, t, /*sizeIdx=*/3, /*glow=*/2, WHITE_TINT, /*state=*/0);
            if (f == 0)
            {
                char buf[8]; snprintf(buf, sizeof(buf), "t%d", t + 1);
                DrawText(buf, (int)s.x + slot / 2 - 8, (int)s.y - 16, 12, TEXT_DIM);
            }
        }
    }
}

// Section 3 — element tint demo: same shape, all element colours.
static void DrawTintDemo(int x, int y)
{
    DrawText("ELEMENT TINT DEMO   identical sprite, runtime DrawTexturePro tint",
             x, y, 18, TEXT_LIGHT);
    DrawText("(family=rounded t1 size=4 glow=3)", x, y + 22, 12, TEXT_DIM);

    const int slot = 88;
    const int gap = 10;
    int sx = x;
    int sy = y + 50;

    for (int i = 0; i < NUM_ELEMENTS; i++)
    {
        Rectangle s = { (float)(sx + i * (slot + gap)), (float)sy, (float)slot, (float)slot };
        DrawTrayCell(s, /*family=*/2, /*tpl=*/0, /*sizeIdx=*/3, /*glow=*/3,
                     ELEMENTS[i].color, /*state=*/0);
        DrawText(ELEMENTS[i].label, (int)s.x + 4, (int)(s.y + s.height + 4), 12, TEXT_DIM);
    }
}

// Section 4 — mock tray: 16 slots arranged 4x4 with mixed visuals + state markers.
// Mimics what a real prospecting tray looks like at runtime.
static void DrawMockTray(int x, int y)
{
    DrawText("MOCK TRAY   16 slots, mixed family/template/size/glow + state markers",
             x, y, 18, TEXT_LIGHT);
    DrawText("orange border = PROCESSING, green border = COMPLETED",
             x, y + 22, 12, TEXT_DIM);

    const int slot = 80;
    const int gap = 6;
    int sx = x;
    int sy = y + 50;

    // Pseudo-random but deterministic mix to simulate a real tray.
    struct Mock { int family; int tpl; float richness; float conf; int elemIdx; int state; };
    static const Mock fixtures[16] = {
        { 0, 2, 0.85f, 0.10f, 0, 0 },  // angular t3, big, low-conf, Fe
        { 2, 0, 0.55f, 0.45f, 6, 0 },  // rounded t1, mid, mid-conf, H2
        { 1, 4, 0.30f, 0.95f, 5, 2 },  // shard t5 small, certain, WATER, COMPLETED
        { 3, 1, 0.70f, 0.65f, 2, 1 },  // slab t2, big, high, Si, PROCESSING
        { 2, 3, 0.20f, 0.80f, 7, 0 },  // rounded t4, tiny, certain, O2
        { 0, 0, 0.60f, 0.05f, 0, 0 },  // angular t1, mid, none, Fe
        { 1, 1, 0.95f, 0.55f, 4, 0 },  // shard t2, biggest, mid, Ca
        { 3, 4, 0.45f, 0.30f, 3, 0 },  // slab t5, mid, low, Al
        { 2, 2, 0.75f, 0.70f, 6, 1 },  // rounded t3, big, high, H2, PROC
        { 0, 4, 0.40f, 0.90f, 0, 2 },  // angular t5, mid, certain, Fe, DONE
        { 1, 3, 0.55f, 0.20f, 5, 0 },  // shard t4, mid, low, WATER
        { 3, 0, 0.85f, 0.50f, 2, 0 },  // slab t1, big, mid, Si
        { 0, 1, 0.30f, 0.75f, 1, 0 },  // angular t2, small, high, Ti
        { 2, 4, 0.65f, 0.25f, 4, 0 },  // rounded t5, mid, low, Ca
        { 1, 0, 0.50f, 0.85f, 7, 0 },  // shard t1, mid, certain, O2
        { 3, 2, 0.20f, 0.40f, 6, 0 },  // slab t3, small, low, H2
    };

    for (int i = 0; i < 16; i++)
    {
        int row = i / 4;
        int col = i % 4;
        Rectangle s = { (float)(sx + col * (slot + gap)),
                        (float)(sy + row * (slot + gap)),
                        (float)slot, (float)slot };
        const Mock& m = fixtures[i];
        int sizeIdx = HarnessSizeLevelIdx(m.richness);
        int glow    = HarnessGlowLevel(m.conf);
        DrawTrayCell(s, m.family, m.tpl, sizeIdx, glow,
                     ELEMENTS[m.elemIdx].color, m.state);
    }
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv)
{
    const char* spriteDir = "../../src/assets/sprites/samples";
    const char* outputPath = "output.png";
    if (argc > 1) spriteDir = argv[1];
    if (argc > 2) outputPath = argv[2];

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(CANVAS_W, CANVAS_H, "Sample Render Test");

    int loaded = LoadAllSprites(spriteDir);
    if (loaded != 400)
    {
        printf("FAIL: expected 400 sprites, got %d. Run regolith_gen first.\n", loaded);
        CloseWindow();
        return 1;
    }

    RenderTexture2D target = LoadRenderTexture(CANVAS_W, CANVAS_H);

    BeginTextureMode(target);
    ClearBackground(CANVAS_BG);

    DrawText("Regolith sample sprite render harness",
             24, 18, 22, TEXT_LIGHT);
    DrawText("Exercises the production load + DrawTexturePro tint path with mock samples.",
             24, 44, 13, TEXT_DIM);

    DrawConfidenceRamp(24, 80);
    DrawFamilyTemplateGrid(24, 280);
    DrawTintDemo(24, 760);
    DrawMockTray(720, 280);

    // Footer
    char footer[128];
    snprintf(footer, sizeof(footer),
             "Loaded %d sprites from %s", loaded, spriteDir);
    DrawText(footer, 24, CANVAS_H - 28, 13, TEXT_DIM);

    EndTextureMode();

    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img);
    ExportImage(img, outputPath);
    UnloadImage(img);

    printf("Wrote %s (%dx%d)\n", outputPath, CANVAS_W, CANVAS_H);

    UnloadRenderTexture(target);
    UnloadAllSprites();
    CloseWindow();
    return 0;
}
