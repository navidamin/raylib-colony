#ifndef REGION_IDENTITY_H
#define REGION_IDENTITY_H

// Who a place on the Moon is, for the region card.
//
// Design: docs/design/site-selection/site-selection-master-design.md SS4.5
// -- regions are NAMED and REAL. The named feature comes from
// src/assets/planet/zones.json (lunar_regions.*), the terrane from a traced
// outline of the Procellarum KREEP Terrane, and mare-or-highland from the
// feature's own record where there is one (a mare is a mare; a crater is
// what its dominant rock says) and from the real elevation otherwise: mare
// floors sit 2-3 km below the reference radius. Composition is the
// feature's where the dataset carries it and derived from the ground type
// otherwise, so an unnamed basalt plain reads as basalt.
//
// Pure: no GL, no game state. Shared by the game and lunar_map.

#include "raylib.h"
#include "game_enums.h"

class LolaDem;

struct RegionIdentity
{
    char name[64] = "";
    const char* terrane = "";
    SiteArchetype archetype = SiteArchetype::MIXED;
    const char* rock = "";
    float fePct = 0.0f;
    float tiPct = 0.0f;
    float thPpm = 0.0f;
    char latitudeNote[96] = "";
    bool isMare = false;
    int featureIndex = -1;         // into GetLunarRegions(); -1 = unnamed ground
};

// dem may be nullptr: the ground then reads as highland everywhere, which
// is the honest fallback (most of the Moon is).
RegionIdentity IdentifyRegion(const LolaDem* dem, double latDeg, double lonDeg);

// The Procellarum KREEP Terrane outline, traced from Jolliff, Gillis &
// Haskin (2000) Fig. 1. Test-grade, not survey-grade.
bool InProcellarumKreepTerrane(double latDeg, double lonDeg);

// The strategy tag each archetype is, for the chip on the card and the
// one-line trade it stands for (master design SS2.1).
struct SiteArchetypeDescriptor
{
    SiteArchetype archetype;
    const char* name;          // "MARE INDUSTRIAL"
    Color tint;
    const char* gives;         // what you get
    const char* costs;         // what you give up
};

const SiteArchetypeDescriptor& GetSiteArchetypeDescriptor(SiteArchetype archetype);

#endif // REGION_IDENTITY_H
