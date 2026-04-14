#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <RC2D/RC2D.h>

#include "game/map/map.h"
#include "game/ships/ship.h"

/**
 * @class VFX
 * @brief Lecteur runtime d'animations VFX exportees par scene-editormap-vfx.
 *
 * Le pipeline charge:
 * - le JSON gameplay du couple (vfx, ship) a partir des dossiers fournis;
 * - la spritesheet JSON (frames + image);
 * - la liste d'instances par direction/state avec timings relatifs;
 * - les champs trainee / cone / couronne a l'arret et le rendu des rejets (aligne editeur).
 *
 * Convention du nom de fichier config:
 * - json config: `<shipFolderPath>/fx-<vfxSlug>_<shipFolderName>.json`
 *   avec `vfxSlug` derive du nom de dossier VFX (`vfx-cannon` -> `cannon`).
 */
class VFX {
public:
    /**
     * @enum ShipDirection
     * @brief Directions diagonales supportees par les pages gameplay.
     */
    enum class ShipDirection {
        DOWN_LEFT = 0,  /**< Bas-gauche. */
        UP_RIGHT = 1,   /**< Haut-droite. */
        UP_LEFT = 2,    /**< Haut-gauche. */
        DOWN_RIGHT = 3  /**< Bas-droite. */
    };

    /**
     * @enum ShipState
     * @brief Etat visuel du navire dans la config gameplay.
     */
    enum class ShipState {
        HEALTHY = 0, /**< Coque intacte. */
        DAMAGED = 1  /**< Coque endommagee. */
    };

    /**
     * @enum TargetingMode
     * @brief Strategie de resolution de page VFX.
     */
    enum class TargetingMode {
        NONE = 0,               /**< Legacy: selection direction/state classique (secteur A force). */
        TARGET_RELATIVE_AB = 1  /**< Resolution par rapport a une cible + variantes A/B. */
    };

    /**
     * @enum TargetRelativeFamily
     * @brief Famille de variante cible resolue pour les pages A/B.
     */
    enum class TargetRelativeFamily {
        A = 0,
        B = 1
    };

    /**
     * @struct DirectionStateResolution
     * @brief Resultat detaille du resolver de page runtime.
     */
    struct DirectionStateResolution {
        int directionStateKey = 0;
        ShipDirection sourceDirection = ShipDirection::DOWN_LEFT;
        ShipDirection resolvedDirection = ShipDirection::DOWN_LEFT;
        ShipState state = ShipState::HEALTHY;
        TargetRelativeFamily family = TargetRelativeFamily::A;
        int targetSectorIndex = 0; /**< [0..3] : [0,90), [90,180), [180,270), [270,360). */
        float relativeAngleDeg = 0.0f; /**< Angle normalise [0, 360). */
        bool usedTargetRelativeMode = false;
        bool hasTargetTile = false;
        bool remappedDirectionWhenStationary = false;
    };

private:
    /**
     * @struct Frame
     * @brief Une frame source dans la spritesheet.
     */
    struct Frame {
        int index = 0;  /**< Index logique de frame (ordre animation). */
        float x = 0.0f; /**< Source X dans la spritesheet (pixels). */
        float y = 0.0f; /**< Source Y dans la spritesheet (pixels). */
        float w = 0.0f; /**< Largeur source (pixels). */
        float h = 0.0f; /**< Hauteur source (pixels). */
    };

    /** Rejet de trainee / couronne : meme payload logique que l'editeur (scene-editormap-vfx). */
    struct TrailPiece {
        uint32_t sourceVfxInstanceId = 0U;
        float anchorShipTileX = 0.0f;
        float anchorShipTileY = 0.0f;
        float bornTimeSeconds = 0.0f;
        float timeRemainingSec = 0.0f;
        float trailLifetimeInitialSec = 0.0f;
        float trailDrawOffsetX = 0.0f;
        float trailDrawOffsetY = 0.0f;
        float anchorShipSpriteCenterOffXPx = 0.0f;
        float anchorShipSpriteCenterOffYPx = 0.0f;
        bool anchorShipSpriteCenterOffValid = false;
        float trailPerpendicularJitterX = 0.0f;
        float trailPerpendicularJitterY = 0.0f;
        float trailRotationJitterDeg = 0.0f;
        float trailInitialPhaseSec = 0.0f;
        float trailAmbientDriftPhase0 = 0.0f;
        float trailAmbientDriftPhase1 = 0.0f;
        uint32_t trailNoiseSeed = 0U;
        float trailWakeDirX = 0.0f;
        float trailWakeDirY = 0.0f;
        float trailSpinOmega0 = 0.0f;
        bool fromIdleRingCrown = false;
    };

