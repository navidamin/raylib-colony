#ifndef SURVEY_HINTS_H
#define SURVEY_HINTS_H

// The hint layer of site selection: one short explanation per survey
// quantity, chosen by the VALUE (its low/mid/high band), plus
// categorical hints for rock types and PSR proximity.
//
// Design: docs/design/site-selection/site-selection-master-design.md
// (the teaching layer). Shown ON HOVER only -- never permanently on a
// panel. The numbers stay authoritative; the hint says what a number
// of that size MEANS for the player's plans.
//
// This is data, not logic: the descriptor-table pattern used by
// resource_types.h. Band thresholds live here too, so "high titanium"
// is defined exactly once, in one place the whole game reads.
//
// Wording rule (coherency contract, master design SS5.0): a hint may
// name what a resource is FOR in this game's design ("feedstock for
// alloys"), but must not claim a live consequence from a system that
// does not exist yet. Wording that implies simulation ("production has
// stalled") lands with the system that simulates it.

#include <cstring>

struct SurveyHintResult
{
    const char* title;    // e.g. "HIGH TITANIUM" -- the classification
    const char* text;     // one short paragraph, plain language
};

struct SurveyHintBand
{
    const char* key;       // quantity key: "iron", "titanium", ...
    float lowBelow;        // value < lowBelow          -> low band
    float highAbove;       // value > highAbove         -> high band
    const char* lowTitle;
    const char* lowText;
    const char* midTitle;
    const char* midText;
    const char* highTitle;
    const char* highText;
};

// ---------------------------------------------------------------------------
// Banded quantities. Thresholds follow the real classifications where
// one exists: TiO2 bands are the very-low/low/high-Ti basalt scheme,
// the thorium high band is the PKT contour (3.5 ppm), iron splits
// feldspathic from mafic ground at ~8-10 wt% FeO.
// ---------------------------------------------------------------------------
inline const SurveyHintBand* GetSurveyHintBands(int* count)
{
    static const SurveyHintBand BANDS[] =
    {
        { "iron", 8.0f, 10.0f,
          "LOW IRON",
          "Feldspathic ground - little metal in the rock. Structural "
          "iron and steel will lean on imports or thin extraction. The "
          "same chemistry that starves iron enriches aluminium and "
          "calcium: this is construction country, not foundry country.",
          "MODERATE IRON",
          "Mixed ground between highland and mare chemistry. Iron is "
          "workable but not rich; expect neither a foundry economy nor "
          "a construction bonanza from the rock alone.",
          "HIGH IRON",
          "Mafic basalt - the foundry feedstock. Direct ore for iron, "
          "steel and machinery chains. The trade is built into the "
          "chemistry: iron-rich rock is aluminium-poor, so structures "
          "lean on imported or hauled highland material." },

        { "titanium", 3.0f, 6.0f,
          "LOW TITANIUM",
          "Low-Ti basalt - fine for bulk iron and steel, but poor in "
          "ilmenite, the mineral that matters: little oxygen-from-"
          "regolith yield and a weak helium-3 trap. Plain metal "
          "country.",
          "INTERMEDIATE TITANIUM",
          "Workable ilmenite for oxygen extraction and titanium "
          "alloys, without the wealth of the true high-Ti flows.",
          "HIGH TITANIUM",
          "Ilmenite-rich basalt - the prize ore. Ilmenite gives "
          "titanium alloys, the best oxygen-from-regolith yield, and "
          "retains implanted helium-3 better than any other common "
          "lunar mineral. High-Ti flows are why prospectors care which "
          "basalt they are standing on." },

        { "thorium", 1.5f, 3.5f,
          "LOW THORIUM",
          "Ordinary crust, far from the KREEP province. Nothing "
          "special for research here - science output rests on the "
          "labs, not the ground.",
          "MODERATE THORIUM",
          "Elevated incompatible elements - KREEP-contaminated ground "
          "near the province edge. A modest draw for research.",
          "HIGH THORIUM",
          "KREEP ground - inside the Procellarum thorium contour, the "
          "Moon's radiogenic province. The signature of concentrated "
          "incompatible elements: the strongest ground bonus research "
          "can get, and a marker of the Moon's strangest geology." },

        { "aluminium", 8.0f, 12.0f,
          "LOW ALUMINIUM",
          "Mafic rock holds little plagioclase, so little aluminium "
          "and calcium: construction materials will need importing or "
          "hauling from highland ground.",
          "MODERATE ALUMINIUM",
          "Some plagioclase in the mix - partial cover for "
          "construction needs.",
          "HIGH ALUMINIUM",
          "Anorthositic rock - plagioclase is the construction "
          "feedstock (aluminium and calcium together). Cheap "
          "structures; the same chemistry keeps iron scarce." },

        { "illumination", 45.0f, 70.0f,
          "STANDARD NIGHTS",
          "Equatorial-style lighting: about 14 days of darkness every "
          "month. Power must be stored or generation paused through "
          "the night - the defining rhythm of non-polar operations.",
          "EXTENDED SUN",
          "Better than the equatorial cycle - terrain shortens the "
          "nights here.",
          "NEAR-CONSTANT SUN",
          "Polar crest lighting: the sun circles the horizon and this "
          "ground catches it most of the month. Small storage, "
          "near-continuous power - the reason polar rims are prime "
          "real estate despite everything else about them." },

        { "earthlink", 40.0f, 85.0f,
          "EARTH OFTEN BELOW HORIZON",
          "Line of sight to Earth is intermittent or absent: control "
          "and data need a relay or more autonomy. Expect isolation.",
          "EARTH INTERMITTENT",
          "Earth dips in and out of view with libration - workable, "
          "with gaps.",
          "EARTH ALWAYS IN VIEW",
          "Continuous direct line to Earth: full-time communications, "
          "teleoperation and data return. The quiet advantage of the "
          "central near side." },
    };
    if (count) *count = (int)(sizeof(BANDS) / sizeof(BANDS[0]));
    return BANDS;
}

