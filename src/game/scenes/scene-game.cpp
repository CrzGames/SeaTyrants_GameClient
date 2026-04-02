#include "game/scenes/scene-game.h"

#include <RC2D/RC2D_internal.h>

#include "game/scenes/scene-manager.h"

/**
 * @brief Construct gameplay scene with safe defaults.
 */
GameScene::GameScene(void)
    // Initialize water texture handle.
    : waterTileImage{},
      // Initialize caustic texture handle.
      causticTileImage{},
      // Initialize atlas handle.
      elite27Atlas{},
      // No loaded shader yet.
      oceanFragmentShader(nullptr),
      // No render state yet.
      oceanRenderState(nullptr),
      // No sampler yet.
      oceanRepeatSampler(nullptr),
      // Initialize uniforms with zeros.
      oceanUniforms{},
      // Reset time accumulator.
      oceanTimeAccum(0.0),
      // Start animation at frame 0.
      eliteFrameIndex(0),
      // Reset animation accumulator.
      eliteFrameAccumulator(0.0)
{
    // Constructor body intentionally empty.
}

/**
 * @brief Destroy GPU objects created for ocean rendering.
 */
void GameScene::destroyOceanRenderState(void)
{
    // Destroy render state if allocated.
    if (oceanRenderState != nullptr)
    {
        SDL_DestroyGPURenderState(oceanRenderState);
        oceanRenderState = nullptr;
    }

    // Release sampler if allocated.
    if (oceanRepeatSampler != nullptr)
    {
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, oceanRepeatSampler);
        oceanRepeatSampler = nullptr;
    }

    // Release shader if allocated.
    if (oceanFragmentShader != nullptr)
    {
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(oceanFragmentShader));
        oceanFragmentShader = nullptr;
    }
}

/**
 * @brief Initialize ocean shader pipeline state.
 * @return True on success, false on failure.
 */
bool GameScene::initializeOceanRenderState(void)
{
    // Load ocean fragment shader from RC2D shader storage.
    oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);

    // Abort if shader failed to load.
    if (oceanFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to load ocean shader: %s", SDL_GetError());
        return false;
    }

    // Build a repeat sampler for tiled water lookups.
    SDL_GPUSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Create the sampler on the RC2D GPU device.
    oceanRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);

    // Abort if sampler creation fails.
    if (oceanRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to create ocean repeat sampler: %s", SDL_GetError());
        destroyOceanRenderState();
        return false;
    }

    // Load base water texture from assets.
    waterTileImage = rc2d_graphics_loadImageFromStorage("assets/images/tile-water.png", RC2D_STORAGE_TITLE);

    // Abort if water texture is missing.
    if (waterTileImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to load water texture");
        destroyOceanRenderState();
        return false;
    }

    // Load optional caustic texture for future extensions.
    causticTileImage = rc2d_graphics_loadImageFromStorage("assets/images/tile-caustic.png", RC2D_STORAGE_TITLE);

    // Keep running even if caustic texture is missing.
    if (causticTileImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "Failed to load caustic texture, continuing without it.");
    }

    // Query properties for the water SDL texture.
    SDL_PropertiesID properties = SDL_GetTextureProperties(waterTileImage.sdl_texture);

    // Abort if properties query fails.
    if (!properties)
    {
        RC2D_log(RC2D_LOG_ERROR, "SDL_GetTextureProperties failed for water texture: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    // Fetch underlying GPU texture pointer used by SDL renderer.
    SDL_GPUTexture* waterGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(properties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));

    // Abort if GPU texture pointer is unavailable.
    if (waterGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "No GPU texture pointer found for water texture");
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    // Bind water texture and repeat sampler for shader slot t1/s1.
    SDL_GPUTextureSamplerBinding samplerBindings[1] = {};
    samplerBindings[0].texture = waterGpuTexture;
    samplerBindings[0].sampler = oceanRepeatSampler;

    // Build render-state descriptor using fragment shader and extra sampler binding.
    SDL_GPURenderStateCreateInfo renderStateCreateInfo = {};
    renderStateCreateInfo.fragment_shader = oceanFragmentShader;
    renderStateCreateInfo.num_sampler_bindings = 1;
    renderStateCreateInfo.sampler_bindings = samplerBindings;

    // Create render state on the RC2D renderer.
    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &renderStateCreateInfo);

    // Abort if render state creation fails.
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to create ocean render state: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    // Upload initial uniform values to fragment slot 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms)))
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to upload initial ocean uniforms: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    // Initialization succeeded.
    return true;
}

/**
 * @brief Update ocean uniform values every frame.
 */
void GameScene::updateOceanUniforms(int outputWidth, int outputHeight, double dt)
{
    // Ignore updates if render state is unavailable.
    if (oceanRenderState == nullptr)
    {
        return;
    }

    // Advance ocean animation time.
    oceanTimeAccum += dt;

    // Update uniform time value.
    oceanUniforms.params0[0] = static_cast<float>(oceanTimeAccum);

    // Update uniform output width value.
    oceanUniforms.params1[0] = static_cast<float>(outputWidth);

    // Update uniform output height value.
    oceanUniforms.params1[1] = static_cast<float>(outputHeight);

    // Upload refreshed uniforms to GPU.
    SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms));
}

/**
 * @brief Release gameplay resources when leaving scene.
 */
