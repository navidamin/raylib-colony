#include "survey_rack.h"
#include "survey_console.h"
#include "survey_constants.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    /* The rack's own palette. Machined metal, not hologram: a steel body over
       a dark well, a highlight along the top edge and a shadow along the
       bottom, and the slot light the only cyan in it. */
    constexpr Color RK_BODY    = { 58,  70,  84, 255};
    constexpr Color RK_BODY_HI = { 92, 108, 126, 255};
    constexpr Color RK_BODY_LO = { 30,  38,  48, 255};
    constexpr Color RK_WELL    = { 14,  20,  27, 255};
    constexpr Color RK_STUD    = {126, 142, 160, 255};
    constexpr Color RK_DEAD    = { 34,  42,  52, 255};

    void Stud(float x, float y, float r, Color c)
    {
        DrawCircleV({x, y}, r, c);
        DrawCircleV({x - r * 0.25f, y - r * 0.25f}, r * 0.45f, Fade(WHITE, 0.30f));
    }
}

SurveyTool SurveyRack::Selected(const SurveyConsole& console)
{
    return console.SelectedTool();
}

bool SurveyRack::Draw(SurveyConsole& console, Rectangle panel, Vector2 mouse,
                      const Font& headerFont, const Font& bodyFont)
{
    const int n = static_cast<int>(SurveyTool::COUNT);
    const float gap = 5.0f;
    const float bayH = (panel.height - gap * (n - 1)) / n;
    const float sprUnit = std::min(4.0f, std::floor(bayH / 9.0f));   // whole pixels only
    bool picked = false;

    for (int k = 0; k < n; k++)
    {
        const SurveyTool tool = static_cast<SurveyTool>(k);
        const SurveyToolInfo& info = SurveyToolOf(tool);
        Rectangle bay = { panel.x, panel.y + k * (bayH + gap), panel.width, bayH };
        const bool selected = console.SelectedTool() == tool;
        const bool hover = CheckCollisionPointRec(mouse, bay);

        // ---- the steel body ----
        const Color body = info.built ? (hover || selected ? RK_BODY_HI : RK_BODY) : RK_DEAD;
        DrawRectangleRounded(bay, 0.22f, 4, body);
        // a highlight along the top edge and a shadow along the bottom: two
        // lines are the whole difference between a rectangle and a machined
        // face with a light over it
        DrawLineEx({bay.x + 4.0f, bay.y + 1.0f}, {bay.x + bay.width - 4.0f, bay.y + 1.0f},
                   1.5f, Fade(WHITE, info.built ? 0.16f : 0.06f));
        DrawLineEx({bay.x + 4.0f, bay.y + bay.height - 1.0f},
                   {bay.x + bay.width - 4.0f, bay.y + bay.height - 1.0f}, 1.5f, Fade(BLACK, 0.35f));
        DrawRectangleRoundedLinesEx(bay, 0.22f, 4, selected ? 1.8f : 1.0f,
                                    selected ? SC_ACCENT : Fade(RK_BODY_LO, 0.9f));

        // ---- the well the tool stands in ----
        const float wellW = std::min(46.0f, bay.width * 0.30f);
        Rectangle well = { bay.x + 6.0f, bay.y + 5.0f, wellW, bay.height - 10.0f };
        DrawRectangleRounded(well, 0.18f, 4, RK_WELL);
        DrawRectangleRoundedLinesEx(well, 0.18f, 4, 1.0f, Fade(RK_BODY_LO, 1.0f));
        if (info.built)
        {
            // The sprite stands on the well's floor, on whole pixels, and it
            // is the SAME sprite the rig plants in the ground.
            DrawSurveyToolSprite(tool, std::round(well.x + well.width * 0.5f),
                                 std::round(well.y + well.height - 4.0f), 0, sprUnit);
        }
        else
        {
            // an empty mount: the bracket is there, the tool is not
            DrawRectangleLinesEx({well.x + well.width * 0.5f - 6.0f,
                                  well.y + well.height * 0.5f - 6.0f, 12.0f, 12.0f},
                                 1.0f, Fade(RK_STUD, 0.35f));
        }

        // ---- studs, name, mode tag ----
        Stud(bay.x + bay.width - 7.0f, bay.y + 7.0f, 2.2f, Fade(RK_STUD, info.built ? 1.0f : 0.4f));
        Stud(bay.x + bay.width - 7.0f, bay.y + bay.height - 7.0f, 2.2f,
             Fade(RK_STUD, info.built ? 1.0f : 0.4f));

        const float tx = well.x + well.width + 9.0f;
        const float nameSize = std::max(9.0f, bayH * 0.24f);
        const float tagSize = std::max(7.5f, bayH * 0.18f);
        DrawTextEx(headerFont, info.name, {tx, bay.y + 6.0f}, nameSize, 1.0f,
                   info.built ? (selected ? SC_ACCENT : SC_BRIGHT) : Fade(SC_DIM, 0.8f));
        const char* mark = info.mark == SurveyToolMark::POINT ? "POINT" : "LINE";
        const float tagY = bay.y + 8.0f + nameSize;
        DrawTextEx(bodyFont, mark, {tx, tagY}, tagSize, 1.0f, Fade(SC_DIM, 0.9f));
        /* The blurb -- what the instrument is FOR -- belongs to the SELECTED
           tool and lives in TOOL STATS, not in the bay. Five blurbs at once is
           a paragraph in a rack, and none of them fits the width anyway; one
           blurb under the bay you picked is a caption. */
        if (!info.built)
        {
            DrawTextEx(bodyFont, "NO TOOL FITTED",
                       {tx + MeasureTextEx(bodyFont, mark, tagSize, 1.0f).x + 8.0f, tagY},
                       tagSize, 1.0f, Fade(SC_WARN, 0.7f));
        }

        /* ---- the slot light ----
           Lit when the bay is powered, dark when the mount is empty, and
           brightest on the bay you have selected. It is the only cyan on the
           rack, which is what makes it read as an indicator rather than as
           more chrome. */
        Rectangle lamp = { bay.x + bay.width - 16.0f, bay.y + bay.height * 0.5f - 4.0f,
                           7.0f, 8.0f };
        const Color lit = !info.built ? Fade(RK_WELL, 1.0f)
                        : (selected ? SC_METER_ON : Fade(SC_METER_ON, 0.42f));
        if (info.built && selected)
            DrawRectangleRounded({lamp.x - 2.0f, lamp.y - 2.0f, lamp.width + 4.0f,
                                  lamp.height + 4.0f}, 0.4f, 4, Fade(SC_METER_ON, 0.22f));
        DrawRectangleRounded(lamp, 0.4f, 4, lit);
        DrawRectangleRoundedLinesEx(lamp, 0.4f, 4, 1.0f, Fade(BLACK, 0.5f));

        if (hover && info.built && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            console.SelectTool(tool);
            picked = true;
        }
    }
    return picked;
}
