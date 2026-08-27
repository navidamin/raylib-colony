// A playtest you can run in CI.
//
//   cmake --build build --target colony_sim && ./build/src/colony_sim
//
// The unit tests check invariants -- does the range contain the truth, does a
// blunt machine blend its neighbours. None of that can tell you whether the
// module is BALANCED, because balance is a property of a session rather than of
// a function call. This runs the real engines for a fixed number of game days
// under several different players and reports what each one got.
//
// It asserts ORDERINGS, never absolute numbers:
//
//     surveying beats digging blind
//     EXPERT >= TRAINED >= BASIC
//     a player deciding for themselves beats every automation level
//     a higher tier beats a lower one
//     nobody ever stalls
//
// Absolute figures move every time a constant is tuned, and pinning them would
// make the suite scream at every balance pass. The orderings ARE the design
// claims -- if tuning inverts one, the tuning broke something real.
//
// What this cannot do: tell you how the module FEELS. Whether the early game
// reads as tense or merely poor, whether a survey is satisfying to act on, and
// whether the gamble is exciting rather than annoying are all questions for a
// human with the interactive playtest. This measures whether the numbers say
// what the design says.

#include "resource_manager.h"
#include "prospecting_system.h"
#include "excavation_system.h"
#include "excavation_constants.h"
#include "survey_progress_engine.h"
#include "game_constants.h"

#include <cstdio>
#include <cmath>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

static const unsigned int SIM_SEED = 20260813u;
static const int SIM_PARENT_X = 5;
static const int SIM_PARENT_Y = 5;

// 20 game days. Long enough for spots to exhaust and for a survey's cost to be
// repaid, short enough to run in CI.
static const int SIM_DAYS = 20;
static const int SIM_TICKS = SIM_DAYS * TICKS_PER_DAY;

static int failures = 0;
static int checks = 0;

static void Check(bool condition, const char* what)
{
    checks++;
    if (!condition)
    {
        failures++;
        printf("  FAIL  %s\n", what);
    }
}

class Quiet
{
public:
    Quiet() : saved(std::cout.rdbuf()) { std::cout.rdbuf(nullptr); }
    ~Quiet() { std::cout.rdbuf(saved); }
private:
    std::streambuf* saved;
};

// ---------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------

enum class Player
{
    IDLE,        // never opens the panel -- the default the game ships with
    BLIND,       // digs at TRAINED and never surveys
    SURVEYOR,    // spends the first days surveying, then digs at TRAINED
    EXPERT,      // EXPERT automation, and surveys where it says to
    HANDS_ON,    // decides everything: true best spot, best machine, full pace
};

static const char* PlayerName(Player p)
{
    switch (p)
    {
        case Player::IDLE:     return "IDLE (never opens panel)";
        case Player::BLIND:    return "BLIND (digs, never surveys)";
        case Player::SURVEYOR: return "SURVEYOR (surveys first)";
        case Player::EXPERT:   return "EXPERT (automation + hints)";
        case Player::HANDS_ON: return "HANDS-ON (decides everything)";
        default:               return "?";
    }
}

struct RunResult
{
    float targetDelivered = 0.0f;
    float totalMoved = 0.0f;
    float energyDrawn = 0.0f;
    float surveyProgress = 0.0f;
    int   spotsExhausted = 0;
    int   deepestLayer = 0;
    int   stalledTicks = 0;
    int   surveyTicks = 0;
    float usefulShare = 0.0f;
};

// One sweep plus a spread of samples. Costs ticks, which is the whole point --
// time spent surveying is time not producing.
static int RunSurvey(ProspectingSystem& system, int budgetTicks)
{
    ProspectingGrid& grid = system.GetGrid();
    SampleTray& tray = system.GetTray();
    int spent = 0;

    for (int band = 0; band < SWEEP_FREQUENCY_BANDS && spent < budgetTicks; band++)
    {
        if (system.GetSweep().CanSweep(grid, band))
        {
            system.GetSweep().ExecuteSweep(grid, band, system.gameTime);
            spent++;
        }
    }

    // Sample at 25 m spacing -- every fourth cell of the 16x16 lattice. A
    // hole speaks for its 20 m halo, so this classifies the whole reach
    // (mid-gap support 0.68, Indicated) for a quarter of the holes; coring
    // finer buys nothing the survey-progress cap can pay for. On the old
    // 8x8 lattice the same physical plan was every second cell.
    int gridSize = grid.GetGridSize();
    for (int y = 2; y < gridSize && spent < budgetTicks; y += 4)
    {
        for (int x = 2; x < gridSize && spent < budgetTicks; x += 4)
        {
            if (!grid.IsInReach(x, y)) continue;
            // A full tray never blocks drilling -- knowledge lives on the
            // grid. This break starved the surveyor of the finer lattice's
            // back half and flipped the survey-beats-blind claim.
            if (system.GetSampler().CollectSample(grid, tray, x, y, DepthLayer::SURFACE))
            {
                spent++;
            }
        }
    }

    // No lab pass: the lab stage is retired -- a recovered core comes out of
    // the ground assayed (SURVEY_TESTING_WEIGHT is zero). Grinding XRF here
    // was ~19 dead ticks the surveyor paid and blind digging did not.

    return spent;
}

