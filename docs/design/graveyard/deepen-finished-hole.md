# Deepening a finished hole from the drill bar

**Lived:** `src/ui/survey_dash.c`:
- the drill bar's ruler pick in COMPLETE, which re-planned the finished
  hole deeper ("Hole re-planned to N m");
- `SurveyDash_Cancel`'s PLANNED branch for "a deeper plan for a hole
  already drilled" ("Deeper plan cancelled. Hole stays at N m.");
- the log card updated in place when a hole was deepened.

**Removed:** the change that resets the console after a hole
**Replaced by:** `DashEndHole`. Once the reveal has run and the cavity has
closed, the console goes back to AIM with the drill bar dimmed and empty.

## What it was

After a hole landed, the drill bar stayed lit on that hole, and a tap on
its ruler below the bit re-planned the same hole deeper. The string ran
back down (`DrillSim` still does this) and carried on cutting. The core
log grew instead of a new one being added.

## Why it went

The player's words: "after a drill is done and the cutaway is closed up,
the drill bar should go back to its first state, no holes, dimmed". A bar
still showing the last hole reads as a hole still in progress. And a reset
arriving under a re-plan in flight would have wiped it, so the bar stops
taking taps the moment a hole is complete.

## What survived

`DrillSim`'s run-back-in (a lifted string returns to the bottom before it
cuts, c2dtest §20). It is unreachable from the console now, but it is the
mechanism a deepen would use. Re-planning a hole's depth *before* it lands,
in PLANNED or DRILLING, is untouched.

## What would bring it back

A design that wants holes deepened in stages, for example to stop at a
contact, read the core, then decide. It should come back as an explicit act
on the hole's barrel or log card, not as a ruler that stays live after the
hole is done.
