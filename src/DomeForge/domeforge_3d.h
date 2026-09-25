// domeforge_3d.h — the ray-marched 3D view of the base. SCAFFOLD ONLY.
//
// The prototype's third renderer (prototypes/dome-forge/dome-forge-3d.js) is
// one WebGL 1 fragment shader, 444 lines / 20 KB of GLSL ES 1.0, that
// ray-marches the whole base: up to 140 march steps, a 22-step soft shadow
// and ambient occlusion per pixel, each step evaluating up to 9 dome
// instances and 16 road segments. Portable to raylib almost verbatim; the
// cost is not the port but running a full-screen march every frame, which
// DomeForge's own README answers with 2-3x coarser pixels. Sized and parked:
// see "3D view" in docs/design/sect-view/domeforge-study.md for the numbers
// and the plan.
//
// The API below is the shape the port will have. Until it exists, Create
// returns false and the caller keeps the 2D view.
#ifndef DOMEFORGE_3D_H
#define DOMEFORGE_3D_H

#include "domeforge.h"

struct DomeForge3DCamera
{
    double yawDeg = -25.0, pitchDeg = 55.0;   // the prototype's default view
    double dist = 0.0;                        // 0 = frame the base (3.9 x its bounding radius)
    double fovDeg = 30.0;
};

class DomeForge3D
{
public:
    // False until the port exists (and later, if the shader fails to compile).
    bool Create();
    // Upload the base: nine dome instances, road segments, ring, config.
    void SetBase(const DomeForgeConfig& cfg, const DomeForgeLayout& lay);
    void SetCamera(const DomeForge3DCamera& cam);
    // Render into the current target at w x h; pixelSize > 1 marches at
    // reduced resolution and upscales (the prototype's own escape hatch).
    void Render(int w, int h, int pixelSize);
    void Unload();

private:
    bool ready = false;
};

#endif // DOMEFORGE_3D_H
