#pragma once

#include <RC2D/RC2D.h>

/**
 * @brief Helper de rendu des controles de fenetre HUD.
 *
 * Cette classe centralise le chargement et le rendu des icones
 * `icon-cross.png`, `icon-lock.png`, `icon-unlock.png` et
 * `icon-scale.png` afin
 * d'eviter que chaque widget redessine ces symboles a la main.
 */
class WindowControlIcons {
public:
    WindowControlIcons(void);
    ~WindowControlIcons(void);

    /**
     * @brief Charge les icones de fermeture et de verrouillage.
     */
    void load(void);

    /**
     * @brief Libere les references d'assets chargees.
     */
    void unload(void);

    /**
     * @brief Dessine un bouton "fermer" dans un rectangle cible.
     * @param buttonRect Rectangle du bouton.
     * @param fillColor Couleur de fond du bouton.
     * @param borderColor Couleur du contour du bouton.
     */
    void drawCloseButton(const SDL_FRect& buttonRect, RC2D_Color fillColor, RC2D_Color borderColor) const;

    /**
     * @brief Dessine un bouton cadenas ouvert/ferme.
     * @param buttonRect Rectangle du bouton.
     * @param locked True pour afficher le cadenas ferme, false pour l'ouvert.
     * @param fillColor Couleur de fond du bouton.
     * @param borderColor Couleur du contour du bouton.
     */
    void drawLockButton(const SDL_FRect& buttonRect, bool locked, RC2D_Color fillColor, RC2D_Color borderColor) const;

    /**
     * @brief Dessine une poignee de redimensionnement.
     * @param buttonRect Rectangle de la zone de resize.
     * @param fillColor Couleur de fond de la poignee.
     * @param borderColor Couleur du contour de la poignee.
     */
    void drawResizeHandle(const SDL_FRect& buttonRect, RC2D_Color fillColor, RC2D_Color borderColor) const;

private:
    RC2D_Image closeIcon;  /**< Icone de fermeture. */
    RC2D_Image lockIcon;   /**< Icone de cadenas ferme. */
    RC2D_Image unlockIcon; /**< Icone de cadenas ouvert. */
    RC2D_Image scaleIcon;  /**< Icone de redimensionnement. */

    /**
     * @brief Dessine le fond commun d'un bouton de controle.
     */
    void drawButtonFrame(const SDL_FRect& buttonRect, RC2D_Color fillColor, RC2D_Color borderColor) const;

    /**
     * @brief Dessine une icone centree dans un rectangle.
     */
    void drawCenteredIcon(const RC2D_Image& icon, const SDL_FRect& buttonRect, float padding = 3.0f) const;
};
