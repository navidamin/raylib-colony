#include "inputmanager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Emscripten 3.1.64's library_glfw.js installs, in glfwInit, a
// Browser.calculateMouseCoords that scales page coordinates by
// canvas.clientWidth / rect.width -- two CSS measures of the same box, so
// the factor is 1 and GLFW reports the mouse in CSS pixels. raylib's
// MouseMoveCallback stores that as the screen position. Touch input
// never sees it: raylib's own touch callback scales targetX by
// screen / CSS size, which is why phones were fine and a desktop mouse
// on a CSS-fitted canvas landed past the cursor by the fit (1.25x on a
// 1600 px box for the 1280 px frame). This puts the pre-3.1.5x scaling
// back: framebuffer over CSS box. Where the two are equal (lunar_map
// sizes its framebuffer to the viewport) it changes nothing.
// EM_ASM is a variadic macro: a comma outside parentheses splits the JS
// into extra arguments (braces do not protect it), so the body has none.
void InputManager::FixWebPointerUnits() {
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (typeof Browser === 'undefined' || !Module['canvas']) return;
        Browser.calculateMouseCoords = function(pageX, pageY) {
            var c = Module['canvas'];
            var rect = c.getBoundingClientRect();
            var sx = (typeof window.scrollX != 'undefined') ? window.scrollX : window.pageXOffset;
            var sy = (typeof window.scrollY != 'undefined') ? window.scrollY : window.pageYOffset;
            var x = pageX - (sx + rect.left);
            var y = pageY - (sy + rect.top);
            if (rect.width > 0 && rect.height > 0) {
                x = x * (c.width / rect.width);
                y = y * (c.height / rect.height);
            }
            var out = {};
            out.x = x;
            out.y = y;
            return out;
        };
    });
#endif
}

InputManager::InputManager()
    : lastClickTime(0),
      lastDoubleClickTime(0),
      lastClickPosition({0, 0}),
      isDragging(false)
{
}

InputManager::~InputManager() {
}

void InputManager::Update() {
    // A left press that travels is a drag, not a click.
    pressGesture.Update(::GetMousePosition(),
                        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                        IsMouseButtonReleased(MOUSE_BUTTON_LEFT));

    // Update mouse dragging state
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        StartDragging();
    }

    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
        StopDragging();
    }
}

SurveyInput InputManager::Survey(float dt) const {
    SurveyInput in;
    in.pointer = ::GetMousePosition();
    in.click = SurveyClick();
    in.escape = IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.wheel = GetMouseWheelMove();
    in.dt = dt;
    return in;
}

bool InputManager::IsDoubleClick() {
    double currentTime = GetTime();
    Vector2 currentPosition = GetMousePosition();

    double timeDiff = currentTime - lastClickTime;
    float distance = Vector2Distance(lastClickPosition, currentPosition);

    // std::cout << "IsDoubleClick check: timeDiff=" << timeDiff << "s, distance=" << distance << "px" << std::endl;

    bool isDoubleClick = (timeDiff <= 0.6) &&   // No more than 600ms between clicks (more lenient)
                        (timeDiff > 0.05) &&     // At least 50ms between clicks (faster allows trackpad)
                        (distance <= 30);        // 30px tolerance for trackpad movement

    // Only update the last click time if this wasn't a double click
    if (!isDoubleClick) {
        lastClickTime = currentTime;
        lastClickPosition = currentPosition;
        // std::cout << "  -> NOT a double-click. Updated lastClickTime." << std::endl;
    } else {
        // std::cout << "  -> DOUBLE CLICK DETECTED!" << std::endl;
    }

    return isDoubleClick;
}

void InputManager::StartDragging() {
    dragStart = GetMousePosition();
    isDragging = true;
}

void InputManager::StopDragging() {
    isDragging = false;
}
