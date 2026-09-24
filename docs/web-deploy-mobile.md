# Web Deploy & Mobile Canvas — Reference

How the playable web builds work, and the hard-won fixes that make them
work on phones. Written after debugging the prospecting playtest on
iPhone (Aug 2026); read this before touching `src/minshell.html` or the
Pages deploy.

## Pipeline

- Targets `colony_game` and `colony_playtest` both build for
  `PLATFORM=Web` (emscripten). `src/CMakeLists.txt` shares
  `web_link_flags` between them: `--shell-file src/minshell.html`,
  `--preload-file src/assets@src/assets`.
- `.github/workflows/deploy-web.yml` builds the pages and publishes them
  to GitHub Pages. Each Pages deploy replaces the whole site, and two
  branches deploy, so each copies the other's folders from its last good
  deploy. `claude/lunar-elevation-lola-dem-1dcdtj` publishes `/` (an index
  of the playtests, `tools/pages/index.html`), `/ladder/` (the game — the
  address to hand out, since no other branch builds one), `/ladder/walk/`,
  `/lunarmap/` and `/regolith/`; `claude/excavation-module-design-jhp3v1`
  publishes `/playtest/` and `/extraction/` — and also its own month-old
  game at `/` and an old walk at `/viewtest/`, so those two addresses are
  its build after each of its deploys. `/viewtest/` is retired on this
  branch (not published, and skipped when copying the other branch's
  folders). A folder one branch stops publishing otherwise comes back
  from the other's last deploy; retiring one needs that skip. If the
  other branch's site cannot be fetched (three tries on each of its last
  three good deploys), this branch's deploy fails rather than publish a
  site without that branch's pages — the live site stays as it was.
- Every deployed page names its build: the browser-tab title, and the
  game's title screen (`COLONY_BUILD_STAMP`, `src/build_stamp.h`), read
  `<branch> <commit> <time> UTC`. A page without it is not from this
  branch's deploy.
- The `github-pages` **environment** restricts which branches may
  deploy. A branch deploy failing in ~2s with no steps run = branch not
  in the environment's allowlist (Settings → Environments →
  github-pages → Deployment branches).
- Emscripten apps need `emscripten_set_main_loop_arg` instead of a
  `while` loop — see `Engine::Run()` and `tools/playtest/playtest_main.cpp`
  for the `#ifdef __EMSCRIPTEN__` pattern.

## The mobile canvas problem (three layers, not one)

A raylib web canvas has **three independent sizes**, and a mobile crop
can come from any of them:

1. **CSS display size** — how large the element is on screen.
2. **Canvas framebuffer attributes** (`canvas.width/height`) — the
   actual pixel buffer.
3. **The game's render size** — what raylib thinks it is drawing into
   (viewport + layout coordinates, fixed 1280x720 here).

What actually happened on iPhone, in order of discovery:

- Plain responsive CSS (`max-width: 100vw`) was **overridden by inline
  styles** the wasm runtime writes on the canvas → still cropped.
- CSS with `!important` + a one-shot fit script lost a **race** against
  runtime style writes on slow mobile loads → still cropped.
- The real killer, found via the on-page debug badge: resize plumbing
  **mirrored the CSS size into the framebuffer attributes**
  (`cnv=402x226`) while raylib kept rendering 1280x720 → GL viewport
  anchored bottom-left → only the bottom-left corner of the UI visible.

## The fix (SHELL v4/v5, in `src/minshell.html`)

A persistent enforcer, not a one-shot:

- Pins the framebuffer: restores `canvas.width/height` to the game's
  fixed 1280x720 whenever anything shrinks them (the game repaints every
  frame, so the buffer clear is invisible).
- Sets the CSS display size to fit the viewport, aspect-preserved, via
  `style.setProperty(..., 'important')` (beats inline-important writes).
- Re-asserts on: MutationObserver (canvas style/width/height),
  `visualViewport` resize, window resize, orientationchange, plus a
  500ms fallback poll. Guarded so it doesn't loop on its own writes.
