#pragma once

class Player;

/**
 * @brief Orchestrateur des shaders gameplay de la scene en jeu.
 *
 * Objectif:
 * - sortir la logique d'orchestration shader de GameScene;
 * - garder GameScene centree sur des appels haut niveau.
 */
class GameplayShaderController {
public:
    /**
     * @brief Charge tous les shaders gameplay.
     */
    static void loadAll(void);

    /**
     * @brief Decharge tous les shaders gameplay.
     */
    static void unloadAll(void);

    /**
     * @brief Met a jour les shaders de visibilite (nuages + fog).
     * @param dt Delta time en secondes.
     * @param player Joueur de reference (position + portee de vue).
     * @param fogOfWarEnabled True si le pass fog-of-war doit etre anime.
     */
    static void updateVisibility(double dt, const Player& player, bool fogOfWarEnabled);
};
