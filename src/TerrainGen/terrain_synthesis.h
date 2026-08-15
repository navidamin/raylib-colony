#ifndef TERRAIN_SYNTHESIS_H
#define TERRAIN_SYNTHESIS_H

#include "raylib.h"

// Procedural terrain amplification on real lunar imagery.
//
// C++ port of prototypes/planet_visuals/site_synthesis.py (the
// photo-real path, no invented craters): the real LROC WAC mosaic
// supplies every landform; below its ~1.3 km/px resolution floor the
// synthesizer re-sharpens, relights (macro luminance as height proxy)
// and adds regolith grain. Deterministic per location: same lat/lon,
// same ground, no stored assets.
//
// Scale system (see prototypes/planet_visuals/SITE_SYNTHESIS.md):
//   1 world unit = 50 m; planet grid cell (sect + units) = 5 km
//   PLANET view = 20x20 cells = 100 km      COLONY view = 25 km
//   SECT view   = 1 cell      = 5 km
//
// The playfield anchor maps the 20x20 planet grid onto a real 100 km
// region of the moon; each grid cell has real lat/lon coordinates.

// Default playfield anchor: centre of the 20x20 planet grid on the real
// moon. Mare Imbrium — flat mare with a distinctive crater nearby. The
// live anchor is settable, so the player can pick a region from orbit
// and the whole grid re-registers there.
const double TERRAIN_ANCHOR_LAT = 32.8;
const double TERRAIN_ANCHOR_LON = -15.6;

const double MOON_KM_PER_DEG = 30.32268;   // pi * 1737.4 / 180
const double TERRAIN_CELL_KM = 5.0;        // one grid cell, sect diameter

// The playfield's current centre on the moon. Setting it invalidates any
// cached terrain (RenderManager re-generates on the next draw).
void SetTerrainAnchor(double latDeg, double lonDeg);
void GetTerrainAnchor(double* latDeg, double* lonDeg);
// Bumped whenever the anchor moves — cheap cache-invalidation token.
unsigned int GetTerrainAnchorVersion();

// Real lat/lon of a planet grid cell centre (gx, gy in 0..19; gy grows
// south, matching the game grid's y-down convention).
void TerrainGridCellToLatLon(int gx, int gy, double* latDeg, double* lonDeg);

// Invert the orbital disc projection: turn a screen-space click on the
// moon disc into real lat/lon. Returns false if the click misses the
// disc (or lands on the limb, where the projection is degenerate).
// The disc is drawn centred on screen at 1200 px with a 12 px margin
// (see prototypes/planet_visuals/asset_bake.py), near side, lon 0.
bool OrbitalPickToLatLon(float screenX, float screenY,
                         int screenWidth, int screenHeight,
                         double* latDeg, double* lonDeg);

// Project lat/lon back onto the orbital disc — the inverse of the above,
// for drawing the pick marker. Returns false if on the far side.
bool OrbitalLatLonToScreen(double latDeg, double lonDeg,
                           int screenWidth, int screenHeight,
                           float* screenX, float* screenY);

// Surface disturbance around an occupied site.
//
// Not a graded platform: the natural ground is kept, and only worked
// over a little where the colony actually operates — gentle undulations
// across the site, and small random alterations (mounds, hollows,
// patchy roughness) around each dome. Applied to the HEIGHT field, so
// the shared sun shades and shadows it like any other terrain.
struct TerrainSiteDisturbance
{
    bool enabled = false;
    int domeCount = 8;             // units ringing the sect core
    float ringRadiusKm = 3.30f;    // radius the unit domes sit on
    float coreRadiusKm = 1.70f;    // the central dome
    float domeWorkKm = 1.15f;      // worked ground around each dome
    float undulationAmp = 0.0075f; // gentle mounds and hollows
    float roughAmp = 0.0045f;      // random fine alterations
    float spotAmp = 0.0060f;       // per-dome mound/hollow depth
    float siteRadiusKm = 4.60f;    // nothing is touched beyond this
};

// Global switch + accessor for the site disturbance (playtest compare).
void SetSiteDisturbanceEnabled(bool enabled);
bool IsSiteDisturbanceEnabled();

// The three geographic zoom levels, in game terms:
//   0  PLANET view  100 km   (20x20 cells of 5 km)
//   1  COLONY view   25 km   (5x5 cells)
//   2  SECT view      5 km   (one cell)
// Generating a sect's ground already computes 0 and 1 on the way down,
// so emitting all three costs nothing extra. Caller owns every Image.
// site == nullptr (or disabled) leaves the ground completely untouched.
void GenerateTerrainChain(double latDeg, double lonDeg, int res,
                          Image outLevels[3],
                          const TerrainSiteDisturbance* site = nullptr);

// Tuning knobs for the surface layers (all multipliers on the
// baseline, except the weights which are absolute). Craters were
// removed by user decision 2026-08-13 — the layers left are grain,
// undulation, boulders, form relief, lighting and speckle.
struct TerrainTuning
{
    float grain = 1.0f;         // regolith roughness (x 0.004 height)
    float undulation = 1.0f;    // long-wavelength rolling (x 0.02)
    float boulders = 1.0f;      // boulder count multiplier
    float boulderAmp = 1.0f;    // boulder bump height multiplier
    float formRelief = 1.0f;    // real-form relighting strength (x 0.13)
    float relWeight = 0.38f;    // hillshade contribution (absolute)
    float lightWeight = 0.55f;  // cast-shadow contribution (absolute)
    float speckle = 1.0f;       // albedo mottling (x 0.04)
    float sCurve = 0.20f;       // shadow-deepening mix (absolute)
};

// Generate the SECT view ground for a location: a res x res RGB image
// of the 5 km cell, amplified through the real-imagery chain
// (100 km -> 25 km -> 5 km). Caller owns the Image (UnloadImage).
// Requires src/assets/planet/wac_global.jpg (loaded once, cached).
// tuning == nullptr uses the baseline TerrainTuning.
Image GenerateSectTerrain(double latDeg, double lonDeg, int res = 300,
                          const TerrainTuning* tuning = nullptr);

#endif // TERRAIN_SYNTHESIS_H
