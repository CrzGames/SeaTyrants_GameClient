#pragma once

#include <array>
#include <vector>

#include <RC2D/RC2D.h>

#include "game/map/map.h"

/**
 * @brief Navire isometrique base sur sprites individuels (1.png..8.png).
 *
 * Le navire supporte:
 * - chargement direct des 8 sprites dans un dossier;
 * - deplacement en coordonnees tile (float) avec cible;
 * - choix de sprite selon direction;
 * - alternance de sprites sur les axes (haut/bas/gauche/droite);
 * - affichage full HP / low HP.
 */
class Ship {
public:
    /**
     * @brief Etat visuel de vie du navire.
     */
    enum class HealthVisual {
        FULL = 0,  /**< Apparence pleine vie. */
        LOW = 1    /**< Apparence faible vie. */
    };

    /**
     * @brief Directions diagonales disponibles dans l'atlas.
     */
    enum class DiagonalDirection {
        DOWN_LEFT = 0,   /**< Bas gauche. */
        UP_RIGHT = 1,    /**< Haut droite. */
        UP_LEFT = 2,     /**< Haut gauche. */
        DOWN_RIGHT = 3   /**< Bas droite. */
    };

    /**
     * @brief Parametres de mouvement/affichage.
     */
    struct Config {
        float speedTilesPerSecond;          /**< Vitesse en tiles par seconde. */
        float cardinalSwapIntervalSeconds;  /**< Intervalle d'alternance sur axes. */
        float scaleX;                       /**< Scale horizontal de rendu. */
        float scaleY;                       /**< Scale vertical de rendu. */
        float drawOffsetX;                  /**< Decalage final en pixels X. */
        float drawOffsetY;                  /**< Decalage final en pixels Y. */
    };

private:
    /**
     * @brief Directions de deplacement quantifiees (repere ecran).
     */
    enum class MoveDirection {
        NONE = 0,    /**< Aucun mouvement. */
        RIGHT = 1,   /**< Vers la droite ecran. */
        DOWN_RIGHT,  /**< Diagonale bas droite ecran. */
        DOWN,        /**< Vers le bas ecran. */
        DOWN_LEFT,   /**< Diagonale bas gauche ecran. */
        LEFT,        /**< Vers la gauche ecran. */
        UP_LEFT,     /**< Diagonale haut gauche ecran. */
        UP,          /**< Vers le haut ecran. */
        UP_RIGHT     /**< Diagonale haut droite ecran. */
    };

    std::array<RC2D_Image, 8> sprites;  /**< Sprites 1..8 (1-based dans les fichiers). */
    bool spritesLoaded;                 /**< True si sprites charges. */
    Config config;                      /**< Parametres runtime du navire. */

    HealthVisual healthVisual;  /**< Etat visuel de vie. */

    SDL_FPoint tilePosition;  /**< Position courante (tile flottante). */
    SDL_FPoint tileTarget;    /**< Position cible (tile flottante). */
    std::vector<SDL_Point> pathTiles;  /**< Chemin A* (sans la tile de depart). */
    std::vector<MoveDirection> pathDirections;  /**< Direction visuelle par segment de path. */
    size_t pathIndex;                  /**< Index du prochain waypoint. */

    bool moving;  /**< True si le navire se deplace vers sa cible. */

    bool directionUsesPair;        /**< True si alternance active (axes). */
    bool directionToggle;          /**< Toggle courant entre directionA/B. */
    double directionTimerSeconds;  /**< Timer interne d'alternance. */
    MoveDirection moveDirection;   /**< Direction quantifiee courante. */

    DiagonalDirection directionA;  /**< Direction principale courante. */
    DiagonalDirection directionB;  /**< Direction secondaire (alternance). */

    /**
     * @brief Convertit une direction diagonale en index [0..3].
     */
    static int directionToIndex(DiagonalDirection direction);

    /**
     * @brief Retourne l'index de sprite [0..7] selon direction + etat de vie.
     */
    int spriteIndexForDirection(DiagonalDirection direction) const;

    /**
     * @brief Retourne le sprite actuellement actif.
     */
    const RC2D_Image* getCurrentSprite(void) const;

    /**
     * @brief Quantifie un vecteur ecran en 8 directions (SeaFight style).
     */
    static MoveDirection quantizeScreenDirection(float deltaScreenX, float deltaScreenY);

