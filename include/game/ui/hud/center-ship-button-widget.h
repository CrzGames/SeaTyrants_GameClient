#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Widget HUD dedie au bouton "centrer sur le navire".
 *
 * Responsabilites:
 * - charger/decharger l'image du bouton;
 * - configurer son ancrage/marges UI;
 * - dessiner le bouton.
 */
class CenterShipButtonWidget {
private:
    RC2D_UIImage buttonUi; /**< Image UI du bouton de centrage navire. */

public:
    CenterShipButtonWidget(void);
    ~CenterShipButtonWidget(void);

    /**
     * @brief Charge les ressources et configure le widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Dessine le bouton.
     */
    void draw(void);
};
