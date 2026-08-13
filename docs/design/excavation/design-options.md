# Excavation — Design Options

> Status: DRAFT — four options for comparison, no decision made
> Last Updated: 2026-08-13
> Parent: [README.md](README.md)
>
> Replaces the earlier Design A/B/C alternatives in
> [excavation-mechanics.md](excavation-mechanics.md) Part 3, which were rejected as unclear
> and jargon-heavy. Part 1 of that document (the science review) still stands as reference.

---

## Part 1: The Aspects

Before choosing a design, here is the list of things *any* excavation design has to answer.
Each of the four options in Part 2 answers all of them — differently.

| # | Aspect | The question it answers | Why it matters |
|---|--------|------------------------|----------------|
| 1 | **Core idea** | What is this module about, in one sentence? | If it can't be said in one sentence, the player will never feel it |
| 2 | **The click** | What does the player physically do? | Every module needs a verb. Watching a bar fill is not a verb |
| 3 | **The decision** | What is the player choosing between? | No choice means no gameplay — just a timer with extra steps |
| 4 | **The tension** | Why isn't there one obviously right answer? | Without tension, the player solves it once and never opens the panel again |
| 5 | **What changes** | What is different at hour 10 vs hour 1? | Keeps the decision alive instead of stale |
| 6 | **Failure** | What does doing it badly look like? | Players only learn from mistakes they can see |
| 7 | **The number** | What single readout tells the player how they're doing? | One number, or they read none |
| 8 | **Machines** | What do machines actually mean? | Machines already exist in code and art — they need a job |
| 9 | **Depth** | What does digging deeper mean? | Four depth layers already exist in the data |
| 10 | **Tier 0 → 3** | What does each upgrade unlock? | The module tier system already exists and needs filling |
| 11 | **Upstream** | How does prospecting matter here? | Prospecting is the biggest finished system in the game |
| 12 | **Downstream** | What does beneficiation receive? | The separation chain already exists and needs meaningful input |
| 13 | **Idle** | What happens if the player never opens the panel? | Most players, most of the time, won't |
| 14 | **Screen** | What does the UI have to show? | Biggest cost driver |
| 15 | **Effort** | How much work is it to build? | |
| 16 | **Weakness** | Where does it fall down? | Every design has one — better to name it now |

---

## Part 2: Four Designs

Four different answers. They differ in *kind*, not degree — each one is built on a
different thing being interesting: **the ground**, **the equipment**, **the flow**, or
**the unknown**.

---

### Design 1 — THE PIT
*You dig a hole, and the hole is the game.*

| Aspect | |
|--------|--|
| **Core idea** | You carve a physical pit into the ground. The shape you dig determines what you can reach next. |
| **The click** | Click a tile next to your existing pit to dig it out. |
| **The decision** | Which direction to expand, and when to go down a level instead of sideways. |
| **The tension** | You can only dig tiles touching the pit you've already opened. The rich spot is three tiles away through poor rock — do you spend the effort tunnelling to it, or take the mediocre material nearby? And you can't reach the level below a tile until you've cleared the tile above it. |
| **What changes** | The pit grows. Easy nearby tiles run out. Every new tile is further from home, so hauling takes longer. By hour 10 you're working a wide, deep pit that took hours to open. |
| **Failure** | You tunnelled a long thin arm to one rich tile, and now everything around it is worthless — you've stranded yourself and have to walk all the way back. Or you dug wide and shallow and never got down to the good material. |
| **The number** | **Walk distance** — how far your diggers travel to the working face. Creeps up as the pit grows, drops when you open a new area near home. |
| **Machines** | Machines set how fast you dig, how many tiles you can work at once, and how deep you can reach. Bigger machine, more tiles per pass. |
| **Depth** | Levels stack physically. A level-2 tile only becomes diggable once the level-1 tile above it is cleared. Deeper is richer. |
| **Tier 0 → 3** | T0: 1 tile at a time, surface only · T1: 2 tiles, reach level 2 · T2: 4 tiles, reach level 3 · T3: 8 tiles, all levels, auto-expand |
| **Upstream** | Prospecting colours the tiles so you know which direction is worth tunnelling toward. Unsurveyed tiles show grey — you dig into the dark. |
| **Downstream** | Sends whatever happened to be in the tiles you dug. Varies tile to tile. |
| **Idle** | Auto-expands into the best adjacent tile. Works fine, just wanders inefficiently. |
| **Screen** | Top-down tile grid you dig on, with a level selector. Very visual — the pit is the picture. |
| **Effort** | **Medium.** The tile grid already half-exists from prospecting. |
| **Weakness** | Can turn into repetitive tile-clicking. Lives or dies on whether the pit *looks* good as it grows. |