    /**
     * @brief Retourne le vecteur unitaire ecran d'une direction quantifiee.
     */
    static SDL_FPoint directionUnitScreen(MoveDirection direction);

    /**
     * @brief Cout d'un pas A* (8 directions) en distance ecran.
     */
    static float aStarStepCost(const Map& map, int deltaTileX, int deltaTileY);

    /**
     * @brief Heuristique A* (distance ecran euclidienne).
     */
    static float aStarHeuristic(const Map& map, int fromTileX, int fromTileY, int toTileX, int toTileY);

    /**
     * @brief Teste si une tile est traversable pour le pathfinding.
     */
    bool isWalkableForPath(const Map& map, int tileX, int tileY, const SDL_Point& startTile, const SDL_Point& goalTile) const;

    /**
     * @brief Construit un chemin A* de start vers goal.
     *
     * Le chemin retourne les waypoints sans la tile de depart.
     */
    bool buildPathAStar(
        const Map& map,
        const SDL_Point& startTile,
        const SDL_Point& goalTile,
        std::vector<SDL_Point>& outPath,
        std::vector<MoveDirection>& outDirections) const;

    /**
     * @brief Met a jour la direction visuelle selon la direction quantifiee.
     */
    void updateDirection(double dt, MoveDirection direction);

    /**
     * @brief Teste si la ligne entre 2 tiles est traversable.
     */
    bool hasLineOfSightTiles(const Map& map, const SDL_Point& fromTile, const SDL_Point& toTile, const SDL_Point& startTile, const SDL_Point& goalTile) const;

    /**
     * @brief Simplifie un chemin brut A* par line-of-sight.
     */
    void simplifyPath(const Map& map, const SDL_Point& startTile, const SDL_Point& goalTile, const std::vector<SDL_Point>& rawPath, std::vector<SDL_Point>& outPath) const;

    /**
     * @brief Dessine un sprite centre sur un point ecran.
     */
    void drawSpriteCentered(const RC2D_Image& sprite, float centerX, float centerY) const;

public:
    /**
     * @brief Construit un navire avec mappings 1..8 par defaut.
     */
    Ship(void);

    /**
     * @brief Libere les ressources du navire.
     */
    ~Ship(void);

    /**
     * @brief Charge les sprites 1.png..8.png depuis un dossier.
     * @param folderPath Dossier contenant les PNG.
     * @param storageKind Storage RC2D.
     * @return True si charge.
     */
    bool loadSpritesFromFolder(const char* folderPath, RC2D_StorageKind storageKind);

    /**
     * @brief Decharge les sprites si presents.
     */
    void unloadSprites(void);

    /**
     * @brief Indique si les sprites sont disponibles.
     */
    bool areSpritesLoaded(void) const;

    /**
     * @brief Choisit le visuel full/low HP.
     */
    void setHealthVisual(HealthVisual health);

    /**
     * @brief Retourne l'etat de vie visuel courant.
     */
    HealthVisual getHealthVisual(void) const;

    /**
     * @brief Regle la vitesse de deplacement (tiles/s).
     */
    void setSpeedTilesPerSecond(float speed);

    /**
     * @brief Regle l'intervalle d'alternance sur axes.
     */
    void setCardinalSwapIntervalSeconds(float intervalSeconds);

    /**
     * @brief Regle l'echelle de rendu.
     */
    void setDrawScale(float scaleX, float scaleY);

    /**
     * @brief Regle un decalage final de rendu en pixels.
     */
    void setDrawOffset(float offsetX, float offsetY);

    /**
     * @brief Fixe la position courante en coordonnees tile flottantes.
     */
    void setPositionTile(float tileX, float tileY);

    /**
     * @brief Fixe la position courante en coordonnees tile entieres.
     */
    void setPositionTileInt(int tileX, int tileY);

    /**
     * @brief Retourne la position tile courante.
     */
    SDL_FPoint getPositionTile(void) const;

    /**
     * @brief Defini la cible de deplacement.
     */
    void setTargetTile(const Map& map, int tileX, int tileY);

    /**
     * @brief Retourne la cible courante.
     */
    SDL_FPoint getTargetTile(void) const;

    /**
     * @brief Indique si le navire est en mouvement.
     */
    bool isMoving(void) const;

    /**
     * @brief Met a jour le mouvement et la direction visuelle.
     */
    void update(double dt, const Map& map);

    /**
     * @brief Dessine le navire a sa position tile actuelle.
     */
    void draw(const Map& map) const;
};
