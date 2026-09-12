#include "survey_rack.h"
#include "survey_console.h"
#include "survey_constants.h"

#include <algorithm>
#include <cmath>
#include <string>

/* =====================================================================
   Drawn against the reference render (prototypes/dashboard/dashboard.png),
   not from a description of it. The rack is a MACHINED OBJECT and every
   part of it is a part a machinist would recognise:

     the backplate   a steel sheet with a border and corner bolts
     the spine       a raised rail down its left edge, bolted top and
                     bottom, with a hinge plate at the head
     the tiles       square bezels, four bolts each, a dark inset well,
                     and the tool's own glyph standing in it
     the slot light  a cyan capsule on the spine beside each tile: this
                     bay is powered
     the name plate  a tapered plate cantilevered off the tile, carrying
                     the tool's name and whether it is a POINT or a LINE
                     instrument, with a second lozenge at its far end

   Every one of them is lit from the upper left, which is where the rest
   of this UI's light comes from: a highlight along each top edge and a
   shadow along each bottom one. Two lines are the whole difference
   between a rectangle and a machined face.

   AN EMPTY BAY IS NOT A HIDDEN BAY. The reference shows the mount still
   there -- the bezel, the bolts, the dark square where the tool would
   clip in -- and only a stub where the name plate would be. A rack that
   pretends to hold five working tools is a menu of lies; one that holds
   two and three empty mounts is a machine you can see the future of.
   ===================================================================== */
namespace
{
    // The rack's own greys. It is a physical object sitting on a
    // holographic console, and it keeps its own palette so that reads.
    constexpr Color RK_PLATE   = { 38,  48,  60, 255};   // the backplate
    constexpr Color RK_SPINE   = { 47,  59,  73, 255};
    constexpr Color RK_BODY    = { 66,  82, 100, 255};   // bezels and name plates
    constexpr Color RK_BODY_HI = {103, 122, 143, 255};
    constexpr Color RK_BODY_LO = { 22,  29,  38, 255};
    constexpr Color RK_WELL    = { 11,  17,  24, 255};   // the inset a tool sits in
    constexpr Color RK_BOLT    = {138, 154, 172, 255};
    constexpr Color RK_EDGE    = { 79,  95, 113, 255};
    constexpr Color RK_LAMP_OFF= { 20,  32,  42, 255};

    void Bolt(float x, float y, float r)
    {
        DrawCircleV({x, y}, r + 0.8f, Fade(BLACK, 0.45f));
        DrawCircleV({x, y}, r, RK_BOLT);
        DrawCircleV({x - r * 0.28f, y - r * 0.28f}, r * 0.5f, Fade(WHITE, 0.45f));
        DrawCircleV({x, y}, r * 0.34f, Fade(BLACK, 0.35f));
    }

    // A machined face: fill, a highlight along the top edge, a shadow along
    // the bottom, and a hairline border. Everything on this rack is one.
    void Face(Rectangle r, Color fill, float round, Color edge)
    {
        DrawRectangleRounded(r, round, 5, fill);
        DrawLineEx({r.x + 3.0f, r.y + 1.2f}, {r.x + r.width - 3.0f, r.y + 1.2f},
                   1.6f, Fade(WHITE, 0.16f));
        DrawLineEx({r.x + 3.0f, r.y + r.height - 1.2f},
                   {r.x + r.width - 3.0f, r.y + r.height - 1.2f}, 1.6f, Fade(BLACK, 0.40f));
        DrawRectangleRoundedLinesEx(r, round, 5, 1.0f, edge);
    }

    /* The slot light. A capsule, not a dot: on the reference it is a long
       lozenge standing in the spine, which is what makes it read as a
       fitting rather than as an LED stuck on. */
    void Lamp(Rectangle r, Color col, bool glow)
    {
        if (glow)
            DrawRectangleRounded({r.x - 2.5f, r.y - 2.5f, r.width + 5.0f, r.height + 5.0f},
                                 1.0f, 6, Fade(col, 0.22f));
        DrawRectangleRounded(r, 1.0f, 6, col);
        DrawRectangleRounded({r.x + r.width * 0.28f, r.y + 2.0f,
                              r.width * 0.28f, r.height - 4.0f}, 1.0f, 4, Fade(WHITE, 0.28f));
        DrawRectangleRoundedLinesEx(r, 1.0f, 6, 1.0f, Fade(BLACK, 0.55f));
    }

