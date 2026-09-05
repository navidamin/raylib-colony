# The pre-manager Engine monolith (`Engine_copy.cpp`)

**Lived:** `src/Engine/Engine_copy.cpp:1-895`
**Removed:** the commit carrying this record
**Replaced by:** `InputManager`, `ViewManager`, `GameManager`, `RenderManager`
(`src/Engine/`), coordinated by the 381-line `Engine.cpp`

## What it was

The whole game engine as one class. `Engine` owned the camera, the view enum,
the pointer to the planet, the current colony / sect / unit, the double-click
timer, the zoom limits and the drag flag — and implemented, as its own methods,
everything done with them: `HandleInput`, `HandleCameraControls`,
`HandlePlanetViewCamera`, `HandleColonyViewCamera`, `ClampCameraColonyView`,
`ResetCameraForCurrentView`, `SwitchTo{Planet,Colony,Sect,Unit}View`,
`Select{Colony,Sect,Unit}`, `BuildNewColony`, `BuildNewSect`, `Update`,
`UpdatePlanetActiveArea`, `Draw`, `DrawCellInfo`, `DrawPlusIndicator`.

It was kept beside the live `Engine.cpp` as a hand-made copy — the `_copy`
suffix is the whole of its documentation — and it has been in the tree since
this repository's first commit (`d1b4098`), untouched by any commit since.

## Why it went

**It was never in a build.** No `CMakeLists.txt` in the repository has ever
named it (`git log -S"Engine_copy"` over the build files returns nothing), and
no file references it. It is not conditionally compiled or behind a flag; it is
simply not a translation unit.

**It could not be built even deliberately.** Compiled by hand against the
current headers it produces **91 errors** on the first pass, starting at the
constructor: `class 'Engine' does not have any field named 'currentView'`,
`...'planet'`, `...'currentColony'`. Every method it defines —
`Engine::SwitchToColonyView`, `Engine::SelectColony`,
`Engine::HandleCameraControls` — is a member of a class that no longer declares
it. There is no path from this file to a running game that does not begin with
rewriting it.

**It read as live code and cost real time.** This is the specific harm, and it
is why the file is worth a record rather than a silent `rm`. While auditing why
active units were being updated twice per frame, `Engine_copy.cpp:472-475` came
up in the grep alongside `gamemanager.cpp:56-59` carrying the identical
duplicated loop:

```cpp
sect->Update(deltaTime);
for (auto& unit : sect->GetUnits()) {
     unit->Update(deltaTime);
```

Two apparent occurrences of a bug in two apparent call sites is a different
investigation from one. Establishing that half of it was a fossil took longer
than fixing the half that was real. A 900-line file that grep cannot
distinguish from live code is not free to keep, however dead it is.

## What survived

**All of it, redistributed.** The monolith was not wrong, it was undivided, and
the manager split is a direct partition of its members:

| Monolith owned | Now lives in |
|---|---|
| `camera`, `minZoom`/`maxZoom`, `HandlePlanetViewCamera`, `ClampCameraColonyView`, `ResetCameraForCurrentView`, `SwitchTo*View` | `ViewManager` |
| `lastClickTime`, `lastClickPosition`, `IsDoubleClick`, `isDragging`, `HandleInput` | `InputManager` |
| `planet`, `colonies`, `currentColony`/`currentSect`/`currentUnit`, `Select*`, `BuildNewColony`, `BuildNewSect`, `UpdatePlanetActiveArea` | `GameManager` |
| `Draw`, `DrawCellInfo`, `DrawPlusIndicator` | `RenderManager` |

`Engine` kept exactly what it should have: construction, the loop, and three
private lines delegating to the four managers.

Worth noting what the split did *not* fix. The duplicated unit-update loop
above was carried across the refactor into `GameManager::Update` intact and
lived there until it was found; partitioning a monolith moves its bugs, it does
not audit them.

## What would bring it back

`Nothing`. The 2025-12 refactor it predates is the architecture the whole
`src/Engine/` directory is now built on, and `git show d1b4098` has the bytes
if a specific old behaviour ever needs reading. If a future session wants a
scratch copy of a file while restructuring it, that is what a branch is for —
a `_copy` file in the source tree outlives the work it was made for and then
lies about being code.
