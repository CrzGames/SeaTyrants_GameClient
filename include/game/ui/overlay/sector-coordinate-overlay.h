#pragma once

#include <RC2D/RC2D.h>
#include "game/entities/player.h"
#include "game/map/map.h"

/**
 * @brief Overlay HUD qui affiche le secteur courant du joueur.
 *
 * Positionnement:
 * - centre horizontal de l'ecran;
 * - au-dessus de la zone map (dans la barre UI haute).
 *
 * Format affiche:
 * - "Secteur : 00-AA" (ou "Secteur : ??-??" hors grille).
 */
class SectorCoordinateOverlay {
public:
    SectorCoordinateOverlay(void);
    ~SectorCoordinateOverlay(void);

    /**
     * @brief Charge les ressources necessaires a l'overlay.
     */
    void load(void);

    /**
     * @brief Libere les ressources de l'overlay.
     */
    void unload(void);

    /**
     * @brief Dessine la coordonnee secteur du joueur en haut au centre.
     * @param map Map courante (conversion tuile -> secteur + rect map).
     * @param player Joueur courant (position tuile).
     */
    void draw(const Map& map, const Player& player);

private:
    /**
     * @brief Ressources de rendu texte de l'overlay secteur.
     */
    RC2D_Font font;      /**< Police utilisee pour le texte secteur. */
    RC2D_Color textColor;/**< Couleur du texte du secteur. */

    /**
     * @brief Genere le label horizontal d'un secteur (00..59).
     * @param index Index horizontal secteur.
     * @param out Buffer de sortie.
     * @param outSize Taille du buffer.
     */
    void getRowLabel(int index, char* out, int outSize) const;

    /**
     * @brief Genere le label vertical d'un secteur (AA..CH).
     * @param index Index vertical secteur.
     * @param out Buffer de sortie.
     * @param outSize Taille du buffer.
     */
    void getColumnLabel(int index, char* out, int outSize) const;
};
