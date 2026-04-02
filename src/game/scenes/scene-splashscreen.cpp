#include "game/scenes/scene-splashscreen.h"

#include "game/scenes/scene-manager.h"

/**
 * @brief Clamp helper for alpha and interpolation values.
 *
 * @param value Input value.
 * @return Value clamped in [0.0, 1.0].
 */
double SplashScreenScene::clamp01(double value)
{
    // Clamp lower bound.
    if (value < 0.0)
    {
        // Return minimum allowed value.
        return 0.0;
    }

    // Clamp upper bound.
    if (value > 1.0)
    {
        // Return maximum allowed value.
        return 1.0;
    }

    // Value is already valid.
    return value;
}

/**
 * @brief Construct splash scene with clean state.
 */
SplashScreenScene::SplashScreenScene(void)
    // Initialize first video handle.
    : splashStudioVideo{},
      // Initialize second video handle.
      splashGameVideo{},
      // Start from the first splash.
      splashState(SPLASH_STUDIO)
{
    // Constructor body intentionally empty.
}

/**
 * @brief Draw a fullscreen black overlay with alpha.
 *
 * @param alpha01 Alpha ratio in [0..1].
 */
void SplashScreenScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    // Clamp alpha before converting to 8-bit.
    const double alphaClamped = clamp01(alpha01);

    // Skip draw if fully transparent.
    if (alphaClamped <= 0.0)
    {
        return;
    }

    // Read visible safe rectangle from RC2D.
    SDL_FRect rect = rc2d_engine_getVisibleSafeRectRender();

    // Guard against invalid render area.
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    // Enable alpha blending for overlay pass.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Set black color with requested alpha.
    rc2d_graphics_setColor({0, 0, 0, static_cast<Uint8>(alphaClamped * 255.0)});

    // Draw fullscreen quad.
    rc2d_graphics_rectangle("fill", &rect);

    // Restore default blend mode.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

/**
 * @brief End splash sequence and go to menu.
 */
void SplashScreenScene::finishAndGoToMenu(void)
{
    // Avoid processing completion twice.
    if (splashState == SPLASH_DONE)
    {
        return;
    }

    // Close first video resource.
    rc2d_video_close(&splashStudioVideo);

    // Close second video resource.
    rc2d_video_close(&splashGameVideo);

    // Mark sequence as done.
    splashState = SPLASH_DONE;

    // Switch to menu scene if manager is valid.
    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("menu");
    }
}

/**
 * @brief Release resources when leaving splash scene.
 */
void SplashScreenScene::unload(void)
{
    // Close first splash video if open.
    rc2d_video_close(&splashStudioVideo);

    // Close second splash video if open.
    rc2d_video_close(&splashGameVideo);

    // Reset state machine for next entry.
    splashState = SPLASH_STUDIO;

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Unloaded\n");
}

/**
 * @brief Prepare splash state when entering scene.
 */
void SplashScreenScene::load(void)
{
    // Ensure first video starts clean.
    rc2d_video_close(&splashStudioVideo);

    // Ensure second video starts clean.
    rc2d_video_close(&splashGameVideo);

    // Restart from first splash.
    splashState = SPLASH_STUDIO;

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Loaded\n");
}

/**
 * @brief Update splash videos and transitions.
 *
 * @param dt Frame delta in seconds.
 */
void SplashScreenScene::update(double dt)
{
    // Drive the splash state machine.
    switch (splashState)
    {
        case SPLASH_STUDIO:
        {
            // Open first splash lazily for platform safety.
            if (splashStudioVideo.format_ctx == nullptr)
            {
                // Try to open studio video.
                if (rc2d_video_openFromStorage(
                        &splashStudioVideo,
                        "assets/videos/splashscreen-studio-1080p.mp4",
                        RC2D_STORAGE_TITLE) != 0)
                {
                    // Skip to game splash if first video fails.
                    RC2D_log(RC2D_LOG_WARN, "Failed to open studio splash video, skipping.");
                    splashState = SPLASH_GAME;
                    return;
                }
            }

            // Decode and advance first video.
            if (rc2d_video_update(&splashStudioVideo, dt) <= 0)
            {
                // Close first video when finished.
                rc2d_video_close(&splashStudioVideo);

                // Move to second splash.
                splashState = SPLASH_GAME;
            }

            // End first state block.
            break;
        }

        case SPLASH_GAME:
        {
            // Open second splash lazily.
            if (splashGameVideo.format_ctx == nullptr)
            {
                // Try to open game splash.
                if (rc2d_video_openFromStorage(
                        &splashGameVideo,
                        "assets/videos/splashscreen-seatyrants-1080p.mp4",
                        RC2D_STORAGE_TITLE) != 0)
                {
                    // Fail safe: continue to menu.
                    RC2D_log(RC2D_LOG_WARN, "Failed to open game splash video, skipping.");
                    finishAndGoToMenu();
                    return;
                }
            }

            // Decode and advance second video.
            if (rc2d_video_update(&splashGameVideo, dt) <= 0)
            {
                // End splash sequence when second video finishes.
                finishAndGoToMenu();
            }

            // End second state block.
            break;
        }

        case SPLASH_DONE:
        default:
            // Nothing to update once done.
            break;
    }
}

/**
 * @brief Draw splash frames and fade effects.
 */
void SplashScreenScene::draw(void)
{
    // First splash rendering branch.
    if (splashState == SPLASH_STUDIO)
    {
        // Ensure first video exists before drawing.
        if (splashStudioVideo.format_ctx != nullptr)
        {
            // Draw current first splash frame.
            rc2d_video_draw(&splashStudioVideo);

            // Query total duration to compute fade-out near the end.
            const double total = rc2d_video_totalSeconds(&splashStudioVideo);

            // Query current playback position.
            const double now = rc2d_video_currentSeconds(&splashStudioVideo);

            // Use fade only if total duration is known.
            if (total > 0.0)
            {
                // Compute remaining seconds.
                const double remaining = total - now;

                // Start fade when entering final window.
                if (remaining <= kFadeSeconds)
                {
                    // Convert to alpha progression.
                    const double alpha = 1.0 - (remaining / kFadeSeconds);

                    // Draw fade overlay.
                    drawFullscreenBlackWithAlpha(alpha);
                }
            }
        }

        // Stop here because we handled studio branch.
        return;
    }

    // Second splash rendering branch.
    if (splashState == SPLASH_GAME)
    {
        // Ensure second video exists before drawing.
        if (splashGameVideo.format_ctx != nullptr)
        {
            // Draw current second splash frame.
            rc2d_video_draw(&splashGameVideo);

            // Read current playback position.
            const double now = rc2d_video_currentSeconds(&splashGameVideo);

            // Fade-in from black at video start.
            if (now < kFadeSeconds)
            {
                // Convert to alpha progression.
                const double alpha = 1.0 - (now / kFadeSeconds);

                // Draw fade overlay.
                drawFullscreenBlackWithAlpha(alpha);
            }
        }
    }
}

/**
 * @brief Handle key presses in splash scene.
 */
void SplashScreenScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{

}

/**
 * @brief Handle mouse press in splash scene.
 */
void SplashScreenScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{

}
