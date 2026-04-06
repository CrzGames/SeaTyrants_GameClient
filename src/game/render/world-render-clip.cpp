#include "game/render/world-render-clip.h"

#include <algorithm>
#include <cmath>

#include <RC2D/RC2D.h>

SDL_Renderer* WorldRenderClip::begin(const SDL_FRect& worldRect)
{
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        return nullptr;
    }

    SDL_Rect clipRect{};
    clipRect.x = static_cast<int>(std::floor(worldRect.x));
    clipRect.y = static_cast<int>(std::floor(worldRect.y));
    const int clipRight = static_cast<int>(std::ceil(worldRect.x + worldRect.w));
    const int clipBottom = static_cast<int>(std::ceil(worldRect.y + worldRect.h));
    clipRect.w = (std::max)(clipRight - clipRect.x, 1);
    clipRect.h = (std::max)(clipBottom - clipRect.y, 1);

    SDL_SetRenderClipRect(renderer, &clipRect);
    return renderer;
}

void WorldRenderClip::end(SDL_Renderer* renderer)
{
    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }
}
