#include "resource_manager.h"

#include "lunar_dem_shared.h"
#include "terrain_synthesis.h"      // TERRAIN_CELL_KM

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace
{

// The natural, extractable elements the planet holds. Everything else in
// ResourceType is produced, not found.
const ResourceType NATURAL[] = {
    ResourceType::H2, ResourceType::O2, ResourceType::C, ResourceType::Fe,
    ResourceType::Si, ResourceType::Ti, ResourceType::Al, ResourceType::Ca,
};
const int NATURAL_COUNT = (int)(sizeof(NATURAL) / sizeof(NATURAL[0]));

// The scale the economy was tuned against. The old cluster generator peaked
// each element at these values and a typical cell sat near 0.6 of the peak;
// the baselines below reproduce that typical cell so extraction rates,
// prospecting richness (RICHNESS_NORMALIZATION) and the c1 economics keep
// their meaning.
const float TYPICAL_FRACTION = 0.6f;
float PeakQuantity(ResourceType type)
{
    switch (type)
    {
        case ResourceType::H2: return 5000.0f;
        case ResourceType::O2: return 4000.0f;
        case ResourceType::C:  return 3000.0f;
        case ResourceType::Fe: return 6000.0f;
        case ResourceType::Si: return 2000.0f;
        case ResourceType::Ti: return 4000.0f;
        case ResourceType::Al: return 3500.0f;
        case ResourceType::Ca: return 2500.0f;
        default: return 0.0f;
    }
}

// Reference compositions the baselines are scaled against: the mare
// figures the region card shows for Mare Imbrium (Fe 14, Ti 2.5 wt%).
const float REF_FE_PCT = 14.0f;
const float REF_TI_PCT = 3.0f;
const float REF_AL_PCT = 12.0f;
const float REF_CA_PCT = 11.0f;

// The variation: a smooth value noise in km, one lattice cell per
// wavelength, so two sects 5 km apart differ and two 500 m apart barely
// do. Hashed, not stored, so it is the same from any process.
const double VARIATION_WAVELENGTH_KM = 20.0;
const float VARIATION_MIN = 0.6f;
const float VARIATION_MAX = 1.4f;

uint32_t Hash3(int64_t x, int64_t y, uint32_t salt)
{
    uint32_t h = 2166136261u;
    uint32_t parts[6] = {
        (uint32_t)(x & 0xffffffffu), (uint32_t)((x >> 32) & 0xffffffffu),
        (uint32_t)(y & 0xffffffffu), (uint32_t)((y >> 32) & 0xffffffffu),
        salt, 0x9e3779b9u };
    for (uint32_t p : parts)
    {
        h ^= p;
        h *= 16777619u;
    }
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return h;
}

float Lattice01(int64_t x, int64_t y, uint32_t salt)
{
    return (float)(Hash3(x, y, salt) & 0xffffffu) / (float)0xffffffu;
}

float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

// Value noise in [0, 1] at a km position, bilinear between hashed lattice
// values with a smoothstep on the fractions.
float ValueNoise01(double xKm, double yKm, uint32_t salt)
{
    double fx = xKm / VARIATION_WAVELENGTH_KM;
    double fy = yKm / VARIATION_WAVELENGTH_KM;
    int64_t ix = (int64_t)std::floor(fx), iy = (int64_t)std::floor(fy);
    float tx = SmoothStep((float)(fx - (double)ix));
    float ty = SmoothStep((float)(fy - (double)iy));
    float a = Lattice01(ix, iy, salt), b = Lattice01(ix + 1, iy, salt);
    float c = Lattice01(ix, iy + 1, salt), d = Lattice01(ix + 1, iy + 1, salt);
    float top = a + (b - a) * tx;
    float bottom = c + (d - c) * tx;
    return top + (bottom - top) * ty;
}

int NaturalIndex(ResourceType type)
{
    for (int i = 0; i < NATURAL_COUNT; i++) if (NATURAL[i] == type) return i;
    return -1;
}

// Plagioclase against mafic minerals: the more iron the rock carries, the
// less aluminium and calcium (master design SS4.6). Highland (Fe ~5) reads
// Al ~12 / Ca ~11 wt%, mare (Fe ~14) Al ~7 / Ca ~8.
float AluminiumPct(float fePct)
{
    return std::clamp(12.0f - (fePct - 5.0f) * (5.0f / 9.0f), 6.0f, 13.0f);
}
float CalciumPct(float fePct)
{
    return std::clamp(11.0f - (fePct - 5.0f) * (3.0f / 9.0f), 7.0f, 12.0f);
}
// How polar a place is, 0 below 60 deg, 1 at the pole. Hydrogen follows it:
// cold traps are a polar thing.
float PolarFactor(double latDeg)
{
    return std::clamp(((float)std::fabs(latDeg) - 60.0f) / 30.0f, 0.0f, 1.0f);
}

} // namespace

