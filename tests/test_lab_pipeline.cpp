#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "lab_engine.h"
#include "sampling_engine.h"

// --- Tool tier availability ---

TEST_CASE("LabEngine tool tier availability", "[lab]")
{
    Sample s = MakeFullCompositionSample();

    LabEngine t0(0);
    REQUIRE(t0.CanApplyTool(s, AnalysisTool::VISUAL_INSPECTION));
    REQUIRE_FALSE(t0.CanApplyTool(s, AnalysisTool::XRF));
    REQUIRE_FALSE(t0.CanApplyTool(s, AnalysisTool::LIBS_PULSE));
    REQUIRE_FALSE(t0.CanApplyTool(s, AnalysisTool::FIRE_ASSAY));

    LabEngine t1(1);
    REQUIRE(t1.CanApplyTool(s, AnalysisTool::XRF));
    REQUIRE(t1.CanApplyTool(s, AnalysisTool::OPTICAL_MICROSCOPY));
    REQUIRE(t1.CanApplyTool(s, AnalysisTool::MAGNETIC_SUSCEPTIBILITY));
    REQUIRE_FALSE(t1.CanApplyTool(s, AnalysisTool::LIBS_PULSE));

    LabEngine t2(2);
    REQUIRE(t2.CanApplyTool(s, AnalysisTool::LIBS_PULSE));
    REQUIRE_FALSE(t2.CanApplyTool(s, AnalysisTool::FIRE_ASSAY));

    LabEngine t3(3);
    REQUIRE(t3.CanApplyTool(s, AnalysisTool::FIRE_ASSAY));
}

// --- Separation tier availability ---

TEST_CASE("LabEngine separation tier availability", "[lab]")
{
    Sample s = MakeFullCompositionSample();

    LabEngine t0(0);
    REQUIRE_FALSE(t0.CanApplySeparation(s, SeparationMethod::MAGNETIC));

    LabEngine t1(1);
    REQUIRE(t1.CanApplySeparation(s, SeparationMethod::MAGNETIC));
    REQUIRE_FALSE(t1.CanApplySeparation(s, SeparationMethod::HEAVY_MINERAL));

    LabEngine t2(2);
    REQUIRE(t2.CanApplySeparation(s, SeparationMethod::HEAVY_MINERAL));
    REQUIRE(t2.CanApplySeparation(s, SeparationMethod::VOLATILE_EXTRACTION));
}

TEST_CASE("Separation NONE is rejected", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);
    REQUIRE_FALSE(engine.CanApplySeparation(s, SeparationMethod::NONE));
}

// --- Tool application: confidence effects ---

TEST_CASE("Visual inspection adds confidence to all elements", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(0);

    REQUIRE(engine.ApplyTool(s, AnalysisTool::VISUAL_INSPECTION, 100.0f));

    for (const auto& [type, abundance] : s.trueComposition)
    {
        if (abundance > 0.01f)
        {
            REQUIRE(s.elementConfidence[type] > 0.0f);
            REQUIRE(s.elementConfidence[type] <= CONFIDENCE_VISUAL_MAX);
        }
    }
}

TEST_CASE("XRF adds confidence to heavy elements only", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(1);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);

    REQUIRE(s.elementConfidence[ResourceType::Fe] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::Si] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::Ti] > 0.0f);

    REQUIRE(s.elementConfidence.find(ResourceType::H2) == s.elementConfidence.end());
    REQUIRE(s.elementConfidence.find(ResourceType::O2) == s.elementConfidence.end());
}

TEST_CASE("LIBS adds confidence to all elements including light", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    engine.ApplyTool(s, AnalysisTool::LIBS_PULSE, 100.0f);

    REQUIRE(s.elementConfidence[ResourceType::Fe] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::H2] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::O2] > 0.0f);
}

TEST_CASE("Magnetic susceptibility only affects Fe and Ti", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(1);

    engine.ApplyTool(s, AnalysisTool::MAGNETIC_SUSCEPTIBILITY, 100.0f);

    REQUIRE(s.elementConfidence[ResourceType::Fe] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::Ti] > 0.0f);
    REQUIRE(s.elementConfidence.find(ResourceType::Si) == s.elementConfidence.end());
    REQUIRE(s.elementConfidence.find(ResourceType::H2) == s.elementConfidence.end());
}

// --- Fire assay ---

