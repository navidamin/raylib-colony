#ifndef LUNAR_DEM_SHARED_H
#define LUNAR_DEM_SHARED_H

// The one LOLA elevation model a process loads.
//
// Three things need real elevation: the survey grid's terrain rows
// (ResourceManager), the site verdict (SiteSelection) and the instrument's
// own renderer (lunar_map). Each used to load its own copy from a path it
// spelled itself. This is the single accessor they share: one 33 MB grid,
// one path constant, loaded on first use and never again.
//
// Returns nullptr when the file is missing, and every caller already has
// a fallback for that -- the game runs with synthetic terrain rows and no
// verdict rather than refusing to start.

#include "lola_dem.h"

// Where the DEM lives in the repository. The web build preloads it at
// the same virtual path, so one spelling serves both.
const char* LunarDemDefaultPath();

// Override the path before the first GetLunarDem(). Used by lunar_map's
// --dem. Ignored (with a warning) once the model is loaded.
void SetLunarDemPath(const char* path);

// Loads on first call: the global model, then any high-resolution
// overlays sitting beside it (fetch-dem's sldem_*_512.tif). Thread-safe to
// READ afterwards; the first call must happen on the main thread before
// any worker asks.
const LolaDem* GetLunarDem();

#endif // LUNAR_DEM_SHARED_H
