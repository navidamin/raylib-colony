# The Machine–Ground Contract — How Drilling and Digging Actually Feel

> Status: **DESIGN** — not built; visual prototype in progress
> Last Updated: 2026-08-27
> Parent: [README.md](README.md)
> Serves BOTH modules: [prospecting/block-model-design.md](../prospecting/block-model-design.md)
> and [excavation/block-mining-design.md](../excavation/block-mining-design.md)
> Pairs with: [prospecting/progression-design.md](../prospecting/progression-design.md)

---

## 1. Why This Document Exists

The per-metre economy shipped honest but abstract: depth costs 359 E because a
table says so. This document makes those costs **diegetic** — depth is
expensive *because* trips get longer, heat builds, bits wear and cuttings pile
up. The number on the chip becomes something felt in the hands.

It lives in `subsurface/` because it is one physical contract read by both
modules: prospecting's drill and excavation's machines are the same kind of
object — **a machine pressed against ground** — and every phenomenon below
applies to both, landing differently.

Two rules govern everything here:

1. **Bad technique costs knowledge or product, never just parts.** In a
   knowledge game the sting must land on the knowledge side: a gap in the
   column, a cooked assay, a diluted load. Broken equipment is the *lesser*
   penalty.
2. **The machine is itself an instrument.** Real drilling logs penetration
   rate, torque and vibration and infers rock from them
   (measurement-while-drilling). Everything the machine does while working is
   data, even when the core is lost.

---

## 2. The Shared Physics

Five phenomena, all documented practice, several specifically lunar:

| Phenomenon | Real basis | State it adds | Ignored, it costs |
|---|---|---|---|
| **Contact pressure** | correct weight-on-bit and RPM depend on the rock currently being cut; the sweet zone *moves with the strata* | a live band the operator tracks | core ground to powder / face over-broken (dilution) |
| **Heat + peck cycle** | no drilling fluid on the Moon, regolith insulates; Apollo 15's drill jammed over exactly this; real lunar concepts drill in short bites with cooling dwells | heat gauge; dwell to cool | dulled bit — and **sublimated ice** (§3, §4) |
| **Wear** | bits dull with metres cut, faster in hard rock, much faster when driven hot | per-bit wear; excavation's existing `wear` stat inherits this | slower cutting, ground core, rising dilution |
| **Cuttings clearance** | advance faster than flights clear cuttings and the string seizes; lunar concepts use auger flights or gas, never mud | clearance gauge | jam → work it free, worst case lose rods below the jam |
| **Telemetry** | penetration rate maps strata for free — hard is slow, a jump means a fracture zone or void | live depth ticker with varying rate | nothing — it is pure free information |

The choreography is **generated from the real column**: the layer boundaries
and the shoots crossing the chosen spot. Every hole plays differently, and
reading the block model beforehand is preparation for the hands part — if the
model says fractured bedrock at 34 m, you know to ease off at 34 m. Survey
feeds execution.

---

## 3. The Prospecting Frame — Failures Cost Knowledge

- **Core recovery.** In-band pressure = intact core. Out of band — especially
  in fractured ground — the barrel comes up part empty, and a lost interval is
  a **gap in the column**: no assay exactly where you wanted one. Recovery %
  is a real logged figure; the crystal `glowLevel` channel (currently pinned
  at 4 and meaningless) becomes recovery quality.
- **The ice irony, part one.** Water sits in the fractured layer — the most
  valuable layer is intrinsically the hardest to core intact. The game does
  not need to invent this tension; the geology supplies it.
- **The ice irony, part two: cooked evidence.** Overheat the bit in an
  ice-bearing interval and the water **sublimates out of the core**. The assay
  comes back *falsely low* — careless technique does not just break equipment,
  it **corrupts the data**, and you may write off a good deposit because of
  your own impatience. The most on-theme failure this game can have.
