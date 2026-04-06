#include "game/controllers/gameplay-camera-controller.h"

#include <RC2D/RC2D.h>

#include "game/camera.h"
#include "game/entities/player.h"
#include "game/map/map.h"

bool GameplayCameraController::updateKeyboardScroll(double dt, Camera& camera, const Map& map, const SDL_FRect& viewportRect)
{
    const bool upPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_UP);
    const bool downPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_DOWN);
    const bool leftPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LEFT);
    const bool rightPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RIGHT);

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
        Camera::CAMERA_SCROLL_SPEED_SECTORS * static_cast<float>(dt);

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
