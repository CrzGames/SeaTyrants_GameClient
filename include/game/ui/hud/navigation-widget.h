#pragma once

#include <RC2D/RC2D.h>

#include <string>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"
#include "game/ui/text-input-edit-history.h"

/**
 * @brief Fenetre HUD de navigation rapide vers un secteur.
 *
 * Le widget expose deux champs distincts:
 * - a gauche, la partie numerique du secteur (`00` a `59`);
 * - a droite, la partie alphabetique (`AA` a `CH`).
 *
 * La saisie repose sur le text input finalise RC2D/SDL pour rester agnostique
 * du clavier physique. Les raccourcis d'edition classiques (`Ctrl+A/C/V/Z`),
 * la selection souris, le clignotement du curseur et l'auto-majuscule sur la
 * partie lettre sont geres directement par le widget.
 */
class NavigationWidget {
public:
    /**
     * @brief Construit la fenetre de navigation dans un etat vide.
     */
    NavigationWidget(void);

    /**
     * @brief Destructeur trivial.
     */
    ~NavigationWidget(void);

    /**
     * @brief Charge les ressources graphiques et reinitialise l'etat runtime.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Met a jour la position, le drag et le blink du curseur.
     *
     * @param dt Delta time de la frame en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre et ses deux champs de saisie.
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris du widget.
     *
     * @param x Position X en coordonnees de rendu.
     * @param y Position Y en coordonnees de rendu.
     * @param button Bouton souris appuye.
     * @param clicks Nombre de clics successifs.
     * @param mouseID Identifiant souris SDL (non utilise ici).
     * @return true si le clic est consomme par le widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite les touches de navigation/edition quand un champ est focus.
     *
     * @return true si la touche est consommee par le widget.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Injecte du texte finalise RC2D/SDL dans le champ actuellement focus.
     *
     * @param text Texte UTF-8 finalise par SDL.
     * @return true si le texte a ete absorbe.
     */
    bool textinput(const char* text);

    /**
     * @brief Retire tout focus texte et stoppe une eventuelle selection souris.
     */
    void clearFocus(void);

    /**
     * @brief Affiche la fenetre de navigation.
     */
    void show(void);

    /**
     * @brief Masque la fenetre de navigation.
     */
    void hide(void);

    /**
     * @brief Retourne true si la fenetre est actuellement visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Retourne true si un champ texte du widget bloque les raccourcis gameplay.
     */
    bool hasBlockingTextInputFocus(void) const;

    /**
     * @brief Indique si un point tombe dans la fenetre courante.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour une position donnee.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

private:
    /**
     * @brief Champ texte interne pilote via une selection/caret simple.
     */
    struct InputFieldState {
        /**
         * @brief Construit un champ vide avec historique actif.
         */
        InputFieldState(void)
            : value{},
              cursorIndex(0U),
              selectionAnchor(0U),
              editHistory(64U)
        {
        }

        /**
         * @brief Construit un champ pre-rempli.
         *
         * @param initialValue Texte initial du champ.
         * @param initialCursor Position initiale du caret.
         * @param initialSelectionAnchor Position initiale de l'ancre de selection.
         */
        InputFieldState(std::string initialValue, std::size_t initialCursor, std::size_t initialSelectionAnchor)
            : value(std::move(initialValue)),
              cursorIndex(initialCursor),
              selectionAnchor(initialSelectionAnchor),
              editHistory(64U)
        {
        }

        std::string value;              /**< Contenu ASCII courant du champ. */
        std::size_t cursorIndex;        /**< Position courante du caret. */
        std::size_t selectionAnchor;    /**< Ancre de selection active. */
        TextInputEditHistory editHistory; /**< Historique Ctrl+Z du champ. */
    };

    /**
     * @brief Identifie quel champ texte est actif.
     */
    enum class FocusedField : int {
        NONE = 0,           /**< Aucun champ actif. */
        SECTOR_NUMBER = 1,  /**< Champ numerique de gauche. */
        SECTOR_LETTERS = 2  /**< Champ alphabetique de droite. */
    };

    RC2D_Font titleFont;     /**< Police du titre et du bouton. */
    RC2D_Font bodyFont;      /**< Police des labels et champs. */
    WindowControlIcons controlIcons; /**< Icnes communes de fermeture / lock. */

    SDL_FRect widgetRect;    /**< Rectangle principal de la fenetre. */
    bool visible;            /**< True si la fenetre est affichee. */

