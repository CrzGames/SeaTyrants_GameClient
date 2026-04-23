#include "game/shaders/vision-cloud-shader.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>

#include <RC2D/RC2D_internal.h>

#include "core/context.h"

VisionCloudShader::VisionCloudShader(void)
    // Initialise la texture de noise.
    : cloudNoiseTexture{},
      // Initialise le pointeur shader.
      visionCloudFragmentShader(nullptr),
      // Initialise le render state.
      visionCloudRenderState(nullptr),
      // Initialise le bloc uniforms.
      visionCloudUniforms{},
      // Initialise le temps cumule.
      visionCloudTimeSeconds(0.0)
{
    // Reinitialise les uniforms du pass nuages.
    resetUniforms();
}

void VisionCloudShader::unload(void)
{
    // Detruit le render state si present.
    if (this->visionCloudRenderState != nullptr)
    {
#if RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
        // Desinscrit le state du systeme hot-reload.
        rc2d_gpu_untrackGraphicsRenderState(&this->visionCloudRenderState);
#endif // RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
        // Detruit l'objet GPU render state.
        SDL_DestroyGPURenderState(this->visionCloudRenderState);
        // Annule le pointeur local.
        this->visionCloudRenderState = nullptr;
    }

    // Libere le shader fragment si present.
    if (this->visionCloudFragmentShader != nullptr)
    {
        // Relache le shader GPU cote device.
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(this->visionCloudFragmentShader));
        // Annule le pointeur local.
        this->visionCloudFragmentShader = nullptr;
    }

    // Libere la texture de noise.
    ResetStorageImageRef(&this->cloudNoiseTexture);
}

void VisionCloudShader::resetUniforms(void)
{
    // Reinitialise la memoire des uniforms.
    this->visionCloudUniforms = {};

    // Reinitialise le temps d'animation.
    this->visionCloudTimeSeconds = 0.0;

    // params0: time, noiseScale, driftSpeed, cloudCoverage.
    this->visionCloudUniforms.params0[0] = 0.0f;
    this->visionCloudUniforms.params0[1] = 1.000f;
    this->visionCloudUniforms.params0[2] = 0.175f;
    this->visionCloudUniforms.params0[3] = 1.0f;

    // params1: shadowStrength, alphaMax, edgeSoftness, blackCutoff.
    this->visionCloudUniforms.params1[0] = 1.00f;
    this->visionCloudUniforms.params1[1] = 0.45f;
    this->visionCloudUniforms.params1[2] = 1.0f;
    this->visionCloudUniforms.params1[3] = 0.006f;

    // params2: view rect gameplay.
    this->visionCloudUniforms.params2[0] = 0.0f;
    this->visionCloudUniforms.params2[1] = 0.0f;
    this->visionCloudUniforms.params2[2] = 1.0f;
    this->visionCloudUniforms.params2[3] = 1.0f;

    // params3: origine map + taille tuile.
    this->visionCloudUniforms.params3[0] = 0.0f;
    this->visionCloudUniforms.params3[1] = 0.0f;
    this->visionCloudUniforms.params3[2] = 1.0f;
    this->visionCloudUniforms.params3[3] = 1.0f;

    // params4: position/rayon joueur.
    this->visionCloudUniforms.params4[0] = 0.0f;
    this->visionCloudUniforms.params4[1] = 0.0f;
    this->visionCloudUniforms.params4[2] = 1.0f;
    this->visionCloudUniforms.params4[3] = 2.0f;

    // params5: teinte d'ombrage nuageux.
    this->visionCloudUniforms.params5[0] = 0.09f;
    this->visionCloudUniforms.params5[1] = 0.12f;
    this->visionCloudUniforms.params5[2] = 0.16f;
    this->visionCloudUniforms.params5[3] = 0.0f;
}

