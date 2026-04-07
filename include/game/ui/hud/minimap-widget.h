#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Widget HUD dedie a la minimap.
 *
 * Responsabilites:
 * - charger/decharger l'image minimap;
 * - configurer son ancrage/marges UI;
 * - dessiner la minimap.
 */
class MinimapWidget {
private:
    RC2D_UIImage minimapUi; /**< Image UI de la minimap. */

public:
    MinimapWidget(void);
    ~MinimapWidget(void);

    /**
     * @brief Charge les ressources et configure le widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Dessine le widget minimap.
     */
    void draw(void);
};
