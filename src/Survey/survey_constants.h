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

// ---- the ground the block is a picture of --------------------------------
// 6 km of ground centred on a sect, 2 km deep. The lattice is the resolution
// the height fields are generated at, NOT the resource grid's: this is a
// picture of the rock, and it is sampled finer than the game's 32x32 cells.
constexpr int   SURVEY_LATTICE      = 28;
constexpr float SURVEY_RELIEF_M     = 93.0f;   // amplitude of the surface undulation
constexpr float SURVEY_FEATURE      = 3.5f;    // noise period, in features across the block
constexpr int   SURVEY_DETAIL       = 3;       // fbm octaves
constexpr float SURVEY_CONFORM      = 0.62f;   // how much each bed inherits from the one above
constexpr float SURVEY_DAMP         = 0.55f;   // how fast relief dies with depth
constexpr float SURVEY_DIP_M        = 67.0f;   // regional dip across the block
constexpr float SURVEY_DIP_AZ_DEG   = 25.0f;
constexpr float SURVEY_DIP_SPREAD   = 0.35f;   // how the dip fans with depth
constexpr float SURVEY_GRAIN        = 0.25f;   // squashes the noise domain: mottle becomes ridges

// Bed thicknesses, metres. Four beds, five interfaces; the last interface is
// the base of the SURVEYED column -- a depth somebody chose, not a bed.
constexpr float SURVEY_T0 = 200.0f;
constexpr float SURVEY_T1 = 400.0f;
constexpr float SURVEY_T2 = 550.0f;
constexpr float SURVEY_T3 = 850.0f;
constexpr int   SURVEY_BEDS = 4;

// A few small craters, hashed so they are the same every load, each nudged
// off the middle -- a crater under the sect is not a crater, it is a mistake.
constexpr int   SURVEY_CRATER_N     = 3;
constexpr float SURVEY_CRATER_SIZE  = 1.0f;
constexpr float SURVEY_CRATER_DEPTH = 90.0f;
constexpr int   SURVEY_CRATER_LAYER = 0;       // which interface they were cut into
constexpr float SURVEY_CRATER_RIM   = 0.34f;
constexpr float SURVEY_CRATER_INFILL= 0.55f;

// ---- what a borehole is worth, and how far -------------------------------
/* A borehole does not light a stripe: it constrains the whole model, most
   where it stands and least far away, and never nothing -- a hole in the far
   corner still says something about the section you are looking at. */
constexpr float SURVEY_K_NEAR    = 0.68f;  // what one hole settles right where it stands
constexpr float SURVEY_K_FAR     = 0.15f;  // and what it still says from anywhere at all
constexpr float SURVEY_K_RADIUS  = 9.0f;   // cells over which the near term falls away
constexpr float SURVEY_K_BELOW   = 0.20f;  // how much it says about ground it never reached
constexpr float SURVEY_K_SKIRT_M = 230.0f; // over what depth it drops to that
constexpr float SURVEY_K_FULL    = 0.95f;  // fog at 5% is confidence 1: the model stops here
constexpr float SURVEY_DELIN_GATE = 0.95f; // MEASURED, and the gate isolate waits on

// ---- the scour a hole leaves ---------------------------------------------
constexpr float SURVEY_SCOUR_R      = 1.5f;   // lattice cells -- about 320 m
constexpr float SURVEY_SCOUR_DEEP_M = 45.0f;  // how far the bowl is driven down
constexpr float SURVEY_SCOUR_RIM_M  = 18.0f;  // how high the spoil rim stands

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

/* One bed's four tones plus its mesh. Taken from Holo3D's LAYERS table, which
   is where the block's colour identity was decided. */
struct SurveyBedPalette
{
    const char* name;
    Color neon;
    Color mid;
    Color deep;
    Color line;
    Color mesh;
};
constexpr SurveyBedPalette SURVEY_BED_PALETTE[SURVEY_BEDS] =
{
    { "SURFACE", { 63,155,212,255}, { 29, 93,144,255}, { 12, 42, 74,255}, {207,239,255,255}, {190,235,255,255} },
    { "SHALLOW", { 53,179,196,255}, { 21,115,131,255}, {  6, 48, 60,255}, {189,239,245,255}, {180,235,240,255} },
    { "MID",     {127,168,174,255}, { 74,109,117,255}, { 34, 56, 62,255}, {214,236,239,255}, {210,230,232,255} },
    { "DEEP",    {122,146,180,255}, { 58, 79,108,255}, { 27, 38, 54,255}, {211,220,236,255}, {200,215,235,255} },
};

// The light the block is shaded by, in model space.
constexpr float SURVEY_LIGHT[3] = { -0.55f, 0.65f, -0.5f };

// ---- the block's model box ------------------------------------------------
// Arbitrary model units; the camera's zoom maps them to the panel.
constexpr float SURVEY_MODEL_W = 740.0f;
constexpr float SURVEY_MODEL_D = 700.0f;

// ---- the cage -------------------------------------------------------------
constexpr int   SURVEY_CAGE_STEP   = 4;      // lattice nodes between column lines
constexpr float SURVEY_CAGE_RING_M = 250.0f; // metres between depth rings
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
