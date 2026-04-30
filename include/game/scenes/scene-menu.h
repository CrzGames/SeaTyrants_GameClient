#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <RC2D/RC2D.h>
#include "game/scenes/scene.h"
#include "game/ui/text-input-edit-history.h"
#include "services/http/types/auth/responses.h"

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

    /**
     * @brief Champ actuellement focalise dans le formulaire de connexion.
     */
    enum class MenuLoginFocusedField {
        NONE = 0,     /**< Aucun champ actif. */
        EMAIL,        /**< Champ e-mail actif. */
        PASSWORD      /**< Champ mot de passe actif. */
    };

    /**
     * @brief Snapshot local du resultat de connexion HTTP partage par le state reseau.
     *
     * La scene copie periodiquement cet etat depuis @ref NetworkState afin de
     * dessiner l'UI sans conserver le mutex reseau pendant le rendu.
     */
    struct MenuAuthUiSnapshot {
        bool signInRequestPending = false;             /**< True pendant l'attente HTTP. */
        bool signInLastRequestSucceeded = false;       /**< True si la derniere reponse est un succes. */
        bool serverSelectionVisible = false;           /**< True si l'overlay serveur doit etre montre. */
        long lastHttpStatusCode = 0;                   /**< Dernier code HTTP brut. */
        std::string lastCode{};                        /**< Dernier code metier backend. */
        std::string lastMessage{};                     /**< Dernier message backend/client. */
        std::vector<AuthSignInHTTPServerEntry> servers{}; /**< Liste des serveurs affichables. */
    };

    /**
     * @brief Etat de selection/caret d'un champ texte du login.
     */
    struct MenuLoginTextSelectionState {
        std::size_t anchorByte = 0U;   /**< Point fixe de la selection (index byte UTF-8). */
        std::size_t caretByte = 0U;    /**< Position courante du caret (index byte UTF-8). */
        bool dragging = false;         /**< True pendant une selection souris en cours. */
    };

    /**
     * @brief Vue calculee d'un champ texte pour rendu et hit-test.
     */
    struct MenuLoginFieldTextView {
        SDL_FRect sourceRect{};                    /**< Rectangle image du champ. */
        SDL_FRect textRect{};                      /**< Zone utile de texte interne. */
        float textStartX = 0.0f;                   /**< Abscisse de depart du texte. */
        std::string displayText{};                 /**< Texte reel ou masque selon le champ. */
        std::vector<std::size_t> rawOffsets{};     /**< Offsets byte UTF-8 du texte reel par codepoint. */
        std::vector<std::size_t> displayOffsets{}; /**< Offsets byte UTF-8 du texte affiche par codepoint. */
    };

    RC2D_Video loginBackgroundVideo;      /**< Background video handle. */
    bool loginBackgroundOpenAttempted;    /**< True once opening was attempted. */

    RC2D_UIImage logoUi;          /**< Top logo image widget. */
    RC2D_UIImage inputEmailUi;    /**< Email field widget image. */
    RC2D_UIImage inputPasswordUi; /**< Password field widget image. */
    RC2D_UIImage buttonLoginUi;   /**< Login button widget image. */

    RC2D_Font menuTitleFont;      /**< Police decorative pour les titres du menu. */
    RC2D_Font menuBodyFont;       /**< Police lisible pour le texte fonctionnel du menu. */

    std::string loginEmailValue;              /**< Texte actuellement saisi dans le champ e-mail. */
    std::string loginPasswordValue;           /**< Texte actuellement saisi dans le champ password. */
    std::string loginEmailFieldErrorMessage;  /**< Erreur locale affichee sous le champ e-mail. */
    std::string loginPasswordFieldErrorMessage; /**< Erreur locale affichee sous le champ mot de passe. */
    MenuLoginFocusedField focusedLoginField;  /**< Champ actuellement focalise pour la saisie. */
    std::string menuLocalStatusMessage;       /**< Message local (validation / action non disponible). */
    bool menuLocalStatusIsError;              /**< Permet de choisir la couleur du message local. */
    MenuAuthUiSnapshot authUiSnapshot;        /**< Copie locale du resultat signin partage. */
    bool authFeedbackPopupVisible;            /**< True quand la popup auth modale doit rester visible. */
    MenuLoginTextSelectionState loginEmailSelectionState;    /**< Selection/caret du champ e-mail. */
    MenuLoginTextSelectionState loginPasswordSelectionState; /**< Selection/caret du champ mot de passe. */
    TextInputEditHistory loginEmailEditHistory;             /**< Historique Ctrl+Z du champ e-mail. */
    TextInputEditHistory loginPasswordEditHistory;          /**< Historique Ctrl+Z du champ mot de passe. */

    std::vector<MenuLanguageFlagEntry> languageFlags; /**< Drapeaux affiches dans le menu. */
    SDL_FRect languageButtonRect;                      /**< Rectangle du bouton langue compact. */
    SDL_FRect languageDropdownRect;                    /**< Rectangle du panneau deroulant. */
    SDL_FRect languageListViewportRect;                /**< Zone clippee des icones de langue. */
    SDL_FRect languageScrollTrackRect;                 /**< Barre de scroll du panneau langue. */
    SDL_FRect languageScrollThumbRect;                 /**< Poignee de scroll du panneau langue. */
    SDL_FRect loginCardRect;                           /**< Rectangle de la carte de login stylisee. */
    SDL_FRect forgotPasswordLinkTextRect;            /**< Zone de dessin du lien mot de passe oublie. */
    SDL_FRect forgotPasswordLinkHitRect;             /**< Zone elargie pour survol / clic sur le lien. */
    bool hoveredForgotPasswordLink;                  /**< True si le lien mot de passe oublie est survole. */
    SDL_FRect authFeedbackPopupRect;                   /**< Panneau principal de la popup auth. */
    SDL_FRect authFeedbackPopupActionButtonRect;       /**< Bouton principal de la popup auth. */
    SDL_FRect serverSelectionOverlayRect;              /**< Panneau principal de selection serveur. */
    SDL_FRect serverSelectionHeaderRect;               /**< Bandeau titre de l'overlay serveur. */
    std::vector<SDL_FRect> serverSelectionRowRects;    /**< Une ligne par serveur visible. */
    std::vector<SDL_FRect> serverSelectionButtonRects; /**< Boutons "Connexion" des lignes. */
    bool hoveredLanguageButton;                        /**< True si le bouton langue est survole. */
    bool hoveredLanguageScrollbar;                     /**< True si la scrollbar langue est survolee. */
    bool hoveredAuthFeedbackPopupActionButton;         /**< True si le bouton popup auth est survole. */
    int hoveredLanguageFlagIndex;                     /**< Drapeau survole ce frame, ou -1. */
    int selectedLanguageFlagIndex;                    /**< Drapeau actuellement selectionne, ou -1. */
    int hoveredServerSelectionButtonIndex;            /**< Bouton serveur survole, ou -1. */
    bool languageDropdownOpen;                        /**< True quand la liste des langues est ouverte. */
    bool languageScrollDragging;                      /**< True pendant le drag du thumb de scroll. */
    float languageScrollWheelHighlightSec;           /**< Surlignage du thumb apres scroll molette. */
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
    static constexpr std::size_t kMaxLoginEmailLength = 128U;     /**< Longueur max du champ e-mail. */
    static constexpr std::size_t kMaxLoginPasswordLength = 128U;  /**< Longueur max du champ password. */

    /**
     * @brief Recopie l'etat de connexion partage depuis le state reseau.
     */
    void refreshAuthUiSnapshotFromNetworkState(void);

    /**
     * @brief Recalcule la geometrie de la popup de feedback auth.
     */
    void rebuildAuthFeedbackOverlayLayout(void);

    /**
     * @brief Recalcule les rectangles de l'overlay de selection des serveurs.
     */
    void rebuildServerSelectionLayout(void);

    /**
     * @brief Dessine les textes saisis dans les deux champs de connexion.
     */
    void drawLoginFieldContents(void);

    /**
     * @brief Dessine les messages de statut sous le formulaire de connexion.
     */
    void drawLoginStatusMessage(void);

    /**
     * @brief Recalcule la position du lien mot de passe oublie (bas gauche de l'aire jeu).
     */
    void layoutForgotPasswordLink(void);

    /**
     * @brief Dessine le lien web mot de passe oublie (bas gauche de l'aire jeu).
     */
    void drawForgotPasswordLink(void);

    /**
     * @brief Dessine les erreurs locales directement sous chaque champ.
     */
    void drawLoginFieldErrors(void);

    /**
     * @brief Dessine la popup modale auth (loader puis erreur).
     */
    void drawAuthFeedbackOverlay(void);

    /**
     * @brief Dessine l'overlay listant les serveurs renvoyes apres signin.
     */
    void drawServerSelectionOverlay(void);

    /**
     * @brief Tente d'ajouter un caractere au champ e-mail.
     * @param character Caractere deja filtre.
     * @return True si le caractere a ete accepte.
     */
    bool appendCharacterToEmailField(char character);

    /**
     * @brief Tente d'ajouter un caractere au champ mot de passe.
     * @param character Caractere deja filtre.
     * @return True si le caractere a ete accepte.
     */
    bool appendCharacterToPasswordField(char character);

    /**
     * @brief Retourne la string editee par un champ login donne.
     */
    std::string* getLoginFieldValue(MenuLoginFocusedField field);

    /**
     * @brief Retourne la string editee par un champ login donne en lecture seule.
     */
    const std::string* getLoginFieldValue(MenuLoginFocusedField field) const;

    /**
     * @brief Retourne l'etat de selection/caret d'un champ login donne.
     */
    MenuLoginTextSelectionState* getLoginFieldSelectionState(MenuLoginFocusedField field);

    /**
     * @brief Retourne l'etat de selection/caret d'un champ login donne en lecture seule.
     */
    const MenuLoginTextSelectionState* getLoginFieldSelectionState(MenuLoginFocusedField field) const;

    /**
     * @brief Construit la vue exploitable d'un champ login pour draw/hit-test.
     */
    bool buildLoginFieldTextView(MenuLoginFocusedField field, MenuLoginFieldTextView* outView) const;

    /**
     * @brief Replie la selection d'un champ sur une position de caret unique.
     */
    void collapseLoginFieldSelection(MenuLoginFocusedField field, std::size_t caretByte);

    /**
     * @brief Annule les drags souris de selection sur les champs login.
     */
    void stopLoginTextSelectionDrag(void);

    /**
     * @brief Efface toutes les selections login en gardant les carets courants.
     */
    void clearAllLoginTextSelections(void);

    /**
     * @brief Retourne true si un champ login a une selection non vide.
     */
    bool hasLoginFieldSelection(MenuLoginFocusedField field) const;

    /**
     * @brief Selectionne tout le contenu du champ login cible.
     */
    void selectAllLoginFieldText(MenuLoginFocusedField field);

    /**
     * @brief Supprime la selection active d'un champ login.
     * @return True si une suppression a eu lieu.
     */
    bool deleteSelectedLoginFieldText(MenuLoginFocusedField field);

    /**
     * @brief Insere un texte au caret du champ login cible.
     * @return True si le texte a ete insere.
     */
    bool insertTextIntoLoginField(MenuLoginFocusedField field, const char* text);

    /**
     * @brief Met le caret d'un champ login selon la souris, avec ou sans extension de selection.
     */
    void placeLoginCaretFromMouse(MenuLoginFocusedField field, float mouseX, bool keepAnchor);

    /**
     * @brief Met a jour le drag de selection souris sur les champs login.
     */
    void updateLoginTextSelectionDrag(void);

    /**
     * @brief Traite une touche de saisie pour le formulaire de connexion.
     * @return True si la touche a ete consommee par le formulaire.
     */
    bool handleLoginInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);

    /**
     * @brief Traite un texte deja compose par SDL/RC2D selon le layout clavier actif.
     */
    void handleLoginTextInput(const char* text);

    /**
     * @brief Efface les erreurs locales affichees sous les champs.
     */
    void clearLoginFieldErrors(void);

    /**
     * @brief Envoie une requete HTTP /signin dans la queue Simulation -> HTTP.
     */
    void submitLoginRequest(void);

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
     * @brief Handle finalized text input from RC2D/SDL.
     */
    void textinput(const RC2D_TextInputEventInfo* info) override;

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
