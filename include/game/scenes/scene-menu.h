#pragma once

#include <string>
#include <vector>

#include <RC2D/RC2D.h>
#include "game/scenes/scene.h"

/**
 * @brief Main menu / login scene.
 *
 * This scene plays a background video, renders login UI widgets,
 * and handles transitions to gameplay.
 */
class MenuScene : public Scene {
private:
    using MenuEaseFunction = double (*)(double);

    /**
     * @brief Etat runtime d'une animation d'apparition pilotee par temps local.
     */
    struct MenuRevealAnimation {
        double startDelay = 0.0;                 /**< Delai de depart voulu en secondes. */
        double remainingDelay = 0.0;             /**< Delai restant avant lancement. */
        double duration = 0.0;                   /**< Duree d'animation totale. */
        double elapsed = 0.0;                    /**< Temps deja ecoule une fois lancee. */
        double startOffset = 0.0;                /**< Offset de depart. */
        double endOffset = 0.0;                  /**< Offset cible. */
        double startAlpha = 0.0;                 /**< Alpha de depart. */
        double endAlpha = 1.0;                   /**< Alpha cible. */
        MenuEaseFunction offsetEaseFunction{};   /**< Courbe d'easing pour l'offset. */
        MenuEaseFunction alphaEaseFunction{};    /**< Courbe d'easing pour l'alpha. */
        bool started = false;                    /**< True une fois l'animation active. */
        float currentOffset = 0.0f;              /**< Valeur courante du decalage. */
        float currentAlpha = 1.0f;               /**< Valeur courante d'opacite [0..1]. */
    };

    /**
     * @brief Entree d'un drapeau affichable dans le selecteur de langue du menu.
     */
    struct MenuLanguageFlagEntry {
        RC2D_Image image{};          /**< Texture du drapeau. */
        std::string assetName{};     /**< Nom sans extension du fichier de drapeau. */
        SDL_FRect drawRect{};        /**< Rectangle courant de draw / hit-test. */
    };

    RC2D_Video loginBackgroundVideo;      /**< Background video handle. */
    bool loginBackgroundOpenAttempted;    /**< True once opening was attempted. */

    RC2D_UIImage logoUi;          /**< Top logo image widget. */
    RC2D_UIImage inputEmailUi;    /**< Email field widget image. */
    RC2D_UIImage inputPasswordUi; /**< Password field widget image. */
    RC2D_UIImage buttonLoginUi;   /**< Login button widget image. */

    std::vector<MenuLanguageFlagEntry> languageFlags; /**< Drapeaux affiches dans le menu. */
    SDL_FRect languageButtonRect;                      /**< Rectangle du bouton langue compact. */
    SDL_FRect languageDropdownRect;                    /**< Rectangle du panneau deroulant. */
    SDL_FRect languageListViewportRect;                /**< Zone clippee des icones de langue. */
    SDL_FRect languageScrollTrackRect;                 /**< Barre de scroll du panneau langue. */
    SDL_FRect languageScrollThumbRect;                 /**< Poignee de scroll du panneau langue. */
    SDL_FRect loginCardRect;                           /**< Rectangle de la carte de login stylisee. */
    bool hoveredLanguageButton;                        /**< True si le bouton langue est survole. */
    bool hoveredLanguageScrollbar;                     /**< True si la scrollbar langue est survolee. */
    int hoveredLanguageFlagIndex;                     /**< Drapeau survole ce frame, ou -1. */
    int selectedLanguageFlagIndex;                    /**< Drapeau actuellement selectionne, ou -1. */
    bool languageDropdownOpen;                        /**< True quand la liste des langues est ouverte. */
    bool languageScrollDragging;                      /**< True pendant le drag du thumb de scroll. */
    float languageScrollOffset;                       /**< Scroll vertical courant du panneau langue. */
    float maxLanguageScrollOffset;                    /**< Scroll max du panneau langue. */
    float languageScrollDragOffsetY;                  /**< Offset souris->thumb pour un drag fluide. */

    MenuRevealAnimation logoReveal;      /**< Apparition du logo. */
    MenuRevealAnimation panelReveal;     /**< Apparition de la carte centrale. */
    MenuRevealAnimation flagsReveal;     /**< Apparition du bandeau de drapeaux. */
    MenuRevealAnimation emailReveal;     /**< Apparition du premier input. */
    MenuRevealAnimation passwordReveal;  /**< Apparition du second input. */
    MenuRevealAnimation buttonReveal;    /**< Apparition du bouton login. */

