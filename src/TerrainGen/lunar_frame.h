#ifndef LUNAR_FRAME_H
#define LUNAR_FRAME_H

// From a place on the Moon to a place on the screen, and back.
//
// The world is the Moon and positions are LunarPoints. A view -- the
// colony's 25 km window, a descent rung, the globe -- has a centre and
// draws everything by its distance east and north of that centre. Inside a
// window the drawing scale is the game's old one, 1 unit = 50 m, so every
// radius, road speed and dome size keeps its meaning: only the origin moved,
// from the corner of a fixed 100 km square to the centre of whatever is
// being looked at.
//
// The km mapping is the one the terrain chain and the survey cursor use: a
// square window in km, longitude widened by 1/cos(lat) at the window
// centre, floored so a window near a pole stays finite. Pure geometry.

#include "raylib.h"
#include "game_structs.h"

// pi * 1737.4 / 180. Also MOON_KM_PER_DEG (terrain_synthesis.h) and
// SURVEY_KM_PER_DEG (survey_cursor.cpp); repeated so this header depends
// on neither.
const double LUNAR_KM_PER_DEG = 30.32268;

// The floor on cos(lat) in the longitude widening. The chain uses the same
// value, so ground and markers agree on where east is.
const double LUNAR_COS_LAT_FLOOR = 0.2;

// Drawing scale inside a local frame: 1 unit = 50 m.
const float LOCAL_UNITS_PER_KM = 20.0f;

// km east and north of `centre` to `p`, in the window's planar approximation.
Vector2 LunarOffsetKm(const LunarPoint& centre, const LunarPoint& p);

// The inverse: the point eastKm east and northKm north of `centre`.
LunarPoint LunarOffsetPoint(const LunarPoint& centre, double eastKm, double northKm);

// Great-circle distance, for questions that span more than a window
// ("is this inside that colony's territory").
double LunarDistanceKm(const LunarPoint& a, const LunarPoint& b);

// A point quantised to a lattice step in degrees, for use as a map key.
// 1e-4 deg is about 3 m: two asks about the same spot get the same answer,
// two sects 5 km apart never collide.
struct LunarKey
{
    long long lat = 0;
    long long lon = 0;
    bool operator<(const LunarKey& o) const
    {
        return (lat != o.lat) ? (lat < o.lat) : (lon < o.lon);
    }
    bool operator==(const LunarKey& o) const { return lat == o.lat && lon == o.lon; }
};
LunarKey LunarQuantise(const LunarPoint& p, double stepDeg = 1e-4);

// A view's drawing frame: origin at the centre, +x east, +y SOUTH (screen
// y grows down, as the old grid's did), 1 unit = 50 m.
struct LocalFrame
{
    LunarPoint centre;

    Vector2 ToLocal(const LunarPoint& p) const;
    LunarPoint FromLocal(Vector2 local) const;
};

#endif // LUNAR_FRAME_H
