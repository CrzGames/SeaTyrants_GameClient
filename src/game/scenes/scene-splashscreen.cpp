#include "game/scenes/scene-splashscreen.h"

#include "game/scenes/scene-manager.h"

namespace {
double Clamp01(double value)
{
    if (value < 0.0)
    {
        return 0.0;
    }

    if (value > 1.0)
    {
        return 1.0;
    }

    return value;
}
} // namespace

SplashScreenScene::SplashScreenScene(void)
    : splashStudioVideo{},
      splashGameVideo{},
      splashState(SPLASH_STUDIO)
{
}

void SplashScreenScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    const double alphaClamped = Clamp01(alpha01);
    if (alphaClamped <= 0.0)
    {
        return;
    }

    SDL_FRect rect = rc2d_engine_getVisibleSafeRectRender();
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor({0, 0, 0, static_cast<Uint8>(alphaClamped * 255.0)});
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void SplashScreenScene::finishAndGoToMenu(void)
{
    if (splashState == SPLASH_DONE)
    {
        return;
    }

    rc2d_video_close(&splashStudioVideo);
    rc2d_video_close(&splashGameVideo);
    splashState = SPLASH_DONE;

    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("menu");
    }
}

void SplashScreenScene::unload(void)
{
    rc2d_video_close(&splashStudioVideo);
    rc2d_video_close(&splashGameVideo);
    splashState = SPLASH_STUDIO;

    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Unloaded\n");
}

void SplashScreenScene::load(void)
{
    rc2d_video_close(&splashStudioVideo);
    rc2d_video_close(&splashGameVideo);
    splashState = SPLASH_STUDIO;

    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Loaded\n");
}

void SplashScreenScene::update(double dt)
{
    switch (splashState)
    {
        case SPLASH_STUDIO:
        {
            if (splashStudioVideo.format_ctx == nullptr)
            {
                if (rc2d_video_openFromStorage(
                        &splashStudioVideo,
                        "assets/videos/splashscreen-studio-1080p.mp4",
                        RC2D_STORAGE_TITLE) != 0)
                {
                    RC2D_log(RC2D_LOG_WARN, "Failed to open studio splash video, skipping.");
                    splashState = SPLASH_GAME;
                    return;
                }
            }

            if (rc2d_video_update(&splashStudioVideo, dt) <= 0)
            {
                rc2d_video_close(&splashStudioVideo);
                splashState = SPLASH_GAME;
            }
            break;
        }

        case SPLASH_GAME:
        {
            if (splashGameVideo.format_ctx == nullptr)
            {
                if (rc2d_video_openFromStorage(
                        &splashGameVideo,
                        "assets/videos/splashscreen-seatyrants-1080p.mp4",
                        RC2D_STORAGE_TITLE) != 0)
                {
                    RC2D_log(RC2D_LOG_WARN, "Failed to open game splash video, skipping.");
                    finishAndGoToMenu();
                    return;
                }
            }

            if (rc2d_video_update(&splashGameVideo, dt) <= 0)
            {
                finishAndGoToMenu();
            }
            break;
        }

        case SPLASH_DONE:
        default:
            break;
    }
}

void SplashScreenScene::draw(void)
{
    if (splashState == SPLASH_STUDIO)
    {
        if (splashStudioVideo.format_ctx != nullptr)
        {
            rc2d_video_draw(&splashStudioVideo);

            const double total = rc2d_video_totalSeconds(&splashStudioVideo);
            const double now = rc2d_video_currentSeconds(&splashStudioVideo);
            if (total > 0.0)
            {
                const double remaining = total - now;
                if (remaining <= kFadeSeconds)
                {
                    const double alpha = 1.0 - (remaining / kFadeSeconds);
                    drawFullscreenBlackWithAlpha(alpha);
                }
            }
        }
        return;
    }

    if (splashState == SPLASH_GAME)
    {
        if (splashGameVideo.format_ctx != nullptr)
        {
            rc2d_video_draw(&splashGameVideo);

            const double now = rc2d_video_currentSeconds(&splashGameVideo);
            if (now < kFadeSeconds)
            {
                const double alpha = 1.0 - (now / kFadeSeconds);
                drawFullscreenBlackWithAlpha(alpha);
            }
        }
    }
}

void SplashScreenScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)scancode;
    (void)mod;
    (void)keyboardID;

    if (isrepeat)
    {
        return;
    }

    if (keycode == SDLK_ESCAPE)
    {
        rc2d_event_quit();
        return;
    }

    finishAndGoToMenu();
}

void SplashScreenScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)clicks;
    (void)mouseID;

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        finishAndGoToMenu();
    }
}
