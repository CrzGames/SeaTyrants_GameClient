#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

/**
 * @brief Main gameplay scene.
 *
 * This scene contains the ocean shader effect and a simple animated atlas sample
 * used as a gameplay visual placeholder.
 */
class GameScene : public Scene {
private:
    /**
     * @brief Uniform layout sent to the ocean fragment shader.
     *
     * params0 = {time, strength, pxAmplitude, tiling}
     * params1 = {width, height, speed, fresnel}
     */
    typedef struct OceanUniforms {
        float params0[4];
        float params1[4];
    } OceanUniforms;

    RC2D_Image waterTileImage;     /**< Base water texture used for shader pass. */
    RC2D_Image causticTileImage;   /**< Optional caustic texture reserved for future use. */
    RC2D_TP_Atlas elite27Atlas;    /**< TexturePacker atlas used for animated ship sample. */

    RC2D_GPUShader* oceanFragmentShader;  /**< Loaded fragment shader handle. */
    SDL_GPURenderState* oceanRenderState; /**< SDL GPU render state for ocean pass. */
    SDL_GPUSampler* oceanRepeatSampler;   /**< REPEAT sampler bound to shader slot. */
    OceanUniforms oceanUniforms;          /**< Cached uniform values. */
    double oceanTimeAccum;                /**< Accumulated time for animation. */

    int eliteFrameIndex;            /**< Current frame index in atlas animation. */
    double eliteFrameAccumulator;   /**< Time accumulator for frame stepping. */

    static constexpr int kElite27FrameCount = 8; /**< Total number of atlas frames. */
    static constexpr double kElite27FrameDurationSeconds = 0.10; /**< Frame duration in seconds. */
    inline static constexpr const char* kElite27FrameNames[kElite27FrameCount] = {
        "1.png",
        "2.png",
        "3.png",
        "4.png",
        "5.png",
        "6.png",
        "7.png",
        "8.png"
    }; /**< Ordered atlas frame names. */

    /**
     * @brief Create shader/sampler/render-state resources for ocean rendering.
     * @return True on success, false on failure.
     */
    bool initializeOceanRenderState(void);

    /**
     * @brief Destroy shader/sampler/render-state resources.
     */
    void destroyOceanRenderState(void);

    /**
     * @brief Update ocean shader uniforms from time and render output size.
     * @param outputWidth Current render output width in pixels.
     * @param outputHeight Current render output height in pixels.
     * @param dt Delta time in seconds.
     */
    void updateOceanUniforms(int outputWidth, int outputHeight, double dt);

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
