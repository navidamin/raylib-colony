#ifndef RELIEF_H
#define RELIEF_H

// The moon's real relief, for the district level.
//
// Heights measured by stereo from Kaguya's Terrain Camera (the USGS
// Astrogeology DTMs), resampled to 128 px/deg (237 m) and stored as the
// height ABOVE the LOLA model the game already ships: 8-bit JPEG tiles, 4 x 4
// degrees each, detail = round((z - base) / 7 m) + 128, where base is the
// bilinear LOLA sample at the pixel centre (LolaDem::GlobalBilinearM). The
// whole moon is 85 MB, so it is streamed, never preloaded: read from disk
// on the desktop, fetched a tile at a time in the browser. Built by
// tools/relief/build_relief.py.
//
// Why it exists: at 200 km the synthesizer's inventions read as noise and
// the 1.33 km mosaic under them is soft; the study on 2026-09-24 (Sinus
// Medii, Tycho, Hadley, Tsiolkovskiy) found real relief lit by the game's
// own sun clearly better at every one, and the mosaic alone better than the
// synthesis. See docs/design/site-selection/level2-relief.md.

#include <vector>

// Windows at least this wide (km north-south) are drawn from real relief:
// the district level (200 km, built 355.6 km across a landscape screen) and
// its zoomed-out view. Level 3 and the Colony view (built 44.4 km) keep the
// synthesizer, whose detail is what they are for.
const double RELIEF_MIN_SPAN_KM = 150.0;

// Their colour is the mosaic blurred by this much. The mosaic was
// photographed under its own sun (at Sinus Medii from the west, 45 deg up);
// left sharp, that light and the relief's -- the game's, north-west at 35 --
// both shade every crater, which reads as ghost craters. Blurred, only its
// albedo remains and all the shading is the relief's.
const double RELIEF_ALBEDO_BLUR_KM = 2.5;

// Which ground the district is drawn with, chosen once:
// COLONY_DISTRICT=relief|super on the desktop, ?district=relief|super in a
// browser.
//   RELIEF (the default): the moon's measured relief, as above.
//   SUPERSAMPLED: the terrain synthesizer -- the ground the site level is
//     built from, so a descent continues it unbroken -- built at
//     DISTRICT_SUPERSAMPLE times the size it is drawn and averaged down
//     (on the GPU; the CPU path builds it at its budgeted size). Option 2 of
//     the study, kept as a playtest beside the relief.
// Every relief entry point below answers as if no tile existed unless the
// style is RELIEF, so nothing else has to ask.
enum class DistrictStyle { RELIEF, SUPERSAMPLED };
DistrictStyle GetDistrictStyle();
const char* DistrictStyleName();          // "real relief" | "supersampled"
const int DISTRICT_SUPERSAMPLE = 2;

// Where the tiles live. On the desktop a directory (default data/relief,
// relative to the working directory); in the browser a URL prefix (default
// window.COLONY_RELIEF_URL, set by the deploy, else "relief/").
void SetReliefSource(const char* dirOrUrl);

// A window's heights in metres on the chain's own frame -- res x res, row 0
// north, spanKm north-south about the centre and widened by 1/cos(lat) east-
// west, exactly as RunChainGPU and MakeNoiseFrame frame it. False when the
// window cannot be drawn from real relief: a tile it needs is still on its
// way (the browser), or too little of it is covered by measured relief.
bool ReliefWindowM(double latDeg, double lonDeg, double spanKm, int res,
                   std::vector<float>* metres);

// Start fetching the tiles a window will need. Free on the desktop; in the
// browser it lets a flight or a drag hide the download.
void ReliefPrefetch(double latDeg, double lonDeg, double spanKm);

// Every tile the window needs has arrived or is known to be absent, so a
// build now is final. A window built before this was built without relief.
bool ReliefWindowSettled(double latDeg, double lonDeg, double spanKm);

// Settled, and covered well enough that ReliefWindowM draws it from real
// relief rather than handing it back to the synthesizer.
bool ReliefWindowCovered(double latDeg, double lonDeg, double spanKm);

#endif // RELIEF_H
