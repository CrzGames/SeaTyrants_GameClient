#pragma once

#include <RC2D/RC2D.h>

#include <cstdint>
#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre HUD de gestion du mortier de guilde.
 *
 * Le widget affiche l'etat courant du mortier, la liste des niveaux disponibles,
 * le coffre de l'unite et la tresorerie de guilde. Les donnees sont alimentees
 * dynamiquement par le gameplay via les methodes publiques.
 */
class GuildMortarWidget {
public:
    /**
     * @brief Donnees d'un niveau de mortier affichable dans la liste.
     */
    struct MortarLevelEntry {
        std::string name;       /**< Nom du niveau affiche dans la GUI. */
        int damage = 0;         /**< Degats infliges par tir. */
        float attackSpeedSec = 0.0f; /**< Vitesse d'attaque exprimee en secondes. */
        int attackRange = 0;    /**< Portee d'attaque du mortier. */
        std::int64_t shotCostGold = 0; /**< Cout en gold consomme a chaque tir. */
        std::int64_t unitChestGoldAmount = 0; /**< Gold stocke dans le coffre de ce mortier. */
        std::int64_t upgradeRubiesCost = 0; /**< Cout en rubies pour ameliorer vers ce niveau. */
        std::int64_t upgradePearlsCost = 0; /**< Cout en perles pour ameliorer vers ce niveau. */
    };

    /**
     * @brief Construit la fenetre dans son etat par defaut.
     */
    GuildMortarWidget(void);

    /**
     * @brief Detruit le widget.
     */
    ~GuildMortarWidget(void);

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
     * @brief Dessine la fenetre de mortier de guilde.
     */
    void draw(void) const;

    /**
     * @brief Traite un clic souris dans la fenetre.
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite la molette quand la liste deroulante des mortiers est ouverte.
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
     * @brief Traite la saisie du montant d'or a transferer.
     * @return True si l'evenement clavier est consomme.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Indique si le champ de transfert bloque les raccourcis gameplay.
     */
    bool hasBlockingTransferInputFocus(void) const { return this->transferGoldInputFocused; }

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
     * @brief Ajoute un niveau de mortier a la liste.
     * @param entry Niveau de mortier a ajouter.
     */
    void addMortarLevel(const MortarLevelEntry& entry);

    /**
     * @brief Remplace entierement la liste des niveaux de mortier.
     * @param entries Niveaux de mortier a afficher.
     */
    void setMortarLevels(const std::vector<MortarLevelEntry>& entries);

    /**
     * @brief Vide la liste des niveaux de mortier.
     */
    void clearMortarLevels(void);

    /**
     * @brief Selectionne le niveau de mortier courant par index.
     * @param index Index dans la liste des niveaux ; les valeurs invalides sont ignorees.
     */
    void setSelectedMortarLevelIndex(int index);

    /**
     * @brief Change l'etat actif du mortier.
     */
    void setMortarEnabled(bool enabled);

    /**
     * @brief Met a jour le montant stocke dans le coffre d'un mortier.
     * @param index Index du mortier dans la liste alimentee.
     * @param amount Nouveau montant de gold. Les valeurs negatives sont ramenees a zero.
     */
    void updateMortarChestGoldAmount(int index, std::int64_t amount);

    /**
     * @brief Met a jour le montant stocke dans le coffre d'un mortier par son nom.
     * @param name Nom du mortier a mettre a jour.
     * @param amount Nouveau montant de gold. Les valeurs negatives sont ramenees a zero.
     */
    void updateMortarChestGoldAmount(const std::string& name, std::int64_t amount);

    /**
     * @brief Change le montant de gold disponible dans la tresorerie de guilde.
     */
    void setTreasuryGoldAmount(std::int64_t amount);

    /**
     * @brief Change le montant de rubies disponible dans la tresorerie de guilde.
     */
    void setTreasuryRubiesAmount(std::int64_t amount);

