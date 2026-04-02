#include "game/scenes/scene-menu.h"

#include "game/scenes/scene-manager.h"

/**
 * @brief Clamp helper for alpha and interpolation values.
 *
 * @param value Input value.
 * @return Value clamped in [0.0, 1.0].
 */
double MenuScene::clamp01(double value)
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
 * @brief Construct menu scene with default state.
 */
MenuScene::MenuScene(void)
    // Initialize video handle.
    : loginBackgroundVideo{},
      // Opening has not been attempted yet.
      loginBackgroundOpenAttempted(false),
      // Initialize logo widget.
      logoUi{},
      // Initialize email widget.
      inputEmailUi{},
      // Initialize password widget.
      inputPasswordUi{},
      // Initialize login button widget.
      buttonLoginUi{},
      // No loaded menu audio yet.
      menuMusic(nullptr),
      // No active track yet.
      menuTrack(nullptr),
      // Music has not started yet.
      menuMusicStarted(false),
      // Start with full black intro fade.
      loginFadeAlpha(1.0f)
{
    // Constructor body intentionally empty.
}

/**
 * @brief Draw black fullscreen overlay with alpha.
 *
 * @param alpha01 Alpha ratio in [0..1].
 */
void MenuScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    // Clamp alpha before conversion.
    const double alphaClamped = clamp01(alpha01);

    // Skip draw if fully transparent.
    if (alphaClamped <= 0.0)
    {
        return;
    }

    // Read visible safe rectangle from RC2D.
    SDL_FRect rect = rc2d_engine_getVisibleSafeRectRender();

    // Guard invalid rectangle.
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    // Enable alpha blending for overlay pass.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Set black color and requested opacity.
    rc2d_graphics_setColor({0, 0, 0, static_cast<Uint8>(alphaClamped * 255.0)});

    // Draw fullscreen rectangle.
    rc2d_graphics_rectangle("fill", &rect);

    // Restore default blend mode.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

/**
 * @brief Switch to gameplay scene.
 */
void MenuScene::goToGameScene(void)
{
    // Switch scene if manager is available.
    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("game");
    }
}

/**
 * @brief Release resources when leaving menu scene.
 */
void MenuScene::unload(void)
{
    // Close background video if open.
    rc2d_video_close(&loginBackgroundVideo);

    // Reset opening-attempt flag.
    loginBackgroundOpenAttempted = false;

    // Free logo texture.
    rc2d_graphics_freeImage(&logoUi.image);

    // Free logo CPU image data.
    rc2d_graphics_freeImageData(&logoUi.imageData);

    // Free email texture.
    rc2d_graphics_freeImage(&inputEmailUi.image);

    // Free email CPU image data.
    rc2d_graphics_freeImageData(&inputEmailUi.imageData);

    // Free password texture.
    rc2d_graphics_freeImage(&inputPasswordUi.image);

    // Free password CPU image data.
    rc2d_graphics_freeImageData(&inputPasswordUi.imageData);

    // Free login button texture.
    rc2d_graphics_freeImage(&buttonLoginUi.image);

    // Free login button CPU image data.
    rc2d_graphics_freeImageData(&buttonLoginUi.imageData);

    // Stop and destroy active track.
    if (menuTrack != nullptr)
    {
        // Stop playback first.
        rc2d_track_stop(menuTrack);

        // Destroy track resource.
        rc2d_track_destroy(menuTrack);

        // Clear pointer after destroy.
        menuTrack = nullptr;
    }

    // Destroy loaded audio resource.
    if (menuMusic != nullptr)
    {
        // Free audio asset.
        rc2d_audio_destroy(menuMusic);

        // Clear pointer after free.
        menuMusic = nullptr;
    }

    // Reset playback flag.
    menuMusicStarted = false;

    // Reset intro fade.
    loginFadeAlpha = 1.0f;

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Unloaded\n");
}

/**
 * @brief Prepare menu resources when entering scene.
 */
