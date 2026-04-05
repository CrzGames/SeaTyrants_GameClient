#include "game/camera.h"

#include <algorithm>

#include "game/map/map.h"

Camera::Camera()
    : cameraTileX(0.0f),
      cameraTileY(0.0f),
      zoomFactor(1.0f),
      minZoomFactor(0.40f),
      maxZoomFactor(1.00f)
{
}

Camera::~Camera()
{
}

void Camera::resetCamera(const Map& map, const SDL_FRect& viewportRect)
{
    this->cameraTileX = static_cast<float>(map.getWidthTiles() - 1) * 0.5f;
    this->cameraTileY = static_cast<float>(map.getHeightTiles() - 1) * 0.5f;

    this->clampCameraToMap(map, viewportRect);
}

void Camera::centerCameraOnTile(float tileX, float tileY, const Map& map, const SDL_FRect& viewportRect)
{
    this->cameraTileX = tileX;
    this->cameraTileY = tileY;

    this->clampCameraToMap(map, viewportRect);
}

void Camera::moveCameraTiles(float deltaTileX, float deltaTileY, const Map& map, const SDL_FRect& viewportRect)
{
    this->cameraTileX += deltaTileX;
    this->cameraTileY += deltaTileY;

    this->clampCameraToMap(map, viewportRect);
}

void Camera::setZoomFactor(float value)
{
    this->zoomFactor = std::clamp(value, this->minZoomFactor, this->maxZoomFactor);
}

float Camera::getZoomFactor(void) const
{
    return this->zoomFactor;
}

void Camera::applyToMap(Map& map, const SDL_FRect& viewportRect)
{
    // La map lit directement le zoom courant via la camera globale.
    // On applique donc seulement le clamp + recentrage.
    this->clampCameraToMap(map, viewportRect);
    map.centerOnTileInRect(this->cameraTileX, this->cameraTileY, viewportRect);
}

void Camera::clampCameraToMap(const Map& map, const SDL_FRect& viewportRect)
{
    const int mapWidth = map.getWidthTiles();
    const int mapHeight = map.getHeightTiles();

    if (mapWidth <= 0 || mapHeight <= 0)
    {
        this->cameraTileX = 0.0f;
        this->cameraTileY = 0.0f;
        return;
    }

    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;

    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        this->cameraTileX = 0.0f;
        this->cameraTileY = 0.0f;
        return;
    }

    // Espace isometrique (u, v):
    // u = tileX - tileY  -> axe horizontal ecran
    // v = tileX + tileY  -> axe vertical ecran
    const float minU =
        static_cast<float>(Map::SECTOR_BASE_X - Map::SECTOR_BASE_Y);

    const float maxU =
        minU + static_cast<float>((Map::NUM_SECTORS_X - 1) * 2 * Map::SECTOR_STEP);

    const float minV =
        static_cast<float>(Map::SECTOR_BASE_X + Map::SECTOR_BASE_Y);

    const float maxV =
        minV + static_cast<float>((Map::NUM_SECTORS_Y - 1) * 2 * Map::SECTOR_STEP);

    const float halfViewU = (viewportRect.w * 0.5f) / halfTileW;
    const float halfViewV = (viewportRect.h * 0.5f) / halfTileH;

    float cameraU = this->cameraTileX - this->cameraTileY;
    float cameraV = this->cameraTileX + this->cameraTileY;

    const float allowedMinU = minU + halfViewU;
    const float allowedMaxU = maxU - halfViewU;
    const float allowedMinV = minV + halfViewV;
    const float allowedMaxV = maxV - halfViewV;

    if (allowedMinU <= allowedMaxU)
    {
        cameraU = std::clamp(cameraU, allowedMinU, allowedMaxU);
    }
    else
    {
        cameraU = (minU + maxU) * 0.5f;
    }

    if (allowedMinV <= allowedMaxV)
    {
        cameraV = std::clamp(cameraV, allowedMinV, allowedMaxV);
    }
    else
    {
        cameraV = (minV + maxV) * 0.5f;
    }

    this->cameraTileX = (cameraU + cameraV) * 0.5f;
    this->cameraTileY = (cameraV - cameraU) * 0.5f;

    // Dernier garde-fou: on reste dans les bornes logiques de la grille.
    this->cameraTileX = std::clamp(this->cameraTileX, 0.0f, static_cast<float>(mapWidth - 1));
    this->cameraTileY = std::clamp(this->cameraTileY, 0.0f, static_cast<float>(mapHeight - 1));
}
