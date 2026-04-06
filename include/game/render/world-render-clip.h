#pragma once

#include <SDL3/SDL.h>

/**
 * @brief Helper de clip du rendu monde (gameplay) dans le rect map.
 */
class WorldRenderClip {
public:
    /**
     * @brief Active le clip renderer dans le rectangle monde.
     * @param worldRect Rectangle logique de la map.
     * @return Renderer utilise (nullptr si indisponible).
     */
    static SDL_Renderer* begin(const SDL_FRect& worldRect);

    /**
     * @brief Desactive le clip renderer precedent.
     * @param renderer Renderer retourne par begin().
     */
    static void end(SDL_Renderer* renderer);
};
