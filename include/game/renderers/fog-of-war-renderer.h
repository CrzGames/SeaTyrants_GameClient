#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Renderer dédié au shader de brouillard de guerre.
 *
 * Cette classe encapsule la texture de masque, la texture de noise,
 * le shader fog, les uniforms, et le cycle GPU complet.
 */
class FogOfWarRenderer {
private:
    /**
     * @brief Bloc d'uniforms envoyé au fragment shader fog.
     *
     * params0 = {time, noiseScale, driftSpeed, fogIntensity}
     * params1 = {revealMin, revealMax, edgeBoost, noiseContrast}
     * params2 = {tintR, tintG, tintB, alphaMax}
     */
    struct FogUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
    };

    RC2D_Image fogMaskTexture;            /**< Texture masque visible (t0/s0 via SDL_RenderTexture). */
    RC2D_Image fogNoiseTexture;           /**< Texture noise additionnelle (t1/s1). */
    RC2D_GPUShader* fogFragmentShader;    /**< Shader fragment fog chargé. */
    SDL_GPURenderState* fogRenderState;   /**< Render state GPU du pass fog. */
    SDL_GPUSampler* fogRepeatSampler;     /**< Sampler repeat pour le noise fog. */
    FogUniforms fogUniforms;              /**< Valeurs runtime des uniforms fog. */
    double fogTimeSeconds;                /**< Temps cumulé pour animer le fog. */

    /**
     * @brief Réinitialise les uniforms fog à un preset.
     */
    void resetUniforms(void);

    /**
     * @brief Adapte les uniforms fog selon le mode couleur de l'océan.
     * @param oceanColorMode 0.0 bleu legacy, 1.0 neutral/non-bleu.
     */
    void syncFromOceanColorMode(float oceanColorMode);

    /**
     * @brief Upload les uniforms fog sur le slot fragment 0.
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
     * @brief Construit un renderer fog vide.
     */
    FogOfWarRenderer(void);

    /**
     * @brief Libère toutes les ressources du renderer fog.
     */
    void unload(void);

    /**
     * @brief Charge les ressources fog et crée le render state.
     * @return True si le renderer est prêt à dessiner.
     */
    bool load(void);

    /**
     * @brief Met à jour le temps et les uniforms fog.
     * @param dt Delta time en secondes.
     * @param oceanColorMode Mode couleur océan pour ajuster le contraste du fog.
     */
    void update(double dt, float oceanColorMode);

    /**
     * @brief Dessine le fog sur le rectangle de destination.
     * @param visibleRect Rectangle visible cible.
     */
    void draw(const SDL_FRect& visibleRect);

    /**
     * @brief Indique si le renderer fog est prêt.
     * @return True si les ressources critiques sont valides.
     */
    bool isReady(void) const;
};
