#ifndef LUNAR_GLOBE_H
#define LUNAR_GLOBE_H

#include "raylib.h"
#include "terrain_synthesis.h"

// The moon as a globe you can turn, zoom and click.
//
// Drawn as ONE screen-space quad through a fragment shader that inverts
// an orthographic projection per pixel. No sphere mesh, which buys three
// things: the silhouette is a perfect circle at every zoom instead of a
// polygon, there is no UV seam or pole pinch where the mosaic wraps, and
// the shader runs the same projection the picker does
// (OrbitalPickToLatLon, terrain_synthesis.h) -- so what you click is
// always the pixel you pointed at.
//
// The imagery is the real LROC WAC global mosaic the terrain synthesizer
// already ships, sampled equirectangularly, so the globe is the same
// moon the ground below it is built from.
//
// Camera state lives in terrain_synthesis.h (GetOrbitalCamera), not
// here, because the projection functions need it and they must not
// depend on GL.

// Build the shader and upload the mosaic. Needs a live GL context; safe
// to call every frame (it returns immediately once ready). Returns false
// if the mosaic or the shader could not be loaded, in which case the
// caller should fall back to the baked disc texture.
bool LunarGlobeReady();

// Draw the globe filling the given viewport, centred. Pure 2D: no depth
// buffer, no camera mode, nothing to restore -- HUD drawn afterwards
// simply lands on top.
void DrawLunarGlobe(int screenWidth, int screenHeight);

// Turn and zoom from the mouse: left-drag spins, the wheel zooms about
// the centre, and the globe drifts on its own when left alone.
// dtSeconds drives the drift. Returns true on frames where the drag
// actually turned the globe.
bool UpdateLunarGlobeInput(int screenWidth, int screenHeight, float dtSeconds);

// Did the press that is happening (or just ended) move far enough to be
// a drag rather than a click? A caller that treats a click on the moon
// as "go here" asks this on mouse-release, so spinning the globe does
// not also select a landing site. Stays true until the next press.
bool LunarGlobeWasDragged();

// The drift rate, degrees of longitude per second (0 stops it).
void SetLunarGlobeSpin(double degreesPerSecond);

// Sunlight. mix 0 draws the mosaic flat, as the baked discs did; 1 is a
// full terminator with the night side in earthshine. The direction is a
// longitude in the moon's own frame, so it can be driven from game time.
void SetLunarGlobeSun(double sunLonDeg, double sunLatDeg, float mix);

void UnloadLunarGlobe();

#endif // LUNAR_GLOBE_H