// Value -> banded hint. Returns nulls if the key is unknown.
inline SurveyHintResult GetSurveyHint(const char* key, float value)
{
    SurveyHintResult result = { nullptr, nullptr };
    int count = 0;
    const SurveyHintBand* bands = GetSurveyHintBands(&count);
    for (int i = 0; i < count; i++)
    {
        if (std::strcmp(bands[i].key, key) != 0) continue;
        if (value < bands[i].lowBelow)
        {
            result.title = bands[i].lowTitle;
            result.text = bands[i].lowText;
        }
        else if (value > bands[i].highAbove)
        {
            result.title = bands[i].highTitle;
            result.text = bands[i].highText;
        }
        else
        {
            result.title = bands[i].midTitle;
            result.text = bands[i].midText;
        }
        return result;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Categorical hints: rock types (matched loosely against the region's
// dominant-rock string, mixed forms first) and PSR proximity.
// ---------------------------------------------------------------------------
inline SurveyHintResult GetRockHint(const char* rockName)
{
    SurveyHintResult result = { nullptr, nullptr };
    if (rockName == nullptr) return result;
    if (std::strstr(rockName, "/") != nullptr ||
        (std::strstr(rockName, "basalt") && std::strstr(rockName, "breccia")))
    {
        result.title = "SHORE GROUND";
        result.text =
            "Two rock families within reach: basalt for metal on one "
            "side, feldspathic rock for construction on the other. "
            "Nothing at top grade, but nothing to import either - the "
            "generalist's position, chosen by settling a boundary.";
    }
    else if (std::strstr(rockName, "anorthosite"))
    {
        result.title = "ANORTHOSITE";
        result.text =
            "Ancient highland crust - plagioclase rock, rich in "
            "aluminium and calcium, poor in iron. The construction "
            "feedstock, on rough ground.";
    }
    else if (std::strstr(rockName, "impact-melt"))
    {
        result.title = "IMPACT MELT";
        result.text =
            "Crater-floor melt sheet - whatever the impact hit, "
            "remixed and refrozen. Composition is a lottery until "
            "prospected; melt sheets can concentrate odd materials.";
    }
    else if (std::strstr(rockName, "basalt"))
    {
        result.title = "MARE BASALT";
        result.text =
            "Flood lava plain - flat, iron-rich, the easiest ground "
            "on the Moon to build and drive on. The industrial base "
            "rock; check the titanium number to see which kind.";
    }
    return result;
}

// Distance to the nearest permanently shadowed region, in km.
// The near threshold is a short haul for a surface rover.
inline SurveyHintResult GetPsrHint(float distanceKm)
{
    SurveyHintResult result;
    if (distanceKm <= 15.0f)
    {
        result.title = "PSR IN HAUL RANGE";
        result.text =
            "A permanently shadowed crater within rover reach - the "
            "only ground on the Moon where water ice survives. Ice "
            "extraction becomes possible, at the price of operating "
            "beside terrain at 40 K that no machine enters casually.";
    }
    else
    {
        result.title = "NO PSR IN REACH";
        result.text =
            "No permanent shadow within haul range means no local "
            "water ice. Water and fuel must come from hydrogen "
            "extracted out of sunlit regolith at parts-per-million "
            "grades - workable, slow - or from imports.";
    }
    return result;
}

#endif // SURVEY_HINTS_H
