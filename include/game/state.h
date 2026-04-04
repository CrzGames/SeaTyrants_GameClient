#pragma once

#include <vector>

#include "game/entities/player.h"
#include "game/map/map.h"

/**
 * @brief Etat runtime global du gameplay.
 *
 * Cette structure centralise les entites qui vivront
 * au-dela d'une seule scene.
 */
class GameState {
public:
    Player player;                    /**< Joueur local. */
    std::vector<Player> npcs;         /**< Liste des NPC (base temporaire Player). */
    std::vector<Player> monsters;     /**< Liste des monstres (base temporaire Player). */
    std::vector<Player> otherPlayers; /**< Liste des autres joueurs. */

    /**
     * @brief Constructeur.
     */
    GameState(void)
        : player{},
          npcs{},
          monsters{},
          otherPlayers{}
    {
    }

    /**
     * @brief Destructeur.
     */
    ~GameState(void) = default;
};
