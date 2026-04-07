#pragma once

#include <RC2D/RC2D.h>
#include <string>
#include <vector>

enum class ChatMessageAuthor {
    SYSTEM, /**< Message systeme (serveur/canal). */
    PLAYER, /**< Message d'un autre joueur. */
    SELF    /**< Message du joueur local connecte. */
};

class ChatWidget {
private:
    static constexpr std::size_t kMaxStoredMessages = 50; /**< Limite d'historique (modifiable facilement). */

    RC2D_Font titleFont; /**< Police titre + labels UI. */
    RC2D_Font bodyFont; /**< Police secondaire (reservee si besoin). */
    SDL_FRect widgetRect; /**< Rectangle global de rendu / hit-test. */

    /** @brief Buffer texte courant saisi par l'utilisateur dans la barre input. */
    std::string inputBuffer;
    /** @brief Position d'insertion du curseur dans `inputBuffer` (index caractere). */
    std::size_t cursorIndex;
    /** @brief True si la barre input a le focus clavier. */
    bool inputFocused;
    /** @brief True si le curseur de saisie doit etre visible a cet instant. */
    bool cursorVisible;
    /** @brief Temps accumule pour l'animation de clignotement du curseur. */
    double cursorBlinkElapsed;

    /** @brief Historique brut des messages du chat (avant wrapping visuel). */
    std::vector<std::string> messages;
    /**
     * @brief Ajoute un message dans l'historique du chat.
     *
     * Methode interne: utiliser publishMessage() depuis l'exterieur.
     */
    void pushMessage(const std::string& message);
    /** @brief Premiere ligne visuelle actuellement affichee dans la zone messages. */
    int scrollFirstLine;
    /** @brief True quand l'utilisateur maintient/drag le pouce de scrollbar. */
    bool scrollBarDragging;
    /** @brief Offset Y souris->thumb pour conserver un drag fluide du scroll. */
    float scrollDragOffsetY;
    /** @brief True si la fenetre chat est visible (false => cachee). */
    bool visible;
    /** @brief True si l'utilisateur est en train de deplacer la fenetre chat. */
    bool widgetDragging;
    /** @brief Offset X souris->coin haut gauche pendant le drag de fenetre. */
    float widgetDragOffsetX;
    /** @brief Offset Y souris->coin haut gauche pendant le drag de fenetre. */
    float widgetDragOffsetY;
    /** @brief Decalage horizontal applique au chat par rapport a sa position centree. */
    float widgetOffsetX;
    /** @brief Decalage vertical applique au chat par rapport a sa position centree. */
    float widgetOffsetY;
    /** @brief Largeur courante du chat (modifiable via redimensionnement). */
    float widgetWidth;
    /** @brief Hauteur courante du chat (modifiable via redimensionnement). */
    float widgetHeight;
    /** @brief True quand le deplacement du chat (drag header) est verrouille. */
    bool widgetDragLocked;
    /** @brief True quand l'utilisateur est en train de redimensionner le chat. */
    bool widgetResizing;
    /** @brief Position souris X au debut du redimensionnement. */
    float resizeStartMouseX;
    /** @brief Position souris Y au debut du redimensionnement. */
    float resizeStartMouseY;
    /** @brief Largeur du chat capturee au debut du redimensionnement. */
    float resizeStartWidth;
    /** @brief Hauteur du chat capturee au debut du redimensionnement. */
    float resizeStartHeight;
    /** @brief Autorise ce widget a piloter le curseur souris ce frame. */
    bool cursorEnabled;

public:
    ChatWidget(void);
    ~ChatWidget(void);

    /**
     * @brief Charge les ressources du widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Met a jour les animations/input continu (blink curseur + drag scrollbar).
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Publie un message chat formate selon son auteur.
     * @param author Type d'auteur (SYSTEM / PLAYER / SELF).
     * @param message Contenu textuel du message.
     * @param playerName Nom du joueur (utilise uniquement pour PLAYER).
     *
     * Formate automatiquement le prefixe ("System:", "Moi:" ou "<Nom>:")
     * puis envoie dans l'historique.
     */
    void publishMessage(ChatMessageAuthor author, const std::string& message, const std::string& playerName = std::string());

    /**
     * @brief Traite un clic souris local au chat.
     * @return True si le clic est consomme par le widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite le scroll molette/trackpad dans la zone messages.
     * @return True si l'evenement est consomme.
     */
    bool mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float wheel_x,
        float wheel_y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID);

    /**
     * @brief Traite le clavier du chat (edition, curseur, validation).
     * @return True si la touche est consommee par le widget.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Dessine la fenetre de chat dans le rectangle cible.
     */
    void draw(void) const;

    /**
     * @brief Retire le focus du champ de saisie et masque son curseur.
     */
    void clearFocus(void);

    /**
     * @brief Indique si la fenetre chat est actuellement visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre chat courante.
     */
    bool containsPoint(float x, float y) const;
};

