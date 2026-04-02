#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

class SplashScreenScene : public Scene {
private:
    enum SplashState {
        SPLASH_STUDIO = 0,
        SPLASH_GAME,
        SPLASH_DONE
    };

    RC2D_Video splashStudioVideo;
    RC2D_Video splashGameVideo;
    SplashState splashState;

    static constexpr double kFadeSeconds = 1.5;

    void finishAndGoToMenu(void);
    void drawFullscreenBlackWithAlpha(double alpha01);

public:
    SplashScreenScene(void);

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
