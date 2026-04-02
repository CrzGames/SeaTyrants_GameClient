#include "game/scenes/scene-game.h"

#include <RC2D/RC2D_internal.h>

#include "game/scenes/scene-manager.h"

GameScene::GameScene(void)
    : oceanTexture{},
      oceanTextureDetail{},
      causticTexture{},
      foamStreaksTexture{},
      macroWaterTexture{},
      depthWaterTexture{},
      oceanFragmentShader(nullptr),
      oceanRenderState(nullptr),
      oceanRepeatSampler(nullptr),
      oceanUniforms{},
      oceanTimeSeconds(0.0)
{
    resetOceanUniforms();
}

void GameScene::unload(void)
{
    releaseOceanResources();
}

void GameScene::load(void)
{
    releaseOceanResources();
    resetOceanUniforms();

    oceanTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-water-base-mint.png", RC2D_STORAGE_TITLE);
    if (oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load ocean texture assets/images/tile-water-base-red.png");
        return;
    }
    if (!SDL_SetTextureScaleMode(oceanTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for tile-water-base: %s", SDL_GetError());
    }

    oceanTextureDetail = rc2d_graphics_loadImageFromStorage("assets/images/tile-water-detail-mint.png", RC2D_STORAGE_TITLE);
    if (oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load ocean detail texture assets/images/tile-water-detail-red.png");
        return;
    }
    if (!SDL_SetTextureScaleMode(oceanTextureDetail.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for tile-water-detail: %s", SDL_GetError());
    }

    causticTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-caustic.png", RC2D_STORAGE_TITLE);
    if (causticTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load caustic texture assets/images/tile-caustic.png");
        return;
    }
    if (!SDL_SetTextureScaleMode(causticTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for tile-caustic: %s", SDL_GetError());
    }

    foamStreaksTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-foam-streaks.png", RC2D_STORAGE_TITLE);
    if (foamStreaksTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load foam streak texture assets/images/tile-foam-streaks.png");
        return;
    }
    if (!SDL_SetTextureScaleMode(foamStreaksTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for tile-foam-streaks: %s", SDL_GetError());
    }

    macroWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/water-macro.png", RC2D_STORAGE_TITLE);
    if (macroWaterTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load macro water texture assets/images/water-macro.png");
        return;
    }
    if (!SDL_SetTextureScaleMode(macroWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for water-macro: %s", SDL_GetError());
    }

    depthWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-water-depth.png", RC2D_STORAGE_TITLE);
    if (depthWaterTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN,
                 "GameScene: water depth texture missing (assets/images/tile-water-depth.png), using water-macro fallback");
    }
    else if (!SDL_SetTextureScaleMode(depthWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to set scale mode for tile-water-depth: %s", SDL_GetError());
    }

    oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);
    if (oceanFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load water.fragment shader");
        return;
    }

    SDL_GPUSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    oceanRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);
    if (oceanRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        return;
    }

    SDL_PropertiesID detailTextureProperties = SDL_GetTextureProperties(oceanTextureDetail.sdl_texture);
    if (!detailTextureProperties)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_GetTextureProperties failed for detail texture: %s", SDL_GetError());
        return;
    }

    SDL_GPUTexture* detailGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(detailTextureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    if (detailGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: missing GPU texture pointer for detail texture");
        return;
    }

    SDL_PropertiesID causticTextureProperties = SDL_GetTextureProperties(causticTexture.sdl_texture);
    if (!causticTextureProperties)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_GetTextureProperties failed for caustic texture: %s", SDL_GetError());
        return;
    }

    SDL_GPUTexture* causticGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(causticTextureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    if (causticGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: missing GPU texture pointer for caustic texture");
        return;
    }

    SDL_PropertiesID foamTextureProperties = SDL_GetTextureProperties(foamStreaksTexture.sdl_texture);
    if (!foamTextureProperties)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_GetTextureProperties failed for foam texture: %s", SDL_GetError());
        return;
    }

    SDL_GPUTexture* foamGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(foamTextureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    if (foamGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: missing GPU texture pointer for foam texture");
        return;
    }

    SDL_PropertiesID macroTextureProperties = SDL_GetTextureProperties(macroWaterTexture.sdl_texture);
    if (!macroTextureProperties)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_GetTextureProperties failed for macro water texture: %s", SDL_GetError());
        return;
    }

    SDL_GPUTexture* macroGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(macroTextureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    if (macroGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: missing GPU texture pointer for macro water texture");
        return;
    }

    SDL_GPUTexture* depthGpuTexture = nullptr;
    if (depthWaterTexture.sdl_texture != nullptr)
    {
        SDL_PropertiesID depthTextureProperties = SDL_GetTextureProperties(depthWaterTexture.sdl_texture);
        if (!depthTextureProperties)
        {
            RC2D_log(RC2D_LOG_WARN,
                     "GameScene: SDL_GetTextureProperties failed for water depth texture: %s (fallback to water-macro)",
                     SDL_GetError());
        }
        else
        {
            depthGpuTexture = static_cast<SDL_GPUTexture*>(
                SDL_GetPointerProperty(depthTextureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
            if (depthGpuTexture == nullptr)
            {
                RC2D_log(RC2D_LOG_WARN, "GameScene: missing GPU texture pointer for water depth texture (fallback to water-macro)");
            }
        }
    }

    if (depthGpuTexture == nullptr)
    {
        depthGpuTexture = macroGpuTexture;
    }

    SDL_GPUTextureSamplerBinding samplerBindings[5] = {};
    samplerBindings[0].texture = detailGpuTexture;
    samplerBindings[0].sampler = oceanRepeatSampler;
    samplerBindings[1].texture = causticGpuTexture;
    samplerBindings[1].sampler = oceanRepeatSampler;
    samplerBindings[2].texture = foamGpuTexture;
    samplerBindings[2].sampler = oceanRepeatSampler;
    samplerBindings[3].texture = macroGpuTexture;
    samplerBindings[3].sampler = oceanRepeatSampler;
    samplerBindings[4].texture = depthGpuTexture;
    samplerBindings[4].sampler = oceanRepeatSampler;

    SDL_GPURenderStateCreateInfo createInfo = {};
    createInfo.fragment_shader = oceanFragmentShader;
    createInfo.num_sampler_bindings = 5;
    createInfo.sampler_bindings = samplerBindings;

    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        return;
    }

    if (!rc2d_gpu_trackGraphicsRenderState("water.fragment", &oceanRenderState, 5, samplerBindings))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: failed to track ocean GPURenderState for shader hot-reload");
    }

    uploadOceanUniforms();
}

void GameScene::update(double dt)
{
    if (oceanRenderState == nullptr)
    {
        return;
    }

    int outputWidth = 0;
    int outputHeight = 0;
    if (rc2d_engine_state.renderer != nullptr)
    {
        SDL_GetCurrentRenderOutputSize(rc2d_engine_state.renderer, &outputWidth, &outputHeight);
    }

    oceanTimeSeconds += dt;
    oceanUniforms.params0[0] = static_cast<float>(oceanTimeSeconds);
    oceanUniforms.params1[0] = static_cast<float>(outputWidth);
    oceanUniforms.params1[1] = static_cast<float>(outputHeight);

    uploadOceanUniforms();
}

void GameScene::draw(void)
{
    if (oceanTexture.sdl_texture == nullptr)
    {
        return;
    }

    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    SDL_TextureAddressMode prevU = SDL_TEXTURE_ADDRESS_AUTO;
    SDL_TextureAddressMode prevV = SDL_TEXTURE_ADDRESS_AUTO;
    bool restoreAddressMode = SDL_GetRenderTextureAddressMode(rc2d_engine_state.renderer, &prevU, &prevV);
    SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, SDL_TEXTURE_ADDRESS_WRAP, SDL_TEXTURE_ADDRESS_WRAP);

    if (oceanRenderState != nullptr)
    {
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }
    else
    {
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
    }

    if (restoreAddressMode)
    {
        SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, prevU, prevV);
    }
}

void GameScene::keypressed(
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

    if (keycode == SDLK_M && sceneManager != nullptr)
    {
        sceneManager->changeScene("menu");
        return;
    }

    if (keycode == SDLK_E && sceneManager != nullptr)
    {
        sceneManager->changeScene("editormap");
        return;
    }

    if (keycode == SDLK_ESCAPE)
    {
        rc2d_event_quit();
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)button;
    (void)clicks;
    (void)mouseID;
}

void GameScene::releaseOceanResources(void)
{
    if (oceanRenderState != nullptr)
    {
        rc2d_gpu_untrackGraphicsRenderState(&oceanRenderState);
        SDL_DestroyGPURenderState(oceanRenderState);
        oceanRenderState = nullptr;
    }

    if (oceanRepeatSampler != nullptr)
    {
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, oceanRepeatSampler);
        oceanRepeatSampler = nullptr;
    }

    if (oceanFragmentShader != nullptr)
    {
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(oceanFragmentShader));
        oceanFragmentShader = nullptr;
    }

    rc2d_graphics_freeImage(&oceanTexture);
    rc2d_graphics_freeImage(&oceanTextureDetail);
    rc2d_graphics_freeImage(&causticTexture);
    rc2d_graphics_freeImage(&foamStreaksTexture);
    rc2d_graphics_freeImage(&macroWaterTexture);
    rc2d_graphics_freeImage(&depthWaterTexture);
}

