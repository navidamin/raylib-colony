#pragma once

#include <algorithm>
#include <cmath>

// Inverting the block model's iso projection -- WITH the relief the plates are
// actually drawn at.
//
// The plates are exploded iso diamonds whose surface is LIFTED per cell by
// believed grade:
//
//     screenX = originX + (i - j) * tileX
//     screenY = originY + (i + j) * tileY + layer * gap - lift(i, j)
//
// Picking used to invert this with lift fixed at 0, i.e. against each plate's
// BASE plane rather than the surface the player can see. That is off by
// lift/tileY lattice rows -- at 16x16 the tile is ~3.9 px tall while the
// relief reaches tens of px, so a rich (highly lifted) block could pick
// several rows from the one under the cursor.
//
// lift only shifts Y, never X, so a cell's own lift decides whether the point
// lands in it: with
//     a  = (mx - originX) / tileX
//     b  = (my - originY - layer*gap + lift(i,j)) / tileY
// the point is inside cell (i,j) exactly when floor((a+b)/2) == i and
// floor((b-a)/2) == j. That is a consistency test per cell, so scan and take
// the FRONT-MOST match (largest i+j) -- which is also the right occlusion
// answer, since the plate is painted front-to-back.
//
// Iterating from a flat guess instead does not work: for a lifted cell near
// the plate's back edge the flat solve lands outside the lattice entirely and
// there is nothing to refine from. The scan is 256 cells per plate, once per
// frame -- nothing next to what the panel already does.
struct BlockPickGeom
{
    float originX = 0.0f, originY = 0.0f;
    float tileX = 1.0f, tileY = 1.0f, gap = 0.0f;
    int   size = 16;
};

// liftAt(i, j) -> the surface lift of that cell, in pixels.
// Returns false when the point misses this plate.
template <class LiftFn>
inline bool BlockPickCell(const BlockPickGeom& g, int layer, float mx, float my,
                          LiftFn liftAt, int& outI, int& outJ)
{
    const float a = (mx - g.originX) / g.tileX;
    const float base = my - g.originY - layer * g.gap;

    bool found = false;
    int bestSum = -1;

    for (int j = 0; j < g.size; j++)
    {
        for (int i = 0; i < g.size; i++)
        {
            if (i + j <= bestSum) continue;          // already have a nearer face
            float b = (base + liftAt(i, j)) / g.tileY;
            if (static_cast<int>(std::floor((a + b) * 0.5f)) != i) continue;
            if (static_cast<int>(std::floor((b - a) * 0.5f)) != j) continue;
            outI = i;
            outJ = j;
            bestSum = i + j;
            found = true;
        }
    }
    return found;
}
