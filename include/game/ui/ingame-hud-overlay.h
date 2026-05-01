#pragma once

#include <RC2D/RC2D.h>

#include <array>
#include <string>
#include <vector>

#include "game/entities/player.h"
#include "game/map/map.h"
#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/account-management-widget.h"
#include "game/ui/hud/announcements-widget.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/hud/barre-action-widget.h"
#include "game/ui/hud/captcha-widget.h"
#include "game/ui/hud/center-ship-button-widget.h"
#include "game/ui/hud/chat-widget.h"
#include "game/ui/hud/espion-search-player-widget.h"
#include "game/ui/hud/experience-bar-widget.h"
#include "game/ui/hud/game-settings-widget.h"
#include "game/ui/hud/guild-mortar-widget.h"
#include "game/ui/hud/guild-tower-widget.h"
#include "game/ui/hud/hp-bar-widget.h"
#include "game/ui/hud/leaderboard-widget.h"
#include "game/ui/hud/log-book-widget.h"
#include "game/ui/hud/markets-and-bazar-widget.h"
#include "game/ui/hud/minimap-espion-button-widget.h"
#include "game/ui/hud/minimap-params-button-widget.h"
#include "game/ui/hud/minimap-worldmap-button-widget.h"
#include "game/ui/hud/minimap-widget.h"
#include "game/ui/hud/money-widget.h"
#include "game/ui/hud/params-minimap-widget.h"
#include "game/ui/hud/top-bar-main-currency-widget.h"
#include "game/ui/hud/top-bar-menu-widget.h"
#include "game/ui/hud/worldmap-widget.h"
#include "game/ui/hud/zoom-widget.h"
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
    void update(double dt, Camera& camera, Map& map);

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
    bool handleMapOverlayMousePressed(float x, float y, RC2D_MouseButton button, Camera& camera, Map& map);

    /**
     * @brief Indique si le clic touche le bouton de recentrage navire.
     */
    bool centerShipButtonMousepressed(float x, float y, RC2D_MouseButton button) const;

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
     * @brief Propage un texte finalise RC2D/SDL au HUD.
     * @return True si le texte est consomme par l'UI.
     */
    bool textinput(const RC2D_TextInputEventInfo* info);

    /**
     * @brief Synchronise la demande de saisie texte native avec le focus GUI courant.
     */
    void syncPlatformTextInputState(void);

    /**
     * @brief True si un champ texte du HUD a le focus : pas de defilement camera ni raccourcis @ref GameScene.
     */
    bool isBlockingGameplayKeyboardInput(void) const;

    /**
     * @brief Retourne l'overlay texte du secteur courant.
     */
    SectorCoordinateOverlay& getSectorCoordinateOverlay(void) { return this->sectorCoordinateOverlay; }

    /**
     * @brief Retourne le marqueur de clic sur tuile.
     */
    TileClickMarkerOverlay& getTileClickMarkerOverlay(void) { return this->tileClickMarkerOverlay; }

    /**
     * @brief Retourne les barres de scroll monde.
     */
    ScrollBarOverlay& getScrollBarOverlay(void) { return this->scrollBarOverlay; }

    /**
     * @brief Retourne le fond UI gameplay.
     */
    BackgroundWidget& getBackgroundWidget(void) { return this->backgroundWidget; }

    /**
     * @brief Retourne la barre de menu haute.
     */
    TopBarMenuWidget& getTopBarMenuWidget(void) { return this->topBarMenuWidget; }

    /**
     * @brief Retourne le widget des monnaies principales affichees sur la top bar.
     * @return Reference mutable vers le widget rubies / gold permanent.
     */
    TopBarMainCurrencyWidget& getTopBarMainCurrencyWidget(void) { return this->topBarMainCurrencyWidget; }

    /**
     * @brief Retourne la minimap.
     */
    MinimapWidget& getMinimapWidget(void) { return this->minimapWidget; }

    /**
     * @brief Retourne le bouton espion ancre sur la minimap.
     */
    MinimapEspionButtonWidget& getMinimapEspionButtonWidget(void) { return this->minimapEspionButtonWidget; }

    /**
     * @brief Retourne le bouton parametres ancre sur la minimap.
     */
    MinimapParamsButtonWidget& getMinimapParamsButtonWidget(void) { return this->minimapParamsButtonWidget; }

    /**
     * @brief Retourne le bouton world map ancre sur la minimap.
     */
    MinimapWorldMapButtonWidget& getMinimapWorldMapButtonWidget(void) { return this->minimapWorldMapButtonWidget; }

    /**
     * @brief Retourne la fenetre gameplay de carte du monde.
     *
     * Cette API permet notamment au gameplay/reseau de mettre a jour les tags
     * de guild par nom de map quand les donnees serveur arrivent.
     */
    WorldMapWidget& getWorldMapWidget(void) { return this->worldMapWidget; }

    /**
     * @brief Retourne la barre d'action.
     */
    BarreActionWidget& getBarreActionWidget(void) { return this->barreActionWidget; }

    /**
     * @brief Retourne le bouton de recentrage du navire.
     */
    CenterShipButtonWidget& getCenterShipButtonWidget(void) { return this->centerShipButtonWidget; }

    /**
     * @brief Retourne le widget de zoom.
     */
    ZoomWidget& getZoomWidget(void) { return this->zoomWidget; }

    /**
     * @brief Retourne la barre de HP HUD pour l'alimenter depuis le gameplay.
     * @return Reference mutable vers la barre de points de vie.
     */
    HpBarWidget& getHpBarWidget(void) { return this->hpBarWidget; }

    /**
     * @brief Retourne la barre d'experience HUD pour l'alimenter depuis le gameplay.
     * @return Reference mutable vers la barre d'experience.
     */
    ExperienceBarWidget& getExperienceBarWidget(void) { return this->experienceBarWidget; }

    /**
     * @brief Retourne la fenetre de chat.
     */
    ChatWidget& getChatWidget(void) { return this->chatWidget; }

    /**
     * @brief Retourne la fenetre Espion.
     */
    EspionSearchPlayerWidget& getEspionSearchPlayerWidget(void) { return this->espionSearchPlayerWidget; }

    /**
     * @brief Retourne la fenetre money pour l'alimenter depuis le gameplay.
     * @return Reference mutable vers le widget des monnaies.
     */
    MoneyWidget& getMoneyWidget(void) { return this->moneyWidget; }

    /**
     * @brief Retourne la fenetre de parametres minimap.
     */
    ParamsMinimapWidget& getParamsMinimapWidget(void) { return this->paramsMinimapWidget; }

    /**
     * @brief Retourne la fenetre de parametres de jeu pour la configurer depuis le gameplay.
     * @return Reference mutable vers le widget de parametres.
     */
    GameSettingsWidget& getGameSettingsWidget(void) { return this->gameSettingsWidget; }

    /**
     * @brief Retourne la fenetre d'annonces.
     */
    AnnouncementsWidget& getAnnouncementsWidget(void) { return this->announcementsWidget; }

    /**
     * @brief Retourne la fenetre Journal de bord.
     */
    LogBookWidget& getLogBookWidget(void) { return this->logBookWidget; }

    /**
     * @brief Retourne la fenetre Marches / Bazar.
     */
    MarketsAndBazarWidget& getMarketsAndBazarWidget(void) { return this->marketsAndBazarWidget; }

    /**
     * @brief Retourne la fenetre de gestion du compte pour l'alimenter depuis le gameplay.
     * @return Reference mutable vers le widget compte / apparence / navires.
     */
    AccountManagementWidget& getAccountManagementWidget(void) { return this->accountManagementWidget; }

    /**
     * @brief Retourne la fenetre de mortier de guilde.
     */
    GuildMortarWidget& getGuildMortarWidget(void) { return this->guildMortarWidget; }

    /**
     * @brief Retourne la fenetre de Tower de guilde.
     */
    GuildTowerWidget& getGuildTowerWidget(void) { return this->guildTowerWidget; }

    /**
     * @brief Acces au widget captcha (callbacks @ref CaptchaWidget::setOnValidateRequested, etc.).
     */
    CaptchaWidget& getCaptchaWidget(void) { return this->captchaWidget; }

    /**
     * @brief Retourne la fenetre de classements.
     */
    LeaderboardWidget& getLeaderboardWidget(void) { return this->leaderboardWidget; }

    /**
     * @brief Applique la visibilite d'un widget HUD pilotable.
     */
    void setHudWidgetVisible(GameSettingsWidget::HudScaleTarget target, bool visible);

    /**
     * @brief Inverse la visibilite d'un widget HUD pilotable.
     */
    void toggleHudWidgetVisibility(GameSettingsWidget::HudScaleTarget target);

