#pragma once

#include <string>

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

class LoadingScene : public Scene
{
public:
    LoadingScene();
    explicit LoadingScene(const char* nextSceneName);
    explicit LoadingScene(const std::string& nextSceneName);
    ~LoadingScene() override;

    void load(void) override;
    void unload(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat,
        SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;

private:
    static std::string getDefaultNextSceneName();
    static double clamp01(double value);
    void drawFullscreenBlackWithAlpha(double alpha01);

    static constexpr float kLoadingFadeSpeed = 0.5f;

    std::string nextSceneName;
    RC2D_Image backgroundImage;
    RC2D_Font headingFont;
    RC2D_Font bodyFont;
    bool transitionStarted;
    bool transitionFadeOutStarted;
    double transitionDelayRemaining;
    float loadingFadeAlpha;
};
