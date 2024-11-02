#include "unit.h"

void Unit::DrawTopBar() {
    const int barHeight = 60;
    DrawRectangle(0, 0, GetScreenWidth(), barHeight, DARKGRAY);

    // Draw title
    //DrawText(("Unit Type: " + unit_type).c_str(), 20, 20, 20, WHITE);

    const UnitModule& module = modules[selectedModuleIndex];

    const char* titleText;
    if (isInModuleView) {
        titleText = TextFormat("Module Control Panel");

    } else {
        titleText = TextFormat("Unit Control Panel");
    }

    DrawText(titleText, GetScreenWidth()/2 - MeasureText(titleText,26)/2, 20, 28, BLACK);

    // Draw day number
    const char* dayText = TextFormat("Day %d", timeManager.GetCurrentDay());
    DrawText(dayText, GetScreenWidth() - MeasureText(dayText, 20) - 20, 20, 20, WHITE);
}

void Unit::DrawBottomBar() {
    const int barHeight = 40;
    const int startY = GetScreenHeight() - barHeight;

    DrawRectangle(0, startY, GetScreenWidth(), barHeight, DARKGRAY);

    // Draw message with fade effect
    if (currentMessage.opacity > 0) {
        Color msgColor = WHITE;
        msgColor.a = static_cast<unsigned char>(255 * currentMessage.opacity);
        DrawText(currentMessage.text.c_str(),
                20, startY + 10, 20, msgColor);
    }
}



void Unit::DrawResourcePanel() {
    const int leftPanelWidth = 300;
    const int rightPanelWidth = 300;
    const int middlePanelWidth = GetScreenWidth() - leftPanelWidth - rightPanelWidth;
    const int topMargin = 80;

    // Calculate middle panel start X position
    const int middlePanelX = leftPanelWidth;

    // Optional: Draw a subtle background for the middle panel
    DrawRectangle(middlePanelX, topMargin,
                 middlePanelWidth, GetScreenHeight() - topMargin - 40,  // -40 for bottom bar
                 Fade(RAYWHITE, 0.5f));

    // Call modified DrawResourceStats with position parameters
    DrawResourceStats(middlePanelX, topMargin, middlePanelWidth);
}

