#pragma once

#include <RC2D/RC2D.h>

class Camera;
class Map;

/**
 * @brief Widget HUD dedie a la minimap.
 *
 * Responsabilites:
 * - charger/decharger l'image minimap;
 * - calculer manuellement son rectangle ecran;
 * - dessiner la minimap.
 */
class MinimapWidget {
public:
    /**
     * @brief Tooltip specifique aux actions rattachees a la minimap.
     */
    enum class Tooltip : int {
        NONE = 0,
        ESPION = 1,
        PARAMS_MINIMAP = 2,
        WORLD_MAP = 3,
        NAVIGATION = 4
    };

    MinimapWidget(void);
    ~MinimapWidget(void);

    /**
     * @brief Charge les ressources et configure le widget.
     */
    void load(void);

    /**
     * @brief Met a jour les interactions runtime de la minimap.
     */
    void update(Camera& camera, Map& map);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Dessine le widget minimap.
     */
    void draw(const Map& map) const;

    /**
     * @brief Definit le facteur d'echelle applique a la minimap HUD.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique a la minimap.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant applique a la minimap.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran de la minimap.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant de la minimap.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Retourne le rectangle courant de la minimap dans l'espace de rendu.
     */
    SDL_FRect getCurrentRect(void) const;

    /**
     * @brief Retourne le rectangle de contenu cliquable de la minimap.
     */
    SDL_FRect getContentRect(void) const;

    /**
     * @brief Indique si un point tombe dans la zone bleue interactive de la minimap.
     */
    bool containsContentPoint(float x, float y) const;

    /**
     * @brief Gere un clic dans la zone interactive de la minimap.
     * @return true si le clic est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, Camera& camera, Map& map);
    
private:
    /**
     * @brief Recentre la camera depuis un point de la minimap.
     */
    void moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset, Camera& camera, Map& map) const;

    /**
     * @brief Ressource image de la minimap HUD.
     */
    RC2D_Image minimapImage;         /**< Texture de la minimap. */
    RC2D_ImageData minimapImageData; /**< Metadonnees source de la minimap. */

    /**
     * @brief Fenetre transparente detectee dans la texture source de la minimap.
     */
    SDL_Rect minimapContentSourceRect; /**< Rectangle source interne pour dessiner le fond minimap. */

    /**
     * @brief Indique si la fenetre interne a ete detectee correctement.
     */
    bool hasMinimapContentRect; /**< Etat de detection de la fenetre interne. */

    /**
     * @brief Etat de drag du rectangle de vue dans la minimap.
     */
    bool minimapDragActive; /**< True tant qu'un drag minimap est en cours. */

    /**
     * @brief Offset souris -> centre viewport pendant le drag minimap.
     */
    float minimapDragOffsetX; /**< Offset X de drag minimap. */
    float minimapDragOffsetY; /**< Offset Y de drag minimap. */
    float uiScale;            /**< Facteur d'echelle applique a l'image HUD. */
    SDL_FPoint positionOffset; /**< Decalage ecran applique au widget. */
};
