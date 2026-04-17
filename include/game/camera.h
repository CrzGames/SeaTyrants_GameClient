#pragma once

#include <SDL3/SDL.h>

class Map;

/**
 * @brief Camera gameplay (position + zoom) independante du viewport UI.
 *
 * La camera manipule une position en coordonnees tuiles flottantes:
 * - cameraTileX / cameraTileY: centre logique observe dans la map.
 * - zoomFactor: echelle monde (1.0 = 100%).
 */
class Camera {
private:
    float cameraTileX;   /**< Centre camera en coordonnee tuile X. */
    float cameraTileY;   /**< Centre camera en coordonnee tuile Y. */
    float zoomFactor;    /**< Zoom monde (1.0 = 100%). */
    float minZoomFactor; /**< Zoom minimal autorise. */
    float maxZoomFactor; /**< Zoom maximal autorise. */

    void clampCameraToMap(const Map& map, const SDL_FRect& viewportRect);

public:
    static constexpr float CAMERA_SCROLL_SPEED_SECTORS = 8.0f;
    static constexpr float CAMERA_DIAGONAL_FACTOR = 0.70710678f;
    static constexpr float CAMERA_ZOOM_MIN_FACTOR = 0.40f;  /**< Zoom minimal gameplay autorise. */
    static constexpr float CAMERA_ZOOM_MAX_FACTOR = 1.00f;  /**< Zoom maximal gameplay autorise. */
    static constexpr float CAMERA_ZOOM_STEP_FACTOR = 0.05f; /**< Pas de zoom gameplay (clavier/UI). */

    Camera();
    ~Camera();

    /**
     * @brief Reinitialise la camera au centre logique de la map.
     */
    void resetCamera(const Map& map, const SDL_FRect& viewportRect);

    /**
     * @brief Centre la camera sur une tuile donnee.
     */
    void centerCameraOnTile(float tileX, float tileY, const Map& map, const SDL_FRect& viewportRect);

    /**
     * @brief Deplace la camera en delta de tuiles.
     */
    void moveCameraTiles(float deltaTileX, float deltaTileY, const Map& map, const SDL_FRect& viewportRect);

    /**
     * @brief Definit le zoom monde. Le clamp map est applique dans applyToMap().
     */
    void setZoomFactor(float value);

    /**
     * @brief Retourne le zoom monde courant.
     */
    float getZoomFactor(void) const;

    /**
     * @brief Applique camera + zoom a la map.
     */
    void applyToMap(Map& map, const SDL_FRect& viewportRect);

    /**
     * @brief Met a jour l'etat camera sur la map.
     *
     * Alias explicite de applyToMap() pour les boucles d'update gameplay.
     */
    void update(Map& map, const SDL_FRect& viewportRect);
};
