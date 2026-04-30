#pragma once

#include <vector>

#include "game/entities/player.h"
#include "game/map/map.h"
#include "game/vfx/vfx-classic.h"
#include "game/vfx/vfx-ship.h"

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
    std::vector<Player> scintilles;   /**< Scintilles (types a affiner plus tard). */
    std::vector<VFXShip> vfxShips;    /**< VFX complexes lies aux navires (gameplay). */
    std::vector<VFXClassic> vfxClassics; /**< VFX spritesheet simples (gameplay). */

    /**
     * @brief Constructeur.
     */
    GameState(void)
        : player{},
          npcs{},
          monsters{},
          otherPlayers{},
          scintilles{},
          vfxShips{},
          vfxClassics{}
    {
    }

    /**
     * @brief Destructeur.
     */
    ~GameState(void) = default;
};
