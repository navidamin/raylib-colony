#ifndef TERRAIN_SYNTHESIS_H
#define TERRAIN_SYNTHESIS_H

#include "raylib.h"

#include <vector>

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

// Where the orbital view is looking from.
//
// The moon is a globe, so "which way is it turned" is part of the
// projection. Three numbers say it: the point of the surface facing the
// viewer, and how large the disc is drawn. The renderer
// (lunar_globe.h) and the two projection functions below both read this
// one camera, which is what makes a click land on the pixel it points
// at however the globe is turned.
//
// subLat 0, subLon 0 is the near side -- the fixed view the baked
// orbital_near.png gave -- so the defaults reproduce the old geometry,
// up to the disc now being fitted to the viewport instead of a fixed
// 1200 px square that overflowed it.
struct OrbitalCamera
{
    double subLatDeg = 0.0;     // surface point facing the viewer
    double subLonDeg = 0.0;     // 0 = near side
    double zoom = 1.0;          // 1 = whole disc visible, with a margin
};

const OrbitalCamera& GetOrbitalCamera();
void SetOrbitalCamera(const OrbitalCamera& camera);

// Zoom limits. The floor keeps the whole moon on screen. The ceiling is
// what the DESCENT needs, not what the wheel offers: flying into a
// 200 km district means 3476/(2*0.46*200) = 18.9x, past the point where
// the WAC mosaic (~1.3 km/px) resolves anything new. The wheel stops at
// ORBITAL_ZOOM_USER_MAX so a player never drives it into the mush; the
// flight is allowed past because it ends by handing over to the DEM.
const double ORBITAL_ZOOM_MIN = 1.0;
const double ORBITAL_ZOOM_MAX = 20.0;
const double ORBITAL_ZOOM_USER_MAX = 8.0;

// The zoom at which a window of spanKm fills the viewport height. This
// is where a descent flight has to end for the next rung to take over
// on the same framing, with no jump.
double OrbitalZoomForSpan(double spanKm);

// The disc's radius in pixels for a viewport, at the camera's current
// zoom. One definition, shared by the renderer and both projections, so
// they cannot disagree about where the moon is.
double OrbitalDiscRadiusPx(int screenWidth, int screenHeight);

// Invert the orbital projection: turn a screen-space click on the moon
// into real lat/lon. Returns false if the click misses the globe (or
// lands on the limb, where the projection is degenerate).
bool OrbitalPickToLatLon(float screenX, float screenY,
                         int screenWidth, int screenHeight,
                         double* latDeg, double* lonDeg);

// Project lat/lon back onto the globe — the inverse of the above, for
// drawing markers. Returns false if the point is turned away.
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
    // Geometry is calibrated against the COLONY view, where the whole
    // site is visible. The sect view draws the same settlement at 0.63x
    // this physical size (fixed screen fractions, see sect.cpp), so the
    // chain scales these per level — otherwise the dome patches land
    // outside the 5 km sect window and the effect is invisible there.
    int domeCount = 8;             // units ringing the sect core
    float ringRadiusKm = 3.40f;    // radius the unit domes sit on
    float coreRadiusKm = 1.55f;    // the central dome
    float domeWorkKm = 1.20f;      // worked ground around each dome
    // How far the natural ground is calmed inside the site. Not a flat
    // platform: 0 keeps the wild terrain, 1 would erase it. The point is
    // to level the elevation swings and their shadows down to a calmer
    // baseline, then lay the worked undulations on top.
    float levelAmount = 0.70f;     // damping of natural elevation variation
    float toneLevelAmount = 0.55f; // damping of imagery contrast/shadows
    float undulationAmp = 0.0075f; // gentle mounds and hollows
    float roughAmp = 0.0045f;      // random fine alterations
    float spotAmp = 0.0060f;       // per-dome mound/hollow depth
    // Full-strength worked ground out to workedRadiusKm — which clears
    // the ring of unit domes (they reach ~4.34 km here) with margin —
    // then fades to untouched over the next fadeKm.
    float workedRadiusKm = 4.95f;  // encompasses the units, plus margin
    float fadeKm = 1.00f;          // fade band beyond the worked ground
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
// The span ladder a chain walks, widest first, in km. The game's is
// 100 / 25 / 5 -- PLANET, COLONY, SECT -- and that is what every caller
// gets by default. An instrument that frames its own window passes its
// own last span instead: a layer covering the central 56% of the window
// it is meant to texture is not a layer, it is a patch.
//
// The FIRST span is where the WAC macro crop is taken, so it is also the
// widest ground the chain has to invent detail from. Every span after it
// is a centre crop of the level above's OUTPUT, which is what registers
// the levels to each other and makes zooming continuous.
const int TERRAIN_CHAIN_MAX_LEVELS = 3;

