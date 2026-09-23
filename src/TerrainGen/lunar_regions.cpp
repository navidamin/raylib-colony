// The named regions, read from zones.json. See lunar_regions.h.
//
// The JSON reader below is deliberately small: only what zones.json
// needs, in the same spirit as the little TIFF reader in lola_dem.cpp.
// It is a real parser rather than a grep -- it tracks nesting and string
// escapes, so a comma inside a "terrain" sentence or a brace inside a
// note cannot desynchronise it -- but it keeps nothing it was not asked
// for. Anything unrecognised is skipped, so extra fields in the asset
// (mission lists, formation notes, lighting prose) cost nothing.

#include "lunar_regions.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{

const double MOON_RADIUS_KM = 1737.4;

// ---------------------------------------------------------------------------
// A JSON subset reader
// ---------------------------------------------------------------------------

struct Reader
{
    const char* p = nullptr;
    const char* end = nullptr;

    bool done() const { return p >= end; }
    char peek() const { return done() ? '\0' : *p; }

    void ws()
    {
        while (!done() && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
    }

    bool take(char c)
    {
        ws();
        if (peek() != c) return false;
        p++;
        return true;
    }

    bool literal(const char* s)
    {
        ws();
        size_t n = 0;
        while (s[n]) n++;
        if ((size_t)(end - p) < n) return false;
        for (size_t i = 0; i < n; i++) if (p[i] != s[i]) return false;
        p += n;
        return true;
    }

    // A JSON string. Escapes are consumed correctly even when the caller
    // throws the value away -- that is what keeps the scan in step.
    bool string(std::string* out)
    {
        ws();
        if (peek() != '"') return false;
        p++;
        if (out) out->clear();
        while (!done() && *p != '"')
        {
            if (*p == '\\')
            {
                p++;
                if (done()) return false;
                char e = *p++;
                if (e == 'u')
                {
                    if (end - p < 4) return false;
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++)
                    {
                        char h = p[i];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                        else return false;
                    }
                    p += 4;
                    // UTF-8, so an em dash in a name survives the trip.
                    if (out)
                    {
                        if (cp < 0x80) out->push_back((char)cp);
                        else if (cp < 0x800)
                        {
                            out->push_back((char)(0xC0 | (cp >> 6)));
                            out->push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        else
                        {
                            out->push_back((char)(0xE0 | (cp >> 12)));
                            out->push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out->push_back((char)(0x80 | (cp & 0x3F)));
                        }
                    }
                    continue;
                }
                if (out)
                {
                    switch (e)
                    {
                        case 'n': out->push_back('\n'); break;
                        case 't': out->push_back('\t'); break;
                        case 'r': out->push_back('\r'); break;
                        case 'b': out->push_back('\b'); break;
                        case 'f': out->push_back('\f'); break;
                        default:  out->push_back(e);    break;   // " \ /
                    }
                }
                continue;
            }
            if (out) out->push_back(*p);
            p++;
        }
        if (done()) return false;
        p++;                        // closing quote
        return true;
    }

    // Number, or null. Returns false for null so the caller can tell
    // "no one has measured this" from "measured as zero" -- which for
    // titanium is a real distinction.
    bool number(double* out)
    {
        ws();
        if (literal("null")) return false;
        char* stop = nullptr;
        double v = std::strtod(p, &stop);
        if (stop == p) return false;
        p = stop;
        if (out) *out = v;
        return true;
    }

    // Skip any value, whatever it is.
    bool skipValue()
    {
        ws();
        char c = peek();
        if (c == '"') return string(nullptr);
        if (c == '{' || c == '[')
        {
            char close = (c == '{') ? '}' : ']';
            p++;
            for (;;)
            {
                ws();
                if (done()) return false;
                if (peek() == close) { p++; return true; }
                if (peek() == ',' || peek() == ':') { p++; continue; }
                if (!skipValue()) return false;
            }
        }
        if (literal("true") || literal("false") || literal("null")) return true;
        return number(nullptr);
    }
};

// Published compositions for near-side features the dataset leaves null
// (Fe/Ti wt%, Th ppm), plus enough geometry to stand in for the whole list
// when the asset is missing. Lifted from lunar_map's hand-entered table.
struct BuiltinFeature
{
    const char* name;
    double latDeg, lonDeg, radiusKm;
    float fePct, tiPct, thPpm;
};

const BuiltinFeature BUILTIN_FEATURES[] =
{
    { "Oceanus Procellarum", 18.4, -57.4, 1296, 13.5f, 3.0f, 6.0f },
    { "Mare Frigoris", 55.0, 0.0, 723, 12.0f, 1.5f, 3.0f },
    { "Mare Imbrium", 32.8, -15.6, 573, 14.0f, 2.5f, 8.0f },
    { "Mare Fecunditatis", -7.8, 51.3, 454, 14.0f, 2.0f, 1.5f },
    { "Mare Tranquillitatis", 8.5, 31.4, 436, 15.5f, 8.0f, 1.5f },
    { "Mare Nubium", -21.3, -16.5, 358, 14.0f, 2.0f, 4.0f },
    { "Mare Serenitatis", 28.0, 17.5, 354, 14.5f, 3.5f, 2.5f },
    { "Mare Crisium", 17.0, 59.1, 278, 13.0f, 1.5f, 1.0f },
    { "Mare Humorum", -24.4, -38.6, 194, 14.5f, 3.0f, 4.5f },
    { "Mare Cognitum", -10.0, -23.1, 175, 14.5f, 3.5f, 5.0f },
    { "Mare Nectaris", -15.2, 35.3, 170, 12.5f, 2.0f, 1.0f },
    { "Sinus Medii", 2.4, 1.7, 144, 12.0f, 2.0f, 3.0f },
    { "Sinus Iridum", 44.1, -31.5, 124, 13.0f, 2.0f, 6.0f },
    { "Mare Vaporum", 13.3, 3.6, 122, 13.5f, 3.0f, 5.5f },
    { "Clavius", -58.4, -14.4, 116, 5.0f, 0.5f, 1.0f },
    { "Ptolemaeus", -9.3, -1.9, 76, 6.5f, 0.8f, 2.0f },
    { "Copernicus", 9.6, -20.1, 47, 8.0f, 1.2f, 5.0f },
    { "Tycho", -43.3, -11.4, 43, 6.0f, 0.8f, 1.5f },
    { "Plato", 51.6, -9.4, 50, 12.5f, 2.0f, 4.0f },
};
const int BUILTIN_FEATURE_COUNT =
    (int)(sizeof(BUILTIN_FEATURES) / sizeof(BUILTIN_FEATURES[0]));

std::vector<LunarRegion> g_regions;
bool g_loaded = false;

// The asset is missing: the built-in table is the list.
void LoadBuiltinOnly()
{
    for (int i = 0; i < BUILTIN_FEATURE_COUNT; i++)
    {
        const BuiltinFeature& b = BUILTIN_FEATURES[i];
        LunarRegion e;
        e.name = b.name;
        e.featureType = (b.radiusKm > 100.0) ? "mare" : "crater";
        e.dominantRock = (b.fePct >= 10.0f) ? "mare basalt" : "anorthosite breccia";
        e.latDeg = b.latDeg;
        e.lonDeg = b.lonDeg;
        e.radiusKm = b.radiusKm;
        e.fePct = b.fePct;
        e.tiPct = b.tiPct;
        e.thPpm = b.thPpm;
        g_regions.push_back(std::move(e));
    }
}

// Fill compositions the asset left null from the table, by name.
void FillBuiltinCompositions()
{
    for (LunarRegion& r : g_regions)
    {
        for (int i = 0; i < BUILTIN_FEATURE_COUNT; i++)
        {
            if (r.name != BUILTIN_FEATURES[i].name) continue;
            if (r.fePct < 0.0f) r.fePct = BUILTIN_FEATURES[i].fePct;
            if (r.tiPct < 0.0f) r.tiPct = BUILTIN_FEATURES[i].tiPct;
            if (r.thPpm < 0.0f) r.thPpm = BUILTIN_FEATURES[i].thPpm;
            break;
        }
    }
}

void Load()
{
    if (g_loaded) return;
    g_loaded = true;

    const char* path = "src/assets/planet/zones.json";
    char* text = LoadFileText(path);
    if (text == nullptr)
    {
        TraceLog(LOG_WARNING, "REGIONS: %s not found; using the built-in "
                              "near-side table", path);
        LoadBuiltinOnly();
        return;
    }

    Reader r;
    r.p = text;
    r.end = text + TextLength(text);

    int skippedLandings = 0;
    if (!r.take('['))
    {
        TraceLog(LOG_WARNING, "REGIONS: %s is not a JSON array", path);
        UnloadFileText(text);
        LoadBuiltinOnly();
        return;
    }
    for (;;)
    {
        r.ws();
        if (r.take(']')) break;
        if (r.take(',')) continue;
        if (!r.take('{')) break;             // malformed: keep what we have

        LunarRegion e;
        double diameterKm = 0.0;
        bool haveDiameter = false;
        for (;;)
        {
            r.ws();
            if (r.take('}')) break;
            if (r.take(',')) continue;
            std::string key;
            if (!r.string(&key)) { r.p = r.end; break; }
            if (!r.take(':')) { r.p = r.end; break; }

            double num = 0.0;
            if (key == "name") r.string(&e.name);
            else if (key == "feature_type") r.string(&e.featureType);
            else if (key == "dominant_rock") r.string(&e.dominantRock);
            else if (key == "lat") { if (r.number(&num)) e.latDeg = num; }
            else if (key == "lon") { if (r.number(&num)) e.lonDeg = num; }
            else if (key == "diameter_km")
            {
                if (r.number(&num)) { diameterKm = num; haveDiameter = true; }
            }
            else if (key == "iron_pct") { if (r.number(&num)) e.fePct = (float)num; }
            else if (key == "titanium_pct") { if (r.number(&num)) e.tiPct = (float)num; }
            else if (key == "thorium_ppm") { if (r.number(&num)) e.thPpm = (float)num; }
            else r.skipValue();
        }
        if (r.done() && e.name.empty()) break;

        if (e.featureType == "landing") { skippedLandings++; continue; }
        if (e.name.empty() || !haveDiameter || diameterKm <= 0.0) continue;
        e.radiusKm = diameterKm * 0.5;
        g_regions.push_back(std::move(e));
    }
    UnloadFileText(text);
    FillBuiltinCompositions();

    int far = 0, withComp = 0;
    for (const LunarRegion& e : g_regions)
    {
        double lon = e.lonDeg;
        while (lon > 180.0) lon -= 360.0;
        while (lon < -180.0) lon += 360.0;
        if (std::fabs(lon) > 90.0) far++;
        if (e.fePct >= 0.0f) withComp++;
    }
    TraceLog(LOG_INFO,
             "REGIONS: %d from zones.json (%d far side, %d with measured "
             "composition), %d landing sites skipped",
             (int)g_regions.size(), far, withComp, skippedLandings);
}

} // namespace

const std::vector<LunarRegion>& GetLunarRegions()
{
    Load();
    return g_regions;
}

double LunarRegionDistanceKm(const LunarRegion& region,
                             double latDeg, double lonDeg)
{
    const double d2r = 3.14159265358979323846 / 180.0;
    double la1 = latDeg * d2r, la2 = region.latDeg * d2r;
    double c = std::sin(la1) * std::sin(la2) +
               std::cos(la1) * std::cos(la2) *
               std::cos((lonDeg - region.lonDeg) * d2r);
    return std::acos(std::clamp(c, -1.0, 1.0)) * MOON_RADIUS_KM;
}

int LunarRegionAt(double latDeg, double lonDeg)
{
    const std::vector<LunarRegion>& all = GetLunarRegions();
    int best = -1;
    double bestR = 1e18;
    for (size_t i = 0; i < all.size(); i++)
    {
        if (all[i].radiusKm < bestR &&
            LunarRegionDistanceKm(all[i], latDeg, lonDeg) <= all[i].radiusKm)
        {
            best = (int)i;
            bestR = all[i].radiusKm;
        }
    }
    return best;
}
