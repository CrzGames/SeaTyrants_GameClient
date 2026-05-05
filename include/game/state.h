#pragma once

#include <vector>

#include "game/entities/player.h"
#include "game/vfx/vfx-ship.h"

class Ship;

/**
 * @brief Definit quel navire porte visuellement le VFX ship en jeu.
 *
 * `ATTACKER` :
 * le VFX est dessine sur le navire attaquant (ex. flash de bouche au depart).
 *
 * `TARGET` :
 * le VFX est dessine sur le navire cible (ex. impact de boulets sur la coque).
 */
enum class GameplayVfxShipAnchorRole {
    ATTACKER = 0,
    TARGET = 1
};

/**
 * @brief Entree runtime d'un VFX navire gameplay place dans @ref GameState::vfxShips.
 *
 * Le slot conserve:
 * - l'instance @ref VFXShip a animer/dessiner ;
 * - le duo attaquant/cible de la salve qui a produit ce VFX ;
 * - quel navire sert d'ancrage visuel pour le draw.
 *
 * Ce contexte permet a @ref scene-game et au systeme de salve de recalculer
 * correctement la page active du VFX (direction, et eventuel mode `TARGET_RELATIVE_AB`)
 * sans re-deviner quel navire est le porteur reel de l'effet.
 */
struct GameplayVfxShipSlot {
    VFXShip vfx; /**< Lecteur/runtime du VFX navire. */
    Ship* attackerShip = nullptr; /**< Navire qui a tire / declenche la salve source. */
    Ship* targetShip = nullptr;   /**< Navire vise par la salve source. */
    GameplayVfxShipAnchorRole anchorRole = GameplayVfxShipAnchorRole::ATTACKER;
};

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
    /** VFX complexes lies aux navires (ex.: flash canon maritime pousse au tir par la salve). */
    std::vector<GameplayVfxShipSlot> vfxShips;

    /**
     * @brief Constructeur.
     */
    GameState(void)
        : player{},
          npcs{},
          monsters{},
          otherPlayers{},
          scintilles{},
          vfxShips{}
    {
    }

    /**
     * @brief Destructeur.
     */
    ~GameState(void) = default;

    /**
     * @brief Decharge les VFX navire gameplay et vide le vecteur.
     *
     * Les sprites boulets / salve restent dans @ref MaritimeCannonSalvoSystem (clear separe).
     */
    void clearGameplayVfx(void)
    {
        for (GameplayVfxShipSlot& slot : this->vfxShips)
        {
            slot.vfx.unload();
        }
        this->vfxShips.clear();
    }
};
