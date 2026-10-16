#include "Overlay.h"
#include "Settings.h"
#include "Statistics.h"
#include "sdk/modtools_plugin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
    namespace
    {
        constexpr const char* kLabels[] = { "Brushes", "Patches", "Vertices", "Models", "Script Models", "Script Brushmodels", "Script Origins", "Script Structs", "FX", "Triggers", "Lights", "Reflection Probes", "Volume Decals" };
        constexpr uintptr_t kDrawText2D = 0x1411E1FE0;
        constexpr uintptr_t kDrawQuad2D = 0x1411E7DF0;
        constexpr uintptr_t kDebugFont = 0x145573AA0;
        constexpr unsigned char kPanelAlpha = 115;

        using DrawText2D = void(__fastcall*)(const char*, void**, const float*, const float*, const float*, const float*, float*, unsigned char);
        using DrawQuad2D = void(__fastcall*)(const float*, const float*, const float*, float, float, float, float, const uint32_t*);
        DrawText2D g_drawText = nullptr;
        DrawQuad2D g_drawQuad = nullptr;
        void** g_fontSlot = nullptr;
        bool g_hudResolved = false;

        std::string Format(const Value& value)
        {
            if (!value.available) return "N/A";
            std::string s = std::to_string(value.value);
            for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
            return s;
        }

        bool NativeHudReady()
        {
            if (!g_hudResolved)
            {
                g_hudResolved = true;
                g_drawText = reinterpret_cast<DrawText2D>(rp::host->RadiantAddress(rp::self, kDrawText2D));
                g_drawQuad = reinterpret_cast<DrawQuad2D>(rp::host->RadiantAddress(rp::self, kDrawQuad2D));
                g_fontSlot = reinterpret_cast<void**>(rp::host->RadiantAddress(rp::self, kDebugFont));
                if (!g_drawText || !g_drawQuad || !g_fontSlot) rp::Log("native HUD drawing unavailable; using DrawLabel fallback");
            }
            if (!g_drawText || !g_drawQuad || !g_fontSlot || !*g_fontSlot) return false;
            const char* font = static_cast<const char*>(*g_fontSlot);
            return *reinterpret_cast<void* const*>(font + 0x48) != nullptr;
        }

        float NativeLineHeight()
        {
            const int height = *reinterpret_cast<const int*>(static_cast<const char*>(*g_fontSlot) + 0x40);
            return height > 0 && height < 128 ? static_cast<float>(height) : 12.0f;
        }

        void Measure(const std::string& text, float& width, float& height)
        {
            width = 8.0f * static_cast<float>(text.size());
            height = 12.0f;
            if (rp::Has(&rp::host->MeasureLabel) && rp::host->MeasureLabel)
                rp::host->MeasureLabel(rp::self, text.c_str(), 2.0f, &width, &height);
        }

        void DrawNative(const ModToolsDrawContext* ctx, float x, float y, const char* text, uint32_t argb)
        {
            const float lineHeight = NativeLineHeight();
            float pos[3] = {
                std::floor(x) - static_cast<float>(ctx->width) * 0.5f,
                static_cast<float>(ctx->height) * 0.5f - std::floor(y + lineHeight),
                0.0f
            };
            float xAdvance[3] = { 1.0f, 0.0f, 0.0f };
            float yDirection[3] = { 0.0f, -1.0f, 0.0f };
            float rgba[4] = {
                static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>(argb & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f
            };
            g_drawText(text, g_fontSlot, pos, xAdvance, yDirection, rgba, nullptr, 0);
        }

        void DrawPanel(const ModToolsDrawContext* ctx, float x, float y, float width, float height)
        {
            const float origin[3] = {
                std::floor(x) - static_cast<float>(ctx->width) * 0.5f,
                static_cast<float>(ctx->height) * 0.5f - std::floor(y),
                0.0f
            };
            const float horizontal[3] = { width, 0.0f, 0.0f };
            const float vertical[3] = { 0.0f, -height, 0.0f };
            const uint32_t color = static_cast<uint32_t>(kPanelAlpha) << 24;
            g_drawQuad(origin, horizontal, vertical, 1.0f, 1.0f, 1.0f, 1.0f, &color);
        }
    }

    void DrawOverlay(const ModToolsDrawContext* ctx)
    {
        if (!ctx || ctx->view != MODTOOLS_VIEW_3D || !GetSettings().enabled) return;
        struct Row { std::string label, value; float lw = 0.0f, vw = 0.0f; };
        std::vector<Row> rows;
        const auto& settings = GetSettings();
        const auto& snapshot = CurrentSnapshot();
        float labelWidth = 0.0f, valueWidth = 0.0f, lineHeight = 12.0f;
        for (size_t i = 0; i < static_cast<size_t>(StatId::Count); ++i)
        {
            if (!settings.shown[i]) continue;
            Row row{ kLabels[i], Format(snapshot.values[i]) };
            float labelHeight = 0.0f, valueHeight = 0.0f;
            Measure(row.label, row.lw, labelHeight);
            Measure(row.value, row.vw, valueHeight);
            labelWidth = std::max(labelWidth, row.lw);
            valueWidth = std::max(valueWidth, row.vw);
            lineHeight = std::max({ lineHeight, labelHeight, valueHeight });
            rows.push_back(std::move(row));
        }
        if (rows.empty()) return;

        float titleWidth = 0.0f, titleHeight = 0.0f;
        Measure("MAP STATISTICS", titleWidth, titleHeight);
        const float columnGap = 28.0f, margin = 14.0f, rowGap = 3.0f, titleGap = 8.0f, padding = 6.0f;
        const float contentWidth = std::max(titleWidth, labelWidth + columnGap + valueWidth);
        const float contentHeight = titleHeight + titleGap + rows.size() * lineHeight + (rows.size() - 1) * rowGap;
        const float panelWidth = contentWidth + padding * 2.0f;
        const float panelHeight = contentHeight + padding * 2.0f;
        const bool right = settings.position == Position::BottomRight || settings.position == Position::TopRight;
        const bool bottom = settings.position == Position::BottomRight || settings.position == Position::BottomLeft;
        const float bottomReserve = 44.0f;
        const float panelX = right ? std::max(margin, static_cast<float>(ctx->width) - margin - panelWidth) : margin;
        const float panelY = bottom ? std::max(margin, static_cast<float>(ctx->height) - bottomReserve - panelHeight) : margin;
        const float textX = panelX + padding;
        const float textY = panelY + padding;

        if (!NativeHudReady())
        {
            rp::host->DrawLabel(rp::self, textX, textY, "MAP STATISTICS", settings.textColor, 2.0f);
            float fallbackY = textY + titleHeight + titleGap;
            for (const Row& row : rows)
            {
                rp::host->DrawLabel(rp::self, textX, fallbackY, row.label.c_str(), settings.textColor, 2.0f);
                rp::host->DrawLabel(rp::self, textX + contentWidth - row.vw, fallbackY, row.value.c_str(), settings.textColor, 2.0f);
                fallbackY += lineHeight + rowGap;
            }
            return;
        }

        DrawPanel(ctx, panelX, panelY, panelWidth, panelHeight);
        DrawNative(ctx, textX, textY, "MAP STATISTICS", settings.textColor);
        float currentY = textY + titleHeight + titleGap;
        for (const Row& row : rows)
        {
            DrawNative(ctx, textX, currentY, row.label.c_str(), settings.textColor);
            DrawNative(ctx, textX + contentWidth - row.vw, currentY, row.value.c_str(), settings.textColor);
            currentY += lineHeight + rowGap;
        }
    }
}
