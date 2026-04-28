#pragma once

#include <RC2D/RC2D.h>

#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

class ChatWidget {
public:
    enum class ChatMessageAuthor {
        SYSTEM, /**< Message systeme (serveur/canal). */
        PLAYER, /**< Message d'un autre joueur. */
        SELF    /**< Message du joueur local connecte. */
    };

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
     * @brief Traite un clic souris local au chat.
     * @param x Position X du clic en coordonnees de rendu.
     * @param y Position Y du clic en coordonnees de rendu.
     * @param button Bouton souris declencheur.
     * @param clicks Nombre de clics transmis par SDL/RC2D.
     * @param mouseID Identifiant souris SDL.
     * @return True si le clic est consomme par le widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite le scroll molette/trackpad dans la zone messages.
     * @param direction Direction de scroll RC2D (`UP`/`DOWN`).
     * @param wheel_x Delta horizontal flottant.
     * @param wheel_y Delta vertical flottant.
     * @param integer_x Delta horizontal entier.
     * @param integer_y Delta vertical entier.
     * @param mouse_x Position X souris au moment du scroll.
     * @param mouse_y Position Y souris au moment du scroll.
     * @param mouseID Identifiant souris SDL.
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
     * @param key Etiquette texte fournie par SDL.
     * @param scancode Scancode physique de la touche.
     * @param keycode Keycode logique de la touche.
     * @param mod Modifieurs clavier actifs (Shift/Ctrl/Alt...).
     * @param isrepeat True si l'evenement provient d'une repetition automatique.
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
     * @brief Rouvre la fenetre de chat sans recharger ses ressources.
     */
    void show(void);

    /**
     * @brief Ferme la fenetre de chat sans decharger ses ressources.
     */
    void hide(void);

