#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Module dedie au shader de brouillard de guerre.
 *
 * Cette classe encapsule la texture de masque, la texture de noise,
 * le shader fog, les uniforms, et le cycle GPU complet.
 */
class FogOfWarShader {
private:
    /**
     * @brief Bloc d'uniforms envoye au fragment shader fog.
     *
     * params0 = {time, noiseScale, driftSpeed, fogIntensity}
     * params1 = {revealMin, revealMax, edgeBoost, noiseContrast}
     * params2 = {tintR, tintG, tintB, alphaMax}
     * params3 = {viewRectX, viewRectY, viewRectW, viewRectH}
     * params4 = {mapOriginX, mapOriginY, tileWidthPx, tileHeightPx}
     * params5 = {playerTileX, playerTileY, viewRangeTiles, viewFalloffTiles}
     */
    struct FogUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
        float params3[4];
        float params4[4];
        float params5[4];
    };

    RC2D_Image fogMaskTexture;            /**< Texture masque visible (t0/s0 via SDL_RenderTexture). */
    RC2D_Image fogNoiseTexture;           /**< Texture noise additionnelle (t1/s1). */
    RC2D_GPUShader* fogFragmentShader;    /**< Shader fragment fog charge. */
    SDL_GPURenderState* fogRenderState;   /**< Render state GPU du pass fog. */
    SDL_GPUSampler* fogRepeatSampler;     /**< Sampler repeat pour le noise fog. */
    FogUniforms fogUniforms;              /**< Valeurs runtime des uniforms fog. */
    double fogTimeSeconds;                /**< Temps cumule pour animer le fog. */

    /**
     * @brief Reinitialise les uniforms fog a un preset.
     */
    void resetUniforms(void);

    /**
     * @brief Adapte les uniforms fog selon le mode couleur de l'ocean.
     * @param oceanColorMode 0.0 bleu legacy, 1.0 neutral/non-bleu.
     */
    void syncFromOceanColorMode(float oceanColorMode);

    /**
     * @brief Upload les uniforms fog sur le slot fragment 0.
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
     * @brief Construit un module shader fog vide.
     */
    FogOfWarShader(void);

    /**
     * @brief Libere toutes les ressources du module shader fog.
     */
    void unload(void);

    /**
     * @brief Charge les ressources fog et cree le render state.
     * @return True si le module est pret a dessiner.
     */
    bool load(void);

    /**
     * @brief Met a jour le temps et les uniforms fog.
     * @param dt Delta time en secondes.
     * @param oceanColorMode Mode couleur ocean pour ajuster le contraste du fog.
     * @param playerTile Position du joueur en coordonnees tuile.
     * @param playerViewRangeTiles Portee de vue du joueur en tuiles.
     * @param viewFalloffTiles Douceur du bord de reveal en tuiles.
     */
    void update(
        double dt,
        float oceanColorMode,
        const SDL_FPoint& playerTile,
        float playerViewRangeTiles,
        float viewFalloffTiles);

    /**
     * @brief Dessine le fog si @p fogEnabled et si le module est pret (no-op sinon).
     * @param visibleRect Rectangle visible cible.
     * @param fogEnabled Option gameplay (brouillard de guerre).
     */
    void draw(const SDL_FRect& visibleRect, bool fogEnabled);

    /**
     * @brief Indique si le module fog est pret.
     * @return True si les ressources critiques sont valides.
     */
    bool isReady(void) const;
};
