#pragma once

#include <SDL3/SDL.h>

class Camera;
class Map;
class Player;

/**
 * @brief Controleur de camera gameplay (hors UI).
 *
 * Regroupe les comportements de navigation camera utilises
 * par la scene de jeu.
 */
class GameplayCameraController {
public:
    /**
     * @brief Met a jour le scroll clavier (fleches).
     * @param dt Delta time en secondes.
     * @param camera Camera a deplacer.
     * @param map Map de reference.
     * @param viewportRect Rectangle de rendu de la map.
     * @return True si la camera a ete deplacee.
     */
    static bool updateKeyboardScroll(double dt, Camera& camera, const Map& map, const SDL_FRect& viewportRect);

    /**
     * @brief Met a jour le scroll clavier avec un mapping et une vitesse configurables.
     */
    static bool updateKeyboardScroll(
        double dt,
        Camera& camera,
        const Map& map,
        const SDL_FRect& viewportRect,
        SDL_Scancode upScancode,
        SDL_Scancode downScancode,
        SDL_Scancode leftScancode,
        SDL_Scancode rightScancode,
        float scrollSpeedSectors);

    /**
     * @brief Centre la camera sur le joueur.
     * @param camera Camera a centrer.
     * @param map Map de reference.
     * @param viewportRect Rectangle de rendu de la map.
     * @param player Joueur a suivre.
     */
    static void centerOnPlayer(Camera& camera, const Map& map, const SDL_FRect& viewportRect, const Player& player);
};