    bool widgetDragging;     /**< True pendant un drag via le header. */
    bool widgetDragLocked;   /**< True si le drag de fenetre est verrouille. */
    float widgetDragOffsetX; /**< Offset souris -> coin haut gauche pendant le drag. */
    float widgetDragOffsetY; /**< Offset souris -> coin haut gauche pendant le drag. */
    float widgetOffsetX;     /**< Decalage runtime applique a la position de base. */
    float widgetOffsetY;     /**< Decalage runtime applique a la position de base. */

    FocusedField focusedField; /**< Champ texte actuellement focus. */
    bool inputSelectingWithMouse; /**< True si la souris etend une selection. */
    bool cursorVisible;          /**< True si le caret doit etre affiche. */
    double cursorBlinkElapsed;   /**< Temps accumule pour le blink du caret. */

    InputFieldState sectorNumberField;  /**< Partie numerique du secteur. */
    InputFieldState sectorLettersField; /**< Partie alphabetique du secteur. */

    std::string statusMessage;   /**< Message de retour affiche sous les champs. */
    bool statusIsError;          /**< True si le message courant est une erreur. */

    /**
     * @brief Retourne la position de base centree dans le game screen.
     */
    SDL_FRect getBaseRect(void) const;

    /**
     * @brief Retourne le rectangle du header de drag.
     */
    SDL_FRect getHeaderRect(void) const;

    /**
     * @brief Retourne le rectangle du bouton de fermeture.
     */
    SDL_FRect getCloseButtonRect(void) const;

    /**
     * @brief Retourne le rectangle du bouton de verrouillage.
     */
    SDL_FRect getLockButtonRect(void) const;

    /**
     * @brief Retourne le rectangle du champ numerique de secteur.
     */
    SDL_FRect getSectorNumberFieldRect(void) const;

    /**
     * @brief Retourne le rectangle du champ alphabetique de secteur.
     */
    SDL_FRect getSectorLettersFieldRect(void) const;

    /**
     * @brief Retourne le rectangle du bouton "Go".
     */
    SDL_FRect getGoButtonRect(void) const;

    /**
     * @brief Retourne le champ actuellement focus, ou `nullptr`.
     */
    InputFieldState* getFocusedFieldState(void);

    /**
     * @brief Retourne le champ actuellement focus en lecture seule, ou `nullptr`.
     */
    const InputFieldState* getFocusedFieldState(void) const;

    /**
     * @brief Retourne le rectangle de rendu du champ actuellement focus.
     */
    SDL_FRect getFocusedFieldRect(void) const;

    /**
     * @brief Retourne la longueur maximale autorisee pour un champ donne.
     */
    std::size_t getMaxLengthForField(FocusedField field) const;

    /**
     * @brief Sanitise un texte entrant pour un champ donne.
     *
     * Le resultat est borne a la syntaxe du champ:
     * - chiffres uniquement pour `SECTOR_NUMBER`;
     * - lettres uniquement, converties en majuscules, pour `SECTOR_LETTERS`.
     */
    std::string sanitizeInputForField(FocusedField field, const char* text) const;

    /**
     * @brief Retourne true si le champ pointe vers une selection non vide.
     */
    bool hasSelection(const InputFieldState& fieldState) const;

    /**
     * @brief Retourne le debut inclus de la selection d'un champ.
     */
    std::size_t getSelectionStart(const InputFieldState& fieldState) const;

    /**
     * @brief Retourne la fin exclue de la selection d'un champ.
     */
    std::size_t getSelectionEnd(const InputFieldState& fieldState) const;

    /**
     * @brief Replie la selection d'un champ sur sa position de caret.
     */
    void clearSelection(InputFieldState& fieldState);

    /**
     * @brief Supprime le texte selectionne d'un champ.
     */
    void deleteSelectedText(InputFieldState& fieldState);

    /**
     * @brief Convertit une position X de rendu en index de caret pour un champ.
     */
    std::size_t getCursorIndexFromPosition(const InputFieldState& fieldState, float renderX, const SDL_FRect& fieldRect) const;

    /**
     * @brief Insere du texte sanitise dans le champ focus.
     */
    bool insertSanitizedTextIntoFocusedField(const std::string& sanitizedText);

    /**
     * @brief Valide les deux champs puis declenche la callback de navigation.
     */
    void commitNavigationRequest(void);

    /**
     * @brief Convertit une partie lettre (`AA`..`CH`) en index secteur Y.
     *
     * @param letters Deux lettres deja majuscules.
     * @param outSectorY Recoit la valeur convertie si la fonction reussit.
     * @return true si la conversion est valide dans les bornes de la map.
     */
    bool parseSectorLetters(const std::string& letters, int* outSectorY) const;
};
