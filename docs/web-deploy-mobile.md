# Web Deploy & Mobile Canvas — Reference

How the playable web builds work, and the hard-won fixes that make them
work on phones. Written after debugging the prospecting playtest on
iPhone (Aug 2026); read this before touching `src/minshell.html` or the
Pages deploy.

## Pipeline

- Targets `colony_game`, `colony_playtest` and `colony_extraction` all
  build for `PLATFORM=Web` (emscripten). `src/CMakeLists.txt` shares
  `web_link_flags` between them: `--shell-file src/minshell.html`,
  `--preload-file src/assets@src/assets`.
- `.github/workflows/deploy-web.yml` builds all three and publishes to
  GitHub Pages: game at `/`, prospecting sandbox at `/playtest/`, whole
  extraction unit at `/extraction/`. Each Pages deploy replaces the
  whole site.

### Two active branches, one site: the overlay truce

One repo has one Pages site and every deploy replaces **all** of it. With
two branches deploying (excavation and lunar-elevation), each push 404'd
the other branch's sandboxes within minutes -- "none of the pages load" is
what this looks like from a phone.

The mitigation, in `deploy-web.yml` ("Overlay other branches' sandboxes"):
after building its own site, the job downloads the latest **successful**
`github-pages` artifact from the other active branch (`gh run download`,
needs `actions: read` in the workflow permissions), untars it, and copies in
any **top-level directory** the current deploy does not itself provide.
Root files are never taken -- whoever deploys owns `/`. A deploy from a
branch carrying this step therefore serves the union; a branch without it
still clobbers the others until it copies the same step. Failures are
swallowed deliberately: a missing or expired artifact means the deploy
ships alone, never that it fails.

Two rules learned from real regressions:

- **Ownership is by directory, and a branch must not build the other's.**
  The excavation branch owns `/playtest/` and `/extraction/`; the lunar
  branch owns `/lunarmap/` and `/viewtest/`. The overlay only fills
  directories *missing* from the local build -- a stray copy block on the
  lunar branch that rebuilt its own `/playtest/` (from its stale checkout
  of the prospecting code) silently served the old module for a day, with
  every run green.
- **Artifacts must outlive quiet spells**: `retention-days: 30` on the
  Pages artifact upload. The action's default 1 day meant a branch that
  paused pushing vanished from the union as soon as its artifact expired.

This is a truce, not a fix. The fix is merging the branches so everything
ships from main.

### Adding another web sandbox

Four steps, all small:

1. `add_executable(<name>)` in `src/CMakeLists.txt` with
   `${COLONY_CORE_SOURCES}`, then inside `if ("${PLATFORM}" STREQUAL "Web")`
   set `SUFFIX ".html"` and `LINK_FLAGS "${web_link_flags}"`.
2. Drive the frame with `emscripten_set_main_loop_arg` under
   `#ifdef __EMSCRIPTEN__` — a `while` loop hangs the browser.
3. Copy the four artefacts (`.html` → `index.html`, `.js`, `.wasm`, and
   `.data` if present) into `deploy/<path>/` in the workflow.
4. Make sure the source path matches the workflow's `paths` filter, or a
   push touching only your file will not trigger a deploy.
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

## The fix (SHELL v4, in `src/minshell.html`)

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

## The pointer-coordinate bug (Firefox, Aug 2026)

A fourth way the three sizes bite, found after the fit was already correct.
Symptom: clicks and hovers land away from the real cursor, the error grows
with x and y, and it flips sign as the displayed canvas crosses 1280 wide.

raylib converts a pointer event like this
(`rcore_web_emscripten.c`, `EmscriptenMouseMoveCallback` -- note this file,
not the older `rcore_web.c`):

```c
float mouseCssX = (float)mouseEvent->targetX;      // CSS px, canvas-relative
emscripten_get_element_css_size(platform.canvasId, &cssWidth, &cssHeight);
CORE.Input.Mouse.currentPosition.x = (mouseCssX/(float)cssWidth)*CORE.Window.screen.width;
```

`emscripten_get_element_css_size` is `getBoundingClientRect().width`
(`library_html5.js`), read **synchronously inside the handler**. Emscripten's
own `updateCanvasDimensions` calls `canvas.style.removeProperty('width')`,
which strips even an `!important` **inline** style -- and with no inline width
the stylesheet's `width: auto` applies, so the rect collapses to the canvas's
natural 1280. The division cancels and the game receives raw CSS pixels.

Two things made this hard to see:

- The `MutationObserver` repairs the style immediately after, so every
  after-the-fact reading -- badge included -- shows the healthy value. The
  badge's own listener is bubble-phase and runs *after* raylib's, which is on
  the canvas in the capture phase.
- Chromium never reproduces it: its microtask checkpoint runs the observer
  *between* event listeners, healing the style before raylib can measure.

**The fix**: put the fitted size in a real **stylesheet rule**, not only an
inline style. `removeProperty` cannot touch a stylesheet, so when the inline
size is stripped the rule underneath still holds the canvas at the fitted
size. This removes the race rather than trying to win it. The shell also
re-asserts `fitCanvas` in the capture phase of pointer events as a second
line of defence.

`tools/shell-test` covers it: it strips the inline style and measures the rect
in one synchronous block, the way raylib does, and asserts the fitted width
survives. That check fails on the pre-fix shell and passes on the current one.

**The fix that actually holds**: stop depending on raylib's measurement. The
shell publishes `window.__colonyMouse` in the capture phase of every pointer
event, converting with the fitted size *it just chose* rather than a measured
rect, and the game reads that through `ColonyGetMousePosition()`
(`src/web_mouse.h`). Use that helper, never raylib's `GetMousePosition()`
directly -- it falls back to raylib when the global is absent, so an older
cached shell is never worse than before. `InputManager::GetMousePosition()`
already delegates to it, so anything going through the input manager is
covered.

The stylesheet rule stays as defence in depth, but it is not what the game
relies on.

**Diagnosing it again**: `?debug=1` plus the sandbox's F9 crosshair. If
`game sees` matches `rel` rather than `expect`, the CSS size collapsed. Check
the badge's version first -- it reads `SHELL v5`; anything older is a cached
page, not a live bug.

## The diagnostic badge

`#shellDebug` overlays live geometry:
`SHELL v4 cnv=WxH style=Wpx/priority rect=WxH@x,y win=WxH vv=WxH@y dpr=N`.

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
