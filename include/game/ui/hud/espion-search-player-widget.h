#pragma once

#include <RC2D/RC2D.h>

#include <functional>
#include <string>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

class EspionSearchPlayerWidget {
public:
    EspionSearchPlayerWidget(void);
    ~EspionSearchPlayerWidget(void);

    /**
     * @brief Charge les ressources du widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Met a jour le rectangle cible du widget.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre espion.
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris (fermeture via croix).
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite le clavier de l'input ID joueur.
     * @return True si la touche est consommee.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Retire le focus de l'input ID et masque son curseur.
     */
    void clearFocus(void);

    /**
     * @brief Indique si la fenetre espion est actuellement visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre espion courante.
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
     * @brief Publie un texte de resultat dans la case du bas.
     * @param resultText Texte a afficher a droite du label.
     */
    void publishSearchResult(const std::string& resultText);

    /**
     * @brief Met a jour le prix en rubis affiche dans la fenetre espion.
     * @param newRubiesPrice Nouveau prix en rubis. Les valeurs negatives sont ramenees a zero.
     */
    void setRubiesPrice(int newRubiesPrice);

    /**
     * @brief Definit la callback executee lors d'un clic sur "Trouver un joueur".
     * @param callback Fonction appelee avec l'ID joueur actuellement saisi.
     *
     * Passez une callback vide pour desactiver l'action du bouton sans modifier
     * le rendu du widget.
     */
    void setOnFindPlayerRequested(const std::function<void(const std::string&)>& callback);

    /**
     * @brief Rouvre la fenetre espion.
     */
    void show(void);

    /**
     * @brief Ferme la fenetre espion.
     */
    void hide(void);

private:
    /**
     * @brief Texte de resultat affiche dans la zone de sortie.
     */
    std::string searchResultText; /**< Valeur affichee a droite de "Resultat de la recherche :". */
    int rubiesPrice; /**< Prix actuel en rubis affiche a gauche du bouton de recherche. */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police du titre et des labels. */
    RC2D_Font bodyFont; /**< Police secondaire pour les textes standards. */
    RC2D_Image rubiesPriceIcon; /**< Icone de rubis affichee a gauche du prix. */
    SDL_FRect widgetRect; /**< Rectangle global de rendu/hit-test. */

    /**
     * @brief Etat global de visibilite de la fenetre.
     */
    bool visible; /**< True si la fenetre est visible. */

    /**
     * @brief Etat de saisie du champ d'ID joueur.
     */
    std::string playerIdInput; /**< Valeur affichee dans le champ ID joueur (reserve). */
    std::size_t cursorIndex; /**< Position d'insertion du curseur dans l'input ID. */
    std::size_t selectionAnchorIndex; /**< Point d'ancrage de la selection active dans l'input ID. */
    bool inputFocused; /**< True si l'input ID a le focus clavier. */
    bool inputSelectingWithMouse; /**< True si un glisser souris etend actuellement la selection dans l'input ID. */
    bool cursorVisible; /**< True si le curseur doit etre affiche. */
    double cursorBlinkElapsed; /**< Temps accumule pour le blink du curseur. */

    /**
     * @brief Etat de deplacement de la fenetre.
     */
    bool widgetDragging; /**< True si l'utilisateur drag la fenetre via le header. */
    float widgetDragOffsetX; /**< Offset X souris->coin haut gauche pendant le drag. */
    float widgetDragOffsetY; /**< Offset Y souris->coin haut gauche pendant le drag. */
    float widgetOffsetX; /**< Decalage X applique a la position de base. */
    float widgetOffsetY; /**< Decalage Y applique a la position de base. */

    /**
     * @brief Autorisation de pilotage du curseur souris.
     */
    bool cursorEnabled; /**< True si ce widget peut piloter le curseur ce frame. */
    WindowControlIcons controlIcons; /**< Helper des icones de controle. */
    std::function<void(const std::string&)> onFindPlayerRequested; /**< Callback executee au clic sur le bouton de recherche. */

    /**
     * @brief Convertit une position X de rendu en index de curseur pour l'input ID.
     * @param renderX Position X de rendu a convertir.
     * @param inputRect Rectangle courant du champ de saisie.
     * @return Index d'insertion le plus proche dans `playerIdInput`.
     */
    std::size_t getPlayerIdCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const;

    /**
     * @brief Indique si une plage de texte est selectionnee dans l'input ID.
     * @return True si au moins un caractere est actuellement selectionne.
     */
    bool hasPlayerIdSelection(void) const;

    /**
     * @brief Retourne le debut inclus de la selection courante dans l'input ID.
     * @return Index de debut de selection, deja normalise.
     */
    std::size_t getPlayerIdSelectionStart(void) const;

    /**
     * @brief Retourne la fin exclue de la selection courante dans l'input ID.
     * @return Index de fin de selection, deja normalise.
     */
    std::size_t getPlayerIdSelectionEnd(void) const;

    /**
     * @brief Replie la selection courante sur la position actuelle du curseur.
     */
    void clearPlayerIdSelection(void);

    /**
     * @brief Supprime le texte actuellement selectionne dans l'input ID.
     *
     * Le curseur est replace au debut de la plage supprimee.
     */
    void deleteSelectedPlayerIdText(void);
};