void Unit::DrawResourceStats(int startX, int startY, int panelWidth) {
    float rowHeight = 22.0f;
    float colWidth = std::min(150.0f, static_cast<float>(panelWidth) / 3);  // Ensure columns fit
    int fontSize = 20;
    Color darkGreen = {34, 139, 34, 255};  // Forest Green

    const int padding = 20;
    //startX += padding;  // Add padding from panel edge
    const int contentWidth = panelWidth - (padding * 2);

    // First draw unit and module info
    DrawText(TextFormat("Unit Type: %s", unit_type.c_str()),
             startX, startY, fontSize + 5, BLACK);
    startY += rowHeight * 1.5f;

    // List of all resource types with their names
    std::vector<std::pair<ResourceType, const char*>> resources = {
        {ResourceType::ENERGY, "Energy"},
        {ResourceType::H2, "Hydrogen"},
        {ResourceType::O2, "Oxygen"},
        {ResourceType::C, "Carbon"},
        {ResourceType::Fe, "Iron"},
        {ResourceType::Si, "Silicon"},
        {ResourceType::WATER, "Water"},
        {ResourceType::FOOD, "Food"},
        {ResourceType::SCIENCE, "Science"},
        {ResourceType::MANPOWER, "Manpower"}
    };

    if (activeModule) {
        DrawText(TextFormat("Active Module: %s (Level %d)",
                activeModule->name.c_str(),
                activeModule->level),
                startX, startY, fontSize, BLACK);
        startY += rowHeight;

        DrawText(TextFormat("Status: %s", status.c_str()),
                startX, startY, fontSize,
                status == "active" ? darkGreen : GRAY);
        startY += rowHeight;

        DrawText(TextFormat("Efficiency: %.1f%%", activeModule->efficiency * 100.0f),
                startX, startY, fontSize, BLACK);
        startY += rowHeight * 2;  // Extra space before rates

        // Column headers for rates
        DrawText("Resource", startX, startY, fontSize, BLACK);
        DrawText("Production", startX + colWidth, startY, fontSize, BLACK);
        DrawText("Consumption", startX + colWidth * 2, startY, fontSize, BLACK);

        startY += rowHeight;



        // Draw rates only for resources that have production or consumption
        for (const auto& [type, name] : resources) {
            bool hasActivity = false;
            float prodRate = activeModule->productionRates[type];
            float consRate = activeModule->consumptionRates[type];

            if (prodRate > 0 || consRate > 0) {
                hasActivity = true;
                DrawText(name, startX, startY, fontSize, BLACK);

                // Production rate
                if (prodRate > 0) {
                    DrawText(TextFormat("+%.2f/s", prodRate),
                            startX + colWidth, startY, fontSize, darkGreen);
                } else {
                    DrawText("-", startX + colWidth, startY, fontSize, GRAY);
                }

                // Consumption rate
                if (consRate > 0) {
                    DrawText(TextFormat("-%.2f/s", consRate),
                            startX + colWidth * 2, startY, fontSize, RED);
                } else {
                    DrawText("-", startX + colWidth * 2, startY, fontSize, GRAY);
                }

                if (hasActivity) {
                    startY += rowHeight;
                }
            }
        }
    } else {
        DrawText("No active module", startX, startY, fontSize, GRAY);
    }

    // Add Debug Storage Info section
    startY += rowHeight * 2;  // Add some space before debug section

    DrawText("Storage Info", startX, startY, fontSize, BLACK);
    startY += rowHeight * 1.2f;

    bool hasStoredResources = false;
    // Reuse resources vector from above

    // Check for any stored resources
    for (const auto& [type, name] : resources) {
        float stored = GetStoredResource(type);
        if (stored > 0) {
            hasStoredResources = true;
            DrawText(TextFormat("%s: %.2f", name, stored),
                    startX, startY, fontSize, BLACK);
            startY += rowHeight;
        }
    }

    // If no resources are stored, show empty message
    if (!hasStoredResources) {
        DrawText("Unit storage is empty", startX, startY, fontSize, GRAY);
    }
}

