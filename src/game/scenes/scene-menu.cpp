#include "game/scenes/scene-menu.h"

#include "core/context.h"
#include "game/scenes/scene-manager.h"

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

void MenuScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    // Clamp alpha before conversion.
    const double alphaClamped = clamp01(alpha01);

    // Skip draw if fully transparent.
    if (alphaClamped <= 0.0)
    {
        return;
    }

    // Read visible safe rectangle from shared game screen wrapper.
    SDL_FRect rect = GetGameScreen().rect;

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

void MenuScene::goToGameScene(void)
{
    // Switch scene if manager is available.
    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("game");
    }
}

void MenuScene::unload(void)
{
    // Close background video if open.
    rc2d_video_close(&this->loginBackgroundVideo);

    // Reset opening-attempt flag.
    this->loginBackgroundOpenAttempted = false;

    // Free logo texture.
    rc2d_graphics_freeImage(&this->logoUi.image);

    // Free logo CPU image data.
    rc2d_graphics_freeImageData(&this->logoUi.imageData);

    // Free email texture.
    rc2d_graphics_freeImage(&this->inputEmailUi.image);

    // Free email CPU image data.
    rc2d_graphics_freeImageData(&this->inputEmailUi.imageData);

    // Free password texture.
    rc2d_graphics_freeImage(&this->inputPasswordUi.image);

    // Free password CPU image data.
    rc2d_graphics_freeImageData(&this->inputPasswordUi.imageData);

    // Free login button texture.
    rc2d_graphics_freeImage(&this->buttonLoginUi.image);

    // Free login button CPU image data.
    rc2d_graphics_freeImageData(&this->buttonLoginUi.imageData);

    // Stop and destroy active track.
    if (this->menuTrack != nullptr)
    {
        // Stop playback first.
        rc2d_track_stop(this->menuTrack);

        // Destroy track resource.
        rc2d_track_destroy(this->menuTrack);

        // Clear pointer after destroy.
        this->menuTrack = nullptr;
    }

    // Destroy loaded audio resource.
    if (this->menuMusic != nullptr)
    {
        // Free audio asset.
        rc2d_audio_destroy(this->menuMusic);

        // Clear pointer after free.
        this->menuMusic = nullptr;
    }

    // Reset playback flag.
    this->menuMusicStarted = false;

    // Reset intro fade.
    this->loginFadeAlpha = 1.0f;

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Unloaded\n");
}

void MenuScene::load(void)
{
    // Show mouse cursor for menu interaction.
    rc2d_mouse_setVisible(true);

    // Reset video struct to clean state.
    this->loginBackgroundVideo = {};

    // Reset open-attempt guard.
    this->loginBackgroundOpenAttempted = false;

    // Reset intro fade.
    this->loginFadeAlpha = 1.0f;

    // Mark music as not started.
    this->menuMusicStarted = false;

    // Load logo texture.
    this->logoUi.image = rc2d_graphics_loadImageFromStorage("assets/images/ui-scene-menu/logo-st.png", RC2D_STORAGE_TITLE);

    // Load logo CPU data for potential pixel collision.
    this->logoUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/ui-scene-menu/logo-st.png", RC2D_STORAGE_TITLE);

    // Anchor logo at top center.
    this->logoUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->logoUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    this->logoUi.margin_x = 0.0f;

    // Small top margin.
    this->logoUi.margin_y = 0.005f;

    // Keep logo visible.
    this->logoUi.visible = true;

    // Logo is not interactable.
    this->logoUi.hittable = false;

    // Load email texture.
    this->inputEmailUi.image = rc2d_graphics_loadImageFromStorage("assets/images/ui-scene-menu/input-email.png", RC2D_STORAGE_TITLE);

    // Load email CPU data.
    this->inputEmailUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/ui-scene-menu/input-email.png", RC2D_STORAGE_TITLE);

    // Anchor email at top center.
    this->inputEmailUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->inputEmailUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    this->inputEmailUi.margin_x = 0.0f;

    // Vertical position for email field.
    this->inputEmailUi.margin_y = 0.45f;

    // Keep email field visible.
    this->inputEmailUi.visible = true;

    // Enable interactions for email field.
    this->inputEmailUi.hittable = true;

    // Load password texture.
    this->inputPasswordUi.image = rc2d_graphics_loadImageFromStorage("assets/images/ui-scene-menu/input-password.png", RC2D_STORAGE_TITLE);

    // Load password CPU data.
    this->inputPasswordUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/ui-scene-menu/input-password.png", RC2D_STORAGE_TITLE);

    // Anchor password at top center.
    this->inputPasswordUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->inputPasswordUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    this->inputPasswordUi.margin_x = 0.0f;

    // Vertical position for password field.
    this->inputPasswordUi.margin_y = 0.55f;

    // Keep password field visible.
    this->inputPasswordUi.visible = true;

    // Enable interactions for password field.
    this->inputPasswordUi.hittable = true;

    // Load login button texture.
    this->buttonLoginUi.image = rc2d_graphics_loadImageFromStorage("assets/images/ui-scene-menu/button-login.png", RC2D_STORAGE_TITLE);

    // Load login button CPU data.
    this->buttonLoginUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/ui-scene-menu/button-login.png", RC2D_STORAGE_TITLE);

    // Anchor button at top center.
    this->buttonLoginUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->buttonLoginUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // No horizontal offset.
    this->buttonLoginUi.margin_x = 0.0f;

    // Vertical position for button.
    this->buttonLoginUi.margin_y = 0.65f;

    // Keep button visible.
    this->buttonLoginUi.visible = true;

    // Enable button hit-testing.
    this->buttonLoginUi.hittable = true;

    // Load menu music audio.
    this->menuMusic = rc2d_audio_loadAudioFromStorage("assets/sounds/sound_menu.opus", RC2D_STORAGE_TITLE, true);

    // Handle audio load failure.
    if (this->menuMusic == nullptr)
    {
        // Log load issue.
        RC2D_log(RC2D_LOG_WARN, "Failed to load menu music: %s", SDL_GetError());
    }
    else
    {
        // Create track for music playback.
        this->menuTrack = rc2d_track_create();

        // Handle track creation failure.
        if (this->menuTrack == nullptr)
        {
            // Log track creation issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to create menu track: %s", SDL_GetError());
        }
        else if (!rc2d_track_setAudio(this->menuTrack, this->menuMusic))
        {
            // Log track binding issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to set menu track audio: %s", SDL_GetError());
        }
    }

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Loaded\n");
}

