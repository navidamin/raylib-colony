#include "region_identity.h"

#include "lola_dem.h"
#include "lunar_regions.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

// PKT outline, lat/lon vertices. Everything else on the near side is FHT
// for this purpose; SPA is essentially a far-side terrane.
const double PKT_POLY[][2] =
{
    { 52, -72 }, { 57, -45 }, { 52, -20 }, { 47, -2 }, { 38, 8 },
    { 28, 17 }, { 18, 14 }, { 8, 10 }, { -2, 7 }, { -12, 2 },
    { -22, -6 }, { -30, -18 }, { -32, -33 }, { -26, -48 },
    { -14, -60 }, { -2, -70 }, { 12, -78 }, { 28, -80 }, { 42, -79 },
};
const int PKT_POLY_COUNT = (int)(sizeof(PKT_POLY) / sizeof(PKT_POLY[0]));

const SiteArchetypeDescriptor ARCHETYPES[] =
{
    { SiteArchetype::MARE_INDUSTRIAL, "MARE INDUSTRIAL", Color{ 224, 168, 108, 255 },
      "Fe/Ti-rich ground, flat, strong Earth comms",
      "aluminium (import it), 14-day nights" },
    { SiteArchetype::HIGHLAND_CONSTRUCTION, "HIGHLAND CONSTRUCTION", Color{ 150, 200, 150, 255 },
      "Al/Ca - cheap structures",
      "metal (import it), rough ground" },
    { SiteArchetype::POLAR_VOLATILE, "POLAR VOLATILE", Color{ 140, 190, 235, 255 },
      "PSR ice next door, near-constant sun on the crest",
      "low metals, brutal terrain, marginal comms" },
    { SiteArchetype::KREEP_SCIENTIFIC, "KREEP SCIENTIFIC", Color{ 196, 150, 220, 255 },
      "science",
      "mediocre everything else" },
    { SiteArchetype::LAVA_TUBE, "LAVA TUBE", Color{ 200, 200, 210, 255 },
      "shelter",
      "nothing this survey can see" },
    { SiteArchetype::MIXED, "MIXED", Color{ 150, 200, 150, 255 },
      "both Fe and Al at moderate grade, no imports",
      "master of none" },
};

} // namespace

bool InProcellarumKreepTerrane(double lat, double lon)
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

const SiteArchetypeDescriptor& GetSiteArchetypeDescriptor(SiteArchetype archetype)
{
    for (const SiteArchetypeDescriptor& d : ARCHETYPES)
    {
        if (d.archetype == archetype) return d;
    }
    return ARCHETYPES[sizeof(ARCHETYPES) / sizeof(ARCHETYPES[0]) - 1];
}

RegionIdentity IdentifyRegion(const LolaDem* dem, double lat, double lon)
{
    RegionIdentity r;
    r.featureIndex = LunarRegionAt(lat, lon);
    bool pkt = InProcellarumKreepTerrane(lat, lon);
    bool polar = (std::fabs(lat) > 80.0);

    // Mare or highland. The dataset knows for a named feature -- a mare is
    // a mare, and a crater's floor is what its dominant rock says -- and
    // elevation is the proxy everywhere else: mare floors sit 2-3 km below
    // the reference radius. The proxy alone read every deep crater floor
    // as basalt (Tycho's is impact melt, 4 km down), which is why the
    // feature's own record wins where there is one.
    const LunarRegion* feature = (r.featureIndex >= 0)
        ? &GetLunarRegions()[(size_t)r.featureIndex] : nullptr;
    const char* featureRock = (feature && !feature->dominantRock.empty())
        ? feature->dominantRock.c_str() : nullptr;
    if (feature && feature->featureType == "mare")
    {
        r.isMare = true;
    }
    else if (featureRock)
    {
        // A crater is mare ground only when basalt is what it is made of,
        // not when basalt merely patches an impact-melt or anorthositic
        // floor.
        r.isMare = (std::strncmp(featureRock, "basalt", 6) == 0)
                || (std::strncmp(featureRock, "mare basalt", 11) == 0)
                || (std::strstr(featureRock, "mare basalt") != nullptr
                    && std::strstr(featureRock, "impact") == nullptr)
                || (std::strncmp(featureRock, "high-Ti mare basalt", 19) == 0)
                || (std::strncmp(featureRock, "young", 5) == 0);
    }
    else
    {
        float elevM = (dem && dem->IsLoaded()) ? dem->ElevationM(lat, lon) : 0.0f;
        r.isMare = (elevM < -500.0f);
    }

    r.terrane = pkt ? "Procellarum KREEP Terrane"
                    : (polar ? "Feldspathic Highlands (polar)"
                             : "Feldspathic Highlands");
    r.rock = featureRock ? featureRock
                         : (r.isMare ? "mare basalt" : "anorthosite breccia");

    // Derived from the ground itself: mafic ground carries iron and
    // titanium, feldspathic ground does not; thorium is the terrane's to
    // give. This is the answer for unnamed ground -- and for the many
    // named regions nobody has measured, which is most of them.
    const float derivedFe = r.isMare ? 13.0f : 5.0f;
    const float derivedTi = r.isMare ? 2.5f : 0.5f;
    const float derivedTh = pkt ? 5.0f : 1.0f;

    if (feature)
    {
        std::snprintf(r.name, sizeof(r.name), "%s", feature->name.c_str());
        r.fePct = (feature->fePct >= 0.0f) ? feature->fePct : derivedFe;
        r.tiPct = (feature->tiPct >= 0.0f) ? feature->tiPct : derivedTi;
        r.thPpm = (feature->thPpm >= 0.0f) ? feature->thPpm : derivedTh;
    }
    else
    {
        std::snprintf(r.name, sizeof(r.name), "%s %s",
                      r.isMare ? "Unnamed mare" : "Unnamed highland",
                      polar ? "(polar)" : "");
        r.fePct = derivedFe;
        r.tiPct = derivedTi;
        r.thPpm = derivedTh;
    }

    if (polar)
    {
        std::snprintf(r.latitudeNote, sizeof(r.latitudeNote),
                      "%.0f %c - PSR floors + near-constant crest sun",
                      std::fabs(lat), lat < 0 ? 'S' : 'N');
    }
    else
    {
        std::snprintf(r.latitudeNote, sizeof(r.latitudeNote),
                      "%.0f %c - 14-day nights, %s Earth comms",
                      std::fabs(lat), lat < 0 ? 'S' : 'N',
                      std::fabs(lon) < 50.0 ? "strong" : "grazing");
    }

    // Archetype: the strategy tag, from the composition just derived.
    if (polar)
    {
        r.archetype = SiteArchetype::POLAR_VOLATILE;
    }
    else if (r.thPpm >= 5.0f && !r.isMare)
    {
        // KREEP tags the ground whose ONLY standout is thorium. A
        // thorium-rich mare is still flat, iron-rich, easy ground, and
        // that is what a colony there is built for -- so mare wins.
        r.archetype = SiteArchetype::KREEP_SCIENTIFIC;
    }
    else if (r.isMare)
    {
        r.archetype = SiteArchetype::MARE_INDUSTRIAL;
    }
    else
    {
        r.archetype = SiteArchetype::HIGHLAND_CONSTRUCTION;
    }
    return r;
}
