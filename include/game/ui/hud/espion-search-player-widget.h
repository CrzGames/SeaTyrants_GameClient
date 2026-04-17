#pragma once

#include <RC2D/RC2D.h>

#include <string>

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
     * @brief Publie un texte de resultat dans la case du bas.
     * @param resultText Texte a afficher a droite du label.
     */
    void publishSearchResult(const std::string& resultText);

private:
    /**
     * @brief Texte de resultat affiche dans la zone de sortie.
     */
    std::string searchResultText; /**< Valeur affichee a droite de "Resultat de la recherche :". */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police du titre et des labels. */
    RC2D_Font bodyFont; /**< Police secondaire pour les textes standards. */
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
    bool inputFocused; /**< True si l'input ID a le focus clavier. */
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
};
