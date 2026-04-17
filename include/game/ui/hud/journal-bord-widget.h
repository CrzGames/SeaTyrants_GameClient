#pragma once

#include <RC2D/RC2D.h>
#include <string>
#include <vector>

/**
 * @brief Fenetre HUD "Journal de bord".
 *
 * - Style visuel aligne sur les autres fenetres HUD;
 * - Deplacement via header;
 * - Fermeture via croix en haut a droite;
 * - Liste de logs en 2 colonnes (date/heure + message);
 * - Scroll via molette (souris dans la fenetre) et drag du pouce de scrollbar.
 * - Affichage limite a 20 messages visibles par page.
 */
class JournalBordWidget {
public:
    struct LogEntry {
        std::string dateTime;
        std::string message;
    };

private:
    RC2D_Font titleFont; /**< Police du titre. */
    RC2D_Font bodyFont; /**< Police du contenu (date + messages). */
    SDL_FRect widgetRect; /**< Rectangle global du widget. */

    bool visible; /**< True si la fenetre est visible. */
    bool cursorEnabled; /**< True si ce widget peut piloter le curseur ce frame. */

    std::vector<LogEntry> entries; /**< Historique des lignes du journal. */
    int scrollFirstRow; /**< Premiere ligne visuelle affichee. */
    bool scrollBarDragging; /**< True si le pouce de scrollbar est en drag. */
    float scrollDragOffsetY; /**< Offset souris->thumb pour drag fluide. */

    bool widgetDragging; /**< True pendant le drag de la fenetre via header. */
    float widgetDragOffsetX; /**< Offset X souris->widget pendant drag. */
    float widgetDragOffsetY; /**< Offset Y souris->widget pendant drag. */
    float widgetOffsetX; /**< Decalage horizontal depuis l'ancrage ecran. */
    float widgetOffsetY; /**< Decalage vertical depuis l'ancrage ecran. */

    /**
     * @brief Ajoute quelques lignes de demonstration.
     *
     * Permet de valider rapidement le rendu sans backend connecte.
     */
    void pushDemoEntries(void);

public:
    JournalBordWidget(void);
    ~JournalBordWidget(void);

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
     * @brief Ajoute une entree au journal.
     * @param dateTime Date/heure de la ligne (ex: "15.04 20:12").
     * @param message Message associe.
     */
    void pushEntry(const std::string& dateTime, const std::string& message);

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
};
