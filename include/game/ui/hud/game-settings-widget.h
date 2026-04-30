#pragma once

#include <RC2D/RC2D.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "game/map/map.h"
#include "game/ui/hud/hud-cursor.h"
#include "game/ui/text-input-edit-history.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre flottante des parametres de jeu accessible depuis la top bar.
 *
 * Le widget expose les interactions principales de la GUI de parametres:
 * sous-onglets, champ de code d'activation, configurateur d'interface,
 * reglages de taille du HUD et selecteur de langue base sur l'etat global
 * de langue du client.
 */
class GameSettingsWidget {
public:
    /**
     * @brief Signature de callback pour la demande d'activation d'un code.
     */
    using RedeemCodeCallback = std::function<void(const std::string&)>;

    /**
     * @brief Signature de callback sans argument.
     */
    using VoidCallback = std::function<void(void)>;

    /**
     * @brief Signature de callback pour le changement de langue visible.
     */
    using LanguageChangedCallback = std::function<void(const std::string&)>;

    /**
     * @brief Cibles HUD pouvant etre reduites depuis l'onglet general.
     */
    enum class HudScaleTarget : std::size_t {
        MINIMAP = 0,        /**< Widget minimap. */
        EXPERIENCE_BAR = 1, /**< Barre d'experience. */
        HP_BAR = 2,         /**< Barre de points de vie. */
        MAP_ZOOM = 3,       /**< Barre + slider de zoom map. */
        CENTER_SHIP = 4,    /**< Bouton centrer navire. */
        ACTION_BAR = 5,     /**< Barre d'action. */
        COUNT = 6           /**< Nombre total de cibles pilotables. */
    };

    /**
     * @brief Actions clavier configurables dans l'onglet Controles.
     */
    enum class ControlAction : std::size_t {
        CAMERA_MOVE_UP = 0,              /**< Deplacement camera vers le haut. */
        CAMERA_MOVE_DOWN = 1,            /**< Deplacement camera vers le bas. */
        CAMERA_MOVE_LEFT = 2,            /**< Deplacement camera vers la gauche. */
        CAMERA_MOVE_RIGHT = 3,           /**< Deplacement camera vers la droite. */
        CENTER_CAMERA_ON_SHIP = 4,       /**< Recentre la camera sur le navire joueur. */
        TOOLBAR_ATTACK = 5,              /**< Action Attaquer. */
        TOOLBAR_CANCEL_ATTACK = 6,       /**< Action Annuler l'attaque. */
        TOOLBAR_BOARDING = 7,            /**< Action Abordage. */
        TOOLBAR_REPAIR = 8,              /**< Action Reparer. */
        SHORTCUT_1 = 9,                  /**< Raccourci 1. */
        SHORTCUT_2 = 10,                 /**< Raccourci 2. */
        SHORTCUT_3 = 11,                 /**< Raccourci 3. */
        SHORTCUT_4 = 12,                 /**< Raccourci 4. */
        SHORTCUT_5 = 13,                 /**< Raccourci 5. */
        SHORTCUT_6 = 14,                 /**< Raccourci 6. */
        SHORTCUT_7 = 15,                 /**< Raccourci 7. */
        SHORTCUT_8 = 16,                 /**< Raccourci 8. */
        SHORTCUT_9 = 17,                 /**< Raccourci 9. */
        JUMP_MAP = 18,                   /**< Jump map. */
        FORCE_CLICKED_POSITION_MOVE = 19,/**< Deplacement force vers la position cliquee. */
        TOGGLE_MINIMAP = 20,             /**< Affiche ou masque la minimap. */
        COUNT = 21                       /**< Nombre total d'actions clavier. */
    };

    /**
     * @brief Modes de fenetre proposes dans l'onglet graphiques.
     */
    enum class GraphicsWindowMode : int {
        MAXIMIZED_WINDOW = 0, /**< Fenetre maximisee. */
        FULLSCREEN = 1        /**< Plein ecran exclusif. */
    };

