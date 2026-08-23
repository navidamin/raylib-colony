#pragma once

#include "raylib.h"

// The pointer position, in game coordinates, that the rest of the code should
// use instead of calling raylib's GetMousePosition() directly.
//
// On every platform but the web this IS raylib's answer. On the web it is not,
// because raylib converts a browser pointer event by dividing by the canvas's
// measured CSS width:
//
//     position.x = (targetX/cssWidth)*screen.width
//
// and that measurement is taken from getBoundingClientRect inside the event
// handler, where it is not reliable -- emscripten strips the canvas's inline
// size mid-flight, the element momentarily collapses to its natural width, and
// the division cancels out. The game then receives raw CSS pixels: an error
// that grows with distance from the origin and reverses as the displayed
// canvas crosses the game's own width.
//
// The web shell computes the same conversion from geometry it chose itself
// rather than measured, and publishes the result. This prefers that value and
// falls back to raylib when it is absent, so an older cached shell is never
// worse than before.
//
// See docs/web-deploy-mobile.md for the full account.
Vector2 ColonyGetMousePosition();

// Convenience for the common `CheckCollisionPointRec(GetMousePosition(), r)`.
bool ColonyMouseOver(Rectangle area);