- **RC vs core is *when you learn*.** RC blows chips up the pipe and is logged
  live at the collar — a noisy grade ticker while drilling ("something rich at
  38 m — stop or push on?"). Diamond core is silence all the way down, then
  certainty. One rig is a conversation, the other a reveal. The stable's
  area-vs-certainty decision, made felt.
- **Tripping.** Changing a dull bit or clearing a jam at depth means pulling
  the whole string, rod by rod, and running it back — time proportional to
  current depth. The recurring push-your-luck: *"bit at 60% at 40 m — trip now
  cheap, or push to target and risk tripping from 90 m?"* This is what gives
  the **wireline add-on** its true meaning: core retrieved *without* tripping
  the string, which is literally what wireline drilling was invented for.
- Hole deviation is real but belongs to angled holes — parked with the
  `DirectionalDrilling` technique.

## 4. The Excavation Frame — Failures Cost Product

Same physics, different landing:

- **Pressure out of band** over-breaks the face: the machine takes waste rock
  with the ore — this is where **dilution** physically comes from, feeding the
  composition Stage 1 already hands to beneficiation. The precision stat stops
  being abstract.
- **Heat while mining ice** sublimates the *product* itself: yield loss rather
  than data loss. Peck-digging — many small bites with dwells — is already the
  lunar reality documented in
  [excavation-mechanics.md](../excavation/excavation-mechanics.md) Part 1
  ("a digger cannot push"); this gives it a control surface.
- **Jam** = machine downtime; **wear** already exists on `Excavator` and
  inherits the hot-and-hard multipliers.
- **A digging machine is an instrument too**: its cutting telemetry feeds the
  same confidence writeback as the empty-out
  ([module-interplay.md](module-interplay.md) §3) — the machine feels the
  ground it eats.

---

## 5. Where the Learning Lives

Per the frame in [progression-design.md](../prospecting/progression-design.md):

- **In the hands (this document):** band control, heat discipline, trip
  timing. No XP anywhere — the learning curve is in the player, so nothing
  can be farmed. After twenty holes you know what megaregolith *feels* like.
- **In the colony's state (outcome-counted only):** terrain familiarity and
  crew hours may grow from *information gained* and *outcomes achieved* —
  never from action counts. This **amends** progression-design.md §7: the
  mastery-by-use rejection stands for action-counted XP; outcome-counted
  learning passes the just-wait and removal tests it failed.
- **AUTO drills through all of it, forever, at a flat mediocre outcome**
  (~85% recovery, no live telemetry read) — the same principle as AUTO
  plateauing at Indicated. None of this is mandatory; all of it is ceiling.

---

## 6. The Drill View (visual prototype)

One side-view cross-section, shared by both modules' hands-on mode: the real
strata of the chosen column, the string descending with its flights turning,
heat as colour on the metal, damage as cracks in it, cuttings rising, the
band widget and gauges alongside. Prototyped standalone first (per the dev
rule: never claim a visual result without rendering it); the prototype is the
reference for the in-game renderer.

---

## 7. Sequencing

1. **Telemetry** — nearly free, pure information, no skill UI needed
2. **Heat + peck** — the lunar signature; brings sublimation and the cooked
   assay
3. **Band control + recovery** — the skill layer; revives `glowLevel`
4. **Bits + tripping** — lands with the wireline rig
5. **Excavation port** — same gauges on machines; dilution and yield-loss
   wiring

Prerequisite already on record: prospecting's clock is advanced by the
renderer (`GetFrameTime()` in the draw call). Anything with dwells, trips and
downtime needs the real clock first.

## 8. Open

- `[?]` Does hands-on drilling pause the colony (a focused minigame) or run in
  game time (interruptible)? Leaning game time, for coherence with pause/scale.
- `[?]` How long is one hole, in seconds, at each depth? The minigame must
  respect a full column being a real commitment without becoming a chore.
- `[?]` Do jams ever cost the hole entirely at the prototype stage, or is
  rod-loss reserved for a later hardness pass?
- `[?]` Does AUTO's flat recovery improve with the apprenticeship mechanism,
  or stay flat forever? Leaning: apprenticeship may lift it, capped well below
  a good hand.