bool VisionCloudShader::uploadUniforms(void)
{
    // Verifie que le render state existe.
    if (this->visionCloudRenderState == nullptr)
    {
        return false;
    }

    // Envoie le bloc uniforms dans le slot fragment 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(this->visionCloudRenderState, 0, &this->visionCloudUniforms, sizeof(this->visionCloudUniforms)))
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: SDL_SetGPURenderStateFragmentUniforms failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool VisionCloudShader::load(void)
{
    // Libere un eventuel etat precedent.
    unload();

    // Reinitialise les uniforms avant un nouveau chargement.
    this->resetUniforms();

    // Charge la texture de nuages (noise).
    this->cloudNoiseTexture = LoadStorageImage("assets/images/shaders/visionclouds/cloud-group-noise.png", RC2D_STORAGE_TITLE);
    if (this->cloudNoiseTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: texture absente (assets/images/shaders/visionclouds/cloud-group-noise.png)");
        unload();
        return false;
    }

    // Active le filtrage lineaire.
    if (!SDL_SetTextureScaleMode(this->cloudNoiseTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: echec SDL_SetTextureScaleMode: %s", SDL_GetError());
    }

    // Active le blend alpha.
    if (!SDL_SetTextureBlendMode(this->cloudNoiseTexture.sdl_texture, SDL_BLENDMODE_BLEND))
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: echec SDL_SetTextureBlendMode: %s", SDL_GetError());
    }

    // Charge le shader fragment nuages.
    this->visionCloudFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("visionclouds.fragment", RC2D_STORAGE_TITLE);
    if (this->visionCloudFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: impossible de charger visionclouds.fragment");
        unload();
        return false;
    }

    // Cree le render state sans binding additionnel.
    SDL_GPURenderStateCreateInfo createInfo = {};
    createInfo.fragment_shader = this->visionCloudFragmentShader;
    createInfo.num_sampler_bindings = 0;
    createInfo.sampler_bindings = nullptr;
    this->visionCloudRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);
    if (this->visionCloudRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        unload();
        return false;
    }

#if RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
    // Enregistre le state pour le hot-reload shader.
    if (!rc2d_gpu_trackGraphicsRenderState("visionclouds.fragment", &this->visionCloudRenderState, 0, nullptr))
    {
        RC2D_log(RC2D_LOG_WARN, "VisionCloudShader: echec tracking GPURenderState pour hot-reload");
    }
#endif // RC2D_GPU_SHADER_HOT_RELOAD_ENABLED

    // Upload des uniforms initiaux.
    this->uploadUniforms();
    return true;
}

void VisionCloudShader::update(
    double dt,
    const SDL_FPoint& playerTile,
    float playerViewRangeTiles,
    float viewFalloffTiles)
{
    // Ignore l'update si le module n'est pas pret.
    if (!isReady())
    {
        return;
    }

    // Incremente le temps d'animation.
    this->visionCloudTimeSeconds += dt;
    this->visionCloudUniforms.params0[0] = static_cast<float>(this->visionCloudTimeSeconds);

    // Aligne l'espace shader sur la map/camera du gameplay.
    Map& map = GetCurrentMap();

    this->visionCloudUniforms.params2[0] = map.rect.x;
    this->visionCloudUniforms.params2[1] = map.rect.y;
    this->visionCloudUniforms.params2[2] = (std::max)(map.rect.w, 1.0f);
    this->visionCloudUniforms.params2[3] = (std::max)(map.rect.h, 1.0f);

    this->visionCloudUniforms.params3[0] = map.getOriginX();
    this->visionCloudUniforms.params3[1] = map.getOriginY();
    this->visionCloudUniforms.params3[2] = (std::max)(map.getTileWidth(), 1.0f);
    this->visionCloudUniforms.params3[3] = (std::max)(map.getTileHeight(), 1.0f);

    this->visionCloudUniforms.params4[0] = playerTile.x;
    this->visionCloudUniforms.params4[1] = playerTile.y;
    this->visionCloudUniforms.params4[2] = (std::max)(playerViewRangeTiles, 0.0f);
    this->visionCloudUniforms.params4[3] = (std::max)(viewFalloffTiles, 0.001f);

    // Upload uniforms mis a jour.
    this->uploadUniforms();
}

void VisionCloudShader::draw(const SDL_FRect& visibleRect)
{
    // Ignore le draw si le module n'est pas pret.
    if (!isReady())
    {
        return;
    }

    // Ignore le draw si la texture est absente.
    if (this->cloudNoiseTexture.sdl_texture == nullptr)
    {
        return;
    }

    // Active le state GPU nuages.
    SDL_SetGPURenderState(rc2d_engine_state.renderer, this->visionCloudRenderState);

    // Dessine le pass nuages sur la zone visible gameplay.
    SDL_RenderTexture(rc2d_engine_state.renderer, this->cloudNoiseTexture.sdl_texture, nullptr, &visibleRect);

    // Desactive le state GPU apres le draw.
    SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
}

bool VisionCloudShader::isReady(void) const
{
    const bool hasCloudTexture = (this->cloudNoiseTexture.sdl_texture != nullptr);
    const bool hasRenderState = (this->visionCloudRenderState != nullptr);
    return hasCloudTexture && hasRenderState;
}