struct TerrainChainSpans
{
    int count = 3;
    float km[TERRAIN_CHAIN_MAX_LEVELS] = {100.0f, 25.0f, 5.0f};
};

// The ladder for a window of an arbitrary span: the 100 km macro, then
// one crop straight to spanKm. A window at or wider than the macro takes
// its crop directly and has no second step, because there is nothing
// above it to crop from.
TerrainChainSpans TerrainChainSpansForWindow(double spanKm);

// Decode the WAC mosaic now rather than inside the first chain that
// wants it. It is one 8192x4096 JPEG and it dominates the first build,
// so paying for it where a pause is expected -- startup -- beats paying
// for it in the middle of a descent. Cheap and idempotent afterwards.
// Returns false if the mosaic is missing.
bool TerrainWarmMosaic();

// spans == nullptr walks the game's own 100 / 25 / 5.
void GenerateTerrainChain(double latDeg, double lonDeg, int res,
                          Image outLevels[3],
                          const TerrainSiteDisturbance* site = nullptr,
                          const TerrainChainSpans* spans = nullptr);

// Support for the GPU path (terrain_gpu.cpp), which runs the same chain
// as fragment-shader passes. It needs two things the CPU keeps to
// itself: the native-resolution WAC crop for the macro window (already
// denoised; a few thousand texels, so uploading it and letting the GPU
// upsample beats resizing here), and the adaptive-contrast stats
// SharpenAdaptive would derive for it. The crop is packed 16-bit into
// R (high byte) and G (low byte) so no float texture is needed anywhere
// -- WebGL1 has none. Caller owns the Image.
//
// It used to carry a location seed as well. Nothing seeds by location
// any more: the noise reads the ground through a world frame instead,
// so a seed would only be a way to put different detail on the same
// rock. See docs/graveyard.md, 8.
struct TerrainMacroCrop
{
    Image image = {};        // R8G8B8, width x height texels, 16-bit in R:G
    float gain = 1.0f;       // adaptive contrast gain (1..2.2)
    float mid = 0.5f;        // ...about this midpoint
    // Where the window sits inside the block, in texels: its top-left
    // corner and how many texels it spans. The block's bounds are whole
    // texels and the window's are not, so the consumer must sample by
    // position -- stretching the block edge to edge would make the
    // mapping from pixel to ground depend on the texel alignment, which
    // slips the imagery up to half a texel between neighbouring windows.
    float originX = 0.0f, originY = 0.0f;
    float spanX = 1.0f, spanY = 1.0f;
};
bool GetTerrainMacroCrop(double latDeg, double lonDeg, TerrainMacroCrop* out,
                         double spanKm = 100.0);

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

// The chain's two fields at its last level, BEFORE it lights them.
//
// GenerateTerrainChain returns a picture with a hillshade and a cast
// shadow march already baked into it. That is what the game's views
// want and it is the wrong thing for a consumer that has its own sun:
// laid under another light the ground is shaded twice, and being a
// picture it carries no relief that a mesh or a normal map can use.
//
// These are the two things the chain computes on the way to that
// picture. Height is the relief it would have lit -- the imagery's form
// re-read as topography, plus grain, undulation and boulders. Albedo is
// the surface with the speckle in it and no shading at all.
//
// Skipping the shading makes this CHEAPER than the lit chain, not dearer:
// the shadow march is the largest per-pixel cost in the fused pass.
//
// height is in chain units, near zero mean. Multiply by heightScaleM for
// metres -- the factor comes from the hillshade's own z, so it is the
// scale at which the chain treats this field as terrain in the first
// place, not a number invented here.
//
// The caller almost certainly wants to HIGH-PASS the height before using
// it: its long wavelengths are the imagery's landforms re-read as
// topography, which real elevation data already carries and which the
// chain cannot confirm. Everything below the data's own floor is what
// this is for.
struct TerrainChainFields
{
    std::vector<float> height;    // res * res, chain units
    std::vector<float> albedo;    // res * res, 0..1, unlit
    int res = 0;
    float heightScaleM = 0.0f;    // height * this = metres
};

// spanKm is the window the fields cover; the chain walks 100 km down to
// it exactly as GenerateTerrainChain does.
bool GenerateTerrainFields(double latDeg, double lonDeg, int res,
                           double spanKm, TerrainChainFields* out,
                           const TerrainSiteDisturbance* site = nullptr);

// Generate the SECT view ground for a location: a res x res RGB image
// of the 5 km cell, amplified through the real-imagery chain
// (100 km -> 25 km -> 5 km). Caller owns the Image (UnloadImage).
// Requires src/assets/planet/wac_global.jpg (loaded once, cached).
// tuning == nullptr uses the baseline TerrainTuning.
Image GenerateSectTerrain(double latDeg, double lonDeg, int res = 300,
                          const TerrainTuning* tuning = nullptr);

#endif // TERRAIN_SYNTHESIS_H
