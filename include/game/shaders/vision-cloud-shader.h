#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Module dedie au shader de nuages visibles dans la portee du joueur.
 *
 * Ce pass dessine un ombrage nuageux leger au-dessus de l'ocean,
 * uniquement dans le cercle de vision du joueur.
 */
class VisionCloudShader {
private:
    /**
     * @brief Bloc d'uniforms envoye au fragment shader visionclouds.
     *
     * params0 = {time, noiseScale, driftSpeed, cloudCoverage}
     * params1 = {shadowStrength, alphaMax, edgeSoftness, noiseContrast}
     * params2 = {viewRectX, viewRectY, viewRectW, viewRectH}
     * params3 = {mapOriginX, mapOriginY, tileWidthPx, tileHeightPx}
     * params4 = {playerTileX, playerTileY, viewRangeTiles, viewFalloffTiles}
     * params5 = {shadowTintR, shadowTintG, shadowTintB, reserved}
     */
    struct VisionCloudUniforms {
        float params0[4];
        float params1[4];
        float params2[4];
        float params3[4];
        float params4[4];
        float params5[4];
    };

    RC2D_Image cloudNoiseTexture;                  /**< Texture cloud-noise rendue en plein rect gameplay. */
    RC2D_GPUShader* visionCloudFragmentShader;     /**< Shader fragment visionclouds charge. */
    SDL_GPURenderState* visionCloudRenderState;    /**< Render state GPU du pass nuages. */
    VisionCloudUniforms visionCloudUniforms;       /**< Valeurs runtime des uniforms. */
    double visionCloudTimeSeconds;                 /**< Temps cumule pour animer la derive des nuages. */

    /**
     * @brief Reinitialise les uniforms du shader nuages.
     */
    void resetUniforms(void);

    /**
     * @brief Upload les uniforms nuages sur le slot fragment 0.
     * @return True si la mise a jour a reussi.
     */
    bool uploadUniforms(void);

public:
    /**
     * @brief Construit un module shader nuages vide.
     */
    VisionCloudShader(void);

    /**
     * @brief Libere toutes les ressources du module shader nuages.
     */
    void unload(void);

    /**
     * @brief Charge les ressources nuages et cree le render state.
     * @return True si le module est pret a dessiner.
     */
    bool load(void);

    /**
     * @brief Met a jour le temps et les uniforms nuages.
     * @param dt Delta time en secondes.
     * @param playerTile Position tuile du joueur.
     * @param playerViewRangeTiles Portee de vue du joueur en tuiles.
     * @param viewFalloffTiles Douceur du bord de la zone de vision.
     */
    void update(
        double dt,
        const SDL_FPoint& playerTile,
        float playerViewRangeTiles,
        float viewFalloffTiles);

    /**
     * @brief Dessine le pass nuages sur le rectangle de destination.
     * @param visibleRect Rectangle visible cible.
     */
    void draw(const SDL_FRect& visibleRect);

    /**
     * @brief Indique si le module nuages est pret.
     * @return True si les ressources critiques sont valides.
     */
    bool isReady(void) const;
};
