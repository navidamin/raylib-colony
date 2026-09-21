#ifndef SITE_SELECTION_CONSTANTS_H
#define SITE_SELECTION_CONSTANTS_H

// Numbers the site-selection module holds in one place, so the game, the
// instrument and the tests read the same ones.
//
// Design: docs/design/site-selection/site-selection-master-design.md

#include "survey_cursor.h"

// ---------------------------------------------------------------------------
// The verdict (master design SS3.2). A site is refused when any of these
// is exceeded, and the refusal names the one that was.
// ---------------------------------------------------------------------------
const float SITE_MAX_MEAN_SLOPE_DEG = 8.0f;
const float SITE_MAX_PEAK_SLOPE_DEG = 25.0f;
const float SITE_MAX_ROUGHNESS_M = 40.0f;
const float SITE_MAX_RELIEF_M = 400.0f;

// How far out EvaluateSite ray-marches the skyline for the live verdict.
// Shorter than the survey grid's 60 km because it runs every frame the
// cursor moves; the illumination it yields is what the card shows.
const double SITE_VERDICT_HORIZON_KM = 30.0;

// ---------------------------------------------------------------------------
// The rungs. One question per level (master design SS2); the count comes
// from the ladder so the HUD cannot promise levels the descent lacks.
// ---------------------------------------------------------------------------
const int SITE_LEVELS = SURVEY_LEVEL_COUNT;
static_assert(SITE_LEVELS == 3, "the level questions below are written for three rungs");

inline const char* SiteLevelQuestion(int level)
{
    static const char* QUESTIONS[SITE_LEVELS] =
    {
        "WHICH ECONOMY?", "WHICH MIX?", "WHICH GROUND?"
    };
    if (level < 0 || level >= SITE_LEVELS) return "";
    return QUESTIONS[level];
}

// ---------------------------------------------------------------------------
// Descent flights. Not one duration: level 1 -> 2 is a ~19x zoom and the
// rest 5-8x, so the flight holds a constant RATE of approach -- octaves
// of zoom per second -- and takes as long as its distance needs, within
// bounds.
// ---------------------------------------------------------------------------
const float SITE_FLIGHT_OCTAVES_PER_SEC = 1.8f;
const float SITE_FLIGHT_MIN_SECONDS = 1.00f;
const float SITE_FLIGHT_MAX_SECONDS = 3.00f;

// ---------------------------------------------------------------------------
// Pointer rules.
// ---------------------------------------------------------------------------
// A press that travels further than this is a drag, not a click. Far
// enough to mean it, close enough that a firm click still counts.
const float SURVEY_DRAG_THRESHOLD_PX = 5.0f;
// A pointer that arrives from further than this in one frame is a touch:
// the finger landed and clicked at once, before the player saw the card.
// Such a click only aims; a second one commits.
const float SURVEY_POINTER_JUMP_PX = 24.0f;
// Below this the pointer counts as still, which is what lets the
// instrument sharpen its ground without freezing under a moving hand.
const float SURVEY_POINTER_MOVE_PX = 2.0f;

// ---------------------------------------------------------------------------
// Region card hit rows: which row of the card a pointer is over, as
// offsets from the card's top. Shared between the card's drawing and the
// controller's hit-test so they cannot drift apart.
// ---------------------------------------------------------------------------
struct SurveyCardRow
{
    int yOffset;
    const char* hintKey;
};

inline const SurveyCardRow* GetRegionCardRows(int* count)
{
    static const SurveyCardRow ROWS[] =
    {
        { 58,  "rock" },
        { 112, "iron" },
        { 144, "titanium" },
        { 176, "thorium" },
        { 210, "psr" },
    };
    if (count) *count = (int)(sizeof(ROWS) / sizeof(ROWS[0]));
    return ROWS;
}
const int SURVEY_CARD_ROW_HEIGHT = 26;

// ---------------------------------------------------------------------------
// Card layout, shared by the drawing and the hit-testing (SurveyLayout).
// ---------------------------------------------------------------------------
const int SURVEY_CARD_W = 336;
const int SURVEY_CARD_TOP = 64;
const int SURVEY_REGION_CARD_H = 252;
const int SURVEY_STRIP_H = 40;
// Below this width the two cards cannot sit side by side: one gets the
// width and the other collapses to a name strip.
const int SURVEY_NARROW_SCREEN_W = 720;

inline int SurveyLevelCardHeight(int level)
{
    // The site rung's verdict line is 16 pt at py + 221; the card has to
    // clear it.
    if (level == SITE_LEVELS - 1) return 252;
    if (level == SITE_LEVELS - 2) return 208;
    return 164;
}

// The polar cap the pictures cannot yet draw. Every window on the Moon is
// cut from the equirectangular mosaic with longitude widened by
// 1/cos(lat), floored (0.2 in the chain, 0.05 in the survey cursor). Past
// this latitude the floors bite: the ground smears into streaks and the
// two frames disagree about where east is, so a colony founded there is
// drawn somewhere other than where it was placed (measured at Shackleton,
// 2026-09-21: game-integration-plan.md D7). A claim there is refused, and
// the refusal names the picture, not the ground. A tangent-plane frame
// would lift this.
const double SITE_POLAR_FRAME_LAT_DEG = 80.0;

// Colony markers on the globe: how large one is drawn and how close a
// click must land to open it rather than found a new colony beside it.
const float ORBITAL_MARKER_RADIUS_PX = 8.0f;
const float ORBITAL_MARKER_PICK_PX = 18.0f;

#endif // SITE_SELECTION_CONSTANTS_H
