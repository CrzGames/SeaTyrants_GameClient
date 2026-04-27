#pragma once

#include <RC2D/RC2D.h>

#include "game/ui/hud/hud-cursor.h"

class CenterShipButtonWidget {
public:
    CenterShipButtonWidget(void);
    ~CenterShipButtonWidget(void);

    /**
     * @brief Charge les ressources et configure le widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Dessine le bouton.
     */
    void draw(void);

    /**
     * @brief Definit le facteur d'echelle applique au bouton.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique au bouton.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant applique au bouton.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran du bouton.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant du bouton.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Indique si un point ecran survole le bouton.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour ce widget.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Retourne le rectangle actuellement utilise par le bouton.
     *
     * @return Rectangle de rendu / hitbox du bouton.
     */
    SDL_FRect getCurrentRect(void) const;

private:
    /**
     * @brief Ressource image du bouton de centrage.
     */
    RC2D_Image buttonImage;         /**< Texture du bouton de centrage navire. */
    RC2D_ImageData buttonImageData; /**< Metadonnees source du bouton. */
    float uiScale;                  /**< Facteur d'echelle applique au bouton. */
    SDL_FPoint positionOffset;      /**< Decalage ecran applique au widget. */
};