    /**
     * @brief Presets du nombre de projectiles visibles par salve (onglet Graphiques).
     *
     * Valeur persistee dans les parametres utilisateur et reappliquee au chargement.
     * Les effets precis dependent du rendu gameplay / shaders du client.
     */
    enum class SalvoBulletPreset : int {
        LOW = 0,    /**< Charge ou densite de projectiles reduite. */
        NORMAL = 1, /**< Preset equilibre par defaut. */
        HIGH = 2    /**< Charge ou densite de projectiles plus elevee. */
    };

    /**
     * @brief Signature de callback pour le changement de taille d'un widget HUD.
     */
    using HudScaleChangedCallback = std::function<void(HudScaleTarget, float)>;

    /**
     * @brief Signature de callback pour le changement de visibilite d'un widget HUD.
     */
    using HudVisibilityChangedCallback = std::function<void(HudScaleTarget, bool)>;

    /**
     * @brief Construit le widget dans un etat vide.
     */
    GameSettingsWidget(void);

    /**
     * @brief Detruit l'instance du widget.
     */
    ~GameSettingsWidget(void);

    /**
     * @brief Charge les polices, icones et ressources runtime du widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees par le widget.
     */
    void unload(void);

    /**
     * @brief Met a jour le drag, le curseur texte et les interactions continues.
     * @param dt Delta time de frame en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre et le sous-onglet actif.
     */
    void draw(void) const;

