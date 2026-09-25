// sect_art.cpp — see sect_art.h.
#include "sect_art.h"

#include "domeforge.h"
#include "terrain_synthesis.h"   // SECT_RING_ROAD_KM
#include "unit_icons.h"

#include <map>
#include <string>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // What the set is made of, in bake order: roads first (the most visible
    // missing piece), then the core, then every unit on, then every unit off.
    enum class Piece { ROADS, CORE, UNIT_ON, UNIT_OFF, CORE_HOVER, UNIT_ON_HOVER, UNIT_OFF_HOVER };

    struct Item
    {
        Piece piece;
        int slot;                // unit slot, game order
        DomeForgeJob job;
        bool started = false;
        Texture2D tex = {0};
        float cx = 0.0f, cy = 0.0f;   // sprite: dome centre inside the texture
    };

    struct Set
    {
        float pxPerKm = 0.0f;    // what this set was baked for; 0 = nothing yet
        int screenW = 0, screenH = 0;   // the road layer covers the whole screen
        DomeForgeConfig cfg;
        DomeForgeLayout lay;
        std::vector<Item> items;
        size_t next = 0;         // first item not finished
        double cpuMs = 0.0;      // time spent baking this set, for the log line
    } g_set;

    DomeForgeConfig BaseConfig() { return SectArt::BaseConfig(); }

    // Green = on, grey = off. A per-unit colour belongs here, keyed by slot;
    // it would also need adding to the bake list below.
    DomeForgeRgb DomeColour(const DomeForgeConfig& cfg, int /*slot*/, bool on)
    {
        return on ? cfg.cardinalColor : cfg.diagonalColor;
    }

    // DomeForge lays units out from the top going counter-clockwise (math
    // angles 90 + 45 j); the game counts them from the top going clockwise.
    int LayoutIndexForSlot(int slot) { return 1 + (SectArt::UNIT_SLOTS - slot) % SectArt::UNIT_SLOTS; }

    void UnloadSet()
    {
        for (Item& it : g_set.items)
            if (it.tex.id != 0) UnloadTexture(it.tex);
        g_set.items.clear();
        g_set.next = 0;
        g_set.pxPerKm = 0.0f;
        g_set.screenW = g_set.screenH = 0;
        g_set.cpuMs = 0.0;
    }

    // The same dome, lit up: brighter glass, more facets catching the light,
    // a bright lip and a stronger far-side rim. Baked as a second sprite and
    // cross-faded in on hover, so the change is DomeForge's own lighting.
    DomeForgeConfig HoverConfig(const DomeForgeConfig& base)
    {
        DomeForgeConfig c = base;
        c.ambient += 0.14;
        c.diffuse += 0.07;
        c.specInt = std::min(1.0, c.specInt + 0.2);
        c.litAmount += 0.25;
        c.litNear += 0.15;
        c.edgeLine += 0.35;
        c.rimInt += 0.25;
        return c;
    }

    int g_hoverOverride = -1;         // a tool can force the hovered slot

    // Hover animation: 0 = at rest, 1 = fully lit. Slots 0-7 are units, 8 the core.
    float g_hoverT[SectArt::UNIT_SLOTS + 1] = {};
    bool g_wantPointer = false;       // a clickable dome is under the pointer this frame
    bool g_pointerShown = false;      // what SetMouseCursor was last told

    void StartSet(int screenW, int screenH)
    {
        UnloadSet();
        const float pxPerKm = SectArt::SectViewPxPerKm(screenW, screenH);
        g_set.pxPerKm = pxPerKm;
        g_set.screenW = screenW;
        g_set.screenH = screenH;
        g_set.cfg = BaseConfig();
        const double s = SECT_RING_ROAD_KM * pxPerKm / g_set.cfg.ringRoadR;
        g_set.lay = DomeForgeMakeLayout(g_set.cfg, s * 1254.0 / g_set.cfg.baseSize);
        g_set.items.push_back({Piece::ROADS, -1});
        g_set.items.push_back({Piece::CORE, -1});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_ON, slot});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_OFF, slot});
        // hover variants last: the view is whole without them
        g_set.items.push_back({Piece::CORE_HOVER, -1});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_ON_HOVER, slot});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_OFF_HOVER, slot});
    }

    void Begin(Item& it)
    {
        const DomeForgeConfig& cfg = g_set.cfg;
        const DomeForgeLayout& lay = g_set.lay;
        if (it.piece == Piece::ROADS)
        {
            // The layer is screen-sized and shares the base's centre, so shift
            // the layout (math coords in the A x A square, y up) to match.
            const int W = g_set.screenW, H = g_set.screenH;
            const double ox = (W - lay.A) * 0.5, oy = (H - lay.A) * 0.5;
            std::vector<DomeForgePrim> prims = lay.prims;
            for (DomeForgePrim& p : prims)
            {
                p.ax += ox; p.bx += ox; p.cx += ox;
                p.ay += oy; p.by += oy; p.cy += oy;
            }
            std::vector<DomeForgeLight> lights = lay.lights;
            for (DomeForgeLight& l : lights)
            {
                l.x += ox;
                l.y += oy;
            }
            it.job = DomeForgeJob::Roads(cfg, W, H, prims, lay.s, lights);
        }
        else if (it.piece == Piece::CORE || it.piece == Piece::CORE_HOVER)
        {
            const DomeForgeDome& d = lay.domes[0];
            const DomeForgeConfig c = it.piece == Piece::CORE_HOVER ? HoverConfig(cfg) : cfg;
            it.job = DomeForgeJob::Sprite(DomeForgeSpriteConfig(c, lay, d, DomeColour(cfg, -1, true)),
                                          DomeForgeKind::CENTRAL);
        }
        else
        {
            const DomeForgeDome& d = lay.domes[LayoutIndexForSlot(it.slot)];
            const bool on = it.piece == Piece::UNIT_ON || it.piece == Piece::UNIT_ON_HOVER;
            const bool hover = it.piece == Piece::UNIT_ON_HOVER || it.piece == Piece::UNIT_OFF_HOVER;
            const DomeForgeConfig c = hover ? HoverConfig(cfg) : cfg;
            it.job = DomeForgeJob::Sprite(DomeForgeSpriteConfig(c, lay, d, DomeColour(cfg, it.slot, on)),
                                          DomeForgeKind::UNIT);
        }
        it.started = true;
    }

    void Finish(Item& it)
    {
        const DomeForgeImage& img = it.job.Image();
        Image im = {(void*)img.rgba.data(), img.width, img.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        it.tex = LoadTextureFromImage(im);
        SetTextureFilter(it.tex, TEXTURE_FILTER_BILINEAR);
        it.cx = (float)img.cx;
        it.cy = (float)img.cy;
        it.job = DomeForgeJob();   // the pixels live on the GPU now
    }

    const Item* Find(Piece piece, int slot)
    {
        for (const Item& it : g_set.items)
            if (it.piece == piece && it.slot == slot && it.tex.id != 0) return &it;
        return nullptr;
    }

    // Sprites are baked at exactly the size they are drawn, so place them on
    // whole pixels: a half-pixel offset would blur the hex lines.
    void DrawSprite(const Item& it, Vector2 domeCentre)
    {
        DrawTexture(it.tex, (int)std::floor(domeCentre.x - it.cx + 0.5f), (int)std::floor(domeCentre.y - it.cy + 0.5f), WHITE);
    }
}