void Unit::DrawControlPanel() {
    const int rightPanelWidth = 300;
    const int topMargin = 60;
    const int padding = 10;
    const int elementHeight = 30;
    const int spaceBetween = 5;

    if (isInModuleView) {
        // Existing module view control panel code
        if (selectedModuleIndex < 0 || selectedModuleIndex >= modules.size()) {
            return;
        }

        const UnitModule& module = modules[selectedModuleIndex];

        // Draw build button if not built
        if (!module.isBuilt) {
            Rectangle buildButton = {
                static_cast<float>(GetScreenWidth() - rightPanelWidth + padding),
                static_cast<float>(topMargin + padding),
                static_cast<float>(rightPanelWidth - padding * 2),
                40
            };

            bool canBuild = CanBuildModule(module);  // You'll need to implement this
            Color buttonColor = canBuild ? BLUE : GRAY;

            DrawRectangleRec(buildButton, buttonColor);
            const char* buildText = "Build Module";
            int textWidth = MeasureText(buildText, 20);
            DrawText(buildText,
                    buildButton.x + (buildButton.width - textWidth) / 2,
                    buildButton.y + 10,
                    20, WHITE);

            if (canBuild && IsModuleButtonClicked(buildButton)) {
                BuildModule(selectedModuleIndex);  // You'll need to implement this
            }
        }
        // Draw upgrade button if built and not max level
        else if (module.level < 5) {
            Rectangle upgradeButton = {
                static_cast<float>(GetScreenWidth() - rightPanelWidth + padding),
                static_cast<float>(topMargin + padding),
                static_cast<float>(rightPanelWidth - padding * 2),
                40
            };

            bool canUpgrade = CanUpgradeModule(module);
            bool isHovered = CheckCollisionPointRec(GetMousePosition(), upgradeButton);

            // Apply hover effect to button color
            Color buttonColor = canUpgrade ?
                (isHovered ? ColorBrightness(BLUE, 0.2f) : BLUE) :
                (isHovered ? ColorBrightness(GRAY, 0.2f) : GRAY);

            DrawRectangleRec(upgradeButton, buttonColor);
            const char* upgradeText = TextFormat("Upgrade to Level %d", module.level + 1);
            int textWidth = MeasureText(upgradeText, 20);
            DrawText(upgradeText,
                    upgradeButton.x + (upgradeButton.width - textWidth) / 2,
                    upgradeButton.y + 10,
                    20, WHITE);

            if (canUpgrade && IsModuleButtonClicked(upgradeButton)) {
                UpgradeModule(selectedModuleIndex);
            }
        }

        // Draw activate/deactivate button if built
        if (module.isBuilt) {
            Rectangle toggleButton = {
                static_cast<float>(GetScreenWidth() - rightPanelWidth + padding),
                static_cast<float>(topMargin + padding + 50),
                static_cast<float>(rightPanelWidth - padding * 2),
                40
            };

            // Draw button with hover effect
            Color buttonColor = module.isActive ? RED : GREEN;
            if (CheckCollisionPointRec(GetMousePosition(), toggleButton)) {
                // Lighten the color when hovering
                buttonColor = module.isActive ?
                    ColorBrightness(RED, 0.2f) :
                    ColorBrightness(GREEN, 0.2f);
            }

            DrawRectangleRec(toggleButton, buttonColor);
            const char* toggleText = module.isActive ? "Deactivate" : "Activate";
            int textWidth = MeasureText(toggleText, 20);
            DrawText(toggleText,
                    toggleButton.x + (toggleButton.width - textWidth) / 2,
                    toggleButton.y + 10,
                    20, WHITE);

            // Only handle activation on actual click
            if (IsModuleButtonClicked(toggleButton)) {
                HandleModuleActivation(selectedModuleIndex);
            }
        }
    }
    else {
        // Unit view control panel - Production Rate Controls
        float yPos = topMargin + padding;
        const int labelWidth = 100;
        const int controlWidth = rightPanelWidth - labelWidth - padding * 3;
        const Vector2 panelPos = {
            static_cast<float>(GetScreenWidth() - rightPanelWidth),
            static_cast<float>(topMargin)
        };

        // Draw panel background
        DrawRectangle(panelPos.x, panelPos.y,
                     rightPanelWidth, GetScreenHeight() - topMargin - 40,
                     Fade(LIGHTGRAY, 0.2f));

        // Title
        DrawText("Production Controls",
                panelPos.x + padding, yPos,
                20, BLACK);
        yPos += elementHeight + spaceBetween * 2;

        // Draw controls for each resource that has a max production rate

        if (activeModule) {
            for (const auto& [resource, maxRate] : activeModule->maxProductionRates) {
                std::string resourceName;
                try {
                    resourceName = ResourceUtils::GetResourceName(resource);
                } catch (...) {
                    resourceName = "Unknown";
                    continue;
                }

                // Draw resource name
                DrawText(resourceName.c_str(),
                        panelPos.x + padding,
                        yPos + 5,
                        20, BLACK);

                // Get current production rate for this resource
                float currentRate = activeModule->productionRates[resource];

                // Draw rate control buttons (0-4)
                for (int rate = 0; rate <= 4; rate++) {
                    float buttonWidth = (controlWidth - spaceBetween * 4) / 5.0f;
                    Rectangle buttonRect = {
                        panelPos.x + labelWidth + padding + (buttonWidth + spaceBetween) * rate,
                        yPos,
                        buttonWidth,
                        elementHeight
                    };

                    // Calculate what this button's rate should be
                    float buttonRate = (rate == 0) ? 0.0f : (maxRate * rate / 4.0f);

                    // Debug output
                    /*
                    std::cout << resourceName << " Button " << rate << ":\n";
                    std::cout << "  Button Rate: " << buttonRate << "\n";
                    std::cout << "  Current Rate: " << currentRate << "\n";
                    std::cout << "  Max Rate: " << maxRate << "\n";
                    */

                    // Calculate which button should be active based on current production rate
                    bool isCurrentRate = false;
                    if (rate == 0 && currentRate == 0) {
                        isCurrentRate = true;
                    } else if (rate > 0) {
                        // Calculate the rate ranges for this button
                        float lowerBound = maxRate * (rate - 0.5f) / 4.0f;
                        float upperBound = maxRate * (rate + 0.5f) / 4.0f;

                        // Special case for max rate button
                        if (rate == 4) {
                            isCurrentRate = (currentRate >= lowerBound);
                        } else {
                            isCurrentRate = (currentRate >= lowerBound && currentRate < upperBound);
                        }
                    }

                    // Draw button with appropriate color
                    Color buttonColor = isCurrentRate ? GREEN : GRAY;
                    if (CheckCollisionPointRec(GetMousePosition(), buttonRect)) {
                        buttonColor = Fade(buttonColor, 0.7f);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            activeModule->productionRates[resource] = buttonRate;
                            ShowMessage(TextFormat("%s production rate set to %.1f",
                                                 resourceName.c_str(), buttonRate));
                            CalculateConsumption();  // Recalculate consumption when production changes
                        }
                    }

                    DrawRectangleRec(buttonRect, buttonColor);

                    // Draw rate value or "Off" for 0
                    const char* rateText = (rate == 0) ? "Off" : TextFormat("%d", rate);
                    int textWidth = MeasureText(rateText, 16);
                    DrawText(rateText,
                            buttonRect.x + (buttonRect.width - textWidth) / 2,
                            buttonRect.y + 7,
                            16, WHITE);
                }

                yPos += elementHeight + spaceBetween * 2;
            }
        }
    }
}


