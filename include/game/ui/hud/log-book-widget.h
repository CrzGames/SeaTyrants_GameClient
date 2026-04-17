#pragma once

#include <RC2D/RC2D.h>

#include <string>
#include <vector>

class LogBookWidget {
public:
    /**
     * @brief Representation publique d'une ligne du journal de bord.
     */
    struct LogBookRow {
        std::string dateTime; /**< Date/heure de la ligne (format libre). */
        std::string message; /**< Message textuel associe a la ligne. */
    };

    LogBookWidget(void);
    ~LogBookWidget(void);

    /**
     * @brief Charge les ressources et reinitialise l'etat runtime.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees dans load().
     */
    void unload(void);

    /**
     * @brief Met a jour drag fenetre, drag scrollbar et curseur contextuel.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre "Journal de bord".
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris du widget.
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite la molette quand la souris est dans la fenetre du journal.
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
     * @brief Indique si le widget est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre courante.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Publie une ligne dans le journal.
     * @param row Donnees de la ligne a ajouter.
     */
    void publishLogBookRow(const LogBookRow& row);

private:
    /**
     * @brief Historique des lignes du journal de bord conserve en memoire.
     */
    std::vector<LogBookRow> rows; /**< Historique des lignes du journal. */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police du titre. */
    RC2D_Font bodyFont; /**< Police du contenu (date + messages). */
    SDL_FRect widgetRect; /**< Rectangle global du widget. */

    /**
     * @brief Etat global de visibilite de la fenetre.
     */
    bool visible; /**< True si la fenetre est visible. */

    /**
     * @brief Etat de scroll vertical.
     */
    int scrollFirstRow; /**< Premiere ligne visuelle affichee. */
    bool scrollBarDragging; /**< True si le pouce de scrollbar est en drag. */
    float scrollDragOffsetY; /**< Offset souris->thumb pour drag fluide. */

    /**
     * @brief Etat de deplacement de la fenetre.
     */
    bool widgetDragging; /**< True pendant le drag de la fenetre via header. */
    float widgetDragOffsetX; /**< Offset X souris->widget pendant drag. */
    float widgetDragOffsetY; /**< Offset Y souris->widget pendant drag. */
    float widgetOffsetX; /**< Decalage horizontal depuis l'ancrage ecran. */
    float widgetOffsetY; /**< Decalage vertical depuis l'ancrage ecran. */

    /**
     * @brief Autorisation de pilotage du curseur souris.
     */
    bool cursorEnabled; /**< True si ce widget peut piloter le curseur ce frame. */
};

