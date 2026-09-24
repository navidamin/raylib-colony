#include "relief.h"

#include "raylib.h"
#include "lunar_dem_shared.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define RELIEF_WEB 1
#else
#define RELIEF_WEB 0
#endif

namespace
{

// The tile grid: tools/relief/build_relief.py writes the same one.
const int PPD = 128;                     // pixels per degree
const int TILE = 512;                    // pixels per tile side
const int COLS = 360 * PPD / TILE;       // 90
const int ROWS = 180 * PPD / TILE;       // 45
const int GLOBAL_W = 360 * PPD;
const int GLOBAL_H = 180 * PPD;
const double STEP_M = 7.0;               // metres per code step
const double MOON_KM_PER_DEG = 30.32268;

// A window with less than this share of measured relief keeps the
// synthesizer: LOLA alone, at 1.9 km, is smoother than what it replaces.
const double MIN_COVERAGE = 0.5;

// Decoded tiles kept: 512x512 bytes each, so 96 is 24 MB -- a district
// window touches about twelve.
const size_t MAX_READY = 96;

enum class State { UNKNOWN, LOADING, READY, ABSENT };

struct Tile
{
    State state = State::UNKNOWN;
    std::vector<unsigned char> px;       // TILE x TILE codes, row 0 north
    unsigned int lastUsed = 0;
};

std::unordered_map<int, Tile> g_tiles;
unsigned int g_clock = 0;
// The desktop builds CPU windows on worker threads; the browser's fetch
// callbacks run on its one thread between frames.
std::recursive_mutex g_lock;
std::string g_source;
bool g_sourceSet = false;

int KeyOf(int row, int col) { return row * COLS + col; }

const std::string& Source()
{
    if (!g_sourceSet)
    {
#if RELIEF_WEB
        g_source = emscripten_run_script_string(
            "(window.COLONY_RELIEF_URL || 'relief/')");
#else
        g_source = "data/relief";
#endif
        g_sourceSet = true;
    }
    return g_source;
}

void EvictIfFull()
{
    size_t ready = 0;
    for (auto& kv : g_tiles) if (kv.second.state == State::READY) ready++;
    while (ready > MAX_READY)
    {
        auto victim = g_tiles.end();
        for (auto it = g_tiles.begin(); it != g_tiles.end(); ++it)
        {
            if (it->second.state != State::READY) continue;
            if (victim == g_tiles.end() || it->second.lastUsed < victim->second.lastUsed)
                victim = it;
        }
        if (victim == g_tiles.end()) break;
        g_tiles.erase(victim);          // UNKNOWN again: fetched again if wanted
        ready--;
    }
}

bool Decode(const unsigned char* data, int size, std::vector<unsigned char>* out)
{
    Image img = LoadImageFromMemory(".jpg", data, size);
    if (img.data == nullptr) return false;
    bool ok = (img.width == TILE && img.height == TILE);
    if (ok)
    {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        const unsigned char* p = (const unsigned char*)img.data;
        out->assign(p, p + (size_t)TILE * TILE);
    }
    UnloadImage(img);
    return ok;
}

void Arrived(int key, const unsigned char* data, int size)
{
    Tile& t = g_tiles[key];
    t.state = Decode(data, size, &t.px) ? State::READY : State::ABSENT;
    t.lastUsed = ++g_clock;
    if (t.state == State::READY) EvictIfFull();
}

#if RELIEF_WEB
void OnLoad(void* arg, void* buffer, int size)
{
    Arrived((int)(intptr_t)arg, (const unsigned char*)buffer, size);
}
void OnError(void* arg)
{
    g_tiles[(int)(intptr_t)arg].state = State::ABSENT;
}
#endif

std::string TileName(int row, int col)
{
    char name[64];
    std::snprintf(name, sizeof(name), "r128_%02d_%02d.jpg", row, col);
    return name;
}

// Ask for a tile. On the desktop it is there (or absent) when this returns.
void Request(int row, int col)
{
    int key = KeyOf(row, col);
    Tile& t = g_tiles[key];
    if (t.state != State::UNKNOWN) return;
#if RELIEF_WEB
    t.state = State::LOADING;
    std::string url = Source() + TileName(row, col);
    emscripten_async_wget_data(url.c_str(), (void*)(intptr_t)key, OnLoad, OnError);
#else
    std::string path = Source() + "/" + TileName(row, col);
    int size = 0;
    unsigned char* data = FileExists(path.c_str()) ? LoadFileData(path.c_str(), &size) : nullptr;
    if (data)
    {
        Arrived(key, data, size);
        UnloadFileData(data);
    }
    else t.state = State::ABSENT;
#endif
}

// The window's extent in degrees, as the chains frame it.
struct Extent
{
    double latTop, latSpan, lon0, lonSpan;
};

Extent ExtentOf(double latDeg, double lonDeg, double spanKm)
{
    const double D2R = 3.14159265358979323846 / 180.0;
    Extent e;
    e.latSpan = spanKm / MOON_KM_PER_DEG;
    e.lonSpan = e.latSpan / std::max(0.2, std::cos(latDeg * D2R));
    e.latTop = latDeg + e.latSpan * 0.5;
    e.lon0 = lonDeg - e.lonSpan * 0.5;
    return e;
}

// Every tile the extent touches, one pixel of margin for the bilinear.
template <typename F>
void ForEachTile(const Extent& e, F fn)
{
    double m = 1.0 / PPD;
    int r0 = std::clamp((int)std::floor((90.0 - (e.latTop + m)) * PPD) / TILE, 0, ROWS - 1);
    int r1 = std::clamp((int)std::floor((90.0 - (e.latTop - e.latSpan - m)) * PPD) / TILE, 0, ROWS - 1);
    long long c0 = (long long)std::floor((e.lon0 - m + 180.0) * PPD) / TILE;
    long long c1 = (long long)std::floor((e.lon0 + e.lonSpan + m + 180.0) * PPD) / TILE;
    if (c1 - c0 >= COLS) c1 = c0 + COLS - 1;
    for (int r = r0; r <= r1; r++)
        for (long long c = c0; c <= c1; c++)
            fn(r, (int)(((c % COLS) + COLS) % COLS));
}

// The tiles one window reads, looked up once rather than per tap.
struct TileTable
{
    const unsigned char* px[ROWS][COLS] = {};
    // One code-grid value, in metres of detail; 0 where there is no tile.
    float DetailAt(long long gx, long long gy) const
    {
        gy = std::clamp(gy, 0LL, (long long)GLOBAL_H - 1);
        gx = ((gx % GLOBAL_W) + GLOBAL_W) % GLOBAL_W;
        const unsigned char* p = px[gy / TILE][gx / TILE];
        if (!p) return 0.0f;
        unsigned char v = p[(size_t)(gy % TILE) * TILE + (size_t)(gx % TILE)];
        return (float)(((int)v - 128) * STEP_M);
    }
};

} // namespace