// ---------------------------------------------------------------------------

ResourceManager::ResourceManager(unsigned int seed)
    : worldSeed(seed)
{
    if (worldSeed == 0) worldSeed = std::random_device{}();
}

void ResourceManager::SetWorldSeed(unsigned int seed)
{
    worldSeed = (seed == 0) ? std::random_device{}() : seed;
    grounds.clear();
    surveys.clear();
}

float ResourceManager::DepthBias(ResourceType type, DepthLayer layer)
{
    // Surface (0-10cm), Shallow (10-30cm), Mid (30-100cm), Deep (100-300cm)
    struct Bias { ResourceType type; float bias[4]; };
    static const Bias BIASES[] = {
        {ResourceType::H2, {1.5f, 1.0f, 0.5f, 0.3f}},
        {ResourceType::O2, {1.3f, 1.0f, 0.8f, 0.6f}},
        {ResourceType::C,  {1.4f, 1.0f, 0.7f, 0.5f}},
        {ResourceType::Fe, {0.6f, 1.0f, 0.9f, 1.8f}},
        {ResourceType::Si, {0.8f, 1.0f, 1.3f, 0.7f}},
        {ResourceType::Ti, {0.4f, 1.0f, 1.0f, 2.0f}},
        {ResourceType::Al, {0.8f, 1.0f, 1.2f, 0.9f}},
        {ResourceType::Ca, {0.9f, 1.0f, 1.3f, 0.8f}},
    };
    int l = std::clamp((int)layer, 0, 3);
    for (const Bias& b : BIASES) if (b.type == type) return b.bias[l];
    return 1.0f;
}

ResourceManager::Ground ResourceManager::Generate(const LunarPoint& point) const
{
    Ground g;
    g.point = point;
    g.region = IdentifyRegion(GetLunarDem(), point.latDeg, point.lonDeg);

    const float fe = g.region.fePct;
    const float ti = g.region.tiPct;
    const float al = AluminiumPct(fe);
    const float ca = CalciumPct(fe);
    const float polar = PolarFactor(point.latDeg);

    // The baseline per element: the region's composition, at the scale the
    // economy expects. Silicon barely varies across lunar rock and so is a
    // constant; oxygen is in every regolith grain; carbon is scarce
    // everywhere; hydrogen follows the cold.
    float base[NATURAL_COUNT] = {};
    base[NaturalIndex(ResourceType::Fe)] = PeakQuantity(ResourceType::Fe) * TYPICAL_FRACTION * (fe / REF_FE_PCT);
    base[NaturalIndex(ResourceType::Ti)] = PeakQuantity(ResourceType::Ti) * TYPICAL_FRACTION * (ti / REF_TI_PCT);
    base[NaturalIndex(ResourceType::Al)] = PeakQuantity(ResourceType::Al) * TYPICAL_FRACTION * (al / REF_AL_PCT);
    base[NaturalIndex(ResourceType::Ca)] = PeakQuantity(ResourceType::Ca) * TYPICAL_FRACTION * (ca / REF_CA_PCT);
    base[NaturalIndex(ResourceType::Si)] = PeakQuantity(ResourceType::Si) * TYPICAL_FRACTION;
    base[NaturalIndex(ResourceType::H2)] = PeakQuantity(ResourceType::H2) * TYPICAL_FRACTION * (0.5f + 0.5f * polar);
    base[NaturalIndex(ResourceType::O2)] = PeakQuantity(ResourceType::O2) * TYPICAL_FRACTION;
    base[NaturalIndex(ResourceType::C)]  = PeakQuantity(ResourceType::C)  * TYPICAL_FRACTION * 0.8f;

    // The variation, in km so its wavelength is a real distance whatever
    // the latitude. Longitude is scaled by cos(lat) so the lattice does not
    // squeeze toward the poles.
    double xKm = point.lonDeg * std::max(0.2, std::cos(point.latDeg * 3.14159265358979323846 / 180.0))
                 * LUNAR_KM_PER_DEG;
    double yKm = point.latDeg * LUNAR_KM_PER_DEG;
    for (int i = 0; i < NATURAL_COUNT; i++)
    {
        float n = ValueNoise01(xKm, yKm, worldSeed ^ (uint32_t)(0x51ed27u * (i + 1)));
        float mult = VARIATION_MIN + (VARIATION_MAX - VARIATION_MIN) * n;
        float q = base[i] * mult;
        if (q > PeakQuantity(NATURAL[i]) * 2.0f) q = PeakQuantity(NATURAL[i]) * 2.0f;
        g.resources[NATURAL[i]] = std::max(0.0f, q);
    }
    return g;
}

