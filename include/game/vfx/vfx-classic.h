#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <RC2D/RC2D.h>

/**
 * @class VFXClassic
 * @brief Lecteur runtime d'une spritesheet VFX simple exportee depuis le mode spritesheet.
 *
 * Le pipeline charge:
 * - le JSON spritesheet `<vfx>-spritesheet.json`;
 * - l'image spritesheet associee;
 * - la liste de frames;
 * - le `fps` d'animation;
 * - `animationTotalDurationMs` (ou `null`, donc lecture infinie).
 */
class VFXClassic {
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

    RC2D_Image spritesheetImage;      /**< Texture spritesheet chargee. */
    std::vector<Frame> frames;        /**< Frames source de l'animation. */
    float defaultFps;                 /**< FPS de lecture. */
    int animationTotalDurationMs;     /**< Duree totale de lecture (ms), 0 = infini. */
    float playbackSeconds;            /**< Horloge runtime accumulee (sec). */
    float framePhaseOffsetSec;       /**< Decalage de phase lecture (sec), ex. demarrage mid-spritesheet. */
    bool loaded;                      /**< True si toutes les ressources sont pretes. */

    std::string vfxFolderPath;        /**< Chemin dossier VFX resolu (debug/trace). */
    std::string spritesheetJsonPath;  /**< Chemin JSON spritesheet charge. */
    std::string spritesheetImagePath; /**< Chemin image spritesheet chargee. */

    /**
     * @brief Retourne la periode d'animation (sec) selon fps et nombre de frames.
     * @return Duree d'un cycle complet.
     */
    float resolvedPlaybackPeriodSeconds(void) const;
    int getCurrentFrameIndexWithPhaseOffset(float additionalPhaseOffsetSec) const;

    void drawInternal(
        float centerX,
        float centerY,
        float scale,
        float rotationDeg,
        bool flipHorizontal,
        bool flipVertical,
        const std::uint8_t* tintRgb,
        std::uint8_t alpha = 255,
        int blendMode = 0,
        float additionalPhaseOffsetSec = 0.0f) const;

public:
    /**
     * @brief Reglages runtime centralises du glow des boulets illumines.
     *
     * Toute modification de cette config s'applique en direct:
     * - aux boulets deja en vol ;
     * - aux nouveaux boulets tires ensuite.
     */
    struct IlluminatedProjectileGlowConfig {
        float glowIntensity = 0.58f;        /**< Multiplicateur global des couches glow. */
        float glowOpacity = 0.48f;          /**< Opacite de base du gradient radial. */
        float glowRadius = 0.48f;           /**< Taille/rayon global du halo. */
        float glowDispersion = 0.08f;       /**< Ecartement relatif entre les couches. */
        float glowRoundness = 1.95f;        /**< Rondeur du coeur radial. */
        float coreSharpness = 2.85f;        /**< Durete / compacite du coeur lumineux. */
        float centerIntensity = 0.78f;      /**< Renfort au centre du glow. */
        float centerFlashReduction = 0.48f; /**< Reduction du hotspot trop flashy au centre. */
        float edgeSoftness = 1.0f;          /**< Douceur/fondu des bords du halo. */
        float coreWhiteIntensity = 0.56f;   /**< Petit coeur blanc tres lumineux au centre. */
        float coreWhiteRadius = 0.68f;      /**< Rayon du coeur blanc par rapport au sprite. */
        float colorShellIntensity = 0.72f;  /**< Couronne coloree autour du coeur blanc. */
        float colorShellRadius = 0.92f;     /**< Rayon de la shell coloree. */
        float motionSmear = 0.18f;          /**< Etirement lumineux leger dans l'axe du mouvement. */
        float spriteOpacity = 1.0f;         /**< Visibilite du sprite central d'origine. */
        float spriteBoost = 0.32f;          /**< Renfort additif du sprite pour retrouver le coeur Seafight. */
        float spriteTintStrength = 0.16f;   /**< Teinte du boost sprite entre blanc (0) et couleur glow (1). */
        bool compactGlowEnabled = true;     /**< Active un glow court, dense et moins brumeux. */
        bool outerLayerEnabled = false;     /**< Active/desactive la couche externe. */
        bool midLayerEnabled = true;        /**< Active/desactive la couche mediane. */
        bool innerLayerEnabled = true;      /**< Active/desactive la couche interne. */
    };