void Unit::DrawModuleList() {
    const int leftPanelWidth = 270;
    const int topMargin = 80;
    const int buttonHeight = 60;
    const int padding = 10;

    for (size_t i = 0; i < modules.size(); i++) {
        const UnitModule& module = modules[i];
        Rectangle buttonRect = {
            static_cast<float>(padding),
            static_cast<float>(topMargin + (i * (buttonHeight + padding))),
            static_cast<float>(leftPanelWidth - padding * 2),
            static_cast<float>(buttonHeight)
        };

        // Check for hover
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), buttonRect);

        // Determine button color based on state
        Color buttonColor;
        Color textColor;
        if (!module.isBuilt) {
            buttonColor = isHovered ? Fade(WHITE, 0.7f) : WHITE;
            textColor = BLACK;
        } else if (module.isActive) {
            buttonColor = isHovered ? Fade(GREEN, 0.7f) : GREEN;
            textColor = DARKGREEN;
        } else {
            buttonColor = isHovered ? Fade(GRAY, 0.7f) : GRAY;
            textColor = BLACK;
        }

        // Draw button with hover effect
        DrawRectangleRec(buttonRect, buttonColor);
        DrawRectangleLinesEx(buttonRect, 1, DARKGRAY);

        // Draw module name and level if built
        std::string displayText = module.name;
        if (module.isBuilt) {
            displayText += " (Lvl " + std::to_string(module.level) + ")";
        }

        const char* text = displayText.c_str();
        int textWidth = MeasureText(text, 20);
        Vector2 textPos = {
            buttonRect.x + (buttonRect.width - textWidth) / 2,
            buttonRect.y + (buttonRect.height - 20) / 2
        };
        DrawText(text, textPos.x, textPos.y, 20, textColor);

        // Handle click and double-click
        if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float currentTime = GetTime();
            // click detected
            selectedModuleIndex = i;
            isInModuleView = true;
            ShowMessage("Switched to module view");

            lastClickTime = currentTime;
            lastClickedModule = i;
        }
    }
}

