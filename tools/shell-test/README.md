# Web Shell Canvas Test

Regression test for `src/minshell.html` — verifies the canvas stays fitted and
the framebuffer stays pinned at phone and tablet viewports, without needing a
device.

```bash
NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/shell_test.js
NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/shell_test.js --shots
```

Exits non-zero on failure. `--shots` writes PNGs to `build/shell-test/`.

**Run this after any change to `minshell.html`.**

`mouse_units_test.js` beside it covers the other half of the canvas contract,
the input: emscripten 3.1.64's GLFW reports the mouse in CSS pixels, and
`InputManager::FixWebPointerUnits()` scales it back to the 1280x720 frame.
The test extracts that JS from `src/Engine/inputmanager.cpp` and runs it in
Chromium against emscripten's exact function at three canvas fits. Run it
after touching that function or bumping emsdk:

```bash
NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/mouse_units_test.js
```

## What it does

Builds a page from the *real* shell, replacing the `{{{ SCRIPT }}}` placeholder
with a stand-in that behaves like the Emscripten/raylib runtime at its worst:
it repaints continuously, repeatedly mirrors the CSS size back into the canvas
framebuffer attributes, and stomps the inline style with `important` priority.

Then, at each viewport, it asserts:

1. the framebuffer is still **1280x720** (the game's fixed render size), and
2. the rendered canvas **fits inside the viewport**.

A second page stands in for `lunar_map`, which sets `window.COLONY_CANVAS_FREE`
and sizes its own framebuffer to the viewport; there the harness asserts the
opposite — the framebuffer **tracks the viewport** and is never pinned back.

Outside the dev container: `npm install playwright && npx playwright install
chromium` anywhere, then `NODE_PATH=<that>/node_modules node tools/shell-test/shell_test.js`.
`SHELL_TEST_CHROME` overrides the browser executable.

## Why this exists

The mobile build shipped cropped three times. A raylib web canvas has three
independent sizes — CSS display size, framebuffer attributes, and the game's
render size — and each of the first two fixes addressed the wrong one. This
harness reproduces the exact fight the shell has to win, so the next change to
`minshell.html` gets an answer in seconds instead of a deploy-and-screenshot
round trip.

Full background: `docs/web-deploy-mobile.md`.

## Viewports covered

| Name | Size | DPR |
|---|---|---|
| phone-portrait | 402x714 | 3 |
| phone-landscape | 714x402 | 3 |
| tablet | 1024x768 | 2 |

If the game's base resolution changes, update `GAME_W`/`GAME_H` here **and**
in `minshell.html`.
