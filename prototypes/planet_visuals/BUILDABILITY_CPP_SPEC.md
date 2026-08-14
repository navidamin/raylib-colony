# Buildability check — C++ integration spec

The Python prototype produces both a *visualisation* (red overlay on
the rendered regional view) and a *buildability mask* (separate
grayscale PNG). For the game, neither of those needs to ship as an
asset — the C++ side regenerates everything from a deterministic seed
when the player selects a zone.

This file is the integration spec for the regional view's
buildability check. It mirrors `regional_view.py:buildability_mask()`.

---

## When does it run?

Once per zone-selection event. After the player picks a zone in the
orbital view, the game transitions into **Regional View** for that
zone. At that point:

1. Compute a per-zone deterministic seed.
2. Generate the crater list.
3. Build the heightmap (FBM + craters + pink-noise texture).
4. Build the lit RGB texture (hillshade + cast shadow + AO + biome tint).
5. Build the **slope map** (one Gaussian blur + gradient pass).
6. Cache the crater list + heightmap + slope map for queries.

Steps 1–4 are the same as the procedural surface render. Step 5 is
the only new work for buildability, and it's cheap (~5 ms at 1200×1200).

---

## Per-query interface

```cpp
class RegionalView {
public:
    /// Is the world position (px, py) inside this region buildable?
    /// Coordinates are pixel-space within the regional canvas
    /// (REGION_PX wide). Player's mouse → world coords → this call.
    bool isBuildable(int px, int py) const;

private:
    Zone zone;
    uint32_t seed;
    std::vector<Crater> craters;        // ~30 items
    std::vector<float> heightmap;       // REGION_PX² floats (~5.5 MB at 1200²)
    std::vector<uint8_t> slopeMap;      // REGION_PX² bytes  (~1.4 MB at 1200²)
    static constexpr float CRATER_RIM_FACTOR  = 1.05f;
    static constexpr float SLOPE_THRESHOLD_DEG = 18.0f;
};
```

---

## isBuildable() implementation

```cpp
bool RegionalView::isBuildable(int px, int py) const {
    // Bounds — anything outside the canvas is trivially "no"
    if (px < 0 || py < 0 || px >= REGION_PX || py >= REGION_PX) {
        return false;
    }

    // 1. Crater rim exclusion: linear scan of the crater list.
    //    With ~30 craters this is sub-microsecond.
    for (const Crater& c : craters) {
        float dx = float(px) - c.cx;
        float dy = float(py) - c.cy;
        float d2 = dx*dx + dy*dy;
        float rRim = c.r * CRATER_RIM_FACTOR;
        if (d2 < rRim * rRim) {
            return false;
        }
    }

    // 2. Slope cutoff: pre-built byte map, O(1) sample.
    //    slopeMap is encoded as: byte = degrees * 2.0  (0..255 → 0..127.5°)
    uint8_t slopeByte = slopeMap[py * REGION_PX + px];
    float slopeDeg = float(slopeByte) * 0.5f;
    if (slopeDeg > SLOPE_THRESHOLD_DEG) {
        return false;
    }

    return true;
}
```

---

## Building the slope map

Done once at zone-load. Mirrors the Python code:

```cpp
void RegionalView::buildSlopeMap() {
    // Smooth the heightmap so the pink-noise texture's micro-roughness
    // doesn't fire the slope check on every pixel.
    std::vector<float> smoothed(REGION_PX * REGION_PX);
    gaussianBlur(heightmap, smoothed, REGION_PX, REGION_PX, /*sigma=*/3.5f);

    constexpr float Z_FACTOR = 75.0f;
    slopeMap.resize(REGION_PX * REGION_PX);

    for (int y = 0; y < REGION_PX; ++y) {
        for (int x = 0; x < REGION_PX; ++x) {
            int xL = std::max(0, x - 1);
            int xR = std::min(REGION_PX - 1, x + 1);
            int yT = std::max(0, y - 1);
            int yB = std::min(REGION_PX - 1, y + 1);
            float dx = (smoothed[y * REGION_PX + xR]
                        - smoothed[y * REGION_PX + xL]) * 0.5f * Z_FACTOR;
            float dy = (smoothed[yB * REGION_PX + x]
                        - smoothed[yT * REGION_PX + x]) * 0.5f * Z_FACTOR;
            float slopeRad = std::atan(std::hypot(dx, dy));
            float slopeDeg = slopeRad * (180.0f / float(M_PI));
            slopeMap[y * REGION_PX + x] =
                uint8_t(std::clamp(slopeDeg * 2.0f, 0.0f, 255.0f));
        }
    }
}
```

The byte encoding (`degrees × 2`) caps representable slope at 127.5°,
which is way more than any real lunar terrain (max ~35° for crater
walls). Saves 75% of memory vs. storing floats.

---

## Determinism — same crater list as the Python prototype

```cpp
uint32_t RegionalView::seedForZone(const Zone& z) {
    // Mirrors zone_view.py: SEED ^ (hash(zone.name) & 0xFFFF)
    uint32_t nameHash = 0;
    for (char ch : z.name) nameHash = nameHash * 31 + uint8_t(ch);
    return GLOBAL_SEED ^ (nameHash & 0xFFFF);
}
```

Then the crater-sampling loop uses this seed with the same RNG
(numpy.random.default_rng → equivalent C++ PCG32 or similar). As long
as both sides use the same algorithm, both produce identical
crater lists.

For early integration, a simpler approach: **port the Python
crater-sampling code 1:1 to C++** using `std::mt19937` seeded with
the same value. Visual match is per-zone deterministic.

---

## Memory + perf budget

Per active region:

| Buffer | Size at 1200×1200 | Use |
|---|---|---|
| `heightmap` (float) | 5.76 MB | drives shading + AO + slope build |
| `slopeMap` (uint8) | 1.44 MB | per-query slope lookup |
| `craters` | ~1 KB | per-query crater scan |
| `rgbTexture` | 4.32 MB | what the player sees |

~12 MB per loaded region. Drop when the player leaves the regional
view; rebuild from seed when they re-enter. No disk caching needed
unless you want to skip the rebuild hit (~50–100 ms total at 1200²).

---

## Optional optimisations (apply only if measurement says so)

1. **Pre-baked uint8 buildability map** — combine crater + slope checks
   into a single 1-bit-per-pixel buffer at load time. Per-query becomes
   one bit lookup. Memory: 180 KB at 1200×1200.
2. **Spatial hash for craters** — only matters if zone count grows past
   ~200, which would mean either denser zones or merging. Today's ~30
   craters are well below the linear-scan crossover.
3. **Sparse crater early-out** — sort craters by descending radius so the
   most-likely-to-hit ones are tested first. Negligible at our N.

For 30 craters none of these matter; ship the simple version first.