void GameScene::resetOceanUniforms(void)
{
    oceanUniforms = {};
    oceanTimeSeconds = 0.0;

    oceanUniforms.params0[0] = 0.0f;   // time
    oceanUniforms.params0[1] = 0.74f;  // waveStrength
    oceanUniforms.params0[2] = 3.6f;   // pixelAmplitude
    oceanUniforms.params0[3] = 2.35f;  // tiling

    oceanUniforms.params1[0] = 1920.0f; // width
    oceanUniforms.params1[1] = 1080.0f; // height
    oceanUniforms.params1[2] = 0.62f;   // speed
    oceanUniforms.params1[3] = 0.36f;   // foamIntensity

    // params2.x colorMode: 0.0 = blue shading, 1.0 = neutral shading.
    oceanUniforms.params2[0] = 1.0f;
    // params2.y fresnelStrength: stronger angle-dependent reflection.
    oceanUniforms.params2[1] = 0.88f;
    // params2.z sunGlintStrength: specular sun highlights on wave crests.
    oceanUniforms.params2[2] = 0.74f;
    // params2.w whitecapBoost: additional foam from steep/windy wave slopes.
    oceanUniforms.params2[3] = 0.68f;
}

bool GameScene::uploadOceanUniforms(void)
{
    if (oceanRenderState == nullptr)
    {
        return false;
    }

    if (!SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms)))
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: SDL_SetGPURenderStateFragmentUniforms failed: %s", SDL_GetError());
        return false;
    }

    return true;
}
