#ifndef TERRAIN_DETAIL_NOISE_H
#define TERRAIN_DETAIL_NOISE_H

#include <cmath>
#include <cstdint>

// The lattice the sub-floor detail is built on, in one place.
//
// Three synthesizers invent ground below the resolution of the data they
// amplify: lola_dem.cpp under the LOLA elevation model, terrain_synthesis.cpp
// under the WAC mosaic, and terrain_gpu.cpp's shaders under both. They only
// agree with each other if they quantise a world point into the SAME lattice
// cell and hash it to the SAME number -- otherwise the CPU and the GPU draw
// different rocks in the same place, and the elevation model and the imagery
// disagree about where the ground is.
//
// These functions were duplicated in lola_dem.cpp and reproduced a third time
// in the JavaScript bench. Hoisting them makes the agreement structural rather
// than a thing to remember: there is now one definition to keep the GLSL in
// step with, and terrain_probe measures whether it is.
//
// Everything here is a pure function of integer lattice coordinates, so it is
// resolution-, window- and centre-independent by construction: the same world
// point regenerates the same ground from any view that frames it.

// Integer hash. The multiply-xor-shift chain is the one the shaders
// reproduce in `uint` -- change it here and the GLSL must change with it.
inline uint32_t DetailHash(int32_t x, int32_t y, uint32_t salt)
{
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u ^
                 salt * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x9e3779b1u; h ^= h >> 16;
    return h;
}

// The same hash as a 0..1 float. 24 bits, so it is exact in a float and
// the GLSL ES 1.00 path can reproduce it without a double.
inline float DetailHash01(int32_t x, int32_t y, uint32_t salt)
{
    return (float)(DetailHash(x, y, salt) & 0xFFFFFF) / 16777215.0f;
}

// Single-octave value noise on a lattice of `waveKm`, smoothstepped. -1..1.
// u and v are kilometres on the moon: u the east-west arc at the point's own
// latitude, v the north-south arc from the equator.
inline float DetailNoise(double u, double v, double waveKm, uint32_t salt)
{
    double gu = u / waveKm, gv = v / waveKm;
    int32_t x0 = (int32_t)std::floor(gu), y0 = (int32_t)std::floor(gv);
    float fx = (float)(gu - x0), fy = (float)(gv - y0);
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float n00 = DetailHash01(x0, y0, salt);
    float n10 = DetailHash01(x0 + 1, y0, salt);
    float n01 = DetailHash01(x0, y0 + 1, salt);
    float n11 = DetailHash01(x0 + 1, y0 + 1, salt);
    float top = n00 + (n10 - n00) * fx;
    float bot = n01 + (n11 - n01) * fx;
    return (top + (bot - top) * fy) * 2.0f - 1.0f;
}

#endif // TERRAIN_DETAIL_NOISE_H
