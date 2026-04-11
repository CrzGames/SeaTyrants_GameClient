#include "game/render/world-render-clip.h"

#include <algorithm>
#include <cmath>

#include "core/context.h"

#include <RC2D/RC2D.h>

SDL_Texture* g_worldTarget = nullptr;
int g_targetW = 0;
int g_targetH = 0;

SDL_Texture* g_savedRenderTarget = nullptr;
SDL_Rect g_savedViewport{};
bool g_savedHadViewport = false;

SDL_FRect g_compositeWorldRect{};
bool g_rttPassActive = false;

static bool queryLogicalOutputPixels(SDL_Renderer* renderer, int* outW, int* outH)
{
    int lw = 0;
    int lh = 0;
    SDL_RendererLogicalPresentation pres{};
    const bool haveLogical =
        SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &pres) && lw > 0 && lh > 0;

    const SDL_FRect& g = GetGameScreen().rect;
    const int gw = static_cast<int>(std::ceil(static_cast<double>(g.x + g.w)));
    const int gh = static_cast<int>(std::ceil(static_cast<double>(g.y + g.h)));

    if (haveLogical)
    {
        *outW = (std::max)(lw, gw);
        *outH = (std::max)(lh, gh);
    }
    else
    {
        *outW = (std::max)(gw, 1);
        *outH = (std::max)(gh, 1);
    }
    return true;
}

static bool ensureWorldTarget(SDL_Renderer* renderer, int reqW, int reqH)
{
    if (reqW < 1 || reqH < 1)
    {
        return false;
    }

    if (g_worldTarget != nullptr && g_targetW == reqW && g_targetH == reqH)
    {
        return true;
    }

    if (g_worldTarget != nullptr)
    {
        SDL_DestroyTexture(g_worldTarget);
        g_worldTarget = nullptr;
        g_targetW = 0;
        g_targetH = 0;
    }

    g_worldTarget = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        reqW,
        reqH);
    if (g_worldTarget == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "WorldRenderClip: SDL_CreateTexture(TARGET %dx%d) failed: %s", reqW, reqH, SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(g_worldTarget, SDL_BLENDMODE_BLEND);
    g_targetW = reqW;
    g_targetH = reqH;
    return true;
}

SDL_Renderer* WorldRenderClip::begin(const SDL_FRect& worldRect)
{
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        return nullptr;
    }

    int lw = 0;
    int lh = 0;
    if (!queryLogicalOutputPixels(renderer, &lw, &lh))
    {
        return nullptr;
    }

    if (!ensureWorldTarget(renderer, lw, lh))
    {
        return nullptr;
    }

    g_savedRenderTarget = SDL_GetRenderTarget(renderer);
    g_savedHadViewport = SDL_RenderViewportSet(renderer);
    if (g_savedHadViewport)
    {
        if (!SDL_GetRenderViewport(renderer, &g_savedViewport))
        {
            g_savedHadViewport = false;
        }
    }

    if (!SDL_SetRenderTarget(renderer, g_worldTarget))
    {
        RC2D_log(RC2D_LOG_ERROR, "WorldRenderClip: SDL_SetRenderTarget failed: %s", SDL_GetError());
        return nullptr;
    }

    if (!SDL_SetRenderViewport(renderer, nullptr))
    {
        RC2D_log(RC2D_LOG_WARN, "WorldRenderClip: SDL_SetRenderViewport(nullptr) failed: %s", SDL_GetError());
    }
    SDL_SetRenderClipRect(renderer, nullptr);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!SDL_RenderClear(renderer))
    {
        RC2D_log(RC2D_LOG_WARN, "WorldRenderClip: SDL_RenderClear failed: %s", SDL_GetError());
    }

    g_compositeWorldRect = worldRect;
    g_rttPassActive = true;
    return renderer;
}

void WorldRenderClip::end(SDL_Renderer* renderer)
{
    if (renderer == nullptr || !g_rttPassActive || g_worldTarget == nullptr)
    {
        g_rttPassActive = false;
        return;
    }

    SDL_FlushRenderer(renderer);
    SDL_SetGPURenderState(renderer, nullptr);

    if (!SDL_SetRenderTarget(renderer, g_savedRenderTarget))
    {
        RC2D_log(RC2D_LOG_ERROR, "WorldRenderClip: restore SDL_SetRenderTarget failed: %s", SDL_GetError());
        g_rttPassActive = false;
        return;
    }

    if (g_savedHadViewport)
    {
        SDL_SetRenderViewport(renderer, &g_savedViewport);
    }
    else
    {
        SDL_SetRenderViewport(renderer, nullptr);
    }
    SDL_SetRenderClipRect(renderer, nullptr);

    const SDL_FRect src = g_compositeWorldRect;
    const SDL_FRect dst = g_compositeWorldRect;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (!SDL_RenderTexture(renderer, g_worldTarget, &src, &dst))
    {
        RC2D_log(RC2D_LOG_WARN, "WorldRenderClip: SDL_RenderTexture composite failed: %s", SDL_GetError());
    }

    g_rttPassActive = false;
}

void WorldRenderClip::destroy(void)
{
    if (g_worldTarget != nullptr)
    {
        SDL_DestroyTexture(g_worldTarget);
        g_worldTarget = nullptr;
        g_targetW = 0;
        g_targetH = 0;
    }
    g_rttPassActive = false;
}
