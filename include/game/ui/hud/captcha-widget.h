#pragma once

#include <RC2D/RC2D.h>

#include <functional>
#include <string>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre flottante de verification type captcha.
 *
 * Affiche une sequence de caracteres fournie par le serveur, un champ de saisie
 * pour la reponse du joueur et un bouton de validation. Le gameplay reseau branche
 * @ref setOnValidateRequested pour transmettre la reponse au serveur.
 */
class CaptchaWidget {
public:
    CaptchaWidget(void);
    ~CaptchaWidget(void);

    /**
     * @brief Charge polices, icones de fenetre et positionne le widget au centre de l'ecran de jeu.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees (@ref load).
     */
    void unload(void);

    /**
     * @brief Met a jour le rectangle a l'ecran, le drag du bandeau et le clignotement du curseur texte.
     * @param dt Delta temps en secondes depuis la frame precedente.
     */
    void update(double dt);

    /**
     * @brief Dessine le panneau, les champs et les boutons.
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris (fermeture, focus champ, validation, drag).
     * @param x Position X du clic en coordonnees de rendu.
     * @param y Position Y du clic en coordonnees de rendu.
     * @param button Bouton souris SDL/RC2D.
     * @param clicks Nombre de clics consecutifs SDL.
     * @param mouseID Identifiant de la souris SDL.
     * @return true si le widget consomme l'evenement.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite le clavier lorsque le champ reponse a le focus.
     *
     * Caracteres acceptes : lettres et chiffres (normalises en majuscules pour la saisie).
     * @param key Libelle texte SDL de la touche.
     * @param scancode Scancode physique.
     * @param keycode Keycode logique.
     * @param mod Masque de modificateurs.
     * @param isrepeat true si evenement de repetition automatique.
     * @return true si la touche est absorbee par le widget.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Retire le focus du champ reponse et replie toute selection texte.
     */
    void clearFocus(void);

    /**
     * @brief Indique si la fenetre captcha est affichee.
     * @return true si @ref show a ete appele et @ref hide ne l'a pas masquee depuis.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief True si le champ reponse captcha a le focus clavier.
     */
    bool hasBlockingTextInputFocus(void) const { return this->visible && this->inputFocused; }

    /**
     * @brief Autorise ou non la demande de curseur (@ref getDesiredCursor).
     * @param enabled false pour laisser passer le curseur par defaut du jeu.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point de rendu tombe dans le rectangle du widget.
     * @param x Coordonnee X.
     * @param y Coordonnee Y.
     * @return true si le point est inclus.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Calcule le curseur souhaite pour une position de souris donnee.
     * @param x Coordonnee X de la souris en coordonnees de rendu.
     * @param y Coordonnee Y de la souris en coordonnees de rendu.
     * @return Type de curseur (texte, deplacement, main, etc.).
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Publie le defi affiche au joueur (chaine envoyee par le serveur).
     *
     * Seuls les caracteres alphanumeriques sont conserves ; les lettres sont affichees en majuscules.
     * Met a jour le defi, reinitialise la saisie, reinitialise le chrono et rouvre la fenetre.
     *
     * @param challengeFromServer Texte brut recu du serveur (peut contenir des caracteres filtres).
     */
    void publishCaptchaChallenge(const std::string& challengeFromServer);

    /**
     * @brief Lit la reponse actuellement saisie (apres normalisation majuscules cote saisie).
     * @return Reference stable jusqu'a la prochaine modification du champ ou nouveau defi.
     */
    const std::string& getUserResponse(void) const { return this->userResponseInput; }

    /**
     * @brief Definit la callback declenchee lors du clic sur @em Valider (ou Entree avec focus).
     *
     * Le gameplay doit envoyer la chaine recue au serveur pour verification.
     * Apres la validation, la fenetre se ferme jusqu'a un nouveau @ref show ou @ref publishCaptchaChallenge.
     *
     * @param callback Fonction appelee avec @ref getUserResponse au moment de la validation.
     */
    void setOnValidateRequested(const std::function<void(const std::string&)>& callback);

