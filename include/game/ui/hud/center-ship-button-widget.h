#pragma once

#include <RC2D/RC2D.h>

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
    
private:
    /**
     * @brief Ressource image du bouton de centrage.
     */
    RC2D_UIImage buttonUi; /**< Image UI du bouton de centrage navire. */
};
