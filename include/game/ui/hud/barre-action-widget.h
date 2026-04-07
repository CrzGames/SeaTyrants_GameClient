#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Widget HUD de la barre d'action.
 *
 * Affiche l'image `barre-action2.png` ancree en bas-centre.
 */
class BarreActionWidget {
private:
    RC2D_UIImage actionBarUi; /**< Image UI de la barre d'action. */

public:
    BarreActionWidget(void);
    ~BarreActionWidget(void);

    /**
     * @brief Charge les ressources et configure l'ancrage.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees.
     */
    void unload(void);

    /**
     * @brief Dessine la barre d'action.
     */
    void draw(void);
};
