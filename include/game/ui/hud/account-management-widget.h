#pragma once

#include <RC2D/RC2D.h>

#include <array>
#include <cstddef>
#include <functional>
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
    struct AccountTabEliteProgressData {
        int currentPoints;           /**< Nombre actuel de points d'elite du joueur. */
        int nextShipPointsRequired;  /**< Objectif de points pour le prochain navire elite. */
        bool hasNextShip;            /**< True si un autre navire elite reste a obtenir. */
    };

    struct ShipManagementTabOptionEntry {
        std::string name;             /**< Nom affiche dans le picker et dans le panneau. */
        std::string previewAssetPath; /**< Chemin image direct ou dossier; un dossier charge automatiquement "1.png". */
    };

    struct AppearanceTabOptionEntry {
        std::string name;             /**< Nom affiche dans le picker et dans le panneau. */
        std::string previewAssetPath; /**< Chemin image direct ou dossier; un dossier charge automatiquement "1.png". */
    };

    struct EliteShipsTabShipEntry {
        std::string name;             /**< Nom affiche dans l'entete de la carte. */
        std::string previewAssetPath; /**< Chemin image direct ou dossier de navire; un dossier charge automatiquement "1.png". */
    };

    struct SpecialShipsTabShipEntry {
        std::string name;             /**< Nom affiche dans l'entete de la carte. */
        std::string previewAssetPath; /**< Chemin image direct ou dossier de navire; un dossier charge automatiquement "1.png". */
    };

    enum class StorageTabTransferLocation : int {
        WAREHOUSE = 0, /**< Colonne "Entrepot". */
        SHIP = 1       /**< Colonne "Equipe" / navire. */
    };

    enum class StorageTabEquipmentCategory : int {
        CANNONS = 0, /**< Canons. */
        SAILS = 1    /**< Voiles. */
    };

    struct StorageTabEquipmentOptionEntry {
        StorageTabEquipmentCategory category = StorageTabEquipmentCategory::CANNONS; /**< Categorie associee a l'option. */
        std::string name;             /**< Nom affiche dans les listes. */
        std::string previewAssetPath; /**< Chemin image direct ou dossier; un dossier charge automatiquement "1.png". */
        int maxEquippedOnShip = 0;    /**< Quantite maximale equipable sur le navire pour cette categorie; <= 0 = illimite. */
    };

    struct StorageTabCannonStatsDisplay {
        std::string damageDisplay;     /**< Degats des canons. */
        std::string critDamageDisplay; /**< Degats critiques des canons. */
        std::string critChanceDisplay; /**< Chance de coup critique des canons. */
        std::string rangeDisplay;      /**< Portee des canons. */
        std::string reloadDisplay;     /**< Temps de recharge des canons. */
    };

    struct StorageTabItemEntry {
        StorageTabEquipmentCategory category = StorageTabEquipmentCategory::CANNONS; /**< Categorie enum de l'objet. */
        std::string name;             /**< Nom affiche dans la ligne. */
        std::string previewAssetPath; /**< Chemin image direct; un dossier charge automatiquement "1.png". */
        int quantity = 0;             /**< Quantite disponible dans l'emplacement. */
        StorageTabCannonStatsDisplay cannonStats; /**< Stats tooltip si @ref category vaut CANNONS. */
    };

    struct StorageTabTransferRequest {
        StorageTabTransferLocation source; /**< Emplacement de depart. */
        StorageTabTransferLocation target; /**< Emplacement d'arrivee. */
        StorageTabItemEntry item;          /**< Copie de l'objet transfere. */
        int quantity;                      /**< Quantite confirmee par l'utilisateur. */
    };

    using StorageTabTransferCallback = std::function<void(const StorageTabTransferRequest&)>;

    // Onglet Gestion de butin d'abordage.
    /**
     * @brief Types de monnaies affichables dans la gestion du butin d'abordage.
     */
    enum class BoardingLootCurrencyType : int {
        GOLD = 0,     /**< Or gagne ou transporte pendant l'abordage. */
        PERLES = 1,   /**< Perles gagnees ou transportees pendant l'abordage. */
        CRISTAUX = 2  /**< Cristaux gagnes ou transportes pendant l'abordage. */
    };

    /**
     * @brief Ligne de monnaie affichee dans les colonnes "Sur le navire" et "Reserve securisee".
     */
    struct BoardingLootCurrencyEntry {
        BoardingLootCurrencyType type = BoardingLootCurrencyType::GOLD; /**< Type enum de la monnaie. */
        std::string previewAssetPath;   /**< Chemin de l'image affichee pour representer la monnaie. */
        int shipQuantity = 0;           /**< Quantite actuellement stockee sur le navire, donc exposable au pillage. */
        int secureReserveQuantity = 0;  /**< Quantite actuellement stockee dans la reserve securisee, non pillable. */
        int minQuantityToSecureReserve = 0; /**< Quantite minimale sur le navire requise pour transferer vers la reserve securisee. */
        bool forceStoreOnShip = false;  /**< True si cette monnaie doit rester sur le navire et ne peut pas etre securisee. */
    };

    /**
     * @brief Monnaie effectivement deplacee par l'action "Tout vers reserve securisee".
     */
    struct BoardingLootTransferEntry {
        BoardingLootCurrencyType type = BoardingLootCurrencyType::GOLD; /**< Type enum de la monnaie transferee. */
        int quantity = 0; /**< Quantite deplacee du navire vers la reserve securisee. */
    };

    /**
     * @brief Payload envoye au gameplay apres le transfert global du butin eligible.
     */
    struct BoardingLootTransferAllToSecureReserveRequest {
        std::vector<BoardingLootTransferEntry> currencies; /**< Monnaies effectivement deplacees par l'action globale. */
    };

    using BoardingLootTransferAllCallback =
        std::function<void(const BoardingLootTransferAllToSecureReserveRequest&)>;

    /**
     * @brief Construit le widget dans un etat vide, pret a etre alimente par le gameplay.
     */
    AccountManagementWidget(void);

    /**
     * @brief Detruit l'instance du widget.
     */
    ~AccountManagementWidget(void);

    // Onglet Compte.
    void setAccountTabPlayerIdentifier(const std::string& playerIdentifier);
    void setAccountTabPirateSince(const std::string& pirateSince);
    void setAccountTabPlayerLevel(int level);
    void setAccountTabExperiencePointsCurrent(int points);
    void setAccountTabEliteProgressData(const AccountTabEliteProgressData& progressData);
    void setAccountTabElitePointsCurrent(int points);
    void setAccountTabCombatPointsCurrent(int points);
    void setAccountTabPremiumSince(const std::string& premiumSince);
    void setAccountTabProfileName(const std::string& profileName);
    AccountTabEliteProgressData getAccountTabEliteProgress(void) const;

    // Onglet Gestion du navire.
    void setShipManagementTabBonusOptions(const std::vector<ShipManagementTabOptionEntry>& options);
    void addShipManagementTabBonusOption(const ShipManagementTabOptionEntry& option);
    void clearShipManagementTabBonusOptions(void);

    // Onglet Apparence.
    void setAppearanceTabShipStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabShipStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabShipStyleOptions(void);
    void setAppearanceTabRepairStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabRepairStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabRepairStyleOptions(void);
    void setAppearanceTabSpeedStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabSpeedStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabSpeedStyleOptions(void);
    void setAppearanceTabProjectileImpactStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabProjectileImpactStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabProjectileImpactStyleOptions(void);
    void setAppearanceTabRocketStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabRocketStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabRocketStyleOptions(void);
    void setAppearanceTabProjectileStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabProjectileStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabProjectileStyleOptions(void);
    void setAppearanceTabMoveClickStyleOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabMoveClickStyleOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabMoveClickStyleOptions(void);
    void setAppearanceTabEmoteOptions(const std::vector<AppearanceTabOptionEntry>& options);
    void addAppearanceTabEmoteOption(const AppearanceTabOptionEntry& option);
    void clearAppearanceTabEmoteOptions(void);

    // Onglet Navires elite acquis.
    void setEliteShipsTabAcquiredShips(const std::vector<EliteShipsTabShipEntry>& ships);
    void addEliteShipsTabAcquiredShip(const EliteShipsTabShipEntry& ship);
    void clearEliteShipsTabAcquiredShips(void);

    // Onglet Navires speciaux acquis.
    void setSpecialShipsTabAcquiredShips(const std::vector<SpecialShipsTabShipEntry>& ships);
    void addSpecialShipsTabAcquiredShip(const SpecialShipsTabShipEntry& ship);
    void clearSpecialShipsTabAcquiredShips(void);

    // Onglet Entrepot / Equipe.
    void clearStorageTabEquipmentOptions(void);
    void setStorageTabEquipmentCategoryOptions(const std::vector<StorageTabEquipmentOptionEntry>& options);
    void addStorageTabEquipmentCategoryOption(const StorageTabEquipmentOptionEntry& option);
    void setStorageTabWarehouseItems(const std::vector<StorageTabItemEntry>& items);
    void addStorageTabWarehouseItem(const StorageTabItemEntry& item);
    void clearStorageTabWarehouseItems(void);
    void setStorageTabEquippedItems(const std::vector<StorageTabItemEntry>& items);
    void addStorageTabEquippedItem(const StorageTabItemEntry& item);
    void clearStorageTabEquippedItems(void);
    void setStorageTabOnTransferRequested(StorageTabTransferCallback callback);

    // Onglet Gestion de butin d'abordage.
    /**
     * @brief Remplace toutes les monnaies affichees dans l'onglet de butin d'abordage.
     * @param currencies Liste complete des monnaies et quantites a afficher.
     */
    void setBoardingLootManagementTabCurrencies(const std::vector<BoardingLootCurrencyEntry>& currencies);

    /**
     * @brief Ajoute une monnaie a l'onglet de butin d'abordage.
     * @param currency Monnaie et quantites a ajouter.
     */
    void addBoardingLootManagementTabCurrency(const BoardingLootCurrencyEntry& currency);

    /**
     * @brief Vide toutes les monnaies affichees dans l'onglet de butin d'abordage.
     */
    void clearBoardingLootManagementTabCurrencies(void);

    /**
     * @brief Definit la callback appelee apres "Tout vers reserve securisee".
     * @param callback Fonction appelee avec les monnaies effectivement transferees.
     */
    void setBoardingLootManagementTabOnTransferAllToSecureReserveRequested(
        BoardingLootTransferAllCallback callback);

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
        STORAGE_EQUIPPED = 1,  /**< Onglet entrepot / equipe. */
        BOARDING_LOOT = 2,     /**< Onglet de gestion du butin d'abordage. */
        SHIP_MANAGEMENT = 3,   /**< Onglet de gestion du navire. */
        APPEARANCE = 4,        /**< Onglet de personnalisation d'apparence. */
        ELITE_SHIPS = 5,       /**< Onglet des navires elite acquis. */
        SPECIAL_SHIPS = 6      /**< Onglet des navires speciaux acquis. */
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

    struct InternalShipEntry {
        std::string name;
        std::string previewAssetPath;
    };

    struct InternalOptionEntry {
        std::string name;
        std::string previewAssetPath;
    };

    using AppearanceOption = InternalOptionEntry; /**< Alias interne des options de liste. */

    /**
     * @brief Etat de glisser-deposer d'un objet de stockage.
     */
    struct StorageDragState {
        bool active;
        StorageTabTransferLocation source;
        int sourceIndex;
        int maxQuantity;
        StorageTabEquipmentCategory category;
        std::string itemName;
        std::string previewAssetPath;
        float pressX;
        float pressY;
    };

    /**
     * @brief Etat du popup de confirmation de transfert.
     */
    struct StorageTransferPopupState {
        bool open;
        StorageTabTransferLocation source;
        StorageTabTransferLocation target;
        int sourceIndex;
        int maxQuantity;
        int quantity;
        StorageTabEquipmentCategory category;
        std::string itemName;
        std::string previewAssetPath;
    };

    RC2D_Font titleFont;   /**< Police de titre utilisee par les entetes de section. */
    RC2D_Font bodyFont;    /**< Police principale utilisee pour les libelles et valeurs. */
    RC2D_Font smallFont;   /**< Petite police utilisee pour les onglets et libelles compacts. */
    SDL_FRect widgetRect;  /**< Rectangle courant du widget en coordonnees de rendu. */

    bool visible;          /**< True si la fenetre est actuellement visible. */
    bool cursorEnabled;    /**< True si le widget peut demander un changement de curseur. */
    bool resourcesLoaded;  /**< True apres load() quand les polices et les icones sont ouvertes. */
    WindowControlIcons controlIcons; /**< Helper partage pour le rendu des icones de controle. */
    RC2D_Image storageDropdownArrowImage; /**< Fleche listes deroulantes entrepot / equipe (icon-arrowdown). */

    ActiveTab activeTab;   /**< Onglet superieur actuellement selectionne. */

    bool widgetDragging;   /**< True pendant le drag de l'entete de fenetre. */
    float widgetDragOffsetX; /**< Ancrage X entre la souris et l'origine du widget pendant le drag. */
    float widgetDragOffsetY; /**< Ancrage Y entre la souris et l'origine du widget pendant le drag. */
    float widgetOffsetX;   /**< Decalage X applique par l'utilisateur depuis la position centree de base. */
    float widgetOffsetY;   /**< Decalage Y applique par l'utilisateur depuis la position centree de base. */

    std::vector<InternalShipEntry> eliteShips;    /**< Cartes dynamiques des navires elite. */
    std::vector<InternalShipEntry> specialShips;  /**< Cartes dynamiques des navires speciaux. */
    std::vector<RC2D_Image> eliteShipIcons;   /**< Textures d'apercu en cache des navires elite. */
    std::vector<RC2D_Image> specialShipIcons; /**< Textures d'apercu en cache des navires speciaux. */
    std::string playerIdentifier;         /**< Identifiant joueur affiche dans l'onglet compte. */
    std::string pirateSinceText;          /**< Date "Pirate since" affichee dans l'onglet compte. */
    int playerLevel;                      /**< Niveau actuellement affiche dans l'onglet compte. */
    int experiencePointsCurrent;          /**< Points d'experience actuellement affiches dans l'onglet compte. */
    AccountTabEliteProgressData eliteProgressData;  /**< Instantane courant de progression elite reutilise dans l'onglet compte et l'onglet elite. */
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
    std::vector<StorageTabEquipmentCategory> storageEquipmentOptionCategories; /**< Categories enum paralleles aux options de stockage. */
    std::vector<int> storageEquipmentOptionMaxEquipped; /**< Capacites max equipables sur le navire par categorie de stockage. */
    std::vector<StorageTabItemEntry> storageWarehouseItems;   /**< Items affiches dans la colonne Entrepot. */
    std::vector<StorageTabItemEntry> storageEquippedItems;    /**< Items affiches dans la colonne Equipe. */
    std::vector<BoardingLootCurrencyEntry> boardingLootCurrencies; /**< Monnaies affichees dans l'onglet de butin d'abordage. */

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
    std::vector<RC2D_Image> storageWarehouseItemIcons;   /**< Textures d'apercu en cache des items entrepot. */
    std::vector<RC2D_Image> storageEquippedItemIcons;    /**< Textures d'apercu en cache des items equipes. */
    std::vector<RC2D_Image> boardingLootCurrencyIcons;   /**< Textures d'apercu en cache des monnaies de butin. */

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

    StorageDragState storageDrag;                  /**< Etat courant de drag entre entrepot et equipe. */
    StorageTransferPopupState storageTransferPopup; /**< Popup de quantite pour le transfert courant. */
    StorageTabTransferCallback storageTabOnTransferRequested; /**< Callback optionnelle apres confirmation de transfert. */
    BoardingLootTransferAllCallback boardingLootOnTransferAllToSecureReserveRequested; /**< Callback optionnelle apres transfert global du butin. */
    int boardingLootFirstRow;                     /**< Premiere monnaie visible dans l'onglet de butin. */
    bool boardingLootScrollDragging;              /**< True pendant le drag du thumb de scrollbar de butin. */
    float boardingLootScrollDragOffsetY;          /**< Offset souris a l'interieur du thumb de scrollbar de butin. */
    float boardingLootScrollWheelHighlightSec;    /**< Surlignage du pouce butin apres scroll molette. */

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
     * @brief Recharge les textures des objets de l'onglet entrepot / equipe.
     */
    void loadStorageItemIcons(void);

    /**
     * @brief Recharge les textures des monnaies de butin d'abordage.
     */
    void loadBoardingLootCurrencyIcons(void);

    /**
     * @brief Retourne le vecteur de flotte correspondant a une collection interne.
     * @param collection Collection interne de flotte cible.
     * @return Vecteur mutable de flotte, ou nullptr si indisponible.
     */
    std::vector<InternalShipEntry>* getShipsForCollection(ShipCollectionType collection);

    /**
     * @brief Retourne le vecteur de flotte correspondant a une collection interne.
     * @param collection Collection interne de flotte cible.
     * @return Vecteur immutable de flotte, ou nullptr si indisponible.
     */
    const std::vector<InternalShipEntry>* getShipsForCollection(ShipCollectionType collection) const;

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
     * @brief Retourne le nom de la categorie de stockage selectionnee.
     */
    StorageTabEquipmentCategory getSelectedStorageTabEquipmentCategory(void) const;

    /**
     * @brief Retourne la quantite deja equipee pour une categorie.
     */
    int getStorageEquippedQuantityForCategory(StorageTabEquipmentCategory category) const;

    /**
     * @brief Retourne le maximum equipable sur le navire pour une categorie, ou 0 si illimite.
     */
    int getStorageMaxEquippedForCategory(StorageTabEquipmentCategory category) const;

    /**
     * @brief Retourne la capacite restante equipable pour une categorie.
     */
    int getStorageRemainingEquipCapacityForCategory(StorageTabEquipmentCategory category) const;

    /**
     * @brief True si un item du depot ne peut plus etre glisse vers le navire.
     */
    bool isStorageWarehouseItemDisabledForEquip(const StorageTabItemEntry& item) const;

    /**
     * @brief Teste si un item correspond a la categorie de stockage selectionnee.
     */
    bool isStorageItemVisibleForSelectedCategory(const StorageTabItemEntry& item) const;

    /**
     * @brief Retourne le vecteur d'items correspondant a un emplacement.
     */
    std::vector<StorageTabItemEntry>* getStorageItemsForLocation(StorageTabTransferLocation location);

    /**
     * @brief Retourne le vecteur d'items correspondant a un emplacement.
     */
    const std::vector<StorageTabItemEntry>* getStorageItemsForLocation(StorageTabTransferLocation location) const;

    /**
     * @brief Retourne le cache d'icones correspondant a un emplacement.
     */
    std::vector<RC2D_Image>* getStorageItemIconsForLocation(StorageTabTransferLocation location);

    /**
     * @brief Retourne le cache d'icones correspondant a un emplacement.
     */
    const std::vector<RC2D_Image>* getStorageItemIconsForLocation(StorageTabTransferLocation location) const;

    /**
     * @brief Retourne l'index source d'un item touche dans une liste filtree.
     */
    int hitTestStorageItem(
        StorageTabTransferLocation location,
        const SDL_FRect& contentRect,
        float x,
        float y,
        bool requireDraggable = false) const;

    /**
     * @brief Ouvre le popup de transfert depuis le drag courant.
     */
    void openStorageTransferPopupFromDrag(StorageTabTransferLocation target);

    /**
     * @brief Ferme le popup de transfert.
     */
    void closeStorageTransferPopup(void);

    /**
     * @brief Annule le drag de stockage courant.
     */
    void cancelStorageDrag(void);

    /**
     * @brief Applique le transfert confirme dans les donnees UI et notifie la callback.
     */
    void applyStorageTransferPopup(void);

    /**
     * @brief Deplace toutes les monnaies eligibles du navire vers la reserve securisee.
     */
    void transferAllBoardingLootToSecureReserve(void);

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
    void setShipsForCollection(ShipCollectionType collection, const std::vector<InternalShipEntry>& ships);

    /**
     * @brief Ajoute un navire a une collection interne de navires acquis.
     * @param collection Collection interne cible.
     * @param ship Navire a ajouter.
     */
    void addShipToCollection(ShipCollectionType collection, const InternalShipEntry& ship);

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
    void setOptionsForCollection(OptionCollectionType collection, const std::vector<InternalOptionEntry>& options);

    /**
     * @brief Ajoute une option a une collection interne.
     * @param collection Collection interne cible.
     * @param option Option a ajouter.
     */
    void addOptionToCollection(OptionCollectionType collection, const InternalOptionEntry& option);

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
    std::vector<InternalShipEntry>* getShipsForTab(ActiveTab tab);

    /**
     * @brief Retourne le vecteur de flotte correspondant a un onglet interne visible.
     * @param tab Identifiant interne de l'onglet.
     * @return Vecteur immutable de flotte, ou nullptr si l'onglet n'utilise pas de donnees de flotte.
     */
    const std::vector<InternalShipEntry>* getShipsForTab(ActiveTab tab) const;

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
