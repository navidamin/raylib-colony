#pragma once

#include "raylib.h"
#include <cmath>

/* =====================================================================
   THE BLOCK'S CAMERA
   ---------------------------------------------------------------------
   Software 3D: yaw and pitch about the block's centre, orthographic, no
   GPU depth buffer -- the beds are painted in order and that IS the
   depth test.

   THE IDENTITY THAT GOVERNS EVERYTHING DRAWN ON TOP OF IT:

       h       = p.x*f0 + p.z*f1
       screenX = (p.x*r0 + p.z*r1) * zoom + cx          <- no p.y term
       screenY = (-(p.y - centreY)*cp - h*sp) * zoom + cy

   Two points differing only in world height share `h`, so they share
   screenX exactly, and their screenY differs by -dy*cos(pitch)*zoom.
   A VERTICAL WORLD SEGMENT PROJECTS TO A VERTICAL SCREEN SEGMENT at
   every yaw and every pitch. That is what lets the drill rig stay a 2D
   pixel drawing standing upright at a projected point, on a camera that
   turns -- it foreshortens, it never leans.
   ===================================================================== */

struct SurveyCamera
{
    float f0 = 0.0f, f1 = 0.0f;   // the view's forward direction on the ground plane
    float r0 = 0.0f, r1 = 0.0f;   // and its right
    float cp = 1.0f, sp = 0.0f;   // cos/sin of pitch
    float zoom = 1.0f;
    float cx = 0.0f, cy = 0.0f;   // where the block's centre lands on screen
    float centreY = 0.0f;         // the model y the camera is levelled on

    static SurveyCamera Make(float yaw, float pitch, float zoom,
                             float cx, float cy, float centreY)
    {
        SurveyCamera c;
        // yaw 0 puts the block's front edge toward the viewer, as the
        // reference render has it.
        const float a = PI * 0.75f + yaw;
        c.f0 = std::cos(a); c.f1 = std::sin(a);
        c.r0 = c.f1;        c.r1 = -c.f0;
        c.cp = std::cos(pitch); c.sp = std::sin(pitch);
        c.zoom = zoom; c.cx = cx; c.cy = cy; c.centreY = centreY;
        return c;
    }

    Vector2 Project(float x, float y, float z) const
    {
        const float yy = y - centreY;
        const float h = x * f0 + z * f1;
        return { (x * r0 + z * r1) * zoom + cx,
                 (-yy * cp - h * sp) * zoom + cy };
    }

    // A wall is drawn when its outward normal points toward the camera.
    bool Facing(float nx, float nz) const
    {
        return nx * f0 + nz * f1 < 0.0f;
    }

    // Pixels of screen travel per metre of depth. A consequence of the
    // camera, never a lever: everything that stands on the ground reads it.
    float PixelsPerMetre(float modelDepth, float columnM) const
    {
        return modelDepth * cp * zoom / (columnM > 1.0f ? columnM : 1.0f);
    }
};
