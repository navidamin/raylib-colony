// unit_icons.h — the unit icon set on the sect view's domes.
//
// The icons are the user's (prototypes/unit-icons/icons.js): SVG drawn on a
// 100x100 grid, ink and knockout. tools/unit_icons/gen_unit_icons.js runs that
// JS and writes the final markup to unit_icons_svg.h; here it is rasterised
// with nanosvg (src/external/nanosvg, zlib licence), 3x supersampled, and
// turned into a tinted icon with a soft shadow so it reads on the glass.
#ifndef UNIT_ICONS_H
#define UNIT_ICONS_H

#include "raylib.h"

#include <string>

// The icon key for a unit type as the game names it ("Manufacture" ->
// "manufacturing"); empty if there is none.
std::string UnitIconKey(const std::string& unitType);

// The icon at px x px, ink in `ink`, with a soft drop shadow in `shadow`
// (alpha 0 = none). An empty image if the key is unknown. Caller unloads.
Image UnitIconImage(const std::string& key, int px, Color ink, Color shadow);

#endif // UNIT_ICONS_H
