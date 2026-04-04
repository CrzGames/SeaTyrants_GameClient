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
    // Etat initial volontairement simple:
    // - grille 64x64
    // - tuile iso 48x32
    // - aucune collision active.
}

Map::~Map(void)
{
}

void Map::setMapSize(int width, int height)
{
    // Etape 1: normaliser les dimensions pour garantir une map valide.
    // Garde-fous sur dimensions minimales.
    if (width < 1)
    {
        width = 1;
    }

    if (height < 1)
    {
        height = 1;
    }

    this->widthTiles = width;
    this->heightTiles = height;

    // Etape 2: quand la taille change, on recree la grille de collisions
    // avec la nouvelle taille. Tout repart en "traversable" par defaut.
    // On reset la couche collision  traversable.
    this->blockedTiles.assign(static_cast<size_t>(this->widthTiles * this->heightTiles), static_cast<Uint8>(0));
}

void Map::setTileSize(float width, float height)
{
    // On refuse une taille invalide pour eviter des divisions par zero
    // dans les conversions tile <-> ecran.
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    this->tileWidth = width;
    this->tileHeight = height;
}

void Map::setOrigin(float x, float y)
{
    this->originX = x;
    this->originY = y;
}

void Map::centerOnRect(const SDL_FRect& rect)
{
    // Demi dimensions d'une tuile iso.
    const float halfTileW = this->tileWidth * 0.5f;
    const float halfTileH = this->tileHeight * 0.5f;

    // Bornes "visuelles" du losange global de la map en espace ecran.
    const float minCenterX = -(static_cast<float>(this->heightTiles - 1) * halfTileW);
    const float maxCenterX =  (static_cast<float>(this->widthTiles - 1) * halfTileW);
    const float minCenterY = 0.0f;
    const float maxCenterY = static_cast<float>(this->widthTiles + this->heightTiles - 2) * halfTileH;

    // Centre du losange map.
    const float mapCenterX = (minCenterX + maxCenterX) * 0.5f;
    const float mapCenterY = (minCenterY + maxCenterY) * 0.5f;

    // Translation de l'origine pour superposer le centre map
    // avec le centre du rectangle cible.
    this->originX = (rect.x + (rect.w * 0.5f)) - mapCenterX;
    this->originY = (rect.y + (rect.h * 0.5f)) - mapCenterY;
}

SDL_FPoint Map::tileToScreenCenter(int tileX, int tileY) const
{
    return tileToScreenCenterFloat(static_cast<float>(tileX), static_cast<float>(tileY));
}

SDL_FPoint Map::tileToScreenCenterFloat(float tileX, float tileY) const
{
    // Formule isometrique standard:
    // screenX = originX + (tileX - tileY) * halfW
    // screenY = originY + (tileX + tileY) * halfH
    const float halfTileW = this->tileWidth * 0.5f;
    const float halfTileH = this->tileHeight * 0.5f;

    SDL_FPoint result = {};
    result.x = this->originX + ((tileX - tileY) * halfTileW);
    result.y = this->originY + ((tileX + tileY) * halfTileH);
    return result;
}

SDL_FPoint Map::screenToTile(float screenX, float screenY) const
{
    // Transformation inverse de la projection isometrique.
    const float halfTileW = this->tileWidth * 0.5f;
    const float halfTileH = this->tileHeight * 0.5f;

    SDL_FPoint result = {};

    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return result;
    }

    const float dx = (screenX - this->originX) / halfTileW;
    const float dy = (screenY - this->originY) / halfTileH;

    // Inversion du systeme lineaire utilise dans tileToScreenCenterFloat.
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
    // On arrondit vers la tuile la plus proche, utile pour les clics.
    SDL_Point tile = {};
    tile.x = static_cast<int>(std::lround(static_cast<double>(tileX)));
    tile.y = static_cast<int>(std::lround(static_cast<double>(tileY)));
    return tile;
}

SDL_Point Map::clampTile(int tileX, int tileY) const
{
    // Toujours renvoyer une tuile valide tant que la map existe.
    SDL_Point result = {};

    if (this->widthTiles <= 0 || this->heightTiles <= 0)
    {
        return result;
    }

    tileX = std::clamp(tileX, 0, this->widthTiles - 1);
    tileY = std::clamp(tileY, 0, this->heightTiles - 1);

    result.x = tileX;
    result.y = tileY;
    return result;
}

bool Map::isInside(int tileX, int tileY) const
{
    return (tileX >= 0 && tileX < this->widthTiles && tileY >= 0 && tileY < this->heightTiles);
}

bool Map::setTileBlocked(int tileX, int tileY, bool blocked)
{
    // Ecriture protegee: on refuse une tuile hors map.
    if (!this->isInside(tileX, tileY))
    {
        return false;
    }

    this->blockedTiles[static_cast<size_t>(this->tileIndex(tileX, tileY))] = blocked ? static_cast<Uint8>(1) : static_cast<Uint8>(0);
    return true;
}

bool Map::isTileBlocked(int tileX, int tileY) const
{
    // Politique defensive:
    // une tuile hors map est consideree comme bloquee.
    if (!this->isInside(tileX, tileY))
    {
        return true;
    }

    return this->blockedTiles[static_cast<size_t>(this->tileIndex(tileX, tileY))] != 0;
}

void Map::clearBlockedTiles(void)
{
    // Reset complet de la couche collision.
    for (size_t i = 0; i < this->blockedTiles.size(); ++i)
    {
        this->blockedTiles[i] = static_cast<Uint8>(0);
    }
}

int Map::tileIndex(int tileX, int tileY) const
{
    return (tileY * this->widthTiles) + tileX;
}

int Map::getWidthTiles(void) const
{
    return this->widthTiles;
}

int Map::getHeightTiles(void) const
{
    return this->heightTiles;
}

float Map::getTileWidth(void) const
{
    return this->tileWidth;
}

float Map::getTileHeight(void) const
{
    return this->tileHeight;
}

float Map::getOriginX(void) const
{
    return this->originX;
}

float Map::getOriginY(void) const
{
    return this->originY;
}