    /**
     * @struct Instance
     * @brief Une instance VFX runtime (placement + rendu + timing relatif + trainee exportee).
     */
    struct Instance {
        uint32_t instanceId = 0U;          /**< ID unique de l'instance. */
        float offsetX = 0.0f;              /**< Offset X relatif au centre visuel du ship (px). */
        float offsetY = 0.0f;              /**< Offset Y relatif au centre visuel du ship (px). */
        float rotationDeg = 0.0f;          /**< Rotation appliquee (degres). */
        bool flipHorizontal = false;       /**< Miroir horizontal. */
        bool flipVertical = false;         /**< Miroir vertical. */
        int drawOrder = 0;                 /**< Ordre de rendu relatif au ship. */
        bool visible = true;               /**< Visibilite runtime de l'instance. */
        uint32_t spawnAfterInstanceId = 0U;/**< ID de reference pour le timing relatif (0 = absolu). */
        int spawnAfterDelayMs = 0;         /**< Delai relatif en millisecondes. */
        bool motionSpawnCaptured = false;
        float motionSpawnOffsetX = 0.0f;
        float motionSpawnOffsetY = 0.0f;
        int motionTrailEveryNTiles = 0;
        int motionTrailLifetimeTiles = 12;
        bool motionTrailStrictTilePlacement = true;
        float motionTrailLateralJitterRadius = 0.0f;
        float motionTrailConeOffsetX = 0.0f;
        float motionTrailConeOffsetY = 0.0f;
        float motionTrailConeDirectionOffsetDeg = 0.0f;
        float motionTrailConeHalfAngleDeg = 28.0f;
        int motionTrailConeSpawnCount = 1;
        int motionTrailRotationRandomPercent = 0;
        bool motionTrailIdleRingWhenStationary = false;
        float motionTrailIdleRingRadius = 48.0f;
        int motionTrailIdleSpawnPeriodMs = 600;
        int motionTrailIdleRingPieceCount = 8;
        int motionTrailIdleRingRotationRandomPercent = 0;
        float motionTrailIdleRingPositionJitterRadius = 0.0f;
        float motionTrailDistanceAcc = 0.0f;
        float motionTrailIdleSpawnAccSec = 0.0f;
        int motionTrailIdleRingSalvoPiecesRemaining = 0;
        float motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
    };

    /**
     * @struct DirectionStateData
     * @brief Donnees gameplay pour un triplet (direction, state, secteur cible tir).
     */
    struct DirectionStateData {
        int shipDrawOrder = 0;             /**< Draw order du ship pour ce state. */
        std::vector<Instance> instances;   /**< Toutes les instances VFX de ce state. */
    };

    static constexpr int kDirectionStateCount = 16; /**< Taille max du tableau (4x2x2). */
    /** Nombre d'entrees chargees depuis gameplay.directionStates (8 sans A/B, 16 avec). */
    int loadedDirectionStateCount = kDirectionStateCount;
    TargetingMode targetingMode = TargetingMode::NONE;
    DirectionStateResolution lastDirectionStateResolution{};
    DirectionStateResolution lastLoggedTargetRelativeResolution{};
    bool hasLoggedTargetRelativeResolution = false;