- Viewport meta: `width=device-width, initial-scale=1,
  viewport-fit=cover, user-scalable=no` (pinch-zoom off so gestures
  don't fight game taps). `touch-action: none` on the canvas.
- Touch coordinates stay correct: emscripten scales input by
  framebuffer/rect, which the enforcer keeps consistent.

**If the game's base resolution ever changes, update `GAME_W/GAME_H` in
minshell.html.**

**Pages that size their own framebuffer** (SHELL v5): `lunar_map` sets
the canvas to the viewport size and re-asserts it when the viewport
changes, which the enforcer above undid on every poll — it drew a
viewport-sized frame into a 1280x720 buffer, which the browser then
stretched: bottom-left corner only, ~1.25x, cursor off by the same
factor. Such a page sets `window.COLONY_CANVAS_FREE = true` before its
first frame (see `SyncWebCanvasToViewport` in `lunarmap_main.cpp`), and
`fitCanvas` then leaves both the framebuffer and the CSS box to it. The
game and the playtest never set it and are unaffected.

## The mouse arrives in CSS pixels (emscripten 3.1.64 GLFW)

Found on the first desktop-browser playtest of the founding flow (Sep
2026): the globe's crosshair landed past the mouse by 1.25x, uniformly.
Under a plain X display the desktop build was exact, so the fault was the
web input path, and it is emscripten's, not ours:

- `library_glfw.js` in 3.1.64 installs, in `glfwInit`, a
  `Browser.calculateMouseCoords` that scales page coordinates by
  `canvas.clientWidth / rect.width`. Both are CSS measures of the same
  box, so the factor is 1 and GLFW hands raylib the mouse in **CSS
  pixels**. raylib's `MouseMoveCallback` stores that as the screen
  position, in a frame that is 1280x720 framebuffer pixels.
- The enforcer above CSS-fits that frame to the viewport, so on any
  desktop window the CSS box is not 1280x720 and every mouse position is
  off by the fit: 1600 px box → 1.25x past the cursor; 1024 px box →
  0.8x short of it.
- Touch never showed it: raylib's own `EmscriptenTouchCallback` scales
  `targetX` by screen / CSS size itself. Phones were fine, which is why
  it survived every phone playtest.
- `lunar_map` never showed it either: it sizes its framebuffer to the
  viewport (`COLONY_CANVAS_FREE`), so frame and CSS box agree.

The fix is `InputManager::FixWebPointerUnits()`, called once after
`InitWindow` by every web-built main: an `EM_ASM` that puts the
pre-3.1.5x behaviour back, scaling by `canvas.width / rect.width`
(framebuffer over CSS box). `glfwInit` has already run by then, so the
override sticks. Verified in the preinstalled Chromium against 3.1.64's
exact function for the 1600, 1024 and 1280 px boxes
(`tools/shell-test/mouse_units_test.js`: it extracts the JS from
`inputmanager.cpp`, so the test and the game cannot drift).
If emsdk is ever bumped, re-check: `main` scales by
`GLFW.active.width / rect.width` when not HiDPI-aware, which is correct,
and the override then changes nothing.

`F9` in the game draws a ring where the game thinks the pointer is, with
the mouse, screen, render, DPI and CSS-box numbers: a screenshot with the
OS cursor visible reads any remaining factor straight off.

## The diagnostic badge

`#shellDebug` overlays live geometry:
`SHELL v5 cnv=WxH style=Wpx/priority rect=WxH@x,y win=WxH vv=WxH@y dpr=N`.

This exists because the dev container **cannot reach the deployed site
at all** (github.io, unauthenticated api.github.com, and Azure artifact
storage are all egress-blocked) — the page reporting its own state via a
user screenshot was the only reliable diagnosis channel, and it found
the framebuffer bug in one round after three blind attempts. Keep it (or
gate it behind `?debug=1`) — it costs nothing and pays for itself.

Healthy portrait iPhone reading:
`cnv=1280x720 style=402px/important rect=402x226@0,244 win=402x714 dpr=3`.

## Verifying shell changes without a device

Chromium + Playwright are preinstalled
(`/opt/pw-browsers/chromium-*/chrome-linux/chrome`,
`NODE_PATH=/opt/node22/lib/node_modules`). Build a test page from the
real shell (replace `{{{ SCRIPT }}}` with a hostile script that mimics
the runtime: repaint loop + periodically mirroring CSS size into the
attributes and stomping the style with 'important'), then screenshot at
mobile viewports and assert the framebuffer stays pinned and
`getBoundingClientRect` fits. See the session that produced this doc for
the exact harness shape.

## The site level's ground in a browser

The pages are built for **WebGL2** (`-s MAX_WEBGL_VERSION=2`), and get
WebGL1 where a browser has nothing better. WebGL2's GLSL ES 3.00 runs the
regolith, so on a real GPU the site level is built there, at the
screen's width; on WebGL1 (or a software rasterizer) it is built on the
CPU, small, to keep the pause short. raylib's own shaders are ES 1.00 and
run on both; it logs "VAO extension not found" on WebGL2 and uses its
non-VAO path, which is harmless.

Before every deploy, `tools/lunarmap/web_site_level_test.mjs` opens the
built pages in headless Chromium at 1656×960, walks Globe → District →
Site, and fails unless the site level was built with its regolith: for
`lunar_map` and the game, each as the CI browser runs it and with
`?terrain=gpu`. The GPU runs matter because the CI browser renders WebGL
in software (SwiftShader), so it takes the CPU path — while an iPad
takes the GPU path. The game's GPU run also fails unless the page got
WebGL2 and built the site level on the GPU at 1024 px or more (2026-09-23
it came up craterless there; 2026-09-24, 512 px and blurred).
`?terrain=cpu|gpu` works on any page as the browser's `COLONY_TERRAIN`,
for trying either path on a device. Run it locally against a web build
directory:

    node tools/lunarmap/web_site_level_test.mjs build/src colony_game gpu

## Ops gotchas

- The GitHub App integration **cannot** dispatch or re-run workflows
  (403). Retrigger by pushing a commit that touches the workflow's
  `paths` filter.
- `setup-emsdk` occasionally fails with `socket hang up` / 503 —
  transient GitHub infra; just retrigger.
- Pages serves HTML with `Cache-Control: max-age=600` and mobile
  browsers cache harder; bust with a query string (`/playtest/?v=N`)
  and test in real Safari, not in-app webviews.
- The deploy-from-branch trigger in `deploy-web.yml` is temporary for
  playtesting — remove it when the branch merges, or every branch push
  replaces the live site.
