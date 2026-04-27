#pragma once

#include <RC2D/RC2D.h>
#include <string>
#include "game/map/map.h"

class Camera;

/**
 * @brief Barres de scroll semi-transparentes autour de l'ecran de jeu.
 *
 * Affiche 4 barres sur les bords (haut, bas, gauche, droite) avec
 * des carres decoratifs dans chaque coin. Un clic gauche maintenu
 * sur une barre deplace la camera dans la direction correspondante.
 * Les barres haute et gauche affichent des coordonnees de grille
 * (numeros 00-59 en haut, lettres AA-CH a gauche).
 */
class ScrollBarOverlay {
public:
    ScrollBarOverlay(void);
    ~ScrollBarOverlay(void);

    /**
     * @brief Charge les ressources (font).
     */
    void load(void);

    /**
     * @brief Libere les ressources.
     */
    void unload(void);

    /**
     * @brief Met a jour le scroll continu si le bouton gauche est maintenu sur une barre.
     */
    void update(
        double dt,
        Camera& camera,
        const Map& map,
        const SDL_FRect& screenRect,
        float scrollSpeedSectors = 8.0f);

    /**
     * @brief Dessine les barres, coins et coordonnees.
     */
    void draw(const SDL_FRect& screenRect, const Map& map);

    /**
     * @brief Teste si un clic tombe dans une barre (pour bloquer la propagation).
     * @return true si le clic est consomme par une barre.
     */
    bool handleClick(float x, float y, const SDL_FRect& screenRect);

    /**
     * @brief Retourne true si une barre de scroll est actuellement capturee
     * (clic maintenu actif sur une barre/coin).
     */
    bool isInteracting(void) const;

    /**
     * @brief Donne les cordonnées par rapport à la Tile X/Y (ex: "00-AA") pour une position de tuile donnée (ex: tileX=0, tileY=0). 
     */
    std::string getMapCoordFromTile(int tileX, int tileY) const;
    
private:
    static const int NUM_ROWS = Map::NUM_SECTORS_X;  /**< Nombre de lignes numerotees (00-59). */
    static const int NUM_COLS = Map::NUM_SECTORS_Y;  /**< Nombre de colonnes lettrees (AA-CH). */

    float barThickness;   /**< Epaisseur des barres en pixels. */
    float cornerSize;     /**< Taille des carres de coin en pixels. */

    RC2D_Color barColor;    /**< Couleur semi-transparente des barres. */
    RC2D_Color cornerColor; /**< Couleur des carres de coin. */
    RC2D_Color textColor;   /**< Couleur du texte des coordonnees. */

    RC2D_Font font; /**< Police pour les labels de coordonnees. */

    int activeBar; /**< Zone actuellement maintenue au clic: 0 = aucune, 1..4 = barres, 5..8 = coins. */

    /**
     * @brief Genere le label lettre pour un index de colonne (0=AA, 25=AZ, 26=BA, 33=BH).
     */
    void getColumnLabel(int index, char* out, int outSize) const;

    /**
     * @brief Genere le label numero pour un index de ligne (0="00", 59="59").
     */
    void getRowLabel(int index, char* out, int outSize) const;

    /**
     * @brief Teste si un point (x,y) est dans une barre ou un coin.
     * @return 0=aucune, 1=haut, 2=bas, 3=gauche, 4=droite, 5=HG, 6=HD, 7=BG, 8=BD.
     */
    int hitTestBar(float x, float y, const SDL_FRect& screenRect) const;
};