    RC2D_Image spritesheetImage;                      /**< Texture spritesheet chargee. */
    std::vector<Frame> frames;                        /**< Frames source de l'animation. */
    std::array<DirectionStateData, kDirectionStateCount> directionStates;/**< Pages gameplay VFX. */
    float defaultVfxFps;                              /**< FPS par defaut gameplay. */
    int animationTotalDurationMs;                     /**< Duree totale de lecture (ms), 0 = infini. */
    float playbackSeconds;                            /**< Horloge runtime accumulatee (sec). */
    int activeDirectionStateKey;                      /**< Cle active [0..15] selectionnee par le ship. */
    bool loaded;                                      /**< True si toutes les ressources sont pretes. */

    std::string shipFolderPath;       /**< Chemin ship resolu (debug/trace). */
    std::string vfxFolderPath;        /**< Chemin vfx resolu (debug/trace). */
    std::string configJsonPath;       /**< Chemin config gameplay charge. */
    std::string spritesheetJsonPath;  /**< Chemin JSON spritesheet charge. */
    std::string spritesheetImagePath; /**< Chemin image spritesheet chargee. */

    std::vector<TrailPiece> trailPieces{};
    float trailPrevShipTileX = 0.0f;
    float trailPrevShipTileY = 0.0f;
    bool trailPrevShipTileValid = false;
    int trailPrevDirectionStateKey = -1;

    static int runtimeTrailFrameIndex(float defaultFps, int frameCount, float ageSec);
    static void runtimeInitTrailMotionExtras(TrailPiece* piece, float moveDxTiles, float moveDyTiles);
    static void runtimeAppendTrailPieceFromStep(
        const Instance& inst,
        float anchorShipTileX,
        float anchorShipTileY,
        float moveDxTiles,
        float moveDyTiles,
        float timeSec,
        float effectiveZoom,
        const Ship& ship,
        float spawnSpeedTilesPerSec,
        std::vector<TrailPiece>& outPieces);
    void updateTrailsAndIdle(float dtf, float timeSec, const Ship& ship, bool allowSpawn);
    void drawTrailPieces(const Map& map, const Ship& ship, bool drawBehindShip, float timeSec) const;

    /**
     * @brief Construit la cle [0..15] a partir (direction, state, secteur cible tir).
     * @param direction Direction diagonale.
     * @param state Etat coque.
     * @param targetFireSector Secteur cible (0 ou 1), ex. demi-plan pour layering canon.
     * @return Cle indexant directionStates.
     */
    static int directionStateKey(ShipDirection direction, ShipState state, int targetFireSector = 0);
    static int shipDirectionToIndex(ShipDirection direction);
    static ShipDirection shipDirectionFromIndex(int directionIndex);
    static float normalizeDegrees0To360(float deg);
    static int computeRelativeTargetSectorFromTiles(
        const SDL_FPoint& controlledShipTile,
        const SDL_FPoint& targetTile,
        float* outAngleDeg,
        bool* outHasTargetGeometry);
    static ShipDirection remapDirectionWhenStationaryForTargetSector(
        int sectorIndex,
        ShipDirection direction);
    static TargetRelativeFamily resolveTargetRelativeFamily(
        int sectorIndex,
        ShipDirection direction);
    DirectionStateResolution resolveDirectionState(
        const Ship& ship,
        const SDL_FPoint* targetTile) const;
    void logTargetRelativeResolutionIfChanged(const DirectionStateResolution& resolution);
    static const char* targetingModeToString(TargetingMode mode);
    static const char* targetRelativeFamilyToString(TargetRelativeFamily family);
    static const char* shipDirectionToString(ShipDirection direction);

    /**
     * @brief Convertit une string JSON vers ShipDirection.
     * @param value Valeur texte attendue (`down_left`, `up_right`, ...).
     * @return Direction parsee (fallback: DOWN_LEFT).
     */
    static ShipDirection shipDirectionFromString(const char* value);

    /**
     * @brief Convertit une string JSON vers ShipState.
     * @param value Valeur texte attendue (`healthy`, `damaged`, ...).
     * @return Etat parse (fallback: HEALTHY).
     */
    static ShipState shipStateFromString(const char* value);

    /**
     * @brief Convertit la direction preview du ship vers ShipDirection runtime.
     * @param direction Direction preview du ship.
     * @return Direction runtime VFX.
     */
    static ShipDirection shipDirectionFromPreviewDirection(Ship::PreviewDirection direction);
    static Ship::PreviewDirection previewDirectionFromShipDirection(ShipDirection direction);