namespace SectArt
{
    DomeForgeConfig BaseConfig()
    {
        DomeForgeConfig cfg = DomeForgeDefaults();
        // The layout centre is the sect centre, which is the terrain's centre;
        // DomeForge's offsetY only frames its own reference image.
        cfg.offsetY = 0.0;

        // Tuned by eye against the concept art (prototypes/dome-forge/samples/
        // base-compare-reference.png, left half), crop against crop at the same
        // scale -- see "Roads like the concept" in domeforge-study.md.
        // Proportions: smaller domes, so more road shows between them.
        cfg.unitSize = 205.0;
        cfg.centralSize = 420.0;
        // Roads: smooth, darker asphalt; a thin, soft kerb instead of a bright
        // segmented bevel; a softer bank; a faint lane line.
        cfg.roadW = 34.0;
        cfg.roadColor = DomeForgeHex("#5d5b57");
        cfg.roadMottle = 0.04;
        cfg.roadGrain = 0.025;
        cfg.curbW = 3.0;
        cfg.curbBevel = 0.6;
        cfg.curbShine = 0.15;
        cfg.curbColor = DomeForgeHex("#8f8b85");
        cfg.curbOutline = 0.35;
        cfg.curbShadow = 0.25;
        cfg.curbSegDepth = 0.12;
        cfg.bankW = 4.0;
        cfg.bankLight = 0.15;
        cfg.bankShadow = 0.25;
        cfg.laneColor = DomeForgeHex("#9a8f7e");
        cfg.laneAlpha = 0.3;
        cfg.laneW = 1.2;
        // Junctions flare: spokes into the ring, and into each dome's collar.
        cfg.fillet = 34.0;
        cfg.filletDome = 30.0;
        // One exit road, north, leaving the ring and fading into the ground.
        cfg.spokesBeyond = true;
        cfg.exitRoads = 1;          // N only (bits N=1 W=2 S=4 E=8)
        cfg.roadOuterW = 26.0;
        cfg.spokesBeyondLen = 62.0;
        cfg.exitFade = 40.0;
        // Roads meet a collar of road round each dome, not socket loops.
        cfg.socketOn = false;
        cfg.domeCollar = 10.0;
        // Glass (the user, against the concept): stronger curvature -- big cells
        // in the middle shrinking smoothly toward the rim -- and much less
        // shadow round the edge. The curvature comes from the perspective lens,
        // not hexCurve: hexCurve is a power law, and above ~1.5 it balloons the
        // centre cell and drops straight to slivers.
        cfg.hexLens = 0.85;
        cfg.hexCells = 0.08;
        cfg.edgeShadow = 0.06;
        cfg.edgeShadowW = 0.12;
        cfg.limbDark = 0.2;
        // Lights sit on the roads: bars on the centre line, lamps round the collars.
        cfg.roadLights = true;
        cfg.roadLightLen = 30.0;
        // A light is a crisp core, white-hot along its centre line, a tight
        // glow, and a wide faint bokeh halo spilling onto the road.
        cfg.roadLightW = 3.2;
        cfg.roadLightGlowR = 4.0;
        cfg.roadLightGlow = 0.55;
        cfg.roadLightBloom = 0.22;
        cfg.roadLightBloomR = 26.0;
        cfg.roadLightHot = 0.85;
        cfg.ringLights = 8;
        cfg.collarLights = 8;
        cfg.coreCollarLights = 16;
        cfg.collarLightSize = 5.0;
        return cfg;
    }

