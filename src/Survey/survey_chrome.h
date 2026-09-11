#pragma once

#include "survey_constants.h"
#include "raylib.h"

/* =====================================================================
   THE CONSOLE'S FOUR CHROME IDIOMS
   ---------------------------------------------------------------------
   The whole frame is built from these and nothing else, which is what
   stops five panels reading as five unrelated windows. From the design,
   section 3: "the four chrome idioms".

   1. BRACKET PANEL   a dim rounded outline with four bright corner
                      brackets ON TOP of it -- the outline is punched
                      just past each leg so the bracket reads as a
                      separate object laid over the frame, not as part
                      of its border.
   2. BRANCH CONNECTOR a short vertical stem with a pip at each end,
                      tying a lower panel to the one above it. The only
                      hierarchy the frame expresses.
   3. SEGMENT BAR     every meter in the console is this one widget.
   4. HEX VIGNETTE    a flattened hexagon that frames the block without
                      boxing it.
   ===================================================================== */
namespace SurveyChrome
{
    void Panel(Rectangle r, Color fill = SC_PANEL, Color ground = SC_BG);
    void Box(Rectangle r);                       // a plain box nested inside a panel
    void Connector(float x, float yFrom, float yTo);
    void SegBar(Rectangle r, float value01, Color on, int segments = 0);
    void HexVignette(float cx, float cy, float rx, float ry);

    // A panel title with the accent underline the design specifies.
    void Title(const Font& font, const char* text, float x, float y, float size);
    // Right-aligned and centred label helpers, because hard-coded offsets are
    // what produced every overlap this UI has ever had.
    void LabelRight(const Font& font, const char* text, float rightX, float y,
                    float size, Color col);
    void LabelCentre(const Font& font, const char* text, float centreX, float y,
                     float size, Color col);

    // Anything that is better high and worse low reads on the health ramp.
    Color Health(float value01);
}
