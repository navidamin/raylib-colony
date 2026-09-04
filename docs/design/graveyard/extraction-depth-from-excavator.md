# Depth layer derived from the first excavator

**Lived:** `src/Unit/unit.cpp:1265-1273` (inside `ProcessExtraction`)
**Removed:** the commit carrying this record — *Phase 0: bury the excavator model the live system already replaced*
**Replaced by:** `ExcavationSystem::selectedDepth`

## What it was

Nine lines at the top of the extraction tick that read `excavators[0].depth` and
bucketed it into a `DepthLayer`:

```cpp
if (depth >= 100.0f) activeLayer = DepthLayer::DEEP;
else if (depth >= 30.0f) activeLayer = DepthLayer::MID;
else if (depth >= 10.0f) activeLayer = DepthLayer::SHALLOW;
```

The result fed `resourceManager.GetResourcesAtGridLayer(gridX, gridY,
activeLayer)`.

## Why it went

**It always returned `SURFACE`.** The only writer of `Excavator::depth` was
`SetExcavatorDepth`, which had no callers, so the field was `0.0f` for every
excavator for the whole life of the game. Every comparison failed and the
`DepthLayer::SURFACE` initialiser stood.

Two further faults made it not worth repairing in place:

1. **The whole fleet's depth came from `excavators[0]`.** Even had the setter
   been wired, machines 2–8 were dug at machine 1's depth.
2. **The thresholds were centimetres** against a world that had moved to metres
   — 100 cm to reach `DEEP`, in a column whose `DEEP` layer starts at 68 m.

Its one consumer, `availableResources`, was itself only read by the fallback
skim buried in [`extraction-fallback-skim.md`](extraction-fallback-skim.md), so
removing this left nothing dangling.

## What survived

**Depth as a per-dig choice**, which is the whole idea. It is now
`ExcavationSystem::selectedDepth`, set by the player on the panel, gated by
`EXC_MAX_DEPTH_PER_TIER`, and read by `DigEngine` per dig rather than inferred
from a machine's stored position.

## What would bring it back

Nothing. Depth is a decision, not a property of a machine — and if it ever
becomes per-machine again it will be read per machine, not from element zero.
