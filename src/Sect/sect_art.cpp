// sect_art.cpp — see sect_art.h.
#include "sect_art.h"

#include "domeforge.h"
#include "terrain_synthesis.h"   // SECT_RING_ROAD_KM

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // What the set is made of, in bake order: roads first (the most visible
    // missing piece), then the core, then every unit on, then every unit off.
    enum class Piece { ROADS, CORE, UNIT_ON, UNIT_OFF };

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

    void StartSet(int screenW, int screenH)
    {
        UnloadSet();
        const float pxPerKm = SectArt::SectViewPxPerKm(screenW, screenH);
        g_set.pxPerKm = pxPerKm;
        g_set.screenW = screenW;
        g_set.screenH = screenH;
        g_set.cfg = BaseConfig();
        const double s = SECT_RING_ROAD_KM * pxPerKm / g_set.cfg.ringRoadR;
        // The cardinal roads leave the base and run off the edges of the
        // screen, as they run off the edges of the concept art.
        g_set.cfg.spokesBeyondLen = (std::max(screenW, screenH) * 0.5 / s - g_set.cfg.ringRoadR) + 40.0;
        g_set.lay = DomeForgeMakeLayout(g_set.cfg, s * 1254.0 / g_set.cfg.baseSize);
        g_set.items.push_back({Piece::ROADS, -1});
        g_set.items.push_back({Piece::CORE, -1});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_ON, slot});
        for (int slot = 0; slot < SectArt::UNIT_SLOTS; slot++) g_set.items.push_back({Piece::UNIT_OFF, slot});
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
        else if (it.piece == Piece::CORE)
        {
            const DomeForgeDome& d = lay.domes[0];
            it.job = DomeForgeJob::Sprite(DomeForgeSpriteConfig(cfg, lay, d, DomeColour(cfg, -1, true)),
                                          DomeForgeKind::CENTRAL);
        }
        else
        {
            const DomeForgeDome& d = lay.domes[LayoutIndexForSlot(it.slot)];
            it.job = DomeForgeJob::Sprite(DomeForgeSpriteConfig(cfg, lay, d, DomeColour(cfg, it.slot, it.piece == Piece::UNIT_ON)),
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
        // Short road stubs past the ring at the four cardinal points.
        cfg.spokesBeyond = true;
        cfg.roadOuterW = 26.0;
        cfg.spokesBeyondLen = 60.0;
        // Roads meet a collar of road round each dome, not socket loops.
        cfg.socketOn = false;
        cfg.domeCollar = 10.0;
        // Lights sit on the roads: bars on the centre line, lamps round the collars.
        cfg.roadLights = true;
        cfg.roadLightLen = 30.0;
        cfg.roadLightW = 2.5;
        cfg.roadLightGlowR = 5.0;
        cfg.roadLightGlow = 0.8;
        cfg.ringLights = 8;
        cfg.collarLights = 8;
        cfg.coreCollarLights = 16;
        cfg.collarLightSize = 3.0;
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

    void DrawCore(const Frame& f)
    {
        const Item* it = Find(Piece::CORE, -1);
        if (it) DrawSprite(*it, f.center);
        else DrawCircleV(f.center, f.coreDomeR, Color{40, 46, 44, 255});
    }

    void DrawUnitDome(const Frame& f, int slot, bool on)
    {
        if (slot < 0 || slot >= UNIT_SLOTS) return;
        const Item* it = Find(on ? Piece::UNIT_ON : Piece::UNIT_OFF, slot);
        if (it) DrawSprite(*it, f.unit[slot]);
        else DrawCircleV(f.unit[slot], f.unitDomeR, Color{40, 46, 44, 255});
    }

    void Unload() { UnloadSet(); }
}