void SetReliefSource(const char* dirOrUrl)
{
    std::lock_guard<std::recursive_mutex> hold(g_lock);
    g_source = dirOrUrl ? dirOrUrl : "";
    g_sourceSet = true;
    g_tiles.clear();
}

void ReliefPrefetch(double latDeg, double lonDeg, double spanKm)
{
    std::lock_guard<std::recursive_mutex> hold(g_lock);
    ForEachTile(ExtentOf(latDeg, lonDeg, spanKm), [](int r, int c) { Request(r, c); });
}

bool ReliefWindowSettled(double latDeg, double lonDeg, double spanKm)
{
    std::lock_guard<std::recursive_mutex> hold(g_lock);
    bool settled = true;
    ForEachTile(ExtentOf(latDeg, lonDeg, spanKm), [&](int r, int c) {
        auto it = g_tiles.find(KeyOf(r, c));
        if (it == g_tiles.end() || it->second.state == State::UNKNOWN
            || it->second.state == State::LOADING)
            settled = false;
    });
    return settled;
}

bool ReliefWindowCovered(double latDeg, double lonDeg, double spanKm)
{
    std::lock_guard<std::recursive_mutex> hold(g_lock);
    int want = 0, ready = 0;
    bool settled = true;
    ForEachTile(ExtentOf(latDeg, lonDeg, spanKm), [&](int r, int c) {
        auto it = g_tiles.find(KeyOf(r, c));
        want++;
        if (it == g_tiles.end() || it->second.state == State::UNKNOWN
            || it->second.state == State::LOADING)
            settled = false;
        else if (it->second.state == State::READY)
            ready++;
    });
    return settled && want > 0 && (double)ready / want >= MIN_COVERAGE;
}

bool ReliefWindowM(double latDeg, double lonDeg, double spanKm, int res,
                   std::vector<float>* metres)
{
    if (!metres || res < 8 || spanKm <= 0.0) return false;
    const LolaDem* dem = GetLunarDem();
    if (!dem || !dem->IsLoaded()) return false;

    std::lock_guard<std::recursive_mutex> hold(g_lock);
    Extent e = ExtentOf(latDeg, lonDeg, spanKm);
    int want = 0, ready = 0;
    bool loading = false;
    static TileTable table;              // under g_lock; 32 KB, so not on the stack
    table = TileTable{};
    ForEachTile(e, [&](int r, int c) {
        Request(r, c);
        Tile& t = g_tiles[KeyOf(r, c)];
        want++;
        if (t.state == State::LOADING) loading = true;
        if (t.state == State::READY)
        {
            ready++;
            t.lastUsed = ++g_clock;
            table.px[r][c] = t.px.data();
        }
    });
    if (loading) return false;
    if (want == 0 || (double)ready / want < MIN_COVERAGE) return false;

    metres->resize((size_t)res * res);
    const double dLat = e.latSpan / res, dLon = e.lonSpan / res;
    for (int y = 0; y < res; y++)
    {
        double lat = e.latTop - (y + 0.5) * dLat;
        double gy = (90.0 - lat) * PPD - 0.5;
        long long y0 = (long long)std::floor(gy);
        float fy = (float)(gy - (double)y0);
        float* row = metres->data() + (size_t)y * res;
        for (int x = 0; x < res; x++)
        {
            double lon = e.lon0 + (x + 0.5) * dLon;
            double gx = (lon + 180.0) * PPD - 0.5;
            long long x0 = (long long)std::floor(gx);
            float fx = (float)(gx - (double)x0);
            float d = (table.DetailAt(x0, y0) * (1.0f - fx) + table.DetailAt(x0 + 1, y0) * fx) * (1.0f - fy)
                    + (table.DetailAt(x0, y0 + 1) * (1.0f - fx) + table.DetailAt(x0 + 1, y0 + 1) * fx) * fy;
            row[x] = dem->GlobalBilinearM(lat, lon) + d;
        }
    }
    return true;
}
