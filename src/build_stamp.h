#ifndef BUILD_STAMP_H
#define BUILD_STAMP_H

#include "raylib.h"

#include <cstdio>

// Which build this is -- branch, commit, time -- drawn on the title screen,
// the one screen every session passes through and nothing else uses. (A
// corner of the play screens was tried: every corner of some screen is
// taken -- lunar_map's toggles, the walk's tabs, the Colony view's day.)
// Every page also carries it in its browser-tab title (StampedTitle).
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

// A window title with the build appended. On the web the window title IS
// the browser-tab title -- emscripten's GLFW writes it to document.title
// when the window opens -- so a page that did not pass its title through
// here would wipe the stamp the deploy wrote into the page's <title>.
// The buffer is static because raylib keeps the pointer, not a copy.
inline const char* StampedTitle(const char* title)
{
#ifdef COLONY_BUILD_STAMP
    static char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s - %s", title, COLONY_BUILD_STAMP);
    return buffer;
#else
    return title;
#endif
}

#endif
