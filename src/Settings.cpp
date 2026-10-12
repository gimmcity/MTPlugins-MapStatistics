#include "Settings.h"
#include "MapStatistics.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>

namespace ms
{
    namespace
    {
        Settings g_settings;
        ModToolsWidget g_dialog = nullptr;
        ModToolsWidget g_enabled = nullptr;
        ModToolsWidget g_refreshButton = nullptr;
        ModToolsWidget g_position = nullptr;
        ModToolsWidget g_colorButton = nullptr;
        ModToolsWidget g_colorHex = nullptr;
        ModToolsWidget g_root = nullptr;
        ModToolsWidget g_toggleAction = nullptr;
        bool g_syncing = false;
        std::array<ModToolsWidget, static_cast<size_t>(StatId::Count)> g_checks{};

        constexpr const char* kKeys[] = { "brushes", "patches", "vertices", "models", "script_models", "script_brushmodels", "script_origins", "script_structs", "fx", "triggers", "lights", "reflection_probes" };
        constexpr const char* kLabels[] = { "Brushes", "Patches", "Vertices", "Models", "Script Models", "Script Brushmodels", "Script Origins", "Script Structs", "FX", "Triggers", "Lights", "Reflection Probes" };

        uint32_t ParseColor(std::string value)
        {
            if (!value.empty() && value.front() == '#') value.erase(value.begin());
            if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) value.erase(0, 2);
            char* end = nullptr;
            const unsigned long rgb = std::strtoul(value.c_str(), &end, 16);
            if (!end || end == value.c_str() || *end != '\0' || rgb > 0xFFFFFFul) return 0xFFFF8000;
            return 0xFF000000u | static_cast<uint32_t>(rgb);
        }

        std::string ColorText(uint32_t argb)
        {
            char text[32] = {};
            std::snprintf(text, sizeof text, "Choose...   #%06X", argb & 0xFFFFFFu);
            return text;
        }

        std::string ColorHex(uint32_t argb)
        {
            char text[8] = {};
            std::snprintf(text, sizeof text, "#%06X", argb & 0xFFFFFFu);
            return text;
        }

        void ApplyUiState()
        {
            const auto* ui = rp::host ? rp::host->ui : nullptr;
            if (!ui) return;
            g_syncing = true;
            if (g_enabled) ui->SetChecked(g_enabled, g_settings.enabled ? 1 : 0);
            if (g_position) ui->ComboSetCurrent(g_position, static_cast<int>(g_settings.position));
            if (g_colorButton) ui->SetButtonText(g_colorButton, ColorText(g_settings.textColor).c_str());
            if (g_colorHex) ui->SetEditText(g_colorHex, ColorHex(g_settings.textColor).c_str());
            for (size_t i = 0; i < g_checks.size(); ++i) if (g_checks[i]) ui->SetChecked(g_checks[i], g_settings.shown[i] ? 1 : 0);
            if (g_toggleAction && rp::HasUi(&ui->SetActionChecked) && ui->SetActionChecked) ui->SetActionChecked(g_toggleAction, 1, g_settings.enabled ? 1 : 0);
            g_syncing = false;
        }

        void Changed(void* user, int value)
        {
            if (g_syncing) return;
            const intptr_t id = reinterpret_cast<intptr_t>(user);
            if (id == -1) g_settings.enabled = value != 0;
            else if (id == -2) g_settings.position = static_cast<Position>(std::clamp(value, 0, 3));
            else if (id >= 0 && id < static_cast<intptr_t>(StatId::Count)) g_settings.shown[static_cast<size_t>(id)] = value != 0;
            SaveSettings();
            ApplyUiState();
            RequestViewportRedraw();
        }

        void PickTextColor(void*)
        {
            const auto* ui = rp::host ? rp::host->ui : nullptr;
            if (!ui || !rp::HasUi(&ui->PickColor) || !ui->PickColor) return;
            uint32_t picked = g_settings.textColor;
            if (!ui->PickColor(g_root, g_settings.textColor, "Map Statistics Text Color", &picked)) return;
            g_settings.textColor = 0xFF000000u | (picked & 0xFFFFFFu);
            rp::Log("text color changed to #%06X", g_settings.textColor & 0xFFFFFFu);
            SaveSettings();
            ApplyUiState();
            RequestViewportRedraw();
        }