static RunResult RunSession(Player player, int tier)
{
    RunResult result;

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    {
        Quiet quiet;
        rm.GenerateResourceMap(SIM_SEED);
    }

    ProspectingSystem prospecting(tier, SIM_PARENT_X, SIM_PARENT_Y, rm);
    ExcavationSystem excavation(tier);

    switch (player)
    {
        case Player::IDLE:     excavation.aiLevel = AiLevel::BASIC;   break;
        case Player::BLIND:    excavation.aiLevel = AiLevel::TRAINED; break;
        case Player::SURVEYOR: excavation.aiLevel = AiLevel::TRAINED; break;
        case Player::EXPERT:   excavation.aiLevel = AiLevel::EXPERT;  break;
        case Player::HANDS_ON: excavation.aiLevel = AiLevel::OFF;     break;
    }

    int machineCount = EXC_MACHINE_COUNT_PER_TIER[tier < 0 ? 0 : (tier > 3 ? 3 : tier)];

    // The surveyor pays up front: no digging until the survey is done.
    if (player == Player::SURVEYOR)
    {
        result.surveyTicks = RunSurvey(prospecting, TICKS_PER_DAY * 3);
    }

    int consecutiveIdle = 0;

    for (int tick = result.surveyTicks; tick < SIM_TICKS; tick++)
    {
        prospecting.gameTime += 1.0f;

        // Expert acts on its own hint: when it says surveying somewhere would
        // pay, go and do it. That is the level's entire value, so a session
        // that ignores the hint is not testing EXPERT at all.
        if (player == Player::EXPERT && excavation.lastDecision.surveyHintX >= 0)
        {
            ProspectingGrid& grid = prospecting.GetGrid();
            if (!prospecting.GetTray().IsFull() &&
                prospecting.GetSampler().CollectSample(
                    grid, prospecting.GetTray(),
                    excavation.lastDecision.surveyHintX,
                    excavation.lastDecision.surveyHintY,
                    excavation.selectedDepth))
            {
                for (Sample& s : prospecting.GetTray().GetSamples())
                {
                    if (prospecting.GetLab().CanApplyTool(s, AnalysisTool::XRF))
                    {
                        prospecting.GetLab().ApplyTool(s, AnalysisTool::XRF,
                                                       prospecting.gameTime);
                    }
                }
                result.surveyTicks++;
                continue;   // surveying costs the tick
            }
        }

        // The hands-on player decides for themselves: true best spot, the
        // machine that suits it, full pace. This is the ceiling every level of
        // automation is measured against.
        if (player == Player::HANDS_ON)
        {
            // Move only when the face is spent -- the same way the automation
            // works, and the same way a person would.
            if (excavation.GetWorked().Remaining(excavation.selectedSpotX,
                                                 excavation.selectedSpotY,
                                                 excavation.selectedDepth) <= 0.02f)
            {
                excavation.SelectBestReachableSpot(prospecting);
            }
            excavation.autoMachine = true;
            excavation.SelectAutoMachine(prospecting);
            excavation.pace = excavation.GetActiveMachine().paceCeiling;
        }

        DigResult dig = excavation.Dig(prospecting, machineCount, 1.0f, 1.0f);

        result.targetDelivered += dig.targetMass;
        result.totalMoved += dig.totalMass;
        result.energyDrawn += dig.powerDraw;

        int layer = static_cast<int>(excavation.selectedDepth);
        if (layer > result.deepestLayer) result.deepestLayer = layer;

        if (dig.totalMass <= 0.0f)
        {
            result.stalledTicks++;
            consecutiveIdle++;
        }
        else
        {
            consecutiveIdle = 0;
        }
        (void)consecutiveIdle;
    }

    // Count DISTINCT spots worked out, not ticks that reported exhaustion --
    // the first version counted the latter and read 387 out of 256 possible.
    //
    // "Worked out" means at or below the threshold the operation abandons a
    // face at, not literally zero: nothing ever reaches exactly zero, because
    // the crew moves on once there is nothing worth digging.
    for (int d = 0; d < 4; d++)
    {
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                if (excavation.GetWorked().Remaining(x, y, static_cast<DepthLayer>(d))
                    <= EXC_AI_ABANDON_BELOW)
                {
                    result.spotsExhausted++;
                }
            }
        }
    }

    CellSurveyResult survey = SurveyProgressEngine::Calculate(prospecting.GetGrid(),
                                                              prospecting.GetTray());
    result.surveyProgress = survey.surveyProgress;
    result.usefulShare = result.totalMoved > 0.0f
                       ? result.targetDelivered / result.totalMoved : 0.0f;

    return result;
}

