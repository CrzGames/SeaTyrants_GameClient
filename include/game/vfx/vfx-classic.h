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
    bool loaded;                      /**< True si toutes les ressources sont pretes. */

    std::string vfxFolderPath;        /**< Chemin dossier VFX resolu (debug/trace). */
    std::string spritesheetJsonPath;  /**< Chemin JSON spritesheet charge. */
    std::string spritesheetImagePath; /**< Chemin image spritesheet chargee. */

    /**
     * @brief Retourne la periode d'animation (sec) selon fps et nombre de frames.
     * @return Duree d'un cycle complet.
     */
    float resolvedPlaybackPeriodSeconds(void) const;

public:
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
};