---

### Design 2 — THE MACHINE SHED
*You run a fleet of machines that eat themselves, and keeping them alive is the game.*

| Aspect | |
|--------|--|
| **Core idea** | Digging is automatic. Your job is deciding which machine works, which rests, and which goes in for repair. |
| **The click** | Assign a machine to a job. Pull a machine into the workshop. |
| **The decision** | Which machine runs now, and when to take your best one offline to fix it. |
| **The tension** | Your fastest machine wears out fastest. Repairs cost parts and take it out of service for real time. Push everything flat out for one big week and you have nothing running the next. |
| **What changes** | Machines age. Early on you have one machine and no choice at all. Later you have a mixed fleet of different types at different conditions, and juggling them *is* the game. |
| **Failure** | Everything breaks at once because you ran the whole fleet hard. Output drops to zero and stays there until repairs finish. |
| **The number** | **Fleet condition** — average machine health, 0–100%. Falls while working, rises while repairing. |
| **Machines** | Machines *are* the module, and each has a personality: the **scoop** is slow and nearly unbreakable; the **wheel** digs twice as fast and eats itself doing it; the **drum** is the reliable middle; the **hammer** is the only thing that gets into hard rock, and it's brutal on itself. |
| **Depth** | Deeper ground is harder, so it wears machines faster. Depth is a wear multiplier, not its own system. |
| **Tier 0 → 3** | T0: 1 machine, manual repair · T1: 2 machines, pick your types · T2: 4 machines, workshop repairs run in background · T3: 8 machines, automatic rotation |
| **Upstream** | Weak. Prospecting says where to send them, and that's about all. |
| **Downstream** | Steady flow when the fleet is healthy, spiky and unreliable when it isn't. |
| **Idle** | Auto-rotates machines and repairs anything under 50%. Safe, conservative, never impressive. |
| **Screen** | A roster — one card per machine with a health bar, current job, and a repair button. No map needed. |
| **Effort** | **Low–Medium.** The `wear` field already exists in the code and does almost nothing. |
| **Weakness** | It's maintenance, not digging. Becomes a chore unless the machines have genuine character. |

---

### Design 3 — THE DIG ORDER
*You tell the operation what you want, and tune it to deliver.*

| Aspect | |
|--------|--|
| **Core idea** | Set a target material and balance the operation to produce it. No map, no tiles — you're tuning a working machine. |
| **The click** | Pick a target material. Drag three sliders. |
| **The decision** | How to balance **speed**, **cleanliness**, and **power**. |
| **The tension** | The three fight each other. Dig fast → more junk rock comes up mixed in → beneficiation works harder and burns more power. Dig carefully → clean material, but slow. Cut power → everything slows down. You can have two, never all three. |
| **What changes** | The colony's needs shift. Early you want iron fast and don't care how dirty it is. Later, manufacturing wants clean feedstock and you retune everything. Ground quality also drops as the site depletes, forcing another retune. |
| **Failure** | You set speed to maximum and beneficiation chokes — storage fills with junk, power drains, and the useful output actually goes *down*. |
| **The number** | **Useful output per unit of power.** One efficiency figure that rises when you tune well. |
| **Machines** | Each machine type moves where the sliders can go. The hammer reaches higher speed but its minimum power draw is higher too. Picking a machine picks which corner of the triangle you can even reach. |
| **Depth** | Deeper ground is richer but costs more power. Depth becomes a fourth dial rather than a place. |
| **Tier 0 → 3** | T0: fixed settings, no sliders · T1: speed slider unlocked · T2: all three sliders + live gauge · T3: saveable presets and auto-tune |
| **Upstream** | Prospecting tells you what's actually down there, so you know what's worth targeting. |
| **Downstream** | **Very strong.** The sliders directly set what beneficiation receives and how hard it has to work. |
| **Idle** | Runs a balanced preset. Fine. Never good. |
| **Screen** | A control panel — target selector, three sliders, one big gauge. Compact, no map. |
| **Effort** | **Low.** No new grid, no spatial data, no new art. |
| **Weakness** | Abstract. You're moving sliders, not digging. Almost no sense of place. |

