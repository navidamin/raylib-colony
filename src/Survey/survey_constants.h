#pragma once

/* =====================================================================
   THE SURVEY CONSOLE -- shared constants
   ---------------------------------------------------------------------
   The console is prospecting's and excavation's shared picture of the
   ground: one solid body cut into four beds, turned in yaw and pitch,
   drawn only where it has been measured.

   Design of record: docs/design/prospecting/survey-dashboard-design.md
   Build plan:       docs/design/prospecting/survey-dashboard-implementation.md
   The HTML prototype that settled the look, stage by stage, is
   docs/design/prospecting/prototypes/survey-dashboard.html -- every
   number below came from it and was verified by render there first.
   ===================================================================== */

#include "raylib.h"
#include "bed_palette.h"

#include "prospecting_constants.h"
#include "subsurface.h"

// ---- the ground the block is a picture of --------------------------------
// 6 km of ground centred on a sect, SUB_COLUMN_M deep. The lattice is the
// resolution the height fields are generated at, NOT the resource grid's:
// this is a picture of the rock, and it is sampled finer than the game's
// 32x32 cells.
constexpr int   SURVEY_LATTICE      = 28;
/* Bed thicknesses, metres. Four beds, five interfaces -- and they are THE
   GAME'S FOUR DEPTH LAYERS, taken from the table that prices a hole and gates
   a dig rather than restated here. The block is now a picture of the ground
   the drill actually reaches. */
constexpr float SURVEY_T0 = LAYER_THICKNESS_M[0];
constexpr float SURVEY_T1 = LAYER_THICKNESS_M[1];
constexpr float SURVEY_T2 = LAYER_THICKNESS_M[2];
constexpr float SURVEY_T3 = LAYER_THICKNESS_M[3];
constexpr int   SURVEY_BEDS = 4;
constexpr float SURVEY_COLUMN_M = SURVEY_T0 + SURVEY_T1 + SURVEY_T2 + SURVEY_T3;

static_assert(SURVEY_COLUMN_M == SUB_COLUMN_M,
              "the block and the drill must be a picture of the same ground");

/* EVERY VERTICAL MEASURE IS A FRACTION OF THE COLUMN, not an absolute in
   metres. The block is drawn as depth / columnM throughout, so a fraction
   keeps the picture identical whatever the column is, and an absolute does
   not. The ratios below are the ones the look was settled at, over the 2 km
   column it was settled on: 93/2000, 67/2000, 90/2000, 45/2000, 18/2000,
   250/2000. See docs/design/subsurface/README.md. */
constexpr float SURVEY_RELIEF_M     = SURVEY_COLUMN_M * 0.0465f; // surface undulation
// Broad rolls on the surface alone -- the ups and downs of a regolith plain,
// wider than the mottle and not inherited by the beds below it.
constexpr float SURVEY_ROLL_M       = SURVEY_COLUMN_M * 0.030f;
constexpr int   SURVEY_ROLL_FEATURE = 2;       // rolls across the block
constexpr float SURVEY_FEATURE      = 3.5f;    // noise period, in features across the block
constexpr int   SURVEY_DETAIL       = 3;       // fbm octaves
constexpr float SURVEY_CONFORM      = 0.62f;   // how much each bed inherits from the one above
constexpr float SURVEY_DAMP         = 0.55f;   // how fast relief dies with depth
constexpr float SURVEY_DIP_M        = SURVEY_COLUMN_M * 0.0335f; // regional dip
constexpr float SURVEY_DIP_AZ_DEG   = 25.0f;
constexpr float SURVEY_DIP_SPREAD   = 0.35f;   // how the dip fans with depth
constexpr float SURVEY_GRAIN        = 0.25f;   // squashes the noise domain: mottle becomes ridges

// Craters, hashed so they are the same every load, each kept off the middle
// -- a crater under the sect is not a crater, it is a mistake. The regolith
// surface is pocked, not dimpled: eight of them, on a power law (many small,
// one or two big), each as deep as it is wide in proportion, as real simple
// craters are. Three small ones fell between the block's samples and the
// cap read as smooth.
constexpr int   SURVEY_CRATER_N     = 8;
constexpr float SURVEY_CRATER_SIZE  = 1.0f;
constexpr float SURVEY_CRATER_DEPTH = SURVEY_COLUMN_M * 0.075f; // the biggest one's bowl
constexpr int   SURVEY_CRATER_LAYER = 0;       // which interface they were cut into
constexpr float SURVEY_CRATER_RIM   = 0.34f;
constexpr float SURVEY_CRATER_INFILL= 0.55f;

// ---- what a borehole is worth, and how far -------------------------------
/* MOVED. These constants now live in src/ui/dash_knowledge.h as DK_K_*,
   which is the one implementation of the knowledge model -- the values are
   unchanged, they just have one home instead of two. */

