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
 *
 * Les anchors de rendu (point pivot) sont charges exclusivement depuis
 * le fichier ship_anchor.json du dossier atlas. Sans JSON, l'anchor
 * par defaut est le centre du sprite (0.5, 0.5).
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

    std::array<RC2D_Image, 8> sprites;           /**< Sprites 1..8 charges en memoire. */
    std::array<SDL_FPoint, 8> spriteDrawAnchors;  /**< Anchor normalise [0..1] par sprite (charge depuis JSON). */
    bool spritesLoaded;                            /**< True si les sprites sont charges. */
    uint64_t runtimeShipId;                        /**< Identifiant runtime unique pour le sillage ocean. */

    Config config;             /**< Parametres de deplacement. */
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

    // ---- Helpers direction / sprite ----

    /**
     * @brief Convertit une direction diagonale en index [0..3].
     */
    static int directionToIndex(DiagonalDirection direction);

    /**
     * @brief Retourne l'index sprite [0..7] selon direction + etat de vie.
     */
    int spriteIndexForDirection(DiagonalDirection direction) const;

    /**
     * @brief Retourne l'index sprite actuellement actif.
     */
    int getCurrentSpriteIndex(void) const;

    /**
     * @brief Quantifie un vecteur ecran en 8 directions.
     */
    static MoveDirection quantizeScreenDirection(float deltaScreenX, float deltaScreenY);

    /**
     * @brief Met a jour la direction visuelle du navire.
     */
    void updateDirection(MoveDirection direction);

    // ---- Pathfinding ----

    /**
     * @brief Heuristique A* pour le repere Sea-like.
     */
    static float aStarHeuristic(int fromTileX, int fromTileY, int toTileX, int toTileY);

    /**
     * @brief Teste si une tuile est traversable pour le pathfinding.
     */
    bool isWalkableForPath(const Map& map, int tileX, int tileY, const SDL_Point& startTile, const SDL_Point& goalTile) const;

    /**
     * @brief Construit un path A* (voisinage Sea-like).
     */
    bool buildPathAStar(
        const Map& map,
        const SDL_Point& startTile,
        const SDL_Point& goalTile,
        std::vector<SDL_Point>& outPath,
        std::vector<MoveDirection>& outDirections) const;

    // ---- Rendu interne ----

    /**
     * @brief Dessine un sprite centre sur un point ecran.
     * @param sprite Sprite a dessiner.
     * @param spriteIndex Index du sprite [0..7] pour choisir son anchor dediee.
     * @param centerX Centre ecran X.
     * @param centerY Centre ecran Y.
     */
    void drawSpriteCentered(const RC2D_Image& sprite, int spriteIndex, float centerX, float centerY) const;

    // ---- Chargement JSON ----

    /**
     * @brief Charge les anchors depuis ship_anchor.json du dossier navire.
     *
     * Formats supportes dans le JSON:
     * - racine:   { "anchorX": 0.5, "anchorY": 0.6 }
     * - default:  { "default": { "anchorX": ..., "anchorY": ... } }
     * - default pixels: { "default": { "anchorPixelX": ..., "anchorPixelY": ... } }
     * - par frame: { "frames": { "1": { "anchorX": ..., "anchorY": ... }, ... } }
     *
     * @param folderPath Dossier atlas du navire.
     * @param storageKind Storage RC2D (TITLE/USER).
     * @return True si au moins une ancre a ete chargee.
     */
    bool loadAnchorsFromJson(const char* folderPath, RC2D_StorageKind storageKind);

public:
    /**
     * @brief Constructeur du navire.
     */
    Ship(void);

    /**
     * @brief Destructeur du navire.
     */
    ~Ship(void);

    // ---- Sprites ----

    /**
     * @brief Charge les sprites 1.png..8.png depuis un dossier.
     * @param folderPath Chemin du dossier atlas navire.
     * @param storageKind Storage RC2D (TITLE/USER).
     * @return True si tous les sprites sont charges.
     *
     * Charge aussi les anchors depuis ship_anchor.json si present.
     * Sans JSON, les anchors restent au centre (0.5, 0.5).
     */
    bool loadSpritesFromFolder(const char* folderPath, RC2D_StorageKind storageKind);

    /**
     * @brief Decharge les sprites du navire.
     */
    void unloadSprites(void);

    /**
     * @brief Retourne l'etat de chargement des sprites.
     */
    bool areSpritesLoaded(void) const;

    // ---- Etat visuel ----

    /**
     * @brief Definit l'apparence de vie du navire (FULL ou LOW).
     */
    void setHealthVisual(HealthVisual health);

    /**
     * @brief Retourne l'apparence de vie courante.
     */
    HealthVisual getHealthVisual(void) const;

    // ---- Vitesse ----

    /**
     * @brief Definit la vitesse de deplacement en tuiles/s.
     */
    void setSpeedTilesPerSecond(float speed);

    /**
     * @brief Retourne la vitesse de deplacement en tuiles/s.
     */
    float getSpeedTilesPerSecond(void) const;

    // ---- Anchor par sprite (utilise par l'editeur) ----

    /**
     * @brief Definit l'anchor d'un sprite specifique.
     * @param spriteIndex Index sprite [0..7].
     * @param anchorX Anchor X normalise [0..1].
     * @param anchorY Anchor Y normalise [0..1].
     */
    void setDrawAnchorForSprite(int spriteIndex, float anchorX, float anchorY);

    // ---- Position / Navigation ----

    /**
     * @brief Force la position du navire en tuile flottante.
     */
    void setPositionTile(float tileX, float tileY);

    /**
     * @brief Force la position du navire en tuile entiere.
     */
    void setPositionTileInt(int tileX, int tileY);

    /**
     * @brief Retourne la position courante du navire.
     */
    SDL_FPoint getPositionTile(void) const;

    /**
     * @brief Lance un deplacement vers une tuile cible via A*.
     */
    void moveToTile(const Map& map, int tileX, int tileY);

    /**
     * @brief Retourne la cible courante du navire.
     */
    SDL_FPoint getTargetTile(void) const;

    /**
     * @brief Indique si le navire est en mouvement.
     */
    bool isMoving(void) const;

    // ---- Update / Draw ----

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
