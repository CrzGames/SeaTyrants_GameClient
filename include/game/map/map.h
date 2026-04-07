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

    float tileWidth;   /**< Largeur d'une tuile isometrique a zoom 100%. */
    float tileHeight;  /**< Hauteur d'une tuile isometrique a zoom 100%. */

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
    SDL_FRect rect; /**< Rectangle de rendu monde (map) en coordonnees logiques. */

    static constexpr float MAP_TOP_UI_MARGIN_PX = 35.0f;    /**< Marge reservee en haut pour la GUI. */
    static constexpr float MAP_BOTTOM_UI_MARGIN_PX = 70.0f; /**< Marge reservee en bas pour la GUI. */

    // -----------------------------------------------------------------------------
    // Grille de secteurs "metier" visible par le joueur
    // -----------------------------------------------------------------------------
    // Axe X des secteurs : 00 -> 59 (de gauche a droite a l'ecran)
    // Axe Y des secteurs : AA -> CH (de haut en bas a l'ecran)
    //
    // Attention:
    // ces coordonnees secteurs NE sont PAS les coordonnees tuiles isometriques.
    // Elles representent une grille logique 60x60, ensuite projetee dans la map iso.
    // -----------------------------------------------------------------------------
    static constexpr int NUM_SECTORS_X = 60; /**< Nombre de secteurs horizontaux : 00..59. */
    static constexpr int NUM_SECTORS_Y = 60; /**< Nombre de secteurs verticaux   : AA..CH. */

    // -----------------------------------------------------------------------------
    // Espacement entre 2 secteurs dans la grille de tuiles isometriques
    // -----------------------------------------------------------------------------
    // SECTOR_STEP indique de combien de tuiles on avance dans la map technique
    // lorsqu'on se deplace d'un secteur:
    // - vers la droite  (secteurX + 1)
    // - vers le bas     (secteurY + 1)
    //
    // Plus cette valeur est grande, plus les secteurs sont eloignes entre eux
    // dans la map technique.
    // -----------------------------------------------------------------------------
    static constexpr int SECTOR_STEP = 6; /**< Distance en tuiles entre deux secteurs voisins. */

    // -----------------------------------------------------------------------------
    // Marge technique de securite autour de la zone secteurs
    // -----------------------------------------------------------------------------
    // Cette bordure n'est pas une zone "metier" jouable en termes de coordonnees
    // 00..59 / AA..CH.
    //
    // Elle sert uniquement a:
    // - eviter les coordonnees negatives en projection isometrique,
    // - laisser de l'air visuellement autour de la grille,
    // - empecher les problemes de bord d'ecran / camera / clic.
    //
    // En pratique, la map technique est plus grande que la zone secteurs.
    // -----------------------------------------------------------------------------
    static constexpr int SECTOR_PAD = 40; /**< Bordure technique autour de la zone secteurs. */

    // -----------------------------------------------------------------------------
    // Point d'ancrage de la grille secteurs dans la map technique
    // -----------------------------------------------------------------------------
    // Le secteur 00-AA ne correspond PAS a la tuile (0,0).
    //
    // On place volontairement la grille secteurs a l'interieur de la map technique,
    // avec une marge (SECTOR_PAD), afin que toute la projection isometrique tienne
    // proprement dans le monde sans sortir en negatif.
    //
    // SECTOR_BASE_X : point de depart X de la grille secteurs.
    // SECTOR_BASE_Y : point de depart Y de la grille secteurs.
    //
    // Pourquoi SECTOR_BASE_Y ajoute (NUM_SECTORS_X - 1) * SECTOR_STEP ?
    // Parce que lorsqu'on avance vers la droite dans les secteurs,
    // la coordonnee tileY diminue dans notre repere iso.
    // On remonte donc artificiellement le point de depart en Y pour que la premiere
    // ligne de secteurs reste dans les bornes positives de la map.
    // -----------------------------------------------------------------------------
    static constexpr int SECTOR_BASE_X = SECTOR_PAD; /**< Origine X de la grille secteurs dans la map technique. */
    static constexpr int SECTOR_BASE_Y = SECTOR_PAD + ((NUM_SECTORS_X - 1) * SECTOR_STEP); /**< Origine Y de la grille secteurs dans la map technique. */

    // -----------------------------------------------------------------------------
    // Taille totale de la map technique en tuiles
    // -----------------------------------------------------------------------------
    // La map ne contient pas seulement la zone secteurs utile,
    // elle contient aussi la marge technique tout autour.
    //
    // Formule:
    // - SECTOR_PAD * 2 : marge gauche + marge droite (et equivalent vertical)
    // - (NUM_SECTORS_X + NUM_SECTORS_Y - 1) * SECTOR_STEP : taille necessaire
    //   pour contenir toute la projection diagonale de la grille secteurs
    // - +1 : pour inclure la derniere tuile extreme
    //
    // Cette taille garantit que tous les secteurs de 00-AA a 59-CH
    // peuvent etre convertis en tuiles sans sortir de la map.
    // -----------------------------------------------------------------------------
    static constexpr int WORLD_SIZE_TILES =
        (SECTOR_PAD * 2) +
        ((NUM_SECTORS_X + NUM_SECTORS_Y - 1) * SECTOR_STEP) +
        1; /**< Taille totale de la map technique carree, en tuiles. */

    /**
     * @brief Construit une map par defaut (64x64, tuiles 48x32).
     */
    Map(void);

    /**
     * @brief Destructeur.
     */
    ~Map(void);

    /**
    * @brief Convertit une coordonnee secteur (00-59 / AA-CH) en coordonnee tuile.
    */
    SDL_Point sectorToTile(int sectorX, int sectorY) const;

    /**
    * @brief Convertit une tuile vers une coordonnee secteur flottante.
    */
    SDL_FPoint tileToSectorFloat(float tileX, float tileY) const;

    /**
    * @brief Convertit une tuile vers une coordonnee secteur entiere la plus proche (arrondie).
    */
    SDL_Point tileToSectorNearest(float tileX, float tileY) const;

    /**
    * @brief Retourne true si le secteur est dans [0..59] x [0..59].
    */
    bool isInsideSector(int sectorX, int sectorY) const;

    /**
    * @brief Clamp un secteur dans les bornes [0..59] x [0..59].
    */
    SDL_Point clampSector(int sectorX, int sectorY) const;

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
     * @brief Met a jour le rectangle de rendu de la map.
     * @param gameScreenRect Rectangle de base du game screen.
     */
    void updateMapRect(const SDL_FRect& gameScreenRect);

    /**
     * @brief Met a jour l'etat runtime de la map.
     *
     * Pour l'instant:
     * - synchronise le rectangle map a partir du game screen logique.
     */
    void update(void);

    /**
     * @brief Centre une tuile specifique dans un rectangle de rendu.
     * @param tileX Coordonnee tuile X a centrer.
     * @param tileY Coordonnee tuile Y a centrer.
     * @param rect Rectangle cible en coordonnees ecran.
     */
    void centerOnTileInRect(float tileX, float tileY, const SDL_FRect& targetRect);

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
