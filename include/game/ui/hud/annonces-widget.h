#pragma once

#include <RC2D/RC2D.h>
#include <string>
#include <vector>

/**
 * @brief Fenetre HUD "Annonces" (style Legend Pirates).
 */
class AnnoncesWidget {
private:
    RC2D_Font titleFont; /**< Police du titre. */
    RC2D_Font bodyFont; /**< Police du contenu des annonces. */
    SDL_FRect widgetRect; /**< Rectangle global du widget. */
    bool visible; /**< True si la fenetre est visible. */
    std::vector<std::string> announcements; /**< Historique brut des annonces. */
    int scrollFirstLine; /**< Premiere ligne visible dans la zone annonces. */
    bool scrollBarDragging; /**< True si le pouce de scrollbar est en cours de drag. */
    float scrollDragOffsetY; /**< Offset souris->thumb pour un drag fluide. */

    bool widgetDragging; /**< True pendant le deplacement via le header. */
    bool widgetDragLocked; /**< True si le cadenas verrouille le drag. */
    float widgetDragOffsetX; /**< Offset X souris->widget pendant le drag. */
    float widgetDragOffsetY; /**< Offset Y souris->widget pendant le drag. */
    float widgetOffsetX; /**< Decalage horizontal depuis la position de base. */
    float widgetOffsetY; /**< Decalage vertical depuis la position de base. */
    float widgetWidth; /**< Largeur courante du widget. */
    float widgetHeight; /**< Hauteur courante du widget. */
    bool widgetResizing; /**< True quand l'utilisateur redimensionne le widget. */
    float resizeStartMouseX; /**< Position X souris au debut du resize. */
    float resizeStartMouseY; /**< Position Y souris au debut du resize. */
    float resizeStartWidth; /**< Largeur capturee au debut du resize. */
    float resizeStartHeight; /**< Hauteur capturee au debut du resize. */
    bool cursorEnabled; /**< True si ce widget peut piloter le curseur ce frame. */

public:
    AnnoncesWidget(void);
    ~AnnoncesWidget(void);

    /**
     * @brief Charge les ressources du widget Annonces.
     *
     * Ouvre les polices, calcule la position de base et remet
     * les etats d'interaction (drag/scroll/visibilite) a zero.
     */
    void load(void);

    /**
     * @brief Libere les ressources allouees par load().
     *
     * Ferme les polices RC2D pour eviter toute fuite memoire.
     */
    void unload(void);

    /**
     * @brief Met a jour la position et l'etat de drag de la fenetre.
     * @param dt Delta time en secondes (non utilise pour l'instant).
     *
     * Recalcule le rectangle final depuis l'ancrage ecran puis applique
     * le deplacement en cours tant que le bouton souris reste enfonce.
     */
    void update(double dt);

    /**
     * @brief Ajoute une annonce dans la liste interne.
     * @param message Texte de l'annonce.
     *
     * Le texte est conserve brut; le wrapping est calcule a l'affichage
     * selon la largeur disponible de la zone de contenu.
     */
    void pushAnnouncement(const std::string& message);

    /**
     * @brief Dessine integralement la fenetre Annonces.
     *
     * Rend le cadre, le header, les boutons cadenas/croix, le contenu
     * wrappe, puis la scrollbar seulement si le texte depasse.
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris de la fenetre.
     * @param x Position X du clic (espace rendu).
     * @param y Position Y du clic (espace rendu).
     * @param button Bouton souris active.
     * @param clicks Nombre de clics (parametre API, non utilise ici).
     * @param mouseID Identifiant souris SDL (non utilise ici).
     * @return True si l'evenement est consomme par le widget.
     *
     * Gere: toggle du cadenas, fermeture via croix, drag via header.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite la molette dans la zone de texte des annonces.
     * @param direction Sens du scroll (haut/bas).
     * @param wheel_x Delta horizontal molette (non utilise).
     * @param wheel_y Delta vertical molette flottant (non utilise).
     * @param integer_x Delta horizontal entier (non utilise).
     * @param integer_y Delta vertical entier, utilise comme pas de scroll.
     * @param mouse_x Position X souris pour tester la zone active.
     * @param mouse_y Position Y souris pour tester la zone active.
     * @param mouseID Identifiant souris SDL (non utilise).
     * @return True si le scroll est consomme par Annonces, sinon false.
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
     * @brief Indique si la fenetre annonces est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre annonces courante.
     */
    bool containsPoint(float x, float y) const;
};

