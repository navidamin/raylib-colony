#ifndef GAME_STRUCTS_H
#define GAME_STRUCTS_H

#include "raylib.h"

// A place on the Moon. Every colony, sect and marker carries one; nothing
// carries a world-unit position any more. Drawing happens in a local frame
// around a view's centre (TerrainGen/lunar_frame.h), so "where is this"
// and "where does it go on screen" are different questions with different
// types.
struct LunarPoint {
    double latDeg = 0.0;
    double lonDeg = 0.0;
};

// Structure to represent a background tile
struct BackgroundTile {
    Texture2D texture;
    Vector2 position;
};

#endif // GAME_STRUCTS_H
