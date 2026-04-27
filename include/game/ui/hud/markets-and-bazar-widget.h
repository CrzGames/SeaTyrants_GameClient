#pragma once

#include <RC2D/RC2D.h>
#include <array>
#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Fenetre unique pour les differents marches du jeu.
 *
 * Cette GUI regroupe:
 * - `Marche basique`
 * - `Marche d'event`
 * - `Bazar`
 * - `Marche noir`
 *
 * Elle supporte le drag de la fenetre, la fermeture, le scroll, les onglets
 * et un panneau de filtres de categories a gauche.
 */
class MarketsAndBazarWidget {
public:
    /**
     * @brief Categories de filtrage partagees par tous les onglets de marche.
     *
     * L'ordre suit l'affichage alphabetique demande dans l'UI.
     */
    enum class MarketCategory : int {
        ACTIVABLES = 0,
        BOOSTER = 1,
        CANNONS = 2,
        CONSOMMABLES = 3,
        HARPONEUSE = 4,
        MATELOTS = 5,
        MUNITION_DE_CANNON = 6,
        MUNITION_DE_HARPON = 7,
        NAVIRES = 8,
        UTILISABLE_SUR_CIBLE = 9,
        VOILES = 10,
        COUNT = 11
    };

    /**
     * @brief Types de monnaies supportees dans les prix du marche.
     */
    enum class MarketCurrency : int {
        GOLD = 0,     /**< Monnaie "gold". */
        CRISTAUX = 1, /**< Monnaie "cristaux". */
        RUBIES = 2,   /**< Monnaie "rubies". */
        FACTIONS = 3  /**< Monnaie "factions". */
    };

    /**
     * @brief Ligne de prix: montant + type de monnaie.
     */
    struct MarketPriceData {
        int amount; /**< Montant numerique du prix. */
        MarketCurrency currency; /**< Type de monnaie utilisee pour ce montant. */
    };

    /**
     * @brief Donnees d'une ligne du Bazar.
     */
    struct BazarRow {
        std::string imagePath; /**< Chemin image dans RC2D_STORAGE_TITLE. */
        std::string itemName; /**< Nom de l'objet. */
        std::string itemDescription; /**< Description de l'objet. */
        MarketCategory category; /**< Categorie de filtrage. */
        int quantity; /**< Quantite disponible. */
        std::string highestBidder; /**< Pseudo du plus offrant. */
        std::string yourOffer; /**< Valeur d'offre pre-remplie. */
    };

    /**
     * @brief Donnees d'une ligne de marche (black/basic/event).
     */
    struct MarketRow {
        std::string imagePath; /**< Chemin image dans RC2D_STORAGE_TITLE. */
        std::string itemName; /**< Nom de l'objet. */
        std::string itemDescription; /**< Description de l'objet. */
        MarketCategory category; /**< Categorie de filtrage. */
        int quantity; /**< Stock/quantite de la ligne. */
        std::string yourOffer; /**< Quantite d'achat pre-remplie. */
        std::vector<MarketPriceData> prices; /**< Liste des prix multi-monnaies. */
    };

    /**
     * @brief Construit la fenetre marche.
     */
    MarketsAndBazarWidget(void);

    /**
     * @brief Detruit la fenetre marche.
     */
    ~MarketsAndBazarWidget(void);

    /**
     * @brief Charge les ressources du widget.
     */
    void load(void);

    /**
     * @brief Libere les ressources du widget.
     */
    void unload(void);

    /**
     * @brief Met a jour l'etat du widget.
     * @param dt Delta time en secondes.
     */
    void update(double dt);

    /**
     * @brief Dessine le widget.
     */
    void draw(void) const;

