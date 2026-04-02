#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Renderer dédié au shader océan.
 *
 * Cette classe encapsule les textures, le shader, les uniforms,
 * et tout le cycle de vie GPU (load/update/draw/unload) de l'océan.
 */
class OceanRenderer {
private:
    /**
     * @brief Bloc d'uniforms envoyé au fragment shader water.
     *
     * params0 = {time, waveStrength, pixelAmplitude, tiling}
     * params1 = {width, height, speed, foamIntensity}
     * params2 = {colorMode, fresnelStrength, sunGlintStrength, whitecapBoost}
     */
    struct OceanUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
    };

    RC2D_Image oceanTexture;               /**< Texture base de l'océan (t0/s0 via SDL_RenderTexture). */
    RC2D_Image oceanTextureDetail;         /**< Texture détail de l'océan (t1/s1). */
    RC2D_Image causticTexture;             /**< Texture caustiques (t2/s2). */
    RC2D_Image foamStreaksTexture;         /**< Texture traînées d'écume (t3/s3). */
    RC2D_Image macroWaterTexture;          /**< Texture macro anti-tiling (t4/s4). */
    RC2D_Image depthWaterTexture;          /**< Texture depth map bathymétrie (t5/s5). */
    RC2D_GPUShader* oceanFragmentShader;   /**< Shader fragment water chargé. */
    SDL_GPURenderState* oceanRenderState;  /**< Render state GPU pour le pass océan. */
    SDL_GPUSampler* oceanRepeatSampler;    /**< Sampler repeat partagé entre les bindings additionnels. */
    OceanUniforms oceanUniforms;           /**< Valeurs runtime des uniforms océaniques. */
    double oceanTimeSeconds;               /**< Temps cumulé pour animer le shader. */

    /**
     * @brief Réinitialise les uniforms de l'océan.
     */
    void resetUniforms(void);

    /**
     * @brief Upload les uniforms océan sur le slot fragment 0.
     * @return True si la mise à jour a réussi.
     */
    bool uploadUniforms(void);

    /**
     * @brief Récupère le pointeur GPU texture SDL depuis une texture 2D RC2D.
     * @param image Image RC2D contenant la texture SDL.
     * @param label Libellé log pour diagnostiquer les erreurs.
     * @param outGpuTexture Sortie: texture GPU SDL.
     * @return True si le pointeur GPU a été trouvé.
     */
    bool resolveGpuTexture(const RC2D_Image& image, const char* label, SDL_GPUTexture** outGpuTexture) const;

public:
    /**
     * @brief Construit un renderer océan vide.
     */
    OceanRenderer(void);

    /**
     * @brief Libère toutes les ressources du renderer océan.
     */
    void unload(void);

    /**
     * @brief Charge les ressources océan et crée le render state.
     * @param oceanBaseTexturePath Chemin texture base océan.
     * @param oceanDetailTexturePath Chemin texture détail océan.
     * @return True si le renderer est prêt à dessiner.
     */
    bool load(const char* oceanBaseTexturePath, const char* oceanDetailTexturePath);

    /**
     * @brief Met à jour le temps et les uniforms océaniques.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine l'océan sur le rectangle de destination.
     * @param visibleRect Rectangle visible cible.
     */
    void draw(const SDL_FRect& visibleRect);

    /**
     * @brief Indique si le renderer océan est prêt.
     * @return True si les ressources critiques sont valides.
     */
    bool isReady(void) const;

    /**
     * @brief Retourne le mode couleur courant du shader océan.
     * @return 0.0 pour bleu legacy, 1.0 pour mode neutral.
     */
    float getColorMode(void) const;
};
