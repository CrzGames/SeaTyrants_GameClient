#include "game/shaders/ocean-shader.h"

#include <algorithm>
#include <cmath>

#include <RC2D/RC2D_internal.h>

#include "core/context.h"

// Reglage de reference du sillage (calibre pour zoom camera = 1.0).
constexpr float kWakeBaseStrength = 0.75f;
constexpr float kWakeBaseWidthPx = 12.0f;
constexpr float kWakeBaseLengthPx = 35.0f;
// Garde une taille minimale pour eviter de perdre totalement le sillage a faible zoom.
constexpr float kWakeMinWidthPx = 3.0f;
constexpr float kWakeMinLengthPx = 9.0f;

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
      // Initialise le pointeur shader.
      oceanFragmentShader(nullptr),
      // Initialise le render state.
      oceanRenderState(nullptr),
      // Initialise le sampler repeat.
      oceanRepeatSampler(nullptr),
      // Initialise le bloc uniforms.
      oceanUniforms{},
      // Initialise le temps cumule.
      oceanTimeSeconds(0.0),
      // Initialise l'historique des sillages.
      wakeStamps{},
      // Initialise les trackers par navire.
      trackers{},
      // Initialise l'espacement minimal entre stamps.
      wakeStampSpacingPx(8.0f),
      // Initialise la duree de vie des stamps.
      wakeLifetimeSeconds(2.0f)
{
    // Reinitialise les uniforms ocean.
    resetUniforms();
}

void OceanShader::unload(void)
{
    // Detruit le render state si present.
    if (this->oceanRenderState != nullptr)
    {
#if RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
        // Desinscrit le state du systeme hot-reload.
        rc2d_gpu_untrackGraphicsRenderState(&this->oceanRenderState);
#endif // RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
        // Detruit l'objet GPU render state.
        SDL_DestroyGPURenderState(this->oceanRenderState);
        // Annule le pointeur local.
        this->oceanRenderState = nullptr;
    }

    // Libere le sampler repeat si present.
    if (this->oceanRepeatSampler != nullptr)
    {
        // Relache la ressource sampler cote device.
        SDL_ReleaseGPUSampler(rc2d_engine_state.gpu_device, this->oceanRepeatSampler);
        // Annule le pointeur local.
        this->oceanRepeatSampler = nullptr;
    }

    // Libere le shader fragment si present.
    if (this->oceanFragmentShader != nullptr)
    {
        // Relache le shader GPU cote device.
        SDL_ReleaseGPUShader(rc2d_engine_state.gpu_device, static_cast<SDL_GPUShader*>(this->oceanFragmentShader));
        // Annule le pointeur local.
        this->oceanFragmentShader = nullptr;
    }

    // Libere la texture base.
    rc2d_graphics_freeImage(&this->oceanTexture);
    // Libere la texture detail.
    rc2d_graphics_freeImage(&this->oceanTextureDetail);
    // Libere la texture caustiques.
    rc2d_graphics_freeImage(&this->causticTexture);
    // Libere la texture ecume.
    rc2d_graphics_freeImage(&this->foamStreaksTexture);
    // Libere la texture macro.
    rc2d_graphics_freeImage(&this->macroWaterTexture);

    // Reinitialise aussi le systeme de sillage.
    this->resetWakeSystem();
}