    /**
     * @brief Gere le clic souris.
     * @return `true` si l'event est consomme.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Gere la molette souris.
     * @return `true` si l'event est consomme.
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
     * @brief Gere le clavier (inputs d'offre).
     * @return `true` si l'event est consomme.
     */
    bool keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);

    /**
     * @brief Enleve le focus des champs de saisie.
     */
    void clearFocus(void);

    /**
     * @brief Retourne l'etat d'affichage du widget.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief True si un champ d'offre / quantite a le focus clavier.
     */
    bool hasBlockingOfferInputFocus(void) const { return this->visible && this->inputFocused; }

    /**
     * @brief Active/desactive la gestion du curseur par le widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point ecran est dans la fenetre.
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
     * @brief Remplace toutes les lignes du Bazar.
     */
    void setBazarRows(const std::vector<BazarRow>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche noir.
     */
    void setBlackMarketRows(const std::vector<MarketRow>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche basique.
     */
    void setBasicMarketRows(const std::vector<MarketRow>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche d'event.
     */
    void setEventMarketRows(const std::vector<MarketRow>& rows);

    /**
     * @brief Rouvre la fenetre sur l'onglet "Marche basique".
     */
    void openBasicMarket(void);
    
private:
    /**
     * @brief Onglet actuellement actif dans la fenetre.
     */
    enum class ActiveTab : int {
        BAZARD = 0,       /**< Vue bazar (enchere). */
        BLACK_MARKET = 1, /**< Vue marche noir. */
        BASIC_MARKET = 2, /**< Vue marche basique. */
        EVENT_MARKET = 3  /**< Vue marche d'event. */
    };

    /**
     * @brief Donnees des onglets et icones chargees associees.
     */
    ActiveTab activeTab; /**< Onglet actuellement selectionne. */

    std::vector<BazarRow> bazarRows; /**< Donnees de l'onglet bazar. */
    std::vector<MarketRow> blackMarketRows; /**< Donnees de l'onglet marche noir. */
    std::vector<MarketRow> basicMarketRows; /**< Donnees de l'onglet marche basique. */
    std::vector<MarketRow> eventMarketRows; /**< Donnees de l'onglet marche d'event. */

    std::vector<RC2D_Image> bazarRowIcons; /**< Icones des lignes du bazar. */
    std::vector<RC2D_Image> blackMarketRowIcons; /**< Icones des lignes du marche noir. */
    std::vector<RC2D_Image> basicMarketRowIcons; /**< Icones des lignes du marche basique. */
    std::vector<RC2D_Image> eventMarketRowIcons; /**< Icones des lignes du marche d'event. */
    
    int bazarFirstRow; /**< Index du premier element visible (bazar, apres filtre). */
    int blackFirstRow; /**< Index du premier element visible (black market, apres filtre). */
    int basicFirstRow; /**< Index du premier element visible (basic market, apres filtre). */
    int eventFirstRow; /**< Index du premier element visible (event market, apres filtre). */

    /**
     * @brief Ressources de rendu et rectangle principal.
     */
    RC2D_Font titleFont; /**< Police principale pour titres/labels importants. */
    RC2D_Font bodyFont;  /**< Police principale du contenu des lignes. */
    RC2D_Font smallFont; /**< Police secondaire (labels compacts/filtres). */
    SDL_FRect widgetRect; /**< Rectangle ecran global de la fenetre marche. */

    /**
     * @brief Etat global d'affichage et de pilotage du curseur.
     */
    bool visible;       /**< Indique si le widget est actuellement visible. */
    bool cursorEnabled; /**< Autorise le widget a piloter l'icone du curseur. */
    WindowControlIcons controlIcons; /**< Helper des icones de controle. */

    /**
     * @brief Etat coche/non coche des categories de filtre.
     */
    std::array<bool, static_cast<std::size_t>(MarketCategory::COUNT)> categoryFilterEnabled;

    /**
     * @brief Etat de drag de la scrollbar verticale.
     */
    bool scrollBarDragging; /**< `true` pendant un drag actif du thumb de scroll vertical. */
    float scrollDragOffsetY; /**< Decalage Y entre curseur et sommet du thumb pendant le drag. */

    /**
     * @brief Etat de deplacement de la fenetre.
     */
    bool widgetDragging; /**< `true` pendant un drag actif de la fenetre via le header. */
    float widgetDragOffsetX; /**< Decalage X curseur/fenetre pris au debut du drag. */
    float widgetDragOffsetY; /**< Decalage Y curseur/fenetre pris au debut du drag. */
    float widgetOffsetX; /**< Offset X de fenetre par rapport a la position de reference. */
    float widgetOffsetY; /**< Offset Y de fenetre par rapport a la position de reference. */

    /**
     * @brief Etat de saisie des champs de quantite/offre.
     */
    bool inputFocused; /**< `true` si un champ d'entree d'offre est actuellement focus. */
    ActiveTab focusedTab; /**< Onglet auquel appartient le champ focus. */
    int focusedRow; /**< Index source de la ligne focussee (non index d'affichage). */
    std::size_t cursorIndex; /**< Position du caret dans le texte de l'input focus. */
    bool cursorVisible; /**< Etat visuel courant du caret (blink). */
    double cursorBlinkElapsed; /**< Temps accumule pour alterner la visibilite du caret. */
    std::string timerText; /**< Texte du timer affiche dans le footer (bazar). */

    /**
     * @brief Vide toutes les lignes de tous les onglets et libere leurs icones.
     */
    void clearRows(void);

    /**
     * @brief Libere un tableau d'icones puis le vide.
     */
    void clearIcons(std::vector<RC2D_Image>& icons);

    /**
     * @brief Retourne le tableau associe a un onglet de type marche.
     */
    std::vector<MarketRow>* getMarketRowsForTab(ActiveTab tab);

    /**
     * @brief Variante const de getMarketRowsForTab.
     */
    const std::vector<MarketRow>* getMarketRowsForTab(ActiveTab tab) const;

    /**
     * @brief Retourne le tableau d'icones associe a un onglet de type marche.
     */
    std::vector<RC2D_Image>* getMarketIconsForTab(ActiveTab tab);

    /**
     * @brief Variante const de getMarketIconsForTab.
     */
    const std::vector<RC2D_Image>* getMarketIconsForTab(ActiveTab tab) const;

    /**
     * @brief Retourne le compteur de scroll (premiere ligne visible) d'un onglet.
     */
    int* getFirstRowForTab(ActiveTab tab);

    /**
     * @brief Variante const de getFirstRowForTab.
     */
    const int* getFirstRowForTab(ActiveTab tab) const;

    /**
     * @brief Injecte les donnees externes dans un onglet de marche.
     */
    void setMarketFamilyRows(ActiveTab tab, const std::vector<MarketRow>& rows);

    /**
     * @brief Retire le focus clavier/texte des champs d'entree du widget.
     */
    void clearInputFocusInternal(void);

    /**
     * @brief Applique l'action du bouton de soumission sur la ligne focussee.
     */
    void submitFocusedInput(void);

    /**
     * @brief Indique si l'onglet actif est le bazar.
     */
    bool isBazarActive(void) const;

    /**
     * @brief Indique si au moins un filtre de categorie est actif.
     */
    bool hasAnyCategoryFilterEnabled(void) const;

    /**
     * @brief Retourne l'etat d'activation d'une categorie.
     */
    bool isCategoryEnabled(MarketCategory category) const;

    /**
     * @brief Teste si une categorie passe le filtre courant.
     */
    bool passesCategoryFilter(MarketCategory category) const;

    /**
     * @brief Reinitialise tous les filtres de categories (tout decocher).
     */
    void resetCategoryFilters(void);

    /**
     * @brief Construit la liste des index source visibles pour un onglet.
     */
    std::vector<int> buildFilteredRowIndices(ActiveTab tab) const;
};
