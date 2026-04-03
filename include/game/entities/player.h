#pragma once

#include "game/map/map.h"
#include "game/ships/ship.h"

/**
 * @brief Entite joueur minimale pour la scene gameplay.
 *
 * Cette classe encapsule:
 * - les stats principales (HP, vitesse);
 * - le navire equipe;
 * - les helpers de spawn, deplacement, update et draw.
 */
class Player {
private:
    int hpCurrent;                 /**< HP actuels du joueur. */
    int hpMax;                     /**< HP max du joueur. */
    float moveSpeedTilesPerSecond; /**< Vitesse logique en tuiles/s. */
    Ship ship;                     /**< Navire actuellement equipe. */

public:
    /**
     * @brief Constructeur du joueur.
     */
    Player(void);

    /**
     * @brief Destructeur du joueur.
     */
    ~Player(void);

    /**
     * @brief Charge les sprites du navire depuis un dossier atlas.
     * @param folderPath Dossier contenant 1.png..8.png.
     * @param storageKind Storage RC2D (TITLE/USER).
     * @return True si le navire est charge.
     */
    bool loadShip(const char* folderPath, RC2D_StorageKind storageKind);

    /**
     * @brief Decharge les ressources du navire.
     */
    void unloadShip(void);

    /**
     * @brief Spawn le joueur sur une tuile.
     * @param map Map de gameplay.
     * @param tileX Tuile spawn X.
     * @param tileY Tuile spawn Y.
     */
    void spawnOnTile(const Map& map, int tileX, int tileY);

    /**
     * @brief Lance un deplacement vers une tuile cible.
     * @param map Map de navigation.
     * @param tileX Tuile cible X.
     * @param tileY Tuile cible Y.
     */
    void moveToTile(const Map& map, int tileX, int tileY);

    /**
     * @brief Met a jour le joueur et son navire.
     * @param dt Delta time en secondes.
     * @param map Map de navigation.
     */
    void update(double dt, const Map& map);

    /**
     * @brief Dessine le navire du joueur.
     * @param map Map utilisee pour la projection ecran.
     */
    void draw(const Map& map) const;

    /**
     * @brief Indique si le navire du joueur est en mouvement.
     * @return True si un deplacement est actif.
     */
    bool isMoving(void) const;

    /**
     * @brief Retourne la position tuile courante du joueur.
     * @return Position tuile flottante.
     */
    SDL_FPoint getTilePosition(void) const;

    /**
     * @brief Retourne la cible tuile courante du joueur.
     * @return Cible tuile flottante.
     */
    SDL_FPoint getTargetTile(void) const;

    /**
     * @brief Retourne le navire mutable du joueur.
     * @return Reference mutable sur le navire.
     */
    Ship& getShip(void);

    /**
     * @brief Retourne le navire en lecture seule.
     * @return Reference const sur le navire.
     */
    const Ship& getShip(void) const;

    /**
     * @brief Definit la vitesse de deplacement du joueur.
     * @param speed Vitesse en tuiles/s.
     */
    void setMoveSpeedTilesPerSecond(float speed);

    /**
     * @brief Retourne la vitesse de deplacement du joueur.
     * @return Vitesse en tuiles/s.
     */
    float getMoveSpeedTilesPerSecond(void) const;

    /**
     * @brief Definit les HP max du joueur.
     * @param value HP max.
     */
    void setHpMax(int value);

    /**
     * @brief Definit les HP actuels du joueur.
     * @param value HP courants.
     */
    void setHpCurrent(int value);

    /**
     * @brief Retourne les HP actuels du joueur.
     * @return HP actuels.
     */
    int getHpCurrent(void) const;

    /**
     * @brief Retourne les HP max du joueur.
     * @return HP max.
     */
    int getHpMax(void) const;

    /**
     * @brief Indique si le joueur est vivant.
     * @return True si HP > 0.
     */
    bool isAlive(void) const;
};