void OceanShader::resetUniforms(void)
{
    // Recupere les references runtime pour initialiser sans constantes en dur.
    Map& map = GetCurrentMap();

    // Reinitialise la memoire des uniforms.
    this->oceanUniforms = {};
    // Reinitialise le temps d'animation.
    this->oceanTimeSeconds = 0.0;

    // Initialise le temps shader.
    this->oceanUniforms.params0[0] = 0.0f;
    // Initialise la force des vagues.
    this->oceanUniforms.params0[1] = 0.74f;
    // Initialise l'amplitude pixel.
    this->oceanUniforms.params0[2] = 3.6f;
    // Initialise le tiling.
    this->oceanUniforms.params0[3] = 2.35f;

    // Initialise la resolution du pass gameplay a partir de la zone map.
    this->oceanUniforms.params1[0] = (std::max)(map.rect.w, 1.0f);
    this->oceanUniforms.params1[1] = (std::max)(map.rect.h, 1.0f);
    // Initialise la vitesse d'animation.
    this->oceanUniforms.params1[2] = 0.62f;
    // Initialise l'intensite d'ecume.
    this->oceanUniforms.params1[3] = 0.36f;

    // Initialise colorMode sur 0.0 (sera ajuste a load selon la couleur).
    this->oceanUniforms.params2[0] = 0.0f;
    // Initialise la force fresnel.
    this->oceanUniforms.params2[1] = 0.88f;
    // Initialise la force de glint solaire.
    this->oceanUniforms.params2[2] = 0.74f;
    // Initialise le boost whitecaps.
    this->oceanUniforms.params2[3] = 0.68f;

    // Initialise le nombre de points de sillage.
    this->oceanUniforms.params3[0] = 0.0f;
    // Initialise la force globale du sillage.
    this->oceanUniforms.params3[1] = kWakeBaseStrength;
    // Initialise la largeur du sillage en pixels ecran.
    this->oceanUniforms.params3[2] = kWakeBaseWidthPx;
    // Initialise la longueur du sillage en pixels ecran.
    this->oceanUniforms.params3[3] = kWakeBaseLengthPx;

    // params4: x/y/w/h = zone visible map (monde) en pixels ecran.
    this->oceanUniforms.params4[0] = map.rect.x;
    this->oceanUniforms.params4[1] = map.rect.y;
    this->oceanUniforms.params4[2] = (map.rect.w > 0.0f) ? map.rect.w : this->oceanUniforms.params1[0];
    this->oceanUniforms.params4[3] = (map.rect.h > 0.0f) ? map.rect.h : this->oceanUniforms.params1[1];

    // params5: origine map + taille tuile ecran.
    this->oceanUniforms.params5[0] = map.getOriginX();
    this->oceanUniforms.params5[1] = map.getOriginY();
    this->oceanUniforms.params5[2] = (std::max)(map.getTileWidth(), 1.0f);
    this->oceanUniforms.params5[3] = (std::max)(map.getTileHeight(), 1.0f);
}

