#pragma once
#include "sdk/modtools_plugin.hpp"
#include <array>
#include <cstdint>

namespace ms
{
    enum class Position { BottomRight = 0, TopRight, TopLeft, BottomLeft };
    enum class StatId { Brushes = 0, Patches, Vertices, Models, ScriptModels, ScriptBrushModels, ScriptOrigins, ScriptStructs, Fx, Triggers, Lights, ReflectionProbes, Count };

    struct Settings
    {
        bool enabled = true;
        std::array<bool, static_cast<size_t>(StatId::Count)> shown{};
        Position position = Position::BottomRight;
        uint32_t textColor = 0xFFFF8000;
    };

    Settings& GetSettings();
    void LoadSettings();
    void SaveSettings();
    bool CreateSettingsUi();
    void ShowSettings(void* = nullptr);
    void ToggleOverlay(void* = nullptr);
    void DestroySettingsUi();
}
