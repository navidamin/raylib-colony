#ifndef ROCK_TEXTURE_H
#define ROCK_TEXTURE_H

#include <vector>

// ---------------------------------------------------------------------------
// Procedural rock, one texture per stratum.
//
// The ground the player reads appears in two projections at once -- the
// borehole strip's bands (a section) and the block model's plates (plan) --
// and Dark Plating section 9.1 says one ground joins the panels. So there is
// exactly ONE image per stratum and both projections wear it: a band in the
// strip and the plate floating beside it are visibly the same rock.
//
// The output is a MODULATION MAP, not a colour: grey centred on exactly 128,
// low contrast, so a surface drawn `PROS_ROCK_COL[L] * 2 * tex/255` keeps the
// mean tone the flat fill had. Texture adds material, never a palette -- the
// stratum colours stay the single source of tone. (The only deliberate hue is
// the ice in the fractured layer, which is information the legend already
// names.)
//
// Deterministic (a fixed seed per layer -- ground must not shimmer between
// frames) and wrap-safe on both axes, so a band can tile down a strip of any
// height without a seam. Size must be a power of two: WebGL/GLES2 only
// repeats POT textures, and a clamped tile shows its edges immediately.
// ---------------------------------------------------------------------------

namespace RockTexture
{

constexpr int SIZE = 128;

// RGBA8, size*size*4 bytes, for layer 0-3 (regolith, megaregolith, fractured,
// basalt). `size` must be a power of two.
void Generate(int layer, int size, unsigned char* dst);

// Convenience wrapper.
std::vector<unsigned char> Generate(int layer, int size = SIZE);

}   // namespace RockTexture

#endif   // ROCK_TEXTURE_H
