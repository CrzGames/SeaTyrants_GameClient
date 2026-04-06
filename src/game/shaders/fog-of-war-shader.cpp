#include "game/shaders/fog-of-war-shader.h"

#include <algorithm>

#include <RC2D/RC2D_internal.h>

#include "core/context.h"

FogOfWarShader::FogOfWarShader(void)
    // Initialise la texture masque.
    : fogMaskTexture{},
      // Initialise la texture noise.
      fogNoiseTexture{},
      // Initialise le pointeur shader.
      fogFragmentShader(nullptr),
      // Initialise le render state.
      fogRenderState(nullptr),
      // Initialise le sampler repeat.
      fogRepeatSampler(nullptr),
      // Initialise le bloc uniforms.
      fogUniforms{},
      // Initialise le temps cumulé.
      fogTimeSeconds(0.0)
{
    // Réinitialise les uniforms fog.
    resetUniforms();
}

void FogOfWarShader::unload(void)
{
    // Nettoyage complet des ressources GPU/CPU du module fog.
    // Détruit le render state si présent.
    if (this->fogRenderState != nullptr)
    {
        // Désinscrit le state du système hot-reload.
        rc2d_gpu_untrackGraphicsRenderState(&this->fogRenderState);

        // Détruit l'objet GPU render state.
        SDL_DestroyGPURenderState(this->fogRenderState);

        // Annule le pointeur local.
        this->fogRenderState = nullptr;
    }

    // Libère le sampler repeat si présent.
    if (this->fogRepeatSampler != nullptr)
    {
        // Relâche la ressource sampler côté device.
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, this->fogRepeatSampler);

        // Annule le pointeur local.
        this->fogRepeatSampler = nullptr;
    }

    // Libère le shader fragment si présent.
    if (this->fogFragmentShader != nullptr)
    {
        // Relâche le shader GPU côté device.
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(this->fogFragmentShader));

        // Annule le pointeur local.
        this->fogFragmentShader = nullptr;
    }

    // Libère la texture masque.
    rc2d_graphics_freeImage(&this->fogMaskTexture);

    // Libère la texture noise.
    rc2d_graphics_freeImage(&this->fogNoiseTexture);
}

void FogOfWarShader::resetUniforms(void)
{
    // Réinitialise la mémoire des uniforms.
    this->fogUniforms = {};

    // Réinitialise le temps d'animation.
    this->fogTimeSeconds = 0.0;

    // Initialise le temps shader.
    this->fogUniforms.params0[0] = 0.0f;

    // Initialise l'échelle du noise.
    this->fogUniforms.params0[1] = 1.45f;

    // Initialise la vitesse de dérive.
    this->fogUniforms.params0[2] = 0.46f;

    // Initialise l'intensité globale du fog.
    this->fogUniforms.params0[3] = 0.56f;

    // Initialise le seuil bas de visibilité.
    this->fogUniforms.params1[0] = 0.34f;

    // Initialise le seuil haut de visibilité.
    this->fogUniforms.params1[1] = 0.66f;

    // Initialise le renfort de bord.
    this->fogUniforms.params1[2] = 0.80f;

    // Initialise le contraste du noise.
    this->fogUniforms.params1[3] = 1.20f;

    // Initialise la teinte rouge.
    this->fogUniforms.params2[0] = 0.18f;

    // Initialise la teinte verte.
    this->fogUniforms.params2[1] = 0.20f;

    // Initialise la teinte bleue.
    this->fogUniforms.params2[2] = 0.22f;

    // Initialise l'alpha maximum.
    this->fogUniforms.params2[3] = 0.86f;

    // Initialise le rect de vue gameplay.
    this->fogUniforms.params3[0] = 0.0f;
    this->fogUniforms.params3[1] = 0.0f;
    this->fogUniforms.params3[2] = 1.0f;
    this->fogUniforms.params3[3] = 1.0f;

    // Initialise l'origine map et la taille de tuile.
    this->fogUniforms.params4[0] = 0.0f;
    this->fogUniforms.params4[1] = 0.0f;
    this->fogUniforms.params4[2] = 1.0f;
    this->fogUniforms.params4[3] = 1.0f;

    // Initialise la zone de reveal autour du joueur.
    this->fogUniforms.params5[0] = 0.0f;
    this->fogUniforms.params5[1] = 0.0f;
    this->fogUniforms.params5[2] = 1.0f;
    this->fogUniforms.params5[3] = 2.0f;
}

