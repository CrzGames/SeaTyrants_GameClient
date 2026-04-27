#pragma once

#include <RC2D/RC2D.h>

#include <string>

/**
 * @brief Widget HUD affichant la barre de points de vie du joueur.
 *
 * La barre est rendue a une position HUD fixe et affiche un libelle
 * du type `HP : courant / max`, avec un remplissage proportionnel.
 */
class HpBarWidget {
public:
    /**
     * @brief Construit la barre de HP dans un etat vide.
     */
    HpBarWidget(void);

    /**
     * @brief Detruit la barre de HP.
     */
    ~HpBarWidget(void);

    /**
     * @brief Charge les ressources de rendu necessaires.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees par le widget.
     */
    void unload(void);

    /**
     * @brief Dessine la barre de HP.
     */
    void draw(void) const;

    /**
     * @brief Definit les HP actuels affiches dans la barre.
     *
     * La valeur est ramenee dans l'intervalle `[0, hpMax]`.
     *
     * @param value HP courants a afficher.
     */
    void setCurrentHp(int value);

    /**
     * @brief Definit les HP max affiches dans la barre.
     *
     * Un minimum de `1` est force pour garder une barre valide.
     *
     * @param value HP max a afficher.
     */
    void setMaxHp(int value);

    /**
     * @brief Definit le facteur d'echelle applique a la barre HP.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique a la barre HP.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant applique a la barre HP.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran de la barre HP.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant de la barre HP.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Retourne le rectangle courant de la barre HP.
     */
    SDL_FRect getCurrentRect(void) const;

private:
    /**
     * @brief Calcule le rectangle de rendu de la barre.
     *
     * @return Rectangle final de la barre HP.
     */
    SDL_FRect getBarRect(void) const;

    /**
     * @brief Retourne le ratio de remplissage de la barre.
     *
     * @return Valeur clamp entre `0.0f` et `1.0f`.
     */
    float getFillRatio(void) const;

    /**
     * @brief Construit le texte a afficher au centre de la barre.
     *
     * @return Libelle formate du type `HP : courant / max`.
     */
    std::string buildLabel(void) const;

    RC2D_Font labelFont; /**< Police du libelle central. */
    int currentHp;       /**< HP actuels affiches. */
    int maxHp;           /**< HP max affiches. */
    float uiScale;       /**< Facteur d'echelle applique au widget. */
    SDL_FPoint positionOffset; /**< Decalage ecran applique au widget. */
};
