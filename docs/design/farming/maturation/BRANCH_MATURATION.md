# Branch Maturation — the session navigation method

**Status:** DRAFT v0.1 (first instance; promote to `docs/design/` when a
second module adopts it)

A working convention for design-heavy git branches, introduced on
`claude/farming-unit-design-4rz536`. Named **branch maturation**: one git
branch runs one design *session* whose job is to mature a cloud of raw
ideas into a navigable **concept tree**, which later *harvests* into the
repo's standard design docs (`docs/design/<module>/…`, per
`docs/design/README.md`).

"Branch" was chosen over "session" for the practice's name (one branch ≈
one running session), which makes the other kind of branch ambiguous — so
inside these docs, git branches are always called *git branches*, and the
tree's branches are called *nodes*.

## The artifacts

Each maturation lives in `docs/design/<module>/maturation/`:

| File | Role |
|---|---|
| `SOURCES.md` | Verbatim capture of raw inputs (notes, ideation text, image descriptions, code state). Never redesigned, only cited. |
| `CONCEPT_TREE.md` | The canonical tree: coded nodes, states, cross-links, open questions. The session's single reference point. |
| `concept_tree.html` / `.png` | The same tree as a visual — for opening beside the conversation. Regenerate when the tree changes shape. |
| `BRANCH_MATURATION.md` | This method doc. |

## Node grammar

- **Codes**: majors are letters (`A`–`G`), children numbered (`B2`),
  grandchildren lettered (`B1.a`). Codes are stable — prune by marking,
  not by renumbering, so old conversation references stay valid.
- **States**: every node carries one:
  - `○ seed` — named but unexplored
  - `● maturing` — actively being worked
  - `◆ design-ready` — settled enough to harvest into a design doc
  - `⏸ parked` — deliberately stored for later (gets a seed doc if it
    must outlive the branch, e.g. `docs/design/biomining/`)

## Navigation verbs (how to steer the conversation)

Point at nodes instead of re-describing ideas:

| Verb | Meaning | Example |
|---|---|---|
| **focus** | make a node the working topic | "focus B2" |
| **graft** | add a child under a node | "graft 'thermal mass' under B1" |
| **prune** | drop a node (state → struck, code retired) | "prune D4" |
| **park** | freeze a node for later; seed doc if needed | "park F5" |
| **harvest** | promote ◆ nodes into a real design doc | "harvest A into the charter doc" |
| **mix** | work two nodes' intersection | "mix C4 × D3" |

## Lifecycle of a maturation git branch

1. **Seed** — dump raw inputs into `SOURCES.md`, grow the first tree
   (aim: 5–8 majors, 2–3 layers; *fairly but not too* rich).
2. **Mature** — sessions navigate by code; the tree accretes states,
   decisions, and open questions. Commits keep the tree honest.
3. **Harvest** — ◆ branches become `docs/design/<module>/README.md` +
   master design per the standard method; the maturation dir stays as
   the record of *why*.
4. **Merge** — the git branch merges; parked seeds live on in their own
   directories.

## Rules of thumb

- The tree is a **map, not a spec** — sentences stay short; depth goes
  into harvested design docs.
- Every major node should end in **open questions**; a branch with no
  questions is either done (harvest it) or dead (prune it).
- Cross-links between nodes are first-class (they're where the good
  ideas hide).
- Keep the visual in sync at shape-changes only, not every wording edit.
