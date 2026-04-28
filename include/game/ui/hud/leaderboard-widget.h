#pragma once

#include <RC2D/RC2D.h>

#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre HUD "Classements" : lien vers le site des classements selon l'environnement de build.
 *
 * Affiche un texte d'introduction et une URL cliquable qui ouvre le navigateur (@c SDL_OpenURL).
 * L'URL depend du build : localhost en developpement, staging ou production sinon.
 */
class LeaderboardWidget {
public:
    LeaderboardWidget(void);
    ~LeaderboardWidget(void);

    /**
     * @brief Charge polices et icone de fermeture, repositionne la fenetre et remet les etats d'interaction.
     */
    void load(void);

    /**
     * @brief Libere les ressources allouees par load().
     */
    void unload(void);

    /**
     * @brief Met a jour le deplacement par glisser-deposer sur le bandeau titre et le survol du lien.
     * @param dt Delta temps en secondes (non utilise).
     */
    void update(double dt);

    /**
     * @brief Dessine le panneau (cadre, titre, texte wrappe, lien souligne au survol).
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris (fermeture, drag entete, ouverture URL).
     * @param x Position X du clic (espace rendu).
     * @param y Position Y du clic (espace rendu).
     * @param button Bouton souris active.
     * @param clicks Nombre de clics (non utilise).
     * @param mouseID Identifiant souris SDL (non utilise).
     * @return True si l'evenement est consomme par ce widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Indique si la fenetre classements est visible.
     * @return True si la fenetre est affichee.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise ou non ce widget a piloter le curseur souris pour ce frame.
     * @param enabled True pour activer la detection de curseur (POINTER / MOVE).
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point recouvre le rectangle courant du widget.
     * @param x Coordonnee X a tester.
     * @param y Coordonnee Y a tester.
     * @return True si le point est a l'interieur de la fenetre.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le type de curseur souhaite pour une position donnee.
     * @param x Position X dans l'espace rendu.
     * @param y Position Y dans l'espace rendu.
     * @return POINTER sur le lien, MOVE sur l'entete (drag), ou valeurs par defaut ailleurs.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Affiche la fenetre classements.
     */
    void show(void);

    /**
     * @brief Masque la fenetre classements.
     */
    void hide(void);

private:
    /**
     * @brief Lignes de texte d'introduction (apres wrapping).
     */
    std::vector<std::string> introLines;
    /**
     * @brief Lignes affichees pour l'URL (apres wrapping), vide en build dev.
     */
    std::vector<std::string> urlLines;
    /**
     * @brief Rectangle englobant toute la zone cliquable de l'URL (vide si pas de lien).
     */
    SDL_FRect urlHitRect;
    /**
     * @brief True si la souris survole actuellement la zone URL.
     */
    bool linkHovered;

    RC2D_Font titleFont;        /**< Police du titre de fenetre. */
    RC2D_Font bodyFont;         /**< Police du corps et du lien. */
    SDL_FRect widgetRect;       /**< Rectangle global courant. */

    bool visible;               /**< Visibilite de la fenetre. */
    bool widgetDragging;        /**< True pendant le deplacement par l'entete. */
    float widgetDragOffsetX;    /**< Offset souris vers coin haut gauche en X. */
    float widgetDragOffsetY;    /**< Offset souris vers coin haut gauche en Y. */
    float widgetOffsetX;        /**< Decalage utilisateur depuis la position de base. */
    float widgetOffsetY;        /**< Decalage utilisateur depuis la position de base. */

    bool cursorEnabled;         /**< Active la contribution au curseur HUD. */
    WindowControlIcons controlIcons; /**< Rendu du bouton fermer. */

    /**
     * @brief Recalcule introLines, urlLines et urlHitRect selon la taille du corps.
     */
    void rebuildLayout(void);

    /**
     * @brief Met a jour linkHovered depuis la position souris rendu.
     * @param mouseX Coordonnee X souris.
     * @param mouseY Coordonnee Y souris.
     */
    void refreshLinkHover(float mouseX, float mouseY);
};
