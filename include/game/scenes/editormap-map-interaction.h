#pragma once

#if GAME_ENV_DEV

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

constexpr int kEditorMapMaxTowerHotspots = 12;
constexpr int kEditorMapMaxMortarHotspots = 12;
constexpr int kEditorMapDefaultAssetClickDistanceTiles = 8;
constexpr int kEditorMapMinAssetClickDistanceTiles = 1;
constexpr int kEditorMapMaxAssetClickDistanceTiles = 30;

enum class EditorMapAssetClickGuiTarget : int
{
    NONE = 0,
    GUILD_TOWER = 1,
    GUILD_MORTAR = 2,
    MARKETS_AND_BAZAR = 3,
    ACCOUNT_MANAGEMENT = 4,
    MONEY = 5,
    LOG_BOOK = 6,
    ANNOUNCEMENTS = 7,
    LEADERBOARD = 8,
    ESPION = 9,
    GAME_SETTINGS = 10,
    PARAMS_MINIMAP = 11,
    CHAT = 12
};

struct EditorMapAssetClickGuiTargetInfo
{
    EditorMapAssetClickGuiTarget target;
    const char* jsonId;
    const char* label;
};

inline constexpr std::array<EditorMapAssetClickGuiTargetInfo, 13> kEditorMapAssetClickGuiTargets = {{
    {EditorMapAssetClickGuiTarget::NONE, "NONE", "AUCUNE"},
    {EditorMapAssetClickGuiTarget::GUILD_TOWER, "GUILD_TOWER", "GUILD TOWER"},
    {EditorMapAssetClickGuiTarget::GUILD_MORTAR, "GUILD_MORTAR", "GUILD MORTAR"},
    {EditorMapAssetClickGuiTarget::MARKETS_AND_BAZAR, "MARKETS_AND_BAZAR", "MARCHES / BAZAR"},
    {EditorMapAssetClickGuiTarget::ACCOUNT_MANAGEMENT, "ACCOUNT_MANAGEMENT", "COMPTE / NAVIRES"},
    {EditorMapAssetClickGuiTarget::MONEY, "MONEY", "MONNAIES"},
    {EditorMapAssetClickGuiTarget::LOG_BOOK, "LOG_BOOK", "JOURNAL DE BORD"},
    {EditorMapAssetClickGuiTarget::ANNOUNCEMENTS, "ANNOUNCEMENTS", "ANNONCES"},
    {EditorMapAssetClickGuiTarget::LEADERBOARD, "LEADERBOARD", "CLASSEMENTS"},
    {EditorMapAssetClickGuiTarget::ESPION, "ESPION", "ESPION"},
    {EditorMapAssetClickGuiTarget::GAME_SETTINGS, "GAME_SETTINGS", "PARAMETRES"},
    {EditorMapAssetClickGuiTarget::PARAMS_MINIMAP, "PARAMS_MINIMAP", "PARAMS MINIMAP"},
    {EditorMapAssetClickGuiTarget::CHAT, "CHAT", "CHAT"}
}};

inline int ClampEditorMapTowerHotspotNumber(int value)
{
    return std::clamp(value, 1, kEditorMapMaxTowerHotspots);
}

inline int ClampEditorMapMortarHotspotNumber(int value)
{
    return std::clamp(value, 1, kEditorMapMaxMortarHotspots);
}

inline int ClampEditorMapAssetClickDistanceTiles(int value)
{
    return std::clamp(
        value,
        kEditorMapMinAssetClickDistanceTiles,
        kEditorMapMaxAssetClickDistanceTiles);
}

inline const EditorMapAssetClickGuiTargetInfo& GetEditorMapAssetClickGuiTargetInfo(
    EditorMapAssetClickGuiTarget target)
{
    for (const EditorMapAssetClickGuiTargetInfo& info : kEditorMapAssetClickGuiTargets)
    {
        if (info.target == target)
        {
            return info;
        }
    }

    return kEditorMapAssetClickGuiTargets[0];
}

inline bool TryParseEditorMapAssetClickGuiTarget(
    const char* jsonId,
    EditorMapAssetClickGuiTarget* outTarget)
{
    if (jsonId == nullptr || outTarget == nullptr)
    {
        return false;
    }

    for (const EditorMapAssetClickGuiTargetInfo& info : kEditorMapAssetClickGuiTargets)
    {
        if (std::strcmp(info.jsonId, jsonId) == 0)
        {
            *outTarget = info.target;
            return true;
        }
    }

    return false;
}

inline EditorMapAssetClickGuiTarget CycleEditorMapAssetClickGuiTarget(
    EditorMapAssetClickGuiTarget current,
    int delta)
{
    const int count = static_cast<int>(kEditorMapAssetClickGuiTargets.size());
    if (count <= 0 || delta == 0)
    {
        return current;
    }

    int currentIndex = 0;
    for (int i = 0; i < count; ++i)
    {
        if (kEditorMapAssetClickGuiTargets[static_cast<std::size_t>(i)].target == current)
        {
            currentIndex = i;
            break;
        }
    }

    int nextIndex = currentIndex + delta;
    while (nextIndex < 0)
    {
        nextIndex += count;
    }
    while (nextIndex >= count)
    {
        nextIndex -= count;
    }

    return kEditorMapAssetClickGuiTargets[static_cast<std::size_t>(nextIndex)].target;
}

#endif
