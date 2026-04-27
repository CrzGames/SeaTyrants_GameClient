#pragma once

#include <RC2D/RC2D.h>

#include <string>

/**
 * @brief Widget HUD affichant la barre de points d'experience du joueur.
 *
 * La barre est rendue a une position HUD fixe, juste sous la barre HP,
 * et affiche un libelle du type `XP : valeur`.
 */
class ExperienceBarWidget {
public:
    /**
     * @brief Construit la barre d'experience dans un etat vide.
     */
    ExperienceBarWidget(void);

    /**
     * @brief Detruit la barre d'experience.
     */
    ~ExperienceBarWidget(void);

    /**
     * @brief Charge les ressources de rendu necessaires.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees par le widget.
     */
    void unload(void);

    /**
     * @brief Dessine la barre d'experience.
     */
    void draw(void) const;

    /**
     * @brief Definit la valeur d'experience actuellement affichee.
     *
     * Les valeurs negatives sont ramenees a `0`.
     *
     * @param value Points d'experience courants a afficher.
     */
    void setCurrentExperiencePoints(int value);

    /**
     * @brief Definit le facteur d'echelle applique a la barre XP.
     *
     * @param scale Facteur d'echelle uniforme du widget.
     */
    void setUiScale(float scale);

    /**
     * @brief Definit le decalage ecran applique a la barre XP.
     *
     * @param offsetX Decalage horizontal en pixels de rendu.
     * @param offsetY Decalage vertical en pixels de rendu.
     */
    void setPositionOffset(float offsetX, float offsetY);

    /**
     * @brief Retourne le decalage ecran courant applique a la barre XP.
     */
    SDL_FPoint getPositionOffset(void) const;

    /**
     * @brief Reinitialise le decalage ecran de la barre XP.
     */
    void resetPositionOffset(void);

    /**
     * @brief Retourne le facteur d'echelle courant de la barre XP.
     */
    float getUiScale(void) const { return this->uiScale; }

    /**
     * @brief Retourne le rectangle courant de la barre XP.
     */
    SDL_FRect getCurrentRect(void) const;

private:
    /**
     * @brief Calcule le rectangle de rendu de la barre.
     *
     * @return Rectangle final de la barre XP.
     */
    SDL_FRect getBarRect(void) const;

    /**
     * @brief Construit le texte a afficher au centre de la barre.
     *
     * @return Libelle formate du type `XP : valeur`.
     */
    std::string buildLabel(void) const;

    RC2D_Font labelFont;           /**< Police du libelle central. */
    int currentExperiencePoints;   /**< Experience actuellement affichee. */
    float uiScale;                 /**< Facteur d'echelle applique au widget. */
    SDL_FPoint positionOffset;     /**< Decalage ecran applique au widget. */
};
