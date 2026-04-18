#pragma once

#include <RC2D/RC2D.h>

class TopBarMenuWidget {
public:
    TopBarMenuWidget(void);
    ~TopBarMenuWidget(void);

    /**
     * @brief Charge la barre de menu haute.
     */
    void load(void);

    /**
     * @brief Libere les ressources de la barre de menu haute.
     */
    void unload(void);

    /**
     * @brief Dessine la barre de menu haute en haut a gauche.
     */
    void draw(void);

private:
    RC2D_Image topBarMenuUiImage; /**< Image decorative de la barre de menu haute. */
};
