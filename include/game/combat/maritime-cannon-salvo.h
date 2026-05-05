#pragma once

#include "game/ships/ship.h"
#include "game/vfx/vfx-classic.h"
#include "game/vfx/vfx-ship.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

class GameState;
struct GameplayVfxShipSlot;

/**
 * @brief Systeme runtime de salve de canons maritimes.
 *
 * - @ref fireSalvo pousse les VFX coque dans @ref GameState::vfxShips et instancie les boulets
 *   (spritesheets @ref VFXClassic) en interne.
 * - @ref update simule trajectoires et retire les entrees terminees.
 * - Rendu: la scene dessine @ref GameState::vfxShips avant/apres les navires; un seul
 *   @ref drawSalvoProjectiles apres les joueurs pour les boulets.
 */
class MaritimeCannonSalvoSystem {
public:
    static constexpr int kProjectileTrajectoryDistanceBandCount = 4;
    static constexpr int kProjectileTrajectoryAngleSectorCount = 6;

    /**
     * @brief Reglages runtime d'une trajectoire de boulets pour un couple distance / angle.
     *
     * Les distances sont regroupees en 4 bandes:
     * - 0: 0..10 tuiles
     * - 1: 10..20 tuiles
     * - 2: 20..40 tuiles
     * - 3: 40+ tuiles
     *
     * Les angles sont regles par secteurs autour du navire attaquant, avec
     * 0 deg a gauche et 90 deg vers le haut de l'ecran.
     */
    struct ProjectileTrajectoryTuning {
        /** Courbure Bezier en fraction de la corde attaquant -> cible. */
        float bezierBowChordFraction = 0.28f;
        /** Cote de l'arc: -1 = gauche, +1 = droite. */
        float arcSide = 1.0f;
        /** Lobe vertical ecran en pixels a zoom 100%, applique au milieu du vol. */
        float screenLobPixels = 72.0f;
        /** Multiplicateur de la fenetre temporelle entre boulets d'une meme salve. */
        float launchStaggerScale = 1.0f;
        /** Decalage fixe perpendiculaire au tir pour choisir le cote de sortie du navire. */
        float launchSideOffsetTiles = 0.0f;
        /** Decalage fixe dans l'axe du tir pour sortir plus devant / derriere le centre du navire. */
        float launchForwardOffsetTiles = 0.0f;
        /** Demi-largeur en tuiles de l'eventail de depart. */
        float launchFanHalfTiles = 0.72f;
        /** Bruit aleatoire en tuiles ajoute a l'eventail de depart. */
        float launchNoiseTiles = 0.16f;
        /** Multiplicateur des offsets d'impact autour de la cible. */
        float impactSpreadScale = 0.75f;
        /** Relachement lateral de la file pendant le vol, en tuiles. */
        float flightLaneJitterTiles = 0.18f;
        /** Multiplicateur de duree de vol de chaque boulet. */
        float flightDurationScale = 1.0f;
        /** Intensite du rythme vite-lent-vite sur la progression 0..1 du vol. */
        float flightEaseStrength = 0.45f;
    };

    /**
     * @struct SalvoEntry
     * @brief Definition runtime d'une famille de salve maritime.
     *
     * Une entree de salve decrit tout ce qui est necessaire pour instancier:
     * - les boulets en vol (spritesheet projectile) ;
     * - l'effet ship joue sur l'attaquant au depart ;
     * - les effets ship joues sur la cible a l'impact.
     *
     * Le contenu est volontairement purement descriptif: la scene, le serveur
     * ou une base de donnees peuvent remplir cette structure puis l'enregistrer
     * dans le systeme via @ref addSalvoEntry.
     */
    struct SalvoEntry {
        struct EndActionVfxShipFolderForTarget {
            const char* vfxShipFolder = nullptr;
            std::uint32_t delayAfterImpactMs = 0U;
        };

        struct IlluminatedProjectileSettings {
            /**
             * @brief Active la variante de rendu "boulets illumines" pour cette salve.
             *
             * - `false`: rendu projectile standard.
             * - `true` + `glowConfigJsonPath` vide/null: utilise la config glow runtime deja active
             *   (mode live, pratique pour les outils).
             * - `true` + `glowConfigJsonPath` renseigne: charge une config glow dediee au chemin JSON.
             */
            bool enabled = false;
            /**
             * @brief Chemin `assets/...` vers une config glow dediee a cette salve.
             *
             * Exemple: `assets/data/illuminated-projectile-glow-gold.json`
             *
             * Si nul/vide, la salve reutilise la config glow illuminee runtime deja active.
             */
            const char* glowConfigJsonPath = nullptr;
        };

