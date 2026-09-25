# Console fix plan — the open list, in order

The survey console's leftovers after the playtest rounds up to the rough-walled
hole (commit d0a75a6). This is the working plan: each step names what is wrong,
the fix, how it gets verified, and whether it is done. Steps are ordered by
risk and dependency: correctness first, then the one refactor the later steps
lean on, then player-facing gaps, then polish, then performance.

Status: **in progress.**

## Step 1 — the six failing excavation checks (`colony_test`)

**Wrong.** `colony_test` fails 6 of 88 checks. Four are about reach and one is
about yield ("reach runs 2x2 to 8x8", "tier 0 reaches the centre", "a spot
inside reach yields material"). The other three are about precision blending.
None of them has been looked at.
**Fix.** Dump the data first (rule 2). Find out whether the test or the engine
moved, fix whichever is wrong, and record the reason.
**Verify.** `colony_test` 88/88, and `colony_tests` still 206/206.

**Done.** The engine was right. The prospecting lattice went 8 → 16 → 32 on
playtest requests, and the reach rings followed it (8/16/24/32). The test
still hard-coded the 8×8 lattice's cell numbers: reach 2..8, a centre at
(3,3)/(4,4), a dig at (4,4), and a blend search over cells 1–6. The checks
now use `PROSPECTING_GRID_SIZE` and `PROSPECTING_REACH_PER_TIER`, and the
blend search covers the whole lattice. CLAUDE.md's "8×8 lattice" is corrected
too.

## Step 2 — refactor: one placement rule for everything that floats

**Wrong.** Three overlays hang off the pointer or a barrel: the cursor tag, the
core card, and (step 3) the cancel control. Each carries its own copy of the
"flip left, keep off the rulers, clamp to the pane" logic. The tag and the
card already disagree on how they clamp. The keep-out areas are a
file-static (`g_rulerBox`) plus a drill-bar span recomputed in the tag.
`survey_dash.c` is 1700 lines, with input, phases, overlays and drawing
mixed together.
**Fix.**
- `DashPlace(anchor, w, h, region, keepOut[], n)` is the only placement
  function. It tries right, then left, then above, then below, and clamps to
  the region.
- The keep-out list is rebuilt once per frame, from what was actually drawn.
- The overlays (tag, card, barrels, grab hand, drill cursor) move to
  `src/ui/dash_overlay.c`, behind a small internal header.
- `survey_dash.c` keeps state, phases and input.
- No behaviour change.
**Verify.** The same captures as the alignment pass show pixel-identical
placement for tag and card. c2dtest, visdiff and every build are green.

**Done.**
- `survey_dash.c` is down from 1708 lines to about 1150.
- `dash_overlay.c` (about 510 lines) holds the height log, the barrels and
  the card, the grab hand, the tag and the drill cursor, plus `DashPlace`
  and the keep-out list.
- `survey_dash_internal.h` holds the layout constants and the handful of
  helpers the two files share. The shared model is `g_dashModel`.
- The same drives captured before and after the move differ only in
  animation (fog crawl, spinning barrels, pulsing frame). The tag, the card
  and the hand are pixel-identical.

## Step 3 — undo on a touch screen

**Wrong.** Right-click undoes a site or a depth. A phone has no right-click,
so on touch a misplaced site can't be taken back except by drilling it.
**Fix.** The drill bar's title row gets one control slot, which already holds
ABORT while drilling. It holds CANCEL in STRETCH and PLANNED and does exactly
what right-click does (`SurveyDash_Cancel`). It shows the hand cursor, and a
tap on it never falls through to the block.
**Verify.** Drive the playtest to STRETCH and PLANNED, tap CANCEL with the
pointer, and capture each phase going back one step. Check it at `--scale 2`.

**Done.**
- The press consumes the tap, so the release does nothing more.
- The STRETCH depth tag now shows only in the middle pane, where a tap
  would actually take the depth.
- The PLANNED tag hides over CANCEL.
- Driven with a real pointer at 1x and at `--scale 2`. STRETCH goes back to
  AIM ("Site cancelled."), and PLANNED goes back to STRETCH ("Depth
  cancelled.").

## Step 4 — the log scrolls

**Wrong.** The scrollbar is painted from the reference: a fixed thumb at
0.30–0.68. Entries past the fourth are drawn under the panel edge and can't
be reached.
**Fix.**
- A scroll offset lives on the log.
- The wheel scrolls it when the pointer is over the log (the wheel is free
  now that the block doesn't zoom), and so do the arrow buttons.
- Dragging the thumb scrolls it too.
- The thumb shows the real visible fraction and position.
- A new entry snaps the view back to the top, where the newest entry is,
  unless the player has scrolled away.
**Verify.** Playtest with `--holes 8`, which gives a long log. Capture the
top, the middle and the bottom, and a drag of the thumb.

**Done.**
- The log keeps 40 entries.
- Painter and input share `Dash_LogBar`, `Dash_LogThumb` and
  `Dash_LogMaxScroll`.
- The wheel (a new `SurveyDash_Wheel`, fed by the render manager) steps a
  row a notch. The arrows step, the track pages, and the thumb drags.
- The thumb is never shorter than 18.
- The pointer is the hand over the scrollbar, and never the drill over the
  log.
- `drive.py` gained a `wheel N` step.
- Captured: the top, wheeled to the bottom, and one row back up by the
  arrow. `--holes 8` pushes no log entries, so the log was filled with
  site-and-cancel rounds instead.

## Step 5 — legibility at 1x

**Wrong.** At 1280×720 the console's 1536-wide design space is drawn at 0.70,
so 12-unit text is 8 px. That covers the rock names in the well, the tag
captions, the ruler names, the stats labels and the card's legend.
**Fix.** No console text below 13 design units, with 14 where there is room.
Each size change is re-checked against its neighbours, since the alignment
rules still hold.
**Verify.** Render every state (idle, holes, aim, stretch, planned, bar hover,
drilling, card) at 1x and 2x and look at each.

**Done.** Raised to 13:
- the drill bar's ruler names and gauge labels
- the rock names in the well
- the TOOL STATS subtitle
- the DRILL STATS status pill and row labels (the bars moved from x + 146
  to x + 156 to clear "Rotary Speed")
- the delineation hint
- the core card's depth marks
- the cursor tag's caption

The rack's own text is the port's, and stays where the visual diff measures
it.

## Step 6 — the empty hole

**Wrong.**
- Cuttings keep hanging in the hole after the pull-out. They rise at a speed
  set by the spindle rpm, so when the spindle stops they stop too.
- Every hole's walls have the same wander at the same depth.
**Fix.**
- Cuttings spawn only while the bit is on bottom and cutting. They rise at a
  floor speed, and at the collar they die.
- The wall noise takes a per-hole seed. The console sets it from the site
  when a hole is sited, so the same site redraws the same hole.
**Verify.** A burst through a hole and its pull-out: the hole is empty within
a second. Two sites side by side show different walls.

**Done.** Two holes were drilled at x40 and burst-captured through their
pull-outs. Both shafts are empty of cuttings as the string comes up, and the
second site's walls wander differently from the first's.

## Step 7 — the last separate rock palette

**Wrong.** `DP_ROCK_COL`, `DP_ROCK_EDGE` and `DP_ROCK_GRAIN` in
`rendermanager.cpp` still colour the older panel's strata and the excavation
plates in the redline browns. That is a fourth table outside `bed_palette.h`.
**Fix.** Derive them from `bed_palette.h`: body from `BED_WELL`, edge from
`BED_WELL_EDGE`, grain from `BED_MID`, with the texture tint law kept. The
excavation view then follows the palette like everything else. Its old colours
get a graveyard entry.
**Verify.** Excavation and the strata sheet (`preview --module strata`),
before and after, shown side by side.

## Step 8 — DisplayScale audit

**Wrong.** Only the playtest, view test, extraction sandbox and game were
checked at 2x/3x. Game code may still call raw `BeginMode2D`,
`GetScreenWidth`, `BeginScissorMode` and the like.
**Fix.** Grep every game-code call site of the forbidden list and move each
one to its `DisplayScale_*` wrapper. Tool-only code that never runs scaled is
left alone and noted.
**Verify.** `sectwalk` and `viewtest` at `--scale 2` show no half-size views
or clipped panels.

## Step 9 — fog cost on the web

**Wrong.** The unknown-rock fog paints per-column quads, wire lines and dashed
rings every frame. It has not been measured, but it is suspected to be what
drags the web build.
**Fix.** Measure first: time the block with and without fog in the headless
preview, and count the quads. Only if it is the cost, cache what doesn't move.
The fog changes only with knowledge, yaw or the reveal, while the 6 Hz crawl
is a phase offset.
**Verify.** The frame-time numbers before and after, and a visual check that
the fog looks the same.

## Step 10 — roadmaps

Update `ROADMAP_IMMINENT.md` (and `ROADMAP_OVERALL.md` if a phase moved) with
what this plan closed, per the session procedure in CLAUDE.md.

## Not in this plan

These are features, not fixes. Each needs a design decision from the player
first:

- a hole's dig profile feeding excavation
- comparing and sorting core cards
- what the surface sweep and the three empty rack slots do
