#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/minimap-widget.h"

/**
 * @brief Bouton HUD d'ouverture de la carte du monde, ancre a la minimap.
 *
 * Ce bouton reutilise la meme logique que les autres boutons satellites de la
 * minimap: il charge une icone, calcule son rectangle relatif a la minimap et
 * expose des helpers de hit-test / curseur / clic.
 */
class MinimapWorldMapButtonWidget {
public:
    /**
     * @brief Construit le bouton dans un etat vide.
     */
    MinimapWorldMapButtonWidget(void);

    /**
     * @brief Destructeur par defaut.
     */
    ~MinimapWorldMapButtonWidget(void);

    /**
     * @brief Charge les ressources du bouton carte du monde.
     */
    void load(void);

    /**
     * @brief Libere les ressources du bouton.
     */
    void unload(void);

    /**
     * @brief Dessine l'icone par-dessus la minimap.
     *
     * @param minimapWidget Widget minimap servant d'ancre ecran.
     */
    void draw(const MinimapWidget& minimapWidget) const;

    /**
     * @brief Indique si un point de rendu survole l'icone.
     *
     * @param x Position X de rendu.
     * @param y Position Y de rendu.
     * @param minimapWidget Widget minimap servant d'ancre ecran.
     * @return true si le point tombe dans le rectangle courant du bouton.
     */
    bool containsPoint(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne le curseur souhaite pour la position donnee.
     *
     * @param x Position X de rendu.
     * @param y Position Y de rendu.
     * @param minimapWidget Widget minimap servant d'ancre ecran.
     * @return Curseur pointeur si l'icone est survolee, sinon `NONE`.
     */
    HudCursorType getDesiredCursor(float x, float y, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Retourne true si le bouton a ete clique.
     *
     * @param x Position X du clic.
     * @param y Position Y du clic.
     * @param button Bouton souris.
     * @param minimapWidget Widget minimap servant d'ancre ecran.
     * @return true si le clic gauche touche l'icone.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, const MinimapWidget& minimapWidget) const;

    /**
     * @brief Calcule le rectangle courant de l'icone.
     *
     * @param minimapWidget Widget minimap servant d'ancre ecran.
     * @return Rectangle de rendu de l'icone.
     */
    SDL_FRect getCurrentRect(const MinimapWidget& minimapWidget) const;

private:
    RC2D_Image iconImage;         /**< Texture GPU de l'icone world map. */
    RC2D_ImageData iconImageData; /**< Surface CPU de l'icone world map. */
};
