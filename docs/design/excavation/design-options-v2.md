# Excavation — Design Options, Round 2

> Status: DRAFT — three variants for comparison, no decision made
> Last Updated: 2026-08-13
> Parent: [README.md](README.md)
>
> Narrows [design-options.md](design-options.md) Option 3 ("The Dig Order"). Option 3 was
> preferred for being user-friendly and for doubling as a foundation for AI automation.
> These three variants each add a bit of **machinery**, a bit of **gamble**, and a bit of
> **place** — differently.

---

## Part 1: The Shared Core

All three variants keep Option 3's control panel unchanged. This part is identical
everywhere; only Part 2 differs.

| | |
|---|---|
| **The screen** | One panel: a target, two sliders, a result readout |
| **Target** | What you want out of the ground: Iron · Silicon · Volatiles · Balanced |
| **Pace slider** | Gentle ↔ Hard. Push harder and more junk rock comes up with the good stuff |
| **Power cap** | Ceiling on how much energy the operation may draw |
| **Purity** | **Not a slider.** It's the consequence you read |
| **Main readout** | **Useful output per power** — one number, rises when you tune well |
| **Second readout** | **Expected vs actual** — what you thought you'd get vs what arrived |

### Why two sliders instead of three

Option 3 originally had speed, cleanliness, and power as three inputs. Making **purity an
output rather than an input** keeps the same tension with one less thing to fiddle with:

- Push **pace** up → purity falls
- Raise the **power cap** → you can buy purity back at high pace, but the colony pays for it

Two dials in, one consequence out. This matters because each variant below adds a third
control, and three total is about the limit before the panel stops being friendly.

### Why this is a good base for AI automation

Everything the player does here is **setting a value**, not performing an action. That means
an AI can do exactly the same job through exactly the same interface — no separate "auto
mode" system, no divergence between what the player can do and what the AI can do. Research
upgrades then improve the AI by giving it better defaults and more slots, not by bolting on
a parallel mechanism.

---

## Part 2: Three Variants

Each adds machinery, gamble, and place — but realizes them differently:

| | Place is… | Gamble is… | Machinery is… |
|---|---|---|---|
| **A — The Field Picker** | a short list of named grounds | a range on the estimate | a picker, AUTO by default |
| **B — The Working Map** | a small map that visibly wears down | fog over unworked ground | tied to what the ground needs |
| **C — The Standing Order** | how far your operation reaches | the price of reaching further | a rule in your order |

---

### Variant A — THE FIELD PICKER
*Same panel, plus you choose which ground it works.*

| Aspect | |
|--------|--|
| **Core idea** | The operation works one named patch of ground at a time. You pick which. |
| **The screen** | Option 3's panel, with a row of place buttons above the sliders |
| **Place** | 4–6 named patches around the sect, generated from the real resource grid when the sect is founded — *North Flats*, *Crater Rim*, *Basalt Ridge*, *Ejecta Field*. Each has a character: what it's rich in, how hard the rock is, how far the haul is. Patches deplete, and their buttons dim as they run down. |
| **Gamble** | Each patch shows an estimate as a **range**: *"Iron 30–70%."* Surveying narrows the range; at low tier it never closes fully. Picking a wide-range patch is the bet — it might land at the bottom. |
| **Machinery** | A machine picker, set to **AUTO** by default. AUTO picks something sensible for the patch. Take manual control and you can beat it by roughly 15% by matching machine to ground — the hammer on hard rock, the wheel on soft flats. Ignoring it entirely costs you nothing catastrophic. |
| **The tension** | Pace vs power, plus: *the ground I know is mediocre, the ground I don't know might be better.* |
| **Failure** | You moved to a wide-range patch and it landed at the bottom — a day of digging for half the output. Or you stayed on a depleted patch because switching felt risky. |
| **What changes** | Patches run out one by one, pushing you onto further, less-known ground. The haul cost of your remaining good options creeps up. |
| **Tier 0 → 3** | T0: one patch, no choice, no machine picker · T1: 3 patches, machinery on AUTO · T2: all patches, manual machinery, ranges shown · T3: saved presets per patch, auto-switching when one depletes |
| **What the AI sets** | The patch, the machine, both sliders — **everything**. The panel is literally the AI's parameter list. |
| **Upstream** | Strong and legible: *"survey the Ridge to find out whether it's worth moving there."* Prospecting narrows ranges. |
| **Downstream** | Purity is directly controlled, so beneficiation gets predictable input. |
| **Effort** | **Low.** No map, no new grid — a generated list of places plus the existing panel. |
| **Weakness** | Places are buttons. The sense of place is thin: it's a menu, not a landscape. |

---

### Variant B — THE WORKING MAP
*Same panel, plus a small map that records what you've done to the ground.*