    float SectViewPxPerKm(int screenW, int screenH)
    {
        return (float)std::max(screenW, screenH) / 5.0f;
    }

    Frame MakeFrame(Vector2 center, float pxPerKm)
    {
        const DomeForgeConfig cfg = BaseConfig();
        const double s = SECT_RING_ROAD_KM * pxPerKm / cfg.ringRoadR;
        const DomeForgeLayout lay = DomeForgeMakeLayout(cfg, s * 1254.0 / cfg.baseSize);
        Frame f;
        f.center = center;
        f.pxPerKm = pxPerKm;
        f.s = (float)s;
        const double cX = lay.domes[0].x, cY = lay.domes[0].y;
        for (int slot = 0; slot < UNIT_SLOTS; slot++)
        {
            const DomeForgeDome& d = lay.domes[LayoutIndexForSlot(slot)];
            f.unit[slot] = {center.x + (float)(d.x - cX), center.y - (float)(d.y - cY)};   // math y up -> screen y down
        }
        const int unitSize = std::max(32, (int)std::floor(lay.domes[1].size + 0.5));
        const int coreSize = std::max(32, (int)std::floor(lay.domes[0].size + 0.5));
        f.unitDomeR = (float)(cfg.unit.domeRadius * unitSize);
        f.unitRimR = (float)((cfg.unit.domeRadius + cfg.unit.ringWidth) * unitSize);
        f.coreDomeR = (float)(cfg.central.domeRadius * coreSize);
        f.coreRimR = (float)((cfg.central.domeRadius + cfg.central.ringWidth) * coreSize);
        f.collar = (float)(cfg.domeCollar * s);
        f.ringRoadR = (float)(cfg.ringRoadR * s);
        f.ringRoadOuterR = (float)((cfg.ringRoadR + cfg.roadW * 0.5 + cfg.bankW) * s);
        return f;
    }

