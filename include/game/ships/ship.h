#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <RC2D/RC2D.h>

#include "game/map/map.h"

/**
 * @brief Navire isometrique base sur 8 sprites (1.png..8.png).
 *
 * Regles visuelles:
 * - diagonales pures: 1 sprite fixe;
 * - axes cardinaux: alternance de 2 sprites a chaque sous-segment du path.
 */
class Ship {
public:
    /**
     * @brief Etat visuel de coque (full HP ou low HP).
     */
    enum class HealthVisual {
        FULL = 0,
        LOW = 1
    };

    /**
     * @brief Configuration runtime du navire.
     */
    struct Config {
        float speedTilesPerSecond; /**< Vitesse de deplacement en tuiles/s. */
        float scaleX;              /**< Echelle de rendu X. */
        float scaleY;              /**< Echelle de rendu Y. */
        float drawOffsetX;         /**< Decalage rendu X en pixels. */
        float drawOffsetY;         /**< Decalage rendu Y en pixels. */
        float drawAnchorX;         /**< Anchor normalise X [0..1] dans le sprite. */
        float drawAnchorY;         /**< Anchor normalise Y [0..1] dans le sprite. */
    };

private:
    /**
     * @brief Directions diagonales disponibles dans les sprites.
     */
    enum class DiagonalDirection {
        DOWN_LEFT = 0,
        UP_RIGHT = 1,
        UP_LEFT = 2,
        DOWN_RIGHT = 3
    };

    /**
     * @brief Directions de mouvement quantifiees en repere ecran.
     */
    enum class MoveDirection {
        NONE = 0,
        RIGHT,
        DOWN_RIGHT,
        DOWN,
        DOWN_LEFT,
        LEFT,
        UP_LEFT,
        UP,
        UP_RIGHT
    };

    std::array<RC2D_Image, 8> sprites; /**< Sprites 1..8 charges en memoire. */
    std::array<SDL_FPoint, 8> spriteDrawAnchors; /**< Anchor normalise [0..1] par sprite. */
    bool spritesLoaded;                 /**< True si les sprites sont charges. */
    uint64_t runtimeShipId;             /**< Identifiant runtime unique pour le sillage ocean. */

    Config config;             /**< Parametres de deplacement/rendu. */
    HealthVisual healthVisual; /**< Etat visuel de coque. */

    SDL_FPoint tilePosition; /**< Position courante en coordonnees tuile flottantes. */
    SDL_FPoint tileTarget;   /**< Cible courante en coordonnees tuile flottantes. */

    std::vector<SDL_Point> pathTiles;          /**< Waypoints du path courant. */
    std::vector<MoveDirection> pathDirections; /**< Direction visuelle par waypoint. */
    size_t pathIndex;                          /**< Index du waypoint courant. */

    bool moving;                 /**< True si un deplacement est actif. */
    bool directionUsesPair;      /**< True si la direction utilise une alternance A/B. */
    bool directionToggle;        /**< Bascule d'alternance entre A et B. */
    MoveDirection moveDirection; /**< Direction quantifiee courante. */

    DiagonalDirection directionA; /**< Direction visuelle principale. */
    DiagonalDirection directionB; /**< Direction visuelle secondaire. */

    /**
     * @brief Convertit une direction diagonale en index [0..3].
     * @param direction Direction diagonale.
     * @return Index direction [0..3].
     */
    static int directionToIndex(DiagonalDirection direction);

    /**
     * @brief Retourne l'index sprite [0..7] selon direction + etat de vie.
     * @param direction Direction diagonale demandee.
     * @return Index sprite [0..7], ou -1 si invalide.
     */
    int spriteIndexForDirection(DiagonalDirection direction) const;

    /**
     * @brief Retourne l'index sprite actuellement actif.
     * @return Index sprite [0..7], ou -1 si indisponible.
     */
    int getCurrentSpriteIndex(void) const;

    /**
     * @brief Quantifie un vecteur ecran en 8 directions.
     * @param deltaScreenX Delta ecran X.
     * @param deltaScreenY Delta ecran Y.
     * @return Direction quantifiee.
     */
    static MoveDirection quantizeScreenDirection(float deltaScreenX, float deltaScreenY);

    /**
     * @brief Heuristique A* pour le repere Sea-like.
     * @param fromTileX Tuile depart X.
     * @param fromTileY Tuile depart Y.
     * @param toTileX Tuile cible X.
     * @param toTileY Tuile cible Y.
     * @return Cout heuristique.
     */
    static float aStarHeuristic(int fromTileX, int fromTileY, int toTileX, int toTileY);

    /**
     * @brief Teste si une tuile est traversable pour le pathfinding.
     * @param map Map de navigation.
     * @param tileX Tuile testee X.
     * @param tileY Tuile testee Y.
     * @param startTile Tuile depart.
     * @param goalTile Tuile cible.
     * @return True si traversable.
     */
    bool isWalkableForPath(const Map& map, int tileX, int tileY, const SDL_Point& startTile, const SDL_Point& goalTile) const;

    /**
     * @brief Construit un path A* (voisinage Sea-like).
     * @param map Map de navigation.
     * @param startTile Tuile depart.
     * @param goalTile Tuile cible.
     * @param outPath Path de sortie (sans tuile depart).
     * @param outDirections Direction visuelle de sortie par segment.
     * @return True si un path valide a ete trouve.
     */
    bool buildPathAStar(
        const Map& map,
        const SDL_Point& startTile,
        const SDL_Point& goalTile,
        std::vector<SDL_Point>& outPath,
        std::vector<MoveDirection>& outDirections) const;

