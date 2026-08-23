# UI Panel Guide

How to build a module panel that looks and behaves like the rest of the game.

Read this when building or restyling any unit/module panel. The extraction
unit's five panels are the reference implementation
(`src/Engine/rendermanager.cpp`); everything below is drawn from them.

There are ~20 module panels still to build (Farming, Energy, Manufacture,
Research × 5 modules each). Following this guide means a new panel looks
native on day one.

---

## 1. Design tokens

Defined near the top of the extraction UI section in `rendermanager.cpp`.
**Never hard-code a colour in a panel** — use a token, or add one.

| Token | Value | Use |
|---|---|---|
| `EXT_SCREEN_BG` | near-black | full-screen backdrop |
| `EXT_PANEL_BG` | dark navy | floating card fill |
| `EXT_PANEL_BG2` | raised navy | buttons, chips, slots |
| `EXT_PANEL_BORDER` | dim slate-cyan | hairline borders |
| `EXT_FRAME_ACCENT` | cyan, faded | corner brackets |
| `EXT_TEXT` | near-white | primary text |
| `EXT_DIM_TEXT` | slate | labels, secondary text |

### Semantic colour language

Colour carries meaning. Keep it consistent or the UI stops being readable:

| Colour | Token | Means |
|---|---|---|
| **Cyan** | `EXT_ACCENT_CYAN` | selected, actionable, primary action |
| **Green** | `EXT_ACCENT_GREEN` | applied, complete, healthy |
| **Gold** | `EXT_ACCENT_GOLD` | warning, attention, degraded |
| **Red** | `EXT_ACCENT_RED` | destructive, offline, critical |
| **Violet** | `EXT_ACCENT_VIOLET` | section headers, pipelines/presets |

`EXT_HEADER_COLOR` (purple) is the standard section-header colour.

Thresholds should change colour, not just numbers: stored energy is dim
above 300 E, gold below 300, red below 100. A player reads the colour
before they read the digits.

---

## 2. Procedural primitives, not image assets

**Default to drawing widgets with raylib primitives.** Icons, frames,
gauges, stripes, and chevrons in this UI are all draw calls. They are
resolution-independent, tintable at runtime, and need no asset pipeline.

Reserve image assets for content that genuinely benefits from pre-rendered
3D or artwork — the crystal sample sprites are the correct exception (see
`docs/design/ui/sprite-manifest.md`).

### Widget inventory

All in `rendermanager.cpp`, usable by any panel drawn in that file:

| Helper | Purpose |
|---|---|
| `ExtDrawPanelFrame(rect)` | floating card: rounded fill, hairline border, corner brackets |
| `ExtDrawIcon(icon, cx, cy, size, color)` | line-icon set (radar, excavator, nodes, gear, crosshair, flask, sliders, bolt, warning, overview, hamburger) |
| `ExtModuleIcon(moduleType)` | maps a module type string to its icon |
| `ExtDrawSegBar(x, y, w, h, value, color)` | segmented gauge (calibration-style) |
| `ExtDrawHazardStripes(rect, color)` | diagonal hazard stripes, clipped |
| `ExtDrawChevrons(x, y, size, color)` | `>>` affordance for action buttons |
| `ExtDrawWireBox` / `ExtDrawWireframeUnit` | isometric blueprint decoration |
| `DrawStyledBar` / `DrawWearBar` | rounded progress / wear bars |
| `DrawTierIndicator(x, y, tier)` | 4 tier pips, cyan→violet |
| `DrawCrystalSprite(visual, dest)` | tinted sample sprite via the texture cache |

**Adding an icon**: extend the `ExtIcon` enum and its `switch` — roughly ten
lines of primitives. Do this rather than importing an icon asset.

---

## 3. Control semantics

Pick the control that matches the verb. Getting this wrong makes a panel
confusing even when it is pretty.

| Control | Means | Looks like |
|---|---|---|
| **Radio row** | *choose one of these* | indicator dot, soft fill on the selected row, no bright border |
| **Action button** | *do this now* | bright fill, cyan border, chevrons, centred label |
| **Toggle** | *this is on/off* | compact chip, state-coloured border, ON/OFF text |
| **Card** | *select this entity* | bordered block, cyan border when selected |
| **Destructive button** | *this removes something* | red border, dark red fill on hover |