void GameScene::unload(void)
{
    // Release GPU shader/render-state/sampler resources.
    destroyOceanRenderState();

    // Release water image texture.
    rc2d_graphics_freeImage(&waterTileImage);

    // Release optional caustic texture.
    rc2d_graphics_freeImage(&causticTileImage);

    // Release atlas texture and frame metadata.
    rc2d_tp_freeAtlas(&elite27Atlas);

    // Reset uniforms to zero state.
    oceanUniforms = {};

    // Reset ocean timer.
    oceanTimeAccum = 0.0;

    // Reset animation frame index.
    eliteFrameIndex = 0;

    // Reset animation accumulator.
    eliteFrameAccumulator = 0.0;

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Game Scene Unloaded\n");
}

/**
 * @brief Prepare gameplay resources when entering scene.
 */
void GameScene::load(void)
{
    // Initialize shader uniform: time.
    oceanUniforms.params0[0] = 0.0f;

    // Initialize shader uniform: distortion strength.
    oceanUniforms.params0[1] = 0.6f;

    // Initialize shader uniform: pixel amplitude.
    oceanUniforms.params0[2] = 30.0f;

    // Initialize shader uniform: texture tiling factor.
    oceanUniforms.params0[3] = 3.0f;

    // Initialize shader uniform: fallback width.
    oceanUniforms.params1[0] = 1920.0f;

    // Initialize shader uniform: fallback height.
    oceanUniforms.params1[1] = 1080.0f;

    // Initialize shader uniform: speed.
    oceanUniforms.params1[2] = 0.60f;

    // Initialize shader uniform: fresnel factor.
    oceanUniforms.params1[3] = 0.25f;

    // Reset ocean timer.
    oceanTimeAccum = 0.0;

    // Reset animation frame.
    eliteFrameIndex = 0;

    // Reset frame accumulator.
    eliteFrameAccumulator = 0.0;

    // Build ocean render pipeline resources.
    if (!initializeOceanRenderState())
    {
        RC2D_log(RC2D_LOG_WARN, "Ocean render state could not be initialized.");
    }

    // Load gameplay atlas used for animation sample.
    elite27Atlas = rc2d_tp_loadAtlasFromStorage("assets/atlas/elite27/elite27.json", RC2D_STORAGE_TITLE);

    // Warn if atlas could not be loaded.
    if (elite27Atlas.atlas_image.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "Failed to load elite27 atlas");
    }

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Game Scene Loaded\n");
}

/**
 * @brief Update gameplay frame state.
 */
void GameScene::update(double dt)
{
    // Default dimensions when renderer is unavailable.
    int outputWidth = 0;

    // Default dimensions when renderer is unavailable.
    int outputHeight = 0;

    // Read actual output size from renderer.
    if (rc2d_engine_state.renderer != nullptr)
    {
        SDL_GetCurrentRenderOutputSize(rc2d_engine_state.renderer, &outputWidth, &outputHeight);
    }

    // Update ocean uniforms with current size and time.
    updateOceanUniforms(outputWidth, outputHeight, dt);

    // Accumulate time for atlas animation stepping.
    eliteFrameAccumulator += dt;

    // Advance one or more frames when enough time elapsed.
    while (eliteFrameAccumulator >= kElite27FrameDurationSeconds)
    {
        // Consume one frame worth of time.
        eliteFrameAccumulator -= kElite27FrameDurationSeconds;

        // Move to next frame with wrap-around.
        eliteFrameIndex = (eliteFrameIndex + 1) % kElite27FrameCount;
    }
}

/**
 * @brief Draw gameplay visuals.
 */
void GameScene::draw(void)
{
    // Read visible safe rectangle for render placement.
    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    // Draw ocean shader pass if resources are ready.
    if (waterTileImage.sdl_texture != nullptr && oceanRenderState != nullptr)
    {
        // Enable custom GPU render state.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);

        // Draw base water texture with shader effect.
        SDL_RenderTexture(rc2d_engine_state.renderer, waterTileImage.sdl_texture, nullptr, &visibleRect);

        // Restore default render state for following draws.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }

    // Draw animated atlas sample if atlas is available.
    if (elite27Atlas.atlas_image.sdl_texture != nullptr)
    {
        // Compute approximate centered x coordinate.
        const float spriteX = visibleRect.x + (visibleRect.w * 0.5f) - 266.0f;

        // Compute approximate centered y coordinate.
        const float spriteY = visibleRect.y + (visibleRect.h * 0.5f) - 280.0f;

        // Draw current animation frame from atlas.
        rc2d_tp_drawFrameByName(
            &elite27Atlas,
            kElite27FrameNames[eliteFrameIndex],
            spriteX,
            spriteY,
            0.0,
            1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            false,
            false);
    }
}

/**
 * @brief Handle keyboard events in gameplay scene.
 */
void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Parameter unused in current behavior.
    (void)key;

    // Parameter unused in current behavior.
    (void)scancode;

    // Parameter unused in current behavior.
    (void)mod;

    // Parameter unused in current behavior.
    (void)keyboardID;

    // Ignore held-key repeats.
    if (isrepeat)
    {
        return;
    }

    // M key returns to menu scene.
    if (keycode == SDLK_M && sceneManager != nullptr)
    {
        sceneManager->changeScene("menu");
        return;
    }

    // E key opens editor map scene.
    if (keycode == SDLK_E && sceneManager != nullptr)
    {
        sceneManager->changeScene("editormap");
        return;
    }

    // ESC quits app.
    if (keycode == SDLK_ESCAPE)
    {
        rc2d_event_quit();
    }
}

/**
 * @brief Handle mouse press in gameplay scene.
 */
void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Parameter unused in current behavior.
    (void)x;

    // Parameter unused in current behavior.
    (void)y;

    // Parameter unused in current behavior.
    (void)button;

    // Parameter unused in current behavior.
    (void)clicks;

    // Parameter unused in current behavior.
    (void)mouseID;
}