void FogOfWarShader::syncFromOceanColorMode(float oceanColorMode)
{
    // Copie locale du mode couleur océan.
    float mode = oceanColorMode;

    // Clamp min de la valeur mode.
    if (mode < 0.0f)
    {
        mode = 0.0f;
    }

    // Clamp max de la valeur mode.
    if (mode > 1.0f)
    {
        mode = 1.0f;
    }

    // Ajuste l'intensité globale selon le mode océan.
    this->fogUniforms.params0[3] = 0.56f + (0.04f * mode);

    // Ajuste le renfort de bord selon le mode océan.
    this->fogUniforms.params1[2] = 0.80f + (0.10f * mode);

    // Ajuste le contraste du noise selon le mode océan.
    this->fogUniforms.params1[3] = 1.20f + (0.10f * mode);

    // Assombrit la composante rouge pour les palettes non bleues.
    this->fogUniforms.params2[0] = 0.18f - (0.07f * mode);

    // Assombrit la composante verte pour les palettes non bleues.
    this->fogUniforms.params2[1] = 0.20f - (0.08f * mode);

    // Assombrit la composante bleue pour les palettes non bleues.
    this->fogUniforms.params2[2] = 0.22f - (0.09f * mode);

    // Augmente légèrement l'alpha max pour renforcer le contraste.
    this->fogUniforms.params2[3] = 0.86f + (0.08f * mode);
}

bool FogOfWarShader::uploadUniforms(void)
{
    // Vérifie que le render state existe.
    if (this->fogRenderState == nullptr)
    {
        return false;
    }

    // Envoie le bloc uniforms dans le slot fragment 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(this->fogRenderState, 0, &this->fogUniforms, sizeof(this->fogUniforms)))
    {
        // Journalise l'erreur SDL.
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: SDL_SetGPURenderStateFragmentUniforms failed: %s", SDL_GetError());

        // Signale un échec d'upload.
        return false;
    }

    // Signale un upload réussi.
    return true;
}

bool FogOfWarShader::resolveGpuTexture(const RC2D_Image& image, const char* label, SDL_GPUTexture** outGpuTexture) const
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
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: texture absente pour %s", label);
        return false;
    }

    // Récupère le conteneur de propriétés SDL.
    SDL_PropertiesID textureProperties = SDL_GetTextureProperties(image.sdl_texture);

    // Vérifie que les propriétés ont été trouvées.
    if (!textureProperties)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: SDL_GetTextureProperties failed for %s: %s", label, SDL_GetError());
        return false;
    }

    // Extrait le pointeur GPU texture depuis les propriétés SDL.
    SDL_GPUTexture* gpuTexture = static_cast<SDL_GPUTexture*>(
        SDL_GetPointerProperty(textureProperties, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));

    // Vérifie que le pointeur GPU est valide.
    if (gpuTexture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: pointeur GPU texture manquant pour %s", label);
        return false;
    }

    // Retourne la texture GPU au code appelant.
    *outGpuTexture = gpuTexture;

    // Signale la réussite.
    return true;
}