    /**
     * @brief Change le montant de perles disponible dans la tresorerie de guilde.
     */
    void setTreasuryPearlsAmount(std::int64_t amount);

    /**
     * @brief Change le montant de cristaux disponible dans la tresorerie de guilde.
     */
    void setTreasuryCrystalsAmount(std::int64_t amount);

private:
    /**
     * @brief Donnees des niveaux et de l'etat courant.
     */
    std::vector<MortarLevelEntry> mortarLevels; /**< Niveaux de mortier affiches dans la liste. */
    int selectedMortarLevelIndex;               /**< Niveau selectionne dans la liste, ou -1 si aucun. */
    bool mortarEnabled;                         /**< True si le mortier est actif. */
    std::int64_t treasuryGoldAmount;            /**< Tresorerie gold de la guilde. */
    std::int64_t treasuryRubiesAmount;          /**< Tresorerie rubies de la guilde. */
    std::int64_t treasuryPearlsAmount;          /**< Tresorerie perles de la guilde. */
    std::int64_t treasuryCrystalsAmount;        /**< Tresorerie cristaux de la guilde. */
    std::string transferGoldInput;              /**< Montant saisi pour transferer de l'or. */
    std::size_t transferGoldCursorIndex;        /**< Position du curseur dans le montant saisi. */
    std::size_t transferGoldSelectionAnchorIndex; /**< Ancre de selection du montant saisi. */
    bool transferGoldInputFocused;              /**< True si le champ de transfert est actif. */
    bool transferGoldSelectingWithMouse;        /**< True pendant une selection souris dans l'input. */
    bool transferGoldCursorVisible;             /**< True si le curseur de saisie est visible. */
    float transferGoldCursorBlinkSec;           /**< Timer de clignotement du curseur. */

    /**
     * @brief Ressources graphiques.
     */
    RC2D_Font titleFont;        /**< Police des titres. */
    RC2D_Font bodyFont;         /**< Police du texte principal. */
    RC2D_Font valueFont;        /**< Police des valeurs numeriques. */
    RC2D_Image goldIcon;        /**< Icone gold. */
    RC2D_Image rubiesIcon;      /**< Icone rubies. */
    RC2D_Image pearlsIcon;      /**< Icone perles. */
    RC2D_Image crystalsIcon;    /**< Icone cristaux. */
    RC2D_Image arrowDownIcon;   /**< Icone de liste deroulante des niveaux. */
    WindowControlIcons controlIcons; /**< Helper des boutons de fenetre. */

    /**
     * @brief Etat de fenetre.
     */
    SDL_FRect widgetRect;       /**< Rectangle courant de la fenetre. */
    bool visible;               /**< True si la fenetre est visible. */
    bool widgetDragging;        /**< True pendant le drag du header. */
    float widgetDragOffsetX;    /**< Offset X souris -> fenetre au debut du drag. */
    float widgetDragOffsetY;    /**< Offset Y souris -> fenetre au debut du drag. */
    float widgetOffsetX;        /**< Offset X applique a la position de base. */
    float widgetOffsetY;        /**< Offset Y applique a la position de base. */

    /**
     * @brief Etat de la liste deroulante des niveaux de mortier.
     */
    bool levelDropdownOpen;     /**< True si la liste des niveaux est ouverte. */
    int levelFirstRow;          /**< Premiere ligne visible dans la liste deroulante. */
    bool levelScrollDragging;   /**< True pendant le drag de scrollbar de la liste. */
    float levelScrollDragOffsetY; /**< Offset souris -> thumb de la liste. */
    float levelScrollWheelHighlightSec; /**< Duree restante du highlight molette de la scrollbar. */

    MortarLevelEntry getSelectedDisplayEntry(void) const;

    std::size_t getTransferGoldCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const;
    bool hasTransferGoldSelection(void) const;
    std::size_t getTransferGoldSelectionStart(void) const;
    std::size_t getTransferGoldSelectionEnd(void) const;
    void clearTransferGoldSelection(void);
    void deleteSelectedTransferGoldText(void);
};
