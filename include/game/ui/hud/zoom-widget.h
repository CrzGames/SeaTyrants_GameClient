#pragma once

#include <RC2D/RC2D.h>

#include <functional>

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
     * @brief Fonction appelee une fois le zoom monde stabilise apres interaction utilisateur.
     *
     * En pratique: fin de drag sur le slider (bouton gauche relache). Sert typiquement
     * a declencher une sauvegarde settings sans spammer a chaque frame de drag.
     */
    using MapWorldZoomCommitCallback = std::function<void(void)>;

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
     * Charge les textures, memorise leurs dimensions natives et reinitialise
     * l'etat de drag.
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
     * @brief Dessine le tooltip de zoom au-dessus des autres GUI.
     *
     * Cette methode est volontairement separee de draw() pour permettre
     * au HUD overlay de la rendre en toute fin de frame, au-dessus des
     * fenetres flottantes.
     */
    void drawTooltip(void) const;

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

    /**
     * @brief Retourne le rectangle courant de la barre de zoom.
     *
     * Le rectangle est calcule a partir de la geometrie HUD connue
     * et sert aux autres widgets bas d'ecran pour s'aligner proprement
     * sur la barre de zoom.
     *
     * @return Rectangle de rendu actuel de la barre de zoom.
     */
    SDL_FRect getBarRect(void) const;

    /**
     * @brief Retourne le rectangle courant complet du widget de zoom.
     *
     * Inclut la barre et le slider afin de pouvoir manipuler / mettre
     * en evidence l'ensemble du widget depuis l'overlay HUD.
     *
     * @return Rectangle englobant actuellement dessine.
     */
    SDL_FRect getCurrentRect(void) const;

    /**
     * @brief Definit le facteur d'echelle applique a la barre et au slider.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique au widget de zoom.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant du widget de zoom.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran du widget de zoom.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant du widget de zoom.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Enregistre @ref MapWorldZoomCommitCallback (typiquement sauvegarde settings).
     */
    void setOnMapWorldZoomCommit(MapWorldZoomCommitCallback callback);

    /**
     * @brief Realigne le slider HUD sur le zoom courant de la camera (apres chargement settings, etc.).
     */
    void syncSliderToCamera(const Camera& camera);

private:
    RC2D_Image zoomBarImage;         /**< Texture de la barre de zoom. */
    RC2D_ImageData zoomBarImageData; /**< Metadonnees source de la barre. */
    RC2D_Image zoomSliderImage;         /**< Texture du slider de zoom. */
    RC2D_ImageData zoomSliderImageData; /**< Metadonnees source du slider. */
    RC2D_Font tooltipFont;             /**< Police du tooltip de zoom. */

    bool sliderDragging;          /**< true tant que le drag gauche est actif. */
    bool sliderHovered;           /**< true si la souris survole le slider. */
    float sliderDragGrabOffsetX;  /**< Offset curseur->slider pris au debut du drag. */
    float hoveredMouseX;          /**< Position X souris memorisee pour le tooltip. */
    float hoveredMouseY;          /**< Position Y souris memorisee pour le tooltip. */
    float displayedZoomFactor;    /**< Derniere valeur de zoom a afficher dans le tooltip. */

    float sliderOffsetX;          /**< Offset horizontal du slider depuis le bord gauche de la barre. */
    float sliderTravelWidth;      /**< Course totale possible du slider. */

    float zoomBarWidthPx;         /**< Largeur native de la barre. */
    float zoomBarHeightPx;        /**< Hauteur native de la barre. */
    float zoomSliderWidthPx;      /**< Largeur native du slider. */
    float zoomSliderHeightPx;     /**< Hauteur native du slider. */
    float uiScale;                /**< Facteur d'echelle applique au widget. */
    SDL_FPoint positionOffset;    /**< Decalage ecran applique au widget. */
    /**
     * Callback optionnel enregistre via @ref setOnMapWorldZoomCommit : invoque a la fin du drag
     * sur le slider lorsque le zoom caméra (monde) est fixe.
     */
    MapWorldZoomCommitCallback onMapWorldZoomCommit;

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
     * Utilise les dimensions natives des textures et l'echelle courante.
     */
    void refreshTravelWidthFromDrawnRects(void);

    /**
     * @brief Calcule le rectangle courant de la barre de zoom.
     *
     * @return Rectangle de rendu de la barre.
     */
    SDL_FRect computeBarRect(void) const;

    /**
     * @brief Calcule le rectangle de reference non reduit de la barre.
     *
     * @return Rectangle de base avant application de l'echelle HUD.
     */
    SDL_FRect computeBaseBarRect(void) const;

    /**
     * @brief Calcule le rectangle courant du slider de zoom.
     *
     * @return Rectangle de rendu du slider.
     */
    SDL_FRect computeSliderRect(void) const;

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

    /**
     * @brief Dessine le tooltip de survol du slider de zoom.
     */
    void drawHoveredTooltip(void) const;
};

