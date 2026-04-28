#pragma once

#include <RC2D/RC2D.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre flottante du HUD pour le compte, l'apparence et la gestion des navires.
 *
 * Le widget expose des API publiques explicites, organisees par onglet, afin
 * que le code gameplay puisse alimenter chaque section sans passer par des
 * enums de collection generiques.
 */
class AccountManagementWidget {
public:
    /**
     * @brief Donnees d'une carte de navire affichee dans un onglet de navires acquis.
     */
    struct ShipEntry {
        std::string name;              /**< Nom affiche dans l'entete de la carte. */
        std::string previewAssetPath;  /**< Chemin d'image direct ou dossier de navire; un dossier charge automatiquement "1.png". */
    };

    /**
     * @brief Donnees reutilisables par les listes d'options des onglets gestion, apparence et entrepot.
     */
    struct OptionEntry {
        std::string name;              /**< Nom affiche dans le picker et dans le panneau. */
        std::string previewAssetPath;  /**< Chemin d'image direct ou dossier de navire; un dossier charge automatiquement "1.png". */
    };

    /**
     * @brief Instantane de progression elite affiche par l'onglet elite.
     */
    struct EliteProgressData {
        int currentPoints;           /**< Nombre actuel de points d'elite du joueur. */
        int nextShipPointsRequired;  /**< Objectif de points pour le prochain navire elite. */
        bool hasNextShip;            /**< True si un autre navire elite reste a obtenir. */
    };

    /**
     * @brief Construit le widget dans un etat vide, pret a etre alimente par le gameplay.
     */
    AccountManagementWidget(void);

    /**
     * @brief Detruit l'instance du widget.
     */
    ~AccountManagementWidget(void);

    /**
     * @brief Remplace entierement les cartes de l'onglet "navires elite acquis".
     * @param ships Nouvelle liste de navires elites acquis.
     */
    void setEliteAcquiredShips(const std::vector<ShipEntry>& ships);

    /**
     * @brief Ajoute une carte a l'onglet "navires elite acquis".
     * @param ship Navire elite a ajouter.
     */
    void addEliteAcquiredShip(const ShipEntry& ship);

    /**
     * @brief Vide l'onglet "navires elite acquis".
     */
    void clearEliteAcquiredShips(void);

    /**
     * @brief Remplace entierement les cartes de l'onglet "navires speciaux acquis".
     * @param ships Nouvelle liste de navires speciaux acquis.
     */
    void setSpecialAcquiredShips(const std::vector<ShipEntry>& ships);

    /**
     * @brief Ajoute une carte a l'onglet "navires speciaux acquis".
     * @param ship Navire special a ajouter.
     */
    void addSpecialAcquiredShip(const ShipEntry& ship);

    /**
     * @brief Vide l'onglet "navires speciaux acquis".
     */
    void clearSpecialAcquiredShips(void);

    /**
     * @brief Remplace entierement les options de bonus du navire dans l'onglet "gestion du navire".
     * @param options Nouvelle liste d'options de bonus du navire.
     */
    void setShipBonusOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de bonus du navire dans l'onglet "gestion du navire".
     * @param option Option de bonus a ajouter.
     */
    void addShipBonusOption(const OptionEntry& option);

    /**
     * @brief Vide les options de bonus du navire dans l'onglet "gestion du navire".
     */
    void clearShipBonusOptions(void);

    /**
     * @brief Remplace entierement les options de style du navire dans l'onglet "apparence".
     * @param options Nouvelle liste de styles du navire.
     */
    void setShipStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de style du navire dans l'onglet "apparence".
     * @param option Style du navire a ajouter.
     */
    void addShipStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de style du navire dans l'onglet "apparence".
     */
    void clearShipStyleOptions(void);

    /**
     * @brief Remplace entierement les options de style de reparation dans l'onglet "apparence".
     * @param options Nouvelle liste de styles de reparation.
     */
    void setRepairStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de style de reparation dans l'onglet "apparence".
     * @param option Style de reparation a ajouter.
     */
    void addRepairStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de style de reparation dans l'onglet "apparence".
     */
    void clearRepairStyleOptions(void);

    /**
     * @brief Remplace entierement les options de style de vitesse dans l'onglet "apparence".
     * @param options Nouvelle liste de styles de vitesse.
     */
    void setSpeedStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de style de vitesse dans l'onglet "apparence".
     * @param option Style de vitesse a ajouter.
     */
    void addSpeedStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de style de vitesse dans l'onglet "apparence".
     */
    void clearSpeedStyleOptions(void);