---

### Design 4 — THE GAMBLE
*You can't see underground. Every dig is a bet, and surveying buys better odds.*

| Aspect | |
|--------|--|
| **Core idea** | The ground is hidden. You commit to digging a spot based on what you *think* is there, and find out when it comes up. |
| **The click** | Pick a spot and commit to digging it. |
| **The decision** | Dig now on a hunch, or spend time surveying first. |
| **The tension** | Time spent surveying is time not digging. But digging the wrong place wastes far more than the survey would have cost. Early on you're poor and can't afford to survey — so you gamble, and sometimes you lose. |
| **What changes** | Your map fills in. Early game is mostly guessing. Late game the area near home is well known and the gambling moves out to the frontier, where the stakes are bigger. |
| **Failure** | You committed three days of digging to a spot that turned out to be poor rock. The material still comes up — it's just mostly worthless. And the game shows you the survey you skipped that would have told you. |
| **The number** | **Expected vs actual.** *"You expected 40 iron. You got 12."* That gap is the entire module. |
| **Machines** | Machines change how big a bet you're making. A big machine digs a wide area fast — great when you're right, painful when you're wrong. A small precise one digs a narrow spot: lower ceiling, lower risk. |
| **Depth** | Deeper ground is less surveyed and more variable. The deep bets are the big ones. |
| **Tier 0 → 3** | T0: dig blind, one spot at a time · T1: see survey results before committing · T2: see a confidence rating per spot · T3: auto-picks the safest good bet |
| **Upstream** | **The strongest possible link.** Prospecting stops being a bonus multiplier and becomes the thing that stops excavation being a coin flip. |
| **Downstream** | Highly variable — beneficiation sometimes gets rich material, sometimes rubbish. |
| **Idle** | Plays safe: only digs well-surveyed spots. Slow, but never a disaster. |
| **Screen** | A map of known and unknown spots with a commit button, plus a result card after each dig showing what you expected against what you got. |
| **Effort** | **Medium.** |
| **Weakness** | Randomness feels unfair if the reveal isn't telegraphed well. The *"I should have surveyed"* moment has to land every single time, or it just reads as bad luck. |

---

## Part 3: Comparison at a Glance

| | 1 — The Pit | 2 — The Machine Shed | 3 — The Dig Order | 4 — The Gamble |
|---|---|---|---|---|
| **Built on** | The ground | The equipment | The flow | The unknown |
| **Feels like** | Carving territory | Running a garage | Tuning an engine | Placing bets |
| **Player verb** | Dig outward | Assign & repair | Tune | Commit |
| **Main tension** | Reach vs distance | Speed vs breakdown | Speed vs clean vs power | Knowing vs digging |
| **Visual?** | **Very** | Moderate | Barely | Moderate |
| **Uses prospecting** | Some | **Little** | Some | **Heavily** |
| **Feeds beneficiation** | Varied input | Uneven timing | **Directly controlled** | Wildly varied |
| **Uses existing code** | Grid from prospecting | **`wear` field** | Multiplier chain | Survey progress |
| **Effort** | Medium | Low–Med | **Low** | Medium |
| **Ages well?** | Good | Moderate | Moderate | **Good** |
| **Main risk** | Repetitive clicking | Feels like chores | Feels abstract | Feels unfair |

### Which to pick depends on what you want excavation to *be*

- Want the player to **see** their colony's history in the landscape → **1**
- Want to make the machines and the existing wear system matter → **2**
- Want the cheapest build that plugs straight into beneficiation → **3**
- Want prospecting to finally pay off → **4**

They are not all mutually exclusive. **1 + 4** combine naturally (a pit you dig into the
unknown). **2 + 3** combine naturally (a fleet you tune). **1 + 2** would be a very large
module. Pairing 3 with 1 or 4 mostly fights, because 3 deliberately has no sense of place.
