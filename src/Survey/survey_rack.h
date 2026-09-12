#pragma once

#include "survey_sprites.h"
#include "raylib.h"

class SurveyConsole;

/* =====================================================================
   THE SURVEY TOOL RACK
   ---------------------------------------------------------------------
   Five bays holding five instruments, and the one place in this console
   that is NOT a hologram. The block, the ruler, the confidence bar and
   the log are all readouts of measured ground; the rack is a physical
   object sitting on top of them -- steel bodies, an inset well per bay,
   studs, and a slot light that is lit when the bay is powered. That
   contrast is the point, and it is why the rack keeps its own greys
   instead of borrowing the console's navy.

   ONLY TWO OF THE FIVE WORK, and the rack says so rather than hiding
   it: an unbuilt bay has a dark slot light and a dimmed body. A rack
   that pretends to hold five working tools is a menu of lies; a rack
   that holds two working tools and three empty mounts is a machine you
   can see the future of.
   ===================================================================== */
namespace SurveyRack
{
    // Draws the rack into a panel and handles its own selection. Returns
    // true if a bay was picked this frame.
    bool Draw(SurveyConsole& console, Rectangle panel, Vector2 mouse,
              const Font& headerFont, const Font& bodyFont);

    SurveyTool Selected(const SurveyConsole& console);
}
