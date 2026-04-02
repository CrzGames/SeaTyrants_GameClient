#include "game/scenes/scene-game.h"

#include <RC2D/RC2D_internal.h>

#include "game/scenes/scene-manager.h"

namespace {
constexpr const char* kElite27FrameNames[] = {
    "1.png",
    "2.png",
    "3.png",
    "4.png",
    "5.png",
    "6.png",
    "7.png",
    "8.png"
};

constexpr int kElite27FrameCount = static_cast<int>(sizeof(kElite27FrameNames) / sizeof(kElite27FrameNames[0]));
constexpr double kElite27FrameDurationSeconds = 0.10;
} // namespace

GameScene::GameScene(void)
    : waterTileImage{},
      causticTileImage{},
      elite27Atlas{},
      oceanFragmentShader(nullptr),
      oceanRenderState(nullptr),
      oceanRepeatSampler(nullptr),
      oceanUniforms{},
      oceanTimeAccum(0.0),
      eliteFrameIndex(0),
      eliteFrameAccumulator(0.0)
{
}

void GameScene::destroyOceanRenderState(void)
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
}

bool GameScene::initializeOceanRenderState(void)
{
    oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);
    if (oceanFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to load ocean shader: %s", SDL_GetError());
        return false;
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
        RC2D_log(RC2D_LOG_ERROR, "Failed to create ocean repeat sampler: %s", SDL_GetError());
        destroyOceanRenderState();
        return false;
    }

    waterTileImage = rc2d_graphics_loadImageFromStorage("assets/images/tile-water.png", RC2D_STORAGE_TITLE);
    if (waterTileImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to load water texture");
        destroyOceanRenderState();
        return false;
    }

    causticTileImage = rc2d_graphics_loadImageFromStorage("assets/images/tile-caustic.png", RC2D_STORAGE_TITLE);
    if (causticTileImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "Failed to load caustic texture, continuing without it.");
    }

    SDL_PropertiesID properties = SDL_GetTextureProperties(waterTileImage.sdl_texture);
    if (!properties)
    {
        RC2D_log(RC2D_LOG_ERROR, "SDL_GetTextureProperties failed for water texture: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    SDL_GPUTexture* waterGpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(properties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    if (waterGpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "No GPU texture pointer found for water texture");
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    SDL_GPUTextureSamplerBinding samplerBindings[1] = {};
    samplerBindings[0].texture = waterGpuTexture;
    samplerBindings[0].sampler = oceanRepeatSampler;

    SDL_GPURenderStateCreateInfo renderStateCreateInfo = {};
    renderStateCreateInfo.fragment_shader = oceanFragmentShader;
    renderStateCreateInfo.num_sampler_bindings = 1;
    renderStateCreateInfo.sampler_bindings = samplerBindings;

    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &renderStateCreateInfo);
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to create ocean render state: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    if (!SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms)))
    {
        RC2D_log(RC2D_LOG_ERROR, "Failed to upload initial ocean uniforms: %s", SDL_GetError());
        destroyOceanRenderState();
        rc2d_graphics_freeImage(&waterTileImage);
        rc2d_graphics_freeImage(&causticTileImage);
        return false;
    }

    return true;
}

void GameScene::updateOceanUniforms(int outputWidth, int outputHeight, double dt)
{
    if (oceanRenderState == nullptr)
    {
        return;
    }

    oceanTimeAccum += dt;

    oceanUniforms.params0[0] = static_cast<float>(oceanTimeAccum);
    oceanUniforms.params1[0] = static_cast<float>(outputWidth);
    oceanUniforms.params1[1] = static_cast<float>(outputHeight);

    SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms));
}

void GameScene::unload(void)
{
    destroyOceanRenderState();

    rc2d_graphics_freeImage(&waterTileImage);
    rc2d_graphics_freeImage(&causticTileImage);
    rc2d_tp_freeAtlas(&elite27Atlas);

    oceanUniforms = {};
    oceanTimeAccum = 0.0;
    eliteFrameIndex = 0;
    eliteFrameAccumulator = 0.0;

    RC2D_log(RC2D_LOG_INFO, "Game Scene Unloaded\n");
}

void GameScene::load(void)
{
    oceanUniforms.params0[0] = 0.0f;
    oceanUniforms.params0[1] = 0.6f;
    oceanUniforms.params0[2] = 30.0f;
    oceanUniforms.params0[3] = 3.0f;

    oceanUniforms.params1[0] = 1920.0f;
    oceanUniforms.params1[1] = 1080.0f;
    oceanUniforms.params1[2] = 0.60f;
    oceanUniforms.params1[3] = 0.25f;

    oceanTimeAccum = 0.0;
    eliteFrameIndex = 0;
    eliteFrameAccumulator = 0.0;

    if (!initializeOceanRenderState())
    {
        RC2D_log(RC2D_LOG_WARN, "Ocean render state could not be initialized.");
    }

    elite27Atlas = rc2d_tp_loadAtlasFromStorage("assets/atlas/elite27/elite27.json", RC2D_STORAGE_TITLE);
    if (elite27Atlas.atlas_image.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "Failed to load elite27 atlas");
    }

    RC2D_log(RC2D_LOG_INFO, "Game Scene Loaded\n");
}

void GameScene::update(double dt)
{
    int outputWidth = 0;
    int outputHeight = 0;

    if (rc2d_engine_state.renderer != nullptr)
    {
        SDL_GetCurrentRenderOutputSize(rc2d_engine_state.renderer, &outputWidth, &outputHeight);
    }

    updateOceanUniforms(outputWidth, outputHeight, dt);

    eliteFrameAccumulator += dt;
    while (eliteFrameAccumulator >= kElite27FrameDurationSeconds)
    {
        eliteFrameAccumulator -= kElite27FrameDurationSeconds;
        eliteFrameIndex = (eliteFrameIndex + 1) % kElite27FrameCount;
    }
}

void GameScene::draw(void)
{
    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    if (waterTileImage.sdl_texture != nullptr && oceanRenderState != nullptr)
    {
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);
        SDL_RenderTexture(rc2d_engine_state.renderer, waterTileImage.sdl_texture, nullptr, &visibleRect);
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }

    if (elite27Atlas.atlas_image.sdl_texture != nullptr)
    {
        const float spriteX = visibleRect.x + (visibleRect.w * 0.5f) - 266.0f;
        const float spriteY = visibleRect.y + (visibleRect.h * 0.5f) - 280.0f;

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
