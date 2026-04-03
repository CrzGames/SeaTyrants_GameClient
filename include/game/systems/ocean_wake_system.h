#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "game/map/map.h"
#include "game/shaders/ocean-shader.h"

/**
 * @brief Systeme global de generation de sillages ocean.
 *
 * Ce module centralise:
 * - l'historique des points de trainee;
 * - le suivi des positions ecran precedentes par navire;
 * - la conversion finale vers les WakePoint du shader ocean.
 *
 * Il est concu pour etre alimente par tous les navires visibles
 * (joueur local, joueurs distants, NPC, monstres marins, etc.).
 */
class OceanWakeSystem {
private:
    struct WakeStamp {
        float tileX;       /**< Position tuile X du stamp. */
        float tileY;       /**< Position tuile Y du stamp. */
        float dirX;        /**< Direction ecran X normalisee. */
        float dirY;        /**< Direction ecran Y normalisee. */
        float ageSeconds;  /**< Age courant du stamp. */
    };

    struct ShipTracker {
        uint64_t shipId;       /**< Identifiant unique du navire. */
        SDL_FPoint lastScreen; /**< Derniere position ecran connue. */
        bool initialized;      /**< True si lastScreen a deja ete initialise. */
        bool seenThisFrame;    /**< True si le navire a ete soumis ce frame. */
    };

    std::vector<WakeStamp> wakeStamps;   /**< Historique global de trainee. */
    std::vector<ShipTracker> trackers;   /**< Etat de suivi par navire. */
    float wakeStampSpacingPx;            /**< Espacement minimal entre stamps. */
    float wakeLifetimeSeconds;           /**< Duree de vie d'un stamp. */

    /**
     * @brief Retourne (ou cree) le tracker associe a un navire.
     * @param shipId Identifiant du navire.
     * @return Reference mutable vers le tracker.
     */
    ShipTracker& getOrCreateTracker(uint64_t shipId);

public:
    /**
     * @brief Constructeur.
     */
    OceanWakeSystem(void);

    /**
     * @brief Reinitialise completement le systeme.
     */
    void reset(void);

    /**
     * @brief Debut de frame: vieillit et nettoie les stamps.
     * @param dt Delta time en secondes.
     */
    void beginFrame(double dt);

    /**
     * @brief Soumet un navire visible pour alimenter sa trainee.
     *
     * @param shipId Identifiant stable du navire.
     * @param map Map utilisee pour conversion tuile -> ecran.
     * @param tilePosition Position tuile courante du navire.
     * @param moving True si le navire est en mouvement.
     */
    void submitShipSample(
        uint64_t shipId,
        const Map& map,
        const SDL_FPoint& tilePosition,
        bool moving);

    /**
     * @brief Fin de frame: convertit et push les points vers le shader.
     *
     * @param map Map pour conversion des stamps.
     * @param visibleRect Rectangle visible ocean courant.
     * @param oceanShader Shader ocean cible.
     */
    void endFrame(
        const Map& map,
        const SDL_FRect& visibleRect,
        OceanShader& oceanShader);

    /**
     * @brief Definit l'espacement des stamps (en pixels ecran).
     * @param spacingPx Espacement minimal.
     */
    void setWakeStampSpacingPx(float spacingPx);

    /**
     * @brief Definit la duree de vie d'un stamp.
     * @param lifetimeSeconds Duree en secondes.
     */
    void setWakeLifetimeSeconds(float lifetimeSeconds);
};
