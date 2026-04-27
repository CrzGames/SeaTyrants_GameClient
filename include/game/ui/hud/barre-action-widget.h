#pragma once

#include <RC2D/RC2D.h>

class BarreActionWidget {
public:
    BarreActionWidget(void);
    ~BarreActionWidget(void);

    /**
     * @brief Charge les ressources et initialise la geometrie de base.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees.
     */
    void unload(void);

    /**
     * @brief Dessine la barre d'action.
     */
    void draw(void);

    /**
     * @brief Definit le facteur d'echelle applique a la barre d'action.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique a la barre d'action.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant applique a la barre d'action.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran de la barre d'action.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant de la barre d'action.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Retourne le rectangle courant de la barre d'action.
     */
    SDL_FRect getCurrentRect(void) const;

private:
    /**
     * @brief Ressource image de la barre d'action HUD.
     */
    RC2D_Image actionBarImage;         /**< Texture de la barre d'action. */
    RC2D_ImageData actionBarImageData; /**< Metadonnees source de la barre d'action. */
    float uiScale;                     /**< Facteur d'echelle applique a l'image HUD. */
    SDL_FPoint positionOffset;         /**< Decalage ecran applique au widget. */
};
