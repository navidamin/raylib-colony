# Dev Workflow — Testing Instruments & The Loop

The tooling in `tools/` exists so UI and gameplay changes can be seen and
verified without a display, and playtested on a phone. **Read this before
starting UI or prospecting work** — the instruments below turn "I think this
looks right" into evidence.

## The loop

This is the working habit, in order. Each step has a tool.

```
1. change code
2. build + render preview          → tools/preview/preview.sh
3. LOOK at the PNG, iterate        → Read the image; fix; re-render
4. show the images                 → deliver the PNGs, don't just describe them
5. verify nothing else broke       → preview.sh --all, build every target
6. commit + push                   → deploy auto-triggers
7. playtest on device              → /playtest/?v=N
```

Two rules that keep this honest:

- **Never claim a visual result without rendering it.** The preview tool costs
  ~5 seconds; a wrong claim costs a round trip.
- **When a value looks wrong, dump the data before theorising.** See
  `colony_inspect`. Reading generation code failed to find the composition
  bug; printing the numbers found it in one step.

## The instruments

### 1. `tools/preview/` — headless UI screenshots

Renders any module panel to a PNG with no display, using the real
`RenderManager` against a real `Unit`, so previews cannot drift from the game.

```bash
tools/preview/preview.sh --module prospecting --tab lab --tier 3
tools/preview/preview.sh --all            # 12-panel set, ~5s
tools/preview/preview.sh --module sprites # crystal sprite contact sheet
```

Key flags: `--module`, `--tab`, `--state` (`empty`/`swept`/`sampled`/`analyzed`),
`--tier 0-3`, `--energy N` (test cost gating), `--size`, `--out`.

The world uses a **fixed seed** (`PREVIEW_MAP_SEED`), so screenshots are
reproducible and comparable between runs. Full docs: `tools/preview/README.md`.

### 2. `tools/playtest/` — interactive sandbox + phone build

Boots straight into the prospecting module with a live game loop. Builds
natively and for Web.

```bash
cmake --build build --target colony_playtest && ./build/src/colony_playtest
```

Deployed with the game to GitHub Pages at **`/playtest/`**. Full docs:
`tools/playtest/README.md`.

### 3. `tools/inspect/` — terminal dump of generated data

Prints what the generators actually produced: raw `ResourceManager` quantities
per depth layer, then the `ProspectingGrid` sub-cell view (composition
fractions + absolute quantity).

```bash
cmake --build build --target colony_inspect
./build/src/colony_inspect            # parent cell (5,5), tier 3
./build/src/colony_inspect 12 7 2     # specific cell and tier
```

Uses the same fixed seed as the preview tool, so its numbers describe the
world the screenshots show. Reach for this the moment a displayed value looks
implausible.

### 4. `tools/shell-test/` — web shell canvas regression test

Verifies `src/minshell.html` keeps the framebuffer pinned and the canvas
fitted, at phone and tablet viewports, against a stand-in runtime that fights
it the way Emscripten/raylib did.

```bash
NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/shell_test.js
NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/shell_test.js --shots
```

**Run this after any `minshell.html` change.** Background:
`docs/web-deploy-mobile.md`.

## Environment notes

- The dev container **cannot reach the deployed site** (github.io, Azure
  artifact storage, and unauthenticated api.github.com are egress-blocked).
  Verify deploys through the GitHub Actions API, and diagnose device issues
  via the shell's on-page debug badge plus a user screenshot.
- The GitHub App **cannot dispatch or re-run workflows** (403). Retrigger a
  deploy by pushing a commit touching the workflow's `paths` filter.
- Headless rendering needs X11 + software GL; `preview.sh` sets this up
  (`xvfb-run`, `LIBGL_ALWAYS_SOFTWARE=1`). System packages are listed in
  `tools/preview/README.md`.
- Chromium and Playwright are preinstalled at `/opt/pw-browsers/` and
  `/opt/node22/lib/node_modules`. Never run `playwright install`.

## Before committing

```bash
cmake --build build --target colony_game colony_playtest colony_preview -j4
tools/preview/preview.sh --all
```

Both must be clean. If `minshell.html` changed, run the shell test too.
