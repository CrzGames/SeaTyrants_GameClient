#pragma once

#include <RC2D/RC2D.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre HUD de gestion d'une Tower de guilde.
 *
 * Le widget reprend la presentation generale de la GUI de mortier, mais il est
 * specialise pour une tour defensive avec points de vie, statistiques de
 * reparation et couts d'amelioration multi-monnaies. Les donnees sont entierement
 * pilotees par le gameplay via des methodes publiques explicites.
 */
class GuildTowerWidget {
public:
    /**
     * @brief Donnees d'un niveau de Tower affichable dans la GUI.
     *
     * Chaque entree represente une tower de guilde concrete avec son nom,
     * son niveau courant et ses statistiques. Les champs de cout d'amelioration
     * correspondent au prix a payer pour atteindre le niveau suivant de cette
     * tower. Une valeur a zero signifie que la monnaie ne doit pas etre affichee.
     */
    struct TowerLevelEntry {
        std::string name;                 /**< Nom de la tower affiche dans la GUI. */
        int level = 1;                    /**< Niveau courant de la tower. */
        int damage = 0;                   /**< Degats infliges par attaque. */
        float attackSpeedSec = 0.0f;      /**< Vitesse d'attaque exprimee en secondes. */
        int attackRange = 0;              /**< Portee d'attaque de la tour. */
        std::int64_t maxHp = 0;           /**< Sante maximale de la tour a ce niveau. */
        std::int64_t currentHp = 0;       /**< Sante courante de la tour a ce niveau. */
        int constructionDurationMinutes = 0; /**< Duree de construction exprimee en minutes pour ce niveau. */
        std::int64_t repairPriceGold = 0; /**< Prix en gold consomme pour declencher la reparation. */
        std::int64_t repairAmountPer5Sec = 0; /**< Quantite de HP reparee toutes les 5 secondes. */
        std::int64_t upgradeGoldCost = 0;     /**< Cout d'amelioration en gold vers ce niveau. */
        std::int64_t upgradeRubiesCost = 0;   /**< Cout d'amelioration en rubies vers ce niveau. */
        std::int64_t upgradePearlsCost = 0;   /**< Cout d'amelioration en perles vers ce niveau. */
        std::int64_t upgradeCrystalsCost = 0; /**< Cout d'amelioration en cristaux vers ce niveau. */
    };

    /**
     * @brief Payload envoye quand l'utilisateur confirme une amelioration.
     */
    struct TowerUpgradeRequest {
        int currentLevelIndex = -1; /**< Index du niveau actuellement actif dans la liste alimentee. */
        int targetLevelIndex = -1;  /**< Index du niveau cible demande. */
        TowerLevelEntry currentLevel; /**< Copie du niveau courant au moment du clic. */
        TowerLevelEntry targetLevel;  /**< Copie du niveau cible demande. */
    };

    using TowerUpgradeRequestedCallback = std::function<void(const TowerUpgradeRequest&)>;
    using RepairAllRequestedCallback = std::function<void(void)>;

    /**
     * @brief Construit la fenetre dans son etat par defaut.
     *
     * La fenetre est visible par defaut afin de servir de base de travail
     * immediate pendant l'integration gameplay.
     */
    GuildTowerWidget(void);

    /**
     * @brief Detruit le widget.
     */
    ~GuildTowerWidget(void);

    /**
     * @brief Charge les polices, icones de controle et images de monnaies.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees par le widget.
     */
    void unload(void);

    /**
     * @brief Met a jour le drag de la fenetre.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre de Tower.
     */
    void draw(void) const;

    /**
     * @brief Traite un clic souris dans la fenetre.
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief La fenetre ne consomme pas la molette pour l'instant.
     * @return False tant qu'aucune interaction molette n'est requise.
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
     * @brief La fenetre ne consomme pas le clavier pour l'instant.
     * @return False tant qu'aucun champ texte n'est present.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Indique si la fenetre bloque les raccourcis gameplay.
     * @return Toujours false tant qu'aucun champ de saisie n'est present.
     */
    bool hasBlockingInputFocus(void) const { return false; }

    /**
     * @brief Indique si la fenetre est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Affiche la fenetre.
     */
    void show(void);

    /**
     * @brief Masque la fenetre.
     */
    void hide(void);

    /**
     * @brief Teste si un point est dans la fenetre.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour une position donnee.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Ajoute un niveau de Tower a la liste.
     * @param entry Niveau complet a ajouter.
     */
    void addTowerLevel(const TowerLevelEntry& entry);

    /**
     * @brief Remplace entierement la liste des niveaux de Tower.
     * @param entries Niveaux a afficher dans la GUI.
     */
    void setTowerLevels(const std::vector<TowerLevelEntry>& entries);

    /**
     * @brief Vide la liste des niveaux de Tower.
     */
    void clearTowerLevels(void);

