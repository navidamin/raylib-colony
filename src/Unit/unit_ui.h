// unit_ui.h
#ifndef UNIT_UI_H
#define UNIT_UI_H

#include <string>
#include <vector>
#include <map>
#include "raylib.h"

// UI-specific structures that will be used in Unit class


struct UIMessage {
    std::string text;
    float opacity;
    float timeRemaining;
};

// These will be private members of Unit class
#define UNIT_UI_PRIVATE_MEMBERS \
    UIMessage currentMessage; \
    bool isInModuleView; \
    int selectedModuleIndex; \
    float lastClickTime; \
    int lastClickedModule; \
    Rectangle hoveredButton; \
    bool showingStats;

// These will be private UI-related methods of Unit class
#define UNIT_UI_PRIVATE_METHODS \
    void DrawTopBar(); \
    void DrawBottomBar(); \
    void DrawModuleList(); \
    void DrawModuleDetails(); \
    void DrawResourcePanel(); \
    void DrawResourceStats(int startX, int startY, int panelWidth); \
    void DrawControlPanel(); \
    void ShowMessage(const std::string& text); \
    void UpdateMessage(float deltaTime); \
    bool IsModuleButtonClicked(Rectangle buttonRect); \
    void HandleModuleActivation(int moduleIndex); \
    bool CanUpgradeModule(const UnitModule& module); \
    bool CanBuildModule(const UnitModule& module); \
    void BuildModule(int moduleIndex); \



#endif // UNIT_UI_H
