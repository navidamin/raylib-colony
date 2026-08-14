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

// Playfield anchor: centre of the 20x20 planet grid on the real moon.
// Mare Imbrium — flat mare with a distinctive crater nearby.
const double TERRAIN_ANCHOR_LAT = 32.8;
const double TERRAIN_ANCHOR_LON = -15.6;

const double MOON_KM_PER_DEG = 30.32268;   // pi * 1737.4 / 180
const double TERRAIN_CELL_KM = 5.0;        // one grid cell, sect diameter

// Real lat/lon of a planet grid cell centre (gx, gy in 0..19; gy grows
// south, matching the game grid's y-down convention).
void TerrainGridCellToLatLon(int gx, int gy, double* latDeg, double* lonDeg);

// Generate the SECT view ground for a location: a res x res RGB image
// of the 5 km cell, amplified through the real-imagery chain
// (100 km -> 25 km -> 5 km). Caller owns the Image (UnloadImage).
// Requires src/assets/planet/wac_global.jpg (loaded once, cached).
Image GenerateSectTerrain(double latDeg, double lonDeg, int res = 300);

#endif // TERRAIN_SYNTHESIS_H
