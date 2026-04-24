#pragma once

#include <RC2D/RC2D.h>

#include <string>

/**
 * @brief Bouton d'action unitaire de la top bar.
 *
 * Cette classe gere le chargement de l'icone, son rendu, son hover,
 * son etat "actif" et la detection de clic.
 */
class TopBarActionButtonWidget {
public:
    /**
     * @brief Actions supportees par les boutons de la top bar.
     */
    enum class Action : int {
        NONE = 0,
        CHAT = 1,
        GUILD = 2,
        QUEST = 3,
        LEADERBOARD = 4,
        MONEY = 5,
        SHIP = 6,
        ANNOUNCEMENT = 7,
        LOGBOOK = 8,
        SETTINGS = 9,
        DISCONNECT = 10,
        PIRATE_EXAM = 11
    };

    TopBarActionButtonWidget(Action action, const char* imagePath);
    virtual ~TopBarActionButtonWidget(void);

    /**
     * @brief Charge l'icone du bouton.
     */
    void load(void);

    /**
     * @brief Libere les ressources du bouton.
     */
    void unload(void);

    /**
     * @brief Positionne le bouton dans l'espace de rendu du game screen.
     * @param localX Offset X en pixels depuis le coin haut-gauche du game screen.
     * @param localY Offset Y en pixels depuis le coin haut-gauche du game screen.
     */
    void setLocalPosition(float localX, float localY);

    /**
     * @brief Defini l'etat actif du bouton.
     */
    void setActive(bool isActive);

    /**
     * @brief Met a jour l'etat hover a partir de la souris.
     * @return True si le pointeur survole le bouton.
     */
    bool updateHover(float mouseX, float mouseY);

    /**
     * @brief Dessine le bouton.
     */
    void draw(void);

    /**
     * @brief Traite un clic souris et retourne l'action si le bouton est vise.
     */
    Action mousepressed(float x, float y, RC2D_MouseButton button) const;

    /**
     * @brief Retourne l'action associee a ce bouton.
     */
    Action getAction(void) const { return this->action; }

private:
    Action action;           /**< Action metier du bouton. */
    std::string imagePath;   /**< Chemin asset de l'icone. */
    RC2D_UIImage uiImage;    /**< Image UI de l'icone. */
    bool hovered;            /**< True si le curseur est au-dessus du bouton. */
    bool active;             /**< True si la fenetre cible est actuellement ouverte. */

    /**
     * @brief Retourne le rectangle theorique courant du bouton.
     */
    SDL_FRect getCurrentRect(void) const;

    /**
     * @brief Indique si un point vise la zone cliquable du bouton.
     */
    bool containsPoint(float x, float y) const;
};