TEST_CASE("Fire assay sets target to 100% and marks COMPLETED", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(3);

    REQUIRE(engine.ApplyTool(s, AnalysisTool::FIRE_ASSAY, 100.0f, ResourceType::Fe));
    REQUIRE_THAT(s.elementConfidence[ResourceType::Fe],
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE(s.state == SampleState::COMPLETED);
}

TEST_CASE("Cannot apply tools to COMPLETED sample", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(3);

    engine.ApplyTool(s, AnalysisTool::FIRE_ASSAY, 100.0f, ResourceType::Fe);
    REQUIRE_FALSE(engine.CanApplyTool(s, AnalysisTool::XRF));
    REQUIRE_FALSE(engine.ApplyTool(s, AnalysisTool::XRF, 200.0f));
}

TEST_CASE("Cannot apply separation to COMPLETED sample", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(3);

    engine.ApplyTool(s, AnalysisTool::FIRE_ASSAY, 100.0f, ResourceType::Fe);
    REQUIRE_FALSE(engine.CanApplySeparation(s, SeparationMethod::MAGNETIC));
}

// --- State transitions ---

TEST_CASE("First tool transitions IN_TRAY to PROCESSING", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    REQUIRE(s.state == SampleState::IN_TRAY);

    LabEngine engine(0);
    engine.ApplyTool(s, AnalysisTool::VISUAL_INSPECTION, 100.0f);
    REQUIRE(s.state == SampleState::PROCESSING);
}

TEST_CASE("Further tools keep PROCESSING state", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);
    REQUIRE(s.state == SampleState::PROCESSING);

    engine.ApplyTool(s, AnalysisTool::LIBS_PULSE, 200.0f);
    REQUIRE(s.state == SampleState::PROCESSING);
}

TEST_CASE("Fire assay transitions PROCESSING to COMPLETED", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(3);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);
    REQUIRE(s.state == SampleState::PROCESSING);

    engine.ApplyTool(s, AnalysisTool::FIRE_ASSAY, 200.0f, ResourceType::Fe);
    REQUIRE(s.state == SampleState::COMPLETED);
}

TEST_CASE("Can still apply tools to PROCESSING sample", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    engine.ApplyTool(s, AnalysisTool::VISUAL_INSPECTION, 100.0f);
    REQUIRE(s.state == SampleState::PROCESSING);
    REQUIRE(engine.CanApplyTool(s, AnalysisTool::XRF));
    REQUIRE(engine.ApplyTool(s, AnalysisTool::XRF, 200.0f));
}

// --- Separation effects ---

TEST_CASE("Magnetic separation adds confidence to Fe and Ti", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(1);

    REQUIRE(engine.ApplySeparation(s, SeparationMethod::MAGNETIC, 100.0f));
    REQUIRE(s.elementConfidence[ResourceType::Fe] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::Ti] > 0.0f);
    REQUIRE(s.separationApplied == SeparationMethod::MAGNETIC);
}

TEST_CASE("Volatile extraction adds confidence to light elements", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    engine.ApplySeparation(s, SeparationMethod::VOLATILE_EXTRACTION, 100.0f);
    REQUIRE(s.elementConfidence[ResourceType::H2] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::O2] > 0.0f);
}

TEST_CASE("Separation can only be applied once", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    REQUIRE(engine.ApplySeparation(s, SeparationMethod::MAGNETIC, 100.0f));
    REQUIRE_FALSE(engine.CanApplySeparation(s, SeparationMethod::HEAVY_MINERAL));
    REQUIRE_FALSE(engine.ApplySeparation(s, SeparationMethod::HEAVY_MINERAL, 200.0f));
}

// --- Probabilistic confidence accumulation ---

TEST_CASE("Confidence accumulates probabilistically", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);
    float afterXRF = s.elementConfidence[ResourceType::Fe];

    engine.ApplyTool(s, AnalysisTool::LIBS_PULSE, 200.0f);
    float afterLIBS = s.elementConfidence[ResourceType::Fe];

    REQUIRE(afterLIBS > afterXRF);

    // Verify probabilistic formula: 1 - (1-c1)(1-c2)
    // afterLIBS should be > simple sum but < 1.0
    REQUIRE(afterLIBS < afterXRF + 0.40f);
    REQUIRE(afterLIBS <= 1.0f);
}