        std::uint32_t entryId = 0U; /**< Identifiant stable de la famille de salve. */
        std::string debugName{};    /**< Nom debug lisible pour logs et outils. */
        /**
         * @brief Dossier du VFX projectile de la salve.
         *
         * Ce chemin est charge par @ref VFXClassic::loadFromFolder et pilote
         * le rendu des boulets pendant leur vol entre le navire attaquant
         * et le navire cible.
         *
         * Exemple: `assets/images/vfxclassic/vfx-bouletillu`
         */
        const char* projectileVfxClassicFolder = nullptr;
        /** Reglages illumines de cette salve. */
        IlluminatedProjectileSettings illuminatedProjectile{};
        /**
         * @brief Dossier du VFX ship joue sur l'attaquant au depart de la salve.
         *
         * - `nullptr` ou chaine vide: aucun VFX ship de depart.
         * - sinon, la valeur est transmise a @ref VFXShip::loadFromFolders avec:
         *   - `shipFolderPath = dossier sprites du navire attaquant`
         *   - `vfxFolderPath = startActionVfxShipFolderForAttacker`
         *
         * Le JSON ship/runtime attendu reste celui derive du couple
         * `(dossier ship attaquant, dossier VFX)`.
         */
        const char* startActionVfxShipFolderForAttacker = nullptr;
        /**
         * @brief Liste des dossiers VFX ship joues sur la cible a l'impact.
         *
         * Chaque entree non vide est chargee via @ref VFXShip::loadFromFolders avec:
         * - `shipFolderPath = dossier sprites du navire cible`
         * - `vfxFolderPath = dossier VFX de cette entree`
         *
         * Cela permet d'empiler plusieurs layers d'impact pour une meme salve,
         * avec un delai optionnel par layer.
         */
        std::vector<EndActionVfxShipFolderForTarget> endActionVfxShipFoldersForTarget{};
    };

    /** Preset client pour le premier type (chemins assets — pourront etre remplaces par des lignes BDD). */

    MaritimeCannonSalvoSystem(void);

    /**
     * @brief Vide le catalogue runtime des familles de salves disponibles.
     */
    void clearSalvoEntries(void);

    /**
     * @brief Ajoute ou remplace une famille de salve dans le catalogue runtime.
     *
     * Si une entree avec le meme @ref SalvoEntry::entryId existe deja,
     * elle est remplacee par la nouvelle definition.
     *
     * @param entry Definition runtime de la salve a enregistrer.
     */
    void addSalvoEntry(const SalvoEntry& entry);

    /**
     * @brief Recherche une famille de salve enregistree par identifiant.
     * @param entryId Identifiant stable recherche.
     * @return Pointeur sur l'entree si trouvee, sinon `nullptr`.
     */
    const SalvoEntry* findSalvoEntryById(std::uint32_t entryId) const;

    /**
     * @brief Declenche une salve depuis @p shipAttack vers @p shipTarget.
     *
     * Le nombre de boulets est resolu via les settings HUD courants
     * (ex: Low=1, Normal=5, High=10).
     *
     * @param shipAttack Navire attaquant.
     * @param shipTarget Navire cible (suivi meme s'il se deplace).
     * @param entry Definition runtime de la famille de salve a jouer.
     */
    void fireSalvo(Ship& shipAttack, Ship& shipTarget, const SalvoEntry& entry);

    /**
     * @brief Declenche une salve avec un nombre de boulets explicite et une destination VFX optionnelle.
     *
     * @param localVfxShipSlots Si non nul, les VFX ship de depart/impact sont stockes dans cette liste.
     *                          Sinon ils alimentent @ref GameState::vfxShips comme en gameplay.
     */
    void fireSalvo(
        Ship& shipAttack,
        Ship& shipTarget,
        const SalvoEntry& entry,
        int ballCount,
        std::vector<GameplayVfxShipSlot>* localVfxShipSlots);

    /**
     * @brief Met a jour les salves (projectiles + action VFX).
     */
    void update(double dt);

    /**
     * @brief Dessine uniquement les boulets (spritesheets internes). Appeler apres les navires.
     */
    void drawSalvoProjectiles(void) const;

    /**
     * @brief Reinitialise le suivi de salve et decharge les @ref VFXClassic internes.
     *
     * Les @ref GameState::vfxShips sont geres par @ref GameState::clearGameplayVfx.
     */
    void clear(void);

    /**
     * @brief Retourne les valeurs par defaut de trajectoire pour une bande distance / secteur angle.
     */
    static ProjectileTrajectoryTuning getDefaultProjectileTrajectoryTuning(
        int distanceBandIndex,
        int angleSectorIndex);

    /**
     * @brief Retourne la config runtime courante pour une bande distance / secteur angle.
     */
    static ProjectileTrajectoryTuning getProjectileTrajectoryTuning(
        int distanceBandIndex,
        int angleSectorIndex);

