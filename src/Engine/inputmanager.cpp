#include "inputmanager.h"

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
    // Update mouse dragging state
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        StartDragging();
    }

    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
        StopDragging();
    }
}

bool InputManager::IsDoubleClick() {
    double currentTime = GetTime();
    Vector2 currentPosition = GetMousePosition();

    double timeDiff = currentTime - lastClickTime;
    float distance = Vector2Distance(lastClickPosition, currentPosition);

    std::cout << "IsDoubleClick check: timeDiff=" << timeDiff << "s, distance=" << distance << "px" << std::endl;

    bool isDoubleClick = (timeDiff <= 0.6) &&   // No more than 600ms between clicks (more lenient)
                        (timeDiff > 0.05) &&     // At least 50ms between clicks (faster allows trackpad)
                        (distance <= 30);        // 30px tolerance for trackpad movement

    // Only update the last click time if this wasn't a double click
    if (!isDoubleClick) {
        lastClickTime = currentTime;
        lastClickPosition = currentPosition;
        std::cout << "  -> NOT a double-click. Updated lastClickTime." << std::endl;
    } else {
        std::cout << "  -> DOUBLE CLICK DETECTED!" << std::endl;
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
