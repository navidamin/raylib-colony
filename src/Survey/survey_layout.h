#pragma once

#include "raylib.h"

/* =====================================================================
   THE CONSOLE'S FRAME
   ---------------------------------------------------------------------
   Three columns, five panels, and a module bar across the top. Every
   rectangle comes out of this one function and NO GEOMETRY IS A
   CONSTANT -- which is the rule the design sets, because the console
   has to survive being re-proportioned.

   It is re-proportioned here already. The design is 1536 x 1024 and the
   game renders 1280 x 720 inside a top bar and a bottom bar, so the
   console is not the design scaled down: it is the design's COLUMN
   RATIOS (0.24 / 0.53 / 0.23) and row ratios (0.72 / 0.26) applied to
   whatever region it is handed.

   WHAT IS NOT HERE, and deliberately: the phone arrangement. In the
   browser prototype the page reflows and a breakpoint means something.
   The game renders one fixed 1280 x 720 frame and scales the canvas, so
   `GetScreenWidth()` says 1280 on a phone and on a desktop alike and a
   breakpoint here would be a lie. The phone problem is real -- a 12 px
   label scaled to a 400 px screen is unreadable -- and the design's
   answer (the block is the app, the rack and the bar are summoned) is
   still the right one. It needs the game to learn its DISPLAY size
   first, which is a web-shell change, not a layout change. Until then
   this returns one arrangement, and the shape of the function is what
   makes adding the other one cheap.
   ===================================================================== */
struct SurveyLayout
{
    Rectangle bar;        // the unit's module selector, along the top
    Rectangle survey;     // SURVEY TOOLS -- the rack
    Rectangle stats;      // TOOL STATS
    Rectangle layers;     // LAYERS -- the block
    Rectangle log;        // the survey log, nested in the lower part of LAYERS
    Rectangle drill;      // DRILL BAR
    Rectangle dstats;     // DRILL STATS
    Rectangle block;      // where the block itself is drawn, inside LAYERS
    Rectangle confidence; // the CONFIDENCE bar under the block

    // The two branch connectors, which are the only hierarchy the frame
    // expresses: TOOL STATS belongs to the rack, DRILL STATS to the bar.
    float leftBranchX = 0.0f;
    float rightBranchX = 0.0f;
};

SurveyLayout ComputeSurveyLayout(Rectangle region);