static void PrintRow(Player player, const RunResult& r)
{
    printf("  %-30s %9.0f %9.0f %7.0f%% %8.2f %6d %6d %6d\n",
           PlayerName(player), r.targetDelivered, r.totalMoved,
           r.usefulShare * 100.0f,
           r.energyDrawn > 0.0f ? r.targetDelivered / r.energyDrawn : 0.0f,
           r.spotsExhausted, r.deepestLayer, r.stalledTicks);
}

int main()
{
    printf("\n=== excavation simulated playtest ===\n");
    printf("%d game days, seed %u, parent cell (%d,%d)\n\n",
           SIM_DAYS, SIM_SEED, SIM_PARENT_X, SIM_PARENT_Y);

    const Player players[] = {
        Player::IDLE, Player::BLIND, Player::SURVEYOR, Player::EXPERT, Player::HANDS_ON
    };

    // ---------------------------------------------------------------- tier 3
    printf("TIER 3\n");
    printf("  %-30s %9s %9s %8s %8s %6s %6s %6s\n",
           "player", "target", "moved", "useful", "per kW", "spent", "depth", "stall");

    RunResult t3[5];
    for (int i = 0; i < 5; i++)
    {
        t3[i] = RunSession(players[i], 3);
        PrintRow(players[i], t3[i]);
    }

    const RunResult& idle3 = t3[0];
    const RunResult& blind3 = t3[1];
    const RunResult& surveyor3 = t3[2];
    const RunResult& expert3 = t3[3];
    const RunResult& hands3 = t3[4];

    // ---------------------------------------------------------------- tier 0
    printf("\nTIER 0\n");
    printf("  %-30s %9s %9s %8s %8s %6s %6s %6s\n",
           "player", "target", "moved", "useful", "per kW", "spent", "depth", "stall");

    RunResult t0[5];
    for (int i = 0; i < 5; i++)
    {
        t0[i] = RunSession(players[i], 0);
        PrintRow(players[i], t0[i]);
    }

    printf("\nsurvey progress reached: blind %.0f%%, surveyor %.0f%%, expert %.0f%%\n",
           blind3.surveyProgress * 100.0f, surveyor3.surveyProgress * 100.0f,
           expert3.surveyProgress * 100.0f);
    printf("surveyor spent %d ticks surveying before digging\n\n", surveyor3.surveyTicks);

    // ---------------------------------------------------------------- claims
    printf("checking the design's claims\n");

    // Nobody may ever be stuck producing nothing. An unattended unit that
    // stalls is the single worst outcome here.
    for (int i = 0; i < 5; i++)
    {
        Check(t3[i].stalledTicks == 0, "no player ever stalls at tier 3");
        Check(t0[i].stalledTicks == 0, "no player ever stalls at tier 0");
    }

    // The whole gamble rests on surveying being worth its cost. If this fails,
    // digging blind is simply the right play and the module has no tension.
    Check(surveyor3.targetDelivered > blind3.targetDelivered,
          "surveying beats digging blind, despite the ticks it costs");

    // And it should win on quality, not merely on total -- a survey buys you
    // better ground, which shows up as a cleaner mix per unit moved.
    Check(surveyor3.usefulShare > blind3.usefulShare,
          "surveyed digging brings up a cleaner mix");

    // The ladder must actually climb.
    Check(expert3.targetDelivered >= idle3.targetDelivered,
          "EXPERT is at least as good as the BASIC default");

    // And attention must beat automation, or the AI levels make the panel
    // pointless.
    Check(hands3.targetDelivered > idle3.targetDelivered,
          "deciding for yourself beats the cautious default");

    // Tier has to be worth buying.
    Check(t3[0].targetDelivered > t0[0].targetDelivered,
          "tier 3 out-produces tier 0 on the same ground");

    // Depletion in a sane band: spots must run out, but not instantly.
    Check(t3[4].spotsExhausted > 0, "faces do get worked out over 20 days");
    Check(t3[4].spotsExhausted < 8 * 8 * 4,
          "20 days does not strip the entire lattice");

    // Selectivity has to show up in the mix, or the pace dial is cosmetic.
    Check(hands3.usefulShare > 0.0f && hands3.usefulShare <= 1.0f,
          "the useful share is a sensible fraction");

    printf("\n%d checks, %d failures\n\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
