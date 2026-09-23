# The depth hand-off to the ruler

**Lived:** `src/ui/survey_dash.c` — `DashDrawCursor`'s second state, the
`guideT` / `guideFrom` fields of `SurveyDashState`, and `DashEaseOut`
**Removed:** the stretch-to-depth change, a few days after it shipped
**Replaced by:** the stretch — `DashDrawBorehole` and `DashStretch01` in the
same file

## What it was

After the player sited a hole, the target mark flew from the site to the
middle of the drill bar's depth ruler on an ease-out, then went back to
following the pointer, leaving a plated sign reading SELECT THE DEPTH and a
dim copy of the mark at the ruler. The depth was then chosen by tapping the
ruler.

## Why it went

It sent the player's eye away from the hole to decide how deep the hole goes.
Depth is a place in the ground; the block shows the ground, with its beds, at
the moment the question is asked. The ruler is a remote control for a spatial
choice. The player who asked for the change put it directly: when you click
where you want to dig, a line should stretch from that point to the cursor
with the depth on it.

It also had a flaw that shaped it from the start: a cursor that parks on a
control cannot be used on that control, so the first version had to be
changed to fly once and come back. A hand-off that has to hand the pointer
back is a sign the pointer should not have left.

## What survived

- The **plated label**. Text over rock needs a backing plate; the stretch's
  `DIG TO 52 m` uses the same plate at the pointer.
- The **target mark**, now at the end of the stretched line.
- The **ruler as a control**. Tapping it still sets a depth; it is no longer
  where the console sends you.

## What would bring it back

A device where pointing at a height on the block is harder than tapping a
list — a small phone screen where the column is a few dozen pixels tall. Then
the ruler is the larger target, and a hand-off to it is the kinder design.
Nothing short of that.
