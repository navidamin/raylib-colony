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

std::vector<LunarRegion> g_regions;
bool g_loaded = false;

void Load()
{
    if (g_loaded) return;
    g_loaded = true;

    const char* path = "src/assets/planet/zones.json";
    char* text = LoadFileText(path);
    if (text == nullptr)
    {
        TraceLog(LOG_WARNING, "REGIONS: %s not found; no named regions", path);
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
