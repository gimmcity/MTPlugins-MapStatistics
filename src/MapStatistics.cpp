#define MODTOOLS_PLUGIN_IMPL
#include "MapStatistics.h"
#include "Overlay.h"
#include "Settings.h"
#include "Statistics.h"

namespace ms
{
    namespace
    {
        void ResetStatistics()
        {
            ClearStatistics();
            RequestViewportRedraw();
        }

        void OnMapLoaded(const wchar_t*) { ResetStatistics(); }
        void OnScopeChanged(const wchar_t*, int) { ResetStatistics(); }
        void OnBeforeScopeChange() { ResetStatistics(); }
        void OnSettings() { ShowSettings(); }
        void OnUnload() { DestroySettingsUi(); ClearStatistics(); }

        int OnLoad()
        {
            LoadSettings();
            if (!CreateSettingsUi()) { rp::Log("Map Statistics: the native settings UI is unavailable"); return 0; }
            const int hotkey = rp::host->RegisterHotkey(rp::self, VK_F12, MODTOOLS_MOD_CTRL | MODTOOLS_MOD_SHIFT, ToggleOverlay, nullptr);
            rp::Log("Map Statistics 1.1.3 loaded; Ctrl+Shift+F12 toggle %s", hotkey ? "registered" : "not registered (shortcut conflict)");
            return 1;
        }
    }

    void RefreshNow(void*)
    {
        if (!rp::host || rp::host->IsReloading(rp::self)) return;
        RefreshStatistics();
        RequestViewportRedraw();
    }

    void RequestViewportRedraw() { if (rp::host && rp::Has(&rp::host->RequestRedraw) && rp::host->RequestRedraw) rp::host->RequestRedraw(rp::self, MODTOOLS_VIEW_3D); }
}

extern "C" MODTOOLS_PLUGIN_EXPORT const ModToolsPluginApi* ModToolsPluginEntry(ModToolsPlugin* self, const ModToolsHostApi* host)
{
    rp::Bind(self, host);
    static ModToolsPluginApi api{};
    api.size = sizeof api; api.apiVersion = MODTOOLS_PLUGIN_API_VERSION;
    api.name = "MapStatistics"; api.displayName = "Map Statistics"; api.version = "1.1.3"; api.author = "GimmCity";
    api.description = "Configurable on-demand map statistics rendered in Radiant's 3D camera viewport.";
    api.apps = MODTOOLS_APP_RADIANT;
    api.OnLoad = ms::OnLoad; api.OnUnload = ms::OnUnload;
    api.OnMapLoaded = ms::OnMapLoaded;
    api.OnBeforeScopeChange = ms::OnBeforeScopeChange; api.OnScopeChanged = ms::OnScopeChanged;
    api.OnDraw = ms::DrawOverlay; api.OnSettings = ms::OnSettings;
    return &api;
}
