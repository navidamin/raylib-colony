# Branch notes — `claude/excavation-module-design-jhp3v1`

Working memory for this branch: the hazards, the decisions already made, and
the things still open. **Read this before doing anything else on this branch.**

Not a design document — those live in `docs/design/`. This is the stuff that
only makes sense if you were here, and that costs an hour to rediscover.

**Fold this into the design docs and delete it when the branch lands.**

---

## 1. Environment hazards — read first, they have cost real work

- **The container silently reverts the checkout.** Between turns *and within a
  single turn*. It has come back at an older commit with stale edits in the
  working tree that, if committed, would have reverted already-pushed work.
  - **Start every turn with:**
    `git fetch origin <branch>:refs/remotes/origin/<branch> && git reset --hard origin/<branch>`
  - Uncommitted changes you did not just make are **not yours** — check them
    against the remote before assuming they are work in progress.
- **Remote refs go stale too.** A stale `origin/main` once made a trial merge
  report "Already up to date", contradicting a commit count taken minutes
  earlier. Fetch with an **explicit refspec**, not a bare `git fetch`.
- **A stale test binary will lie to you.** "All tests passed" once came from a
  binary that had not rebuilt, while the build had actually failed on a
  changed function signature. **Check the build's exit status before believing
  any test output.**

## 2. Where the branch stands

- **`main` is already fully merged in.** Merge commit `66099d2`, which took
  main at `a26a9e6` — still main's tip.
  - `git merge-base --is-ancestor origin/main HEAD` → yes; 0 commits in main
    not here. `git merge origin/main` does nothing. Verify before re-merging.
- **108 commits ahead of main**, 120 files, +23,404 / −2,574.
- **Nothing is proposed to main.** PR #12 was opened and then **closed
  unmerged** at the user's request — https://github.com/navidamin/raylib-colony/pull/12
  - The instruction was *pull main in*, not *push to main*, and the PR was
    not wanted. **Do not push `main`, and do not open a PR** unless asked in
    so many words. How this branch lands is the user's call, not a default.
- **From the merge:** 4 shared test cases resolved to our versions, 15 of
  main's dropped as superseded design — each checked for a renamed equivalent
  here first. If a "missing" test from main turns up, that is why.
- **CI is green on all six workflows** at `0a5b8e1` (Tests, Linux, macOS,
  Windows, WebAssembly, Deploy to Pages).

## 3. Traps in the test and preview harnesses

- **Excavation tests need generated ground.** A bare `ResourceManager` has
  empty ground truth, so every dig moves zero mass and the failure looks like
  a broken rate law. Call `rm.GenerateResourceMap(<fixed seed>)` first.
- **`ExcavationSystem` defaults are not neutral:** `aiLevel = BASIC` and
  `autoMachine = true`, so the autopilot overrides the spot, the machine and
  the pace. A test that sets a pace is not testing the pace it set. Set
  `aiLevel = AiLevel::OFF; autoMachine = false;` and pin `activeMachine`.
- **`colony_sim` skips the sect layer** — it drives `unit->Update()` directly.
  It therefore cannot catch anything wrong between Colony, Sect and Unit, and
  did not catch the double-update bug. Its numbers are single-tick numbers.
- **Preview PNGs are not pixel-reproducible in general** (see
  `docs/dev-workflow.md`) — animation eases on frame time. The excavation
  panel diff *did* come out clean apart from the drill head, because that
  panel's only frame-driven motion is the rig. Do not generalise that to
  other panels; judge by looking.

## 4. Engine bugs found and fixed here — and what they imply

- **`fb58883` — the output rate was reconstructed, not measured.**
  - The readout divided dug mass by the *draw* frame's `GetFrameTime()`,
    assuming that was the interval the dig ran over. It is not: the caller
    passes `deltaTime * efficiencyMultiplier`, and the preview harness steps
    a fixed `0.5f`. The same panel rendered twice reported **323 and 178
    C/day** for identical work.
  - `DigResult` now carries `dtSeconds`; the facade divides on the dig tick
    and exposes `massPerSecTarget` / `massPerSecTotal` (per **second** — the
    panel multiplies by `TICKS_PER_DAY`).
  - Eased with `EXC_RATE_SMOOTH_TAU_S`, **on the dig tick, not the draw** —
    same reason bit heat is: a headless preview draws two frames. First
    sample seeds whole so a one-dig screenshot is truthful.
