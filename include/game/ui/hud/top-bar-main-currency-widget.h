#pragma once

#include <RC2D/RC2D.h>

#include <cstdint>

/**
 * @brief Widget d'affichage permanent des monnaies principales sur la top bar.
 *
 * Ce widget est purement informatif : il affiche les rubies et le gold
 * directement en haut a gauche de l'ecran, independamment de la fenetre Money.
 */
class TopBarMainCurrencyWidget {
public:
    TopBarMainCurrencyWidget(void);
    ~TopBarMainCurrencyWidget(void);

    /**
     * @brief Charge les ressources graphiques du widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources graphiques du widget.
     */
    void unload(void);

    /**
     * @brief Dessine le widget des monnaies principales.
     */
    void draw(void) const;

    /**
     * @brief Definit le montant de rubies affiche dans la top bar.
     *
     * La valeur est automatiquement bornee a zero si un montant negatif
     * est fourni, afin d'eviter tout affichage incoherent dans le HUD.
     *
     * @param amount Nouveau montant de rubies a afficher.
     */
    void setRubiesAmount(std::int64_t amount);

    /**
     * @brief Definit le montant de gold affiche dans la top bar.
     *
     * La valeur est automatiquement bornee a zero si un montant negatif
     * est fourni, afin d'eviter tout affichage incoherent dans le HUD.
     *
     * @param amount Nouveau montant de gold a afficher.
     */
    void setGoldAmount(std::int64_t amount);

private:
    RC2D_Image rubiesIcon;     /**< Icone de la monnaie rubies. */
    RC2D_Image goldIcon;       /**< Icone de la monnaie gold. */

    RC2D_Font amountFont;      /**< Police utilisee pour les montants. */

    float localX;              /**< Position X locale dans le game screen. */
    float localY;              /**< Position Y locale dans le game screen. */

    std::int64_t rubiesAmount; /**< Montant de rubies actuellement affiche. */
    std::int64_t goldAmount;   /**< Montant de gold actuellement affiche. */

    /**
     * @brief Positionne le widget dans l'espace du game screen.
     * @param newLocalX Offset horizontal depuis le coin haut-gauche du game screen.
     * @param newLocalY Offset vertical depuis le coin haut-gauche du game screen.
     */
    void setLocalPosition(float newLocalX, float newLocalY);
};