> **Earned the hard way**: the depth-layer list was styled like lit action
> buttons, so the selected layer read as "press me" when `COLLECT` was the
> real action. Selection lists get radio treatment; only the thing that
> *acts* gets action styling.

---

## 4. Touch-first feedback

**The game is played on phones. Hover does not exist there.** A control
whose only feedback is a hover highlight gives a touch player nothing.

Every actionable control needs the feedback triad:

1. **Pressed** — brighter fill while the button is held
   (`IsMouseButtonDown`), so a press registers visually.
2. **Flash** — a short (~0.4s) accent flash after a *successful* action, so
   a quick tap is visibly acknowledged. Store `lastActionKind/Index/Time`
   on the module's facade and compare against `gameTime`.
3. **Persistent state** — if an action's effect is durable, show it
   permanently: applied lab tools keep a green border, checkmark, and
   dimmed fill.

Also: **disabled must look disabled**. Gate on affordability/legality and
grey the control out, so an impossible action is visible before it is
attempted rather than failing on click.

Minimum tap target is ~24px tall at the game's 1280x720 render size; the
canvas scales down on phones, so anything smaller becomes unhittable.

---

## 5. IMGUI discipline

This UI is immediate-mode: **draw and hit-test in the same pass**.

```cpp
Rectangle btn = {x, y, w, h};
bool hover = CheckCollisionPointRec(mouse, btn);
bool enabled = CanDoTheThing() && ProsCanAfford(unit, cost);

DrawRectangleRounded(btn, 0.3f, 4, PickFill(hover, enabled, applied));
DrawRectangleRoundedLinesEx(btn, 0.3f, 4, 1.0f, PickBorder(...));
DrawTextEx(font, label, {btn.x + 8, btn.y + 5}, FS(9.0f), sp, PickText(...));

if (hover && enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
{
    DoTheThing();
}
```

Consequences worth knowing:

- **Panel input lives in the render file**, not `inputmanager.cpp`. This
  surprises people looking for click handling.
- **Draw order is z-order.** Tooltips and overlays must be drawn *after*
  the content they sit on top of.
- **State that must persist between frames** (selected index, active tab,
  last-action timestamps) lives on the module facade, not in the renderer.

---

## 6. Layout

- **Wrap every panel in `ExtDrawPanelFrame`** — the shared card is drawn by
  the center-panel dispatcher, so a new panel inherits it.
- **Measure text before placing it.** `MeasureTextEx` then centre/right-align.
  Hard-coded offsets are what caused the `EXTRACTION UNIT` / `ONLINE`
  overlap and the clipped survey percentage.
- **Wrap `DrawTextEx`/`MeasureTextEx` sizes in `FS()`** — the extraction
  UI's font scale multiplier.
- **When a list overflows its card, go two-column.** Six analysis tools in
  one column overflowed; a 2×3 grid fit comfortably.
- **Verify by rendering, never by arithmetic**:
  ```bash
  tools/preview/preview.sh --module <name> --tier 3
  ```
  Adding a section pushes everything below it — the preview catches that in
  five seconds.

---

## 7. Building a new module panel

1. Add `DrawFooPanel(Unit*, int x, int y, int w, int h)` to `RenderManager`.
2. Dispatch to it from `DrawExtractionModuleCenter` by `moduleType`.
3. Add the module's icon to `ExtIcon` + `ExtModuleIcon`.
4. Lay out with the widget helpers and tokens above.
5. Keep per-panel UI state on the module's facade.
6. Render every state you support (`--tier`, `--state`, `--energy`) and
   look at the PNGs before committing.
7. Check it against `docs/guides/feature-completeness.md`.

> **Note for non-extraction units**: they currently render through the
> legacy `unit->DrawInUnitView()` path (`unit_ui.cpp`), *not* through
> `RenderManager`. When building a real panel for one, route it through
> `RenderManager` like the extraction unit does — that is what gives it the
> shared chrome, theme, and preview-tool support.
