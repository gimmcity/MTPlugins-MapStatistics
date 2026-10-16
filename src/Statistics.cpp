#include "Statistics.h"
#include "MapStatistics.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ms
{
    namespace
    {
        Snapshot g_snapshot;
        std::wstring g_lastLoggedMap;
        int g_lastLoggedDepth = -1;

        std::string Trim(const std::string& s)
        {
            const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        }

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::wstring Lower(std::wstring s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return s;
        }

        bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

        bool SplitKey(const std::string& line, std::string& key, std::string& value)
        {
            if (line.size() < 5 || line.front() != '"') return false;
            const size_t a = line.find('"', 1), b = a == std::string::npos ? a : line.find('"', a + 1), c = line.rfind('"');
            if (a == std::string::npos || b == std::string::npos || c <= b) return false;
            key = line.substr(1, a - 1); value = line.substr(b + 1, c - b - 1); return true;
        }

        bool IsPrefabReference(const std::string& value)
        {
            const std::string lower = Lower(value);
            return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".map") == 0;
        }

        struct PointKey
        {
            long long x = 0, y = 0, z = 0;
            bool operator<(const PointKey& o) const { return x != o.x ? x < o.x : (y != o.y ? y < o.y : z < o.z); }
        };

        PointKey Point(double x, double y, double z)
        {
            constexpr double scale = 10000.0;
            return { std::llround(x * scale), std::llround(y * scale), std::llround(z * scale) };
        }

        void ParseClassicFace(const std::string& line, std::set<PointKey>& points)
        {
            size_t pos = 0;
            for (int group = 0; group < 3; ++group)
            {
                const size_t l = line.find('(', pos), r = l == std::string::npos ? l : line.find(')', l + 1);
                if (l == std::string::npos || r == std::string::npos) return;
                std::istringstream xyz(line.substr(l + 1, r - l - 1)); double x, y, z;
                if (xyz >> x >> y >> z) points.insert(Point(x, y, z));
                pos = r + 1;
            }
        }

        struct Accumulator
        {
            uint64_t brushes = 0, patches = 0, vertices = 0, models = 0, scriptModels = 0, scriptBrushModels = 0;
            uint64_t scriptOrigins = 0, scriptStructs = 0, fx = 0, triggers = 0, lights = 0, reflectionProbes = 0, volumeDecals = 0;
            bool failed = false;
            bool fxDependenciesComplete = true;
            std::unordered_set<std::string> entityIds;
        };

        struct FxAsset
        {
            std::filesystem::file_time_type stamp{};
            uintmax_t size = 0;
            uint64_t lights = 0;
            std::vector<std::string> runners;
        };

        std::unordered_map<std::wstring, FxAsset> g_fxCache;
        std::unordered_set<std::wstring> g_reportedFxErrors;

        bool ResolveFx(const std::string& name, std::filesystem::path& out)
        {
            if (!rp::host || !rp::host->RadiantRoot || name.empty()) return false;
            const wchar_t* rootText = rp::host->RadiantRoot(rp::self);
            if (!rootText || !*rootText) return false;
            std::string relative = name;
            std::replace(relative.begin(), relative.end(), '\\', '/');
            while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
            if (StartsWith(Lower(relative), "fx/")) relative.erase(0, 3);
            const std::string lowerRelative = Lower(relative);
            if (lowerRelative.size() < 4 || lowerRelative.compare(lowerRelative.size() - 4, 4, ".efx") != 0) relative += ".efx";

            const std::filesystem::path root(rootText);
            std::vector<std::filesystem::path> bases = { root / L"share" / L"raw" / L"fx", root / L"raw" / L"fx" };
            const std::wstring mapPath = rp::CurrentMapPath();
            if (!mapPath.empty()) bases.push_back(root / L"usermaps" / std::filesystem::path(mapPath).stem() / L"fx");
            for (const std::filesystem::path& baseRaw : bases)
            {
                std::error_code ec;
                const std::filesystem::path base = std::filesystem::weakly_canonical(baseRaw, ec);
                if (ec) continue;
                const std::filesystem::path candidate = std::filesystem::weakly_canonical(base / std::filesystem::u8path(relative), ec);
                if (ec) continue;
                std::wstring baseKey = Lower(base.native()); if (!baseKey.empty() && baseKey.back() != L'\\') baseKey += L'\\';
                const std::wstring candidateKey = Lower(candidate.native());
                if (candidateKey.rfind(baseKey, 0) == 0 && std::filesystem::is_regular_file(candidate, ec) && !ec) { out = candidate; return true; }
            }
            return false;
        }

        std::string QuotedValue(const std::string& line)
        {
            const size_t first = line.find('"'), last = line.rfind('"');
            return first != std::string::npos && last > first ? line.substr(first + 1, last - first - 1) : std::string();
        }

        const FxAsset* LoadFx(const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto stamp = std::filesystem::last_write_time(path, ec); if (ec) return nullptr;
            const uintmax_t size = std::filesystem::file_size(path, ec); if (ec) return nullptr;
            const std::wstring key = Lower(path.native());
            auto found = g_fxCache.find(key);
            if (found != g_fxCache.end() && found->second.stamp == stamp && found->second.size == size) return &found->second;

            std::ifstream file(path, std::ios::binary);
            if (!file) return nullptr;
            FxAsset parsed; parsed.stamp = stamp; parsed.size = size;
            enum class Capture { None, Runner } capture = Capture::None, pending = Capture::None;
            int captureDepth = 0;
            std::string raw;
            while (std::getline(file, raw))
            {
                const std::string line = Trim(raw), token = Lower(line);
                if (capture != Capture::None)
                {
                    if (line == "{") ++captureDepth;
                    else if (line == "}" && --captureDepth == 0) capture = Capture::None;
                    else
                    {
                        const std::string quoted = QuotedValue(line);
                        if (!quoted.empty()) parsed.runners.push_back(quoted);
                    }
                    continue;
                }
                if (pending != Capture::None)
                {
                    if (line == "{") { capture = pending; captureDepth = 1; }
                    pending = Capture::None;
                    if (capture != Capture::None) continue;
                }
                if (token == "dynamiclight" || token == "dynamiclight2" || token == "spotlight" || token == "omnilight") ++parsed.lights;
                else if (token == "runner") pending = Capture::Runner;
            }
            return &g_fxCache.insert_or_assign(key, std::move(parsed)).first->second;
        }

        void ExpandFx(const std::string& name, Accumulator& out, std::unordered_set<std::wstring>& stack, unsigned depth)
        {
            std::filesystem::path path;
            if (depth > 32 || !ResolveFx(name, path)) { out.fxDependenciesComplete = false; return; }
            const std::wstring key = Lower(path.native());
            if (!stack.insert(key).second) { out.fxDependenciesComplete = false; return; }
            const FxAsset* asset = LoadFx(path);
            if (!asset)
            {
                out.fxDependenciesComplete = false;
                if (g_reportedFxErrors.insert(key).second) rp::LogW(L"Map Statistics: cannot read FX %s", path.c_str());
                stack.erase(key); return;
            }
            out.lights += asset->lights;
            for (const std::string& child : asset->runners) ExpandFx(child, out, stack, depth + 1);
            stack.erase(key);
        }

        struct ParsedEntity { std::string classname, model, fxdef; };

        ParsedEntity ParseEntity(const std::string& text, const std::string& fallbackId, Accumulator& out, bool prefabContent)
        {
            int depth = 0;
            bool brushPatch = false, brushClassic = false;
            std::set<PointKey> brushPoints;
            std::string cls, model, fxdef, guid, key, value;
            size_t pos = 0;
            while (pos <= text.size())
            {
                const size_t nl = text.find('\n', pos);
                const std::string line = Trim(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
                pos = nl == std::string::npos ? text.size() + 1 : nl + 1;

                if (depth == 1 && SplitKey(line, key, value))
                {
                    const std::string lowerKey = Lower(key);
                    if (lowerKey == "classname") cls = Lower(value);
                    else if (lowerKey == "model") model = value;
                    else if (lowerKey == "fxdef") fxdef = value;
                }
                if (depth == 1 && StartsWith(Lower(line), "guid ")) guid = Lower(Trim(line.substr(5)));

                if (depth == 2 && (line == "mesh" || line == "curve")) brushPatch = true;
                else if (depth == 2 && !line.empty() && line.front() == '(')
                {
                    brushClassic = true; ParseClassicFace(line, brushPoints);
                }
                else if (brushPatch && depth >= 3 && StartsWith(line, "v ")) ++out.vertices;

                if (line == "{") ++depth;
                else if (line == "}")
                {
                    if (depth == 2)
                    {
                        if (brushPatch) ++out.patches;
                        else if (brushClassic) { ++out.brushes; out.vertices += brushPoints.size(); }
                        brushPatch = brushClassic = false; brushPoints.clear();
                    }
                    --depth;
                }
            }

            if (!(prefabContent && cls == "worldspawn"))
            {
                const std::string id = guid.empty() ? fallbackId : guid;
                if (out.entityIds.insert(id).second)
                {
                    if (cls == "misc_model" || cls == "dyn_model") ++out.models;
                    if (cls == "script_model") ++out.scriptModels;
                    if (cls == "script_brushmodel") ++out.scriptBrushModels;
                    if (cls == "script_origin") ++out.scriptOrigins;
                    if (cls == "script_struct") ++out.scriptStructs;
                    if (cls == "fx")
                    {
                        ++out.fx;
                        std::unordered_set<std::wstring> stack;
                        ExpandFx(fxdef, out, stack, 0);
                    }
                    if (StartsWith(cls, "trigger_")) ++out.triggers;
                    if (cls == "light" || StartsWith(cls, "light_")) ++out.lights;
                    if (cls == "reflection_probe") ++out.reflectionProbes;
                    if (cls == "misc_volume_decal") ++out.volumeDecals;
                }
            }
            return { cls, model, fxdef };
        }

        std::vector<std::string> EntityBlocks(const std::string& mapText)
        {
            std::vector<std::string> blocks;
            std::string current;
            int depth = 0;
            size_t pos = 0;
            while (pos <= mapText.size())
            {
                const size_t nl = mapText.find('\n', pos);
                const std::string raw = mapText.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
                pos = nl == std::string::npos ? mapText.size() + 1 : nl + 1;
                const std::string line = Trim(raw);
                if (depth == 0 && line != "{") continue;
                current += raw; current += '\n';
                if (line == "{") ++depth;
                else if (line == "}" && --depth == 0) { blocks.push_back(std::move(current)); current.clear(); }
            }
            return blocks;
        }

        void AddCounts(Accumulator& dst, const Accumulator& src)
        {
            dst.brushes += src.brushes; dst.patches += src.patches; dst.vertices += src.vertices;
            dst.models += src.models; dst.scriptModels += src.scriptModels; dst.scriptBrushModels += src.scriptBrushModels;
            dst.scriptOrigins += src.scriptOrigins; dst.scriptStructs += src.scriptStructs; dst.fx += src.fx;
            dst.triggers += src.triggers; dst.lights += src.lights;
            dst.reflectionProbes += src.reflectionProbes;
            dst.volumeDecals += src.volumeDecals;
            dst.failed = dst.failed || src.failed;
            dst.fxDependenciesComplete = dst.fxDependenciesComplete && src.fxDependenciesComplete;
        }

        struct CachedPrefab
        {
            std::filesystem::file_time_type stamp{};
            uintmax_t size = 0;
            Accumulator local;
            std::vector<std::string> nestedModels;
        };

        std::unordered_map<std::wstring, CachedPrefab> g_prefabCache;
        std::unordered_set<std::wstring> g_reportedPrefabErrors;

        bool ResolvePrefab(const std::string& model, std::filesystem::path& out)
        {
            if (!rp::host || !rp::host->RadiantRoot || !IsPrefabReference(model)) return false;
            const wchar_t* rootText = rp::host->RadiantRoot(rp::self);
            if (!rootText || !*rootText) return false;
            std::string relative = model;
            while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) relative.erase(relative.begin());
            std::error_code ec;
            const std::filesystem::path base = std::filesystem::weakly_canonical(std::filesystem::path(rootText) / L"map_source", ec);
            if (ec) return false;
            const std::filesystem::path candidate = std::filesystem::weakly_canonical(base / std::filesystem::u8path(relative), ec);
            if (ec) return false;
            std::wstring baseKey = Lower(base.native()); if (!baseKey.empty() && baseKey.back() != L'\\') baseKey += L'\\';
            const std::wstring candidateKey = Lower(candidate.native());
            if (candidateKey.rfind(baseKey, 0) != 0) return false;
            out = candidate; return true;
        }

        const CachedPrefab* LoadPrefab(const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto stamp = std::filesystem::last_write_time(path, ec); if (ec) return nullptr;
            const uintmax_t size = std::filesystem::file_size(path, ec); if (ec) return nullptr;
            const std::wstring key = Lower(path.native());
            auto found = g_prefabCache.find(key);
            if (found != g_prefabCache.end() && found->second.stamp == stamp && found->second.size == size) return &found->second;

            std::ifstream file(path, std::ios::binary);
            if (!file) return nullptr;
            const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            CachedPrefab parsed; parsed.stamp = stamp; parsed.size = size;
            const std::vector<std::string> blocks = EntityBlocks(text);
            for (size_t i = 0; i < blocks.size(); ++i)
            {
                const ParsedEntity entity = ParseEntity(blocks[i], "file:" + std::to_string(i), parsed.local, true);
                if (entity.classname == "misc_prefab" && IsPrefabReference(entity.model)) parsed.nestedModels.push_back(entity.model);
            }
            return &g_prefabCache.insert_or_assign(key, std::move(parsed)).first->second;
        }

        void ExpandPrefab(const std::string& model, Accumulator& out, std::unordered_set<std::wstring>& stack, unsigned depth)
        {
            std::filesystem::path path;
            if (depth > 32 || !ResolvePrefab(model, path)) { out.failed = true; return; }
            const std::wstring key = Lower(path.native());
            if (!stack.insert(key).second) { out.failed = true; return; }
            const CachedPrefab* prefab = LoadPrefab(path);
            if (!prefab)
            {
                out.failed = true;
                if (g_reportedPrefabErrors.insert(key).second) rp::LogW(L"Map Statistics: cannot read prefab %s", path.c_str());
                stack.erase(key); return;
            }
            AddCounts(out, prefab->local);
            const std::vector<std::string> nested = prefab->nestedModels;
            for (const std::string& child : nested) ExpandPrefab(child, out, stack, depth + 1);
            stack.erase(key);
        }

        struct ScanContext
        {
            Accumulator* out = nullptr;
            std::map<ModToolsEntity, std::vector<ModToolsBrush>> selected;
            std::vector<std::string> prefabModels;
            uint64_t fallback = 0;
        };

        void ScanEntity(ModToolsEntity entity, void* user)
        {
            auto& ctx = *static_cast<ScanContext*>(user);
            char* raw = nullptr;
            const auto selected = ctx.selected.find(entity);
            if (selected != ctx.selected.end() && rp::Has(&rp::host->WriteEntityTextWith) && rp::host->WriteEntityTextWith)
                raw = rp::host->WriteEntityTextWith(rp::self, entity, selected->second.data(), selected->second.size());
            else raw = rp::host->WriteEntityText(rp::self, entity);
            if (!raw) { ctx.out->failed = true; return; }
            const ParsedEntity parsed = ParseEntity(raw, "ptr:" + std::to_string(++ctx.fallback), *ctx.out, false);
            if (parsed.classname == "misc_prefab" && IsPrefabReference(parsed.model)) ctx.prefabModels.push_back(parsed.model);
            rp::host->FreeString(rp::self, raw);
        }
    }

    const Snapshot& CurrentSnapshot() { return g_snapshot; }

    void ClearStatistics()
    {
        const uint64_t generation = g_snapshot.generation + 1;
        g_snapshot = {}; g_snapshot.generation = generation;
    }

    bool RefreshStatistics()
    {
        if (!rp::host || !rp::host->WorldEntity(rp::self) || !rp::Has(&rp::host->ForEachEntity) || !rp::host->ForEachEntity) { ClearStatistics(); return false; }
        Accumulator acc;
        ScanContext ctx; ctx.out = &acc;
        if (rp::Has(&rp::host->SelectedBrushInsts) && rp::host->SelectedBrushInsts && rp::Has(&rp::host->BrushOfInst) && rp::host->BrushOfInst)
        {
            std::vector<ModToolsBrushInst> insts(256);
            size_t n = rp::host->SelectedBrushInsts(rp::self, insts.data(), insts.size());
            if (n > insts.size()) { insts.resize(n); n = rp::host->SelectedBrushInsts(rp::self, insts.data(), insts.size()); }
            for (size_t i = 0; i < std::min(n, insts.size()); ++i)
            {
                ModToolsBrush b = rp::host->BrushOfInst(rp::self, insts[i]);
                ModToolsEntity e = b ? rp::host->BrushOwner(rp::self, b) : nullptr;
                if (b && e) ctx.selected[e].push_back(b);
            }
        }
        rp::host->ForEachEntity(rp::self, ScanEntity, &ctx);
        std::unordered_set<std::wstring> prefabStack;
        for (const std::string& model : ctx.prefabModels) ExpandPrefab(model, acc, prefabStack, 0);

        Snapshot next; next.generation = g_snapshot.generation + 1;
        auto put = [&](StatId id, uint64_t value) { next.values[static_cast<size_t>(id)] = { !acc.failed, value }; };
        put(StatId::Brushes, acc.brushes); put(StatId::Patches, acc.patches); put(StatId::Vertices, acc.vertices);
        put(StatId::Models, acc.models); put(StatId::ScriptModels, acc.scriptModels);
        put(StatId::ScriptBrushModels, acc.scriptBrushModels);
        put(StatId::ScriptOrigins, acc.scriptOrigins); put(StatId::ScriptStructs, acc.scriptStructs); put(StatId::Fx, acc.fx);
        put(StatId::Triggers, acc.triggers);
        next.values[static_cast<size_t>(StatId::Lights)] = { !acc.failed && acc.fxDependenciesComplete, acc.lights };
        put(StatId::ReflectionProbes, acc.reflectionProbes);
        put(StatId::VolumeDecals, acc.volumeDecals);
        g_snapshot = next;
        const std::wstring currentMap = rp::CurrentMapPath();
        const int currentDepth = rp::host->PrefabDepth(rp::self);
        if (!currentMap.empty() && (currentMap != g_lastLoggedMap || currentDepth != g_lastLoggedDepth))
        {
            rp::Log("counts: brushes=%llu patches=%llu vertices=%llu models=%llu script_models=%llu script_brushmodels=%llu script_origins=%llu script_structs=%llu fx=%llu triggers=%llu lights=%llu reflection_probes=%llu volume_decals=%llu prefabs=%llu%s%s",
                acc.brushes, acc.patches, acc.vertices, acc.models, acc.scriptModels, acc.scriptBrushModels, acc.scriptOrigins, acc.scriptStructs, acc.fx,
                acc.triggers, acc.lights, acc.reflectionProbes, acc.volumeDecals, static_cast<unsigned long long>(ctx.prefabModels.size()),
                acc.failed ? " (incomplete map)" : "", acc.fxDependenciesComplete ? "" : " (incomplete FX dependencies)");
            g_lastLoggedMap = currentMap; g_lastLoggedDepth = currentDepth;
        }
        return !acc.failed;
    }
}