const ResourceManager::Ground& ResourceManager::GroundAt(const LunarPoint& point) const
{
    LunarKey key = LunarQuantise(point);
    auto it = grounds.find(key);
    if (it == grounds.end())
    {
        it = grounds.emplace(key, Generate(point)).first;
    }
    return it->second;
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAt(const LunarPoint& point) const
{
    std::vector<std::pair<ResourceType, float>> result;
    for (const auto& [type, quantity] : GroundAt(point).resources)
    {
        if (quantity > 0.0f) result.push_back({type, quantity});
    }
    return result;
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAtLayer(
    const LunarPoint& point, DepthLayer layer) const
{
    std::vector<std::pair<ResourceType, float>> result;
    for (const auto& [type, quantity] : GroundAt(point).resources)
    {
        float q = quantity * DepthBias(type, layer);
        if (q > 0.0f) result.push_back({type, q});
    }
    return result;
}

void ResourceManager::Deplete(const LunarPoint& point, ResourceType type, float amount)
{
    Ground& g = const_cast<Ground&>(GroundAt(point));
    auto it = g.resources.find(type);
    if (it == g.resources.end()) return;
    it->second = std::max(0.0f, it->second - amount);
    g.isExploited = true;
}

void ResourceManager::EnsureBasicResources(const LunarPoint& point)
{
    Ground& g = const_cast<Ground&>(GroundAt(point));
    static const std::pair<ResourceType, float> FLOORS[] = {
        {ResourceType::H2, 100.0f}, {ResourceType::O2, 100.0f},
        {ResourceType::C, 100.0f},  {ResourceType::Fe, 100.0f},
        {ResourceType::Si, 100.0f}, {ResourceType::Ti, 50.0f},
        {ResourceType::Al, 75.0f},  {ResourceType::Ca, 50.0f},
    };
    for (const auto& [type, floor] : FLOORS)
    {
        g.resources[type] = std::max(g.resources[type], floor);
    }
}

ResourceManager::OrbitalSurveyData ResourceManager::SurveyAt(const LunarPoint& point) const
{
    LunarKey key = LunarQuantise(point);
    auto cached = surveys.find(key);
    if (cached != surveys.end()) return cached->second;

    const Ground& g = GroundAt(point);
    OrbitalSurveyData s;
    // Composition belongs to the region: one value, never refining.
    s.fePercent = g.region.fePct / 100.0f;
    s.tiPercent = g.region.tiPct / 100.0f;
    s.siPercent = 0.21f;                            // SiO2 ~45 wt% everywhere
    s.alPercent = AluminiumPct(g.region.fePct) / 100.0f;
    s.caPercent = CalciumPct(g.region.fePct) / 100.0f;
    s.thPpm = g.region.thPpm;
    s.kPpm = std::clamp(g.region.thPpm * 300.0f, 0.0f, 2000.0f);
    auto h2 = g.resources.find(ResourceType::H2);
    s.hydrogenSignal = std::clamp((h2 != g.resources.end() ? h2->second : 0.0f)
                                  / PeakQuantity(ResourceType::H2), 0.0f, 1.0f);

    // Terrain: measured where the elevation model is present. Without it
    // the latitude alone answers, so the game runs with no data files.
    const LolaDem* moon = GetLunarDem();
    if (moon != nullptr)
    {
        TerrainBuildability site = moon->EvaluateSite(point.latDeg, point.lonDeg,
                                                      TERRAIN_CELL_KM);
        s.terrainSlope = std::clamp(site.meanSlopeDeg, 0.0f, 45.0f);
        s.solarIllumination = site.illumination;
        s.earthVisibility = site.earthVisibility;
    }
    else
    {
        float latFactor = 1.0f - (float)std::fabs(point.latDeg) / 90.0f;
        s.solarIllumination = std::clamp(0.3f + latFactor * 0.6f, 0.0f, 1.0f);
        s.terrainSlope = g.region.isMare ? 3.0f : 9.0f;
        double lon = point.lonDeg;
        while (lon > 180.0) lon -= 360.0;
        while (lon < -180.0) lon += 360.0;
        s.earthVisibility = std::clamp(1.0f - (float)std::fabs(lon) / 90.0f, 0.0f, 1.0f);
    }
    surveys.emplace(key, s);
    return s;
}

SiteArchetype ResourceManager::ArchetypeAt(const LunarPoint& point) const
{
    return GroundAt(point).region.archetype;
}