void MenuScene::update(double dt)
{
    // Animate intro fade until transparent.
    if (this->loginFadeAlpha > 0.0f)
    {
        // Subtract fade speed scaled by dt.
        this->loginFadeAlpha -= static_cast<float>(kLoginFadeSpeed * dt);

        // Clamp at zero.
        if (this->loginFadeAlpha < 0.0f)
        {
            this->loginFadeAlpha = 0.0f;
        }
    }

    // Start menu music once.
    if (!this->menuMusicStarted && this->menuTrack != nullptr && this->menuMusic != nullptr)
    {
        // Request looped playback.
        if (!rc2d_track_play(this->menuTrack, -1))
        {
            // Log playback issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to play menu music: %s", SDL_GetError());
        }
        else
        {
            // Mark as started to avoid replay spam.
            this->menuMusicStarted = true;
        }
    }

    // Try opening menu background video once.
    if (this->loginBackgroundVideo.format_ctx == nullptr && !this->loginBackgroundOpenAttempted)
    {
        // Mark attempt to avoid retry loops.
        this->loginBackgroundOpenAttempted = true;

        // Open menu background video.
        if (rc2d_video_openFromStorage(
                &this->loginBackgroundVideo,
                "assets/videos/background-menu-1080p.mp4",
                RC2D_STORAGE_TITLE) != 0)
        {
            // Log opening issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to open menu background video: %s", SDL_GetError());
        }
        else
        {
            // Loop the menu background.
            rc2d_video_setLoop(&this->loginBackgroundVideo, 1);
        }
    }

    // Decode and advance background video when available.
    if (this->loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_update(&this->loginBackgroundVideo, dt);
    }
}

void MenuScene::draw(void)
{
    // Draw menu video if opened.
    if (this->loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_draw(&this->loginBackgroundVideo);
    }

    // Draw logo widget.
    rc2d_ui_drawImage(&this->logoUi);

    // Draw email widget.
    rc2d_ui_drawImage(&this->inputEmailUi);

    // Draw password widget.
    rc2d_ui_drawImage(&this->inputPasswordUi);

    // Draw login button widget.
    rc2d_ui_drawImage(&this->buttonLoginUi);

    // Draw intro fade if still active.
    if (this->loginFadeAlpha > 0.0f)
    {
        this->drawFullscreenBlackWithAlpha(this->loginFadeAlpha);
    }
}

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
        this->goToGameScene();
        return;
    }
}

void MenuScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Only process left click.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Check email box hit.
    if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputEmailUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked EMAIL input box");
    }
    // Check password box hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputPasswordUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked PASSWORD input box");
    }
    // Check login button hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&this->buttonLoginUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked LOGIN button");
    }
}
