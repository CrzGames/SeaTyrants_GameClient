#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/center-ship-button-widget.h"
#include "game/ui/hud/minimap-widget.h"
#include "game/ui/hud/sector-coordinate-overlay.h"

class Map;
class Player;

/**
 * @brief UI gameplay de la scene en jeu.
 *
 * Cette classe encapsule:
 * - le fond UI (haut/bas) dessine en coordonnees ecran absolues;
 * - la minimap;
 * - le bouton "centrer la map".
 * - l'affichage du secteur courant du joueur.
 */
class IngameHudOverlay {
private:
    RC2D_Image backgroundUiImage; /**< Fond UI gameplay (haut/bas). */
    MinimapWidget minimapWidget; /**< Widget minimap. */
    CenterShipButtonWidget centerShipButtonWidget; /**< Widget bouton centrer navire. */
    SectorCoordinateOverlay sectorCoordinateOverlay; /**< Overlay texte du secteur courant. */

public:
    IngameHudOverlay(void);
    ~IngameHudOverlay(void);

    /**
     * @brief Charge les ressources et configure les widgets UI.
     */
    void load(void);

    /**
     * @brief Libere les ressources UI.
     */
    void unload(void);

    /**
     * @brief Dessine le fond UI gameplay (en dessous du monde).
     */
    void drawBackground(void);

    /**
     * @brief Dessine les widgets UI gameplay (au dessus du monde).
     * @param map Map courante pour positionner les widgets dependants du monde.
     * @param player Joueur courant pour afficher son secteur.
     */
    void drawWidgets(const Map& map, const Player& player);
};
