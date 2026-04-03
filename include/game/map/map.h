#pragma once

#include <vector>

#include <RC2D/RC2D.h>

/**
 * @brief Grille isometrique de base pour une map type SeaFight.
 *
 * Le module gere:
 * - la taille de la map en nombre de tuiles;
 * - la taille d'une tuile (48x32 par defaut);
 * - l'origine ecran de la grille;
 * - les conversions tile<->screen;
 * - un rendu debug isometrique (checker + contours).
 */
class Map {
private:
    int widthTiles;   /**< Nombre de tuiles sur l'axe X de la grille. */
    int heightTiles;  /**< Nombre de tuiles sur l'axe Y de la grille. */

    float tileWidth;   /**< Largeur d'une tuile isometrique (diamant). */
    float tileHeight;  /**< Hauteur d'une tuile isometrique (diamant). */

    float originX;  /**< Origine ecran X de la tuile (0,0). */
    float originY;  /**< Origine ecran Y de la tuile (0,0). */

    bool debugFillEnabled;   /**< Active le remplissage checker des tuiles. */
    bool debugLinesEnabled;  /**< Active les contours des tuiles. */

    RC2D_Color debugFillColorA;  /**< Couleur checker A. */
    RC2D_Color debugFillColorB;  /**< Couleur checker B. */
    RC2D_Color debugLineColor;   /**< Couleur des contours. */

    std::vector<int> tileObjects;  /**< Objet place par tile (-1 = vide). */

    /**
     * @brief Convertit une coordonnee tile en index 1D.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return Index lineaire.
     */
    int tileIndex(int tileX, int tileY) const;

public:
    /**
     * @brief Construit une map isometrique.
     *
     * Defaults:
     * - map: 64x64 tuiles
     * - tuile: 48x32
     * - origine: (0,0)
     */
    Map(void);

    /**
     * @brief Destructeur.
     */
    ~Map(void);

    /**
     * @brief Definit le nombre de tuiles de la map.
     * @param width Nombre de tuiles en X.
     * @param height Nombre de tuiles en Y.
     */
    void setMapSize(int width, int height);

    /**
     * @brief Definit la taille d'une tuile isometrique.
     * @param width Largeur tuile.
     * @param height Hauteur tuile.
     */
    void setTileSize(float width, float height);

    /**
     * @brief Definit l'origine ecran de la tuile (0,0).
     * @param x Origine ecran X.
     * @param y Origine ecran Y.
     */
    void setOrigin(float x, float y);

    /**
     * @brief Centre la map dans un rectangle ecran.
     * @param rect Rectangle cible.
     */
    void centerOnRect(const SDL_FRect& rect);

    /**
     * @brief Active/desactive le remplissage checker debug.
     * @param enabled True pour activer.
     */
    void setDebugFillEnabled(bool enabled);

    /**
     * @brief Active/desactive les contours debug.
     * @param enabled True pour activer.
     */
    void setDebugLinesEnabled(bool enabled);

    /**
     * @brief Definit les couleurs debug de remplissage checker.
     * @param colorA Couleur pour (x+y) pair.
     * @param colorB Couleur pour (x+y) impair.
     */
    void setDebugFillColors(const RC2D_Color& colorA, const RC2D_Color& colorB);

    /**
     * @brief Definit la couleur debug des contours.
     * @param color Couleur de ligne.
     */
    void setDebugLineColor(const RC2D_Color& color);

    /**
     * @brief Convertit des coordonnees tile (entieres) en centre ecran.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return Position ecran (centre de la tuile).
     */
    SDL_FPoint tileToScreenCenter(int tileX, int tileY) const;

    /**
     * @brief Convertit des coordonnees tile flottantes en centre ecran.
     * @param tileX Coord tile X flottante.
     * @param tileY Coord tile Y flottante.
     * @return Position ecran (centre de la tuile).
     */
    SDL_FPoint tileToScreenCenterFloat(float tileX, float tileY) const;

    /**
     * @brief Convertit une position ecran en coordonnees tile flottantes.
     * @param screenX Position ecran X.
     * @param screenY Position ecran Y.
     * @return Coordonnees tile flottantes (x,y).
     */
    SDL_FPoint screenToTile(float screenX, float screenY) const;

    /**
     * @brief Convertit une position ecran en tile arrondie au plus proche.
     * @param screenX Position ecran X.
     * @param screenY Position ecran Y.
     * @return Coordonnees tile entieres.
     */
    SDL_Point screenToTileNearest(float screenX, float screenY) const;

    /**
     * @brief Arrondit des coordonnees tile flottantes au plus proche.
     * @param tileX Coord tile X flottante.
     * @param tileY Coord tile Y flottante.
     * @return Coordonnees tile entieres.
     */
    SDL_Point roundTile(float tileX, float tileY) const;

    /**
     * @brief Clamp des coordonnees tile dans les limites de la map.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return Coordonnees tile clamp.
     */
    SDL_Point clampTile(int tileX, int tileY) const;

    /**
     * @brief Teste si une tile est dans la map.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return True si la tile est valide.
     */
    bool isInside(int tileX, int tileY) const;

    /**
     * @brief Place un objet logique dans une tile.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @param objectId Id de l'objet (>=0).
     * @return True si la tile est valide.
     */
    bool setTileObject(int tileX, int tileY, int objectId);

    /**
     * @brief Vide l'objet d'une tile.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return True si la tile est valide.
     */
    bool clearTileObject(int tileX, int tileY);

    /**
     * @brief Recupere l'objet d'une tile.
     * @param tileX Coord tile X.
     * @param tileY Coord tile Y.
     * @return Id de l'objet ou -1 si vide/invalide.
     */
    int getTileObject(int tileX, int tileY) const;

    /**
     * @brief Vide tous les objets de la grille.
     */
    void clearAllTileObjects(void);

    /**
     * @brief Dessine la map en mode debug (checker + contours).
     */
    void drawDebug(void) const;

    /**
     * @brief Retourne la largeur de la map (tuiles).
     */
    int getWidthTiles(void) const;

    /**
     * @brief Retourne la hauteur de la map (tuiles).
     */
    int getHeightTiles(void) const;

    /**
     * @brief Retourne la largeur d'une tuile.
     */
    float getTileWidth(void) const;

    /**
     * @brief Retourne la hauteur d'une tuile.
     */
    float getTileHeight(void) const;

    /**
     * @brief Retourne l'origine X.
     */
    float getOriginX(void) const;

    /**
     * @brief Retourne l'origine Y.
     */
    float getOriginY(void) const;
};
