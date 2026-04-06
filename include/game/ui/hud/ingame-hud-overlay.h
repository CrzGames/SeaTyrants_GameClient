#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief UI gameplay de la scene en jeu.
 *
 * Cette classe encapsule:
 * - le fond UI (haut/bas) dessine en coordonnees ecran absolues;
 * - la minimap;
 * - le bouton "centrer la map".
 */
class IngameHudOverlay {
private:
    RC2D_Image backgroundUiImage;      /**< Fond UI gameplay (haut/bas). */
    RC2D_UIImage minimapUi;            /**< Image UI de minimap. */
    RC2D_UIImage buttonCenterMapUi;    /**< Image UI du bouton centrer. */

public:
    IngameHudOverlay(void);
    ~IngameHudOverlay(void);

    /**
     * @brief Charge les ressources et configure les widgets UI.
     */
    void load(void);

    /**
     * @brief Libere les ressources UI.
     */
    void unload(void);

    /**
     * @brief Dessine le fond UI gameplay (en dessous du monde).
     */
    void drawBackground(void);

    /**
     * @brief Dessine les widgets UI gameplay (au dessus du monde).
     */
    void drawWidgets(void);
};
