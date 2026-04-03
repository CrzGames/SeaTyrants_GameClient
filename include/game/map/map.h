#pragma once

#include <vector>

#include <RC2D/RC2D.h>

/**
 * @brief Representation logique d'une map isometrique.
 *
 * Cette classe gere:
 * - les dimensions de la grille en tuiles;
 * - la taille des tuiles isometriques;
 * - l'origine ecran de la grille;
 * - les conversions tile <-> ecran;
 * - une couche collision statique (tuile bloquee / traversable).
 */
class Map {
private:
    int widthTiles;   /**< Nombre de tuiles sur l'axe X. */
    int heightTiles;  /**< Nombre de tuiles sur l'axe Y. */

    float tileWidth;   /**< Largeur d'une tuile isometrique. */
    float tileHeight;  /**< Hauteur d'une tuile isometrique. */

    float originX;  /**< Origine ecran X de la tuile (0,0). */
    float originY;  /**< Origine ecran Y de la tuile (0,0). */

    std::vector<Uint8> blockedTiles; /**< 0 = traversable, 1 = bloquee. */

    /**
     * @brief Convertit une tuile en index lineaire du tableau interne.
     * @param tileX Coordonnee tuile sur X.
     * @param tileY Coordonnee tuile sur Y.
     * @return Index lineaire dans blockedTiles.
     */
    int tileIndex(int tileX, int tileY) const;

public:
    /**
     * @brief Construit une map par defaut (64x64, tuiles 48x32).
     */
    Map(void);

    /**
     * @brief Destructeur.
     */
    ~Map(void);

    /**
     * @brief Definit le nombre de tuiles de la map.
     * @param width Nombre de tuiles sur X.
     * @param height Nombre de tuiles sur Y.
     */
    void setMapSize(int width, int height);

    /**
     * @brief Definit la taille d'une tuile isometrique.
     * @param width Largeur d'une tuile en pixels.
     * @param height Hauteur d'une tuile en pixels.
     */
    void setTileSize(float width, float height);

    /**
     * @brief Definit l'origine ecran de la map.
     * @param x Origine ecran X.
     * @param y Origine ecran Y.
     */
    void setOrigin(float x, float y);

    /**
     * @brief Centre la map dans un rectangle de rendu.
     * @param rect Rectangle cible en coordonnees ecran.
     */
    void centerOnRect(const SDL_FRect& rect);

    /**
     * @brief Convertit une tuile entiere vers son centre ecran.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     * @return Position ecran du centre de la tuile.
     */
    SDL_FPoint tileToScreenCenter(int tileX, int tileY) const;

    /**
     * @brief Convertit une tuile flottante vers son centre ecran.
     * @param tileX Coordonnee tuile X flottante.
     * @param tileY Coordonnee tuile Y flottante.
     * @return Position ecran du centre correspondant.
     */
    SDL_FPoint tileToScreenCenterFloat(float tileX, float tileY) const;

    /**
     * @brief Convertit une position ecran vers une position tuile flottante.
     * @param screenX Coordonnee ecran X.
     * @param screenY Coordonnee ecran Y.
     * @return Coordonnees tuile flottantes.
     */
    SDL_FPoint screenToTile(float screenX, float screenY) const;

    /**
     * @brief Convertit une position ecran vers la tuile la plus proche.
     * @param screenX Coordonnee ecran X.
     * @param screenY Coordonnee ecran Y.
     * @return Coordonnees tuile entieres arrondies.
     */
    SDL_Point screenToTileNearest(float screenX, float screenY) const;

    /**
     * @brief Arrondit des coordonnees tuile flottantes.
     * @param tileX Coordonnee tuile X flottante.
     * @param tileY Coordonnee tuile Y flottante.
     * @return Coordonnees tuile entieres.
     */
    SDL_Point roundTile(float tileX, float tileY) const;

    /**
     * @brief Clamp une tuile dans les limites de la map.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     * @return Coordonnees tuile valides dans la map.
     */
    SDL_Point clampTile(int tileX, int tileY) const;

    /**
     * @brief Indique si une tuile est dans la map.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     * @return True si la tuile est dans [0..width) x [0..height).
     */
    bool isInside(int tileX, int tileY) const;

    /**
     * @brief Definit l'etat collision d'une tuile.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     * @param blocked True pour bloquer la tuile, false pour la rendre traversable.
     * @return True si la tuile est valide.
     */
    bool setTileBlocked(int tileX, int tileY, bool blocked);

    /**
     * @brief Retourne l'etat collision d'une tuile.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     * @return True si la tuile est bloquee (ou invalide).
     */
    bool isTileBlocked(int tileX, int tileY) const;

    /**
     * @brief Reinitialise toutes les tuiles en traversable.
     */
    void clearBlockedTiles(void);

    /**
     * @brief Retourne la largeur de map en tuiles.
     * @return Largeur en tuiles.
     */
    int getWidthTiles(void) const;

    /**
     * @brief Retourne la hauteur de map en tuiles.
     * @return Hauteur en tuiles.
     */
    int getHeightTiles(void) const;

    /**
     * @brief Retourne la largeur d'une tuile.
     * @return Largeur en pixels.
     */
    float getTileWidth(void) const;

    /**
     * @brief Retourne la hauteur d'une tuile.
     * @return Hauteur en pixels.
     */
    float getTileHeight(void) const;

    /**
     * @brief Retourne l'origine ecran X.
     * @return Origine X.
     */
    float getOriginX(void) const;

    /**
     * @brief Retourne l'origine ecran Y.
     * @return Origine Y.
     */
    float getOriginY(void) const;
};
