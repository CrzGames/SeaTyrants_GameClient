#pragma once

#include <SDL3/SDL.h>

/**
 * @brief Rendu du monde gameplay dans une render target puis composite sur l'ecran.
 *
 * Evite SDL_SetRenderClipRect (problematique avec GPU + HUD sur certaines plateformes)
 * tout en empechant navire / VFX de deborder visuellement dans les bandeaux GUI:
 * seule la region map.rect est copiee depuis la texture vers la cible par defaut.
 */
class WorldRenderClip {
public:
    /**
     * @brief Passe la cible de rendu sur une texture pleine taille (coordonnees logiques).
     * @param worldRect Zone map (meme repere qu'avant); sert au composite dans end().
     * @return Renderer pour end(), ou nullptr si indisponible / echec texture.
     */
    static SDL_Renderer* begin(const SDL_FRect& worldRect);

    /**
     * @brief Restaure la cible par defaut et compose la zone worldRect depuis la RTT.
     * @param renderer Renderer retourne par begin().
     */
    static void end(SDL_Renderer* renderer);

    /**
     * @brief Libere la texture RTT (appeler au shutdown, ex. rc2d_unload).
     */
    static void destroy(void);
};
