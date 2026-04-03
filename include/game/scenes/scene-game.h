#pragma once

#include <RC2D/RC2D.h>

#include "game/shaders/fog-of-war-shader.h"
#include "game/shaders/ocean-shader.h"
#include "game/scenes/scene.h"

/**
 * @brief Main gameplay scene.
 *
 * This scene contains the ocean shader effect and a fog-of-war overlay pass.
 */
class GameScene : public Scene {
private:
    OceanShader oceanShader;         /**< Module shader dédié à l'océan. */
    FogOfWarShader fogOfWarShader;   /**< Module shader dédié au brouillard de guerre. */

public:
    /**
     * @brief Build gameplay scene instance.
     */
    GameScene(void);

    /**
     * @brief Release gameplay resources when leaving scene.
     */
    void unload(void) override;

    /**
     * @brief Prepare gameplay resources when entering scene.
     */
    void load(void) override;

    /**
     * @brief Update gameplay logic and visuals.
     * @param dt Delta time in seconds.
     */
    void update(double dt) override;

    /**
     * @brief Render ocean effect and animated atlas sprite.
     */
    void draw(void) override;

    /**
     * @brief Handle keyboard interactions in gameplay scene.
     * @param key Text key representation from RC2D/SDL.
     * @param scancode Physical keyboard scancode.
     * @param keycode Logical keyboard keycode.
     * @param mod Keyboard modifiers.
     * @param isrepeat True when event is a key repeat.
     * @param keyboardID SDL keyboard device id.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Handle mouse presses in gameplay scene.
     * @param x Mouse x position in render space.
     * @param y Mouse y position in render space.
     * @param button Mouse button identifier.
     * @param clicks Number of clicks.
     * @param mouseID SDL mouse device id.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