        void ColorHexChanged(void*, int)
        {
            if (g_syncing || !g_colorHex) return;
            g_settings.textColor = ParseColor(rp::EditText(g_colorHex));
            rp::Log("text color changed to #%06X", g_settings.textColor & 0xFFFFFFu);
            SaveSettings();
            ApplyUiState();
            RequestViewportRedraw();
        }
    }

    Settings& GetSettings() { return g_settings; }

    void LoadSettings()
    {
        // Remove obsolete options from configurations written by older releases.
        rp::host->ConfigSet(rp::self, "statistics", "images", nullptr);
        rp::host->ConfigSet(rp::self, "statistics", "entities", nullptr);
        rp::host->ConfigSet(rp::self, "statistics", "materials", nullptr);
        g_settings.enabled = rp::ConfigGetBool("overlay", "enabled", true);
        int pos = rp::ConfigGetInt("overlay", "position", 0);
        g_settings.position = static_cast<Position>(std::clamp(pos, 0, 3));
        g_settings.textColor = ParseColor(rp::ConfigGet("overlay", "color", "FF8000"));
        for (size_t i = 0; i < g_settings.shown.size(); ++i) g_settings.shown[i] = rp::ConfigGetBool("statistics", kKeys[i], true);
    }

    void SaveSettings()
    {
        rp::ConfigSet("overlay", "enabled", g_settings.enabled ? "1" : "0");
        rp::ConfigSet("overlay", "position", std::to_string(static_cast<int>(g_settings.position)));
        char color[7] = {}; std::snprintf(color, sizeof color, "%06X", g_settings.textColor & 0xFFFFFFu);
        rp::ConfigSet("overlay", "color", color);
        for (size_t i = 0; i < g_settings.shown.size(); ++i) rp::ConfigSet("statistics", kKeys[i], g_settings.shown[i] ? "1" : "0");
    }

    bool CreateSettingsUi()
    {
        if (!rp::host || !rp::host->ui || !rp::Has(&rp::host->CreateDialogWindow) || !rp::host->CreateDialogWindow) return false;
        const ModToolsUiApi* ui = rp::host->ui;
        ModToolsWidget root = ui->Widget(nullptr);
        g_root = root;
        ModToolsWidget box = ui->VBox(root);
        ui->LayoutMargins(box, 14, 14, 14, 14);
        ui->LayoutSpacing(box, 8);

        ModToolsWidget intro = ui->Label("Choose which values appear in Radiant's 3D camera viewport. Values are scanned only when you click Refresh Statistics Now.", root);
        ui->SetWordWrap(intro, 1);
        ui->BoxAddWidget(box, intro, 0);
        g_enabled = ui->CheckBox("Enable viewport overlay", root);
        ui->BoxAddWidget(box, g_enabled, 0);
        g_refreshButton = ui->PushButton("Refresh Statistics Now", root);
        ui->SetToolTip(g_refreshButton, "Scan the currently open map once and update the overlay.");
        ui->BoxAddWidget(box, g_refreshButton, 0);

        ModToolsWidget grid = ui->Grid();
        for (size_t i = 0; i < g_checks.size(); ++i)
        {
            g_checks[i] = ui->CheckBox(kLabels[i], root);
            ui->GridAddWidget(grid, g_checks[i], static_cast<int>(i / 2), static_cast<int>(i % 2));
            rp::host->OnChanged(rp::self, g_checks[i], Changed, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        }
        ui->GridColumnStretch(grid, 0, 1); ui->GridColumnStretch(grid, 1, 1);
        ui->BoxAddLayout(box, grid, 0);

        ModToolsWidget row = ui->HBox();
        ui->BoxAddWidget(row, ui->Label("Overlay position:", root), 0);
        g_position = ui->ComboBox(root);
        ui->ComboAdd(g_position, "Bottom Right"); ui->ComboAdd(g_position, "Top Right");
        ui->ComboAdd(g_position, "Top Left"); ui->ComboAdd(g_position, "Bottom Left");
        ui->BoxAddWidget(row, g_position, 1);
        ui->BoxAddLayout(box, row, 0);

        ModToolsWidget colorRow = ui->HBox();
        ui->BoxAddWidget(colorRow, ui->Label("Text color:", root), 0);
        g_colorButton = ui->PushButton(ColorText(g_settings.textColor).c_str(), root);
        ui->BoxAddWidget(colorRow, g_colorButton, 1);
        g_colorHex = ui->LineEdit(ColorHex(g_settings.textColor).c_str(), root);
        ui->SetMaximumWidth(g_colorHex, 90);
        ui->SetToolTip(g_colorHex, "Hex color in #RRGGBB format; press Enter to apply.");
        ui->BoxAddWidget(colorRow, g_colorHex, 0);
        ui->BoxAddLayout(box, colorRow, 0);
        ui->BoxAddStretch(box, 1);

        rp::host->OnChanged(rp::self, g_enabled, Changed, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
        rp::host->OnClicked(rp::self, g_refreshButton, RefreshNow, nullptr);
        rp::host->OnChanged(rp::self, g_position, Changed, reinterpret_cast<void*>(static_cast<intptr_t>(-2)));
        rp::host->OnClicked(rp::self, g_colorButton, PickTextColor, nullptr);
        rp::host->OnChanged(rp::self, g_colorHex, ColorHexChanged, nullptr);
        if (!rp::HasUi(&ui->PickColor) || !ui->PickColor) ui->SetEnabled(g_colorButton, 0);
        g_dialog = rp::host->CreateDialogWindow(rp::self, "Map Statistics Settings", root, 430, 470);
        ApplyUiState();

        ModToolsWidget menu = rp::host->AddMenu(rp::self, "Map Statistics");
        if (menu)
        {
            rp::host->AddMenuItem(rp::self, menu, "Refresh Statistics", RefreshNow, nullptr);
            rp::host->AddMenuItem(rp::self, menu, "Settings...", ShowSettings, nullptr);
            g_toggleAction = rp::host->AddMenuItem(rp::self, menu, "Enable Overlay", ToggleOverlay, nullptr);
            ApplyUiState();
        }
        return g_dialog != nullptr;
    }

    void ShowSettings(void*) { if (g_dialog && rp::host && rp::host->ui) rp::host->ui->SetVisible(g_dialog, 1); }

    void ToggleOverlay(void*)
    {
        g_settings.enabled = !g_settings.enabled;
        SaveSettings(); ApplyUiState(); RequestViewportRedraw();
    }

    void DestroySettingsUi()
    {
        g_dialog = g_enabled = g_refreshButton = g_position = g_colorButton = g_colorHex = g_root = g_toggleAction = nullptr;
        g_checks.fill(nullptr);
    }
}
