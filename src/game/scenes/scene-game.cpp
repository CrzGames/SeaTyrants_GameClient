#include "game/scenes/scene-game.h"

#include <RC2D/RC2D_internal.h>

#include "game/scenes/scene-manager.h"

GameScene::GameScene(void)
    : oceanTexture{},
      oceanTextureDetail{},
      causticTexture{},
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

    oceanTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-water1.png", RC2D_STORAGE_TITLE);
    if (oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load ocean texture assets/images/tile-water1.png");
        return;
    }

    oceanTextureDetail = rc2d_graphics_loadImageFromStorage("assets/images/tile-water2.png", RC2D_STORAGE_TITLE);
    if (oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load ocean detail texture assets/images/tile-water2.png");
        return;
    }

    causticTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-caustic.png", RC2D_STORAGE_TITLE);
    if (causticTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: failed to load caustic texture assets/images/tile-caustic.png");
        return;
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

    SDL_GPUTextureSamplerBinding samplerBindings[2] = {};
    samplerBindings[0].texture = detailGpuTexture;
    samplerBindings[0].sampler = oceanRepeatSampler;
    samplerBindings[1].texture = causticGpuTexture;
    samplerBindings[1].sampler = oceanRepeatSampler;

    SDL_GPURenderStateCreateInfo createInfo = {};
    createInfo.fragment_shader = oceanFragmentShader;
    createInfo.num_sampler_bindings = 2;
    createInfo.sampler_bindings = samplerBindings;

    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        return;
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

    if (oceanRenderState != nullptr)
    {
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
        return;
    }

    SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
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
}

void GameScene::resetOceanUniforms(void)
{
    oceanUniforms = {};
    oceanTimeSeconds = 0.0;

    oceanUniforms.params0[0] = 0.0f;   // time
    oceanUniforms.params0[1] = 1.00f;  // waveStrength
    oceanUniforms.params0[2] = 20.0f;  // pixelAmplitude
    oceanUniforms.params0[3] = 11.0f;  // tiling

    oceanUniforms.params1[0] = 1920.0f; // width
    oceanUniforms.params1[1] = 1080.0f; // height
    oceanUniforms.params1[2] = 0.60f;   // speed
    oceanUniforms.params1[3] = 0.82f;   // foamIntensity
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