    /**
     * @brief Remplace entierement les options d'impact des boulets dans l'onglet "apparence".
     * @param options Nouvelle liste de styles d'impact des boulets.
     */
    void setProjectileImpactStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option d'impact des boulets dans l'onglet "apparence".
     * @param option Style d'impact des boulets a ajouter.
     */
    void addProjectileImpactStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options d'impact des boulets dans l'onglet "apparence".
     */
    void clearProjectileImpactStyleOptions(void);

    /**
     * @brief Remplace entierement les options de style de fusee dans l'onglet "apparence".
     * @param options Nouvelle liste de styles de fusee.
     */
    void setRocketStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de style de fusee dans l'onglet "apparence".
     * @param option Style de fusee a ajouter.
     */
    void addRocketStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de style de fusee dans l'onglet "apparence".
     */
    void clearRocketStyleOptions(void);

    /**
     * @brief Remplace entierement les options de style de projectile en vol dans l'onglet "apparence".
     * @param options Nouvelle liste de styles de projectile en vol.
     */
    void setProjectileStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de style de projectile en vol dans l'onglet "apparence".
     * @param option Style de projectile a ajouter.
     */
    void addProjectileStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de style de projectile en vol dans l'onglet "apparence".
     */
    void clearProjectileStyleOptions(void);

    /**
     * @brief Remplace entierement les options de clic de deplacement dans l'onglet "apparence".
     * @param options Nouvelle liste de styles de clic de deplacement.
     */
    void setMoveClickStyleOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option de clic de deplacement dans l'onglet "apparence".
     * @param option Style de clic a ajouter.
     */
    void addMoveClickStyleOption(const OptionEntry& option);

    /**
     * @brief Vide les options de clic de deplacement dans l'onglet "apparence".
     */
    void clearMoveClickStyleOptions(void);

    /**
     * @brief Remplace entierement les emotes de l'onglet "apparence".
     * @param options Nouvelle liste d'emotes.
     */
    void setEmoteOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une emote dans l'onglet "apparence".
     * @param option Emote a ajouter.
     */
    void addEmoteOption(const OptionEntry& option);

    /**
     * @brief Vide les emotes de l'onglet "apparence".
     */
    void clearEmoteOptions(void);