void Unit::DrawModuleDetails() {
    if (selectedModuleIndex < 0 || selectedModuleIndex >= modules.size()) {
        return;
    }

    const UnitModule& module = modules[selectedModuleIndex];
    const int screenHeight = GetScreenHeight();
    const int bottomMargin = 80;  // Increased to make room for stats button
    const int leftMargin = 300;
    const int topMargin = 80;
    const int padding = 20;
    const int lineSpacing = 22;  // Reduced spacing between lines
    int yPos = topMargin;

    // Back button (keep at fixed position)
    Rectangle backButton = {
        10,  // Left margin
        static_cast<float>(screenHeight - bottomMargin + 10),  // Same Y as stats button
        static_cast<float>(leftMargin - 20),  // Width of left panel minus margins
        30
    };
    bool isBackHovered = CheckCollisionPointRec(GetMousePosition(), backButton);
    DrawRectangleRec(backButton, isBackHovered ? Fade(GRAY, 0.7f) : GRAY);

    const char* backText = "< Back to Unit Panel";
    int textWidth = MeasureText(backText, 20);
    DrawText(backText,
             backButton.x + (backButton.width - textWidth) / 2,
             backButton.y + 5,
             20, WHITE);

    if (isBackHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        isInModuleView = false;
        showingStats = false;  // Reset stats view when leaving module view
        ShowMessage("Returned to unit view");
        return;
    }

    // Helper function to safely draw text
    auto safeDrawText = [screenHeight, bottomMargin](const char* text, int x, int y, int fontSize, Color color) {
        if (y + fontSize < screenHeight - bottomMargin) {
            DrawText(text, x, y, fontSize, color);
            return true;
        }
        return false;
    };

    if (!showingStats) {
        // Draw module name and description
        safeDrawText(module.name.c_str(), leftMargin, yPos, 30, BLACK);
        yPos += 35;  // Reduced spacing after title

        // Draw description using the new multiline function
        if (!module.description.empty()) {
            DrawMultilineText(module.description, leftMargin, yPos, 20, DARKGRAY);
            yPos += (20 + 5) * (std::count(module.description.begin(), module.description.end(), '\n') + 1);
        }

        // Draw current level info
        if (module.isBuilt) {
            safeDrawText(TextFormat("Current Level: %d", module.level),
                        leftMargin, yPos, 20, BLACK);
            yPos += lineSpacing;

            // Show next level info if not max level
            if (module.level < 5) {
                try {
                    auto nextLevel = module.level + 1;
                    auto costIter = module.upgradeCosts.find(nextLevel);
                    if (costIter != module.upgradeCosts.end()) {
                        safeDrawText("Upgrade Requirements:", leftMargin, yPos, 20, BLACK);
                        yPos += lineSpacing;

                        for (const auto& [resource, amount] : costIter->second) {
                            std::string resourceName = ResourceUtils::GetResourceName(resource);
                            safeDrawText(TextFormat("%s: %.1f", resourceName.c_str(), amount),
                                       leftMargin + padding, yPos, 20, DARKGRAY);
                            yPos += lineSpacing;
                        }
                    }

                    // Show enhancements
                    auto enhIter = module.enhancements.find(nextLevel);
                    if (enhIter != module.enhancements.end()) {
                        yPos += lineSpacing/2;  // Small gap between sections
                        safeDrawText("Next Level Enhancements:", leftMargin, yPos, 20, BLACK);
                        yPos += lineSpacing;

                        for (const auto& [stat, value] : enhIter->second) {
                            safeDrawText(TextFormat("%s: +%.1f%%", stat.c_str(), (value - 1.0f) * 100),
                                       leftMargin + padding, yPos, 20, DARKGRAY);
                            yPos += lineSpacing;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "Error accessing upgrade info: " << e.what() << std::endl;
                }
            } else {
                safeDrawText("Maximum Level Reached", leftMargin, yPos, 20, DARKGRAY);
            }
        } else {
            safeDrawText("Module needs to be built first", leftMargin, yPos, 20, DARKGRAY);
        }

    } else {
        // Stats View
        safeDrawText(TextFormat("%s - Statistics", module.name.c_str()),
                    leftMargin, yPos, 30, BLACK);
        yPos += 35;

        if (module.isBuilt) {
            // Efficiency
            safeDrawText(TextFormat("Current Efficiency: %.1f%%",
                                  module.efficiency * 100),
                        leftMargin, yPos, 20, BLACK);
            yPos += lineSpacing * 1.5f;

            // Production Rates - with safety checks
            bool hasProduction = false;
            for (const auto& [resource, rate] : module.productionRates) {
                if (rate > 0) {
                    hasProduction = true;
                    break;
                }
            }

            if (hasProduction) {
                safeDrawText("Production Rates:", leftMargin, yPos, 20, BLACK);
                yPos += lineSpacing;
                for (const auto& [resource, rate] : module.productionRates) {
                    if (rate > 0) {
                        std::string resourceName;
                        try {
                            resourceName = ResourceUtils::GetResourceName(resource);
                        } catch (...) {
                            resourceName = "Unknown";
                        }
                        safeDrawText(TextFormat("%s: +%.2f/s", resourceName.c_str(), rate),
                                   leftMargin + padding, yPos, 20, GREEN);
                        yPos += lineSpacing;
                    }
                }
                yPos += lineSpacing/2;  // Add spacing only if we displayed production rates
            }

            // Consumption Rates - with safety checks
            bool hasConsumption = false;
            for (const auto& [resource, rate] : module.consumptionRates) {
                if (rate > 0) {
                    hasConsumption = true;
                    break;
                }
            }
            if (hasConsumption) {
                if (safeDrawText("Consumption Rates:", leftMargin, yPos, 20, BLACK)) {
                    yPos += lineSpacing;
                    for (const auto& [resource, rate] : module.consumptionRates) {
                        if (rate > 0) {
                            std::string resourceName;
                            try {
                                resourceName = ResourceUtils::GetResourceName(resource);
                            } catch (...) {
                                resourceName = "Unknown";
                            }

                            const char* rateText = TextFormat("%s: -%.2f/s",
                                                            resourceName.c_str(),
                                                            rate);
                            if (!safeDrawText(rateText, leftMargin + padding, yPos, 20, RED)) {
                                break;  // Stop if we've run out of space
                            }
                            yPos += lineSpacing;
                        }
                    }
                }
            }

        } else {
            safeDrawText("No statistics available - Module not built",
                        leftMargin, yPos, 20, DARKGRAY);
        }
    }

    // Draw Stats/Back button at the bottom
    const int buttonWidth = 100;
    const int horizontalPadding = 20;  // Padding from panel edges
    Rectangle statsButton = {
        static_cast<float>(GetScreenWidth()/2 - 50),  // Start after left panel
        static_cast<float>(screenHeight - bottomMargin + 10),
        static_cast<float>(buttonWidth),  // Fill width between panels
        30
    };

    Color buttonColor = module.isBuilt ? BLUE : GRAY;
    bool isStatsHovered = CheckCollisionPointRec(GetMousePosition(), statsButton) && module.isBuilt;
    if (isStatsHovered) {
        buttonColor = Fade(buttonColor, 0.7f);
    }

    DrawRectangleRec(statsButton, buttonColor);
    const char* buttonText = showingStats ? "< Back" : "Stats >";
    textWidth = MeasureText(buttonText, 20);
    DrawText(buttonText,
             statsButton.x + (statsButton.width - textWidth) / 2,  // Center text
             statsButton.y + 5,
             20, WHITE);


    if (isStatsHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        showingStats = !showingStats;
        ShowMessage(showingStats ? "Showing module statistics" : "Showing module details");
    }
}

void Unit::DrawMultilineText(const std::string& text, int x, int y, int fontSize, Color color, int lineSpacing) {
    std::string currentLine;
    int currentY = y;

    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '\n') {
            // Draw current line and move to next line
            DrawText(currentLine.c_str(), x, currentY, fontSize, color);
            currentY += fontSize + lineSpacing;
            currentLine.clear();
        } else {
            currentLine += text[i];
        }
    }

    // Draw the last line if any
    if (!currentLine.empty()) {
        DrawText(currentLine.c_str(), x, currentY, fontSize, color);
    }
}

