#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

/**
 * @brief Main gameplay scene.
 *
 * This scene contains the ocean shader effect and a fog-of-war overlay pass.
 */
class GameScene : public Scene {
private:
    /**
     * @brief Uniform block sent to the ocean fragment shader.
     *
     * params0 = {time, waveStrength, pixelAmplitude, tiling}
     * params1 = {width, height, speed, foamIntensity}
     * params2 = {colorMode, fresnelStrength, sunGlintStrength, whitecapBoost}
     * colorMode = 0.0 -> blue shading (legacy/current look)
     * colorMode = 1.0 -> neutral shading (for non-blue water texture variants)
     */
    struct OceanUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
    };

    /**
     * @brief Uniform block sent to the fog-of-war fragment shader.
     *
     * params0 = {time, noiseScale, driftSpeed, fogIntensity}
     * params1 = {revealMin, revealMax, edgeBoost, noiseContrast}
     * params2 = {tintR, tintG, tintB, alphaMax}
     */
    struct FogUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
    };

    RC2D_Image oceanTexture;                 /**< Base water texture #1 (bound as t0/s0 by SDL_RenderTexture). */
    RC2D_Image oceanTextureDetail;           /**< Water texture #2 (bound as additional t1/s1 sampler binding). */
    RC2D_Image causticTexture;               /**< Caustic texture (bound as additional t2/s2 sampler binding). */
    RC2D_Image foamStreaksTexture;           /**< Foam streak texture (bound as additional t3/s3 sampler binding). */
    RC2D_Image macroWaterTexture;            /**< Macro anti-tiling texture (bound as additional t4/s4 sampler binding). */
    RC2D_Image depthWaterTexture;            /**< Bathymetry depth texture (bound as additional t5/s5 sampler binding). */
    RC2D_Image fogMaskTexture;               /**< Fog visibility mask texture (bound as t0/s0 by SDL_RenderTexture). */
    RC2D_Image fogNoiseTexture;              /**< Fog noise texture (bound as additional t1/s1 sampler binding). */
    RC2D_GPUShader* oceanFragmentShader;     /**< Loaded fragment shader. */
    RC2D_GPUShader* fogFragmentShader;       /**< Loaded fragment shader for fog-of-war overlay. */
    SDL_GPURenderState* oceanRenderState;    /**< Custom GPU render state for ocean pass. */
    SDL_GPURenderState* fogRenderState;      /**< Custom GPU render state for fog-of-war pass. */
    SDL_GPUSampler* oceanRepeatSampler;      /**< Repeat sampler used by caustic texture binding. */
    OceanUniforms oceanUniforms;             /**< Runtime uniforms for shader animation. */
    FogUniforms fogUniforms;                 /**< Runtime uniforms for fog animation. */
    double oceanTimeSeconds;                 /**< Accumulated ocean time. */
    double fogTimeSeconds;                   /**< Accumulated fog time. */

    /**
     * @brief Release all runtime GPU/texture resources owned by the scene.
     */
    void releaseOceanResources(void);

    /**
     * @brief Reset uniform values to a sane default preset.
     */
    void resetOceanUniforms(void);

    /**
     * @brief Reset fog uniform values to a sane default preset.
     */
    void resetFogUniforms(void);

    /**
     * @brief Push current uniforms to GPU fragment slot 0.
     * @return True when upload succeeded.
     */
    bool uploadOceanUniforms(void);

    /**
     * @brief Push current fog uniforms to GPU fragment slot 0.
     * @return True when upload succeeded.
     */
    bool uploadFogUniforms(void);

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
