// domeforge_3d.cpp — scaffold. See domeforge_3d.h: the shader port is sized
// and planned, not built, so every call is a no-op and Create says so.
#include "domeforge_3d.h"

#include "raylib.h"

bool DomeForge3D::Create()
{
    TraceLog(LOG_INFO, "DomeForge3D: not built yet (docs/design/sect-view/domeforge-study.md, 3D view)");
    ready = false;
    return false;
}

void DomeForge3D::SetBase(const DomeForgeConfig&, const DomeForgeLayout&) {}
void DomeForge3D::SetCamera(const DomeForge3DCamera&) {}
void DomeForge3D::Render(int, int, int) {}
void DomeForge3D::Unload() { ready = false; }
