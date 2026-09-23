# The drill that was always turning

**Lived:** `src/ui/drill_sim.c` — `DrillSim_Step`'s cutting path, reached on
every frame from the first; `src/ui/survey_dash.c` — `SurveyDash_Press`, which
took the drill bar's ruler and face whatever the console's state
**Removed:** the start-digging change, straight after stretch-to-depth
**Replaced by:** `DrillSim.running` and `DrillSim_Start`; the console's
phases, `SurveyDash_Phase`, gating the bar

## What it was

The redline prototype's drill has no off switch: the spindle idles at 0.14 of
full speed and the bit rubs its way down at about 0.12 m/s whether anyone is
playing or not. The port kept that. The console opened with DRILL STATS
reading DRILLING, the string creeping toward 120 m before a site existed, and
the drill bar answering taps -- the ruler set a depth and the face kicked the
spindle -- at any time, including before there was a hole to drill.

## Why it went

It made the two acts of drilling one. Choosing where and how deep is a plan;
starting the string is a commitment. With the drill always turning there was
nothing to commit to -- the hole was being drilled while it was still being
chosen, and the depth a player picked on the block was racing a bit that had
already set off. It also had no answer to "when does the record start", and
the dig profile needs one.

The bar answering before a site was the same flaw from the other side: a
depth picked on the ruler with no site was wiped the moment a site was taken,
so the control looked live and did nothing.

## What survived

- **The rhythm.** Once started, the drill is exactly the prototype's: taps
  kick the spindle, heat follows speed times hardness, below the band the bit
  rubs. Only its start and stop are new.
- **Idle rub.** A started drill left alone still creeps at the idle rate; the
  player drives it faster, the prototype's loop unchanged.
- **The ruler as a control** -- once a hole is planned it re-plans the depth,
  deeper from where the bit stopped or shorter before it gets there.

## What would bring it back

Nothing, for the survey drill. A free-running rig the colony operates on its
own -- an automated drill directive, with no player at the bar -- would want a
drill that runs without a start, and should be built as that, not by removing
the gate.
