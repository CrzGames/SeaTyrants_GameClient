#include "game/map/map.h"

#include <cmath>

Map::Map(void)
    : widthTiles(64),
      heightTiles(64),
      tileWidth(48.0f),
      tileHeight(32.0f),
      originX(0.0f),
      originY(0.0f),
      debugFillEnabled(true),
      debugLinesEnabled(true),
      debugFillColorA{0, 194, 255, 255},
      debugFillColorB{0, 166, 236, 255},
      debugLineColor{0, 110, 175, 220},
      tileObjects(static_cast<size_t>(64 * 64), -1)
{
}

Map::~Map(void)
{
}

void Map::setMapSize(int width, int height)
{
    // Evite les dimensions invalides.
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
    tileObjects.assign(static_cast<size_t>(widthTiles * heightTiles), -1);
}

void Map::setTileSize(float width, float height)
{
    // Evite les tailles invalides.
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
    // Centre geometrique des centres de tuiles de la grille.
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

void Map::setDebugFillEnabled(bool enabled)
{
    debugFillEnabled = enabled;
}

void Map::setDebugLinesEnabled(bool enabled)
{
    debugLinesEnabled = enabled;
}

void Map::setDebugFillColors(const RC2D_Color& colorA, const RC2D_Color& colorB)
{
    debugFillColorA = colorA;
    debugFillColorB = colorB;
}

void Map::setDebugLineColor(const RC2D_Color& color)
{
    debugLineColor = color;
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

    if (tileX < 0)
    {
        tileX = 0;
    }
    else if (tileX >= widthTiles)
    {
        tileX = widthTiles - 1;
    }

    if (tileY < 0)
    {
        tileY = 0;
    }
    else if (tileY >= heightTiles)
    {
        tileY = heightTiles - 1;
    }

    result.x = tileX;
    result.y = tileY;
    return result;
}

bool Map::isInside(int tileX, int tileY) const
{
    return (tileX >= 0 && tileX < widthTiles && tileY >= 0 && tileY < heightTiles);
}

int Map::tileIndex(int tileX, int tileY) const
{
    return (tileY * widthTiles) + tileX;
}

bool Map::setTileObject(int tileX, int tileY, int objectId)
{
    if (!isInside(tileX, tileY))
    {
        return false;
    }

    tileObjects[static_cast<size_t>(tileIndex(tileX, tileY))] = objectId;
    return true;
}

bool Map::clearTileObject(int tileX, int tileY)
{
    if (!isInside(tileX, tileY))
    {
        return false;
    }

    tileObjects[static_cast<size_t>(tileIndex(tileX, tileY))] = -1;
    return true;
}

int Map::getTileObject(int tileX, int tileY) const
{
    if (!isInside(tileX, tileY))
    {
        return -1;
    }

    return tileObjects[static_cast<size_t>(tileIndex(tileX, tileY))];
}

void Map::clearAllTileObjects(void)
{
    for (size_t i = 0; i < tileObjects.size(); ++i)
    {
        tileObjects[i] = -1;
    }
}

void Map::drawDebug(void) const
{
    // Evite un rendu invalide.
    if (widthTiles < 1 || heightTiles < 1 || tileWidth <= 0.0f || tileHeight <= 0.0f)
    {
        return;
    }

    for (int j = 0; j < heightTiles; ++j)
    {
        for (int i = 0; i < widthTiles; ++i)
        {
            const bool odd = (((i + j) & 1) != 0);

            if (debugFillEnabled)
            {
                rc2d_graphics_setColor(odd ? debugFillColorA : debugFillColorB);
                rc2d_graphics_drawTileIsometricAt("fill", i, j, originX, originY, tileWidth, tileHeight);
            }

            if (debugLinesEnabled)
            {
                rc2d_graphics_setColor(debugLineColor);
                rc2d_graphics_drawTileIsometricAt("line", i, j, originX, originY, tileWidth, tileHeight);
            }
        }
    }
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