    /**
     * @brief Remplace la config runtime pour une bande distance / secteur angle.
     */
    static void setProjectileTrajectoryTuning(
        int distanceBandIndex,
        int angleSectorIndex,
        const ProjectileTrajectoryTuning& tuning);

    /**
     * @brief Charge les configs de trajectoire depuis le fichier gameplay exporte.
     * @return True si un fichier valide a ete charge; false si les defaults restent actifs.
     */
    static bool loadProjectileTrajectoryTuningsFromFile(void);

    /**
     * @brief Exporte les configs de trajectoire courantes vers le fichier gameplay.
     * @return True si l'ecriture a reussi.
     */
    static bool exportProjectileTrajectoryTuningsToFile(void);

    /**
     * @brief Chemin gameplay du fichier de config exporte / charge.
     */
    static const char* getProjectileTrajectoryConfigPath(void);

    /**
     * @brief Reinitialise toutes les configs de trajectoire distance / angle.
     */
    static void resetProjectileTrajectoryTunings(void);

    /**
     * @brief Force le prochain chargement one-shot du fichier trajectoire gameplay.
     */
    static void invalidateProjectileTrajectoryConfigFileLoadState(void);

    /**
     * @brief Invalide le cache runtime des JSON glow dedies par salve.
     *
     * - `nullptr` ou vide: vide tout le cache.
     * - sinon: invalide uniquement ce chemin `assets/...`.
     */
    static void invalidateIlluminatedProjectileGlowConfigFileCache(const char* glowConfigJsonPath);

private:
    struct Cannonball {
        struct ImpactVfxRequest {
            std::string folderPath{};
            std::uint32_t delayAfterImpactMs = 0U;
        };

        std::size_t projectileVfxIndex = 0U; /**< Index dans @ref salvoProjectileVfx. */
        Ship* target;
        Ship* attacker;
        std::vector<ImpactVfxRequest> endImpactVfxRequests;
        std::vector<GameplayVfxShipSlot>* vfxShipSlots = nullptr;
        /** Courbure Bezier (fraction de la corde) capturee au tir selon distance. */
        float bezierBowChordFraction = 0.0f;
        /** Cote de la normale de l'arc capture au tir (-1 ou +1). */
        float arcSide = 1.0f;
        /** Lobe vertical ecran en pixels capture au tir. */
        float screenLobPixels = 0.0f;
        /** Rythme vite-lent-vite capture au tir. */
        float flightEaseStrength = 0.0f;
        /** Decalage lateral propre a ce boulet pour casser une file trop parfaite. */
        float flightLaneOffsetTiles = 0.0f;
        float tileX;
        float tileY;
        float ageSec;        /**< Temps ecoule depuis creation. */
        float launchDelaySec; /**< Delai avant depart (sec), pour etaler la salve. */
        /** Duree de vol sur la courbe apres le depart (u 0->1), varie legerement par boulet. */
        float flightDurationSec;
        /** Point de depart fixe de la courbe, resolu au lancement reel du boulet. */
        float startTileX;
        float startTileY;
        /** Decalages de sortie captures au tir, reappliques depuis le navire au lancement. */
        float launchForwardOffsetTiles = 0.0f;
        float launchSideOffsetTiles = 0.0f;
        bool launchStartResolved = false;
        /**
         * Frame inertiel "soft" capture au lancement:
         * on autorise le point de depart a suivre l'attaquant uniquement selon l'axe
         * du tir initial (projection), pour eviter les zigzags quand le navire tourne.
         */
        float launchAttackerTileX = 0.0f;
        float launchAttackerTileY = 0.0f;
        float launchAxisDirX = 0.0f; // vecteur unitaire
        float launchAxisDirY = 0.0f; // vecteur unitaire
        bool launchAxisResolved = false;
        /** Longueur de corde (P0->P2) capturee au lancement, sert a stabiliser l'amplitude de courbure. */
        float launchChordLenTiles = 0.0f;
        /** Indice lateral dans la salve (ex: -2..2 pour 5 boulets), pour ecarter les trajectoires. */
        float lateralSlot;
        std::uint8_t salvoBallCount;
        /** Decal impact (tuiles); salve maritime: toujours 0 (centre de la cible). */
        float impactTileOffX;
        float impactTileOffY;
        bool useIlluminatedProjectileTint = false; /**< True => halo illumine + sprite blanc. */
        /** Index 0..6 (palette); fige au tir; la couleur affichee vient surtout des champs illuminatedGlow*. */
        std::uint8_t illuminatedColorIndex = 0;
        /** Couleur du halo figee au tir (evite derive / etats SDL partages entre frames). */
        std::uint8_t illuminatedGlowR = 255;
        std::uint8_t illuminatedGlowG = 255;
        std::uint8_t illuminatedGlowB = 255;
        /** True si ce boulet transporte une config glow dediee plutot que la config runtime globale. */
        bool hasIlluminatedGlowConfigOverride = false;
        /** Copie resolue au tir d'une config glow dediee a ce boulet/salve. */
        VFXClassic::IlluminatedProjectileGlowConfig illuminatedGlowConfigOverride{};
    };