TEST_CASE("Re-applying same tool has diminishing returns", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(1);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);
    float first = s.elementConfidence[ResourceType::Fe];

    float prevConf = first;
    engine.ApplyTool(s, AnalysisTool::XRF, 200.0f);
    float gainSecond = s.elementConfidence[ResourceType::Fe] - prevConf;

    prevConf = s.elementConfidence[ResourceType::Fe];
    engine.ApplyTool(s, AnalysisTool::XRF, 300.0f);
    float gainThird = s.elementConfidence[ResourceType::Fe] - prevConf;

    REQUIRE(gainSecond < first);
    REQUIRE(gainThird < gainSecond);
}

TEST_CASE("Tool diversity gives broader coverage than repeating", "[lab]")
{
    Sample s1 = MakeFullCompositionSample();
    Sample s2 = MakeFullCompositionSample();
    LabEngine engine(2);

    // s1: XRF twice — only covers heavy elements
    engine.ApplyTool(s1, AnalysisTool::XRF, 100.0f);
    engine.ApplyTool(s1, AnalysisTool::XRF, 200.0f);

    // s2: XRF + LIBS — covers heavy AND light elements
    engine.ApplyTool(s2, AnalysisTool::XRF, 100.0f);
    engine.ApplyTool(s2, AnalysisTool::LIBS_PULSE, 200.0f);

    // XRF-only misses light elements entirely
    REQUIRE(s1.elementConfidence.find(ResourceType::H2) == s1.elementConfidence.end());
    REQUIRE(s1.elementConfidence.find(ResourceType::O2) == s1.elementConfidence.end());

    // Diverse toolset covers light elements
    REQUIRE(s2.elementConfidence[ResourceType::H2] > 0.0f);
    REQUIRE(s2.elementConfidence[ResourceType::O2] > 0.0f);
}

// --- Crystal glow updates ---

TEST_CASE("Crystal glow updates after tool application", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    s.visual.glowLevel = 0;
    LabEngine engine(2);

    engine.ApplyTool(s, AnalysisTool::XRF, 100.0f);
    engine.ApplyTool(s, AnalysisTool::LIBS_PULSE, 200.0f);

    int expectedGlow = GetGlowLevel(s.GetAggregateConfidence());
    REQUIRE(s.visual.glowLevel == expectedGlow);
}

// --- Analysis history ---

TEST_CASE("Analysis history records each tool application", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(2);

    REQUIRE(s.analysisHistory.empty());

    engine.ApplyTool(s, AnalysisTool::VISUAL_INSPECTION, 100.0f);
    REQUIRE(s.analysisHistory.size() == 1);
    REQUIRE(s.analysisHistory[0].tool == AnalysisTool::VISUAL_INSPECTION);
    REQUIRE_THAT(s.analysisHistory[0].timestamp, Catch::Matchers::WithinAbs(100.0f, 0.01f));

    engine.ApplyTool(s, AnalysisTool::XRF, 200.0f);
    REQUIRE(s.analysisHistory.size() == 2);
    REQUIRE(s.analysisHistory[1].tool == AnalysisTool::XRF);
}

// --- Presets ---

TEST_CASE("Presets are defined correctly", "[lab]")
{
    const auto& presets = LabEngine::GetPresets();
    REQUIRE(presets.size() == 4);
    REQUIRE(presets[0].name == "Quick Survey");
    REQUIRE(presets[1].name == "Structural");
    REQUIRE(presets[2].name == "Life Support");
    REQUIRE(presets[3].name == "Strategic");
}

TEST_CASE("Quick Survey preset applies visual inspection", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(0);

    REQUIRE(engine.ApplyPreset(s, 0, 100.0f));
    REQUIRE(s.analysisHistory.size() == 1);
    REQUIRE(s.analysisHistory[0].tool == AnalysisTool::VISUAL_INSPECTION);
}

TEST_CASE("Structural preset applies magnetic separation + XRF", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(1);

    REQUIRE(engine.ApplyPreset(s, 1, 100.0f));
    REQUIRE(s.separationApplied == SeparationMethod::MAGNETIC);
    REQUIRE(s.analysisHistory.size() == 1);
    REQUIRE(s.analysisHistory[0].tool == AnalysisTool::XRF);

    REQUIRE(s.elementConfidence[ResourceType::Fe] > 0.0f);
    REQUIRE(s.elementConfidence[ResourceType::Ti] > 0.0f);
}

