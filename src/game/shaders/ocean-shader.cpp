#include "game/shaders/ocean-shader.h"

#include <algorithm>
#include <cmath>

#include <RC2D/RC2D_internal.h>

const char* OceanShader::colorToSuffix(WaterColor color)
{
    // Retourne le suffixe correspondant a la couleur BLUE.
    if (color == WaterColor::BLUE) { return "blue"; }
    // Retourne le suffixe correspondant a la couleur AMBER.
    if (color == WaterColor::AMBER) { return "amber"; }
    // Retourne le suffixe correspondant a la couleur BROWN.
    if (color == WaterColor::BROWN) { return "brown"; }
    // Retourne le suffixe correspondant a la couleur CORAL.
    if (color == WaterColor::CORAL) { return "coral"; }
    // Retourne le suffixe correspondant a la couleur CYAN.
    if (color == WaterColor::CYAN) { return "cyan"; }
    // Retourne le suffixe correspondant a la couleur GREEN.
    if (color == WaterColor::GREEN) { return "green"; }
    // Retourne le suffixe correspondant a la couleur JADE.
    if (color == WaterColor::JADE) { return "jade"; }
    // Retourne le suffixe correspondant a la couleur LAVENDER.
    if (color == WaterColor::LAVENDER) { return "lavender"; }
    // Retourne le suffixe correspondant a la couleur LIME.
    if (color == WaterColor::LIME) { return "lime"; }
    // Retourne le suffixe correspondant a la couleur MAGENTA.
    if (color == WaterColor::MAGENTA) { return "magenta"; }
    // Retourne le suffixe correspondant a la couleur MINT.
    if (color == WaterColor::MINT) { return "mint"; }
    // Retourne le suffixe correspondant a la couleur OBSIDIAN.
    if (color == WaterColor::OBSIDIAN) { return "obsidian"; }
    // Retourne le suffixe correspondant a la couleur ORANGE.
    if (color == WaterColor::ORANGE) { return "orange"; }
    // Retourne le suffixe correspondant a la couleur PEACH.
    if (color == WaterColor::PEACH) { return "peach"; }
    // Retourne le suffixe correspondant a la couleur PINK.
    if (color == WaterColor::PINK) { return "pink"; }
    // Retourne le suffixe correspondant a la couleur PLUM.
    if (color == WaterColor::PLUM) { return "plum"; }
    // Retourne le suffixe correspondant a la couleur PURPLE.
    if (color == WaterColor::PURPLE) { return "purple"; }
    // Retourne le suffixe correspondant a la couleur RED.
    if (color == WaterColor::RED) { return "red"; }
    // Retourne le suffixe correspondant a la couleur ROSE.
    if (color == WaterColor::ROSE) { return "rose"; }
    // Retourne le suffixe correspondant a la couleur SEAWEED.
    if (color == WaterColor::SEAWEED) { return "seaweed"; }
    // Retourne le suffixe correspondant a la couleur SLATE.
    if (color == WaterColor::SLATE) { return "slate"; }
    // Retourne le suffixe correspondant a la couleur STORM.
    if (color == WaterColor::STORM) { return "storm"; }
    // Retourne le suffixe correspondant a la couleur SUNSET.
    if (color == WaterColor::SUNSET) { return "sunset"; }
    // Retourne le suffixe correspondant a la couleur TEAL.
    if (color == WaterColor::TEAL) { return "teal"; }
    // Retourne le suffixe correspondant a la couleur TURQUOISE.
    if (color == WaterColor::TURQUOISE) { return "turquoise"; }
    // Retourne le suffixe correspondant a la couleur VIOLET.
    if (color == WaterColor::VIOLET) { return "violet"; }
    // Retourne le suffixe correspondant a la couleur YELLOW.
    if (color == WaterColor::YELLOW) { return "yellow"; }
    // Retourne une valeur par defaut si aucune branche n'a match.
    return "blue";
}

OceanShader::OceanShader(void)
    // Initialise la texture base.
    : oceanTexture{},
      // Initialise la texture detail.
      oceanTextureDetail{},
      // Initialise la texture caustiques.
      causticTexture{},
      // Initialise la texture trainees d'ecume.
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
      // Initialise le temps cumule.
      oceanTimeSeconds(0.0)
{
    // Reinitialise les uniforms ocean.
    resetUniforms();
}

