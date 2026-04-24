#pragma once

#include <RC2D/RC2D.h>

#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

class MoneyWidget {
public:
    /**
     * @brief Types de monnaies actuellement supportes par la fenetre.
     */
    enum class CurrencyType : int {
        RUBIES = 0, /**< Monnaie premium de type rubies. */
        GOLD = 1    /**< Monnaie classique de type gold. */
    };

    /**
     * @brief Donnees publiques d'une ligne de monnaie.
     */
    struct CurrencyEntry {
        CurrencyType type; /**< Type logique de la monnaie affichee. */
        std::string iconPath; /**< Chemin de l'image / icone GUI a charger pour cette monnaie. */
        int value; /**< Valeur numerique actuellement affichee pour cette monnaie. */
    };

    MoneyWidget(void);
    ~MoneyWidget(void);

    /**
     * @brief Charge les ressources du widget money.
     */
    void load(void);

    /**
     * @brief Libere les ressources chargees par le widget money.
     */
    void unload(void);

    /**
     * @brief Met a jour le drag de la fenetre et le drag de la scrollbar.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre money.
     */
    void draw(void) const;

    /**
     * @brief Traite les clics souris de la fenetre.
     * @return True si l'evenement est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Traite la molette quand la souris est dans la fenetre money.
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
     * @brief Indique si la fenetre money est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Autorise/interdit le pilotage du curseur par ce widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point est dans la fenetre money courante.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour une position donnee.
     * @param x Position X de rendu a evaluer.
     * @param y Position Y de rendu a evaluer.
     * @return Type de curseur demande par le widget.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Ajoute ou remplace une ligne de monnaie par type.
     * @param type Type logique de la monnaie a alimenter.
     * @param iconPath Chemin de l'image / icone GUI a charger.
     * @param value Valeur numerique a afficher. Les valeurs negatives sont ramenees a zero.
     */
    void setCurrencyEntry(CurrencyType type, const std::string& iconPath, int value);

    /**
     * @brief Remplace entierement la liste des monnaies affichees.
     * @param entries Nouvelles lignes de monnaie a afficher. Si un type apparait plusieurs fois,
     *               la derniere occurrence ecrase les precedentes afin de garder une seule ligne
     *               par type de monnaie dans la GUI.
     */
    void setCurrencyEntries(const std::vector<CurrencyEntry>& entries);

    /**
     * @brief Vide toutes les lignes de monnaie actuellement affichees.
     */
    void clearCurrencyEntries(void);

    /**
     * @brief Rouvre la fenetre money.
     */
    void show(void);

    /**
     * @brief Ferme la fenetre money.
     */
    void hide(void);

private:
    /**
     * @brief Representation runtime d'une ligne de monnaie.
     */
    struct CurrencyEntryRuntime {
        CurrencyType type; /**< Type logique de la monnaie. */
        std::string iconPath; /**< Chemin actuellement memorise pour l'icone. */
        int value; /**< Valeur actuellement affichee. */
        RC2D_Image iconImage; /**< Reference runtime de l'icone chargee. */
    };

    /**
     * @brief Lignes de monnaies actuellement alimentees.
     */
    std::vector<CurrencyEntryRuntime> currencyEntries; /**< Liste affichee dans le corps de la fenetre. */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police du titre. */
    RC2D_Font bodyFont; /**< Police des lignes de monnaies. */
    SDL_FRect widgetRect; /**< Rectangle global du widget. */

    /**
     * @brief Etat global de visibilite de la fenetre.
     */
    bool visible; /**< True si la fenetre est visible. */

    /**
     * @brief Etat de deplacement et de scroll de la fenetre.
     */
    bool widgetDragging; /**< True si l'utilisateur drag la fenetre via le header. */
    int scrollFirstRow; /**< Premiere ligne de monnaie visible dans la liste. */
    bool scrollBarDragging; /**< True si le pouce de scrollbar est en cours de drag. */
    float scrollDragOffsetY; /**< Offset souris->thumb pendant le drag vertical. */
    float widgetDragOffsetX; /**< Offset X souris->coin haut gauche pendant le drag. */
    float widgetDragOffsetY; /**< Offset Y souris->coin haut gauche pendant le drag. */
    float widgetOffsetX; /**< Decalage X applique a la position de base. */
    float widgetOffsetY; /**< Decalage Y applique a la position de base. */

    /**
     * @brief Autorisation de pilotage du curseur souris.
     */
    bool cursorEnabled; /**< True si ce widget peut piloter le curseur ce frame. */
    WindowControlIcons controlIcons; /**< Helper des icones de controle. */

    /**
     * @brief Retourne l'index d'une ligne de monnaie deja presente.
     * @param type Type recherche.
     * @return Index trouve, ou `currencyEntries.size()` si absent.
     */
    std::size_t findCurrencyEntryIndex(CurrencyType type) const;

    /**
     * @brief Recharge l'icone runtime d'une ligne de monnaie.
     * @param entry Ligne runtime a mettre a jour.
     */
    void reloadCurrencyIcon(CurrencyEntryRuntime& entry);

    /**
     * @brief Retourne le libelle UI associe a un type de monnaie.
     * @param type Type de monnaie a convertir.
     * @return Libelle court affiche dans la ligne.
     */
    const char* getCurrencyLabel(CurrencyType type) const;
};
