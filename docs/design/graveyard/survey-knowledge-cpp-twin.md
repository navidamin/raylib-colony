# The C++ twin of the knowledge model

**Lived:** `src/Survey/survey_knowledge.{h,cpp}` and the
`SURVEY_K_*` / `SURVEY_DELIN_GATE` block of `src/Survey/survey_constants.h`
**Removed:** real-data integration, P3 (the implementation) and P5c (the face)
**Replaced by:** `src/ui/dash_knowledge.{h,c}`, called directly

## What it was

The knowledge model — how much a borehole settles the ground around and below
it, and the one delineation number that says whether the block is INFERRED,
INDICATED or MEASURED. It was written in C++ for `SurveyConsole` first. When
the ported console needed the same model, `colony_c2d` could not use a C++
class, so the formulas and the six constants were transcribed into C in
`dash_knowledge.c`.

That transcription was deliberate and its own header said so out loud: *"the
two must never disagree about the same rock."* Two copies of a formula only
stay equal while someone keeps checking.

## Why it went

Nothing was wrong with it. It was a **second** copy, and the plan that follows
this commit retunes the depth term — `K_SKIRT_M` is calibrated for a 2 km
column and the game's column is 120 m. Retuning one of two copies is how a
duplicate turns into a divergence, and the divergence would have been invisible:
both consoles draw fog from their own copy, so they would simply have disagreed
about the same rock while each looked internally consistent.

Collapsing them first is what makes that retune a one-line change.

## What survived

All of it. The constants moved to `dash_knowledge.h` as `DK_K_*` with their
values unchanged, and `SurveyKnowledge` still exists — same name, same methods,
same call sites — with every body delegating to `DashKnow_*`. Verified rather
than asserted: the deleted implementation and the surviving one were compiled
side by side and fed the same eleven holes, and all **17,347** compared values
(`KnowAt` over a 15×15×7 sample volume after each hole, plus `Delineation` on
both a 120 m and a 2 km column) were bit-identical.

That harness also found a latent bug the duplicate had hidden in both copies:
`Delineation` cached against the hole revision alone, so the second caller to
ask about a *different column* was answered out of the first caller's cache.
Only one column is asked today, which is why it never showed. It is fixed in
the surviving copy.

One behaviour did change, and it is a cap rather than a formula:
`DashKnowledge` stores at most `DK_HOLES_MAX` (64) holes where the
`std::vector` was unbounded. Seven well-spread full-depth holes already clear
the MEASURED gate and the model saturates at 1.0 long before 64, so no output
can move once the cap is reached.

## The second half: the face went too (P5c)

P3 left `SurveyKnowledge` standing as a header-only C++ class whose every
method was a one-line forward to `DashKnow_*`. P5c removed it.

The reason was ownership, not tidiness. P5c makes the console's own state own
the knowledge model — it is what the player's drill writes into — and
`SurveyConsole` borrow it, so the block is regenerated from the same holes the
drill just made. A class that owns its storage by value cannot be borrowed, and
a version that owns *or* borrows behind a raw pointer is a copy hazard for no
gain when every caller wanted the same four free functions anyway.

So `SurveyGround::Build`, `SurveyBlock::DrawCage` and `DrawBeds` now take a
`const DashKnowledge&`, and `SurveyConsole` holds a `DashKnowledge*` pointing
at `ProspectingSystem::Dash().own`. There is one model, one owner, and no layer
between them.

## What would bring it back

A second consumer of the model that genuinely must not link `colony_c2d` —
a server, a tool, a port where the UI library is unavailable. The fix then is
still not a second copy: lift `dash_knowledge.c` out of `colony_c2d` into a
pure-logic target of its own and let both link that. Static linking already
pulls only that object, which is how `colony_sim` uses it today.
