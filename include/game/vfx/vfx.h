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
 * - la liste d'instances par direction/state avec timings relatifs.
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

    /**
     * @struct Instance
     * @brief Une instance VFX runtime (placement + rendu + timing relatif).
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
    };

    /**
     * @struct DirectionStateData
     * @brief Donnees gameplay pour un couple (direction, state).
     */
    struct DirectionStateData {
        int shipDrawOrder = 0;             /**< Draw order du ship pour ce state. */
        std::vector<Instance> instances;   /**< Toutes les instances VFX de ce state. */
    };

    RC2D_Image spritesheetImage;                      /**< Texture spritesheet chargee. */
    std::vector<Frame> frames;                        /**< Frames source de l'animation. */
    std::array<DirectionStateData, 8> directionStates;/**< 4 directions x 2 states. */
    float defaultVfxFps;                              /**< FPS par defaut gameplay. */
    float playbackSeconds;                            /**< Horloge runtime accumulatee (sec). */
    int activeDirectionStateKey;                      /**< Cle active [0..7] selectionnee par le ship. */
    bool loaded;                                      /**< True si toutes les ressources sont pretes. */

    std::string shipFolderPath;       /**< Chemin ship resolu (debug/trace). */
    std::string vfxFolderPath;        /**< Chemin vfx resolu (debug/trace). */
    std::string configJsonPath;       /**< Chemin config gameplay charge. */
    std::string spritesheetJsonPath;  /**< Chemin JSON spritesheet charge. */
    std::string spritesheetImagePath; /**< Chemin image spritesheet chargee. */

    /**
     * @brief Construit la cle [0..7] a partir (direction, state).
     * @param direction Direction diagonale.
     * @param state Etat coque.
     * @return Cle indexant directionStates.
     */
    static int directionStateKey(ShipDirection direction, ShipState state);

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

    /**
     * @brief Convertit l'etat visuel du ship vers ShipState runtime.
     * @param healthVisual Etat visuel du ship.
     * @return Etat runtime VFX.
     */
    static ShipState shipStateFromHealthVisual(Ship::HealthVisual healthVisual);

    /**
     * @brief Synchronise la cle active [direction,state] depuis le ship.
     * @param ship Ship runtime courant.
     */
    void setDirectionStateFromShip(const Ship& ship);

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
     */
    void update(double dt, const Ship& ship);

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