    /**
     * @brief Constructeur runtime VFX simple.
     */
    VFXClassic(void);

    /**
     * @brief Destructeur runtime VFX simple (appelle unload()).
     */
    ~VFXClassic(void);

    /**
     * @brief Charge un VFX simple via son dossier.
     *
     * Exemple:
     * - vfxFolderPath = `assets/images/vfx/vfx-speedwhitedeux`
     * - JSON derive = `assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-spritesheet.json`
     *
     * @param vfxFolderPath Dossier du VFX.
     * @return True si spritesheet JSON + image sont charges depuis RC2D_STORAGE_TITLE.
     */
    bool loadFromFolder(const char* vfxFolderPath);

    /**
     * @brief Decharge toutes les ressources runtime.
     */
    void unload(void);

    /**
     * @brief Indique si le VFX est pret a jouer/dessiner.
     * @return True si texture + frames + etat charge sont valides.
     */
    bool isLoaded(void) const;

    /**
     * @brief Reinitialise l'horloge d'animation au debut.
     */
    void resetPlayback(void);

    /**
     * @brief Decale la phase de l'animation (boucle) sans avancer l'horloge.
     * @param seconds Offset en secondes ajoute au calcul de frame (modulo periode).
     */
    void setFramePhaseOffsetSeconds(float seconds);

    /**
     * @brief Met a jour l'horloge d'animation.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Indique si la lecture est infinie.
     * @return True si `animationTotalDurationMs` vaut 0 / null.
     */
    bool isInfinite(void) const;

    /**
     * @brief Indique si l'animation finite a atteint sa duree max.
     * @return True si la lecture finite est terminee.
     */
    bool isFinished(void) const;

    /**
     * @brief Retourne le nombre total de frames chargees.
     * @return Nombre de frames.
     */
    int getFrameCount(void) const;

    /**
     * @brief Retourne le FPS de lecture.
     * @return FPS utilise pour la lecture.
     */
    float getDefaultFps(void) const;

    /**
     * @brief Retourne la duree max de lecture en millisecondes.
     * @return 0 si lecture infinie, sinon la duree max.
     */
    int getAnimationTotalDurationMs(void) const;

    /**
     * @brief Retourne les valeurs par defaut du glow illumine.
     */
    static IlluminatedProjectileGlowConfig getDefaultIlluminatedProjectileGlowConfig(void);

    /**
     * @brief Retourne la config glow actuellement active.
     */
    static const IlluminatedProjectileGlowConfig& getIlluminatedProjectileGlowConfig(void);

    /**
     * @brief Remplace la config glow runtime appliquee aux boulets illumines.
     */
    static void setIlluminatedProjectileGlowConfig(const IlluminatedProjectileGlowConfig& config);

    /**
     * @brief Reinitialise la config glow illumine a ses valeurs par defaut.
     */
    static void resetIlluminatedProjectileGlowConfig(void);

    /**
     * @brief Charge la config glow illumine depuis le fichier gameplay exporte.
     * @return True si un fichier valide a ete charge; false si les defaults restent actifs.
     */
    static bool loadIlluminatedProjectileGlowConfigFromFile(void);

    /**
     * @brief Lit une config glow illumine depuis un chemin JSON explicite sans modifier l'etat runtime global.
     * @param path Chemin `assets/...` a lire.
     * @param outConfig Destination de la config lue.
     * @return True si le JSON est valide et a pu etre converti.
     */
    static bool readIlluminatedProjectileGlowConfigFromFile(
        const char* path,
        IlluminatedProjectileGlowConfig* outConfig);

    /**
     * @brief Exporte la config glow illumine courante vers le fichier gameplay.
     * @return True si l'ecriture a reussi.
     */
    static bool exportIlluminatedProjectileGlowConfigToFile(void);