    /**
     * @brief Selectionne le niveau courant par index.
     * @param index Index dans la liste alimentee ; ignore si invalide.
     */
    void setSelectedTowerLevelIndex(int index);

    /**
     * @brief Met a jour les points de vie courants de la Tower selectionnee.
     *
     * La valeur est automatiquement bornee entre 0 et la sante maximale du
     * niveau actuellement selectionne.
     *
     * @param hp Points de vie courants a afficher.
     */
    void setCurrentHp(std::int64_t hp);

    /**
     * @brief Change le montant de gold disponible dans la tresorerie de guilde.
     * @param amount Nouveau montant affiche. Les valeurs negatives sont ramenees a zero.
     */
    void setTreasuryGoldAmount(std::int64_t amount);

    /**
     * @brief Change le montant de rubies disponible dans la tresorerie de guilde.
     * @param amount Nouveau montant affiche. Les valeurs negatives sont ramenees a zero.
     */
    void setTreasuryRubiesAmount(std::int64_t amount);

    /**
     * @brief Change le montant de perles disponible dans la tresorerie de guilde.
     * @param amount Nouveau montant affiche. Les valeurs negatives sont ramenees a zero.
     */
    void setTreasuryPearlsAmount(std::int64_t amount);

    /**
     * @brief Change le montant de cristaux disponible dans la tresorerie de guilde.
     * @param amount Nouveau montant affiche. Les valeurs negatives sont ramenees a zero.
     */
    void setTreasuryCrystalsAmount(std::int64_t amount);

    /**
     * @brief Definit la callback appelee quand l'utilisateur demande une amelioration.
     * @param callback Fonction recevant le niveau courant et le niveau cible.
     */
    void setOnUpgradeRequested(TowerUpgradeRequestedCallback callback);

    /**
     * @brief Definit la callback appelee quand l'utilisateur demande la reparation globale.
     * @param callback Fonction sans parametre appelee au clic.
     */
    void setOnRepairAllRequested(RepairAllRequestedCallback callback);

private:
    std::vector<TowerLevelEntry> towerLevels; /**< Niveaux de Tower affiches dans la liste. */
    int selectedTowerLevelIndex;              /**< Niveau courant selectionne dans la liste, ou -1 si aucun. */
    std::int64_t treasuryGoldAmount;          /**< Tresorerie gold de la guilde. */
    std::int64_t treasuryRubiesAmount;        /**< Tresorerie rubies de la guilde. */
    std::int64_t treasuryPearlsAmount;        /**< Tresorerie perles de la guilde. */
    std::int64_t treasuryCrystalsAmount;      /**< Tresorerie cristaux de la guilde. */
    TowerUpgradeRequestedCallback onUpgradeRequested; /**< Callback optionnelle de demande d'amelioration. */
    RepairAllRequestedCallback onRepairAllRequested; /**< Callback optionnelle de reparation globale. */

    RC2D_Font titleFont;        /**< Police des titres. */
    RC2D_Font bodyFont;         /**< Police du texte principal. */
    RC2D_Font valueFont;        /**< Police des valeurs numeriques. */
    RC2D_Image goldIcon;        /**< Icone gold. */
    RC2D_Image rubiesIcon;      /**< Icone rubies. */
    RC2D_Image pearlsIcon;      /**< Icone perles. */
    RC2D_Image crystalsIcon;    /**< Icone cristaux. */
    WindowControlIcons controlIcons; /**< Helper des boutons de fenetre. */

    SDL_FRect widgetRect;       /**< Rectangle courant de la fenetre. */
    bool visible;               /**< True si la fenetre est visible. */
    bool widgetDragging;        /**< True pendant le drag du header. */
    float widgetDragOffsetX;    /**< Offset X souris -> fenetre au debut du drag. */
    float widgetDragOffsetY;    /**< Offset Y souris -> fenetre au debut du drag. */
    float widgetOffsetX;        /**< Offset X applique a la position de base. */
    float widgetOffsetY;        /**< Offset Y applique a la position de base. */
    int towerListFirstRow;      /**< Premiere ligne visible de la liste de towers. */
    bool towerListScrollDragging; /**< True pendant le drag de la scrollbar de la liste. */
    float towerListScrollDragOffsetY; /**< Offset souris -> pouce de scrollbar. */

    /**
     * @brief Retourne une copie du niveau actuellement selectionne.
     */
    TowerLevelEntry getSelectedDisplayEntry(void) const;

    /**
     * @brief Construit un apercu du niveau suivant pour la tower selectionnee.
     */
    TowerLevelEntry getUpgradePreviewEntry(void) const;

    /**
     * @brief Repare toutes les towers endommagees du widget.
     */
    void repairAllDamagedTowers(void);

    /**
     * @brief Recale les HP courants selon le niveau selectionne.
     */
    void clampCurrentHpToSelectedLevel(void);
};