TEST_CASE("Preset tier gating", "[lab]")
{
    Sample s = MakeFullCompositionSample();

    LabEngine t0(0);
    REQUIRE(t0.CanApplyPreset(s, 0));
    REQUIRE_FALSE(t0.CanApplyPreset(s, 1));

    LabEngine t1(1);
    REQUIRE(t1.CanApplyPreset(s, 1));
    REQUIRE_FALSE(t1.CanApplyPreset(s, 2));

    LabEngine t2(2);
    REQUIRE(t2.CanApplyPreset(s, 2));
    REQUIRE_FALSE(t2.CanApplyPreset(s, 3));

    LabEngine t3(3);
    REQUIRE(t3.CanApplyPreset(s, 3));
}

TEST_CASE("Preset rejects invalid index", "[lab]")
{
    Sample s = MakeFullCompositionSample();
    LabEngine engine(3);

    REQUIRE_FALSE(engine.CanApplyPreset(s, -1));
    REQUIRE_FALSE(engine.CanApplyPreset(s, 99));
}

// --- Element classification helpers ---

TEST_CASE("IsHeavyElement identifies correctly", "[lab]")
{
    REQUIRE(LabEngine::IsHeavyElement(ResourceType::Fe));
    REQUIRE(LabEngine::IsHeavyElement(ResourceType::Si));
    REQUIRE(LabEngine::IsHeavyElement(ResourceType::Ti));
    REQUIRE(LabEngine::IsHeavyElement(ResourceType::Al));
    REQUIRE(LabEngine::IsHeavyElement(ResourceType::Ca));
    REQUIRE_FALSE(LabEngine::IsHeavyElement(ResourceType::H2));
    REQUIRE_FALSE(LabEngine::IsHeavyElement(ResourceType::O2));
    REQUIRE_FALSE(LabEngine::IsHeavyElement(ResourceType::C));
}

TEST_CASE("IsLightElement identifies correctly", "[lab]")
{
    REQUIRE(LabEngine::IsLightElement(ResourceType::H2));
    REQUIRE(LabEngine::IsLightElement(ResourceType::O2));
    REQUIRE(LabEngine::IsLightElement(ResourceType::C));
    REQUIRE(LabEngine::IsLightElement(ResourceType::WATER));
    REQUIRE_FALSE(LabEngine::IsLightElement(ResourceType::Fe));
}

TEST_CASE("IsFerromagnetic identifies correctly", "[lab]")
{
    REQUIRE(LabEngine::IsFerromagnetic(ResourceType::Fe));
    REQUIRE(LabEngine::IsFerromagnetic(ResourceType::Ti));
    REQUIRE_FALSE(LabEngine::IsFerromagnetic(ResourceType::Si));
    REQUIRE_FALSE(LabEngine::IsFerromagnetic(ResourceType::H2));
}

// --- Energy costs ---

TEST_CASE("Tool costs scale with capability", "[lab]")
{
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::VISUAL_INSPECTION) == LAB_TOOL_COST_VISUAL);
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::XRF) == LAB_TOOL_COST_XRF);
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::LIBS_PULSE) == LAB_TOOL_COST_LIBS);
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::FIRE_ASSAY) == LAB_TOOL_COST_FIRE_ASSAY);

    REQUIRE(LabEngine::GetToolCost(AnalysisTool::VISUAL_INSPECTION)
          < LabEngine::GetToolCost(AnalysisTool::XRF));
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::XRF)
          < LabEngine::GetToolCost(AnalysisTool::LIBS_PULSE));
    REQUIRE(LabEngine::GetToolCost(AnalysisTool::LIBS_PULSE)
          < LabEngine::GetToolCost(AnalysisTool::FIRE_ASSAY));
}

TEST_CASE("Separation costs are defined", "[lab]")
{
    REQUIRE(LabEngine::GetSeparationCost(SeparationMethod::MAGNETIC) == LAB_SEPARATION_COST_MAGNETIC);
    REQUIRE(LabEngine::GetSeparationCost(SeparationMethod::HEAVY_MINERAL) == LAB_SEPARATION_COST_HEAVY);
    REQUIRE(LabEngine::GetSeparationCost(SeparationMethod::VOLATILE_EXTRACTION) == LAB_SEPARATION_COST_VOLATILE);
    REQUIRE(LabEngine::GetSeparationCost(SeparationMethod::NONE) == 0.0f);
}