void Unit::ShowMessage(const std::string& text) {
    currentMessage.text = text;
    currentMessage.opacity = 1.0f;
    currentMessage.timeRemaining = 2.0f;
}

void Unit::UpdateMessage(float deltaTime) {
    if (currentMessage.timeRemaining > 0) {
        currentMessage.timeRemaining -= deltaTime;
        if (currentMessage.timeRemaining <= 0.5f) {
            currentMessage.opacity = currentMessage.timeRemaining / 0.5f;
        }
    }
}

bool Unit::IsModuleButtonClicked(Rectangle buttonRect) {
    Vector2 mousePoint = GetMousePosition();
    return CheckCollisionPointRec(mousePoint, buttonRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Unit::HandleModuleActivation(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= modules.size()) {
        return;
    }

    UnitModule& module = modules[moduleIndex];
    if (!module.isBuilt) {
        ShowMessage("Module needs to be built first");
        return;
    }

    // Toggle the selected module's active state
    if (module.isActive) {
        module.isActive = false;
        module.efficiency *= 0.9f;  // Penalty for deactivating
        ShowMessage(module.name + " deactivated");
    } else {
        // Deactivate other modules before activating this one
        for (auto& mod : modules) {
            if (mod.isActive) {
                mod.isActive = false;
                mod.efficiency *= 0.9f;  // Penalty for switching
            }
        }
        module.isActive = true;
        ShowMessage(module.name + " activated");
    }

    // Update unit status based on module states
    UpdateUnitStatus();

    // Recalculate consumption if we have an active module
    if (activeModule) {
        CalculateConsumption();
    }
}

bool Unit::CanUpgradeModule(const UnitModule& module) {
    if (!module.isBuilt || module.level >= 5) return false;

    // Safely check if next level costs exist
    auto costIter = module.upgradeCosts.find(module.level + 1);
    if (costIter == module.upgradeCosts.end()) {
        return false;
    }

    // Check if we have required resources for upgrade
    for (const auto& [resource, amount] : costIter->second) {
        if (resourceStorage[resource] < amount) {
            return false;
        }
    }
    return true;
}

bool Unit::CanBuildModule(const UnitModule& module) {
    if (module.isBuilt) return false;

    // Safely check if level 1 costs exist
    auto costIter = module.upgradeCosts.find(1);  // Changed from .at() to .find()
    if (costIter == module.upgradeCosts.end()) {
        return false;
    }

    // Check if we have required resources for initial build
    for (const auto& [resource, amount] : costIter->second) {
        if (resourceStorage[resource] < amount) {
            return false;
        }
    }
    return true;
}

void Unit::BuildModule(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= modules.size()) return;

    UnitModule& module = modules[moduleIndex];
    if (module.isBuilt) return;

    // Safely get level 1 costs
    auto costIter = module.upgradeCosts.find(1);
    if (costIter == module.upgradeCosts.end()) {
        ShowMessage("Error: No build costs defined!");
        return;
    }

    // Consume resources
    for (const auto& [resource, amount] : costIter->second) {
        resourceStorage[resource] -= amount;
    }

    module.isBuilt = true;
    module.level = 1;
    ShowMessage(module.name + " built successfully!");
}