    MIX_Audio* menuMusic;   /**< Loaded menu audio resource. */
    MIX_Track* menuTrack;   /**< Track used to play menu music. */
    bool menuMusicStarted;  /**< True after first successful playback. */
    double ambientAnimationTime; /**< Temps local pour les animations permanentes. */

    float loginFadeAlpha;                           /**< Intro fade alpha. */
    static constexpr float kLoginFadeSpeed = 0.5f; /**< Intro fade speed. */
    static constexpr float kLogoMarginY = 0.005f;          /**< Marge Y cible du logo. */
    static constexpr float kInputEmailMarginY = 0.45f;     /**< Marge Y cible de l'email. */
    static constexpr float kInputPasswordMarginY = 0.55f;  /**< Marge Y cible du password. */
    static constexpr float kButtonLoginMarginY = 0.65f;    /**< Marge Y cible du bouton. */

    /**
     * @brief Cree les tweens d'apparition du menu principal.
     */
    void resetRevealAnimations(void);

    /**
     * @brief Met a jour un tween d'apparition unique.
     * @param animation Animation a faire progresser.
     * @param dt Delta time de frame.
     */
    void updateRevealAnimation(MenuRevealAnimation* animation, double dt);

    /**
     * @brief Synchronise les marges animees et les rectangles UI courants.
     */
    void syncAnimatedUiState(void);

    /**
     * @brief Recalcule la geometrie courante du selecteur de drapeaux.
     */
    void updateLanguageFlagLayout(void);

    /**
     * @brief Relit le drapeau actuellement selectionne depuis le contexte global.
     */
    void rebuildLanguageSelectionFromContext(void);

    /**
     * @brief Met a jour hover + curseur interactif du menu.
     */
    void updateMenuInteractivity(void);

    /**
     * @brief Met a jour un drag actif de la scrollbar du selecteur de langue.
     */
    void updateLanguageScrollbarDrag(void);

    /**
     * @brief Applique un delta de scroll au panneau des langues.
     * @param deltaPixels Delta vertical en pixels.
     */
    void scrollLanguageDropdown(float deltaPixels);

    /**
     * @brief Replace le scroll pour garder la langue selectionnee visible.
     */
    void snapLanguageScrollToSelection(void);

    /**
     * @brief Dessine la carte decorative derriere les champs de login.
     */
    void drawLoginCard(void);

    /**
     * @brief Dessine le bandeau de drapeaux de langue du menu.
     */
    void drawLanguageSelector(void);

    /**
     * @brief Dessine une image UI avec une opacite dynamique.
     * @param uiImage Image UI a dessiner.
     * @param alpha01 Opacite normalisee [0..1].
     */
    void drawUiImageWithAlpha(RC2D_UIImage* uiImage, float alpha01);

    /**
     * @brief Clamp helper used for alpha values.
     * @param value Input floating point value.
     * @return Value clamped between 0.0 and 1.0.
     */
    static double clamp01(double value);

    /**
     * @brief Draw black fullscreen overlay using alpha.
     * @param alpha01 Alpha ratio in [0..1].
     */
    void drawFullscreenBlackWithAlpha(double alpha01);

    /**
     * @brief Request transition from menu to gameplay scene.
     */
    void goToGameScene(void);

public:
    /**
     * @brief Build a menu scene instance.
     */
    MenuScene(void);

    /**
     * @brief Release menu resources when leaving the scene.
     */
    void unload(void) override;

    /**
     * @brief Prepare menu resources when entering the scene.
     */
    void load(void) override;

    /**
     * @brief Update menu background video, music and fade.
     * @param dt Delta time in seconds.
     */
    void update(double dt) override;

    /**
     * @brief Render menu background and UI elements.
     */
    void draw(void) override;

    /**
     * @brief Handle keyboard interaction in menu.
     * @param key Text key representation from RC2D/SDL.
     * @param scancode Physical keyboard scancode.
     * @param keycode Logical keyboard keycode.
     * @param mod Keyboard modifiers.
     * @param isrepeat True when event is a key repeat.
     * @param keyboardID SDL keyboard device id.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Handle mouse clicks on login UI widgets.
     * @param x Mouse x position in render space.
     * @param y Mouse y position in render space.
     * @param button Mouse button identifier.
     * @param clicks Number of clicks.
     * @param mouseID SDL mouse device id.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;

    /**
     * @brief Handle mouse wheel to scroll the language list when open.
     */
    void mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID) override;
};
