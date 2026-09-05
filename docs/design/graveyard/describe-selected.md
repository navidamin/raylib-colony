# `ExcavationSystem::DescribeSelected`

**Lived:** `src/Excavation/excavation_system.h:34`, `src/Excavation/excavation_system.cpp:55-60`
**Removed:** the commit carrying this record — *Phase 0: bury the excavator model the live system already replaced*
**Replaced by:** `ExcavationSystem::EstimateSelected()`

## What it was

A one-line convenience: hand it the prospecting system, get back the
`SpotView` for the currently selected spot and depth.

## Why it went

Zero callers. The panel wants the **estimate**, not the view — what it draws and
what the readout compares against is `SpotEstimate` (`shown`, `low`, `high`,
`confidence`), and `EstimateSelected()` already returns exactly that.

`SpotView` is ground truth. A convenience accessor that hands truth to the UI
layer is a trap in a module whose entire design rests on the player seeing a
blurred belief instead — this one was never used, but the next person to reach
for a "describe the selected spot" helper would have found truth in it.

## What survived

The accessor pattern, aimed at the right object: `EstimateSelected()` is the
selected-spot convenience the panel actually calls. `SiteView::Describe`
remains for the engines, which are entitled to truth.

## What would bring it back

A debug or inspection overlay that deliberately shows truth beside belief. It
should be named so that nobody mistakes it for the player-facing number —
`DescribeSelectedTruth`, and only compiled into the dev tools.