    /**
     * @brief Remplace entierement les options de l'onglet "entrepot / equipe".
     * @param options Nouvelle liste d'equipements proposes.
     */
    void setStorageEquipmentOptions(const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option dans l'onglet "entrepot / equipe".
     * @param option Equipement a ajouter.
     */
    void addStorageEquipmentOption(const OptionEntry& option);

    /**
     * @brief Vide les options de l'onglet "entrepot / equipe".
     */
    void clearStorageEquipmentOptions(void);

    /**
     * @brief Alimente les valeurs affichees pour la categorie cannons (panneau droit, onglet entrepot / equipe).
     *
     * Les chaines sont affichees telles quelles (texte deja formate cote gameplay). Aucun libelle n'est
     * fige dans ces parametres hormis les titres de ligne dans la GUI.
     *
     * @param cannonsEquippedSummary Texte associe a la ligne equipement principal (ex. etat / aucun equipe).
     * @param cannonDamageDisplay Valeur affichee pour les degats des canons.
     * @param cannonCritDamageDisplay Valeur affichee pour les degats critiques des canons.
     * @param cannonCritChanceDisplay Valeur affichee pour la chance de coup critique des canons.
     * @param cannonRangeDisplay Valeur affichee pour la portee des canons.
     * @param cannonReloadDisplay Valeur affichee pour le temps de rechargement des canons.
     */
    void setStorageEquippedCannonStats(
        const std::string& cannonsEquippedSummary,
        const std::string& cannonDamageDisplay,
        const std::string& cannonCritDamageDisplay,
        const std::string& cannonCritChanceDisplay,
        const std::string& cannonRangeDisplay,
        const std::string& cannonReloadDisplay);

    /**
     * @brief Alimente les valeurs affichees pour la categorie voiles (panneau droit, onglet entrepot / equipe).
     *
     * La phrase de coulage est composee dans le widget a partir de @p sailSinkCurrentPoints et
     * @p sailSinkMaxPoints, afin que le denominateur depende du type de voiles sans code en dur dans la GUI.
     *
     * @param speedKnotsDisplay Vitesse en noeuds, texte deja formate (ex. "12" ou "12,5 nds").
     * @param sailSinkCurrentPoints Numerateur (points de coulage / degradation actuels).
     * @param sailSinkMaxPoints Denominateur (seuil avant destruction des voiles). Si <= 0, la ligne de coulage affiche "-".
     */
    void setStorageEquippedSailStats(
        const std::string& speedKnotsDisplay,
        int sailSinkCurrentPoints,
        int sailSinkMaxPoints);

    /**
     * @brief Alimente la ligne resume harponneuse (plus de grille de stats : un seul libelle + cette valeur).
     *
     * @param summary Texte libre affiche a droite de "Harponneuse" (ex. equipement choisi, indisponible, etc.).
     */
    void setStorageEquippedHarpoonSummary(const std::string& summary);

    /**
     * @brief Remplace entierement l'instantane de progression elite affiche dans l'onglet elite.
     * @param progressData Nouvel etat de progression elite a afficher. Le libelle du prochain
     *                     navire elite est genere automatiquement a partir du nombre de cartes
     *                     elite deja acquises.
     */
    void setEliteProgressData(const EliteProgressData& progressData);

    /**
     * @brief Met a jour les points d'elite actuellement affiches dans les onglets compte et elite.
     * @param points Nombre actuel de points d'elite du joueur. Les valeurs negatives sont ramenees a zero.
     *               Les autres champs de progression elite deja configures sont conserves.
     */
    void setElitePointsCurrent(int points);

    /**
     * @brief Met a jour l'identifiant joueur affiche dans l'onglet compte.
     * @param playerIdentifier Identifiant texte affiche a droite de "IDJoueur".
     */
    void setPlayerIdentifier(const std::string& playerIdentifier);

    /**
     * @brief Met a jour la date "Pirate since" affichee dans l'onglet compte.
     * @param pirateSince Texte libre affiche a droite de "Pirate since".
     */
    void setPirateSince(const std::string& pirateSince);

    /**
     * @brief Met a jour le niveau actuellement affiche dans l'onglet compte.
     * @param level Niveau du joueur. Les valeurs negatives sont ramenees a zero.
     */
    void setPlayerLevel(int level);

    /**
     * @brief Met a jour les points d'experience affiches dans l'onglet compte.
     * @param points Total de points d'experience du joueur. Les valeurs negatives sont ramenees a zero.
     */
    void setExperiencePointsCurrent(int points);

    /**
     * @brief Met a jour les points de combat affiches dans l'onglet compte.
     * @param points Total de points de combat du joueur. Les valeurs negatives sont ramenees a zero.
     */
    void setCombatPointsCurrent(int points);

    /**
     * @brief Met a jour la date "Premium depuis" affichee dans l'onglet compte.
     * @param premiumSince Texte libre affiche dans le panneau premium.
     */
    void setPremiumSince(const std::string& premiumSince);

    /**
     * @brief Met a jour le nom de profil editable affiche dans l'onglet compte.
     * @param profileName Nouveau nom de profil. Le texte est tronque a la taille maximale du champ.
     */
    void setProfileName(const std::string& profileName);

    /**
     * @brief Retourne l'instantane de progression elite utilise par l'onglet elite.
     * @return Un instantane contenant les points actuels et l'objectif du prochain navire elite.
     */
    EliteProgressData getEliteProgress(void) const;

    /**
     * @brief Charge les polices, les icones et l'etat runtime du widget.
     */
    void load(void);

    /**
     * @brief Libere les polices, les icones et les ressources runtime mises en cache.
     */
    void unload(void);

    /**
     * @brief Met a jour le drag, le scroll et les interactions continues.
     * @param dt Delta time de la frame en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine le widget complet et l'onglet actuellement actif.
     */
    void draw(void) const;

    /**
     * @brief Gere les interactions de clic souris du widget.
     * @param x Position X de la souris en coordonnees de rendu.
     * @param y Position Y de la souris en coordonnees de rendu.
     * @param button Bouton souris utilise.
     * @param clicks Nombre de clics remonte par SDL.
     * @param mouseID Identifiant SDL de la souris.
     * @return True si l'evenement est consomme par le widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Gere la molette souris pour les listes et pickers actifs.
     * @param direction Direction de la molette.
     * @param wheel_x Delta horizontal de molette en coordonnees flottantes.
     * @param wheel_y Delta vertical de molette en coordonnees flottantes.
     * @param integer_x Delta horizontal de molette en coordonnees entieres.
     * @param integer_y Delta vertical de molette en coordonnees entieres.
     * @param mouse_x Position X de la souris en coordonnees de rendu.
     * @param mouse_y Position Y de la souris en coordonnees de rendu.
     * @param mouseID Identifiant SDL de la souris.
     * @return True si l'evenement est consomme par le widget.
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
     * @brief Gere la saisie clavier du champ de nom de profil.
     * @param key Libelle UTF-8 de la touche remonte par SDL/RC2D.
     * @param scancode Scancode SDL.
     * @param keycode Keycode SDL.
     * @param mod Modificateurs SDL.
     * @param isrepeat True si l'evenement correspond a une repetition automatique.
     * @return True si l'evenement est consomme par le widget.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Retourne si la fenetre est actuellement visible.
     * @return True si le widget est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief True si le champ de nom de profil a le focus clavier.
     */
    bool hasBlockingProfileNameInputFocus(void) const { return this->visible && this->profileInputFocused; }

    /**
     * @brief Active ou desactive la prise de possession du curseur pour la frame courante.
     * @param enabled True pour laisser le widget demander un curseur, false sinon.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point en espace de rendu se trouve dans les bornes du widget.
     * @param x Coordonnee X de rendu a tester.
     * @param y Coordonnee Y de rendu a tester.
     * @return True si le point est dans le rectangle courant du widget.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur voulu par le widget pour un point donne.
     * @param x Coordonnee X de rendu a evaluer.
     * @param y Coordonnee Y de rendu a evaluer.
     * @return Type de curseur demande par le widget.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Efface tous les etats de focus courants et ferme le picker ouvert.
     */
    void clearFocus(void);

    /**
     * @brief Ouvre directement le widget sur l'onglet de gestion du navire.
     */
    void openShipManagement(void);

    /**
     * @brief Masque le widget et nettoie les interactions transitoires.
     */
    void hide(void);

private:
    /**
     * @brief Onglets visibles internes dessines par le widget.
     */
    enum class ActiveTab : int {
        ACCOUNT = 0,           /**< Onglet des informations de compte. */
        APPEARANCE = 1,        /**< Onglet de personnalisation d'apparence. */
        SHIP_MANAGEMENT = 2,   /**< Onglet de gestion du navire. */
        ELITE_SHIPS = 3,       /**< Onglet des navires elite acquis. */
        SPECIAL_SHIPS = 4,     /**< Onglet des navires speciaux acquis. */
        STORAGE_EQUIPPED = 5   /**< Onglet entrepot / equipe. */
    };

    /**
     * @brief Routage interne des pickers utilises par les panneaux cliquables.
     */
    enum class AppearancePickerType : int {
        NONE = 0,              /**< Aucun picker n'est actuellement ouvert. */
        SHIP_BONUS = 1,        /**< Picker de bonus / configuration du navire. */
        SHIP_STYLE = 2,        /**< Picker de style visuel du navire. */
        REPAIR_STYLE = 3,      /**< Picker d'effet de reparation. */
        SPEED_STYLE = 4,       /**< Picker d'effet de vitesse. */
        EXPLOSION_STYLE = 5,   /**< Picker d'impact visuel des boulets / projectiles. */
        ROCKET_STYLE = 6,      /**< Picker d'effet de fusee. */
        PROJECTILE_STYLE = 7,  /**< Picker d'effet du projectile en vol. */
        MOVE_CLICK_STYLE = 8,  /**< Picker d'effet de clic de deplacement. */
        EMOTE = 9,             /**< Picker d'emotes. */
        STORAGE_ITEM_LEFT = 10,/**< Liste deroulante gauche de stockage. */
        STORAGE_ITEM_RIGHT = 11/**< Liste deroulante droite de stockage. */
    };

    /**
     * @brief Collections internes des onglets de navires acquis.
     */
    enum class ShipCollectionType : int {
        ELITE_ACQUIRED = 0,   /**< Donnees de l'onglet "navires elite acquis". */
        SPECIAL_ACQUIRED = 1  /**< Donnees de l'onglet "navires speciaux acquis". */
    };

    /**
     * @brief Collections internes des listes d'options alimentees par le gameplay.
     */
    enum class OptionCollectionType : int {
        SHIP_BONUS = 0,              /**< Options de bonus du navire. */
        SHIP_STYLE = 1,              /**< Options de style visuel du navire. */
        REPAIR_STYLE = 2,            /**< Options de reparation. */
        SPEED_STYLE = 3,             /**< Options de vitesse. */
        PROJECTILE_IMPACT_STYLE = 4, /**< Options d'impact des boulets. */
        ROCKET_STYLE = 5,            /**< Options de fusee. */
        PROJECTILE_STYLE = 6,        /**< Options de projectile en vol. */
        MOVE_CLICK_STYLE = 7,        /**< Options de clic de deplacement. */
        EMOTE = 8,                   /**< Options d'emotes. */
        STORAGE_EQUIPMENT = 9        /**< Options de l'onglet entrepot / equipe. */
    };

    using AppearanceOption = OptionEntry; /**< Alias interne des options de liste. */

    RC2D_Font titleFont;   /**< Police de titre utilisee par les entetes de section. */
    RC2D_Font bodyFont;    /**< Police principale utilisee pour les libelles et valeurs. */
    RC2D_Font smallFont;   /**< Petite police utilisee pour les onglets et libelles compacts. */
    SDL_FRect widgetRect;  /**< Rectangle courant du widget en coordonnees de rendu. */

    bool visible;          /**< True si la fenetre est actuellement visible. */
    bool cursorEnabled;    /**< True si le widget peut demander un changement de curseur. */
    bool resourcesLoaded;  /**< True apres load() quand les polices et les icones sont ouvertes. */
    WindowControlIcons controlIcons; /**< Helper partage pour le rendu des icones de controle. */

    ActiveTab activeTab;   /**< Onglet superieur actuellement selectionne. */

    bool widgetDragging;   /**< True pendant le drag de l'entete de fenetre. */
    float widgetDragOffsetX; /**< Ancrage X entre la souris et l'origine du widget pendant le drag. */
    float widgetDragOffsetY; /**< Ancrage Y entre la souris et l'origine du widget pendant le drag. */
    float widgetOffsetX;   /**< Decalage X applique par l'utilisateur depuis la position centree de base. */
    float widgetOffsetY;   /**< Decalage Y applique par l'utilisateur depuis la position centree de base. */

    std::vector<ShipEntry> eliteShips;    /**< Cartes dynamiques des navires elite. */
    std::vector<ShipEntry> specialShips;  /**< Cartes dynamiques des navires speciaux. */
    std::vector<RC2D_Image> eliteShipIcons;   /**< Textures d'apercu en cache des navires elite. */
    std::vector<RC2D_Image> specialShipIcons; /**< Textures d'apercu en cache des navires speciaux. */
    std::string playerIdentifier;         /**< Identifiant joueur affiche dans l'onglet compte. */
    std::string pirateSinceText;          /**< Date "Pirate since" affichee dans l'onglet compte. */
    int playerLevel;                      /**< Niveau actuellement affiche dans l'onglet compte. */
    int experiencePointsCurrent;          /**< Points d'experience actuellement affiches dans l'onglet compte. */
    EliteProgressData eliteProgressData;  /**< Instantane courant de progression elite reutilise dans l'onglet compte et l'onglet elite. */
    int combatPointsCurrent;              /**< Points de combat actuellement affiches dans l'onglet compte. */
    std::string premiumSinceText;         /**< Date "Premium depuis" affichee dans l'onglet compte. */

    int selectedEliteShip;   /**< Index du navire elite selectionne pour de futures interactions. */
    int selectedSpecialShip; /**< Index du navire special selectionne pour de futures interactions. */
    int eliteFirstRow;       /**< Premiere ligne visible dans la grille scrollable des elites. */
    int specialFirstRow;     /**< Premiere ligne visible dans la grille scrollable des speciaux. */

    bool fleetScrollDragging;   /**< True pendant le drag du thumb de scrollbar de flotte. */
    float fleetScrollDragOffsetY; /**< Offset souris a l'interieur du thumb de scrollbar de flotte. */
    float fleetScrollWheelHighlightSec; /**< Surlignage du pouce flotte apres scroll molette. */

    std::vector<AppearanceOption> shipOptions;             /**< Options de bonus / configuration du navire. */
    std::vector<AppearanceOption> coatingOptions;          /**< Options de style visuel / coating du navire. */
    std::vector<AppearanceOption> repairStyleOptions;      /**< Options d'effet de reparation. */
    std::vector<AppearanceOption> speedStyleOptions;       /**< Options d'effet de vitesse. */
    std::vector<AppearanceOption> explosionStyleOptions;   /**< Options d'impact visuel des projectiles. */
    std::vector<AppearanceOption> rocketStyleOptions;      /**< Options d'effet de fusee. */
    std::vector<AppearanceOption> projectileStyleOptions;  /**< Options d'effet du projectile en vol. */
    std::vector<AppearanceOption> moveClickStyleOptions;   /**< Options d'effet du clic de deplacement. */
    std::vector<AppearanceOption> emoteOptions;            /**< Options d'emotes. */
    std::vector<AppearanceOption> storageEquipmentOptions; /**< Options des listes de stockage. */

    std::vector<RC2D_Image> shipOptionIcons;             /**< Textures d'apercu en cache des bonus / configurations de navire. */
    std::vector<RC2D_Image> coatingOptionIcons;          /**< Textures d'apercu en cache des styles visuels du navire. */
    std::vector<RC2D_Image> repairStyleOptionIcons;      /**< Textures d'apercu en cache des effets de reparation. */
    std::vector<RC2D_Image> speedStyleOptionIcons;       /**< Textures d'apercu en cache des effets de vitesse. */
    std::vector<RC2D_Image> explosionStyleOptionIcons;   /**< Textures d'apercu en cache des impacts de projectiles. */
    std::vector<RC2D_Image> rocketStyleOptionIcons;      /**< Textures d'apercu en cache des effets de fusee. */
    std::vector<RC2D_Image> projectileStyleOptionIcons;  /**< Textures d'apercu en cache des projectiles en vol. */
    std::vector<RC2D_Image> moveClickStyleOptionIcons;   /**< Textures d'apercu en cache des effets de clic de deplacement. */
    std::vector<RC2D_Image> emoteOptionIcons;            /**< Textures d'apercu en cache des emotes. */
    std::vector<RC2D_Image> storageEquipmentOptionIcons; /**< Textures d'apercu en cache des options de stockage. */

    int selectedShipOption;             /**< Index du bonus / de la configuration de navire selectionne. */
    int selectedCoatingOption;          /**< Index du style visuel du navire selectionne. */
    int selectedRepairStyleOption;      /**< Index de l'effet de reparation selectionne. */
    int selectedSpeedStyleOption;       /**< Index de l'effet de vitesse selectionne. */
    int selectedExplosionStyleOption;   /**< Index de l'impact de projectile selectionne. */
    int selectedRocketStyleOption;      /**< Index de l'effet de fusee selectionne. */
    int selectedProjectileStyleOption;  /**< Index de l'effet de projectile en vol selectionne. */
    int selectedMoveClickStyleOption;   /**< Index de l'effet de clic de deplacement selectionne. */
    int selectedEmoteOption;            /**< Index de l'emote selectionnee. */
    int selectedStorageEquipmentOption; /**< Index de l'option de stockage selectionnee. */

    std::string storageEquippedCannonSummary;    /**< Resume equipement canons (panneau entrepot / valeurs). */
    std::string storageEquippedCannonDamage;     /**< Affichage degats canons. */
    std::string storageEquippedCannonCritDamage; /**< Affichage degats critique canons. */
    std::string storageEquippedCannonCritChance; /**< Affichage chance coup critique canons. */
    std::string storageEquippedCannonRange;      /**< Affichage portee canons. */
    std::string storageEquippedCannonReload;     /**< Affichage temps de recharge canons. */
    std::string storageEquippedSailSpeedKnots;   /**< Affichage vitesse en noeuds. */
    int storageEquippedSailSinkCurrent;            /**< Numerateur coulage voiles. */
    int storageEquippedSailSinkMax;                /**< Denominateur coulage voiles (0 = non affichable). */
    std::string storageEquippedHarpoonSummary;     /**< Resume harponneuse (une seule ligne de stats). */

    AppearancePickerType openPicker; /**< Picker actuellement ouvert, si present. */
    SDL_FRect openPickerAnchorRect;  /**< Rectangle d'ancrage utilise pour placer le popup du picker. */
    int pickerFirstRow;              /**< Premiere ligne visible dans le picker courant. */
    bool pickerScrollDragging;       /**< True pendant le drag du thumb de scrollbar du picker. */
    float pickerScrollDragOffsetY;   /**< Offset souris a l'interieur du thumb de scrollbar du picker. */
    float pickerScrollWheelHighlightSec; /**< Surlignage du pouce picker apres scroll molette. */

    std::string profileName;         /**< Nom de profil editable affiche dans l'onglet compte. */
    bool profileInputFocused;        /**< True quand le champ de profil possede le focus clavier. */
    std::size_t profileCursorIndex;  /**< Position du curseur texte dans profileName. */
    bool profileCursorVisible;       /**< Etat de visibilite du curseur clignotant. */
    double profileCursorBlinkElapsed;/**< Temps ecoule depuis le dernier changement du curseur clignotant. */

    /**
     * @brief Libere toutes les images RC2D stockees dans un vecteur de cache.
     * @param icons Vecteur de cache a vider.
     */
    void clearIcons(std::vector<RC2D_Image>& icons);

    /**
     * @brief Libere toutes les images mises en cache et reinitialise les donnees alimentees.
     */
    void clearAllData(void);

    /**
     * @brief Recharge toutes les textures d'apercu des navires a partir des donnees de flotte courantes.
     */
    void loadShipIcons(void);

    /**
     * @brief Recharge toutes les textures d'apercu d'apparence a partir des options courantes.
     */
    void loadAppearanceIcons(void);

    /**
     * @brief Retourne le vecteur de flotte correspondant a une collection interne.
     * @param collection Collection interne de flotte cible.
     * @return Vecteur mutable de flotte, ou nullptr si indisponible.
     */
    std::vector<ShipEntry>* getShipsForCollection(ShipCollectionType collection);

    /**
     * @brief Retourne le vecteur de flotte correspondant a une collection interne.
     * @param collection Collection interne de flotte cible.
     * @return Vecteur immutable de flotte, ou nullptr si indisponible.
     */
    const std::vector<ShipEntry>* getShipsForCollection(ShipCollectionType collection) const;

    /**
     * @brief Retourne le vecteur d'options correspondant a une collection interne.
     * @param collection Collection interne d'options cible.
     * @return Vecteur mutable d'options, ou nullptr si indisponible.
     */
    std::vector<AppearanceOption>* getOptionsForCollection(OptionCollectionType collection);

    /**
     * @brief Retourne le vecteur d'options correspondant a une collection interne.
     * @param collection Collection interne d'options cible.
     * @return Vecteur immutable d'options, ou nullptr si indisponible.
     */
    const std::vector<AppearanceOption>* getOptionsForCollection(OptionCollectionType collection) const;

    /**
     * @brief Retourne l'index selectionne associe a une collection interne d'options.
     * @param collection Collection interne d'options cible.
     * @return Index mutable selectionne, ou nullptr si indisponible.
     */
    int* getSelectedIndexForCollection(OptionCollectionType collection);

    /**
     * @brief Normalise la selection et le scroll apres une modification de donnees de flotte.
     * @param collection Collection de flotte qui vient d'etre mise a jour.
     */
    void syncShipCollectionState(ShipCollectionType collection);

    /**
     * @brief Normalise la selection apres une modification de donnees d'apparence.
     * @param collection Collection d'apparence qui vient d'etre mise a jour.
     */
    void syncOptionCollectionState(OptionCollectionType collection);

    /**
     * @brief Convertit le picker actuellement ouvert vers une collection interne d'options.
     * @param picker Identifiant interne du picker.
     * @return Collection interne correspondante.
     */
    OptionCollectionType getCollectionForPicker(AppearancePickerType picker) const;

    /**
     * @brief Remplace entierement une collection interne de navires acquis.
     * @param collection Collection interne cible.
     * @param ships Nouvelle liste de navires a stocker.
     */
    void setShipsForCollection(ShipCollectionType collection, const std::vector<ShipEntry>& ships);

    /**
     * @brief Ajoute un navire a une collection interne de navires acquis.
     * @param collection Collection interne cible.
     * @param ship Navire a ajouter.
     */
    void addShipToCollection(ShipCollectionType collection, const ShipEntry& ship);

    /**
     * @brief Vide une collection interne de navires acquis.
     * @param collection Collection interne cible.
     */
    void clearShipsForCollection(ShipCollectionType collection);

    /**
     * @brief Remplace entierement une collection interne d'options.
     * @param collection Collection interne cible.
     * @param options Nouvelle liste d'options a stocker.
     */
    void setOptionsForCollection(OptionCollectionType collection, const std::vector<OptionEntry>& options);

    /**
     * @brief Ajoute une option a une collection interne.
     * @param collection Collection interne cible.
     * @param option Option a ajouter.
     */
    void addOptionToCollection(OptionCollectionType collection, const OptionEntry& option);

    /**
     * @brief Vide une collection interne d'options.
     * @param collection Collection interne cible.
     */
    void clearOptionsForCollection(OptionCollectionType collection);

    /**
     * @brief Retourne le nombre de cartes visibles dans un onglet de flotte.
     * @param tab Onglet de flotte cible.
     * @return Nombre de navires reels affiches dans la grille.
     */
    int getVisibleShipCountForTab(ActiveTab tab) const;

    /**
     * @brief Convertit un index visible de grille vers l'index source dans la collection complete.
     * @param tab Onglet de flotte cible.
     * @param visibleIndex Index de la carte dans la grille.
     * @return Index source dans la collection complete, ou -1 si introuvable.
     */
    int getVisibleShipSourceIndexForTab(ActiveTab tab, int visibleIndex) const;

    /**
     * @brief Retourne le vecteur de flotte correspondant a un onglet interne visible.
     * @param tab Identifiant interne de l'onglet.
     * @return Vecteur mutable de flotte, ou nullptr si l'onglet n'utilise pas de donnees de flotte.
     */
    std::vector<ShipEntry>* getShipsForTab(ActiveTab tab);

    /**
     * @brief Retourne le vecteur de flotte correspondant a un onglet interne visible.
     * @param tab Identifiant interne de l'onglet.
     * @return Vecteur immutable de flotte, ou nullptr si l'onglet n'utilise pas de donnees de flotte.
     */
    const std::vector<ShipEntry>* getShipsForTab(ActiveTab tab) const;

    /**
     * @brief Retourne le cache d'icones de navire correspondant a un onglet interne visible.
     * @param tab Identifiant interne de l'onglet.
     * @return Cache mutable d'icones, ou nullptr si l'onglet n'utilise pas de flotte.
     */
    std::vector<RC2D_Image>* getShipIconsForTab(ActiveTab tab);

    /**
     * @brief Retourne le cache d'icones de navire correspondant a un onglet interne visible.
     * @param tab Identifiant interne de l'onglet.
     * @return Cache immutable d'icones, ou nullptr si l'onglet n'utilise pas de flotte.
     */
    const std::vector<RC2D_Image>* getShipIconsForTab(ActiveTab tab) const;

    /**
     * @brief Retourne l'etat de premiere ligne de scroll correspondant a un onglet interne de flotte.
     * @param tab Identifiant interne de l'onglet.
     * @return Etat mutable de premiere ligne, ou nullptr si non applicable.
     */
    int* getFirstRowForTab(ActiveTab tab);

    /**
     * @brief Retourne l'etat de premiere ligne de scroll correspondant a un onglet interne de flotte.
     * @param tab Identifiant interne de l'onglet.
     * @return Etat immutable de premiere ligne, ou nullptr si non applicable.
     */
    const int* getFirstRowForTab(ActiveTab tab) const;

    /**
     * @brief Retourne le vecteur d'options correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Vecteur mutable d'options, ou nullptr si indisponible.
     */
    std::vector<AppearanceOption>* getOptionsForPicker(AppearancePickerType picker);

    /**
     * @brief Retourne le vecteur d'options correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Vecteur immutable d'options, ou nullptr si indisponible.
     */
    const std::vector<AppearanceOption>* getOptionsForPicker(AppearancePickerType picker) const;

    /**
     * @brief Retourne le cache d'icones correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Cache mutable d'icones, ou nullptr si indisponible.
     */
    std::vector<RC2D_Image>* getIconsForPicker(AppearancePickerType picker);

    /**
     * @brief Retourne le cache d'icones correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Cache immutable d'icones, ou nullptr si indisponible.
     */
    const std::vector<RC2D_Image>* getIconsForPicker(AppearancePickerType picker) const;

    /**
     * @brief Retourne l'index selectionne correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Index mutable selectionne, ou nullptr si indisponible.
     */
    int* getSelectedIndexForPicker(AppearancePickerType picker);

    /**
     * @brief Retourne l'index selectionne correspondant au picker actuellement ouvert.
     * @param picker Identifiant interne du picker.
     * @return Index immutable selectionne, ou nullptr si indisponible.
     */
    const int* getSelectedIndexForPicker(AppearancePickerType picker) const;

    /**
     * @brief Ouvre le popup de picker pour un rectangle d'ancrage donne.
     * @param picker Picker a ouvrir.
     * @param anchorRect Rectangle utilise pour placer le popup.
     */
    void openAppearancePickerForRect(AppearancePickerType picker, const SDL_FRect& anchorRect);

    /**
     * @brief Ferme le popup de picker actuellement ouvert.
     */
    void closeAppearancePicker(void);

    /**
     * @brief Retire le focus du champ de saisie du nom de profil.
     */
    void clearProfileInputFocus(void);

    /**
     * @brief Efface tous les focus transitoires possedes par le widget.
     */
    void clearAllFocus(void);
};