private:
    /**
     * @brief Couches de fenetres flottantes dessinees de bas vers haut.
     */
    enum class WindowLayer : int {
        CHAT = 0,               /**< Fenetre chat. */
        ESPION = 1,             /**< Fenetre espion (recherche joueur). */
        MONEY = 2,              /**< Fenetre money / monnaies du joueur. */
        PARAMS_MINIMAP = 3,     /**< Fenetre des parametres minimap. */
        WORLD_MAP = 4,          /**< Fenetre gameplay de carte du monde. */
        GAME_SETTINGS = 5,      /**< Fenetre des parametres de jeu. */
        ANNOUNCEMENTS = 6,      /**< Fenetre annonces serveur. */
        LOG_BOOK = 7,           /**< Fenetre journal de bord. */
        MARKETS_AND_BAZAR = 8,  /**< Fenetre marches + bazar. */
        ACCOUNT_MANAGEMENT = 9, /**< Fenetre compte/apparence/navires. */
        GUILD_MORTAR = 10,      /**< Fenetre de mortier de guilde. */
        GUILD_TOWER = 11,       /**< Fenetre de Tower de guilde. */
        CAPTCHA = 12,           /**< Verification captcha (reseau). */
        LEADERBOARD = 13        /**< Fenetre classements (lien site). */
    };

    /**
     * @brief Doit preceder les widgets : evite toute lecture pendant construction partielle de l'overlay.
     */
    bool suppressUserSettingsSave = false;

    /**
     * @brief Overlay de l'UI gameplay.
     */
    SectorCoordinateOverlay sectorCoordinateOverlay; /**< Overlay texte du secteur courant. */
    TileClickMarkerOverlay tileClickMarkerOverlay;   /**< Marqueur visuel de clic sur tuile. */
    ScrollBarOverlay scrollBarOverlay;               /**< Barres de scroll monde (haut/bas/gauche/droite). */

    /**
     * @brief Widgets de l'UI gameplay.
     */
    BackgroundWidget backgroundWidget;                      /**< Fond UI gameplay (haut/bas). */
    TopBarMenuWidget topBarMenuWidget;                     /**< Barre de menu haute en haut de l'ecran. */
    TopBarMainCurrencyWidget topBarMainCurrencyWidget;     /**< Widget permanent des rubies / gold en haut a gauche. */
    MinimapWidget minimapWidget;                           /**< Widget minimap. */
    MinimapEspionButtonWidget minimapEspionButtonWidget;   /**< Bouton espion ancre sur la minimap. */
    MinimapParamsButtonWidget minimapParamsButtonWidget;   /**< Bouton params minimap ancre sur la minimap. */
    MinimapWorldMapButtonWidget minimapWorldMapButtonWidget; /**< Bouton world map ancre sur la minimap. */
    BarreActionWidget barreActionWidget;                   /**< Barre d'action en bas-centre. */
    CenterShipButtonWidget centerShipButtonWidget;         /**< Widget bouton centrer navire. */
    ZoomWidget zoomWidget;                                 /**< Widget de zoom (barre + slider). */
    HpBarWidget hpBarWidget;                               /**< Barre de HP affichee a droite de la zoom bar. */
    ExperienceBarWidget experienceBarWidget;               /**< Barre d'XP affichee sous la barre HP. */
    ChatWidget chatWidget;                                 /**< Fenetre chat interactive. */
    EspionSearchPlayerWidget espionSearchPlayerWidget;     /**< Fenetre "Espion" de recherche joueur. */
    MoneyWidget moneyWidget;                               /**< Fenetre "Money" des monnaies du joueur. */
    ParamsMinimapWidget paramsMinimapWidget;               /**< Fenetre de parametres de la minimap. */
    WorldMapWidget worldMapWidget;                         /**< Fenetre gameplay de carte du monde. */
    GameSettingsWidget gameSettingsWidget;                 /**< Fenetre "Parametres" du client. */
    AnnouncementsWidget announcementsWidget;               /**< Fenetre "Announcements". */
    LogBookWidget logBookWidget;                           /**< Fenetre "Journal de bord". */
    MarketsAndBazarWidget marketsAndBazarWidget;           /**< Fenetre "Marches / Bazar". */
    AccountManagementWidget accountManagementWidget;       /**< Fenetre "Compte / Apparence / Navires". */
    GuildMortarWidget guildMortarWidget;                   /**< Fenetre "Guild Mortier". */
    GuildTowerWidget guildTowerWidget;                     /**< Fenetre "Guild Tower". */
    CaptchaWidget captchaWidget;                           /**< Fenetre de verification captcha serveur. */
    LeaderboardWidget leaderboardWidget;                   /**< Fenetre classements (URL selon environnement). */

    /**
     * @brief Ressources partagees par le HUD.
     */
    RC2D_Font tooltipFont; /**< Police partagee des tooltips HUD locaux. */

    /**
     * @brief Visibilite et mode de configuration des widgets HUD.
     */
    std::array<bool, static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT)> hudWidgetVisibility; /**< Etats visibles des widgets HUD pilotables. */
    bool hudConfiguratorMode;                              /**< True si le mode de configuration des positions HUD est actif. */
    bool hudConfiguratorRestoreGameSettingsVisibility;     /**< True si les parametres doivent etre rouverts a la sortie du configurateur. */

    /**
     * @brief Etat de drag d'un widget HUD configurable.
     */
    bool hudConfiguratorDragging;                          /**< True pendant le drag d'un widget HUD configurable. */
    GameSettingsWidget::HudScaleTarget hudConfiguratorDraggedTarget; /**< Widget HUD actuellement deplace. */
    float hudConfiguratorDragGrabOffsetX;                  /**< Offset souris -> rect HUD au debut du drag. */
    float hudConfiguratorDragGrabOffsetY;                  /**< Offset souris -> rect HUD au debut du drag. */
    SDL_FPoint hudConfiguratorDragStartOffset;             /**< Offset memorise au debut du drag. */
    SDL_FRect hudConfiguratorDragStartRect;                /**< Rect memorise au debut du drag. */
    SDL_FRect hudConfiguratorDragStartTargetRect;          /**< Rect widget memorise au debut du drag (sans padding). */

    /**
     * @brief Etat de drag du panneau de configuration HUD.
     */
    bool hudConfiguratorPanelDragging;                     /**< True pendant le drag du panneau du configurateur. */
    float hudConfiguratorPanelDragGrabOffsetX;             /**< Offset souris -> panneau au debut du drag. */
    float hudConfiguratorPanelDragGrabOffsetY;             /**< Offset souris -> panneau au debut du drag. */
    SDL_FPoint hudConfiguratorPanelOffset;                 /**< Decalage ecran applique au panneau du configurateur. */
    SDL_FPoint hudConfiguratorPanelDragStartOffset;        /**< Offset panneau memorise au debut du drag. */
    SDL_FRect hudConfiguratorPanelDragStartRect;           /**< Rect panneau memorise au debut du drag. */

    /**
     * @brief Ordre de mise a jour et de dessin des fenetres flottantes (fond -> premier plan).
     *
     * Chaque entree est une @ref WindowLayer. Le premier element est traite le plus tot
     * dans les boucles d'@c update / @c draw ; le dernier correspond a la fenetre la plus
     * au-dessus (empilement visuel). Les tests de hit souris parcourent la liste en sens
     * inverse pour privilegier la fenetre du dessus.
     *
     * L'ordre est reinitialise a la valeur par defaut dans @ref load ; @ref bringWindowToFront
     * fait tourner une couche vers la fin du conteneur. Ce classement n'est pas actuellement
     * serialise dans le fichier utilisateur (seules echelles, visibilites, offsets, etc. le sont).
     */
    std::vector<WindowLayer> windowDrawOrder;
    MinimapWidget::Tooltip hoveredMinimapTooltip; /**< Tooltip minimap actuellement survole. */
    float hoveredMinimapTooltipMouseX;            /**< Position X souris pour tooltip minimap. */
    float hoveredMinimapTooltipMouseY;            /**< Position Y souris pour tooltip minimap. */

    /**
     * @brief Etats precedents de visibilite pour detecter les ouvertures.
     */
    bool prevChatVisible;               /**< Etat visible precedent du chat. */
    bool prevEspionVisible;             /**< Etat visible precedent de la fenetre espion. */
    bool prevMoneyVisible;              /**< Etat visible precedent de la fenetre money. */
    bool prevParamsMiniMapVisible;      /**< Etat visible precedent de la fenetre ParamsMiniMap. */
    bool prevWorldMapVisible;           /**< Etat visible precedent de la fenetre carte du monde. */
    bool prevGameSettingsVisible;       /**< Etat visible precedent de la fenetre parametres. */
    bool prevAnnouncementsVisible;      /**< Etat visible precedent de la fenetre announcements. */
    bool prevLogBookVisible;            /**< Etat visible precedent de la fenetre journal de bord. */
    bool prevMarketsAndBazarVisible;    /**< Etat visible precedent de la fenetre marches + bazar. */
    bool prevAccountManagementVisible;  /**< Etat visible precedent de la fenetre compte/apparence/navires. */
    bool prevGuildMortarVisible;        /**< Etat visible precedent de la fenetre mortier de guilde. */
    bool prevGuildTowerVisible;         /**< Etat visible precedent de la fenetre Tower de guilde. */
    bool prevCaptchaVisible;            /**< Etat visible precedent de la fenetre captcha. */
    bool prevLeaderboardVisible;        /**< Etat visible precedent de la fenetre classements. */

    bool keepDefaultCursorAfterClose;   /**< Suspension temporaire du hover apres fermeture. */
    float keepDefaultCursorMouseX;      /**< X de reference au moment de la fermeture. */
    float keepDefaultCursorMouseY;      /**< Y de reference au moment de la fermeture. */

    /**
     * @brief Deplace une couche de fenetre en fin de pile de rendu.
     * @param layer Couche cible a amener au premier plan.
     */
    void bringWindowToFront(WindowLayer layer);

    /**
     * @brief Ouvre la fenetre associee a une action top bar.
     */
    void handleTopBarAction(TopBarMenuWidget::Action action);

    /**
     * @brief Synchronise les etats actifs des icones top bar.
     */
    void syncTopBarActionState(void);

    /**
     * @brief Dessine le tooltip de survol des boutons minimap.
     */
    void drawHoveredMinimapTooltip(void) const;

    /**
     * @brief Synchronise l'ordre de fenetres quand une fenetre vient de s'ouvrir.
     *
     * Compare les etats visibles courants/precedents pour remonter
     * automatiquement la fenetre nouvellement ouverte.
     */
    void syncWindowOrderOnOpen(void);

    /**
     * @brief Retourne l'etat visible courant d'une couche de fenetre.
     * @param layer Couche dont on veut connaitre la visibilite.
     * @return true si la fenetre est visible, sinon false.
     */
    bool isWindowLayerVisible(WindowLayer layer) const;

    /**
     * @brief Retourne le curseur demande par une couche de fenetre.
     * @param layer Couche interrogee.
     * @param mouseX Position X courante de la souris.
     * @param mouseY Position Y courante de la souris.
     * @return Curseur souhaite par cette couche, ou NONE si non applicable.
     */
    HudCursorType getWindowLayerDesiredCursor(WindowLayer layer, float mouseX, float mouseY) const;

    /**
     * @brief Force temporairement le curseur standard apres fermeture d'une fenetre.
     *
     * L'objectif est d'eviter qu'un hover "traverse" instantanement vers une
     * fenetre en dessous alors que la souris n'a pas encore bouge.
     *
     * @param mouseX Position X de la souris au moment de la fermeture.
     * @param mouseY Position Y de la souris au moment de la fermeture.
     */
    void beginCursorResetAfterClose(float mouseX, float mouseY);

    /**
     * @brief Indique si le hover doit rester suspendu tant que la souris n'a pas bouge.
     *
     * @param mouseX Position X courante de la souris.
     * @param mouseY Position Y courante de la souris.
     * @return true si le curseur doit rester en mode standard, sinon false.
     */
    bool shouldKeepDefaultCursorAfterClose(float mouseX, float mouseY);

    /**
     * @brief Retourne l'etat visible d'un widget HUD pilotable.
     * @param target Cible HUD demandee.
     * @return True si le widget doit etre dessine/interactable.
     */
    bool isHudWidgetVisible(GameSettingsWidget::HudScaleTarget target) const;

    /**
     * @brief Active le mode de configuration des positions HUD.
     */
    void startHudConfiguratorMode(void);

    /**
     * @brief Quitte le mode de configuration des positions HUD.
     */
    void stopHudConfiguratorMode(void);

    /**
     * @brief Gere les clics pendant le mode de configuration HUD.
     *
     * @return True si le clic est consomme.
     */
    bool handleHudConfiguratorMousePressed(float x, float y, RC2D_MouseButton button);

    /**
     * @brief Met a jour le drag et le curseur du configurateur HUD.
     */
    void updateHudConfigurator(float mouseX, float mouseY);

    /**
     * @brief Dessine l'overlay de configuration HUD.
     */
    void drawHudConfiguratorOverlay(void) const;

    /**
     * @brief Retourne le curseur desire par le configurateur HUD.
     */
    HudCursorType getHudConfiguratorDesiredCursor(float mouseX, float mouseY) const;

    /**
     * @brief Retourne le rectangle courant du widget HUD configurable.
     */
    SDL_FRect getHudConfiguratorTargetRect(GameSettingsWidget::HudScaleTarget target) const;

    /**
     * @brief Retourne le rectangle de mise en evidence du widget HUD configurable.
     */
    SDL_FRect getHudConfiguratorSelectionRect(GameSettingsWidget::HudScaleTarget target) const;

    /**
     * @brief Retourne l'offset ecran courant d'un widget HUD configurable.
     */
    SDL_FPoint getHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target) const;

    /**
     * @brief Applique un offset ecran a un widget HUD configurable.
     */
    void setHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target, const SDL_FPoint& offset);

    /**
     * @brief Reinitialise la position d'un widget HUD configurable.
     */
    void resetHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target);

    /**
     * @brief Reinitialise toutes les positions configurables du HUD.
     */
    void resetAllHudConfiguratorPositions(void);

    /**
     * @brief Lit `settings/user_settings.json` depuis le stockage utilisateur RC2D/SDL.
     *
     * Attend que le stockage utilisateur soit pret (`rc2d_storage_user`). Si le fichier
     * est absent ou vide, cree le repertoire `settings` et ecrit un JSON par defaut
     * (l'ecriture force transient @ref suppressUserSettingsSave a false le temps du flush).
     *
     * Applique au widget des parametres et aux widgets HUD concernees : echelles, visibilites,
     * offsets des blocs configurables, decalage du panneau du configurateur, liaisons clavier,
     * options graphiques (fond des coordonnees, fog of war, sillages, VFX des autres joueurs,
     * preset de salve), etc. Silencieux si fichier invalide, trop gros, ou parse JSON en echec.
     */
    void loadUserSettingsFromDisk(void);

    /**
     * @brief Ecrit l'etat courant des reglages persistants dans `settings/user_settings.json`.
     *
     * Ne fait rien si @ref suppressUserSettingsSave est true (chargement ou operations groupes)
     * ou si le stockage utilisateur n'est pas pret. Sinon serialise une version de schema,
     * le bloc HUD (echelles, visibilites, offsets, panneau configurateur), les controles
     * (vitesse de defilement camera, scancodes) et le bloc graphique du @ref GameSettingsWidget.
     *
     * @note Appeler depuis les callbacks du widget parametres ou apres modification locale
     *       des donnees a persister ; la fonction alloue et libere le JSON via cJSON.
     */
    void saveUserSettingsToDisk(void);

};