    void Update(double budgetMs)
    {
        // The pointer: AnimateHover sets the hand as the sect view draws. This
        // runs once a frame in every view (before drawing), so once the sect
        // view stops drawing -- the player left it -- the arrow comes back.
        if (g_wantPointer != g_pointerShown)
        {
            SetMouseCursor(g_wantPointer ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
            g_pointerShown = g_wantPointer;
        }
        g_wantPointer = false;

        if (g_set.screenW != GetScreenWidth() || g_set.screenH != GetScreenHeight())
            StartSet(GetScreenWidth(), GetScreenHeight());
        while (g_set.next < g_set.items.size() && budgetMs > 0.0)
        {
            Item& it = g_set.items[g_set.next];
            const double t0 = GetTime();
            if (!it.started) Begin(it);
            if (it.job.Step(budgetMs))
            {
                Finish(it);
                g_set.next++;
            }
            const double spent = (GetTime() - t0) * 1000.0;
            budgetMs -= spent;
            g_set.cpuMs += spent;
            if (g_set.next == g_set.items.size())
                TraceLog(LOG_INFO, "SectArt: baked %d pieces for %.0f px/km in %.0f ms of CPU",
                         (int)g_set.items.size(), g_set.pxPerKm, g_set.cpuMs);
        }
    }

    void BakeAll() { Update(1e30); }

    bool Ready()
    {
        return g_set.screenW == GetScreenWidth() && g_set.screenH == GetScreenHeight() &&
               g_set.next >= g_set.items.size();
    }

    void DrawBase(const Frame& f)
    {
        const Item* it = Find(Piece::ROADS, -1);
        if (!it) return;
        // The road layer is screen-sized and centred on the sect centre.
        DrawTexture(it->tex, (int)std::floor(f.center.x - it->tex.width * 0.5f + 0.5f),
                    (int)std::floor(f.center.y - it->tex.height * 0.5f + 0.5f), WHITE);
    }

    // The lit sprite over the resting one, at the slot's animation level.
    static void DrawHoverLayer(Piece piece, int slot, int t, Vector2 at)
    {
        const float k = g_hoverT[t];
        if (k <= 0.002f) return;
        const Item* it = Find(piece, slot);
        if (!it) return;
        const float e = k * k * (3.0f - 2.0f * k);   // smoothstep: eases in and out
        DrawTexture(it->tex, (int)std::floor(at.x - it->cx + 0.5f), (int)std::floor(at.y - it->cy + 0.5f),
                    Fade(WHITE, e));
    }

    void DrawCore(const Frame& f)
    {
        const Item* it = Find(Piece::CORE, -1);
        if (it) DrawSprite(*it, f.center);
        else DrawCircleV(f.center, f.coreDomeR, Color{40, 46, 44, 255});
        DrawHoverLayer(Piece::CORE_HOVER, -1, CORE_SLOT, f.center);
    }

    void DrawUnitDome(const Frame& f, int slot, bool on)
    {
        if (slot < 0 || slot >= UNIT_SLOTS) return;
        const Item* it = Find(on ? Piece::UNIT_ON : Piece::UNIT_OFF, slot);
        if (it) DrawSprite(*it, f.unit[slot]);
        else DrawCircleV(f.unit[slot], f.unitDomeR, Color{40, 46, 44, 255});
        DrawHoverLayer(on ? Piece::UNIT_ON_HOVER : Piece::UNIT_OFF_HOVER, slot, slot, f.unit[slot]);
    }

    void AnimateHover(int hovered, float dt)
    {
        // ~150 ms to light up, a little slower to settle back
        for (int i = 0; i <= UNIT_SLOTS; i++)
        {
            const float target = (i == hovered) ? 1.0f : 0.0f;
            const float rate = target > g_hoverT[i] ? 14.0f : 9.0f;
            g_hoverT[i] += (target - g_hoverT[i]) * (1.0f - std::exp(-rate * dt));
            if (g_hoverOverride >= 0) g_hoverT[i] = target;   // a still frame shows the end state
        }
        // Units open on click; the core has nothing to open, so it keeps the arrow.
        g_wantPointer = hovered >= 0 && hovered < UNIT_SLOTS;
        if (g_wantPointer != g_pointerShown)
        {
            SetMouseCursor(g_wantPointer ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
            g_pointerShown = g_wantPointer;
        }
    }

    // ---- icons ----
    namespace
    {
        std::map<std::string, Texture2D> g_icons;   // key: type + size + on
        std::map<int, Font> g_fonts;                 // Exo 2 Bold, by px
    }

    void DrawUnitIcon(const Frame& f, int slot, const std::string& unitType, bool on)
    {
        if (slot < 0 || slot >= UNIT_SLOTS) return;
        const std::string key = UnitIconKey(unitType);
        if (key.empty()) return;
        const int px = std::max(12, (int)std::lround(f.unitDomeR * 1.12f));
        const std::string ck = key + ":" + std::to_string(px) + (on ? ":1" : ":0");
        auto it = g_icons.find(ck);
        if (it == g_icons.end())
        {
            // near-white ink with a soft dark shadow; a touch dimmer when off
            const Color ink = on ? Color{244, 252, 246, 240} : Color{226, 230, 234, 220};
            Image img = UnitIconImage(key, px, ink, Color{0, 0, 0, 130});
            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
            it = g_icons.emplace(ck, tex).first;
        }
        const Texture2D& t = it->second;
        const Vector2 c = f.unit[slot];
        DrawTexture(t, (int)std::floor(c.x - t.width * 0.5f + 0.5f), (int)std::floor(c.y - t.height * 0.5f + 0.5f), WHITE);
    }

    int HoveredSlot(const Frame& f, Vector2 mouse)
    {
        if (g_hoverOverride >= 0) return g_hoverOverride;
        for (int i = 0; i < UNIT_SLOTS; i++)
        {
            const float dx = mouse.x - f.unit[i].x, dy = mouse.y - f.unit[i].y;
            if (dx * dx + dy * dy <= f.unitRimR * f.unitRimR) return i;
        }
        const float dx = mouse.x - f.center.x, dy = mouse.y - f.center.y;
        if (dx * dx + dy * dy <= f.coreRimR * f.coreRimR) return CORE_SLOT;
        return -1;
    }

    void SetHoverOverride(int slot) { g_hoverOverride = slot; }

    // The label face, loaded at twice the drawn size with mipmaps and
    // trilinear filtering, so small text stays continuous (raylib's default
    // font is a pixel font, which is what looked broken). One per face + size.
    static const Font& LabelFont(int px)
    {
        auto it = g_fonts.find(px);
        if (it != g_fonts.end()) return it->second;
        Font font = LoadFontEx("src/assets/fonts/Exo2-Bold.ttf", px * 2, nullptr, 0);
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        return g_fonts.emplace(px, font).first->second;
    }

    void DrawTextCentred(const std::string& text, Vector2 centre, int px, Color colour)
    {
        const Font& font = LabelFont(px);
        const Vector2 ts = MeasureTextEx(font, text.c_str(), (float)px, 0.5f);
        const Vector2 at = {std::floor(centre.x - ts.x * 0.5f + 0.5f), std::floor(centre.y - ts.y * 0.5f + 0.5f)};
        DrawTextEx(font, text.c_str(), {at.x + 1.0f, at.y + 1.0f}, (float)px, 0.5f, Fade(BLACK, 0.55f));
        DrawTextEx(font, text.c_str(), at, (float)px, 0.5f, colour);
    }

    void DrawUnitLabel(const Frame& f, int slot, const std::string& name, bool on)
    {
        if (slot < 0 || slot >= UNIT_SLOTS) return;
        const int px = 16;
        const Font& font = LabelFont(px);
        const float size = (float)px, spacing = 0.5f;
        const Vector2 ts = MeasureTextEx(font, name.c_str(), size, spacing);
        const Vector2 c = f.unit[slot];
        const float padX = 9.0f, padY = 4.0f;
        const float y = c.y + f.unitRimR + f.collar + 5.0f;
        const Rectangle box = {c.x - ts.x * 0.5f - padX, y, ts.x + 2 * padX, ts.y + 2 * padY};
        const Color accent = on ? Color{46, 200, 110, 255} : Color{150, 156, 162, 255};
        DrawRectangleRounded(box, 0.5f, 8, Color{14, 18, 20, 215});
        DrawRectangleRoundedLinesEx(box, 0.5f, 8, 1.0f, Fade(accent, 0.85f));
        DrawTextEx(font, name.c_str(), {std::floor(box.x + padX + 0.5f), std::floor(y + padY + 0.5f)}, size, spacing,
                   Color{236, 242, 238, 255});
    }

    void Unload()
    {
        UnloadSet();
        for (auto& kv : g_icons) UnloadTexture(kv.second);
        g_icons.clear();
        for (auto& kv : g_fonts) UnloadFont(kv.second);
        g_fonts.clear();
    }
}
