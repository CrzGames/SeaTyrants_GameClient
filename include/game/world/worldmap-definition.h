#pragma once

#include <RC2D/RC2D.h>

#include <array>
#include <cstddef>
#include <cstring>

namespace WorldMapDefinition
{
enum class ZoneType {
    TERRESTRIAL = 0,
    CITY = 1,
    NORDIC = 2,
    CELESTIAL = 3,
    VOLCANIC = 4
};

struct LegendVisibility {
    bool showTerrestrial = true;
    bool showCity = true;
    bool showNordic = false;
    bool showCelestial = false;
    bool showVolcanic = false;
};

struct ZoneVisual {
    const char* typeId;
    const char* colorName;
    const char* colorHex;
    const char* legendLabel;
    const char* typeButtonLabel;
    RC2D_Color borderColor;
};

constexpr size_t kZoneTypeCount = 5U;

inline const std::array<ZoneType, kZoneTypeCount>& getZoneOrder(void)
{
    static const std::array<ZoneType, kZoneTypeCount> kZoneOrder = {{
        ZoneType::TERRESTRIAL,
        ZoneType::CITY,
        ZoneType::NORDIC,
        ZoneType::CELESTIAL,
        ZoneType::VOLCANIC
    }};
    return kZoneOrder;
}

inline ZoneType parseZoneTypeId(const char* typeId)
{
    if (typeId == nullptr)
    {
        return ZoneType::TERRESTRIAL;
    }

    if (std::strcmp(typeId, "map-ville") == 0)
    {
        return ZoneType::CITY;
    }
    if (std::strcmp(typeId, "map-normal") == 0 ||
        std::strcmp(typeId, "map-zone-terrestre") == 0)
    {
        return ZoneType::TERRESTRIAL;
    }
    if (std::strcmp(typeId, "map-zone-nordique") == 0)
    {
        return ZoneType::NORDIC;
    }
    if (std::strcmp(typeId, "map-zone-celeste") == 0)
    {
        return ZoneType::CELESTIAL;
    }
    if (std::strcmp(typeId, "map-zone-volcanique") == 0)
    {
        return ZoneType::VOLCANIC;
    }

    return ZoneType::TERRESTRIAL;
}

inline ZoneType getNextZoneType(ZoneType zoneType)
{
    switch (zoneType)
    {
        case ZoneType::TERRESTRIAL:
            return ZoneType::CITY;
        case ZoneType::CITY:
            return ZoneType::NORDIC;
        case ZoneType::NORDIC:
            return ZoneType::CELESTIAL;
        case ZoneType::CELESTIAL:
            return ZoneType::VOLCANIC;
        case ZoneType::VOLCANIC:
        default:
            return ZoneType::TERRESTRIAL;
    }
}

inline ZoneVisual getZoneVisual(ZoneType zoneType)
{
    switch (zoneType)
    {
        case ZoneType::CITY:
            return ZoneVisual{
                "map-ville",
                "orange",
                "#D99032",
                "Map Ville",
                "MAP VILLE",
                RC2D_Color{217, 144, 50, 244}};
        case ZoneType::NORDIC:
            return ZoneVisual{
                "map-zone-nordique",
                "blue",
                "#4A93D1",
                "Map Zone Nordique",
                "MAP ZONE NORDIQUE",
                RC2D_Color{74, 147, 209, 244}};
        case ZoneType::CELESTIAL:
            return ZoneVisual{
                "map-zone-celeste",
                "white",
                "#EFF3FA",
                "Map Zone Celeste",
                "MAP ZONE CELESTE",
                RC2D_Color{239, 243, 250, 244}};
        case ZoneType::VOLCANIC:
            return ZoneVisual{
                "map-zone-volcanique",
                "red",
                "#C43E2E",
                "Map Zone Volcanique",
                "MAP ZONE VOLCANIQUE",
                RC2D_Color{196, 62, 46, 244}};
        case ZoneType::TERRESTRIAL:
        default:
            return ZoneVisual{
                "map-zone-terrestre",
                "green",
                "#4FA35C",
                "Map Zone Terrestre",
                "MAP ZONE TERRESTRE",
                RC2D_Color{79, 163, 92, 244}};
    }
}

inline RC2D_Color getNeutralLinkColor(void)
{
    return RC2D_Color{134, 145, 158, 230};
}

inline bool isLegendVisible(const LegendVisibility& legendVisibility, ZoneType zoneType)
{
    switch (zoneType)
    {
        case ZoneType::CITY:
            return legendVisibility.showCity;
        case ZoneType::NORDIC:
            return legendVisibility.showNordic;
        case ZoneType::CELESTIAL:
            return legendVisibility.showCelestial;
        case ZoneType::VOLCANIC:
            return legendVisibility.showVolcanic;
        case ZoneType::TERRESTRIAL:
        default:
            return legendVisibility.showTerrestrial;
    }
}

inline void setLegendVisible(LegendVisibility* legendVisibility, ZoneType zoneType, bool visible)
{
    if (legendVisibility == nullptr)
    {
        return;
    }

    switch (zoneType)
    {
        case ZoneType::CITY:
            legendVisibility->showCity = visible;
            return;
        case ZoneType::NORDIC:
            legendVisibility->showNordic = visible;
            return;
        case ZoneType::CELESTIAL:
            legendVisibility->showCelestial = visible;
            return;
        case ZoneType::VOLCANIC:
            legendVisibility->showVolcanic = visible;
            return;
        case ZoneType::TERRESTRIAL:
        default:
            legendVisibility->showTerrestrial = visible;
            return;
    }
}

inline void enableLegendForZone(LegendVisibility* legendVisibility, ZoneType zoneType)
{
    setLegendVisible(legendVisibility, zoneType, true);
}

inline bool hasAnyLegendVisible(const LegendVisibility& legendVisibility)
{
    return
        legendVisibility.showTerrestrial ||
        legendVisibility.showCity ||
        legendVisibility.showNordic ||
        legendVisibility.showCelestial ||
        legendVisibility.showVolcanic;
}
} // namespace WorldMapDefinition
