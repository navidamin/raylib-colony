# Graphics — Coded-Art Design Documents

> **Auto-context rule:** Read [dark-plating.md](dark-plating.md) before drawing
> **any** graphic in code — a hero visual, a canvas prototype, an animated rig,
> an icon, a particle effect — whether in a prototype HTML page or in
> `RenderManager`.

This directory is not a module. It is the **look of everything drawn by code**,
extracted from the work that produced the drill rig
(`../subsurface/prototypes/drill-rig.html`, `redline.html`) so the next
machine, building or instrument is drawn in the same world instead of a new
one.

## The Rule

**Every new graphical component is a change to this document too.**

When you add a graphic (or restyle one):

1. **Before drawing** — read the world sections of
   [dark-plating.md](dark-plating.md) (§1–§3), the material section for what
   the thing is made of (§4 metal, §5 rock), and the component family section
   if one exists (§6 drills).
2. **While drawing** — reuse the named helpers and recipes. Change tints,
   never structure (see §4.4). Render previews and look at them —
   `tools/preview/preview.sh` for in-game panels, a Playwright screenshot for
   prototypes. Never claim a visual result without rendering it.
3. **In the same commit** — if the component needed a technique this document
   does not have, add it: a subsection under the right material, or a new
   component-family section written like §6. If it needed nothing new, it
   should look like it belongs; if it doesn't, one of you is wrong.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [dark-plating.md](dark-plating.md) | **The style itself.** The dark world, the chunky line, stepped tone; metal and rock as materials; the drill family; stage, motion and capture craft | LIVING |

## Cross-References

| Where | What |
|-------|------|
| `../subsurface/prototypes/drill-rig.html` | the reference implementation — the rig renderer |
| `../subsurface/prototypes/redline.html` | the same renderer inside a playable stage with console chrome |
| `../prospecting/prototypes/drill-dock.html` | the reference for **linked views** (section 9): borehole panel docked against the 4-layer block model, three placements |
| `../excavation/prototypes/diamond-drill.html` | the reference for the **coring crown** (section 6.5): a rotary bit as an elliptical annulus, and a two-variant sheet drawn without a camera |
| `docs/guides/ui-panels.md` | the **in-game** panel/widget system (raylib) — tokens and widgets for module panels. Dark Plating governs drawn *art*; ui-panels governs panel *chrome*. They share a world; when they disagree about a panel, ui-panels wins |
| `docs/dev-workflow.md` | the render-and-look loop and the preview instruments |
