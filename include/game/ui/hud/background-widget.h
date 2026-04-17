#pragma once

#include <RC2D/RC2D.h>

class BackgroundWidget {
public:
    BackgroundWidget(void);
    ~BackgroundWidget(void);

    /**
     * @brief Charge les ressources du fond UI gameplay.
     */
    void load(void);

    /**
     * @brief Libere les ressources du fond UI gameplay.
     */
    void unload(void);

    /**
     * @brief Dessine le fond UI gameplay (bandes haut/bas).
     */
    void draw(void);

private:
    RC2D_Image backgroundUiImage; /**< Image de fond UI gameplay. */
};
