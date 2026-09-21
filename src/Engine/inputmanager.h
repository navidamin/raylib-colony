#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "raylib.h"
#include "raymath.h"
#include "survey_input.h"
#include <iostream>

class InputManager {
public:
    InputManager();
    ~InputManager();

    void Update();

    bool IsDoubleClick();
    bool IsInfoKeyPressed() const { return IsKeyDown(KEY_TAB); }
    bool IsCommandPressed() const { return IsKeyDown(KEY_LEFT_CONTROL); }

    Vector2 GetMousePosition() const { return ::GetMousePosition(); }
    Vector2 GetMouseDelta() const { return ::GetMouseDelta(); }
    bool IsMouseDragging() const { return isDragging; }
    Vector2 GetDragStart() const { return dragStart; }

    void StartDragging();
    void StopDragging();

    // The survey descent's input for this frame: the pointer, a click
    // (a release whose press did not travel -- a drag turns the globe,
    // it does not claim), Esc or right-click as escape, the wheel.
    SurveyInput Survey(float dt) const;
    bool SurveyClick() const { return pressGesture.Click(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)); }
    bool SurveyDragged() const { return pressGesture.Dragged(); }

private:
    SurveyPressGesture pressGesture;
    double lastClickTime;
    double lastDoubleClickTime;
    Vector2 lastClickPosition;
    Vector2 dragStart;
    bool isDragging;
};

#endif // INPUT_MANAGER_H
