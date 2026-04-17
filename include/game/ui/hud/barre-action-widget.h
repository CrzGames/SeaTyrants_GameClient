#pragma once

#include <RC2D/RC2D.h>

class BarreActionWidget {
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

private:
    /**
     * @brief Ressource image de la barre d'action HUD.
     */
    RC2D_UIImage actionBarUi; /**< Image UI de la barre d'action. */
};
