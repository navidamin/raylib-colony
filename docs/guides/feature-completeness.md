# Feature Completeness Checklist

Read this when you think a feature is done.

> **Engine-implemented is not player-reachable.**

This guide exists because that gap appeared **five separate times** in one
module, each time as working, committed, tested-looking code that no player
could ever benefit from:

| What shipped | What was missing |
|---|---|
| `SweepEngine::StartCalibration()` | no button — calibration decayed forever with no way to restore it |
| `SampleTray::RemoveSample()` | no button — a full 16/16 tray permanently blocked collecting |
| `LabEngine::ApplyPreset()` + 4 preset definitions | no UI at all — entirely unreachable |
| Energy costs on every action label | never deducted — prospecting was free |
| 400 pre-rendered crystal sprites | never drawn — the tray showed letters `A`/`C`/`N`/`L` |

None of these were bugs in the usual sense. Every one was correct code that
stopped one step short of the player.

---

## The six questions

Run these against any feature before calling it done.

### 1. Reachable — is there an input path?

Can the player actually trigger it? Trace from a click/tap to the function.
If the only caller is a test harness or nothing at all, the feature does not
exist yet.

*Cheap check*: `grep` for the method name outside its own module. No hits in
`rendermanager.cpp` (or wherever input is handled) means unreachable.

### 2. Legible — does it show its state?

- **Disabled** actions must look disabled — gate on legality *and*
  affordability, and grey out. An impossible action should be visible as
  impossible before it is attempted.
- **In-progress** actions need progress feedback (`CALIBRATING 46%` with a
  fill bar).
- **Completed/durable** effects need persistent marks — applied lab tools
  keep a green border and checkmark.
- **Touch has no hover**: add a pressed state and a brief post-action flash,
  or a phone player gets no acknowledgement at all. See
  `docs/guides/ui-panels.md` §4.

### 3. Escapable — can the player get out?

Ask: *can the player reach a state they cannot leave?*

The full-tray case was a true dead end — 16/16 samples, no discard, no way to
ever collect again for the rest of the run. Calibration was a slower one:
quality only ever decreased.

Every consumable, filling, or degrading resource needs a release valve:
discard, reset, recharge, recalibrate.

### 4. Real — are the displayed numbers enforced?

If the UI shows a cost, charge it. If it shows a limit, enforce it. If it
shows a rate, apply it.

Displayed-but-unenforced numbers are worse than no numbers: they teach the
player a mental model the game does not actually implement, and they mask
missing systems during testing.

When charging a cost:
- **gate before commit** — check affordability and disable the control, so
  the player sees the constraint rather than a failure message;
- **charge on success only** — and refund if the action then fails
  (a drill charged but not taken refunds);
- **report shortfalls usefully** — "need 80 E, have 45 E" beats "not enough
  energy";
- **charge compound actions once** — a preset charges its whole pipeline,
  not per step.

### 5. Supplied — if it costs, where does it come from?

A cost without an income is not difficulty, it is a wall. Wiring energy costs
without a supply would have made the playtest unplayable inside a minute —
strictly worse than the free version it replaced.

Check the full loop: what produces this resource, at what rate, and does that
rate make the intended play pattern possible? Sandboxes and test harnesses
lacking the real supplier need a documented stand-in (the playtest trickles
30 E/s capped at 1500).

### 6. Rendered — are the generated assets actually drawn?

Assets can be produced, committed, and never referenced. 400 crystal sprites
sat in `src/assets/sprites/samples/` while the game drew letters instead.

*Cheap check*: `grep -rn "LoadTexture\|sprites/" src/` — if an asset
directory never appears in code, nothing is drawing it.

---

## Before committing a feature

```bash
# every target still builds
cmake --build build --target colony_game colony_playtest colony_preview colony_inspect -j4

# nothing else visually regressed
tools/preview/preview.sh --all

# render the states the feature adds, and LOOK at them
tools/preview/preview.sh --module <name> --tier 3 --energy 12   # e.g. cost gating
```

Then walk the six questions above. In practice the ones that catch real
problems are **#1 (reachable)** and **#4 (real)** — they are invisible in
code review, because the code they concern is correct. What is missing is
the connection to the player.

---

## A note on test harnesses

Harnesses must run the **real initialization path**. The preview and playtest
tools constructed a bare `ResourceManager` and never called
`GenerateResourceMap()` — which `Planet` does in the real game. The result
was an empty planet, samples reading 0% richness, and a convincing but
entirely fake bug that was misdiagnosed twice before the data was dumped.

If a harness sets up game state by hand, audit it against the real caller.
And when a value looks implausible, **dump the data before theorising**:
`./build/src/colony_inspect` (see `tools/inspect/README.md`).