    /**
     * @brief Traite un clic souris sur le widget.
     * @param x Position X de rendu.
     * @param y Position Y de rendu.
     * @param button Bouton souris recu.
     * @param clicks Nombre de clics SDL.
     * @param mouseID Identifiant SDL de la souris.
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite la molette souris pour la liste deroulante des langues.
     * @return True si l'evenement est consomme.
     */
    bool mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float wheel_x,
        float wheel_y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID);

    /**
     * @brief Traite le clavier pour le champ de code d'activation.
     * @return True si l'evenement est consomme.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Injecte un texte finalise RC2D/SDL dans le champ de code.
     * @return True si le texte a ete absorbe.
     */
    bool textinput(const char* text);

    /**
     * @brief Retourne true si la fenetre est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief True si le champ de code d'activation a le focus clavier.
     */
    bool hasBlockingRedeemInputFocus(void) const { return this->visible && this->redeemInputFocused; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     * @param enabled True pour autoriser les demandes de curseur.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point de rendu est dans la fenetre courante.
     * @param x Coordonnee X de rendu.
     * @param y Coordonnee Y de rendu.
     * @return True si le point est dans la fenetre.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur desire pour un point de rendu donne.
     * @param x Coordonnee X a evaluer.
     * @param y Coordonnee Y a evaluer.
     * @return Type de curseur souhaite.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Nettoie les focus transitoires et ferme la liste de langues.
     */
    void clearFocus(void);

    /**
     * @brief Rouvre la fenetre sur l'onglet general.
     */
    void show(void);

    /**
     * @brief Ferme la fenetre et arrete les drags en cours.
     */
    void hide(void);

    /**
     * @brief Remplace entierement le contenu du champ de code d'activation.
     * @param value Nouveau texte a memoriser. Le texte est filtre a un format alphanumerique.
     */
    void setRedeemCode(const std::string& value);

    /**
     * @brief Retourne le code d'activation actuellement saisi.
     */
    const std::string& getRedeemCode(void) const;

    /**
     * @brief Enregistre une callback appelee lors du clic sur "Activer".
     * @param callback Callback utilisateur recevant le code saisi.
     */
    void setOnRedeemCodeRequested(RedeemCodeCallback callback);

    /**
     * @brief Enregistre une callback appelee lors du clic sur le configurateur d'interface.
     * @param callback Callback utilisateur sans argument.
     */
    void setOnStartUiConfiguratorRequested(VoidCallback callback);

    /**
     * @brief Enregistre une callback appelee lors d'un changement de langue.
     * @param callback Callback utilisateur recevant le nom de drapeau choisi.
     */
    void setOnLanguageChanged(LanguageChangedCallback callback);

    /**
     * @brief Met a jour la valeur affichee d'un slider de reduction HUD.
     *
     * @param target Widget HUD concerne.
     * @param scale Facteur d'echelle a memoriser.
     */
    void setHudScaleValue(HudScaleTarget target, float scale);

    /**
     * @brief Retourne la valeur courante d'un slider de reduction HUD.
     *
     * @param target Widget HUD concerne.
     * @return Facteur d'echelle memorise pour cette cible.
     */
    float getHudScaleValue(HudScaleTarget target) const;

    /**
     * @brief Enregistre une callback appelee quand un slider HUD change.
     *
     * @param callback Callback recevant la cible modifiee et sa nouvelle valeur.
     */
    void setOnHudScaleChanged(HudScaleChangedCallback callback);

    /**
     * @brief Met a jour l'etat visible memorise d'un widget HUD.
     *
     * @param target Widget HUD concerne.
     * @param visible True si le widget doit etre affiche.
     */
    void setHudVisibilityValue(HudScaleTarget target, bool isVisible);

    /**
     * @brief Retourne l'etat visible memorise pour un widget HUD.
     *
     * @param target Widget HUD concerne.
     * @return True si le widget est actuellement considere comme visible.
     */
    bool getHudVisibilityValue(HudScaleTarget target) const;

    /**
     * @brief Enregistre une callback appelee quand la visibilite d'un widget HUD change.
     *
     * @param callback Callback recevant la cible modifiee et son nouvel etat visible.
     */
    void setOnHudVisibilityChanged(HudVisibilityChangedCallback callback);

    /**
     * @brief Retourne le scancode physique associe a une action configurable.
     *
     * Le scancode reste stable entre claviers, tandis que son libelle visible est
     * resolu avec le layout courant (AZERTY, QWERTY, etc.).
     */
    SDL_Scancode getControlActionScancode(ControlAction action) const;

    /**
     * @brief Retourne le keycode logique associe a une action pour le layout courant.
     */
    SDL_Keycode getControlActionKeycode(ControlAction action) const;

    /**
     * @brief Retourne le libelle visible de la touche associee a une action.
     */
    std::string getControlActionLabel(ControlAction action) const;

    /**
     * @brief Retourne la vitesse de defilement camera en secteurs par seconde.
     */
    float getCameraScrollSpeedSectors(void) const;

    /**
     * @brief Definit la vitesse de defilement camera en secteurs par seconde.
     */
    void setCameraScrollSpeedSectors(float speedSectors);

    /**
     * @brief Retourne true si le fond des coordonnees de map doit etre masque.
     */
    bool getHideCoordinateBackground(void) const;

    /**
     * @brief Retourne true si le brouillard de guerre doit etre anime et dessine.
     */
    bool getFogOfWarEnabled(void) const;

    /**
     * @brief Retourne true si les trainees des navires sur l'ocean doivent etre actives.
     */
    bool getShipWakeTrailsEnabled(void) const;

    /**
     * @brief Indique si les effets visuels des autres joueurs doivent etre masques.
     *
     * Lorsque true, le client peut reduire la charge GPU / visuelle en n'affichant pas
     * (ou en simplifiant) certaines VFX issues des autres navires.
     *
     * @return true si les VFX des autres joueurs sont desactivees ou attenuees.
     */
    bool getHideOtherPlayersVfxEnabled(void) const;

    /**
     * @brief Active ou desactive le masquage / attenuation des VFX des autres joueurs.
     *
     * @param enabled true pour masquer ou attenuer ces effets, false pour les afficher normalement.
     */
    void setHideOtherPlayersVfxEnabled(bool enabled);

    /**
     * @brief Retourne le preset courant pour le nombre de boulets par salve (affichage / gameplay).
     *
     * @return Valeur @ref SalvoBulletPreset memorisee dans le widget.
     */
    SalvoBulletPreset getSalvoBulletPreset(void) const;

    /**
     * @brief Definit le preset "nombre de boulets par salve" et met a jour l'etat interne.
     *
     * @param preset Nouvelle valeur a appliquer (@ref SalvoBulletPreset).
     */
    void setSalvoBulletPreset(SalvoBulletPreset preset);

    /**
     * @brief Retourne les marges % (gauche, droite, bas) reservees hors zone map / ocean.
     */
    MapPlayfieldFrameMarginsPercent getMapPlayfieldFrameMarginsPercent(void) const;

    /**
     * @brief Definit les marges % cadre autour de la zone jouable (0 a 20, pas de 1 %).
     *
     * @param margins Pourcentages entiers ; snap et clamp alignes sur @ref MapSetPlayfieldFrameMarginsPercent.
     * @param notifyUserSettingsChanged Si true et callback enregistree, declenche une sauvegarde disque.
     */
    void setMapPlayfieldFrameMarginsPercent(
        const MapPlayfieldFrameMarginsPercent& margins,
        bool notifyUserSettingsChanged = true);

    /**
     * @brief Active ou desactive le masquage du fond derriere l'affichage des coordonnees de secteur.
     *
     * @param value true pour masquer le fond decoratif, false pour l'afficher.
     */
    void setHideCoordinateBackground(bool value);

    /**
     * @brief Active ou desactive le rendu / l'animation du brouillard de guerre.
     *
     * @param value true pour activer le fog of war, false pour le desactiver (gain de perf possible).
     */
    void setFogOfWarEnabled(bool value);

    /**
     * @brief Active ou desactive les trainees de sillage des navires sur l'eau.
     *
     * @param value true pour afficher les trainees, false pour les couper.
     */
    void setShipWakeTrailsEnabled(bool value);

    /**
     * @brief Associe une touche (scancode physique SDL) a une action configurable.
     *
     * Met a jour la table interne et declenche la callback eventuelle enregistree via
     * @ref setOnUserSettingsChanged.
     * Contrairement au rebind depuis l'onglet Controles, aucune capture clavier ni dialogue
     * de conflit n'est affiche : plusieurs actions peuvent temporairement partager le meme
     * scancode jusqu'a correction par le joueur dans l'UI.
     *
     * @param action Action logique a modifier (@ref ControlAction).
     * @param scancode Scancode SDL a assigner (ex. SDL_SCANCODE_W). Les scancodes que
     *        le widget refuse comme liaison sont simplement ignores (aucune modification).
     */
    void setControlActionScancode(ControlAction action, SDL_Scancode scancode);

    /**
     * @brief Enregistre une callback pour les changements de parametres utilisateur "metier".
     *
     * Appelee lorsqu'une valeur susceptible d'etre ecrite dans `user_settings.json` change
     * (echelles HUD, visibilite, controles, graphiques inclus VFX / salve, etc.), afin que
     * l'overlay ou un autre module declenche une sauvegarde disque. Remplace tout callback
     * precedemment enregistre ; passer une fonction vide pour desinscrire le comportement.
     *
     * @param callback Fonction sans argument invoque apres application d'un reglage persistant.
     */
    void setOnUserSettingsChanged(VoidCallback callback);

private:
    /**
     * @brief Sous-onglets internes de la fenetre.
     */
    enum class SettingsTab : int {
        GENERAL = 0,   /**< Sous-onglet general. */
        CONTROLS = 1,  /**< Sous-onglet controles. */
        GRAPHICS = 2,  /**< Sous-onglet graphiques. */
        SOUNDS = 3     /**< Sous-onglet sons. */
    };

    /**
     * @brief Donnees UI d'une ligne de langue dans la liste deroulante.
     */
    struct LanguageOption {
        std::string flagName; /**< Nom d'asset de drapeau memorise dans le contexte global. */
        std::string label;    /**< Libelle affiche dans le selecteur. */
    };

    RC2D_Font titleFont;      /**< Police de titre du widget. */
    RC2D_Font bodyFont;       /**< Police de corps pour les contenus. */
    RC2D_Font smallFont;      /**< Police compacte pour les sous-onglets et labels. */
    SDL_FRect widgetRect;     /**< Rectangle courant du widget en coordonnees de rendu. */

    bool visible;             /**< True si la fenetre est actuellement visible. */
    bool cursorEnabled;       /**< True si le widget peut demander un curseur HUD. */
    bool resourcesLoaded;     /**< True une fois les ressources chargees. */
    WindowControlIcons controlIcons; /**< Helper des icones de controle. */
    RC2D_Image arrowDownIcon;        /**< Icone de fleche pour la liste deroulante. */
    RC2D_Image checkboxValidIcon;    /**< Icone affichee dans une case cochee. */

    SettingsTab activeTab;    /**< Sous-onglet actuellement affiche. */

    bool widgetDragging;      /**< True pendant le drag du header. */
    float widgetDragOffsetX;  /**< Offset souris -> coin gauche pendant le drag. */
    float widgetDragOffsetY;  /**< Offset souris -> coin haut pendant le drag. */
    float widgetOffsetX;      /**< Decalage X applique a la position centree de base. */
    float widgetOffsetY;      /**< Decalage Y applique a la position centree de base. */

    std::vector<LanguageOption> languageOptions; /**< Langues proposees dans la liste deroulante. */
    int selectedLanguageIndex;                   /**< Index actuellement selectionne. */
    int hoveredLanguageIndex;                    /**< Index survole dans la liste deroulante. */
    int languageFirstRow;                        /**< Premiere ligne visible dans la liste de langues. */
    bool languageDropdownOpen;                   /**< True si la liste deroulante est ouverte. */
    bool languageScrollDragging;                 /**< True pendant le drag du thumb vertical. */
    float languageScrollDragOffsetY;             /**< Offset souris -> thumb pendant le drag. */
    float languageScrollWheelHighlightSec;       /**< Surlignage du thumb langue apres scroll molette. */

    std::string redeemCode;                      /**< Texte actuellement saisi dans le champ de code. */
    bool redeemInputFocused;                     /**< True si l'input de code possede le focus clavier. */
    std::size_t redeemCursorIndex;               /**< Position courante du curseur texte. */
    std::size_t redeemSelectionAnchorIndex;      /**< Point d'ancrage de la selection active dans l'input code. */
    bool redeemInputSelectingWithMouse;          /**< True si un glisser souris etend la selection de l'input code. */
    bool redeemCursorVisible;                    /**< Etat visible du curseur clignotant. */
    double redeemCursorBlinkElapsed;             /**< Temps ecoule depuis le dernier clignotement. */
    TextInputEditHistory redeemEditHistory;      /**< Historique Ctrl+Z du champ redeem. */
    std::array<float, static_cast<std::size_t>(HudScaleTarget::COUNT)> hudScaleValues; /**< Valeurs affichees par les sliders HUD. */
    std::array<bool, static_cast<std::size_t>(HudScaleTarget::COUNT)> hudVisibilityValues; /**< Etats visibles affiches pour chaque widget HUD. */
    bool hudScaleDragging;                       /**< True pendant le drag horizontal d'un slider HUD. */
    HudScaleTarget draggedHudScaleTarget;        /**< Slider HUD actuellement en cours de drag. */
    float hudScaleDragGrabOffsetX;               /**< Offset souris -> thumb du slider HUD. */
    std::array<SDL_Scancode, static_cast<std::size_t>(ControlAction::COUNT)> controlActionScancodes; /**< Touches configurees par action. */
    float cameraScrollSpeedSectors;              /**< Vitesse de defilement camera configuree. */
    float controlsScrollOffsetY;                 /**< Offset vertical de l'onglet controles. */
    bool controlsScrollDragging;                 /**< True pendant le drag de la scrollbar controles. */
    float controlsScrollDragOffsetY;             /**< Offset souris -> thumb de la scrollbar controles. */
    float controlsScrollWheelHighlightSec;     /**< Surlignage du thumb controles apres scroll molette. */
    bool cameraScrollSpeedDragging;              /**< True pendant le drag du slider vitesse camera. */
    float cameraScrollSpeedDragGrabOffsetX;      /**< Offset souris -> thumb du slider vitesse camera. */
    bool controlCaptureActive;                   /**< True si une action attend la prochaine touche clavier. */
    ControlAction capturedControlAction;         /**< Action actuellement en cours de rebind clavier. */
    bool controlConflictPending;                 /**< True si une confirmation de conflit clavier est attendue. */
    ControlAction pendingConflictAction;         /**< Action cible de la confirmation de conflit. */
    ControlAction conflictingControlAction;      /**< Action deja associee a la touche demandee. */
    SDL_Scancode pendingConflictScancode;        /**< Touche demandee pendant la confirmation de conflit. */
    bool graphicsWindowModeDropdownOpen;         /**< True si le selecteur de mode fenetre est ouvert. */
    bool graphicsPresentationModeDropdownOpen;   /**< True si le selecteur letterbox / overscan est ouvert. */
    bool graphicsHideCoordinateBackground;       /**< True si les barres de fond des coordonnees sont masquees. */
    bool graphicsHideOtherPlayersVfx;            /**< True si les VFX des autres joueurs doivent etre masques. */
    float graphicsScrollOffsetY;                 /**< Offset vertical de l'onglet graphiques. */
    bool graphicsScrollDragging;                 /**< True pendant le drag de la scrollbar graphiques. */
    float graphicsScrollDragOffsetY;             /**< Offset souris -> thumb de la scrollbar graphiques. */
    float graphicsScrollWheelHighlightSec;     /**< Surlignage du thumb graphiques apres scroll molette. */
    SalvoBulletPreset graphicsSalvoBulletPreset; /**< Preset selectionne pour "Nombre de boulets par salve". */

    RedeemCodeCallback onRedeemCodeRequested;    /**< Callback appelee lors du clic sur "Activer". */
    VoidCallback onStartUiConfiguratorRequested; /**< Callback appelee lors du clic sur le configurateur UI. */
    LanguageChangedCallback onLanguageChanged;   /**< Callback appelee lors d'un changement de langue. */
    HudScaleChangedCallback onHudScaleChanged;   /**< Callback appelee lors d'un changement de reduction HUD. */
    HudVisibilityChangedCallback onHudVisibilityChanged; /**< Callback appelee lors d'un changement de visibilite HUD. */
    /**
     * @brief Callback optionnelle : changement d'un reglage persistant (voir @ref setOnUserSettingsChanged).
     */
    VoidCallback onUserSettingsChanged;

    /**
     * @brief Charge la liste fixe des langues disponibles dans le widget.
     */
    void rebuildLanguageOptions(void);

    /**
     * @brief Aligne la selection courante sur l'etat global de langue partage.
     */
    void syncSelectedLanguageFromContext(void);

    /**
     * @brief Propulse la selection courante vers le contexte global de langue.
     */
    void applySelectedLanguageToContext(void);

    /**
     * @brief Clamp la premiere ligne visible de la liste des langues.
     */
    void clampLanguageScroll(void);

    /**
     * @brief Replace le scroll pour garder la langue selectionnee visible.
     */
    void scrollLanguageToSelection(void);

    /**
     * @brief Retourne le nombre maximum de lignes visibles dans la liste de langues.
     */
    int getVisibleLanguageRowCount(void) const;

    /**
     * @brief Lance la callback d'activation si un code valide est saisi.
     */
    void triggerRedeemCodeRequest(void);

    /**
     * @brief Lance la callback du configurateur d'interface si disponible.
     */
    void triggerUiConfiguratorRequest(void);

    /**
     * @brief Convertit une position X de rendu en index de curseur pour l'input code.
     * @param renderX Position X de rendu a convertir.
     * @param inputRect Rectangle courant du champ de saisie.
     * @return Index d'insertion le plus proche dans `redeemCode`.
     */
    std::size_t getRedeemCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const;

    /**
     * @brief Indique si une plage de texte est selectionnee dans l'input code.
     * @return True si au moins un caractere est actuellement selectionne.
     */
    bool hasRedeemSelection(void) const;

    /**
     * @brief Retourne le debut inclus de la selection courante dans l'input code.
     * @return Index de debut de selection, deja normalise.
     */
    std::size_t getRedeemSelectionStart(void) const;

    /**
     * @brief Retourne la fin exclue de la selection courante dans l'input code.
     * @return Index de fin de selection, deja normalise.
     */
    std::size_t getRedeemSelectionEnd(void) const;

    /**
     * @brief Replie la selection courante sur la position actuelle du curseur.
     */
    void clearRedeemSelection(void);

    /**
     * @brief Supprime le texte actuellement selectionne dans l'input code.
     */
    void deleteSelectedRedeemText(void);

    /**
     * @brief Applique une valeur de reduction HUD et notifie eventuellement l'exterieur.
     *
     * @param target Widget HUD concerne.
     * @param scale Facteur d'echelle a memoriser.
     * @param notify True pour declencher la callback de sortie.
     */
    void applyHudScaleValue(HudScaleTarget target, float scale, bool notify);

    /**
     * @brief Applique une visibilite HUD et notifie eventuellement l'exterieur.
     *
     * @param target Widget HUD concerne.
     * @param visible True si le widget doit rester visible.
     * @param notify True pour declencher la callback de sortie.
     */
    void applyHudVisibilityValue(HudScaleTarget target, bool isVisible, bool notify);

    /**
     * @brief Met a jour le slider HUD actuellement attrape par la souris.
     *
     * @param mouseX Position X de la souris en coordonnees de rendu.
     */
    void updateDraggedHudScaleFromMouse(float mouseX);

    /**
     * @brief Applique une touche a une action de controle.
     */
    void applyControlActionScancode(ControlAction action, SDL_Scancode scancode);

    /**
     * @brief Cherche une autre action qui utilise deja une touche.
     */
    bool findControlActionUsingScancode(SDL_Scancode scancode, ControlAction excludedAction, ControlAction* outAction) const;

    /**
     * @brief Demarre la confirmation quand une touche est deja utilisee.
     */
    void beginControlConflictConfirmation(ControlAction targetAction, ControlAction conflictAction, SDL_Scancode scancode);

    /**
     * @brief Confirme le remplacement d'une touche deja utilisee.
     */
    void confirmPendingControlConflict(void);

    /**
     * @brief Annule le remplacement d'une touche deja utilisee.
     */
    void cancelPendingControlConflict(void);

    /**
     * @brief Reinitialise une action de controle sur sa touche par defaut.
     */
    void resetControlActionScancode(ControlAction action);

    /**
     * @brief Reinitialise toutes les actions de controle.
     */
    void resetAllControlActionScancodes(void);

    /**
     * @brief Garde le scroll de l'onglet controles dans ses bornes.
     */
    void clampControlsScroll(void);

    /**
     * @brief Met a jour le scroll de l'onglet controles depuis le drag souris.
     */
    void updateControlsScrollFromMouse(float mouseY);

    /**
     * @brief Garde le scroll de l'onglet graphiques dans ses bornes.
     */
    void clampGraphicsScroll(void);

    /**
     * @brief Met a jour le scroll de l'onglet graphiques depuis le drag souris.
     */
    void updateGraphicsScrollFromMouse(float mouseY);

    /**
     * @brief Met a jour le slider de vitesse camera depuis le drag souris.
     */
    void updateDraggedCameraScrollSpeedFromMouse(float mouseX);

    /**
     * @brief Retourne le mode fenetre actuellement applique via RC2D.
     */
    GraphicsWindowMode getGraphicsWindowMode(void) const;

    /**
     * @brief Applique un mode fenetre via RC2D.
     */
    void applyGraphicsWindowMode(GraphicsWindowMode mode);

    /**
     * @brief Retourne le mode de presentation logique RC2D courant.
     */
    RC2D_LogicalPresentationMode getGraphicsPresentationMode(void) const;

    /**
     * @brief Applique le mode de presentation logique RC2D.
     */
    void applyGraphicsPresentationMode(RC2D_LogicalPresentationMode mode);

    /**
     * @brief Applique les marges cadre au runtime map et optionnellement notifie la persistance.
     */
    void applyMapPlayfieldFrameMargins(bool notifyUserSettingsChanged);

    /**
     * @brief Met a jour le slider marges cadre depuis la position souris.
     */
    void updateDraggedMapFrameMarginFromMouse(float mouseX);

    std::array<int, 3> mapPlayfieldFrameMarginPercent; /**< Gauche, droite, bas (0..20, pas 1 %). */
    bool mapFrameMarginDragging;                     /**< Drag d'un slider marges cadre. */
    std::size_t mapFrameMarginDragRow;               /**< 0 gauche, 1 droite, 2 bas. */
    float mapFrameMarginDragGrabOffsetX;             /**< Offset souris -> thumb marges cadre. */
};
