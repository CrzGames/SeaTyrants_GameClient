#include "game/entities/player.h"

#include <algorithm>

Player::Player(void)
    : hpCurrent(100),
      hpMax(100),
      moveSpeedTilesPerSecond(3.0f),
      ship{}
{
    ship.setSpeedTilesPerSecond(moveSpeedTilesPerSecond);
}

Player::~Player(void)
{
    unloadShip();
}

bool Player::loadShip(const char* folderPath, RC2D_StorageKind storageKind)
{
    return ship.loadSpritesFromFolder(folderPath, storageKind);
}

void Player::unloadShip(void)
{
    ship.unloadSprites();
}

void Player::spawnOnTile(const Map& map, int tileX, int tileY)
{
    const SDL_Point clamped = map.clampTile(tileX, tileY);
    ship.setPositionTileInt(clamped.x, clamped.y);
}

void Player::moveToTile(const Map& map, int tileX, int tileY)
{
    ship.moveToTile(map, tileX, tileY);
}

void Player::update(double dt, const Map& map)
{
    ship.update(dt, map);
}

void Player::draw(const Map& map) const
{
    ship.draw(map);
}

bool Player::isMoving(void) const
{
    return ship.isMoving();
}

SDL_FPoint Player::getTilePosition(void) const
{
    return ship.getPositionTile();
}

SDL_FPoint Player::getTargetTile(void) const
{
    return ship.getTargetTile();
}

Ship& Player::getShip(void)
{
    return ship;
}

const Ship& Player::getShip(void) const
{
    return ship;
}

void Player::setMoveSpeedTilesPerSecond(float speed)
{
    if (speed <= 0.0f)
    {
        return;
    }

    moveSpeedTilesPerSecond = speed;
    ship.setSpeedTilesPerSecond(speed);
}

float Player::getMoveSpeedTilesPerSecond(void) const
{
    return moveSpeedTilesPerSecond;
}

void Player::setHpMax(int value)
{
    if (value < 1)
    {
        value = 1;
    }

    hpMax = value;

    if (hpCurrent > hpMax)
    {
        hpCurrent = hpMax;
    }
}

void Player::setHpCurrent(int value)
{
    hpCurrent = std::clamp(value, 0, hpMax);
}

int Player::getHpCurrent(void) const
{
    return hpCurrent;
}

int Player::getHpMax(void) const
{
    return hpMax;
}

bool Player::isAlive(void) const
{
    return hpCurrent > 0;
}
