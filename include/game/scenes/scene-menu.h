#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

/**
 * @brief Main menu / login scene.
 *
 * This scene plays a background video, renders login UI widgets,
 * and handles transitions to gameplay.
 */
class MenuScene : public Scene {
private:
    RC2D_Video loginBackgroundVideo;      /**< Background video handle. */
    bool loginBackgroundOpenAttempted;    /**< True once opening was attempted. */

    RC2D_UIImage logoUi;          /**< Top logo image widget. */
    RC2D_UIImage inputEmailUi;    /**< Email field widget image. */
    RC2D_UIImage inputPasswordUi; /**< Password field widget image. */
    RC2D_UIImage buttonLoginUi;   /**< Login button widget image. */

    MIX_Audio* menuMusic;   /**< Loaded menu audio resource. */
    MIX_Track* menuTrack;   /**< Track used to play menu music. */
    bool menuMusicStarted;  /**< True after first successful playback. */

    float loginFadeAlpha;                           /**< Intro fade alpha. */
    static constexpr float kLoginFadeSpeed = 0.5f; /**< Intro fade speed. */

    /**
     * @brief Clamp helper used for alpha values.
     * @param value Input floating point value.
     * @return Value clamped between 0.0 and 1.0.
     */
    static double clamp01(double value);

    /**
     * @brief Draw black fullscreen overlay using alpha.
     * @param alpha01 Alpha ratio in [0..1].
     */
    void drawFullscreenBlackWithAlpha(double alpha01);

    /**
     * @brief Request transition from menu to gameplay scene.
     */
    void goToGameScene(void);

public:
    /**
     * @brief Build a menu scene instance.
     */
    MenuScene(void);

    /**
     * @brief Release menu resources when leaving the scene.
     */
    void unload(void) override;

    /**
     * @brief Prepare menu resources when entering the scene.
     */
    void load(void) override;

    /**
     * @brief Update menu background video, music and fade.
     * @param dt Delta time in seconds.
     */
    void update(double dt) override;

    /**
     * @brief Render menu background and UI elements.
     */
    void draw(void) override;

    /**
     * @brief Handle keyboard interaction in menu.
     * @param key Text key representation from RC2D/SDL.
     * @param scancode Physical keyboard scancode.
     * @param keycode Logical keyboard keycode.
     * @param mod Keyboard modifiers.
     * @param isrepeat True when event is a key repeat.
     * @param keyboardID SDL keyboard device id.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Handle mouse clicks on login UI widgets.
     * @param x Mouse x position in render space.
     * @param y Mouse y position in render space.
     * @param button Mouse button identifier.
     * @param clicks Number of clicks.
     * @param mouseID SDL mouse device id.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
