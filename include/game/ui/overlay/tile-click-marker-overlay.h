#pragma once

#include <RC2D/RC2D.h>

#include "game/map/map.h"

/**
 * @brief Marqueur visuel de clic sur tuile (losange pulse).
 */
class TileClickMarkerOverlay {
public:
    /**
     * @brief Constructeur du marqueur.
     */
    TileClickMarkerOverlay(void);

    /**
     * @brief Destructeur du marqueur.
     */
    ~TileClickMarkerOverlay(void);

    /**
     * @brief Affiche le marqueur sur une tuile.
     * @param tileX Tuile cible X.
     * @param tileY Tuile cible Y.
     */
    void show(int tileX, int tileY);

    /**
     * @brief Cache le marqueur.
     */
    void hide(void);

    /**
     * @brief Met a jour le timer du marqueur.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine le marqueur.
     * @param map Map utilisee pour la projection tuile -> ecran.
     */
    void draw(const Map& map) const;

    /**
     * @brief Definit la duree d'affichage du marqueur.
     * @param value Duree en secondes.
     *        - value > 0  : auto-expiration active.
     *        - value <= 0 : marqueur persistant (jusqu'au prochain show/hide).
     */
    void setDurationSeconds(double value);

    /**
     * @brief Retourne l'etat visible du marqueur.
     * @return True si le marqueur est affiche.
     */
    bool isVisible(void) const;
    
private:
    bool visible;    /**< True si le marqueur est actif. */
    SDL_Point tile;  /**< Tuile du dernier clic gauche. */

    double elapsedSeconds;  /**< Temps ecoule depuis l'affichage. */
    double durationSeconds; /**< Duree totale d'affichage. */

    float pulseAmplitude; /**< Amplitude de la pulsation. */
    float pulseSpeed;     /**< Vitesse de pulsation. */

    RC2D_Color fillColor; /**< Couleur de remplissage. */
    RC2D_Color lineColor; /**< Couleur du contour. */
};