bool OceanShader::uploadUniforms(void)
{
    // Verifie que le render state existe.
    if (this->oceanRenderState == nullptr)
    {
        return false;
    }

    // Envoie le bloc uniforms dans le slot fragment 0.
    if (!SDL_SetGPURenderStateFragmentUniforms(this->oceanRenderState, 0, &this->oceanUniforms, sizeof(this->oceanUniforms)))
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

OceanShader::ShipWakeTracker& OceanShader::getOrCreateTracker(uint64_t shipId)
{
    // Un tracker = memoire de la derniere position tuile d'un navire.
    // On le reutilise d'une frame a l'autre pour savoir quand poser un stamp.
    for (ShipWakeTracker& tracker : this->trackers)
    {
        if (tracker.shipId == shipId)
        {
            return tracker;
        }
    }

    ShipWakeTracker tracker = {};
    tracker.shipId = shipId;
    tracker.lastTile = SDL_FPoint{0.0f, 0.0f};
    tracker.initialized = false;
    tracker.seenThisFrame = false;
    this->trackers.push_back(tracker);
    return this->trackers.back();
}

void OceanShader::resetWakeSystem(void)
{
    this->wakeStamps.clear();
    this->trackers.clear();
    this->clearWakePoints();
}

void OceanShader::beginWakeFrame(double dt)
{
    // Debut de frame sillage:
    // 1) vieillir tous les stamps
    // 2) supprimer les stamps trop vieux
    // 3) marquer tous les trackers comme "non vus" pour cette frame
    const float deltaSeconds = static_cast<float>(dt);

    for (WakeStamp& stamp : this->wakeStamps)
    {
        stamp.ageSeconds += deltaSeconds;
    }

    this->wakeStamps.erase(
        std::remove_if(
            this->wakeStamps.begin(),
            this->wakeStamps.end(),
            [this](const WakeStamp& stamp) {
                return stamp.ageSeconds >= this->wakeLifetimeSeconds;
            }),
        this->wakeStamps.end());

    for (ShipWakeTracker& tracker : this->trackers)
    {
        tracker.seenThisFrame = false;
    }
}

void OceanShader::submitWakeSample(
    uint64_t shipId,
    const Map& map,
    const SDL_FPoint& tilePosition,
    bool moving)
{
    // Cette fonction est appelee par chaque navire.
    // Elle decide si un nouveau "stamp" de sillage doit etre cree
    // selon la distance parcourue en espace monde (independant camera).
    ShipWakeTracker& tracker = this->getOrCreateTracker(shipId);
    tracker.seenThisFrame = true;

    if (!tracker.initialized)
    {
        tracker.lastTile = tilePosition;
        tracker.initialized = true;
        return;
    }

    // Delta en tuiles logiques (pas sensible au deplacement camera).
    const float deltaTileX = tilePosition.x - tracker.lastTile.x;
    const float deltaTileY = tilePosition.y - tracker.lastTile.y;

    // Conversion vers un delta isometrique "monde" avec la taille de tuile non zoomee.
    const float cameraZoom = (std::max)(GetCamera().getZoomFactor(), 0.001f);
    const float baseTileWidth = map.getTileWidth() / cameraZoom;
    const float baseTileHeight = map.getTileHeight() / cameraZoom;
    const float halfBaseTileW = baseTileWidth * 0.5f;
    const float halfBaseTileH = baseTileHeight * 0.5f;
    const float deltaWorldX = (deltaTileX - deltaTileY) * halfBaseTileW;
    const float deltaWorldY = (deltaTileX + deltaTileY) * halfBaseTileH;

    const float distSq = (deltaWorldX * deltaWorldX) + (deltaWorldY * deltaWorldY);
    const float spacingSq = this->wakeStampSpacingPx * this->wakeStampSpacingPx;

    if (moving && distSq >= spacingSq)
    {
        // On cree un stamp oriente selon le vecteur de mouvement monde.
        const float length = std::sqrt(distSq);

        WakeStamp stamp = {};
        stamp.tileX = tilePosition.x;
        stamp.tileY = tilePosition.y;
        stamp.dirX = deltaWorldX / length;
        stamp.dirY = deltaWorldY / length;
        stamp.ageSeconds = 0.0f;
        this->wakeStamps.push_back(stamp);

        tracker.lastTile = tilePosition;
    }
    else if (!moving && (deltaTileX * deltaTileX + deltaTileY * deltaTileY) > 0.0001f)
    {
        tracker.lastTile = tilePosition;
    }
}

void OceanShader::endWakeFrame(const Map& map, const SDL_FRect& visibleRect)
{
    // Fin de frame sillage:
    // 1) retirer les trackers non vus
    // 2) projeter les stamps actifs en UV visibles
    // 3) envoyer le tableau compact au shader
    this->trackers.erase(
        std::remove_if(
            this->trackers.begin(),
            this->trackers.end(),
            [](const ShipWakeTracker& tracker) {
                return !tracker.seenThisFrame;
            }),
        this->trackers.end());

    if (visibleRect.w <= 1.0f || visibleRect.h <= 1.0f || this->wakeStamps.empty())
    {
        this->clearWakePoints();
        return;
    }

    std::vector<WakePoint> wakePoints;
    wakePoints.reserve(static_cast<size_t>(MAX_WAKE_POINTS));

    for (auto it = this->wakeStamps.rbegin(); it != this->wakeStamps.rend(); ++it)
    {
        if (wakePoints.size() >= static_cast<size_t>(MAX_WAKE_POINTS))
        {
            break;
        }

        const SDL_FPoint wakeScreen = map.tileToScreenCenterFloat(it->tileX, it->tileY);
        const float uvX = (wakeScreen.x - visibleRect.x) / visibleRect.w;
        const float uvY = (wakeScreen.y - visibleRect.y) / visibleRect.h;
        const float age01 = std::clamp(it->ageSeconds / this->wakeLifetimeSeconds, 0.0f, 1.0f);

        WakePoint point = {};
        point.uvX = uvX;
        point.uvY = uvY;
        point.dirX = it->dirX;
        point.dirY = it->dirY;
        point.intensity = 1.0f - age01;
        point.age01 = age01;
        wakePoints.push_back(point);
    }

    if (wakePoints.empty())
    {
        this->clearWakePoints();
        return;
    }

    this->setWakePoints(wakePoints);
}

void OceanShader::setWakeStampSpacingPx(float spacingPx)
{
    if (spacingPx > 0.0f)
    {
        this->wakeStampSpacingPx = spacingPx;
    }
}

void OceanShader::setWakeLifetimeSeconds(float lifetimeSeconds)
{
    if (lifetimeSeconds > 0.0f)
    {
        this->wakeLifetimeSeconds = lifetimeSeconds;
    }
}

bool OceanShader::load(WaterColor color)
{
    // Pipeline de chargement:
    // - textures
    // - shader + sampler
    // - bindings GPU
    // - render state + uniforms initiaux

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

    // Hot-swap de couleur: si le pipeline ocean est deja pret,
    // on ne detruit pas le render state, on remplace seulement
    // les textures base/detail + les sampler bindings associes.
    if (this->oceanRenderState != nullptr &&
        this->oceanFragmentShader != nullptr &&
        this->oceanRepeatSampler != nullptr &&
        this->causticTexture.sdl_texture != nullptr &&
        this->foamStreaksTexture.sdl_texture != nullptr &&
        this->macroWaterTexture.sdl_texture != nullptr)
    {
        RC2D_Image newBaseTexture = rc2d_graphics_loadImageFromStorage(loadedBaseTexturePath, RC2D_STORAGE_TITLE);
        if (newBaseTexture.sdl_texture == nullptr)
        {
            RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture base %s", loadedBaseTexturePath);
            return false;
        }
        if (!SDL_SetTextureScaleMode(newBaseTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode base(hot-swap): %s", SDL_GetError());
        }

        RC2D_Image newDetailTexture = rc2d_graphics_loadImageFromStorage(loadedDetailTexturePath, RC2D_STORAGE_TITLE);
        if (newDetailTexture.sdl_texture == nullptr)
        {
            RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture detail %s", loadedDetailTexturePath);
            rc2d_graphics_freeImage(&newBaseTexture);
            return false;
        }
        if (!SDL_SetTextureScaleMode(newDetailTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
        {
            RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode detail(hot-swap): %s", SDL_GetError());
        }

        SDL_GPUTexture* detailGpuTexture = nullptr;
        SDL_GPUTexture* causticGpuTexture = nullptr;
        SDL_GPUTexture* foamGpuTexture = nullptr;
        SDL_GPUTexture* macroGpuTexture = nullptr;
        if (!this->resolveGpuTexture(newDetailTexture, "detail texture(hot-swap)", &detailGpuTexture) ||
            !this->resolveGpuTexture(this->causticTexture, "caustic texture(hot-swap)", &causticGpuTexture) ||
            !this->resolveGpuTexture(this->foamStreaksTexture, "foam texture(hot-swap)", &foamGpuTexture) ||
            !this->resolveGpuTexture(this->macroWaterTexture, "macro texture(hot-swap)", &macroGpuTexture))
        {
            rc2d_graphics_freeImage(&newBaseTexture);
            rc2d_graphics_freeImage(&newDetailTexture);
            return false;
        }

        SDL_GPUTextureSamplerBinding samplerBindings[4] = {};
        samplerBindings[0].texture = detailGpuTexture;
        samplerBindings[0].sampler = this->oceanRepeatSampler;
        samplerBindings[1].texture = causticGpuTexture;
        samplerBindings[1].sampler = this->oceanRepeatSampler;
        samplerBindings[2].texture = foamGpuTexture;
        samplerBindings[2].sampler = this->oceanRepeatSampler;
        samplerBindings[3].texture = macroGpuTexture;
        samplerBindings[3].sampler = this->oceanRepeatSampler;

        if (!SDL_SetGPURenderStateSamplerBindings(this->oceanRenderState, 4, samplerBindings))
        {
            RC2D_log(RC2D_LOG_ERROR, "OceanShader: SDL_SetGPURenderStateSamplerBindings failed: %s", SDL_GetError());
            rc2d_graphics_freeImage(&newBaseTexture);
            rc2d_graphics_freeImage(&newDetailTexture);
            return false;
        }

        rc2d_graphics_freeImage(&this->oceanTexture);
        rc2d_graphics_freeImage(&this->oceanTextureDetail);
        this->oceanTexture = newBaseTexture;
        this->oceanTextureDetail = newDetailTexture;
        this->oceanUniforms.params2[0] = (color == WaterColor::BLUE) ? 0.0f : 1.0f;
        this->uploadUniforms();
        return true;
    }

    // Chargement complet (premier load ou pipeline non pret):
    // Libere un eventuel etat precedent.
    unload();
    // Reinitialise les uniforms avant un nouveau chargement.
    this->resetUniforms();
    // Reinitialise le runtime de sillage.
    this->resetWakeSystem();

    // Charge la texture base demandee.
    this->oceanTexture = rc2d_graphics_loadImageFromStorage(loadedBaseTexturePath, RC2D_STORAGE_TITLE);
    // Stoppe le chargement si la texture base reste absente.
    if (this->oceanTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture base %s", loadedBaseTexturePath);
        unload();
        return false;
    }
    // Active le filtrage lineaire sur la texture base.
    if (!SDL_SetTextureScaleMode(this->oceanTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode base: %s", SDL_GetError());
    }

    // Charge la texture detail demandee.
    this->oceanTextureDetail = rc2d_graphics_loadImageFromStorage(loadedDetailTexturePath, RC2D_STORAGE_TITLE);
    // Stoppe le chargement si la texture detail reste absente.
    if (this->oceanTextureDetail.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger la texture detail %s", loadedDetailTexturePath);
        unload();
        return false;
    }
    // Active le filtrage lineaire sur la texture detail.
    if (!SDL_SetTextureScaleMode(this->oceanTextureDetail.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode detail: %s", SDL_GetError());
    }

    // Applique le colorMode selon la regle demandee:
    // BLUE => 0.0, toutes les autres couleurs => 1.0.
    this->oceanUniforms.params2[0] = (color == WaterColor::BLUE) ? 0.0f : 1.0f;

    // Charge la texture caustiques.
    this->causticTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/tile-caustic2.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite des caustiques.
    if (this->causticTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/tile-caustic.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire des caustiques.
    if (!SDL_SetTextureScaleMode(this->causticTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode caustic: %s", SDL_GetError());
    }

    // Charge la texture d'ecume.
    this->foamStreaksTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/tile-foam-streaks.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite de l'ecume.
    if (this->foamStreaksTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/tile-foam-streaks.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire de l'ecume.
    if (!SDL_SetTextureScaleMode(this->foamStreaksTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode foam: %s", SDL_GetError());
    }

    // Charge la texture macro.
    this->macroWaterTexture = rc2d_graphics_loadImageFromStorage("assets/images/shaders/ocean/water-macro.png", RC2D_STORAGE_TITLE);
    // Verifie la disponibilite de la macro texture.
    if (this->macroWaterTexture.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: impossible de charger assets/images/shaders/ocean/water-macro.png");
        unload();
        return false;
    }
    // Active le filtrage lineaire de la macro texture.
    if (!SDL_SetTextureScaleMode(this->macroWaterTexture.sdl_texture, SDL_SCALEMODE_LINEAR))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec SDL_SetTextureScaleMode macro: %s", SDL_GetError());
    }

    // Charge le shader fragment water.
    this->oceanFragmentShader = rc2d_gpu_loadGraphicsShaderFromStorage("water.fragment", RC2D_STORAGE_TITLE);
    // Stoppe le chargement si le shader est absent.
    if (this->oceanFragmentShader == nullptr)
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
    this->oceanRepeatSampler = SDL_CreateGPUSampler(rc2d_engine_state.gpu_device, &samplerCreateInfo);
    // Stoppe le chargement si le sampler n'est pas cree.
    if (this->oceanRepeatSampler == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture detail.
    SDL_GPUTexture* detailGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture detail.
    if (!this->resolveGpuTexture(this->oceanTextureDetail, "detail texture", &detailGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture caustiques.
    SDL_GPUTexture* causticGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture caustiques.
    if (!this->resolveGpuTexture(this->causticTexture, "caustic texture", &causticGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture ecume.
    SDL_GPUTexture* foamGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture ecume.
    if (!this->resolveGpuTexture(this->foamStreaksTexture, "foam texture", &foamGpuTexture))
    {
        unload();
        return false;
    }

    // Declare le pointeur GPU pour la texture macro.
    SDL_GPUTexture* macroGpuTexture = nullptr;
    // Resolve le pointeur GPU pour la texture macro.
    if (!this->resolveGpuTexture(this->macroWaterTexture, "macro texture", &macroGpuTexture))
    {
        unload();
        return false;
    }

    // Prepare les bindings t1..t4 du shader water.
    SDL_GPUTextureSamplerBinding samplerBindings[4] = {};
    // Lie la texture detail sur binding 0.
    samplerBindings[0].texture = detailGpuTexture;
    // Lie le sampler repeat sur binding 0.
    samplerBindings[0].sampler = this->oceanRepeatSampler;
    // Lie la texture caustiques sur binding 1.
    samplerBindings[1].texture = causticGpuTexture;
    // Lie le sampler repeat sur binding 1.
    samplerBindings[1].sampler = this->oceanRepeatSampler;
    // Lie la texture ecume sur binding 2.
    samplerBindings[2].texture = foamGpuTexture;
    // Lie le sampler repeat sur binding 2.
    samplerBindings[2].sampler = this->oceanRepeatSampler;
    // Lie la texture macro sur binding 3.
    samplerBindings[3].texture = macroGpuTexture;
    // Lie le sampler repeat sur binding 3.
    samplerBindings[3].sampler = this->oceanRepeatSampler;

    // Prepare la structure de creation du render state.
    SDL_GPURenderStateCreateInfo createInfo = {};
    // Renseigne le shader fragment.
    createInfo.fragment_shader = this->oceanFragmentShader;
    // Renseigne le nombre de sampler bindings additionnels.
    createInfo.num_sampler_bindings = 4;
    // Renseigne le tableau des sampler bindings.
    createInfo.sampler_bindings = samplerBindings;

    // Cree le render state GPU ocean.
    this->oceanRenderState = SDL_CreateGPURenderState(rc2d_engine_state.renderer, &createInfo);
    // Stoppe le chargement si le render state est invalide.
    if (this->oceanRenderState == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "OceanShader: SDL_CreateGPURenderState failed: %s", SDL_GetError());
        unload();
        return false;
    }

#if RC2D_GPU_SHADER_HOT_RELOAD_ENABLED
    // Enregistre le state pour le hot-reload shader.
    if (!rc2d_gpu_trackGraphicsRenderState("water.fragment", &this->oceanRenderState, 4, samplerBindings))
    {
        RC2D_log(RC2D_LOG_WARN, "OceanShader: echec tracking GPURenderState pour hot-reload");
    }
#endif

    // Upload les uniforms initiaux.
    this->uploadUniforms();

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

    // Incremente le temps d'animation cumule.
    this->oceanTimeSeconds += dt;
    // Met a jour le temps dans les uniforms.
    this->oceanUniforms.params0[0] = static_cast<float>(this->oceanTimeSeconds);

    // Le shader ocean est ancre en espace monde:
    // - position camera en tuiles
    // - conversion ecran -> tuile dans le shader
    // Cela evite un ocean "colle" a l'ecran pendant le pan camera.
    Camera& camera = GetCamera();
    Map& map = GetCurrentMap();
    // Resolution du pass gameplay = taille de la zone map.
    this->oceanUniforms.params1[0] = (std::max)(map.rect.w, 1.0f);
    this->oceanUniforms.params1[1] = (std::max)(map.rect.h, 1.0f);
    const float cameraZoom = (std::max)(camera.getZoomFactor(), 0.001f);
    // Le zoom de l'ocean est gere par l'ancrage monde.
    // On conserve ici un tiling de reference stable.
    this->oceanUniforms.params0[3] = 2.35f;
    this->oceanUniforms.params0[2] = 3.6f * cameraZoom;

    // Le sillage suit aussi le zoom:
    // - a zoom 1.0: rendu identique a la reference
    // - a zoom faible: taille proportionnelle au navire (pas sur-intensifiee)
    this->oceanUniforms.params3[1] = kWakeBaseStrength;
    this->oceanUniforms.params3[2] = (std::max)(kWakeBaseWidthPx * cameraZoom, kWakeMinWidthPx);
    this->oceanUniforms.params3[3] = (std::max)(kWakeBaseLengthPx * cameraZoom, kWakeMinLengthPx);

    // Prepare les donnees de conversion ecran -> monde pour le shader.
    // params4 = rectangle visible de la map (monde) en pixels ecran.
    this->oceanUniforms.params4[0] = map.rect.x;
    this->oceanUniforms.params4[1] = map.rect.y;
    this->oceanUniforms.params4[2] = map.rect.w;
    this->oceanUniforms.params4[3] = map.rect.h;
    // params5 = origine map + taille de tuile actuellement affichee.
    this->oceanUniforms.params5[0] = map.getOriginX();
    this->oceanUniforms.params5[1] = map.getOriginY();
    this->oceanUniforms.params5[2] = map.getTileWidth();
    this->oceanUniforms.params5[3] = map.getTileHeight();

    // Upload les uniforms mis a jour.
    this->uploadUniforms();
}

void OceanShader::draw(const SDL_FRect& visibleRect)
{
    // Draw ocean:
    // - active le mode wrap pour le tiling
    // - dessine avec shader si possible
    // - restaure l'etat d'adressage precedent
    // Ignore le draw si la texture base est absente.
    if (this->oceanTexture.sdl_texture == nullptr)
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
    if (this->oceanRenderState != nullptr)
    {
        // Active le state GPU ocean.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, this->oceanRenderState);
        // Dessine la texture base sur la zone visible.
        SDL_RenderTexture(rc2d_engine_state.renderer, this->oceanTexture.sdl_texture, nullptr, &visibleRect);
        // Desactive le state GPU apres le draw.
        SDL_SetGPURenderState(rc2d_engine_state.renderer, nullptr);
    }
    else
    {
        // Dessine en fallback sans shader si le state est absent.
        SDL_RenderTexture(rc2d_engine_state.renderer, this->oceanTexture.sdl_texture, nullptr, &visibleRect);
    }

    // Restaure l'ancien mode d'adressage si lecture initiale valide.
    if (restoreAddressMode)
    {
        SDL_SetRenderTextureAddressMode(rc2d_engine_state.renderer, prevU, prevV);
    }
}

void OceanShader::drawUiScreenRect(const SDL_FRect& screenRect, float worldPreviewZoom)
{
    if (!this->isReady() || screenRect.w < 1.0f || screenRect.h < 1.0f)
    {
        return;
    }

    const float saveP10 = this->oceanUniforms.params1[0];
    const float saveP11 = this->oceanUniforms.params1[1];
    const float saveP40 = this->oceanUniforms.params4[0];
    const float saveP41 = this->oceanUniforms.params4[1];
    const float saveP42 = this->oceanUniforms.params4[2];
    const float saveP43 = this->oceanUniforms.params4[3];
    const float saveP02 = this->oceanUniforms.params0[2];
    const float saveP03 = this->oceanUniforms.params0[3];
    const float saveP30 = this->oceanUniforms.params3[0];
    const float saveP52 = this->oceanUniforms.params5[2];
    const float saveP53 = this->oceanUniforms.params5[3];

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    const float mapW = (std::max)(map.rect.w, 1.0f);
    const float rel = (std::clamp)(screenRect.w / mapW, 0.22f, 2.5f);
    const float camZ = (std::max)(camera.getZoomFactor(), 0.001f);
    const float wz = (std::clamp)(worldPreviewZoom, 0.05f, 4.0f);

    this->oceanUniforms.params1[0] = (std::max)(screenRect.w, 1.0f);
    this->oceanUniforms.params1[1] = (std::max)(screenRect.h, 1.0f);
    this->oceanUniforms.params4[0] = screenRect.x;
    this->oceanUniforms.params4[1] = screenRect.y;
    this->oceanUniforms.params4[2] = screenRect.w;
    this->oceanUniforms.params4[3] = screenRect.h;
    /** Meme logique que editorMapVfxBuildMarchePopupGrid : tuiles ecran = mapTile/camera * zoom apercu. */
    this->oceanUniforms.params5[2] = map.getTileWidth() / camZ * wz;
    this->oceanUniforms.params5[3] = map.getTileHeight() / camZ * wz;
    this->oceanUniforms.params0[2] = 3.6f * camZ * rel * wz;
    this->oceanUniforms.params0[3] = (std::max)(2.35f / (std::max)(wz, 0.25f), 0.85f);
    this->oceanUniforms.params3[0] = 0.0f;

    this->uploadUniforms();
    this->draw(screenRect);

    this->oceanUniforms.params1[0] = saveP10;
    this->oceanUniforms.params1[1] = saveP11;
    this->oceanUniforms.params4[0] = saveP40;
    this->oceanUniforms.params4[1] = saveP41;
    this->oceanUniforms.params4[2] = saveP42;
    this->oceanUniforms.params4[3] = saveP43;
    this->oceanUniforms.params0[2] = saveP02;
    this->oceanUniforms.params0[3] = saveP03;
    this->oceanUniforms.params3[0] = saveP30;
    this->oceanUniforms.params5[2] = saveP52;
    this->oceanUniforms.params5[3] = saveP53;
    this->uploadUniforms();
}

bool OceanShader::isReady(void) const
{
    // Verifie la presence de la texture base.
    const bool hasBaseTexture = (this->oceanTexture.sdl_texture != nullptr);
    // Verifie la presence du render state.
    const bool hasRenderState = (this->oceanRenderState != nullptr);
    // Signale l'etat pret global.
    return hasBaseTexture && hasRenderState;
}

float OceanShader::getColorMode(void) const
{
    // Retourne la valeur courante de colorMode.
    return this->oceanUniforms.params2[0];
}

void OceanShader::setWakePoints(const std::vector<WakePoint>& points)
{
    // Conversion CPU -> uniforms GPU:
    // - clamp des valeurs
    // - normalisation direction
    // - remplissage dense des tableaux wakePoints/wakeMeta
    int wakeCount = static_cast<int>(points.size());
    if (wakeCount > MAX_WAKE_POINTS)
    {
        wakeCount = MAX_WAKE_POINTS;
    }

    this->oceanUniforms.params3[0] = static_cast<float>(wakeCount);

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

        this->oceanUniforms.wakePoints[i][0] = uvX;
        this->oceanUniforms.wakePoints[i][1] = uvY;
        this->oceanUniforms.wakePoints[i][2] = dirX;
        this->oceanUniforms.wakePoints[i][3] = dirY;

        this->oceanUniforms.wakeMeta[i][0] = intensity;
        this->oceanUniforms.wakeMeta[i][1] = age01;
        this->oceanUniforms.wakeMeta[i][2] = 0.0f;
        this->oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    for (int i = wakeCount; i < MAX_WAKE_POINTS; ++i)
    {
        this->oceanUniforms.wakePoints[i][0] = 0.0f;
        this->oceanUniforms.wakePoints[i][1] = 0.0f;
        this->oceanUniforms.wakePoints[i][2] = 0.0f;
        this->oceanUniforms.wakePoints[i][3] = 0.0f;

        this->oceanUniforms.wakeMeta[i][0] = 0.0f;
        this->oceanUniforms.wakeMeta[i][1] = 1.0f;
        this->oceanUniforms.wakeMeta[i][2] = 0.0f;
        this->oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    // Pousse immediatement les nouveaux points au GPU.
    this->uploadUniforms();
}

void OceanShader::clearWakePoints(void)
{
    // Ecrit un etat "aucun sillage" dans les uniforms GPU.
    this->oceanUniforms.params3[0] = 0.0f;

    for (int i = 0; i < MAX_WAKE_POINTS; ++i)
    {
        this->oceanUniforms.wakePoints[i][0] = 0.0f;
        this->oceanUniforms.wakePoints[i][1] = 0.0f;
        this->oceanUniforms.wakePoints[i][2] = 0.0f;
        this->oceanUniforms.wakePoints[i][3] = 0.0f;

        this->oceanUniforms.wakeMeta[i][0] = 0.0f;
        this->oceanUniforms.wakeMeta[i][1] = 1.0f;
        this->oceanUniforms.wakeMeta[i][2] = 0.0f;
        this->oceanUniforms.wakeMeta[i][3] = 0.0f;
    }

    // Pousse immediatement l'etat vide au GPU.
    this->uploadUniforms();
}

