#pragma once

#include <RC2D/RC2D.h>

#include <string>
#include <vector>

#include "game/entities/player.h"
#include "game/map/map.h"
#include "game/ui/hud/announcements-widget.h"
#include "game/ui/hud/barre-action-widget.h"
#include "game/ui/hud/center-ship-button-widget.h"
#include "game/ui/hud/chat-widget.h"
#include "game/ui/hud/espion-search-player-widget.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/hud/log-book-widget.h"
#include "game/ui/hud/markets-and-bazar-widget.h"
#include "game/ui/hud/minimap-widget.h"
#include "game/ui/hud/params-minimap-widget.h"
#include "game/ui/overlay/scroll-bar-overlay.h"
#include "game/ui/overlay/sector-coordinate-overlay.h"
#include "game/ui/overlay/tile-click-marker-overlay.h"

class Camera;

class IngameHudOverlay {
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
     * @brief Met a jour l'ensemble du HUD et des overlays map.
     */
    void update(double dt, Camera& camera, const Map& map);

    /**
     * @brief Dessine le widget de fond UI gameplay (en dessous du monde).
     */
    void drawBackgroundWidget(void);

    /**
     * @brief Dessine les widgets UI gameplay (au dessus du monde).
     * @param map Map courante pour positionner les widgets dependants du monde.
     * @param player Joueur courant pour afficher son secteur.
     */
    void drawWidgets(const Map& map, const Player& player);

    /**
     * @brief Dessine le marqueur de clic (overlay monde).
     */
    void drawTileClickMarkerOverlay(const Map& map);

    /**
     * @brief Dessine la scrollbar (overlay monde).
     */
    void drawScrollBarOverlay(const Map& map);

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
     * @brief Wrapper de clic souris pour les overlays map.
     * @return True si le clic est consomme par un overlay map.
     */
    bool handleMapOverlayMousePressed(float x, float y, RC2D_MouseButton button, const Map& map);

    /**
     * @brief Notifie un clic tuile gameplay aux overlays map.
     */
    void notifyMapTileClicked(int tileX, int tileY);

    /**
     * @brief Propage le clavier au HUD.
     * @return True si la touche est consommee par l'UI.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Publie une ligne d'annonce dans la fenetre "Announcements".
     */
    void publishAnnouncementRow(const std::string& rowText);

    /**
     * @brief Publie un resultat de recherche dans la fenetre "Espion".
     */
    void publishEspionSearchResult(const std::string& resultText);

    /**
     * @brief Publie une ligne dans la fenetre "Journal de bord".
     */
    void publishLogbookRow(const LogBookWidget::LogBookRow& row);

    /**
     * @brief Remplace les lignes de l'onglet Bazar.
     */
    void setBazarRows(const std::vector<MarketsAndBazarWidget::BazarRow>& rows);

    /**
     * @brief Remplace les lignes de l'onglet Marche noir.
     */
    void setBlackMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows);

    /**
     * @brief Remplace les lignes de l'onglet Marche basique.
     */
    void setBasicMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows);

    /**
     * @brief Remplace les lignes de l'onglet Marche d'evenement.
     */
    void setEventMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows);
    
private:
    /**
     * @brief Couches de fenetres flottantes dessinees de bas vers haut.
     */
    enum class WindowLayer : int {
        CHAT = 0,              /**< Fenetre chat. */
        ESPION = 1,            /**< Fenetre espion (recherche joueur). */
        PARAMS_MINIMAP = 2,    /**< Fenetre des parametres minimap. */
        ANNOUNCEMENTS = 3,     /**< Fenetre annonces serveur. */
        LOG_BOOK = 4,          /**< Fenetre journal de bord. */
        MARKETS_AND_BAZAR = 5  /**< Fenetre marches + bazar. */
    };

    /**
     * @brief Overlay de l'UI gameplay.
     */
    SectorCoordinateOverlay sectorCoordinateOverlay; /**< Overlay texte du secteur courant. */
    TileClickMarkerOverlay tileClickMarkerOverlay; /**< Marqueur visuel de clic sur tuile. */
    ScrollBarOverlay scrollBarOverlay; /**< Barres de scroll monde (haut/bas/gauche/droite). */

    /**
     * @brief Widgets de l'UI gameplay.
     */
    BackgroundWidget backgroundWidget; /**< Fond UI gameplay (haut/bas). */
    MinimapWidget minimapWidget; /**< Widget minimap. */
    BarreActionWidget barreActionWidget; /**< Barre d'action en bas-centre. */
    CenterShipButtonWidget centerShipButtonWidget; /**< Widget bouton centrer navire. */
    ChatWidget chatWidget; /**< Fenetre chat interactive. */
    EspionSearchPlayerWidget espionSearchPlayerWidget; /**< Fenetre "Espion" de recherche joueur. */
    ParamsMinimapWidget paramsMinimapWidget; /**< Fenetre de parametres de la minimap. */
    AnnouncementsWidget announcementsWidget; /**< Fenetre "Announcements". */
    LogBookWidget logBookWidget; /**< Fenetre "Journal de bord". */
    MarketsAndBazarWidget marketsAndBazarWidget; /**< Fenetre "Marches / Bazar". */

    /**
     * @brief Pile de rendu des fenetres flottantes (bas -> haut).
     */
    std::vector<WindowLayer> windowDrawOrder; /**< Ordre de rendu de bas vers haut. */

    /**
     * @brief Etats precedents de visibilite pour detecter les ouvertures.
     */
    bool prevChatVisible; /**< Etat visible precedent du chat. */
    bool prevEspionVisible; /**< Etat visible precedent de la fenetre espion. */
    bool prevParamsMiniMapVisible; /**< Etat visible precedent de la fenetre ParamsMiniMap. */
    bool prevAnnouncementsVisible; /**< Etat visible precedent de la fenetre announcements. */
    bool prevLogBookVisible; /**< Etat visible precedent de la fenetre journal de bord. */
    bool prevMarketsAndBazarVisible; /**< Etat visible precedent de la fenetre marches + bazar. */

    /**
     * @brief Deplace une couche de fenetre en fin de pile de rendu.
     * @param layer Couche cible a amener au premier plan.
     */
    void bringWindowToFront(WindowLayer layer);

    /**
     * @brief Synchronise l'ordre de fenetres quand une fenetre vient de s'ouvrir.
     *
     * Compare les etats visibles courants/precedents pour remonter
     * automatiquement la fenetre nouvellement ouverte.
     */
    void syncWindowOrderOnOpen(void);
};
