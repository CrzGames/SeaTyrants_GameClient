#include "game/controllers/gameplay-camera-controller.h"

#include <RC2D/RC2D.h>

#include <algorithm>

#include "game/camera.h"
#include "game/entities/player.h"
#include "game/map/map.h"

static bool isConfiguredScancodeDown(SDL_Scancode scancode)
{
    if (scancode == SDL_SCANCODE_UNKNOWN)
    {
        return false;
    }

    return rc2d_keyboard_isScancodeDown((RC2D_Scancode)scancode);
}

bool GameplayCameraController::updateKeyboardScroll(double dt, Camera& camera, const Map& map, const SDL_FRect& viewportRect)
{
    return GameplayCameraController::updateKeyboardScroll(
        dt,
        camera,
        map,
        viewportRect,
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        Camera::CAMERA_SCROLL_SPEED_SECTORS);
}

bool GameplayCameraController::updateKeyboardScroll(
    double dt,
    Camera& camera,
    const Map& map,
    const SDL_FRect& viewportRect,
    SDL_Scancode upScancode,
    SDL_Scancode downScancode,
    SDL_Scancode leftScancode,
    SDL_Scancode rightScancode,
    float scrollSpeedSectors)
{
    const bool upPressed =
        isConfiguredScancodeDown(upScancode);
    const bool downPressed =
        isConfiguredScancodeDown(downScancode);
    const bool leftPressed =
        isConfiguredScancodeDown(leftScancode);
    const bool rightPressed =
        isConfiguredScancodeDown(rightScancode);

    float deltaSectorX = 0.0f;
    float deltaSectorY = 0.0f;

    if (upPressed)
    {
        deltaSectorY -= 1.0f;
    }
    if (downPressed)
    {
        deltaSectorY += 1.0f;
    }
    if (leftPressed)
    {
        deltaSectorX -= 1.0f;
    }
    if (rightPressed)
    {
        deltaSectorX += 1.0f;
    }

    if (deltaSectorX == 0.0f && deltaSectorY == 0.0f)
    {
        return false;
    }

    // Normalisation des diagonales pour garder la meme vitesse
    // que les directions simples.
    if (deltaSectorX != 0.0f && deltaSectorY != 0.0f)
    {
        deltaSectorX *= Camera::CAMERA_DIAGONAL_FACTOR;
        deltaSectorY *= Camera::CAMERA_DIAGONAL_FACTOR;
    }

    const float sectorDistance =
        (std::max)(0.0f, scrollSpeedSectors) * static_cast<float>(dt);

    deltaSectorX *= sectorDistance;
    deltaSectorY *= sectorDistance;

    const float deltaTileX =
        (deltaSectorX + deltaSectorY) * static_cast<float>(Map::SECTOR_STEP);
    const float deltaTileY =
        (deltaSectorY - deltaSectorX) * static_cast<float>(Map::SECTOR_STEP);

    camera.moveCameraTiles(deltaTileX, deltaTileY, map, viewportRect);
    return true;
}

void GameplayCameraController::centerOnPlayer(Camera& camera, const Map& map, const SDL_FRect& viewportRect, const Player& player)
{
    const SDL_FPoint playerTile = player.getTilePosition();
    camera.centerCameraOnTile(playerTile.x, playerTile.y, map, viewportRect);
}
