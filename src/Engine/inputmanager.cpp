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

    bool isDoubleClick = (currentTime - lastClickTime <= 0.5) &&   // No more than 500ms between clicks
                        (currentTime - lastClickTime > 0.1) &&      // At least 100ms between clicks
                        (Vector2Distance(lastClickPosition, currentPosition) <= 10);

    // Only update the last click time if this wasn't a double click
    if (!isDoubleClick) {
        lastClickTime = currentTime;
        lastClickPosition = currentPosition;
    }

    if (isDoubleClick) {
        std::cout << "Double click detected!\n";
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
