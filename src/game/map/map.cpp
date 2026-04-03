#include "game/map/map.h"

#include <algorithm>
#include <cmath>

Map::Map(void)
    : widthTiles(64),
      heightTiles(64),
      tileWidth(48.0f),
      tileHeight(32.0f),
      originX(0.0f),
      originY(0.0f),
      blockedTiles(static_cast<size_t>(64 * 64), static_cast<Uint8>(0))
{
}

Map::~Map(void)
{
}

void Map::setMapSize(int width, int height)
{
    // Garde-fous sur dimensions minimales.
    if (width < 1)
    {
        width = 1;
    }

    if (height < 1)
    {
        height = 1;
    }

    widthTiles = width;
    heightTiles = height;

    // On reset la couche collision à traversable.
    blockedTiles.assign(static_cast<size_t>(widthTiles * heightTiles), static_cast<Uint8>(0));
}

void Map::setTileSize(float width, float height)
{
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    tileWidth = width;
    tileHeight = height;
}

void Map::setOrigin(float x, float y)
{
    originX = x;
    originY = y;
}

void Map::centerOnRect(const SDL_FRect& rect)
{
    const float halfTileW = tileWidth * 0.5f;
    const float halfTileH = tileHeight * 0.5f;

    const float minCenterX = -(static_cast<float>(heightTiles - 1) * halfTileW);
    const float maxCenterX =  (static_cast<float>(widthTiles - 1) * halfTileW);
    const float minCenterY = 0.0f;
    const float maxCenterY = static_cast<float>(widthTiles + heightTiles - 2) * halfTileH;

    const float mapCenterX = (minCenterX + maxCenterX) * 0.5f;
    const float mapCenterY = (minCenterY + maxCenterY) * 0.5f;

    originX = (rect.x + (rect.w * 0.5f)) - mapCenterX;
    originY = (rect.y + (rect.h * 0.5f)) - mapCenterY;
}

SDL_FPoint Map::tileToScreenCenter(int tileX, int tileY) const
{
    return tileToScreenCenterFloat(static_cast<float>(tileX), static_cast<float>(tileY));
}

SDL_FPoint Map::tileToScreenCenterFloat(float tileX, float tileY) const
{
    const float halfTileW = tileWidth * 0.5f;
    const float halfTileH = tileHeight * 0.5f;

    SDL_FPoint result = {};
    result.x = originX + ((tileX - tileY) * halfTileW);
    result.y = originY + ((tileX + tileY) * halfTileH);
    return result;
}

SDL_FPoint Map::screenToTile(float screenX, float screenY) const
{
    const float halfTileW = tileWidth * 0.5f;
    const float halfTileH = tileHeight * 0.5f;

    SDL_FPoint result = {};

    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return result;
    }

    const float dx = (screenX - originX) / halfTileW;
    const float dy = (screenY - originY) / halfTileH;

    result.x = (dx + dy) * 0.5f;
    result.y = (dy - dx) * 0.5f;
    return result;
}

SDL_Point Map::screenToTileNearest(float screenX, float screenY) const
{
    const SDL_FPoint tileFloat = screenToTile(screenX, screenY);
    return roundTile(tileFloat.x, tileFloat.y);
}

SDL_Point Map::roundTile(float tileX, float tileY) const
{
    SDL_Point tile = {};
    tile.x = static_cast<int>(std::lround(static_cast<double>(tileX)));
    tile.y = static_cast<int>(std::lround(static_cast<double>(tileY)));
    return tile;
}

SDL_Point Map::clampTile(int tileX, int tileY) const
{
    SDL_Point result = {};

    if (widthTiles <= 0 || heightTiles <= 0)
    {
        return result;
    }

    tileX = std::clamp(tileX, 0, widthTiles - 1);
    tileY = std::clamp(tileY, 0, heightTiles - 1);

    result.x = tileX;
    result.y = tileY;
    return result;
}

bool Map::isInside(int tileX, int tileY) const
{
    return (tileX >= 0 && tileX < widthTiles && tileY >= 0 && tileY < heightTiles);
}

bool Map::setTileBlocked(int tileX, int tileY, bool blocked)
{
    if (!isInside(tileX, tileY))
    {
        return false;
    }

    blockedTiles[static_cast<size_t>(tileIndex(tileX, tileY))] = blocked ? static_cast<Uint8>(1) : static_cast<Uint8>(0);
    return true;
}

bool Map::isTileBlocked(int tileX, int tileY) const
{
    if (!isInside(tileX, tileY))
    {
        return true;
    }

    return blockedTiles[static_cast<size_t>(tileIndex(tileX, tileY))] != 0;
}

void Map::clearBlockedTiles(void)
{
    for (size_t i = 0; i < blockedTiles.size(); ++i)
    {
        blockedTiles[i] = static_cast<Uint8>(0);
    }
}

int Map::tileIndex(int tileX, int tileY) const
{
    return (tileY * widthTiles) + tileX;
}

int Map::getWidthTiles(void) const
{
    return widthTiles;
}

int Map::getHeightTiles(void) const
{
    return heightTiles;
}

float Map::getTileWidth(void) const
{
    return tileWidth;
}

float Map::getTileHeight(void) const
{
    return tileHeight;
}

float Map::getOriginX(void) const
{
    return originX;
}

float Map::getOriginY(void) const
{
    return originY;
}
