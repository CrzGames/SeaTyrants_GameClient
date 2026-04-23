#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

/**
 * @brief Scene responsible for the startup splash flow.
 *
 * This scene plays the studio splash first, then the game splash,
 * and finally switches to the loading scene.
 */
class SplashScreenScene : public Scene {
private:
    /**
     * @brief Internal state machine for splash playback.
     */
    enum SplashState {
        SPLASH_STUDIO = 0, /**< First splash video (studio). */
        SPLASH_GAME,       /**< Second splash video (game). */
        SPLASH_DONE        /**< Splash sequence completed. */
    };

    RC2D_Video splashStudioVideo; /**< Video handle for the studio splash. */
    RC2D_Video splashGameVideo;   /**< Video handle for the game splash. */
    SplashState splashState;      /**< Current splash state. */

    static constexpr double kFadeSeconds = 1.5; /**< Fade duration in seconds. */

    /**
     * @brief Clamp helper used for alpha values.
     * @param value Input floating point value.
     * @return Value clamped between 0.0 and 1.0.
     */
    static double clamp01(double value);

    /**
     * @brief End splash sequence and switch to loading scene.
     */
    void finishAndGoToLoading(void);

    /**
     * @brief Draw a black fullscreen overlay with alpha.
     * @param alpha01 Alpha in [0..1].
     */
    void drawFullscreenBlackWithAlpha(double alpha01);

public:
    /**
     * @brief Build a splash scene instance.
     */
    SplashScreenScene(void);

    /**
     * @brief Release splash resources when leaving the scene.
     */
    void unload(void) override;

    /**
     * @brief Prepare splash scene state when entering the scene.
     */
    void load(void) override;

    /**
     * @brief Update splash playback and transitions.
     * @param dt Delta time in seconds.
     */
    void update(double dt) override;

    /**
     * @brief Render active splash video and fade overlay.
     */
    void draw(void) override;

    /**
     * @brief Handle key press events during splash.
     * @param key Text key representation from RC2D/SDL.
     * @param scancode Physical keyboard scancode.
     * @param keycode Logical keyboard keycode.
     * @param mod Keyboard modifiers.
     * @param isrepeat True when event is a key repeat.
     * @param keyboardID SDL keyboard device id.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Handle mouse press events during splash.
     * @param x Mouse x position in render space.
     * @param y Mouse y position in render space.
     * @param button Mouse button identifier.
     * @param clicks Number of clicks.
     * @param mouseID SDL mouse device id.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