    /**
     * @brief Met a jour la direction visuelle du navire.
     * @param direction Direction quantifiee a appliquer.
     */
    void updateDirection(MoveDirection direction);

    /**
     * @brief Dessine un sprite centre sur un point ecran.
     * @param sprite Sprite a dessiner.
     * @param spriteIndex Index du sprite [0..7] pour choisir son anchor dediee.
     * @param centerX Centre ecran X.
     * @param centerY Centre ecran Y.
     */
    void drawSpriteCentered(const RC2D_Image& sprite, int spriteIndex, float centerX, float centerY) const;

    /**
     * @brief Charge l'ancre de rendu depuis le fichier JSON du dossier navire.
     *
     * Formats supportes:
     * - racine: { "anchorX": ..., "anchorY": ... }
     * - bloc default: { "default": { "anchorX": ..., "anchorY": ... } }
     * - bloc default en pixels: { "default": { "anchorPixelX": ..., "anchorPixelY": ... } }
     * - bloc frames["1.png"]..["8.png"] ou frames["1"]..["8"]
     *   avec anchorX/anchorY ou anchorPixelX/anchorPixelY.
     *
     * - ship_anchor.json
     *
     * @param folderPath Dossier atlas du navire.
     * @param storageKind Storage RC2D (TITLE/USER).
     * @return True si une ancre a ete chargee depuis le JSON.
     */
    bool loadDrawAnchorFromJson(const char* folderPath, RC2D_StorageKind storageKind);

public:
    /**
     * @brief Constructeur du navire.
     */
    Ship(void);

    /**
     * @brief Destructeur du navire.
     */
    ~Ship(void);

    /**
     * @brief Charge les sprites 1.png..8.png depuis un dossier.
     * @param folderPath Chemin du dossier atlas navire.
     * @param storageKind Storage RC2D (TITLE/USER).
     * @return True si tous les sprites sont charges.
     *
     * Note:
     * Cette methode essaye aussi de lire la configuration JSON d'ancre
     * pour surcharger l'ancre de rendu. En cas d'absence/erreur, l'ancre par
     * defaut reste active.
     */
    bool loadSpritesFromFolder(const char* folderPath, RC2D_StorageKind storageKind);

    /**
     * @brief Decharge les sprites du navire.
     */
    void unloadSprites(void);

    /**
     * @brief Retourne l'etat de chargement des sprites.
     * @return True si les sprites sont charges.
     */
    bool areSpritesLoaded(void) const;

    /**
     * @brief Definit l'apparence de vie du navire.
     * @param health Etat visuel FULL ou LOW.
     */
    void setHealthVisual(HealthVisual health);

    /**
     * @brief Retourne l'apparence de vie courante.
     * @return Etat visuel FULL ou LOW.
     */
    HealthVisual getHealthVisual(void) const;

    /**
     * @brief Definit la vitesse de deplacement en tuiles/s.
     * @param speed Vitesse en tuiles/s.
     */
    void setSpeedTilesPerSecond(float speed);

    /**
     * @brief Retourne la vitesse de deplacement en tuiles/s.
     * @return Vitesse en tuiles/s.
     */
    float getSpeedTilesPerSecond(void) const;

    /**
     * @brief Definit l'echelle de rendu du navire.
     * @param scaleX Echelle X.
     * @param scaleY Echelle Y.
     */
    void setDrawScale(float scaleX, float scaleY);

    /**
     * @brief Definit un decalage final de rendu.
     * @param offsetX Decalage X en pixels.
     * @param offsetY Decalage Y en pixels.
     */
    void setDrawOffset(float offsetX, float offsetY);

    /**
     * @brief Definit l'anchor normalise de rendu dans le sprite.
     * @param anchorX Anchor X normalise [0..1].
     * @param anchorY Anchor Y normalise [0..1].
     */
    void setDrawAnchor(float anchorX, float anchorY);

    /**
     * @brief Definit l'anchor d'un sprite specifique.
     * @param spriteIndex Index sprite [0..7].
     * @param anchorX Anchor X normalise [0..1].
     * @param anchorY Anchor Y normalise [0..1].
     */
    void setDrawAnchorForSprite(int spriteIndex, float anchorX, float anchorY);

    /**
     * @brief Force la position du navire en tuile flottante.
     * @param tileX Coordonnee tuile X flottante.
     * @param tileY Coordonnee tuile Y flottante.
     */
    void setPositionTile(float tileX, float tileY);

    /**
     * @brief Force la position du navire en tuile entiere.
     * @param tileX Coordonnee tuile X.
     * @param tileY Coordonnee tuile Y.
     */
    void setPositionTileInt(int tileX, int tileY);

    /**
     * @brief Retourne la position courante du navire.
     * @return Position en coordonnees tuile flottantes.
     */
    SDL_FPoint getPositionTile(void) const;

    /**
     * @brief Lance un deplacement vers une tuile cible.
     * @param map Map de navigation.
     * @param tileX Tuile cible X.
     * @param tileY Tuile cible Y.
     */
    void moveToTile(const Map& map, int tileX, int tileY);

    /**
     * @brief Retourne la cible courante du navire.
     * @return Cible en coordonnees tuile flottantes.
     */
    SDL_FPoint getTargetTile(void) const;

    /**
     * @brief Indique si le navire est en mouvement.
     * @return True si un deplacement est actif.
     */
    bool isMoving(void) const;

    /**
     * @brief Met a jour le deplacement du navire.
     * @param dt Delta time en secondes.
     * @param map Map de navigation.
     */
    void update(double dt, const Map& map);

    /**
     * @brief Dessine le navire a sa position courante.
     * @param map Map utilisee pour la projection ecran.
     */
    void draw(const Map& map) const;
};
