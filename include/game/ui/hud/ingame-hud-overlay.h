#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/center-ship-button-widget.h"
#include "game/ui/hud/chat-widget.h"
#include "game/ui/hud/espion-search-player-widget.h"
#include "game/ui/hud/minimap-widget.h"
#include "game/ui/hud/params-minimap-widget.h"
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
    ChatWidget chatWidget; /**< Fenetre chat interactive. */
    EspionSearchPlayerWidget espionSearchPlayerWidget; /**< Fenetre "Espion" de recherche joueur. */
    ParamsMinimapWidget paramsMinimapWidget; /**< Fenetre de parametres de la minimap. */

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
     * @brief Met a jour les widgets qui ont un etat dynamique.
     */
    void update(double dt);

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

    /**
     * @brief Propage un clic souris au HUD.
     * @return True si le clic est consomme par l'UI.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Propage la molette souris/trackpad au HUD.
     * @return True si l'evenement est consomme par l'UI.
     */
    bool mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID);

    /**
     * @brief Propage le clavier au HUD.
     * @return True si la touche est consommee par l'UI.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
};
