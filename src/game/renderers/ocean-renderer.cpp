#include "game/renderers/ocean-renderer.h"

#include <RC2D/RC2D_internal.h>

OceanRenderer::OceanRenderer(void)
    // Initialise la texture base.
    : oceanTexture{},
      // Initialise la texture détail.
      oceanTextureDetail{},
      // Initialise la texture caustiques.
      causticTexture{},
      // Initialise la texture traînées d'écume.
      foamStreaksTexture{},
      // Initialise la texture macro.
      macroWaterTexture{},
      // Initialise la texture depth.
      depthWaterTexture{},
      // Initialise le pointeur shader.
      oceanFragmentShader(nullptr),
      // Initialise le render state.
      oceanRenderState(nullptr),
      // Initialise le sampler repeat.
      oceanRepeatSampler(nullptr),
      // Initialise le bloc uniforms.
      oceanUniforms{},
      // Initialise le temps cumulé.
      oceanTimeSeconds(0.0)
{
    // Réinitialise les uniforms océan.
    resetUniforms();
}

void OceanRenderer::unload(void)
{
    // Détruit le render state si présent.
    if (oceanRenderState != nullptr)
    {
        // Désinscrit le state du système hot-reload.
        rc2d_gpu_untrackGraphicsRenderState(&oceanRenderState);

        // Détruit l'objet GPU render state.
        SDL_DestroyGPURenderState(oceanRenderState);

        // Annule le pointeur local.
        oceanRenderState = nullptr;
    }

    // Libère le sampler repeat si présent.
    if (oceanRepeatSampler != nullptr)
    {
        // Relâche la ressource sampler côté device.
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, oceanRepeatSampler);

        // Annule le pointeur local.
        oceanRepeatSampler = nullptr;
    }

    // Libère le shader fragment si présent.
    if (oceanFragmentShader != nullptr)
    {
        // Relâche le shader GPU côté device.
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(oceanFragmentShader));

        // Annule le pointeur local.
        oceanFragmentShader = nullptr;
    }

    // Libère la texture base.
    rc2d_graphics_freeImage(&oceanTexture);

    // Libère la texture détail.
    rc2d_graphics_freeImage(&oceanTextureDetail);

    // Libère la texture caustiques.
    rc2d_graphics_freeImage(&causticTexture);

    // Libère la texture écume.
    rc2d_graphics_freeImage(&foamStreaksTexture);

    // Libère la texture macro.
    rc2d_graphics_freeImage(&macroWaterTexture);

    // Libère la texture depth.
    rc2d_graphics_freeImage(&depthWaterTexture);
}

void OceanRenderer::resetUniforms(void)
{
    // Réinitialise la mémoire des uniforms.
    oceanUniforms = {};

    // Réinitialise le temps d'animation.
    oceanTimeSeconds = 0.0;

    // Initialise le temps shader.
    oceanUniforms.params0[0] = 0.0f;

    // Initialise la force des vagues.
    oceanUniforms.params0[1] = 0.74f;

    // Initialise l'amplitude pixel.
    oceanUniforms.params0[2] = 3.6f;

    // Initialise le tiling.
    oceanUniforms.params0[3] = 2.35f;

    // Initialise la largeur de rendu par défaut.
    oceanUniforms.params1[0] = 1920.0f;

    // Initialise la hauteur de rendu par défaut.
    oceanUniforms.params1[1] = 1080.0f;

    // Initialise la vitesse d'animation.
    oceanUniforms.params1[2] = 0.62f;

    // Initialise l'intensité d'écume.
    oceanUniforms.params1[3] = 0.36f;

    // Initialise colorMode sur mode neutral (sera ajusté à load).
    oceanUniforms.params2[0] = 0.0f;

    // Initialise la force fresnel.
    oceanUniforms.params2[1] = 0.88f;

    // Initialise la force de glint solaire.
    oceanUniforms.params2[2] = 0.74f;

    // Initialise le boost whitecaps.
    oceanUniforms.params2[3] = 0.68f;
}

bool OceanRenderer::uploadUniforms(void)
{
    // Vérifie que le render state existe.
    if (oceanRenderState == nullptr)
    {
        return false;
    }

    // Envoie le bloc uniforms dans le slot fragment 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms)))
    {
        // Journalise l'erreur SDL.
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: SDL_SetGPURenderStateFragmentUniforms failed: %s", SDL_GetError());

        // Signale un échec d'upload.
        return false;
    }

    // Signale un upload réussi.
    return true;
}

