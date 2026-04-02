#include "game/scenes/scene-menu.h"

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

MenuScene::MenuScene(void)
    : loginBackgroundVideo{},
      loginBackgroundOpenAttempted(false),
      logoUi{},
      inputEmailUi{},
      inputPasswordUi{},
      buttonLoginUi{},
      menuMusic(nullptr),
      menuTrack(nullptr),
      menuMusicStarted(false),
      loginFadeAlpha(1.0f)
{
}

void MenuScene::drawFullscreenBlackWithAlpha(double alpha01)
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

void MenuScene::goToGameScene(void)
{
    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("game");
    }
}

void MenuScene::unload(void)
{
    rc2d_video_close(&loginBackgroundVideo);
    loginBackgroundOpenAttempted = false;

    rc2d_graphics_freeImage(&logoUi.image);
    rc2d_graphics_freeImageData(&logoUi.imageData);
    rc2d_graphics_freeImage(&inputEmailUi.image);
    rc2d_graphics_freeImageData(&inputEmailUi.imageData);
    rc2d_graphics_freeImage(&inputPasswordUi.image);
    rc2d_graphics_freeImageData(&inputPasswordUi.imageData);
    rc2d_graphics_freeImage(&buttonLoginUi.image);
    rc2d_graphics_freeImageData(&buttonLoginUi.imageData);

    if (menuTrack != nullptr)
    {
        rc2d_track_stop(menuTrack);
        rc2d_track_destroy(menuTrack);
        menuTrack = nullptr;
    }

    if (menuMusic != nullptr)
    {
        rc2d_audio_destroy(menuMusic);
        menuMusic = nullptr;
    }

    menuMusicStarted = false;
    loginFadeAlpha = 1.0f;

    RC2D_log(RC2D_LOG_INFO, "Menu Scene Unloaded\n");
}

void MenuScene::load(void)
{
    loginBackgroundVideo = {};
    loginBackgroundOpenAttempted = false;
    loginFadeAlpha = 1.0f;
    menuMusicStarted = false;

    logoUi.image = rc2d_graphics_loadImageFromStorage("assets/images/logo-st-login.png", RC2D_STORAGE_TITLE);
    logoUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/logo-st-login.png", RC2D_STORAGE_TITLE);
    logoUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;
    logoUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    logoUi.margin_x = 0.0f;
    logoUi.margin_y = 0.005f;
    logoUi.visible = true;
    logoUi.hittable = false;

    inputEmailUi.image = rc2d_graphics_loadImageFromStorage("assets/images/input-email-login.png", RC2D_STORAGE_TITLE);
    inputEmailUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/input-email-login.png", RC2D_STORAGE_TITLE);
    inputEmailUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;
    inputEmailUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    inputEmailUi.margin_x = 0.0f;
    inputEmailUi.margin_y = 0.45f;
    inputEmailUi.visible = true;
    inputEmailUi.hittable = true;

    inputPasswordUi.image = rc2d_graphics_loadImageFromStorage("assets/images/input-password-login.png", RC2D_STORAGE_TITLE);
    inputPasswordUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/input-password-login.png", RC2D_STORAGE_TITLE);
    inputPasswordUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;
    inputPasswordUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    inputPasswordUi.margin_x = 0.0f;
    inputPasswordUi.margin_y = 0.55f;
    inputPasswordUi.visible = true;
    inputPasswordUi.hittable = true;

    buttonLoginUi.image = rc2d_graphics_loadImageFromStorage("assets/images/button-login.png", RC2D_STORAGE_TITLE);
    buttonLoginUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/button-login.png", RC2D_STORAGE_TITLE);
    buttonLoginUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;
    buttonLoginUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    buttonLoginUi.margin_x = 0.0f;
    buttonLoginUi.margin_y = 0.65f;
    buttonLoginUi.visible = true;
    buttonLoginUi.hittable = true;

    menuMusic = rc2d_audio_loadAudioFromStorage("assets/sounds/sound_menu.mp3", RC2D_STORAGE_TITLE, true);
    if (menuMusic == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "Failed to load menu music: %s", SDL_GetError());
    }
    else
    {
        menuTrack = rc2d_track_create();
        if (menuTrack == nullptr)
        {
            RC2D_log(RC2D_LOG_WARN, "Failed to create menu track: %s", SDL_GetError());
        }
        else if (!rc2d_track_setAudio(menuTrack, menuMusic))
        {
            RC2D_log(RC2D_LOG_WARN, "Failed to set menu track audio: %s", SDL_GetError());
        }
    }

    RC2D_log(RC2D_LOG_INFO, "Menu Scene Loaded\n");
}

void MenuScene::update(double dt)
{
    if (loginFadeAlpha > 0.0f)
    {
        loginFadeAlpha -= static_cast<float>(kLoginFadeSpeed * dt);
        if (loginFadeAlpha < 0.0f)
        {
            loginFadeAlpha = 0.0f;
        }
    }

    if (!menuMusicStarted && menuTrack != nullptr && menuMusic != nullptr)
    {
        if (!rc2d_track_play(menuTrack, -1))
        {
            RC2D_log(RC2D_LOG_WARN, "Failed to play menu music: %s", SDL_GetError());
        }
        else
        {
            menuMusicStarted = true;
        }
    }

    if (loginBackgroundVideo.format_ctx == nullptr && !loginBackgroundOpenAttempted)
    {
        loginBackgroundOpenAttempted = true;
        if (rc2d_video_openFromStorage(
                &loginBackgroundVideo,
                "assets/videos/background-menu.mp4",
                RC2D_STORAGE_TITLE) != 0)
        {
            RC2D_log(RC2D_LOG_WARN, "Failed to open menu background video: %s", SDL_GetError());
        }
        else
        {
            rc2d_video_setLoop(&loginBackgroundVideo, 1);
        }
    }

    if (loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_update(&loginBackgroundVideo, dt);
    }
}

void MenuScene::draw(void)
{
    if (loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_draw(&loginBackgroundVideo);
    }

    rc2d_ui_drawImage(&logoUi);
    rc2d_ui_drawImage(&inputEmailUi);
    rc2d_ui_drawImage(&inputPasswordUi);
    rc2d_ui_drawImage(&buttonLoginUi);

    if (loginFadeAlpha > 0.0f)
    {
        drawFullscreenBlackWithAlpha(loginFadeAlpha);
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
    (void)key;
    (void)scancode;
    (void)mod;
    (void)keyboardID;

    if (isrepeat)
    {
        return;
    }

    if (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER)
    {
        goToGameScene();
        return;
    }

    if (keycode == SDLK_E)
    {
        if (sceneManager != nullptr)
        {
            sceneManager->changeScene("editormap");
        }
        return;
    }

    if (keycode == SDLK_ESCAPE)
    {
        rc2d_event_quit();
    }
}

void MenuScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    if (rc2d_collision_pointInUIImagePixelPerfect(&inputEmailUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked EMAIL input box");
    }
    else if (rc2d_collision_pointInUIImagePixelPerfect(&inputPasswordUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked PASSWORD input box");
    }
    else if (rc2d_collision_pointInUIImagePixelPerfect(&buttonLoginUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked LOGIN button");
        goToGameScene();
    }
}
