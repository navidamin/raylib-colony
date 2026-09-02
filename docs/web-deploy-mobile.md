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
- `.github/workflows/deploy-web.yml` builds both and publishes to GitHub
  Pages: game at `/`, playtest at `/playtest/`. Each Pages deploy
  replaces the whole site.
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