bool OceanRenderer::resolveGpuTexture(const RC2D_Image& image, const char* label, SDL_GPUTexture** outGpuTexture) const
{
    // Vérifie le pointeur de sortie.
    if (outGpuTexture == nullptr)
    {
        return false;
    }

    // Initialise la sortie à null par sécurité.
    *outGpuTexture = nullptr;

    // Vérifie que la texture SDL existe.
    if (image.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: texture absente pour %s", label);
        return false;
    }

    // Récupère le conteneur de propriétés SDL.
    SDL_PropertiesID textureProperties = SDL_GetTextureProperties(image.sdl_texture);

    // Vérifie que les propriétés ont été trouvées.
    if (!textureProperties)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: SDL_GetTextureProperties failed for %s: %s", label, SDL_GetError());
        return false;
    }

    // Extrait le pointeur GPU texture depuis les propriétés SDL.
    SDL_GPUTexture* gpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(textureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));

    // Vérifie que le pointeur GPU est valide.
    if (gpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: pointeur GPU texture manquant pour %s", label);
        return false;
    }

    // Retourne la texture GPU au code appelant.
    *outGpuTexture = gpuTexture;

    // Signale la réussite.
    return true;
}

bool OceanRenderer::load(const char* oceanBaseTexturePath, const char* oceanDetailTexturePath)
{
    // Libère un éventuel état précédent.
    unload();

    // Réinitialise les uniforms avant un nouveau chargement.
    resetUniforms();

    // Définit le fallback de texture base.
    const char* fallbackBaseTexturePath = "assets/images/tile-water-base-blue.png";

    // Définit le fallback de texture détail.
    const char* fallbackDetailTexturePath = "assets/images/tile-water-detail-blue.png";

    // Sécurise le chemin base demandé.
    const char* requestedBaseTexturePath = (oceanBaseTexturePath != nullptr) ? oceanBaseTexturePath : fallbackBaseTexturePath;

    // Sécurise le chemin détail demandé.
    const char* requestedDetailTexturePath = (oceanDetailTexturePath != nullptr) ? oceanDetailTexturePath : fallbackDetailTexturePath;

    // Conserve le chemin base réellement utilisé.
    const char* loadedBaseTexturePath = requestedBaseTexturePath;

    // Conserve le chemin détail réellement utilisé.
    const char* loadedDetailTexturePath = requestedDetailTexturePath;

    // Charge la texture base demandée.
    oceanTexture = rc2d_graphics_loadImageFromStorage(loadedBaseTexturePath, RC2D_STORAGE_TITLE);

    // Gère le fallback si la texture base est manquante.
    if (oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: base %s absente, fallback sur %s", loadedBaseTexturePath, fallbackBaseTexturePath);
        loadedBaseTexturePath = fallbackBaseTexturePath;
        oceanTexture = rc2d_graphics_loadImageFromStorage(loadedBaseTexturePath, RC2D_STORAGE_TITLE);
    }

    // Stoppe le chargement si la texture base reste absente.
    if (oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger la texture base %s", loadedBaseTexturePath);
        unload();
        return false;
    }

    // Active le filtrage linéaire sur la texture base.
    if (!SDL_SetTextureScaleMode(oceanTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode base: %s", SDL_GetError());
    }

    // Charge la texture détail demandée.
    oceanTextureDetail = rc2d_graphics_loadImageFromStorage(loadedDetailTexturePath, RC2D_STORAGE_TITLE);

    // Gère le fallback si la texture détail est manquante.
    if (oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: detail %s absent, fallback sur %s", loadedDetailTexturePath, fallbackDetailTexturePath);
        loadedDetailTexturePath = fallbackDetailTexturePath;
        oceanTextureDetail = rc2d_graphics_loadImageFromStorage(loadedDetailTexturePath, RC2D_STORAGE_TITLE);
    }

    // Stoppe le chargement si la texture détail reste absente.
    if (oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger la texture detail %s", loadedDetailTexturePath);
        unload();
        return false;
    }

    // Active le filtrage linéaire sur la texture détail.
    if (!SDL_SetTextureScaleMode(oceanTextureDetail.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode detail: %s", SDL_GetError());
    }

    // Détecte le mode bleu legacy via le nom de texture chargé.
    const bool isBlueLegacyBase = (SDL_strcmp(loadedBaseTexturePath, "assets/images/tile-water-base.png") == 0);

    // Détecte explicitement le variant bleu nommé.
    const bool isBlueVariantBase = (SDL_strcmp(loadedBaseTexturePath, "assets/images/tile-water-base-blue.png") == 0);

    // Applique le colorMode du shader water.
    oceanUniforms.params2[0] = (isBlueLegacyBase || isBlueVariantBase) ? 0.0f : 1.0f;

    // Charge la texture caustiques.
    causticTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-caustic.png", RC2D_STORAGE_TITLE);

    // Vérifie la disponibilité des caustiques.
    if (causticTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger assets/images/tile-caustic.png");
        unload();
        return false;
    }

    // Active le filtrage linéaire sur les caustiques.
    if (!SDL_SetTextureScaleMode(causticTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode caustic: %s", SDL_GetError());
    }

    // Charge la texture d'écume.
    foamStreaksTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-foam-streaks.png", RC2D_STORAGE_TITLE);

    // Vérifie la disponibilité de l'écume.
    if (foamStreaksTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger assets/images/tile-foam-streaks.png");
        unload();
        return false;
    }

    // Active le filtrage linéaire sur l'écume.
    if (!SDL_SetTextureScaleMode(foamStreaksTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode foam: %s", SDL_GetError());
    }

    // Charge la texture macro.
    macroWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/water-macro.png", RC2D_STORAGE_TITLE);

    // Vérifie la disponibilité de la macro texture.
    if (macroWaterTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger assets/images/water-macro.png");
        unload();
        return false;
    }

    // Active le filtrage linéaire sur la macro texture.
    if (!SDL_SetTextureScaleMode(macroWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode macro: %s", SDL_GetError());
    }

    // Charge la texture depth dédiée.
    depthWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/tile-water-depth.png", RC2D_STORAGE_TITLE);

    // Active le filtrage linéaire de la depth map si présente.
    if (depthWaterTexture.sdl_texture != nullptr)
    {
        if (!SDL_SetTextureScaleMode(depthWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec SDL_SetTextureScaleMode depth: %s", SDL_GetError());
        }
    }
    else
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: tile-water-depth manquant, fallback macro actif");
    }

    // Charge le shader fragment water.
    oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);

    // Stoppe le chargement si le shader est absent.
    if (oceanFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: impossible de charger water.fragment");
        unload();
        return false;
    }

    // Prépare la structure de création du sampler.
    SDL_GPUSamplerCreateInfo samplerCreateInfo = {};

    // Configure le filtre min en linéaire.
    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;

    // Configure le filtre mag en linéaire.
    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;

    // Configure le mode mipmap en linéaire.
    samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

    // Active le wrap sur l'axe U.
    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Active le wrap sur l'axe V.
    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Active le wrap sur l'axe W.
    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Crée le sampler GPU repeat.
    oceanRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);

    // Stoppe le chargement si le sampler n'est pas créé.
    if (oceanRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Déclare le pointeur GPU pour la texture détail.
    SDL_GPUTexture* detailGpuTexture = nullptr;

    // Résout le pointeur GPU pour la texture détail.
    if (!resolveGpuTexture(oceanTextureDetail, "detail texture", &detailGpuTexture))
    {
        unload();
        return false;
    }

    // Déclare le pointeur GPU pour la texture caustiques.
    SDL_GPUTexture* causticGpuTexture = nullptr;

    // Résout le pointeur GPU pour la texture caustiques.
    if (!resolveGpuTexture(causticTexture, "caustic texture", &causticGpuTexture))
    {
        unload();
        return false;
    }

    // Déclare le pointeur GPU pour la texture écume.
    SDL_GPUTexture* foamGpuTexture = nullptr;

    // Résout le pointeur GPU pour la texture écume.
    if (!resolveGpuTexture(foamStreaksTexture, "foam texture", &foamGpuTexture))
    {
        unload();
        return false;
    }

    // Déclare le pointeur GPU pour la texture macro.
    SDL_GPUTexture* macroGpuTexture = nullptr;

    // Résout le pointeur GPU pour la texture macro.
    if (!resolveGpuTexture(macroWaterTexture, "macro texture", &macroGpuTexture))
    {
        unload();
        return false;
    }

    // Initialise le pointeur depth GPU à null.
    SDL_GPUTexture* depthGpuTexture = nullptr;

    // Tente de résoudre la depth map GPU si disponible.
    if (depthWaterTexture.sdl_texture != nullptr)
    {
        if (!resolveGpuTexture(depthWaterTexture, "depth texture", &depthGpuTexture))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanRenderer: fallback depth vers macro texture");
        }
    }

    // Active le fallback vers macro si depth absente.
    if (depthGpuTexture == nullptr)
    {
        depthGpuTexture = macroGpuTexture;
    }

    // Prépare les bindings t1..t5 du shader water.
    SDL_GPUTextureSamplerBinding samplerBindings[5] = {};

    // Lie la texture détail sur binding 0.
    samplerBindings[0].texture = detailGpuTexture;

    // Lie le sampler repeat pour la texture détail.
    samplerBindings[0].sampler = oceanRepeatSampler;

    // Lie la texture caustiques sur binding 1.
    samplerBindings[1].texture = causticGpuTexture;

    // Lie le sampler repeat pour les caustiques.
    samplerBindings[1].sampler = oceanRepeatSampler;

    // Lie la texture écume sur binding 2.
    samplerBindings[2].texture = foamGpuTexture;

    // Lie le sampler repeat pour l'écume.
    samplerBindings[2].sampler = oceanRepeatSampler;

    // Lie la texture macro sur binding 3.
    samplerBindings[3].texture = macroGpuTexture;

    // Lie le sampler repeat pour la macro.
    samplerBindings[3].sampler = oceanRepeatSampler;

    // Lie la texture depth (ou fallback macro) sur binding 4.
    samplerBindings[4].texture = depthGpuTexture;

    // Lie le sampler repeat pour la depth.
    samplerBindings[4].sampler = oceanRepeatSampler;

    // Prépare la structure de création du render state.
    SDL_GPURenderStateCreateInfo createInfo = {};

    // Renseigne le shader fragment pour ce state.
    createInfo.fragment_shader = oceanFragmentShader;

    // Renseigne le nombre de bindings additionnels.
    createInfo.num_sampler_bindings = 5;

    // Renseigne le tableau de bindings textures/samplers.
    createInfo.sampler_bindings = samplerBindings;

    // Crée le render state GPU de l'océan.
    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);

    // Stoppe le chargement si le render state est invalide.
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanRenderer: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Enregistre le state pour le hot-reload shader.
    if (!rc2d_gpu_trackGraphicsRenderState("water.fragment", &oceanRenderState, 5, samplerBindings))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanRenderer: echec tracking GPURenderState pour hot-reload");
    }

    // Upload les uniforms initiaux.
    uploadUniforms();

    // Signale un chargement réussi.
    return true;
}

void OceanRenderer::update(double dt)
{
    // Ignore l'update si le renderer n'est pas prêt.
    if (!isReady())
    {
        return;
    }

    // Initialise la largeur courante de sortie.
    int outputWidth = 0;

    // Initialise la hauteur courante de sortie.
    int outputHeight = 0;

    // Lit la taille de sortie actuelle du renderer SDL.
    if (rc2d_engine_state.renderer != nullptr)
    {
        SDL_GetCurrentRenderOutputSize(rc2d_engine_state.renderer, &outputWidth, &outputHeight);
    }

    // Incrémente le temps d'animation cumulé.
    oceanTimeSeconds += dt;

    // Met à jour le temps dans les uniforms.
    oceanUniforms.params0[0] = static_cast<float>(oceanTimeSeconds);

    // Met à jour la largeur dans les uniforms.
    oceanUniforms.params1[0] = static_cast<float>(outputWidth);

    // Met à jour la hauteur dans les uniforms.
    oceanUniforms.params1[1] = static_cast<float>(outputHeight);

    // Upload les uniforms mis à jour.
    uploadUniforms();
}

void OceanRenderer::draw(const SDL_FRect& visibleRect)
{
    // Ignore le draw si la texture base est absente.
    if (oceanTexture.sdl_texture == nullptr)
    {
        return;
    }

    // Sauvegarde l'adresse mode U courante.
    SDL_TextureAddressMode prevU = SDL_TEXTURE_ADDRESS_AUTO;

    // Sauvegarde l'adresse mode V courante.
    SDL_TextureAddressMode prevV = SDL_TEXTURE_ADDRESS_AUTO;

    // Tente de lire l'ancien mode d'adressage texture.
    bool restoreAddressMode = SDL_GetRenderTextureAddressMode(rc2d_engine_state.renderer, &prevU, &prevV);

    // Force le wrap pour permettre le tiling.
    SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, SDL_TEXTURE_ADDRESS_WRAP, SDL_TEXTURE_ADDRESS_WRAP);

    // Applique le render state shader si disponible.
    if (oceanRenderState != nullptr)
    {
        // Active le state GPU océan.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);

        // Dessine la texture base sur la zone visible.
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);

        // Désactive le state GPU après le draw.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }
    else
    {
        // Fallback sans shader si le state est absent.
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
    }

    // Restaure l'ancien mode d'adressage si lecture initiale valide.
    if (restoreAddressMode)
    {
        SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, prevU, prevV);
    }
}

bool OceanRenderer::isReady(void) const
{
    // Vérifie la présence de la texture base.
    const bool hasBaseTexture = (oceanTexture.sdl_texture != nullptr);

    // Vérifie la présence du render state.
    const bool hasRenderState = (oceanRenderState != nullptr);

    // Signale l'état prêt global.
    return hasBaseTexture && hasRenderState;
}

float OceanRenderer::getColorMode(void) const
{
    // Retourne la valeur courante de colorMode.
    return oceanUniforms.params2[0];
}