bool FogOfWarShader::load(void)
{
    // Pipeline de chargement fog:
    // - textures
    // - shader + sampler
    // - bindings additionnels
    // - render state + upload uniforms
    // Libère un éventuel état précédent.
    unload();

    // Réinitialise les uniforms avant un nouveau chargement.
    this->resetUniforms();

    // Charge la texture masque du fog.
    this->fogMaskTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/fogofwar/cloud-noise.png", RC2D_STORAGE_TITLE);

    // Vérifie la disponibilité du masque.
    if (this->fogMaskTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: masque absent (assets/images/shaders/fogofwar/cloud-noise.png)");
        unload();
        return false;
    }

    // Active le filtrage linéaire du masque.
    if (!SDL_SetTextureScaleMode(this->fogMaskTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: echec SDL_SetTextureScaleMode mask: %s", SDL_GetError());
    }

    // Active le blend alpha du masque.
    if (!SDL_SetTextureBlendMode(this->fogMaskTexture.sdl_texture, SDL_BLENDMODE_BLEND))
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: echec SDL_SetTextureBlendMode mask: %s", SDL_GetError());
    }

    // Charge la texture noise secondaire.
    this->fogNoiseTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/fogofwar/cloud-noise.png", RC2D_STORAGE_TITLE);

    // Vérifie la disponibilité du noise.
    if (this->fogNoiseTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: noise absent (assets/images/shaders/fogofwar/cloud-noise.png)");
        unload();
        return false;
    }

    // Active le filtrage linéaire du noise.
    if (!SDL_SetTextureScaleMode(this->fogNoiseTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: echec SDL_SetTextureScaleMode noise: %s", SDL_GetError());
    }

    // Charge le shader fog.
    this->fogFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("fogofwar.fragment", RC2D_STORAGE_TITLE);

    // Stoppe le chargement si le shader est absent.
    if (this->fogFragmentShader == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: impossible de charger fogofwar.fragment");
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

    // Crée le sampler GPU repeat du fog.
    this->fogRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);

    // Stoppe le chargement si le sampler n'est pas créé.
    if (this->fogRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Déclare le pointeur GPU pour le noise.
    SDL_GPUTexture* fogNoiseGpuTexture = nullptr;

    // Résout le pointeur GPU du noise.
    if (!this->resolveGpuTexture(this->fogNoiseTexture, "fog noise texture", &fogNoiseGpuTexture))
    {
        unload();
        return false;
    }

    // Prépare le binding additionnel t1/s1.
    SDL_GPUTextureSamplerBinding fogSamplerBindings[1] = {};

    // Lie la texture noise sur le binding 0 additionnel.
    fogSamplerBindings[0].texture = fogNoiseGpuTexture;

    // Lie le sampler repeat au noise.
    fogSamplerBindings[0].sampler = this->fogRepeatSampler;

    // Prépare la structure de création du render state.
    SDL_GPURenderStateCreateInfo fogCreateInfo = {};

    // Renseigne le shader fragment fog.
    fogCreateInfo.fragment_shader = this->fogFragmentShader;

    // Renseigne le nombre de bindings additionnels.
    fogCreateInfo.num_sampler_bindings = 1;

    // Renseigne le tableau de bindings textures/samplers.
    fogCreateInfo.sampler_bindings = fogSamplerBindings;

    // Crée le render state GPU du fog.
    this->fogRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &fogCreateInfo);

    // Stoppe le chargement si le render state est invalide.
    if (this->fogRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Enregistre le state pour le hot-reload shader.
    if (!rc2d_gpu_trackGraphicsRenderState("fogofwar.fragment", &this->fogRenderState, 1, fogSamplerBindings))
    {
        RC2D_log(RC2D_LOG_WARN, "FogOfWarShader: echec tracking GPURenderState pour hot-reload");
    }

    // Upload les uniforms initiaux.
    this->uploadUniforms();

    // Signale un chargement réussi.
    return true;
}

void FogOfWarShader::update(
    double dt,
    float oceanColorMode,
    const SDL_FPoint& playerTile,
    float playerViewRangeTiles,
    float viewFalloffTiles)
{
    // Update fog:
    // - avance le temps
    // - adapte les params au mode ocean
    // - push uniforms vers le GPU
    // Ignore l'update si le renderer n'est pas prêt.
    if (!isReady())
    {
        return;
    }

    // Incrémente le temps d'animation cumulé.
    this->fogTimeSeconds += dt;

    // Met à jour le temps dans les uniforms.
    this->fogUniforms.params0[0] = static_cast<float>(this->fogTimeSeconds);

    // Synchronise le style fog avec la palette océan.
    this->syncFromOceanColorMode(oceanColorMode);

    // Meme base que l'ocean pour que le fog ne glisse pas en camera pan/zoom.
    Map& map = GetCurrentMap();
    GameScreen& gameScreen = GetGameScreen();

    int outputWidth = 1;
    int outputHeight = 1;
    if (rc2d_engine_state.renderer != nullptr)
    {
        SDL_GetCurrentRenderOutputSize(rc2d_engine_state.renderer, &outputWidth, &outputHeight);
    }

    this->fogUniforms.params3[0] = gameScreen.rect.x;
    this->fogUniforms.params3[1] = gameScreen.rect.y;
    this->fogUniforms.params3[2] =
        (gameScreen.rect.w > 0.0f) ? gameScreen.rect.w : static_cast<float>((std::max)(outputWidth, 1));
    this->fogUniforms.params3[3] =
        (gameScreen.rect.h > 0.0f) ? gameScreen.rect.h : static_cast<float>((std::max)(outputHeight, 1));

    this->fogUniforms.params4[0] = map.getOriginX();
    this->fogUniforms.params4[1] = map.getOriginY();
    this->fogUniforms.params4[2] = (std::max)(map.getTileWidth(), 1.0f);
    this->fogUniforms.params4[3] = (std::max)(map.getTileHeight(), 1.0f);

    this->fogUniforms.params5[0] = playerTile.x;
    this->fogUniforms.params5[1] = playerTile.y;
    this->fogUniforms.params5[2] = (std::max)(playerViewRangeTiles, 0.0f);
    this->fogUniforms.params5[3] = (std::max)(viewFalloffTiles, 0.001f);

    // Upload les uniforms mis à jour.
    this->uploadUniforms();
}

void FogOfWarShader::draw(const SDL_FRect& visibleRect)
{
    // Draw fog:
    // - active le state GPU
    // - dessine la texture masque sur le rect visible
    // - desactive le state
    // Ignore le draw si le renderer n'est pas prêt.
    if (!isReady())
    {
        return;
    }

    // Ignore le draw si la texture masque est absente.
    if (this->fogMaskTexture.sdl_texture == nullptr)
    {
        return;
    }

    // Active le state GPU fog.
    SDL_SetGPURenderState(rc2d_engine_state.renderer, this->fogRenderState);

    // Dessine la texture masque sur la zone visible.
    SDL_RenderTexture(rc2d_engine_state.renderer, this->fogMaskTexture.sdl_texture, nullptr, &visibleRect);

    // Désactive le state GPU après le draw.
    SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
}

bool FogOfWarShader::isReady(void) const
{
    // Vérifie la présence de la texture masque.
    const bool hasMaskTexture = (this->fogMaskTexture.sdl_texture != nullptr);

    // Vérifie la présence du render state.
    const bool hasRenderState = (this->fogRenderState != nullptr);

    // Signale l'état prêt global.
    return hasMaskTexture && hasRenderState;
}
