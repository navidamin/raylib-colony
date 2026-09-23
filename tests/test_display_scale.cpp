#include <catch2/catch_test_macros.hpp>
#include "display_scale.h"

// The buffer is a whole number of layouts, big enough that the display only
// ever shrinks it. The policy is the one thing here that can be wrong without
// a window, and a wrong answer is either a soft picture or a wasted buffer.

TEST_CASE("DisplayScale: a display no bigger than the layout draws 1:1", "[display_scale]")
{
    CHECK(DisplayScale_StepForFit(0.3f) == 1);    // a phone
    CHECK(DisplayScale_StepForFit(1.0f) == 1);    // 1280x720 exactly
    // a few percent over is not worth four times the pixels
    CHECK(DisplayScale_StepForFit(1.05f) == 1);
}

TEST_CASE("DisplayScale: the common desktops get 2x", "[display_scale]")
{
    // 1920x1080 (fit 1.5) and 1920x1200 with toolbars (~1.28) or fullscreen
    // (1.5) -- the display that used to flip between small and soft
    CHECK(DisplayScale_StepForFit(1920.0f / 1280.0f) == 2);
    CHECK(DisplayScale_StepForFit(920.0f / 720.0f) == 2);
    CHECK(DisplayScale_StepForFit(2.0f) == 2);    // 2560x1440
    CHECK(DisplayScale_StepForFit(2.05f) == 2);
}

TEST_CASE("DisplayScale: 4K gets 3x, and nothing gets more", "[display_scale]")
{
    CHECK(DisplayScale_StepForFit(2.8f) == 3);
    CHECK(DisplayScale_StepForFit(3.0f) == 3);
    CHECK(DisplayScale_StepForFit(6.0f) == 3);    // an 8K panel is capped
}

TEST_CASE("DisplayScale: never a buffer smaller than the display", "[display_scale]")
{
    // except inside the 5% dead band, which is the point of it
    for (float fit = 1.06f; fit <= 3.0f; fit += 0.01f)
        CHECK(static_cast<float>(DisplayScale_StepForFit(fit)) >= fit - 0.05f);
}

TEST_CASE("DisplayScale: without Init it is the plain 1x layout", "[display_scale]")
{
    // every harness that never calls Init must draw exactly as before
    CHECK(DisplayScale_Step() == 1);
    CHECK(DisplayScale_LogicalW() == 1280);
    CHECK(DisplayScale_LogicalH() == 720);
    CHECK(DisplayScale_BufferW() == 1280);
}