// ---- the scour a hole leaves ---------------------------------------------
constexpr float SURVEY_SCOUR_R      = 1.5f;   // lattice cells -- about 320 m
constexpr float SURVEY_SCOUR_DEEP_M = SURVEY_COLUMN_M * 0.0225f; // bowl depth
constexpr float SURVEY_SCOUR_RIM_M  = SURVEY_COLUMN_M * 0.009f;  // spoil rim

// ---- the palette ---------------------------------------------------------
/* Navy and cyan, from the console design. This is NOT the extraction UI's
   near-black and amber: the console is a readout of measured ground, and
   Dark Plating covers the machinery (the drill) drawn on top of it.
   See dark-plating.md 10b for where the boundary runs. */
constexpr Color SURVEY_GROUND      = {  2,  11,  19, 255 };
constexpr Color SURVEY_PANEL       = {  6,  20,  31, 255 };
constexpr Color SURVEY_CAGE        = {110, 190, 225, 255 };
constexpr Color SURVEY_RIM         = {230, 255, 255, 255 };
constexpr Color SURVEY_BASE_RING   = { 95, 240, 255, 255 };

/* One bed's four tones plus its mesh, from bed_palette.h -- the one table
   the console's block, its drill bar and its core cards read too. */
struct SurveyBedPalette
{
    const char* name;
    Color neon;
    Color mid;
    Color deep;
    Color line;
    Color mesh;
};
constexpr SurveyBedPalette SurveyBedFromPalette(const char* name, int k)
{
    return { name, BedPalette(k, BED_NEON), BedPalette(k, BED_MID), BedPalette(k, BED_DEEP),
             BedPalette(k, BED_LINE), BedPalette(k, BED_MESH) };
}
constexpr SurveyBedPalette SURVEY_BED_PALETTE[SURVEY_BEDS] =
{
    SurveyBedFromPalette("SURFACE", 0),
    SurveyBedFromPalette("SHALLOW", 1),
    SurveyBedFromPalette("MID",     2),
    SurveyBedFromPalette("DEEP",    3),
};

// The light the block is shaded by, in model space.
constexpr float SURVEY_LIGHT[3] = { -0.55f, 0.65f, -0.5f };

// ---- the block's model box ------------------------------------------------
// Arbitrary model units; the camera's zoom maps them to the panel.
constexpr float SURVEY_MODEL_W = 740.0f;
constexpr float SURVEY_MODEL_D = 700.0f;

// ---- the cage -------------------------------------------------------------
constexpr int   SURVEY_CAGE_STEP   = 4;      // lattice nodes between column lines
constexpr float SURVEY_CAGE_RING_M = SURVEY_COLUMN_M * 0.125f; // depth rings
constexpr float SURVEY_CAGE_ALPHA  = 0.30f;
constexpr float SURVEY_CRAWL_HZ    = 6.0f;   // stepped, not run: data arriving, not judder

/* ---- the console's chrome palette ---------------------------------------
   From the dashboard design (survey-dashboard-design.md section 3). This is
   deliberately NOT the extraction UI's near-black and amber: the console is a
   readout of measured ground. Dark Plating still owns the machinery drawn on
   top of it -- the drill -- and section 10b of that guide is where the
   boundary runs. */
constexpr Color SC_BG        = {  2,  11,  19, 255 };   // page ground
constexpr Color SC_PANEL     = {  3,  18,  29, 255 };   // panel fill
constexpr Color SC_LINE      = { 26,  74,  92, 255 };   // the dim rounded outline
constexpr Color SC_ACCENT    = { 53, 216, 238, 255 };   // brackets, connectors, active rules
constexpr Color SC_ACCENT_DIM= { 28, 127, 149, 255 };   // connector stems
constexpr Color SC_TITLE     = { 98, 179, 245, 255 };
constexpr Color SC_UNDERLINE = { 33, 227, 240, 255 };
constexpr Color SC_BODY      = {111, 143, 176, 255 };   // prose
constexpr Color SC_LABEL     = {163, 184, 204, 255 };
constexpr Color SC_BRIGHT    = {188, 210, 230, 255 };
constexpr Color SC_DIM       = { 95, 122, 150, 255 };
constexpr Color SC_METER_ON  = { 36, 220, 242, 255 };
constexpr Color SC_METER_OFF = { 21,  55,  71, 255 };
constexpr Color SC_BAR_EDGE  = { 28, 100, 122, 255 };
constexpr Color SC_HEX       = { 31,  90, 110, 255 };   // the block's vignette
constexpr Color SC_BOX_FILL  = {  5,  24,  37, 255 };   // a box nested inside a panel

// The health ramp: confidence, wear, heat -- anything that is better high and
// worse low reads on these four and nothing else.
constexpr Color SC_GOOD  = { 63, 227, 110, 255 };
constexpr Color SC_FAIR  = {233, 227,  75, 255 };
constexpr Color SC_WARN  = {255, 164,  65, 255 };
constexpr Color SC_BAD   = {255,  90,  90, 255 };
