/* bed_palette.h -- THE BEDS' COLOURS, ONE TABLE.
 *
 * Every picture of the ground under a sect reads its colours from here: the
 * console's block (Holo3D_SetGround), the drill bar's well and the core card's
 * strip (dash_chrome.c, survey_dash.c), and the C++ survey block
 * (SURVEY_BED_PALETTE in src/Survey/survey_constants.h). They used to keep a
 * table each, and the block was repainted while the drill bar stayed brown.
 *
 * Change a bed's colour in BedPalette_Hex and all of them follow. Everything
 * else is derived, so a new base colour needs no other edit:
 *
 *   NEON       the bed's colour, as the block's lit faces show it
 *   MID, DEEP  x0.52 and x0.22: the block's face gradient
 *   LINE, MESH toward white by 0.72 and 0.60: bed edges and the wire mesh
 *   WELL       x0.34: the drill bar's rock, dark enough for the steel string
 *              and the bed names on it to read
 *   WELL_EDGE  x0.55: the contact line under each bed in the well
 *
 * Header-only, and usable from C and C++ alike: C gets static inline
 * functions, C++ gets constexpr ones, so the C++ side can keep its palette a
 * compile-time table. */
#ifndef BED_PALETTE_H
#define BED_PALETTE_H

#include "raylib.h"

#ifdef __cplusplus
  #define BED_FN static constexpr inline
#else
  #define BED_FN static inline
#endif

/* bed 0 is the top; the playtest ground uses four, and the fifth is below
   anything it reaches */
#define BED_PALETTE_BEDS 5

typedef enum BedShade
{
    BED_NEON,
    BED_MID,
    BED_DEEP,
    BED_LINE,
    BED_MESH,
    BED_WELL,
    BED_WELL_EDGE
} BedShade;

/* "ice & iron": pale ice over rust, chosen from sixteen candidates drawn as
   4-bed strata (dark-plating.md, "The beds are ice over iron") */
BED_FN unsigned int BedPalette_Hex(int bed)
{
    switch (bed <= 0 ? 0 : (bed >= BED_PALETTE_BEDS ? BED_PALETTE_BEDS - 1 : bed))
    {
        case 0:  return 0xb4dcecu;    /* pale ice    */
        case 1:  return 0x9cc8dcu;    /* steel-blue  */
        case 2:  return 0xb0765au;    /* rust        */
        case 3:  return 0x6e3c34u;    /* oxblood     */
        default: return 0x4e2c28u;    /* dark rust   */
    }
}

BED_FN unsigned char BedPalette_Scale(unsigned int c, float f)
{
    return (unsigned char)((float)c * f + 0.5f > 255.0f ? 255.0f : (float)c * f + 0.5f);
}

BED_FN unsigned char BedPalette_ToWhite(unsigned int c, float t)
{
    return (unsigned char)((float)c + (255.0f - (float)c) * t + 0.5f);
}

BED_FN Color BedPalette(int bed, BedShade shade)
{
    const unsigned int h = BedPalette_Hex(bed);
    const unsigned int r = (h >> 16) & 0xffu, g = (h >> 8) & 0xffu, b = h & 0xffu;
    float f = 1.0f;
    switch (shade)
    {
        case BED_LINE:
            return CLITERAL(Color){BedPalette_ToWhite(r, 0.72f), BedPalette_ToWhite(g, 0.72f),
                                   BedPalette_ToWhite(b, 0.72f), 255};
        case BED_MESH:
            return CLITERAL(Color){BedPalette_ToWhite(r, 0.60f), BedPalette_ToWhite(g, 0.60f),
                                   BedPalette_ToWhite(b, 0.60f), 255};
        case BED_MID:       f = 0.52f; break;
        case BED_DEEP:      f = 0.22f; break;
        case BED_WELL:      f = 0.34f; break;
        case BED_WELL_EDGE: f = 0.55f; break;
        case BED_NEON:      f = 1.00f; break;
    }
    return CLITERAL(Color){BedPalette_Scale(r, f), BedPalette_Scale(g, f),
                           BedPalette_Scale(b, f), 255};
}

#undef BED_FN

#endif