| Aspect | |
|--------|--|
| **Core idea** | A small map sits beside the sliders. You click a zone to work it; the sliders tune how it's worked; the map visibly wears down as you go. |
| **The screen** | Split panel — left: a 3×3 zone map of the ground around the unit. Right: target, two sliders, readout. |
| **Place** | Nine zones, each with its own content and rock hardness drawn from the resource grid. **Worked zones visibly change** — colour drains, texture roughens, a small pit appears and deepens. After a few hours the map is a picture of your operation's history. Working a zone longer also takes it deeper: richer, but slower. |
| **Gamble** | Unsurveyed zones show **"?"** and a rough guess. You find out what's really there by working it. Prospecting clears the fog properly. So the map always shows three kinds of ground — known, guessed, and unknown — and which is which is visible at a glance. |
| **Machinery** | Some zones are hard rock and only the hammer gets into them; the map marks these with an icon. On **AUTO** the right machine is assigned for you. Manual control wins you the edge cases — running the fast wheel on a soft zone where AUTO played it safe. |
| **The tension** | Pace vs power, plus: *work the known zone next door, or the unknown one that might be better?* Zones deplete, so you're always deciding when to move on. |
| **Failure** | You worked a "?" zone hard for a day and it was poor rock. Or you used up every good near zone and everything left is far and hard. |
| **What changes** | The map is the record. Early it's mostly fog; mid-game a patchwork of worked and unworked; late-game a worn-out core with a fringe of unknowns around it. |
| **Tier 0 → 3** | T0: one zone, map shown but not interactive · T1: 4 zones clickable, machinery on AUTO · T2: all 9 zones + depth within a zone + manual machinery · T3: work multiple zones at once, automatic zone selection |
| **What the AI sets** | The zone, the machine, both sliders — everything, same as A. |
| **Upstream** | **The strongest of the three.** Prospecting literally clears fog off a map you're looking at. |
| **Downstream** | Controlled purity, with more variation as zone quality changes. |
| **Effort** | **Medium.** Needs the zone map, depletion visuals, and fog state. |
| **Weakness** | Biggest build of the three, and the map has to actually look good — otherwise it's coloured squares next to a slider panel. |

---

### Variant C — THE STANDING ORDER
*You don't operate the mine. You write its instructions.*

| Aspect | |
|--------|--|
| **Core idea** | You write a short standing order — a handful of rules — and the operation runs itself against it. Geography is how far it reaches. |
| **The screen** | An order sheet. A few rows, each a plain-language rule with a dropdown or slider: *"Prioritise **[Iron]**." · "Work ground within **[reach ——●——]**." · "Pace **[normal]**." · "Power cap **[——●—]**." · "When hard rock is hit, **[switch to hammer]**."* Beside it, a small map showing the circle your current reach covers. |
| **Place** | **Reach is the geography.** A small reach means the well-known ground right by the sect, quickly used up. Push the radius out and your footprint spreads across the map, taking in new ground — richer and more varied, but further to haul and less surveyed. You watch your territory grow. |
| **Gamble** | Uncertainty scales with reach. Ground near home is well known; ground at the edge of a wide reach is guesswork. **Pushing the radius out is the bet.** As you drag the reach slider the expected-output figure visibly widens into a range. |
| **Machinery** | Machines are **rules**, not selections: *"When hard rock is hit → switch to hammer."* Leave it on AUTO and it's handled. Set it yourself and you can be smarter — *"skip hard rock entirely, it isn't worth the power."* |
| **The tension** | Pace vs power, plus reach: wider reach buys richer ground at the cost of longer hauls, worse knowledge, and more machine switching. |
| **Failure** | You pushed reach wide, the order started working far unknown ground, output dropped while power spiked — and you didn't notice for a day, because you'd set it and walked away. |
| **What changes** | Your footprint grows across the site. Ground inside it gets used up, so you push out again. The map becomes a record of expansion. |
| **Tier 0 → 3** | T0: no order sheet, fixed behaviour · T1: target + pace rules · T2: reach + power cap + machine rules · T3: multiple saved orders you switch between (a day order, an emergency order) |
| **What the AI sets** | **Nothing extra — the order sheet *is* the AI.** Player and AI use the identical interface. Research-gated AI upgrades add rule slots and better default rules rather than replacing the player's controls with a separate system. |
| **Upstream** | Prospecting shrinks the uncertainty at any given reach, letting you push further without gambling as hard. |
| **Downstream** | Steady, controlled feed — the order keeps running whether or not you're watching. |
| **Effort** | **Low–Medium.** No zone map or tile grid — a rule list plus a footprint circle. |
| **Weakness** | Least tactile. You set it and watch. The sense of place is *territorial* rather than *terrain* — you feel extent, not ground. |

---

## Part 3: Comparison

| | A — Field Picker | B — Working Map | C — Standing Order |
|---|---|---|---|
| **Player picks** | A named patch | A zone on a map | A set of rules |
| **Sense of place** | Light (a menu) | **Strong (a map)** | Medium (your territory) |
| **Gamble shows as** | A range on a number | Fog on a map | A widening estimate |
| **Machinery** | Optional picker | Suggested by the ground | A rule in the order |
| **Friendliness** | **Highest** | Medium | High |
| **AI automation fit** | Very good | Very good | **Perfect — UI is the AI** |
| **Prospecting payoff** | Clear | **Most tangible** | Indirect |
| **Screens to build** | 1 (existing + a row) | 2 (panel + map) | 1 (order sheet + circle) |
| **New art needed** | None | **Zone/depletion visuals** | A footprint overlay |
| **Effort** | **Low** | Medium | Low–Medium |
| **Weakness** | Place is thin | Biggest build | Least tactile |