void OceanShader::unload(void)
{
    // Detruit le render state si present.
    if (oceanRenderState != nullptr)
    {
        // Desinscrit le state du systeme hot-reload.
        rc2d_gpu_untrackGraphicsRenderState(&oceanRenderState);
        // Detruit l'objet GPU render state.
        SDL_DestroyGPURenderState(oceanRenderState);
        // Annule le pointeur local.
        oceanRenderState = nullptr;
    }

    // Libere le sampler repeat si present.
    if (oceanRepeatSampler != nullptr)
    {
        // Relache la ressource sampler cote device.
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, oceanRepeatSampler);
        // Annule le pointeur local.
        oceanRepeatSampler = nullptr;
    }

    // Libere le shader fragment si present.
    if (oceanFragmentShader != nullptr)
    {
        // Relache le shader GPU cote device.
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(oceanFragmentShader));
        // Annule le pointeur local.
        oceanFragmentShader = nullptr;
    }

    // Libere la texture base.
    rc2d_graphics_freeImage(&oceanTexture);
    // Libere la texture detail.
    rc2d_graphics_freeImage(&oceanTextureDetail);
    // Libere la texture caustiques.
    rc2d_graphics_freeImage(&causticTexture);
    // Libere la texture ecume.
    rc2d_graphics_freeImage(&foamStreaksTexture);
    // Libere la texture macro.
    rc2d_graphics_freeImage(&macroWaterTexture);
    // Libere la texture depth.
    rc2d_graphics_freeImage(&depthWaterTexture);
}

void OceanShader::resetUniforms(void)
{
    // Reinitialise la memoire des uniforms.
    oceanUniforms = {};
    // Reinitialise le temps d'animation.
    oceanTimeSeconds = 0.0;

    // Initialise le temps shader.
    oceanUniforms.params0[0] = 0.0f;
    // Initialise la force des vagues.
    oceanUniforms.params0[1] = 0.74f;
    // Initialise l'amplitude pixel.
    oceanUniforms.params0[2] = 3.6f;
    // Initialise le tiling.
    oceanUniforms.params0[3] = 2.35f;

    // Initialise la largeur de rendu par defaut.
    oceanUniforms.params1[0] = 1920.0f;
    // Initialise la hauteur de rendu par defaut.
    oceanUniforms.params1[1] = 1080.0f;
    // Initialise la vitesse d'animation.
    oceanUniforms.params1[2] = 0.62f;
    // Initialise l'intensite d'ecume.
    oceanUniforms.params1[3] = 0.36f;

    // Initialise colorMode sur 0.0 (sera ajuste a load selon la couleur).
    oceanUniforms.params2[0] = 0.0f;
    // Initialise la force fresnel.
    oceanUniforms.params2[1] = 0.88f;
    // Initialise la force de glint solaire.
    oceanUniforms.params2[2] = 0.74f;
    // Initialise le boost whitecaps.
    oceanUniforms.params2[3] = 0.68f;

    // Initialise le nombre de points de sillage.
    oceanUniforms.params3[0] = 0.0f;
    // Initialise la force globale du sillage.
    oceanUniforms.params3[1] = 0.75f;
    // Initialise la largeur du sillage en pixels ecran.
    oceanUniforms.params3[2] = 12.0f;
    // Initialise la longueur du sillage en pixels ecran.
    oceanUniforms.params3[3] = 35.0f;
}

bool OceanShader::uploadUniforms(void)
{
    // Verifie que le render state existe.
    if (oceanRenderState == nullptr)
    {
        return false;
    }

    // Envoie le bloc uniforms dans le slot fragment 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(oceanRenderState, 0, &oceanUniforms, sizeof(oceanUniforms)))
    {
        // Journalise l'erreur SDL.
        RC2D_log(RC2D_LOG_WARN, "OceanShader: SDL_SetGPURenderStateFragmentUniforms failed: %s", SDL_GetError());
        // Signale un echec d'upload.
        return false;
    }

    // Signale un upload reussi.
    return true;
}

