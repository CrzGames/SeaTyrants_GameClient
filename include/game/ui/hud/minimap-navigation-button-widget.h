#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/minimap-widget.h"

/**
 * @brief Bouton HUD d'ouverture de la navigation rapide, ancre a la minimap.
 *
 * Son comportement est volontairement identique aux trois autres boutons
 * satellites de la minimap: chargement de l'icone, calcul du rectangle courant,
 * hit-test, curseur et validation du clic.
 */
class MinimapNavigationButtonWidget {
public:
    /**
     * @brief Construit le bouton dans un etat vide.
     */
    MinimapNavigationButtonWidget(void);

    /**
     * @brief Destructeur par defaut.
     */
    ~MinimapNavigationButtonWidget(void);

    /**
     * @brief Charge l'icone du bouton de navigation.
     */
    void load(void);

    /**
     * @brief Libere les ressources associees au bouton.
     */
    void unload(void);

    /**
     * @brief Dessine l'icone a droite de la minimap.
     */
    void draw(const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne true si un point ecran touche l'icone.
     */
    bool containsPoint(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne le curseur souhaite pour l'icone.
     */
    HudCursorType getDesiredCursor(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne true si l'icone a ete cliquee avec le bouton gauche.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Calcule le rectangle de rendu courant de l'icone.
     */
    SDL_FRect getCurrentRect(const MinimapWidget& minimapWidget) const;

private:
    RC2D_Image iconImage;         /**< Texture GPU de l'icone de navigation. */
    RC2D_ImageData iconImageData; /**< Surface CPU de l'icone de navigation. */
};