void MenuScene::load(void)
{
    // Show mouse cursor for menu interaction.
    rc2d_mouse_setVisible(true);

    // Reset video struct to clean state.
    loginBackgroundVideo = {};

    // Reset open-attempt guard.
    loginBackgroundOpenAttempted = false;

    // Reset intro fade.
    loginFadeAlpha = 1.0f;

    // Mark music as not started.
    menuMusicStarted = false;

    // Load logo texture.
    logoUi.image = rc2d_graphics_loadImageFromStorage("assets/images/logo-st-login.png", RC2D_STORAGE_TITLE);

    // Load logo CPU data for potential pixel collision.
    logoUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/logo-st-login.png", RC2D_STORAGE_TITLE);

    // Anchor logo at top center.
    logoUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    logoUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    logoUi.margin_x = 0.0f;

    // Small top margin.
    logoUi.margin_y = 0.005f;

    // Keep logo visible.
    logoUi.visible = true;

    // Logo is not interactable.
    logoUi.hittable = false;

    // Load email texture.
    inputEmailUi.image = rc2d_graphics_loadImageFromStorage("assets/images/input-email-login.png", RC2D_STORAGE_TITLE);

    // Load email CPU data.
    inputEmailUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/input-email-login.png", RC2D_STORAGE_TITLE);

    // Anchor email at top center.
    inputEmailUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    inputEmailUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    inputEmailUi.margin_x = 0.0f;

    // Vertical position for email field.
    inputEmailUi.margin_y = 0.45f;

    // Keep email field visible.
    inputEmailUi.visible = true;

    // Enable interactions for email field.
    inputEmailUi.hittable = true;

    // Load password texture.
    inputPasswordUi.image = rc2d_graphics_loadImageFromStorage("assets/images/input-password-login.png", RC2D_STORAGE_TITLE);

    // Load password CPU data.
    inputPasswordUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/input-password-login.png", RC2D_STORAGE_TITLE);

    // Anchor password at top center.
    inputPasswordUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    inputPasswordUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    inputPasswordUi.margin_x = 0.0f;

    // Vertical position for password field.
    inputPasswordUi.margin_y = 0.55f;

    // Keep password field visible.
    inputPasswordUi.visible = true;

    // Enable interactions for password field.
    inputPasswordUi.hittable = true;

    // Load login button texture.
    buttonLoginUi.image = rc2d_graphics_loadImageFromStorage("assets/images/button-login.png", RC2D_STORAGE_TITLE);

    // Load login button CPU data.
    buttonLoginUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/button-login.png", RC2D_STORAGE_TITLE);

    // Anchor button at top center.
    buttonLoginUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    buttonLoginUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    buttonLoginUi.margin_x = 0.0f;

    // Vertical position for button.
    buttonLoginUi.margin_y = 0.65f;

    // Keep button visible.
    buttonLoginUi.visible = true;

    // Enable button hit-testing.
    buttonLoginUi.hittable = true;

    // Load menu music audio.
    menuMusic = rc2d_audio_loadAudioFromStorage("assets/sounds/sound_menu.opus", RC2D_STORAGE_TITLE, true);

    // Handle audio load failure.
    if (menuMusic == nullptr)
    {
        // Log load issue.
        RC2D_log(RC2D_LOG_WARN, "Failed to load menu music: %s", SDL_GetError());
    }
    else
    {
        // Create track for music playback.
        menuTrack = rc2d_track_create();

        // Handle track creation failure.
        if (menuTrack == nullptr)
        {
            // Log track creation issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to create menu track: %s", SDL_GetError());
        }
        else if (!rc2d_track_setAudio(menuTrack, menuMusic))
        {
            // Log track binding issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to set menu track audio: %s", SDL_GetError());
        }
    }

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Loaded\n");
}

/**
 * @brief Update menu state every frame.
 *
 * @param dt Frame delta in seconds.
 */
void MenuScene::update(double dt)
{
    // Animate intro fade until transparent.
    if (loginFadeAlpha > 0.0f)
    {
        // Subtract fade speed scaled by dt.
        loginFadeAlpha -= static_cast<float>(kLoginFadeSpeed * dt);

        // Clamp at zero.
        if (loginFadeAlpha < 0.0f)
        {
            loginFadeAlpha = 0.0f;
        }
    }

    // Start menu music once.
    if (!menuMusicStarted && menuTrack != nullptr && menuMusic != nullptr)
    {
        // Request looped playback.
        if (!rc2d_track_play(menuTrack, -1))
        {
            // Log playback issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to play menu music: %s", SDL_GetError());
        }
        else
        {
            // Mark as started to avoid replay spam.
            menuMusicStarted = true;
        }
    }

    // Try opening menu background video once.
    if (loginBackgroundVideo.format_ctx == nullptr && !loginBackgroundOpenAttempted)
    {
        // Mark attempt to avoid retry loops.
        loginBackgroundOpenAttempted = true;

        // Open menu background video.
        if (rc2d_video_openFromStorage(
                &loginBackgroundVideo,
                "assets/videos/background-menu.mp4",
                RC2D_STORAGE_TITLE) != 0)
        {
            // Log opening issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to open menu background video: %s", SDL_GetError());
        }
        else
        {
            // Loop the menu background.
            rc2d_video_setLoop(&loginBackgroundVideo, 1);
        }
    }

    // Decode and advance background video when available.
    if (loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_update(&loginBackgroundVideo, dt);
    }
}

/**
 * @brief Draw menu background and UI.
 */
void MenuScene::draw(void)
{
    // Draw menu video if opened.
    if (loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_draw(&loginBackgroundVideo);
    }

    // Draw logo widget.
    rc2d_ui_drawImage(&logoUi);

    // Draw email widget.
    rc2d_ui_drawImage(&inputEmailUi);

    // Draw password widget.
    rc2d_ui_drawImage(&inputPasswordUi);

    // Draw login button widget.
    rc2d_ui_drawImage(&buttonLoginUi);

    // Draw intro fade if still active.
    if (loginFadeAlpha > 0.0f)
    {
        drawFullscreenBlackWithAlpha(loginFadeAlpha);
    }
}

/**
 * @brief Handle keyboard events in menu.
 */
void MenuScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Enter starts game.
    if (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER)
    {
        goToGameScene();
        return;
    }
}

/**
 * @brief Handle mouse clicks on menu UI.
 */
void MenuScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Only process left click.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Check email box hit.
    if (rc2d_collision_pointInUIImagePixelPerfect(&inputEmailUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked EMAIL input box");
    }
    // Check password box hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&inputPasswordUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked PASSWORD input box");
    }
    // Check login button hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&buttonLoginUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked LOGIN button");

        // Start gameplay transition.
        goToGameScene();
    }
}
