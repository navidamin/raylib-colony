#pragma once

#include "raylib.h"

/* =====================================================================
   THE FIVE TOOL SPRITES
   ---------------------------------------------------------------------
   One grid, one unit, two tones. Shared by the rack and by the rig,
   which is the whole point: the machine you pick out of the rack has to
   be the machine that goes into the ground. Two drawings of one tool is
   two tools.

   Each has to say what it DOES from its silhouette alone at 22 px wide,
   so each keeps exactly one idea:

     drill         a bit on a string -- a thing that cuts into the ground
     sweep         a body on wheels -- a thing that drives across it
     seismic       a mass over a plate -- a thing that hits it
     penetrometer  a needle with a load collar -- a thing pushed into it
     gpr           a sled with a radiating face -- a thing towed over it

   All five end in the same three-cell stem, so a lead line meets a shaft
   rather than a corner. Sampled off the reference artwork at half-unit
   resolution: 'L' light steel, 'm' mid steel, '.' nothing.
   ===================================================================== */
enum class SurveyTool { DRILL = 0, SWEEP, SEISMIC, PENETROMETER, GPR, COUNT };

// POINT tools stand in a ring; LINE tools stand on a traverse.
enum class SurveyToolMark { POINT, LINE };

struct SurveyToolInfo
{
    const char* name;
    const char* blurb;
    SurveyToolMark mark;
    bool built;            // does the game actually run this one yet
};

const SurveyToolInfo& SurveyToolOf(SurveyTool tool);

/* The sprite, cell by cell, tip at (X, Y) and growing upward. Its SILHOUETTE
   never moves; only the tones cycle through it, one cell at a time, leftwards
   -- shifting the sprite itself would slide the tool sideways, while shifting
   the shading inside a fixed outline is a helix turning under a stationary
   flute, which is the thing being drawn. */
/* The two tones are arguments because the same sprite is an ICON in the rack
   and a MACHINE on the block: in the rack it is drawn in the console's cyan,
   out on the ground it is drawn in steel. One silhouette, two readings --
   which is the opposite of two drawings. Defaults to the steel. */
void DrawSurveyToolSprite(SurveyTool tool, float X, float Y, int shift, float unit,
                          Color light = {250, 250, 250, 255},
                          Color mid = {133, 149, 172, 255});
int  SurveyToolSpriteRows();