- **`4d608d9` — every active unit was updated twice per frame.**
  - `GameManager::Update` called `sect->Update(dt)` and then looped the same
    `sect->GetUnits()` calling `unit->Update(dt)` again.
  - **This was a balance change:** live extraction output halved. No constants
    needed retuning — `colony_sim` had been measuring single-tick behaviour
    all along — but anything tuned by eye against the *running game* before
    this commit was tuned against doubled output.
  - Third occurrence of the same shape (see
    `module-architecture.md` §4, "the hierarchy owns the recursion"). The
    manager refactor carried this bug across intact: **splitting a monolith
    moves its bugs rather than auditing them.**
- **`0a5b8e1` — `Engine_copy.cpp` buried** (`docs/design/graveyard/engine-monolith.md`).
  Never compiled, 91 errors against current headers, but it matched greps and
  cost real time during the double-update audit.

## 5. Decisions already settled — do not relitigate without cause

- **Two depths, deliberately different.** `PlatePlaneM(L)` = display /
  correspondence (the plate is the top surface of its layer);
  `PlateTargetM(L)` = `LAYER_CENTRE_M[L]`, the drill target. Coring uses
  **`PlateTargetM`** — using the plane depth put the core one cell from the
  click.
- **The z axis is z.** Moving the pointer *within* a plate must not change the
  depth shown on the drill bar; depth changes only *between* plates. This was
  reported as counter-intuitive in playtest and rebuilt accordingly.
- **Plate focus:** surface plate pinned lit, the other three rest dim
  (`PLATE_REST_LIGHT`), hovered or drilled comes to full,
  `PLATE_LIGHT_TAU_S = 0.045f`. State lives on the facade, not the renderer.
  Tau was retuned once because a comment claimed ~3 frames and the maths gave
  ~16 — **check easing arithmetic against the frame budget, not intuition.**
- **Drill lifecycle:** `NONE → AIMING → DRILLING → RETRACTING → DONE`. `DONE`
  means the string is out of the ground, which is what removes the trace line
  from the plates; the borehole and core log persist in the dock.
- **Strata textures are generated, not sprites** (`rock_texture.cpp`), mean
  exactly 128, POT and wrap-safe, same texture on the drill bar and the layer
  planes. The textured plate path measured **8× faster** than the untextured
  `DrawTriangle` fallback (27 ms vs 216 ms on llvmpipe).
- **Alpha is not lightness.** A request for "20% lighter" was first served by
  scaling alpha 0.11 → 0.13, which moved the band by 1.3/255 and was correctly
  rejected as "not there". Solve against **rendered pixels**: 0.20 → 25.00
  luminance was the change that landed.
- **Unsurveyed ground is uniform.** No initial curvature on the plates is
  correct — curvature arrives with the first LIBS sweep. (Raised, then
  self-corrected by the user; do not "fix" it.)
- **MSVC:** no compound literals. Use `CLITERAL(Color){...}` — a plain
  `(Color){...}` turned Windows CI red on every push until found.

## 6. Open / deferred

- **How this branch lands is undecided.** No PR is open; see §2.
- **`GetOrbitalSurveyAt` is never read by `src/Prospecting`** — a real but
  unhooked seam between orbital survey data and the prospecting grid. Found,
  raised, **left alone by the user's decision.** It is not an oversight.
- **`docs/design/graveyard/README.md`** still says "Empty until the first
  burial" above nine entries. Cosmetic; left deliberately rather than mixed
  into an unrelated commit.
- **Non-extraction module panels** fall back to `DrawGenericModulePanel`
  (marked PRELIMINARY). Only Extraction's five have bespoke panels.
