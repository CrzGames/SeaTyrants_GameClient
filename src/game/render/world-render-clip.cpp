#include "game/render/world-render-clip.h"

#include <algorithm>
#include <cmath>

#include <RC2D/RC2D.h>

SDL_Renderer* WorldRenderClip::begin(const SDL_FRect& worldRect)
{
    // Recupere le renderer SDL associe a la fenetre du jeu.
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        return nullptr;
    }

    // Conversion du rectangle monde (float) vers un rectangle entier (pixels).
    // - floor sur x/y pour englober tout pixel partiellement couvert en haut-gauche.
    // - ceil sur x+w / y+h pour englober tout pixel partiellement couvert en bas-droite.
    // Cela garantit qu'aucun pixel du monde ne sera coupe par le clip.
    SDL_Rect clipRect{};
    clipRect.x = static_cast<int>(std::floor(worldRect.x));
    clipRect.y = static_cast<int>(std::floor(worldRect.y));

    const int clipRight = static_cast<int>(std::ceil(worldRect.x + worldRect.w));
    const int clipBottom = static_cast<int>(std::ceil(worldRect.y + worldRect.h));

    // Largeur/hauteur min = 1 pour eviter un clip rect degenere (0x0).
    clipRect.w = (std::max)(clipRight - clipRect.x, 1);
    clipRect.h = (std::max)(clipBottom - clipRect.y, 1);

    // Active le clip rect sur le renderer:
    // tout dessin ulterieur sera restreint a cette zone pixel.
    SDL_SetRenderClipRect(renderer, &clipRect);

    return renderer;
}

void WorldRenderClip::end(SDL_Renderer* renderer)
{
    if (renderer != nullptr)
    {
        // Passer nullptr desactive le clip rect:
        // le renderer retrouve sa zone de dessin complete.
        SDL_SetRenderClipRect(renderer, nullptr);
    }
}