    /**
     * @brief Exporte une config glow illumine vers un chemin JSON explicite.
     *
     * Si @p configOverride vaut `nullptr`, la config runtime courante est ecrite.
     */
    static bool exportIlluminatedProjectileGlowConfigToFile(
        const char* path,
        const IlluminatedProjectileGlowConfig* configOverride);

    /**
     * @brief Force le prochain chargement one-shot du fichier glow gameplay par defaut.
     */
    static void invalidateIlluminatedProjectileGlowConfigFileLoadState(void);

    /**
     * @brief Chemin gameplay du fichier de config glow exporte / charge.
     */
    static const char* getIlluminatedProjectileGlowConfigPath(void);

    /**
     * @brief Retourne l'index de frame courant.
     * @return Index valide dans `frames`, ou 0 si aucune frame.
     */
    int getCurrentFrameIndex(void) const;

    /**
     * @brief Dessine la frame courante centree sur une position ecran.
     * @param centerX Centre X ecran.
     * @param centerY Centre Y ecran.
     * @param scale Echelle uniforme.
     * @param rotationDeg Rotation appliquee (degres).
     * @param flipHorizontal Miroir horizontal.
     * @param flipVertical Miroir vertical.
     */
    void draw(
        float centerX,
        float centerY,
        float scale = 1.0f,
        float rotationDeg = 0.0f,
        bool flipHorizontal = false,
        bool flipVertical = false) const;

    /**
     * @brief Meme rendu que @ref draw avec multiplication RGB sur la texture (tinte stable).
     *
     * Utilise SDL_SetTextureColorMod sur la spritesheet: convenable pour les boulets illumines
     * (une teinte par instance, identique sur toutes les frames).
     *
     * @param tintR Multiplicateur canal rouge [0..255].
     * @param tintG Multiplicateur canal vert [0..255].
     * @param tintB Multiplicateur canal bleu [0..255].
     */
    void drawWithRgbTint(
        float centerX,
        float centerY,
        float scale,
        float rotationDeg,
        bool flipHorizontal,
        bool flipVertical,
        std::uint8_t tintR,
        std::uint8_t tintG,
        std::uint8_t tintB) const;

    /**
     * @brief Variante generique avec teinte RGB optionnelle, alpha et blend SDL explicites.
     *
     * Si @p tintRgbOrNull vaut `nullptr`, aucun ColorMod n'est applique.
     * `blendMode = 0` conserve le blend mode actuel de la texture.
     */
    void drawWithTintAlphaBlend(
        float centerX,
        float centerY,
        float scale,
        float rotationDeg,
        bool flipHorizontal,
        bool flipVertical,
        const std::uint8_t* tintRgbOrNull,
        std::uint8_t alpha,
        int blendMode) const;

    void drawWithTintAlphaBlendPhaseOffset(
        float centerX,
        float centerY,
        float scale,
        float rotationDeg,
        bool flipHorizontal,
        bool flipVertical,
        const std::uint8_t* tintRgbOrNull,
        std::uint8_t alpha,
        int blendMode,
        float phaseOffsetSec) const;

    /**
     * @brief Rendu "boulet illumine": halo radial colore (texture procedurale) puis spritesheet sans ColorMod (sprite tel quel).
     *
     * Le halo suit la meme echelle que @ref draw; la couleur est portee par le gradient.
     */
    /**
     * @param lockGlowToMaxSpriteFrame Si true, le halo radial utilise la plus grande taille de frame
     *        de la spritesheet (halo toujours "large"), sinon la frame courante (halo qui pulse avec l'anim).
     */
    void drawIlluminatedProjectile(
        float centerX,
        float centerY,
        float scale,
        float rotationDeg,
        bool flipHorizontal,
        bool flipVertical,
        std::uint8_t glowR,
        std::uint8_t glowG,
        std::uint8_t glowB,
        const IlluminatedProjectileGlowConfig* glowConfigOverride,
        bool lockGlowToMaxSpriteFrame = false) const;
};