bool OceanShader::resolveGpuTexture(const RC2D_Image& image, const char* label, SDL_GPUTexture** outGpuTexture) const
{
    // Verifie le pointeur de sortie.
    if (outGpuTexture == nullptr)
    {
        return false;
    }

    // Initialise la sortie a null par securite.
    *outGpuTexture = nullptr;

    // Verifie que la texture SDL existe.
    if (image.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: texture absente pour %s", label);
        return false;
    }

    // Recupere le conteneur de proprietes SDL.
    SDL_PropertiesID textureProperties = SDL_GetTextureProperties(image.sdl_texture);
    // Verifie que les proprietes ont ete trouvees.
    if (!textureProperties)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: SDL_GetTextureProperties failed for %s: %s", label, SDL_GetError());
        return false;
    }

    // Extrait le pointeur GPU texture depuis les proprietes SDL.
    SDL_GPUTexture* gpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(textureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    // Verifie que le pointeur GPU est valide.
    if (gpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: pointeur GPU texture manquant pour %s", label);
        return false;
    }

    // Retourne la texture GPU au code appelant.
    *outGpuTexture = gpuTexture;
    // Signale la reussite.
    return true;
}

bool OceanShader::load(WaterColor color)
{
    // Libere un eventuel etat precedent.
    unload();
    // Reinitialise les uniforms avant un nouveau chargement.
    resetUniforms();

    // Construit le chemin base avec le format impose tile-water-base-color.png.
    char requestedBaseTexturePath[256] = {};
    SDL_snprintf(requestedBaseTexturePath, sizeof(requestedBaseTexturePath), "assets/images/shaders/ocean/tile-water-base-%s.png", colorToSuffix(color));
    // Construit le chemin detail avec le format impose tile-water-detail-color.png.
    char requestedDetailTexturePath[256] = {};
    SDL_snprintf(requestedDetailTexturePath, sizeof(requestedDetailTexturePath), "assets/images/shaders/ocean/tile-water-detail-%s.png", colorToSuffix(color));

    // Conserve le chemin base reellement utilise.
    const char* loadedBaseTexturePath = requestedBaseTexturePath;
    // Conserve le chemin detail reellement utilise.
    const char* loadedDetailTexturePath = requestedDetailTexturePath;

    // Charge la texture base demandee.
    oceanTexture = rc2d_graphics_loadImageFromStorage(loadedBaseTexturePath, RC2D_STORAGE_TITLE);
    // Stoppe le chargement si la texture base reste absente.
    if (oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture base %s", loadedBaseTexturePath);
        unload();
        return false;
    }
    // Active le filtrage lineaire sur la texture base.
    if (!SDL_SetTextureScaleMode(oceanTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode base: %s", SDL_GetError());
    }

    // Charge la texture detail demandee.
    oceanTextureDetail = rc2d_graphics_loadImageFromStorage(loadedDetailTexturePath, RC2D_STORAGE_TITLE);
    // Stoppe le chargement si la texture detail reste absente.
    if (oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture detail %s", loadedDetailTexturePath);
        unload();
        return false;
    }
    // Active le filtrage lineaire sur la texture detail.
    if (!SDL_SetTextureScaleMode(oceanTextureDetail.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode detail: %s", SDL_GetError());
    }

    // Applique le colorMode selon la regle demandee:
    // BLUE => 0.0, toutes les autres couleurs => 1.0.
    oceanUniforms.params2[0] = (color == WaterColor::BLUE) ? 0.0f : 1.0f;

    // Charge la texture caustiques.
    causticTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/tile-caustic.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite des caustiques.
    if (causticTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/tile-caustic.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire des caustiques.
    if (!SDL_SetTextureScaleMode(causticTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode caustic: %s", SDL_GetError());
    }

    // Charge la texture d'ecume.
    foamStreaksTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/tile-foam-streaks.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite de l'ecume.
    if (foamStreaksTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/tile-foam-streaks.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire de l'ecume.
    if (!SDL_SetTextureScaleMode(foamStreaksTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode foam: %s", SDL_GetError());
    }

    // Charge la texture macro.
    macroWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/water-macro.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite de la macro texture.
    if (macroWaterTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/water-macro.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire de la macro texture.
    if (!SDL_SetTextureScaleMode(macroWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode macro: %s", SDL_GetError());
    }

    // Charge la texture depth dediee.
    depthWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/tile-water-depth.png", RC2D_STORAGE_TITLE);
    // Active le filtrage lineaire de la depth map si presente.
    if (depthWaterTexture.sdl_texture != nullptr)
    {
        if (!SDL_SetTextureScaleMode(depthWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode depth: %s", SDL_GetError());
        }
    }
    else
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: tile-water-depth manquant, fallback macro actif");
    }

    // Charge le shader fragment water.
    oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);
    // Stoppe le chargement si le shader est absent.
    if (oceanFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger water.fragment");
        unload();
        return false;
    }

    // Prepare la structure de creation du sampler.
    SDL_GPUSamplerCreateInfo samplerCreateInfo = {};
    // Configure le filtre min en lineaire.
    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    // Configure le filtre mag en lineaire.
    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    // Configure le mode mipmap en lineaire.
    samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    // Active le wrap sur l'axe U.
    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    // Active le wrap sur l'axe V.
    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    // Active le wrap sur l'axe W.
    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Cree le sampler GPU repeat.
    oceanRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);
    // Stoppe le chargement si le sampler n'est pas cree.
    if (oceanRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture detail.
    SDL_GPUTexture* detailGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture detail.
    if (!resolveGpuTexture(oceanTextureDetail, "detail texture", &detailGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture caustiques.
    SDL_GPUTexture* causticGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture caustiques.
    if (!resolveGpuTexture(causticTexture, "caustic texture", &causticGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture ecume.
    SDL_GPUTexture* foamGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture ecume.
    if (!resolveGpuTexture(foamStreaksTexture, "foam texture", &foamGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture macro.
    SDL_GPUTexture* macroGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture macro.
    if (!resolveGpuTexture(macroWaterTexture, "macro texture", &macroGpuTexture))
    {
        unload();
        return false;
    }

    // Initialise le pointeur depth GPU a null.
    SDL_GPUTexture* depthGpuTexture = nullptr;
    // Tente de resoudre la depth map GPU si disponible.
    if (depthWaterTexture.sdl_texture != nullptr)
    {
        if (!resolveGpuTexture(depthWaterTexture, "depth texture", &depthGpuTexture))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanShader: fallback depth vers macro texture");
        }
    }
    // Active le fallback vers macro si depth absente.
    if (depthGpuTexture == nullptr)
    {
        depthGpuTexture = macroGpuTexture;
    }

    // Prepare les bindings t1..t5 du shader water.
    SDL_GPUTextureSamplerBinding samplerBindings[5] = {};
    // Lie la texture detail sur binding 0.
    samplerBindings[0].texture = detailGpuTexture;
    // Lie le sampler repeat sur binding 0.
    samplerBindings[0].sampler = oceanRepeatSampler;
    // Lie la texture caustiques sur binding 1.
    samplerBindings[1].texture = causticGpuTexture;
    // Lie le sampler repeat sur binding 1.
    samplerBindings[1].sampler = oceanRepeatSampler;
    // Lie la texture ecume sur binding 2.
    samplerBindings[2].texture = foamGpuTexture;
    // Lie le sampler repeat sur binding 2.
    samplerBindings[2].sampler = oceanRepeatSampler;
    // Lie la texture macro sur binding 3.
    samplerBindings[3].texture = macroGpuTexture;
    // Lie le sampler repeat sur binding 3.
    samplerBindings[3].sampler = oceanRepeatSampler;
    // Lie la texture depth (ou fallback macro) sur binding 4.
    samplerBindings[4].texture = depthGpuTexture;
    // Lie le sampler repeat sur binding 4.
    samplerBindings[4].sampler = oceanRepeatSampler;

    // Prepare la structure de creation du render state.
    SDL_GPURenderStateCreateInfo createInfo = {};
    // Renseigne le shader fragment.
    createInfo.fragment_shader = oceanFragmentShader;
    // Renseigne le nombre de sampler bindings additionnels.
    createInfo.num_sampler_bindings = 5;
    // Renseigne le tableau des sampler bindings.
    createInfo.sampler_bindings = samplerBindings;

    // Cree le render state GPU ocean.
    oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);
    // Stoppe le chargement si le render state est invalide.
    if (oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Enregistre le state pour le hot-reload shader.
    if (!rc2d_gpu_trackGraphicsRenderState("water.fragment", &oceanRenderState, 5, samplerBindings))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec tracking GPURenderState pour hot-reload");
    }

    // Upload les uniforms initiaux.
    uploadUniforms();

    // Signale un chargement reussi.
    return true;
}

void OceanShader::update(double dt)
{
    // Ignore l'update si le module n'est pas pret.
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

    // Incremente le temps d'animation cumule.
    oceanTimeSeconds += dt;
    // Met a jour le temps dans les uniforms.
    oceanUniforms.params0[0] = static_cast<float>(oceanTimeSeconds);
    // Met a jour la largeur dans les uniforms.
    oceanUniforms.params1[0] = static_cast<float>(outputWidth);
    // Met a jour la hauteur dans les uniforms.
    oceanUniforms.params1[1] = static_cast<float>(outputHeight);

    // Upload les uniforms mis a jour.
    uploadUniforms();
}

void OceanShader::draw(const SDL_FRect& visibleRect)
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
        // Active le state GPU ocean.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, oceanRenderState);
        // Dessine la texture base sur la zone visible.
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
        // Desactive le state GPU apres le draw.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }
    else
    {
        // Dessine en fallback sans shader si le state est absent.
        SDL_RenderTexture(rc2d_engine_state.renderer, oceanTexture.sdl_texture, nullptr, &visibleRect);
    }

    // Restaure l'ancien mode d'adressage si lecture initiale valide.
    if (restoreAddressMode)
    {
        SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, prevU, prevV);
    }
}

bool OceanShader::isReady(void) const
{
    // Verifie la presence de la texture base.
    const bool hasBaseTexture = (oceanTexture.sdl_texture != nullptr);
    // Verifie la presence du render state.
    const bool hasRenderState = (oceanRenderState != nullptr);
    // Signale l'etat pret global.
    return hasBaseTexture && hasRenderState;
}

float OceanShader::getColorMode(void) const
{
    // Retourne la valeur courante de colorMode.
    return oceanUniforms.params2[0];
}

void OceanShader::setWakePoints(const std::vector<WakePoint>& points)
{
    int wakeCount = static_cast<int>(points.size());
    if (wakeCount > MAX_WAKE_POINTS)
    {
        wakeCount = MAX_WAKE_POINTS;
    }

    oceanUniforms.params3[0] = static_cast<float>(wakeCount);

    for (int i = 0; i < wakeCount; ++i)
    {
        const WakePoint& p = points[static_cast<size_t>(i)];

        const float uvX = std::clamp(p.uvX, -0.25f, 1.25f);
        const float uvY = std::clamp(p.uvY, -0.25f, 1.25f);
        const float intensity = std::clamp(p.intensity, 0.0f, 3.0f);
        const float age01 = std::clamp(p.age01, 0.0f, 1.0f);

        float dirX = p.dirX;
        float dirY = p.dirY;
        const float dirLen = std::sqrt((dirX * dirX) + (dirY * dirY));
        if (dirLen > 0.0001f)
        {
            dirX /= dirLen;
            dirY /= dirLen;
        }
        else
        {
            dirX = 1.0f;
            dirY = 0.0f;
        }

        oceanUniforms.wakePoints[i][0] = uvX;
        oceanUniforms.wakePoints[i][1] = uvY;
        oceanUniforms.wakePoints[i][2] = dirX;
        oceanUniforms.wakePoints[i][3] = dirY;

        oceanUniforms.wakeMeta[i][0] = intensity;
        oceanUniforms.wakeMeta[i][1] = age01;
        oceanUniforms.wakeMeta[i][2] = 0.0f;
        oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    for (int i = wakeCount; i < MAX_WAKE_POINTS; ++i)
    {
        oceanUniforms.wakePoints[i][0] = 0.0f;
        oceanUniforms.wakePoints[i][1] = 0.0f;
        oceanUniforms.wakePoints[i][2] = 0.0f;
        oceanUniforms.wakePoints[i][3] = 0.0f;

        oceanUniforms.wakeMeta[i][0] = 0.0f;
        oceanUniforms.wakeMeta[i][1] = 1.0f;
        oceanUniforms.wakeMeta[i][2] = 0.0f;
        oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    // Pousse immediatement les nouveaux points au GPU.
    uploadUniforms();
}

void OceanShader::clearWakePoints(void)
{
    oceanUniforms.params3[0] = 0.0f;

    for (int i = 0; i < MAX_WAKE_POINTS; ++i)
    {
        oceanUniforms.wakePoints[i][0] = 0.0f;
        oceanUniforms.wakePoints[i][1] = 0.0f;
        oceanUniforms.wakePoints[i][2] = 0.0f;
        oceanUniforms.wakePoints[i][3] = 0.0f;

        oceanUniforms.wakeMeta[i][0] = 0.0f;
        oceanUniforms.wakeMeta[i][1] = 1.0f;
        oceanUniforms.wakeMeta[i][2] = 0.0f;
        oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    // Pousse immediatement l'etat vide au GPU.
    uploadUniforms();
}
