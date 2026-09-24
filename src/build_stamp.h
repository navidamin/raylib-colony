#ifndef BUILD_STAMP_H
#define BUILD_STAMP_H

#include "raylib.h"

// Which build this is -- branch, commit, time -- drawn on the title screen,
// the one screen every session passes through and nothing else uses. (A
// corner of the play screens was tried: every corner of some screen is
// taken -- lunar_map's toggles, the walk's tabs, the Colony view's day.)
// The deploy also writes it into each page's browser-tab title.
//
// It exists because the Pages site is shared: every branch that deploys
// replaces every URL on it, and on 2026-09-24 a playtest of this branch's
// game turned out to be another branch's month-old build, with no way to
// tell from the screen. The deploy sets COLONY_BUILD_STAMP (CMake cache
// variable of the same name); a local build leaves it empty and draws
// nothing, so headless renders are unchanged.
inline void DrawBuildStamp(int centreX, int y, int size, Color colour)
{
#ifdef COLONY_BUILD_STAMP
    const char* stamp = COLONY_BUILD_STAMP;
    DrawText(stamp, centreX - MeasureText(stamp, size)/2, y, size, colour);
#else
    (void)centreX; (void)y; (void)size; (void)colour;
#endif
}

#endif