    /**
     * @brief Reaffiche la fenetre et reinitialise le compte a rebours (3 minutes).
     */
    void show(void);

    /**
     * @brief Masque la fenetre, stoppe le drag et le focus clavier.
     */
    void hide(void);

    /**
     * @brief Longueur maximale du defi serveur memorise pour l'affichage.
     */
    static constexpr std::size_t kMaxChallengeCharacters = 32U;

    /**
     * @brief Longueur maximale de la reponse saisie par le joueur.
     */
    static constexpr std::size_t kMaxUserResponseCharacters = 32U;

private:
    RC2D_Font titleFont; /**< Police titre / defi serveur. */
    RC2D_Font bodyFont;  /**< Police labels et champ reponse. */
    SDL_FRect widgetRect; /**< Rectangle courant en coordonnees de rendu. */

    bool visible;                 /**< Visibilite de la fenetre. */
    bool cursorEnabled;           /**< Autorisation de proposer un curseur. */
    bool resourcesLoaded;         /**< true apres @ref load reussi. */

    std::string challengeDisplay; /**< Texte du defi (filtre, majuscules). */
    std::string userResponseInput; /**< Saisie utilisateur normalisee. */
    std::size_t cursorIndex;         /**< Index d'insertion dans @ref userResponseInput. */
    std::size_t selectionAnchorIndex; /**< Autre borne de selection dans le champ. */
    bool inputFocused;            /**< Focus clavier sur le champ reponse. */
    bool inputSelectingWithMouse; /**< Selection etendue a la souris. */
    bool cursorVisible;           /**< Bit de clignotement du caret. */
    double cursorBlinkElapsed;    /**< Accumulateur pour le blink. */
    double solveTimeRemainingSec; /**< Compte a rebours pour valider (secondes restantes). */

    float widgetDragOffsetX;  /**< Offset souris -> coin haut gauche en drag. */
    float widgetDragOffsetY;  /**< Offset souris -> coin haut gauche en drag. */
    float widgetOffsetX;      /**< Decalage par rapport au centrage ecran. */
    float widgetOffsetY;      /**< Decalage par rapport au centrage ecran. */
    bool widgetDragging;      /**< true pendant un drag du bandeau titre. */

    WindowControlIcons controlIcons; /**< Bouton fermer. */
    std::function<void(const std::string&)> onValidateRequested; /**< Callback validation. */

    /**
     * @brief Normalise un caractere issu d'un evenement clavier pour la reponse.
     * @return Caractere alphanumerique majuscule ou '\\0' si refuse.
     */
    static char extractInputCharacterFromKeyLabel(const char* key);

    /**
     * @brief Filtre et tronque la chaine serveur pour @ref challengeDisplay.
     */
    static std::string sanitizeChallengePayload(const std::string& raw);

    /**
     * @brief Convertit une coordonnee X en index de curseur dans @ref userResponseInput.
     */
    std::size_t getResponseCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const;
    /**
     * @brief Teste si une selection non vide est active dans le champ reponse.
     */
    bool hasResponseSelection(void) const;
    /**
     * @brief Borne inferieure (incluse) de la selection du champ reponse.
     */
    std::size_t getResponseSelectionStart(void) const;
    /**
     * @brief Borne superieure (exclue) de la selection du champ reponse.
     */
    std::size_t getResponseSelectionEnd(void) const;
    /**
     * @brief Replie la selection sur @ref cursorIndex.
     */
    void clearResponseSelection(void);
    /**
     * @brief Supprime les caracteres compris dans la selection courante.
     */
    void deleteSelectedResponseText(void);
    /**
     * @brief Notifie @ref onValidateRequested avec @ref userResponseInput.
     */
    void triggerValidateRequest(void);
};
