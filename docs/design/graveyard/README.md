# The Graveyard — what was removed, and why

Code is cheap to delete and expensive to re-learn. When a part of this game is
retired, the **code goes** and the **reasoning stays** — here, one record per
part.

This directory is not an archive of source. Nothing here compiles, and nothing
here should be copied back. `git` already keeps the bytes; what git does not
keep is *why the thing was wrong*, and that is the only reason a future session
would otherwise resurrect it.

## The Rule

**Removing a part is a two-file change: the deletion, and its record here, in
the same commit.**

A record is owed by anything that was once reachable in the game, or that was
built deliberately and then abandoned. A typo fix, a rename, or dead code that
never worked owes nothing.

## What a record must answer

Five questions, in this order. If a record cannot answer the third and fourth,
the part is probably not ready to be removed yet.

| # | Question | Why it matters |
|---|----------|----------------|
| 1 | **What was it?** | Name, file, line range, and one sentence on what it did |
| 2 | **When did it die?** | The commit that removed it, so the bytes are findable |
| 3 | **Why did it go?** | The specific defect or the design that replaced it. "Unused" is only an answer if you also say what made it unused |
| 4 | **What survived?** | Which idea moved into the replacement, and where it lives now. Most retired parts are half-right |
| 5 | **What would bring it back?** | The condition under which resurrecting it would be correct — or `Nothing`, said plainly |

Write them as prose, not as a form. The failures are the value.

## The record template

```markdown
# <Part name>

**Lived:** `src/Path/file.cpp:120-180`
**Removed:** `<sha>` — <commit subject>
**Replaced by:** `src/Path/new_thing.cpp` (or `Nothing`)

## What it was
...

## Why it went
...

## What survived
...

## What would bring it back
...
```

## Index

Newest first. Empty until the first burial.

| Part | Removed in | Replaced by |
|------|-----------|-------------|
| [Excavator controls](excavator-controls.md) — `MoveExcavator`, `SetExcavatorDepth`, `SetExcavatorRate`, `GetExcavators` | Excavation rebuild phase 0 | `ExcavationSystem` public state |
| [Depth from the first excavator](extraction-depth-from-excavator.md) | Excavation rebuild phase 0 | `ExcavationSystem::selectedDepth` |
| [The flat per-cell fallback skim](extraction-fallback-skim.md) | Excavation rebuild phase 0 | nothing — `Dig` is unconditional |
| [`DescribeSelected`](describe-selected.md) | Excavation rebuild phase 0 | `EstimateSelected` |
| [`DEFAULT_WearAndTear`](wear-and-tear-parameter.md) | Excavation rebuild phase 0 | `DigResult::wearDelta`, `EXC_MACHINES[].wear` |

## Cross-references

| Where | What |
|-------|------|
| [`../excavation/README.md`](../excavation/README.md) | the module whose rebuild opened this directory |
| [`../../guides/feature-completeness.md`](../../guides/feature-completeness.md) | the six questions that decide whether a feature is reachable at all — a part that fails them is a graveyard candidate |