    struct MuzzleBurst {
        std::size_t vfxShipIndex = 0U; /**< Index dans @ref GameState::vfxShips. */
        std::vector<GameplayVfxShipSlot>* vfxShipSlots = nullptr;
        Ship* attacker; /**< Navire attaquant portant visuellement le muzzle VFX. */
        Ship* target;   /**< Navire vise, utilise pour le contexte cible relatif. */
        float elapsedSec; /**< Temps ecoule depuis le spawn du VFX muzzle. */
    };

    /** Suivi d'un VFX ship d'impact attache au navire cible. */
    struct TargetImpactBurst {
        std::size_t vfxShipIndex = 0U;
        std::vector<GameplayVfxShipSlot>* vfxShipSlots = nullptr;
        Ship* targetShip;   /**< Navire cible portant visuellement l'effet d'impact. */
        Ship* attackerShip; /**< Navire attaquant, utile pour TARGET_RELATIVE_AB. */
        float elapsedSec;   /**< Temps ecoule depuis le spawn du VFX d'impact. */
    };

    /** VFX ship d'impact programme apres l'impact physique du boulet. */
    struct PendingTargetImpactBurst {
        std::string vfxShipFolder{};
        std::vector<GameplayVfxShipSlot>* vfxShipSlots = nullptr;
        Ship* targetShip = nullptr;
        Ship* attackerShip = nullptr;
        float remainingDelaySec = 0.0f;
    };

    std::vector<Cannonball> cannonballs; /**< Boulets actuellement en vol pour toutes les salves actives. */
    std::vector<MuzzleBurst> muzzleBursts; /**< VFX ship de depart encore vivants sur les attaquants. */
    std::vector<TargetImpactBurst> targetImpactBursts; /**< VFX ship d'impact encore vivants sur les cibles. */
    std::vector<PendingTargetImpactBurst> pendingTargetImpactBursts; /**< VFX impact differes en attente. */
    std::vector<SalvoEntry> salvoEntries; /**< Catalogue runtime des familles de salves disponibles. */
    std::vector<VFXClassic> salvoProjectileVfx; /**< Lecteurs/runtime des spritesheets projectiles en vol. */
    std::mt19937 rng; /**< Generateur aleatoire utilise pour etaler et varier visuellement les salves. */
    /** Salve 1 boulet illumine: derniere couleur palette pour eviter deux fois la meme a la suite. */
    int lastIlluminatedSingleSalvoPaletteIdx = -1;

    static constexpr float kMuzzleMaxDurationSec = 3.0f;
    /** Duree de vol fixe de chaque boulet apres son depart. */
    static constexpr std::uint32_t kProjectileFlightDurationMs = 1150U;
    /**
     * Echelle de base des spritesheets projectile a zoom carte 100% (1.0): pixels asset ~ pixels ecran.
     * En jeu: taille affichee = @ref kDrawScale * @ref Camera::getZoomFactor (ex. zoom 0.9 => 10% plus petit).
     */
    static constexpr float kDrawScale = 1.0f;
    /** Courbure: deplacement du point de controle Bezier = fraction de la corde P0-P2. */
    static constexpr float kBezierBowChordFraction = 0.2f;
    /** Fenetre max (salve 5): base avant facteur distance; pres = boulets lisibles, loin = stream plus serre. */
    static constexpr float kSalvoLaunchStaggerMax5 = 0.44f;
    /** Fenetre max (salve 10). */
    static constexpr float kSalvoLaunchStaggerMax10 = 0.58f;
    /** Echelonnement si 2..4 boulets. */
    static constexpr float kSalvoLaunchStaggerMaxSmall = 0.18f;
    /** Bruit leger sur les delais (ordre de file conserve). */
    static constexpr float kSalvoLaunchStaggerJitter = 0.022f;

    void fireSalvoInternal(
        Ship& shipAttack,
        Ship& shipTarget,
        const SalvoEntry& entry,
        int ballCount,
        std::vector<GameplayVfxShipSlot>* localVfxShipSlots);

    void updateInternal(double dt);
    void drawSalvoProjectilesInternal(void) const;
    void clearInternal(void);

    void trimFinishedMuzzles(void);
    void processPendingTargetImpacts(double dt);
    void trimFinishedTargetImpacts(void);

    void scheduleTargetImpactVfxFromCannonball(Cannonball& b);
    void spawnTargetImpactVfx(
        Ship& targetShip,
        Ship& attackerShip,
        const char* endFolder,
        std::vector<GameplayVfxShipSlot>* vfxShipSlots);
};
