#include "game/entities/player.h"

#include <algorithm>

Player::Player(void)
    : hpCurrent(0),
      hpMax(0),
      viewRangeTiles(0.0f),
      moveSpeedTilesPerSecond(0.0f),
      ship{}
{
}

Player::~Player(void)
{
}

void Player::load(void)
{
    // Valeurs runtime de base du joueur au chargement de scene.
    this->hpCurrent = 100;
    this->hpMax = 100;
    this->moveSpeedTilesPerSecond = 4.0f;
    this->viewRangeTiles = 60.0f;

    // La vitesse logique du joueur est propagee au navire.
    this->ship.setSpeedTilesPerSecond(this->moveSpeedTilesPerSecond);
}

void Player::unload(void)
{
    this->unloadShip();
}

void Player::setViewRangeTiles(float value)
{
    if (value > 0.0f)
    {
        this->viewRangeTiles = value;
    }
}

float Player::getViewRangeTiles(void) const
{
    return this->viewRangeTiles;
}

bool Player::loadShip(const char* folderPath, RC2D_StorageKind storageKind)
{
    // Delegation directe au composant Ship (atlas + validation).
    return this->ship.loadSpritesFromFolder(folderPath, storageKind);
}

void Player::unloadShip(void)
{
    this->ship.unloadSprites();
}

void Player::spawnOnTile(const Map& map, int tileX, int tileY)
{
    // Clamp pour garantir un spawn legal meme si l'appelant donne
    // une tuile hors des bornes.
    const SDL_Point clamped = map.clampTile(tileX, tileY);
    this->ship.setPositionTileInt(clamped.x, clamped.y);
}

void Player::moveToTile(const Map& map, int tileX, int tileY)
{
    // Le pathfinding est gere dans Ship::moveToTile.
    this->ship.moveToTile(map, tileX, tileY);
}

void Player::update(double dt, const Map& map)
{
    // Le joueur n'a pas de logique complexe ici:
    // la simulation de mouvement est concentree dans Ship.
    this->ship.update(dt, map);
}

void Player::draw(const Map& map) const
{
    this->ship.draw(map);
}

bool Player::isMoving(void) const
{
    return this->ship.isMoving();
}

SDL_FPoint Player::getTilePosition(void) const
{
    return this->ship.getPositionTile();
}

SDL_FPoint Player::getTargetTile(void) const
{
    return this->ship.getTargetTile();
}

Ship& Player::getShip(void)
{
    return this->ship;
}

const Ship& Player::getShip(void) const
{
    return this->ship;
}

void Player::setMoveSpeedTilesPerSecond(float speed)
{
    // On ignore les vitesses non positives pour eviter un etat incoherent.
    if (speed <= 0.0f)
    {
        return;
    }

    this->moveSpeedTilesPerSecond = speed;
    this->ship.setSpeedTilesPerSecond(speed);
}

float Player::getMoveSpeedTilesPerSecond(void) const
{
    return this->moveSpeedTilesPerSecond;
}

void Player::setHpMax(int value)
{
    // On force un minimum de 1 HP max pour eviter un personnage "mort ne".
    if (value < 1)
    {
        value = 1;
    }

    this->hpMax = value;

    if (this->hpCurrent > this->hpMax)
    {
        this->hpCurrent = this->hpMax;
    }
}

void Player::setHpCurrent(int value)
{
    // Clamp des HP courants dans [0, hpMax].
    this->hpCurrent = std::clamp(value, 0, this->hpMax);
}

int Player::getHpCurrent(void) const
{
    return this->hpCurrent;
}

int Player::getHpMax(void) const
{
    return this->hpMax;
}

bool Player::isAlive(void) const
{
    return this->hpCurrent > 0;
}
