#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

class MenuScene : public Scene {
private:
    RC2D_Video loginBackgroundVideo;
    bool loginBackgroundOpenAttempted;

    RC2D_UIImage logoUi;
    RC2D_UIImage inputEmailUi;
    RC2D_UIImage inputPasswordUi;
    RC2D_UIImage buttonLoginUi;

    MIX_Audio* menuMusic;
    MIX_Track* menuTrack;
    bool menuMusicStarted;

    float loginFadeAlpha;
    static constexpr float kLoginFadeSpeed = 0.5f;

    void drawFullscreenBlackWithAlpha(double alpha01);
    void goToGameScene(void);

public:
    MenuScene(void);

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
