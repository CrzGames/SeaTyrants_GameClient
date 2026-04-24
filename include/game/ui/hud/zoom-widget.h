#pragma once

#include <RC2D/RC2D.h>

class Camera;

/**
 * @brief Widget de zoom HUD (barre + slider).
 *
 * Le slider est ancre en bas-gauche:
 * - barre a 5 px du bord gauche/bas;
 * - slider positionne sur la barre (centre vertical).
 *
 * Le zoom applique a la camera est discretise sur les pas
 * Camera::CAMERA_ZOOM_STEP_FACTOR entre
 * Camera::CAMERA_ZOOM_MAX_FACTOR et Camera::CAMERA_ZOOM_MIN_FACTOR.
 */
class ZoomWidget {
public:
    /**
     * @brief Construit le widget avec un etat vide (pas de ressources chargees).
     */
    ZoomWidget(void);

    /**
     * @brief Detruit le widget.
     *
     * @note Cette methode ne remplace pas unload(); les ressources
     *       graphiques doivent etre liberees explicitement.
     */
    ~ZoomWidget(void);

    /**
     * @brief Charge les assets UI et initialise la geometrie du widget.
     *
     * Configure les ancrages/marges de la barre et du slider, memorise les
     * dimensions natives des textures et reinitialise l'etat de drag.
     */
    void load(void);

    /**
     * @brief Libere les ressources graphiques chargees par load().
     */
    void unload(void);

    /**
     * @brief Met a jour l'interaction du slider et synchronise le zoom camera.
     *
     * - Si aucun drag n'est actif, le slider suit le zoom courant de la camera.
     * - Pendant un drag gauche, la position du slider est "snap" sur les crans
     *   de zoom puis appliquee a la camera.
     *
     * @param camera Camera gameplay a lire/ecrire.
     */
    void update(Camera& camera);

    /**
     * @brief Dessine la barre de zoom et le slider.
     *
     * Les elements sont rendus en bas-gauche et le slider est recentre
     * verticalement sur la barre.
     */
    void draw(void);

    /**
     * @brief Traite un appui souris pour potentiellement demarrer un drag slider.
     *
     * Le drag est lance uniquement si le bouton gauche est presse sur la
     * surface actuellement dessinee du slider.
     *
     * @param x Position X du clic (coords de rendu).
     * @param y Position Y du clic (coords de rendu).
     * @param button Bouton souris recu depuis l'evenement.
     * @return true si l'evenement est consomme par le widget, sinon false.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button);

    /**
     * @brief Indique si le slider de zoom est actuellement en cours de drag.
     *
     * @return true si le bouton de zoom est attrape par la souris, sinon false.
     */
    bool isDraggingSlider(void) const;

    /**
     * @brief Indique si un point de rendu survole la surface dessinee du slider.
     *
     * @param x Position X en coordonnees de rendu.
     * @param y Position Y en coordonnees de rendu.
     * @return true si le point est sur le slider visible, sinon false.
     */
    bool isSliderHovered(float x, float y) const;

private:
    RC2D_UIImage zoomBarUi;     /**< Barre de zoom. */
    RC2D_UIImage zoomSliderUi;  /**< Slider de zoom. */

    bool sliderDragging;          /**< true tant que le drag gauche est actif. */
    float sliderDragGrabOffsetX;  /**< Offset curseur->slider pris au debut du drag. */

    float sliderOffsetX;          /**< Offset horizontal du slider depuis le bord gauche de la barre. */
    float sliderTravelWidth;      /**< Course totale possible du slider. */

    float zoomBarWidthPx;         /**< Largeur native de la barre. */
    float zoomBarHeightPx;        /**< Hauteur native de la barre. */
    float zoomSliderWidthPx;      /**< Largeur native du slider. */
    float zoomSliderHeightPx;     /**< Hauteur native du slider. */

    /**
     * @brief Teste si un point est inclus dans un rectangle SDL.
     *
     * @param x Coordonnee X du point.
     * @param y Coordonnee Y du point.
     * @param rect Rectangle de reference.
     * @return true si le point est dans le rectangle, sinon false.
     */
    static bool pointInRect(float x, float y, const SDL_FRect& rect);

    /**
     * @brief Calcule le nombre de crans de zoom entre min et max camera.
     *
     * @return Nombre de pas de zoom (minimum 1).
     */
    static int getZoomStepsCount(void);

    /**
     * @brief Recalcule la course horizontale disponible du slider.
     *
     * Priorise les dimensions des rectangles effectivement dessines, avec
     * fallback sur les dimensions natives des textures.
     */
    void refreshTravelWidthFromDrawnRects(void);

    /**
     * @brief Met a jour la position X du slider a partir d'une valeur brute.
     *
     * La valeur est clamp dans la course disponible puis quantifiee sur les
     * crans de zoom.
     *
     * @param rawOffset Offset horizontal brut depuis le bord gauche de la barre.
     */
    void setSliderOffsetFromRawValue(float rawOffset);

    /**
     * @brief Aligne la position du slider sur le zoom actuel de la camera.
     *
     * @param camera Camera source du zoom courant.
     */
    void syncSliderFromCameraZoom(const Camera& camera);

    /**
     * @brief Convertit la position du slider en zoom et l'applique a la camera.
     *
     * @param camera Camera cible a mettre a jour.
     */
    void applySliderToCameraZoom(Camera& camera) const;
};

