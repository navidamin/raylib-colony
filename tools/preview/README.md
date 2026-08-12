# UI Preview Tool

Renders a unit module panel straight to a PNG — no game loop, no window, no GPU.

The point is to see UI changes without playing to the state you want to look at.
A full set of 12 panels renders in about 5 seconds.

```bash
tools/preview/preview.sh --module prospecting --tab lab --tier 3
tools/preview/preview.sh --all          # renders the whole set into build/preview/
```

Output lands in `build/preview/` unless you pass `--out`.

## Why it's trustworthy

The tool drives the real `RenderManager::DrawUnitView()` against a real `Unit`.
There is no mock panel and no second implementation, so a preview cannot drift
from what the game draws. If the preview looks wrong, the game looks wrong.

## Options

| Flag | Values | Notes |
|------|--------|-------|
| `--module` | `prospecting`, `excavation`, `beneficiation`, `operations`, `directives`, `overview` | which panel to draw |
| `--tab` | `sweep`, `samples`, `lab` | prospecting only |
| `--state` | `empty`, `swept`, `sampled`, `analyzed` | how far to drive the prospecting pipeline |
| `--tier` | `0`–`3` | module tier (uses the debug upgrade path, so no techs or resources needed) |
| `--size` | e.g. `1920x1080` | output resolution |
| `--out` | path | output PNG |

`--state` is cumulative: `swept` runs GPR sweeps, `sampled` also fills the sample
tray, `analyzed` also pushes every sample through the best available lab preset.

## How it runs headless

raylib needs an OpenGL context even to draw a single frame. The wrapper script
supplies one without a display:

- `xvfb-run` provides a virtual X server
- `LIBGL_ALWAYS_SOFTWARE=1` + `GALLIUM_DRIVER=llvmpipe` give software OpenGL 4.5

So it works over SSH, in CI, and inside containers.

### System packages

On a fresh Debian/Ubuntu machine:

```bash
sudo apt-get install -y --no-install-recommends \
    xvfb libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libx11-dev \
    libgl-dev libglvnd-dev mesa-common-dev
```

These are the same X11/GL headers raylib needs to build at all, plus `xvfb`.

## Adding a scenario

Scenario setup lives in `ApplyProspectingState()` in `preview_main.cpp`. It only
uses public APIs (`ExecuteSweep`, `CollectSample`, `ApplyPreset`), so adding a
state is a few lines. To preview a different module's state, drive it the same
way before the draw call.

## Limitations

- **Static frames.** It captures one frame, so hover states, animations, and
  message fades are not exercised.
- **Not an input test.** Prospecting input is handled IMGUI-style inside
  `DrawProspectingPanel`, and this tool never moves a mouse. It shows how a panel
  *looks* in a given state, not whether clicking works.
- **Software rendering.** Pixel output matches the GPU path for this 2D UI, but
  it is not a GPU conformance check.