---

## Part 4: Note — These Stack

They aren't really competitors; they're the same design at three levels of ambition, and
they graft onto each other cheaply:

```
A  (named places)  →  B  swaps the place list for a map — mechanics unchanged
A  (named places)  →  C  becomes A's tier-3 automation layer
```

**Suggested route: build A, and use C's order sheet as the T3 AI mode.**

- A is the cheapest and friendliest, and its named patches give real (if light) place
- When AI automation arrives, C's order sheet is exactly what "AI mode" should look like —
  so C isn't a rival design, it's A's endgame
- B's map can be dropped in later as a **visual upgrade** to A's place list without changing
  a single mechanic — the patches simply get drawn on the ground instead of listed as buttons

That path gets something playable early, gives the AI work a natural home, and leaves the
expensive visual work until it's clearly worth doing.

---

## Part 5: Open Questions

- `[?]` How many named patches per sect in A — 4, or 6? Enough for a choice, few enough to read at a glance.
- `[?]` Does purity affect what beneficiation *receives*, or how much *power* beneficiation burns? (Second option is more interesting but touches beneficiation's design.)
- `[?]` Does the power cap draw from the sect's shared pool, or is it a local budget?
- `[?]` Should a poor result be revealed as it happens, or only at the end of a work period? (Affects how the gamble feels — slow dread vs sharp surprise.)
- `[?]` In C, is reach a smooth radius or a small number of steps (Near / Mid / Far)? Steps are friendlier and easier to explain.

---

## Part 6: Correction — Geography Is Vertical, Not Horizontal

**Variant A's named patches and Variant C's reach slider are both invalid.** They assume
horizontal ground that the player chooses among, and neither exists.

### What the code actually says

| Fact | Where |
|------|-------|
| `SECT_CORE_RADIUS = 50.0f`, so a grid cell is 100×100 world units | `game_constants.h:9` |
| A sect sits at exactly **one** grid cell | `Sect::location`, `Unit::GetGridPosition()` floors sect position into one cell |
| An extraction unit works exactly that one cell — `GetResourcesAtGridLayer(gridX, gridY, layer)` | `unit.cpp:1220` |
| A cell holds **one** `LayeredResourceTile` — `std::map<ResourceType,float> layers[4]` | `resource_manager.h:42-45` |
| Resource data exists from world generation; nothing about the cell is discovered at runtime | `GenerateResourceMap()` |
| Reach *does* exist, but at colony scale — `jurisdiction_radius = SECT_CORE_RADIUS * 4` — and it holds sects, not dig sites | `colony.cpp:4, 205, 220` |

Three consequences:

1. **The area is fixed and pre-known.** The sect's cell is its working ground. There is no
   reach to widen and nothing to expand into — that decision was already made when the
   player placed the sect.
2. **Names have no source.** Nothing in the code names sub-areas. Inventing 4–6 names inside
   a single 100 m cell would be fabricated flavor over uniform data. Named places only make
   sense at colony scale, where sects are genuinely distinct locations.
3. **There is no horizontal variation to pick between.** One cell = one resource map per
   depth layer. Every square metre of the cell is identical in the data.

### What actually varies: depth

The **only real spatial variation excavation has today is the four depth layers**, each with
its own resource map. That is genuine, already generated, and free to use.

It is also the better story scientifically — the science review already covers it
([mechanics §8](excavation-mechanics.md#8-depth-dependent-geotechnics)): regolith density
rises from ~1.30 to ~1.92 g/cm³ over the first metre, and cohesion rises with it. Deeper
ground is richer, harder, slower, and more power-hungry. That is a real tradeoff with no
invention required.

**So "place" in this module means depth, not location.** The player isn't choosing *where*
on the surface to dig — they're choosing *how deep*, and the pit goes down rather than out.

### What survives

| | Status |
|---|---|
| Shared core (target + pace + power cap, purity as output) | **Unaffected** |
| Variant A — named patches | **Dead.** Becomes "pick a depth layer" |
| Variant B — top-down zone map | **Reshaped.** Becomes a side-on cross-section through the four layers — cheaper to draw and more distinctive |
| Variant C — reach slider | **Dead.** Reach isn't the player's to set |
| Machinery (optional, AUTO default) | **Unaffected** — and stronger, since depth is exactly what separates the machines |
| Gamble | **Survives with a caveat** — see below |

### The open fork

Uncertainty needs something unknown to be uncertain *about*. With one uniform cell, the only
honest unknown is **how much is left in each layer** and **what the deeper layers hold before
you've surveyed them** — which is thinner than the gamble described in Variants A and B.

Restoring a richer gamble requires horizontal variation inside the cell, which means
generating sub-cell resource data. The prospecting design already proposes a 5×5 sub-cell
grid; if excavation shares that generator, one piece of work serves both modules. That is a
real decision with real cost, and it is recorded as unresolved rather than assumed.
