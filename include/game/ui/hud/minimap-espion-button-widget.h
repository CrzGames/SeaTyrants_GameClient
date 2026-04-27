#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/minimap-widget.h"

class MinimapEspionButtonWidget {
public:
    MinimapEspionButtonWidget(void);
    ~MinimapEspionButtonWidget(void);

    /**
     * @brief Charge les ressources du bouton espion relatif a la minimap.
     */
    void load(void);

    /**
     * @brief Libere les ressources du bouton.
     */
    void unload(void);

    /**
     * @brief Dessine l'icone espion par-dessus la minimap.
     */
    void draw(const MinimapWidget& minimapWidget) const;

    /**
     * @brief Indique si un point ecran survole l'icone.
     */
    bool containsPoint(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne le curseur souhaite pour cette icone.
     */
    HudCursorType getDesiredCursor(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne true si l'icone espion a ete cliquee.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Calcule le rectangle courant de l'icone par rapport a la minimap.
     */
    SDL_FRect getCurrentRect(const MinimapWidget& minimapWidget) const;

private:
    RC2D_Image iconImage;         /**< Texture GPU de l'icone espion. */
    RC2D_ImageData iconImageData; /**< Surface CPU de l'icone espion. */
};
