#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/hud-cursor.h"

class CenterShipButtonWidget {
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

    /**
     * @brief Indique si un point ecran survole le bouton.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour ce widget.
     */
    HudCursorType getDesiredCursor(float x, float y) const;
    
private:
    /**
     * @brief Ressource image du bouton de centrage.
     */
    RC2D_UIImage buttonUi; /**< Image UI du bouton de centrage navire. */
};
