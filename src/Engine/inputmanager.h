#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "raylib.h"
#include "raymath.h"
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

private:
    double lastClickTime;
    double lastDoubleClickTime;
    Vector2 lastClickPosition;
    Vector2 dragStart;
    bool isDragging;
};

#endif // INPUT_MANAGER_H
