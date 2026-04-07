#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Fenetre "Parametres de MiniMap" avec options cochables.
 */
class ParamsMinimapWidget {
private:
    RC2D_Font titleFont; /**< Police du titre. */
    RC2D_Font bodyFont; /**< Police des libelles des options. */
    SDL_FRect widgetRect; /**< Rectangle principal du widget. */
    bool visible; /**< True si la fenetre est visible. */

    bool showPlayers; /**< Etat de la case "Afficher les joueurs". */
    bool showMonsters; /**< Etat de la case "Afficher les monstres". */
    bool showShips; /**< Etat de la case "Afficher les navires". */
    bool showTreasures; /**< Etat de la case "Afficher les tresors". */

    bool widgetDragging; /**< True pendant un drag via le header. */
    float widgetDragOffsetX; /**< Offset X souris->coin haut gauche en drag. */
    float widgetDragOffsetY; /**< Offset Y souris->coin haut gauche en drag. */
    float widgetOffsetX; /**< Decalage horizontal depuis la position de base. */
    float widgetOffsetY; /**< Decalage vertical depuis la position de base. */

public:
    ParamsMinimapWidget(void);
    ~ParamsMinimapWidget(void);

    /**
     * @brief Charge les ressources graphiques du widget.
     *
     * Ouvre les polices, initialise la position de base et remet
     * les etats interactifs dans un etat propre (visible, pas de drag).
     */
    void load(void);

    /**
     * @brief Libere les ressources graphiques du widget.
     *
     * Ferme les polices ouvertes dans load() pour eviter les fuites memoire.
     */
    void unload(void);

    /**
     * @brief Met a jour la position et le drag du widget.
     * @param dt Delta time en secondes (non utilise actuellement).
     *
     * Recalcule le rectangle final a partir de l'ancrage ecran + offsets,
     * puis applique le deplacement si un drag est en cours.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre "Parametres de MiniMap".
     *
     * Rend le cadre, le header, la croix et les 4 lignes d'options
     * avec leurs cases a cocher.
     */
    void draw(void) const;

    /**
     * @brief Traite le clic souris sur le widget.
     * @param x Position X du clic en coordonnees de rendu.
     * @param y Position Y du clic en coordonnees de rendu.
     * @param button Bouton souris.
     * @param clicks Nombre de clics (non utilise ici).
     * @param mouseID Identifiant souris SDL (non utilise ici).
     * @return True si l'evenement est consomme par ce widget, sinon false.
     *
     * Gere la fermeture via croix, le toggle des cases et le drag par le header.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);
};