    /**
     * @brief Indique si la fenetre chat est actuellement visible.
     * @return True si la fenetre chat est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief True si la barre de saisie a le focus (le gameplay ne doit pas traiter les memes touches).
     */
    bool hasBlockingTextInputFocus(void) const { return this->visible && this->inputFocused; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     * @param enabled True pour autoriser ce widget a definir le curseur ce frame.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre chat courante.
     * @param x Position X a tester.
     * @param y Position Y a tester.
     * @return True si le point est a l'interieur de la fenetre.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour une position donnee.
     * @param x Position X de rendu a evaluer.
     * @param y Position Y de rendu a evaluer.
     * @return Type de curseur demande par le widget.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Publie un message chat formate selon son auteur.
     * @param author Type d'auteur (SYSTEM / PLAYER / SELF).
     * @param message Contenu textuel du message.
     * @param playerName Nom du joueur (utilise uniquement pour PLAYER).
     *
     * Formate automatiquement le prefixe ("System:", "Moi:" ou "<Nom>:")
     * puis envoie dans l'historique.
     */
    void publishChatMessage(ChatMessageAuthor author, const std::string& message, const std::string& playerName = std::string());
    
private:
    /**
     * @brief Historique des messages conserve en memoire.
     */
    static constexpr std::size_t maxStoredChatMessages = 50; /**< Limite d'historique conservee en memoire. */
    std::vector<std::string> chatMessages; /**< Historique brut des messages du chat (avant wrapping visuel). */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police titre + labels UI. */
    RC2D_Font bodyFont; /**< Police secondaire (reservee si besoin). */
    SDL_FRect widgetRect; /**< Rectangle global de rendu / hit-test. */

    /**
     * @brief Etat de saisie clavier de la barre input.
     */
    std::string inputBuffer; /**< Buffer texte courant saisi dans la barre input. */
    std::size_t cursorIndex; /**< Position d'insertion du curseur dans `inputBuffer`. */
    std::size_t selectionAnchorIndex; /**< Point d'ancrage de la selection active dans `inputBuffer`. */
    bool inputFocused; /**< True si la barre input a le focus clavier. */
    bool inputSelectingWithMouse; /**< True si un glisser souris etend actuellement la selection dans la barre input. */
    bool cursorVisible; /**< True si le curseur de saisie doit etre visible. */
    double cursorBlinkElapsed; /**< Temps accumule pour l'animation de clignotement du curseur. */

    /**
     * @brief Etat de scroll vertical dans la zone messages.
     */
    int scrollFirstLine; /**< Premiere ligne visuelle actuellement affichee dans la zone messages. */
    bool scrollBarDragging; /**< True quand l'utilisateur maintient/drag le pouce de scrollbar. */
    float scrollDragOffsetY; /**< Offset Y souris->thumb pour conserver un drag fluide du scroll. */
    float scrollBarWheelHighlightSec; /**< Surlignage du pouce apres un scroll molette (decroit chaque frame). */

    /**
     * @brief Etat global et deplacement de la fenetre chat.
     */
    bool visible; /**< True si la fenetre chat est visible (false => cachee). */
    bool widgetDragging; /**< True si l'utilisateur est en train de deplacer la fenetre chat. */
    float widgetDragOffsetX; /**< Offset X souris->coin haut gauche pendant le drag de fenetre. */
    float widgetDragOffsetY; /**< Offset Y souris->coin haut gauche pendant le drag de fenetre. */
    float widgetOffsetX; /**< Decalage horizontal applique au chat par rapport a sa position centree. */
    float widgetOffsetY; /**< Decalage vertical applique au chat par rapport a sa position centree. */
    float widgetWidth; /**< Largeur courante du chat (modifiable via redimensionnement). */
    float widgetHeight; /**< Hauteur courante du chat (modifiable via redimensionnement). */
    bool widgetDragLocked; /**< True quand le deplacement du chat (drag header) est verrouille. */

    /**
     * @brief Etat de redimensionnement de la fenetre chat.
     */
    bool widgetResizing; /**< True quand l'utilisateur est en train de redimensionner le chat. */
    float resizeStartMouseX; /**< Position souris X au debut du redimensionnement. */
    float resizeStartMouseY; /**< Position souris Y au debut du redimensionnement. */
    float resizeStartWidth; /**< Largeur du chat capturee au debut du redimensionnement. */
    float resizeStartHeight; /**< Hauteur du chat capturee au debut du redimensionnement. */

    /**
     * @brief Autorisation de pilotage du curseur souris.
     */
    bool cursorEnabled; /**< Autorise ce widget a piloter le curseur souris ce frame. */
    WindowControlIcons controlIcons; /**< Helper des icones croix/cadenas. */

    /**
     * @brief Calcule la fenetre visible du texte de l'input autour d'un index donne.
     * @param inputMaxWidth Largeur utile disponible pour afficher le texte.
     * @param focusIndex Index qui doit rester visible dans la fenetre.
     * @param outStart Debut inclus calcule de la fenetre visible.
     * @param outEnd Fin exclue calculee de la fenetre visible.
     */
    void computeInputVisibleRange(float inputMaxWidth, std::size_t focusIndex, std::size_t* outStart, std::size_t* outEnd) const;

    /**
     * @brief Convertit une position X de rendu en index de curseur dans l'input du chat.
     * @param renderX Position X de rendu a convertir.
     * @param inputArea Rectangle courant de la barre input.
     * @return Index d'insertion le plus proche dans `inputBuffer`.
     */
    std::size_t getInputCursorIndexFromPosition(float renderX, const SDL_FRect& inputArea) const;

    /**
     * @brief Indique si une plage de texte est selectionnee dans l'input du chat.
     * @return True si au moins un caractere est selectionne.
     */
    bool hasInputSelection(void) const;

    /**
     * @brief Retourne le debut inclus de la selection courante dans l'input.
     * @return Index de debut de selection, deja normalise.
     */
    std::size_t getInputSelectionStart(void) const;

    /**
     * @brief Retourne la fin exclue de la selection courante dans l'input.
     * @return Index de fin de selection, deja normalise.
     */
    std::size_t getInputSelectionEnd(void) const;

    /**
     * @brief Replie la selection courante sur la position actuelle du curseur.
     */
    void clearInputSelection(void);

    /**
     * @brief Supprime le texte actuellement selectionne dans l'input du chat.
     *
     * Le curseur est replace au debut de la plage supprimee.
     */
    void deleteSelectedInputText(void);
};