    /**
     * @brief Convertit l'etat visuel du ship vers ShipState runtime.
     * @param healthVisual Etat visuel du ship.
     * @return Etat runtime VFX.
     */
    static ShipState shipStateFromHealthVisual(Ship::HealthVisual healthVisual);

    /**
     * @brief Synchronise la cle active [direction, state, secteur cible tir] depuis le ship.
     * @param ship Ship runtime courant.
     * @param targetTile Tuile cible optionnelle (requise en mode target-relative).
     */
    void setDirectionStateFromShip(const Ship& ship, const SDL_FPoint* targetTile);

    /**
     * @brief Retourne les donnees du state actuellement actif.
     * @return Reference constante vers la page direction/state active.
     */
    const DirectionStateData& currentDirectionState(void) const;

    /**
     * @brief Recherche une instance par son ID dans une page direction/state.
     * @param directionState Page de recherche.
     * @param instanceId ID recherche.
     * @return Pointeur instance trouvee, sinon nullptr.
     */
    const Instance* findInstanceById(
        const DirectionStateData& directionState,
        uint32_t instanceId) const;

    /**
     * @brief Calcule la phase de cycle (sec) d'une instance avec chainage relatif.
     * @param directionState Page direction/state active.
     * @param instance Instance cible.
     * @param chainDepth Profondeur recursion anti-boucle.
     * @return Phase normalisee dans [0, period).
     */
    float computePhaseSecondsInCycle(
        const DirectionStateData& directionState,
        const Instance& instance,
        int chainDepth) const;

    /**
     * @brief Convertit la phase runtime en index de frame.
     * @param directionState Page direction/state active.
     * @param instance Instance cible.
     * @return Index frame valide dans `frames`.
     */
    int computeFrameIndex(
        const DirectionStateData& directionState,
        const Instance& instance) const;

    /**
     * @brief Retourne la periode d'animation (sec) selon fps et nombre de frames.
     * @return Duree d'un cycle complet.
     */
    float resolvedPlaybackPeriodSeconds(void) const;

public:
    /**
     * @brief Constructeur runtime VFX.
     */
    VFX(void);

    /**
     * @brief Destructeur runtime VFX (appelle unload()).
     */
    ~VFX(void);

    /**
     * @brief Charge un VFX gameplay via dossier ship + dossier vfx.
     *
     * Exemple:
     * - shipFolderPath = `assets/images/ships/ship-elite27`
     * - vfxFolderPath = `assets/images/vfx/vfx-cannon`
     * - config derivee = `assets/images/ships/ship-elite27/fx-cannon_ship-elite27.json`
     *
     * @param shipFolderPath Dossier du ship.
     * @param vfxFolderPath Dossier du VFX.
     * @return True si gameplay + spritesheet sont charges depuis RC2D_STORAGE_TITLE.
     */
    bool loadFromFolders(const char* shipFolderPath, const char* vfxFolderPath);

    /**
     * @brief Decharge toutes les ressources runtime VFX.
     */
    void unload(void);

    /**
     * @brief Indique si le VFX est pret a jouer/dessiner.
     * @return True si texture + frames + etat charge sont valides.
     */
    bool isLoaded(void) const;

    /**
     * @brief Met a jour direction/state active + horloge d'animation.
     * @param dt Delta time en secondes.
     * @param ship Ship runtime de reference.
     * @param targetTile Tuile cible optionnelle pour la resolution target-relative.
     */
    void update(double dt, Ship& ship, const SDL_FPoint* targetTile);

    /**
     * @brief Dessine les instances selon leur relation avec le drawOrder du ship.
     * @param map Map de projection monde->ecran.
     * @param ship Ship de reference (position + anchor runtime).
     * @param drawBehindShip
     * true  => dessine uniquement les instances avec drawOrder < shipDrawOrder;
     * false => dessine uniquement les instances avec drawOrder >= shipDrawOrder.
     */
    void draw(const Map& map, const Ship& ship, bool drawBehindShip) const;
};