    /* The name plate, cantilevered off the tile and CHAMFERED at its far
       end -- the reference cuts both right corners back, which is what stops
       a row of five plates reading as five buttons. */
    void Plate(Rectangle r, float chamfer, Color fill, Color edge)
    {
        Vector2 p[6] = {
            { r.x,                       r.y },
            { r.x + r.width - chamfer,   r.y },
            { r.x + r.width,             r.y + r.height * 0.5f },
            { r.x + r.width - chamfer,   r.y + r.height },
            { r.x,                       r.y + r.height },
            { r.x,                       r.y },
        };
        DrawTriangleFan(p, 5, fill);
        DrawLineEx(p[0], p[1], 1.6f, Fade(WHITE, 0.14f));
        DrawLineEx(p[3], p[4], 1.6f, Fade(BLACK, 0.40f));
        DrawLineStrip(p, 6, edge);
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
    bool picked = false;

    // ---- the backplate ----
    Face(panel, RK_PLATE, 0.06f, RK_BODY_LO);
    DrawRectangleRoundedLinesEx({panel.x + 4.0f, panel.y + 4.0f,
                                 panel.width - 8.0f, panel.height - 8.0f},
                                0.05f, 5, 1.0f, Fade(RK_EDGE, 0.55f));
    for (int c = 0; c < 4; c++)
        Bolt(panel.x + (c % 2 ? panel.width - 9.0f : 9.0f),
             panel.y + (c / 2 ? panel.height - 9.0f : 9.0f), 2.6f);

    // ---- the spine: a raised rail down the left edge ----
    const float spineW = 20.0f;
    Rectangle spine = { panel.x + 10.0f, panel.y + 12.0f, spineW, panel.height - 24.0f };
    Face(spine, RK_SPINE, 0.14f, RK_BODY_LO);
    // the hinge plate at its head, and a bolt at each end of the rail
    Face({spine.x - 2.0f, spine.y + 2.0f, spineW + 4.0f, 13.0f}, RK_BODY, 0.35f, RK_BODY_LO);
    Bolt(spine.x + spineW * 0.5f, spine.y + 8.5f, 2.2f);
    Bolt(spine.x + spineW * 0.5f, spine.y + spine.height - 7.0f, 2.2f);

    const float bayTop = spine.y + 20.0f;
    const float bayH = (spine.height - 26.0f) / n;
    const float tile = std::min(bayH - 6.0f, 54.0f);
    const float tileX = spine.x + spineW + 7.0f;

    for (int k = 0; k < n; k++)
    {
        const SurveyTool tool = static_cast<SurveyTool>(k);
        const SurveyToolInfo& info = SurveyToolOf(tool);
        const float cy = bayTop + (k + 0.5f) * bayH;
        Rectangle bezel = { tileX, cy - tile * 0.5f, tile, tile };
        // the whole bay is the target, so a tap does not have to find the tile
        Rectangle hitBox = { spine.x, cy - bayH * 0.5f,
                             panel.x + panel.width - spine.x - 8.0f, bayH };
        const bool selected = console.SelectedTool() == tool;
        const bool hover = CheckCollisionPointRec(mouse, hitBox);

        // ---- the slot light, on the spine beside its bay ----
        Lamp({ spine.x + spineW * 0.5f - 3.5f, cy - 13.0f, 7.0f, 26.0f },
             !info.built ? RK_LAMP_OFF
                         : (selected ? SC_METER_ON : Fade(SC_METER_ON, 0.45f)),
             info.built && selected);

        // ---- the tile ----
        Face(bezel, hover && info.built ? RK_BODY_HI : RK_BODY, 0.16f,
             selected ? SC_ACCENT : RK_BODY_LO);
        if (selected)
            DrawRectangleRoundedLinesEx({bezel.x - 1.0f, bezel.y - 1.0f,
                                         bezel.width + 2.0f, bezel.height + 2.0f},
                                        0.16f, 5, 1.0f, Fade(SC_ACCENT, 0.55f));
        const float inset = 5.0f;
        Rectangle well = { bezel.x + inset, bezel.y + inset,
                           bezel.width - inset * 2.0f, bezel.height - inset * 2.0f };
        DrawRectangleRounded(well, 0.12f, 5, RK_WELL);
        DrawRectangleRoundedLinesEx(well, 0.12f, 5, 1.0f, Fade(BLACK, 0.5f));
        for (int c = 0; c < 4; c++)
            Bolt(bezel.x + (c % 2 ? bezel.width - 4.5f : 4.5f),
                 bezel.y + (c / 2 ? bezel.height - 4.5f : 4.5f), 1.7f);

        if (info.built)
        {
            /* THE GLYPH, and it is the same glyph the rig plants in the
               ground -- the same grid, the same silhouette -- standing on the
               same mark it will stand on out there: a ring for a POINT tool,
               a traverse for a LINE one. The rack draws it at icon scale and
               in the console's cyan rather than in the rig's steel, because
               here it is an ICON of the tool and out there it is the tool. */
            // Whole pixels only, sized so the sprite fills about three
            // quarters of the well -- a pixel icon at a fractional unit is a
            // smooth icon with a pixel texture.
            const float u = std::max(2.0f, std::floor(well.height * 0.115f));
            const float gx = std::round(well.x + well.width * 0.5f);
            const float gy = std::round(well.y + well.height - u * 1.1f);
            if (info.mark == SurveyToolMark::POINT)
            {
                // the ground ring, open where the tool stands in it
                const float rx = well.width * 0.34f, ry = rx * 0.42f;
                for (int d = 0; d < 12; d++)
                {
                    const float a = d / 12.0f * 2.0f * PI;
                    if (std::fabs(a - PI * 1.5f) < 0.55f) continue;
                    DrawRectangle(static_cast<int>(gx + std::cos(a) * rx - 1.5f),
                                  static_cast<int>(gy + std::sin(a) * ry - 1.0f),
                                  3, 2, Fade(SC_ACCENT, 0.85f));
                }
            }
            else
            {
                // the traverse: a line tool is dragged, not put down
                for (float px = -well.width * 0.36f; px <= well.width * 0.36f; px += 5.0f)
                {
                    if (std::fabs(px) < u) continue;
                    DrawRectangle(static_cast<int>(gx + px - 1.0f), static_cast<int>(gy - 1.0f),
                                  2, 3, Fade(SC_ACCENT, 0.85f));
                }
            }
            /* In the console's cyan, not the rig's steel: here it is an icon
               OF the tool, out on the block it is the tool. */
            DrawSurveyToolSprite(tool, gx, gy - 2.0f, 0, u,
                                 Color{224, 252, 255, 255}, Color{46, 168, 204, 255});
        }
        else
        {
            // the empty mount: the bracket is there, the tool is not
            const float s = well.height * 0.42f;
            DrawRectangleRec({well.x + (well.width - s) * 0.5f,
                              well.y + (well.height - s) * 0.5f, s, s}, Fade(BLACK, 0.55f));
            DrawRectangleLinesEx({well.x + (well.width - s) * 0.5f,
                                  well.y + (well.height - s) * 0.5f, s, s},
                                 1.0f, Fade(RK_EDGE, 0.35f));
        }

        /* ---- the name plate ----
           Full length and named when the bay holds a tool; a short blank stub
           when it does not, which is exactly how the reference distinguishes
           them and is far quieter than a label saying EMPTY five times. */
        const float plateX = bezel.x + bezel.width + 4.0f;
        const float plateRight = panel.x + panel.width - 10.0f;
        const float plateH = std::min(tile * 0.78f, bayH - 8.0f);
        Rectangle plate = { plateX, cy - plateH * 0.5f,
                            info.built ? (plateRight - plateX) : 26.0f, plateH };
        Plate(plate, plateH * 0.42f,
              info.built ? (hover ? RK_BODY_HI : RK_BODY) : RK_SPINE,
              selected ? SC_ACCENT : RK_BODY_LO);

        if (info.built)
        {
            /* Measured and stacked from the plate's CENTRE, so the two lines
               sit on it however tall it comes out -- placed from the top they
               ran off the bottom edge the moment the panel got shorter. */
            const float nameSize = std::max(9.0f, plateH * 0.33f);
            const float tagSize = std::max(7.0f, plateH * 0.24f);
            const float block = nameSize + tagSize + 2.0f;
            const float ty = cy - block * 0.5f;
            DrawTextEx(headerFont, info.name, {plate.x + 9.0f, ty}, nameSize, 1.0f,
                       selected ? SC_ACCENT : SC_BRIGHT);
            DrawTextEx(bodyFont, info.mark == SurveyToolMark::POINT ? "Point" : "Line",
                       {plate.x + 9.0f, ty + nameSize + 2.0f}, tagSize, 1.0f,
                       SC_LABEL);
            // the second lozenge, at the plate's far end
            Lamp({ plate.x + plate.width - plateH * 0.42f - 9.0f, cy - plateH * 0.26f,
                   5.0f, plateH * 0.52f },
                 selected ? SC_METER_ON : Fade(SC_METER_ON, 0.40f), false);
        }

        if (hover && info.built && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            console.SelectTool(tool);
            picked = true;
        }
    }
    return picked;
}
