#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Module dedie au shader ocean.
 *
 * Cette classe encapsule les textures, le shader, les uniforms,
 * et tout le cycle de vie GPU (load/update/draw/unload) de l'ocean.
 */
class OceanShader {
public:
    /**
     * @brief Liste complete des couleurs supportees pour l'ocean.
     */
    enum class WaterColor {
        BLUE,
        AMBER,
        BROWN,
        CORAL,
        CYAN,
        GREEN,
        JADE,
        LAVENDER,
        LIME,
        MAGENTA,
        MINT,
        OBSIDIAN,
        ORANGE,
        PEACH,
        PINK,
        PLUM,
        PURPLE,
        RED,
        ROSE,
        SEAWEED,
        SLATE,
        STORM,
        SUNSET,
        TEAL,
        TURQUOISE,
        VIOLET,
        YELLOW
    };

private:
    /**
     * @brief Bloc d'uniforms envoye au fragment shader water.
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

    RC2D_Image oceanTexture;               /**< Texture base de l'ocean (t0/s0 via SDL_RenderTexture). */
    RC2D_Image oceanTextureDetail;         /**< Texture detail de l'ocean (t1/s1). */
    RC2D_Image causticTexture;             /**< Texture caustiques (t2/s2). */
    RC2D_Image foamStreaksTexture;         /**< Texture trainees d'ecume (t3/s3). */
    RC2D_Image macroWaterTexture;          /**< Texture macro anti-tiling (t4/s4). */
    RC2D_Image depthWaterTexture;          /**< Texture depth map bathymetrie (t5/s5). */
    RC2D_GPUShader* oceanFragmentShader;   /**< Shader fragment water charge. */
    SDL_GPURenderState* oceanRenderState;  /**< Render state GPU pour le pass ocean. */
    SDL_GPUSampler* oceanRepeatSampler;    /**< Sampler repeat partage entre les bindings additionnels. */
    OceanUniforms oceanUniforms;           /**< Valeurs runtime des uniforms oceaniques. */
    double oceanTimeSeconds;               /**< Temps cumule pour animer le shader. */

    /**
     * @brief Convertit une couleur enum en suffixe de fichier.
     * @param color Couleur ocean selectionnee.
     * @return Suffixe minuscule attendu dans les noms de fichiers.
     */
    static const char* colorToSuffix(WaterColor color);

    /**
     * @brief Reinitialise les uniforms de l'ocean.
     */
    void resetUniforms(void);

    /**
     * @brief Upload les uniforms ocean sur le slot fragment 0.
     * @return True si la mise a jour a reussi.
     */
    bool uploadUniforms(void);

    /**
     * @brief Recupere le pointeur GPU texture SDL depuis une texture 2D RC2D.
     * @param image Image RC2D contenant la texture SDL.
     * @param label Libelle log pour diagnostiquer les erreurs.
     * @param outGpuTexture Sortie: texture GPU SDL.
     * @return True si le pointeur GPU a ete trouve.
     */
    bool resolveGpuTexture(const RC2D_Image& image, const char* label, SDL_GPUTexture** outGpuTexture) const;

public:
    /**
     * @brief Construit un module shader ocean vide.
     */
    OceanShader(void);

    /**
     * @brief Libere toutes les ressources du module shader ocean.
     */
    void unload(void);

    /**
     * @brief Charge les ressources ocean et cree le render state.
     * @param color Couleur ocean a charger.
     * @return True si le module est pret a dessiner.
     */
    bool load(WaterColor color);

    /**
     * @brief Met a jour le temps et les uniforms oceaniques.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine l'ocean sur le rectangle de destination.
     * @param visibleRect Rectangle visible cible.
     */
    void draw(const SDL_FRect& visibleRect);

    /**
     * @brief Indique si le module ocean est pret.
     * @return True si les ressources critiques sont valides.
     */
    bool isReady(void) const;

    /**
     * @brief Retourne le mode couleur courant du shader ocean.
     * @return 0.0 pour bleu legacy, 1.0 pour mode neutral.
     */
    float getColorMode(void) const;
};
