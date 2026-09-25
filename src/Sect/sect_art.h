// sect_art.h — the sect view's base art: DomeForge (src/DomeForge/), baked to
// textures and placed on the ground at its real size.
//
// Every sect is the same base, so ONE set is baked for the whole game, not
// one per sect: the road layer, the core dome, and each unit dome on and off.
// Baking a set is ~1 s of CPU at 1280x720 in an optimised build and several
// in a debug one, so it runs a slice per frame from the first frame on
// (Update), long before the player reaches a sect. A tool that renders one
// frame calls BakeAll first.
//
// Scale: the ring road's centre line sits SECT_RING_ROAD_KM from the sect
// centre on the same ground DrawSectTerrainBackground draws, which is also
// where SectLevelSite levels the terrain. Art and ground agree by
// construction.
//
// Colour: green glass = on, grey = off (DomeColour). A unit's own colour
// would go there too; nothing sets one yet.
#ifndef SECT_ART_H
#define SECT_ART_H

#include "raylib.h"
#include "domeforge.h"

namespace SectArt
{
    constexpr int UNIT_SLOTS = 8;

    // Where things are on screen for a sect drawn around `center`.
    struct Frame
    {
        Vector2 center;
        float pxPerKm;
        float s;                        // screen px per DomeForge base px
        Vector2 unit[UNIT_SLOTS];       // unit dome centres, game order: top, then clockwise
        float unitDomeR;                // glass radius, px
        float unitRimR;                 // outer edge of the rim, px (hit radius)
        float coreDomeR;
        float coreRimR;                 // outer edge of the core's rim, px
        float collar;                   // road collar round every rim, px
        float ringRoadR;                // ring road centre line, px
        float ringRoadOuterR;           // its outer kerb and bank, px
    };

    // The DomeForge config the sect view bakes: DomeForge's defaults tuned to
    // the concept art (proportions, roads, collars, lights).
    DomeForgeConfig BaseConfig();

    // The sect view's ground scale: DrawSectTerrainBackground cover-fits the
    // square 5 km tile, so the longer screen side spans 5 km.
    float SectViewPxPerKm(int screenW, int screenH);
    Frame MakeFrame(Vector2 center, float pxPerKm);

    // Advance the bake by about budgetMs. Call every frame; free once done.
    // Rebakes on its own if the screen size (and so pxPerKm) changes.
    void Update(double budgetMs);
    void BakeAll();          // finish now (tools that render a single frame)
    bool Ready();            // the whole set is baked for the current screen

    // Draw calls fall back to a plain disc for anything not baked yet.
    void DrawBase(const Frame& f);                   // roads, kerbs, lanes
    void DrawCore(const Frame& f);
    void DrawUnitDome(const Frame& f, int slot, bool on);

    void Unload();
}

#endif // SECT_ART_H
