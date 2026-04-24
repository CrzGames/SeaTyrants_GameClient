#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/top-bar-action-button-widget.h"
#include "game/ui/hud/top-bar-announcement-button-widget.h"
#include "game/ui/hud/top-bar-chat-button-widget.h"
#include "game/ui/hud/top-bar-disconnect-button-widget.h"
#include "game/ui/hud/top-bar-guild-button-widget.h"
#include "game/ui/hud/top-bar-leaderboard-button-widget.h"
#include "game/ui/hud/top-bar-logbook-button-widget.h"
#include "game/ui/hud/top-bar-money-button-widget.h"
#include "game/ui/hud/top-bar-quest-button-widget.h"
#include "game/ui/hud/top-bar-settings-button-widget.h"
#include "game/ui/hud/top-bar-ship-button-widget.h"

class TopBarMenuWidget {
public:
    using Action = TopBarActionButtonWidget::Action;

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
     * @brief Met a jour l'etat hover des icones top bar.
     * @return True si au moins une icone est survolee.
     */
    bool update(float mouseX, float mouseY);

    /**
     * @brief Dessine la barre de menu haute et la rangee d'icones.
     */
    void draw(void);

    /**
     * @brief Traite un clic souris sur une icone top bar.
     * @return Action declenchee, ou `Action::NONE`.
     */
    Action mousepressed(float x, float y, RC2D_MouseButton button);

    /**
     * @brief Met a jour l'etat "ouvert" d'une action.
     */
    void setActionActive(Action action, bool active);

private:
    RC2D_Image topBarMenuUiImage; /**< Image decorative de la barre de menu haute. */
    RC2D_Font tooltipFont;        /**< Police du tooltip affiche au survol. */
    Action hoveredAction;         /**< Action actuellement survolee, si presente. */
    float hoveredMouseX;          /**< Position X souris memorisee pour le tooltip. */
    float hoveredMouseY;          /**< Position Y souris memorisee pour le tooltip. */

    TopBarChatButtonWidget chatButton;                 /**< Bouton chat. */
    TopBarGuildButtonWidget guildButton;               /**< Bouton guilde. */
    TopBarQuestButtonWidget questButton;               /**< Bouton quetes. */
    TopBarLeaderboardButtonWidget leaderboardButton;   /**< Bouton classement. */
    TopBarMoneyButtonWidget moneyButton;               /**< Bouton economie. */
    TopBarShipButtonWidget shipButton;                 /**< Bouton navire. */
    TopBarAnnouncementButtonWidget announcementButton; /**< Bouton annonces. */
    TopBarLogbookButtonWidget logbookButton;           /**< Bouton journal de bord. */
    TopBarSettingsButtonWidget settingsButton;         /**< Bouton parametres. */
    TopBarDisconnectButtonWidget disconnectButton;     /**< Bouton deconnexion. */

    /**
     * @brief Recalcule la position des icones selon la taille du game screen.
     */
    void updateButtonLayout(void);

    /**
     * @brief Dessine le tooltip du bouton survole.
     */
    void drawHoveredTooltip(void) const;
};
