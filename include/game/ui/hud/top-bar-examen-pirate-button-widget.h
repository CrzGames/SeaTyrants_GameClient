#pragma once

#include "game/ui/hud/top-bar-action-button-widget.h"

/**
 * @brief Bouton top bar reserve a l'examen pirate.
 *
 * Le bouton utilise l'icone `icon-examenpirate.png`.
 * Pour l'instant, il ne declenche encore aucune ouverture de GUI.
 */
class TopBarExamenPirateButtonWidget : public TopBarActionButtonWidget {
public:
    /**
     * @brief Construit le bouton "Examen pirate".
     */
    TopBarExamenPirateButtonWidget(void);

    /**
     * @brief Destructeur trivial.
     */
    ~TopBarExamenPirateButtonWidget(void);
};
