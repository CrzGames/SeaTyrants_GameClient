#pragma once

#include <RC2D/RC2D.h>
#include <array>
#include <string>
#include <vector>

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
class MarcheWidget {
public:
    /**
     * @brief Categories de filtrage partagees par tous les onglets.
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
     * @brief Ligne de prix: montant + type de monnaie.
     *
     * Exemples:
     * - `350 gold`
     * - `500 rubies`
     * - `3 cristaux`
     */
    struct MarketPriceData {
        int amount;
        std::string currency;
    };

    /**
     * @brief Donnees d'une ligne du Bazar.
     */
    struct BazardItemData {
        std::string imagePath;        /**< Chemin image dans RC2D_STORAGE_TITLE. */
        std::string itemName;         /**< Nom de l'objet. */
        std::string itemDescription;  /**< Description de l'objet. */
        MarketCategory category;      /**< Categorie de filtrage. */
        int quantity;                 /**< Quantite disponible. */
        std::string highestBidder;    /**< Pseudo du plus offrant. */
        std::string yourOffer;        /**< Valeur d'offre pre-remplie. */
    };

    /**
     * @brief Donnees d'une ligne de marche (noir/basique/event).
     */
    struct MarketItemData {
        std::string imagePath;               /**< Chemin image dans RC2D_STORAGE_TITLE. */
        std::string itemName;                /**< Nom de l'objet. */
        std::string itemDescription;         /**< Description de l'objet. */
        MarketCategory category;             /**< Categorie de filtrage. */
        int quantity;                        /**< Stock/quantite de la ligne. */
        std::string yourOffer;               /**< Quantite d'achat pre-remplie. */
        std::vector<MarketPriceData> prices; /**< Liste des prix multi-monnaies. */
    };

private:
    /**
     * @brief Onglet actuellement actif dans la fenetre.
     */
    enum class ActiveTab : int {
        BAZARD = 0,   /**< Vue bazar (enchere). */
        NOIR = 1,     /**< Vue marche noir. */
        BASIQUE = 2,  /**< Vue marche basique. */
        EVENEMENT = 3 /**< Vue marche d'event. */
    };

    /**
     * @brief Representation interne d'une ligne du bazar.
     *
     * Ce format est utilise uniquement au runtime pour le rendu et
     * l'interaction, apres conversion des donnees externes.
     */
    struct BazardRow {
        RC2D_Image icon;           /**< Texture de l'icone de l'objet. */
        std::string name;          /**< Nom court affiche dans la premiere colonne. */
        std::string description;   /**< Description courte affichee sous le nom. */
        MarketCategory category;   /**< Categorie associee a cette ligne (pour les filtres). */
        int quantity;              /**< Quantite de l'objet pour la ligne courante. */
        std::string highestBidder; /**< Nom du plus offrant affiche dans la colonne dediee. */
        std::string yourOffer;     /**< Valeur d'entree utilisateur (offre). */
    };

    /**
     * @brief Representation interne d'une ligne des marches classiques.
     *
     * Utilisee pour les onglets `NOIR`, `BASIQUE` et `EVENEMENT`.
     */
    struct NoirRow {
        RC2D_Image icon;                  /**< Texture de l'icone de l'objet. */
        std::string name;                 /**< Nom court affiche dans la premiere colonne. */
        std::string description;          /**< Description courte affichee sous le nom. */
        MarketCategory category;          /**< Categorie associee a cette ligne (pour les filtres). */
        int quantity;                     /**< Quantite/stock affiche dans la colonne quantite. */
        std::string buyQuantity;          /**< Valeur d'entree utilisateur (quantite d'achat). */
        std::vector<MarketPriceData> prices; /**< Liste de prix multi-monnaies affichee ligne par ligne. */
    };

    RC2D_Font titleFont; /**< Police principale pour titres/labels importants. */
    RC2D_Font bodyFont;  /**< Police principale du contenu des lignes. */
    RC2D_Font smallFont; /**< Police secondaire (labels compacts/filtres). */
    SDL_FRect widgetRect; /**< Rectangle ecran global de la fenetre marche. */

    bool visible;       /**< Indique si le widget est actuellement visible. */
    bool cursorEnabled; /**< Autorise le widget a piloter l'icone du curseur. */

    ActiveTab activeTab;             /**< Onglet actuellement selectionne. */
    std::vector<BazardRow> bazardRows; /**< Donnees internes de l'onglet bazar. */
    std::vector<NoirRow> noirRows;     /**< Donnees internes de l'onglet marche noir. */
    std::vector<NoirRow> basicRows;    /**< Donnees internes de l'onglet marche basique. */
    std::vector<NoirRow> eventRows;    /**< Donnees internes de l'onglet marche d'event. */
    int bazardFirstRow;              /**< Index du premier element visible (bazar, apres filtre). */
    int noirFirstRow;                /**< Index du premier element visible (noir, apres filtre). */
    int basicFirstRow;               /**< Index du premier element visible (basique, apres filtre). */
    int eventFirstRow;               /**< Index du premier element visible (event, apres filtre). */

    /**
     * @brief Etat coche/non coche des categories de filtre.
     *
     * Indexe par `MarketCategory` converti en entier.
     * Si aucune categorie n'est cochee, le filtrage est considere "tout afficher".
     */
    std::array<bool, static_cast<std::size_t>(MarketCategory::COUNT)> categoryFilterEnabled;

    bool scrollBarDragging; /**< `true` pendant un drag actif du thumb de scroll vertical. */
    float scrollDragOffsetY; /**< Decalage Y entre curseur et sommet du thumb pendant le drag. */

    bool widgetDragging;     /**< `true` pendant un drag actif de la fenetre via le header. */
    float widgetDragOffsetX; /**< Decalage X curseur/fenetre pris au debut du drag. */
    float widgetDragOffsetY; /**< Decalage Y curseur/fenetre pris au debut du drag. */
    float widgetOffsetX;     /**< Offset X de fenetre par rapport a la position de reference. */
    float widgetOffsetY;     /**< Offset Y de fenetre par rapport a la position de reference. */

    bool inputFocused;        /**< `true` si un champ d'entree d'offre est actuellement focus. */
    ActiveTab focusedTab;     /**< Onglet auquel appartient le champ focus. */
    int focusedRow;           /**< Index source de la ligne focussee (non index d'affichage). */
    std::size_t cursorIndex;  /**< Position du caret dans le texte de l'input focus. */
    bool cursorVisible;       /**< Etat visuel courant du caret (blink). */
    double cursorBlinkElapsed; /**< Temps accumule pour alterner la visibilite du caret. */
    std::string timerText;    /**< Texte du timer affiche dans le footer (bazar). */

    /**
     * @brief Vide toutes les lignes internes de tous les onglets.
     *
     * Libere egalement les textures d'icones associees.
     */
    void clearRows(void);

    /**
     * @brief Vide un tableau de lignes de marche et libere leurs icones.
     * @param rows Tableau cible a vider.
     */
    void clearNoirRows(std::vector<NoirRow>& rows);

    /**
     * @brief Retourne le tableau de lignes associe a un onglet de type marche.
     * @param tab Onglet cible (`NOIR`, `BASIQUE`, `EVENEMENT`).
     * @return Pointeur vers le tableau correspondant, ou `nullptr` si non applicable.
     */
    std::vector<NoirRow>* getNoirRowsForTab(ActiveTab tab);

    /**
     * @brief Variante const de getNoirRowsForTab.
     * @param tab Onglet cible (`NOIR`, `BASIQUE`, `EVENEMENT`).
     * @return Pointeur const vers le tableau correspondant, ou `nullptr`.
     */
    const std::vector<NoirRow>* getNoirRowsForTab(ActiveTab tab) const;

    /**
     * @brief Retourne le compteur de scroll (premiere ligne visible) d'un onglet.
     * @param tab Onglet cible.
     * @return Pointeur vers l'index de premiere ligne visible.
     */
    int* getFirstRowForTab(ActiveTab tab);

    /**
     * @brief Variante const de getFirstRowForTab.
     * @param tab Onglet cible.
     * @return Pointeur const vers l'index de premiere ligne visible.
     */
    const int* getFirstRowForTab(ActiveTab tab) const;

    /**
     * @brief Injecte les donnees externes dans un onglet de marche.
     *
     * Convertit les `MarketItemData` vers le format interne `NoirRow`,
     * nettoie les anciennes ressources et reinitialise le scroll de l'onglet.
     *
     * @param tab Onglet cible (`NOIR`, `BASIQUE`, `EVENEMENT`).
     * @param rows Donnees externes a convertir/stocker.
     */
    void setNoirFamilyRows(ActiveTab tab, const std::vector<MarketItemData>& rows);

    /**
     * @brief Retire le focus clavier/texte des champs d'entree du widget.
     */
    void clearInputFocusInternal(void);

    /**
     * @brief Applique l'action du bouton de soumission sur la ligne focussee.
     *
     * Bazar: soumet une offre.
     * Marches: applique une tentative d'achat.
     */
    void submitFocusedInput(void);

    /**
     * @brief Indique si l'onglet actif est le bazar.
     */
    bool isBazardActive(void) const;

    /**
     * @brief Indique si l'onglet actif fait partie de la famille des marches.
     */
    bool isNoirFamilyActive(void) const;

    /**
     * @brief Indique si au moins un filtre de categorie est actif.
     */
    bool hasAnyCategoryFilterEnabled(void) const;

    /**
     * @brief Retourne l'etat d'activation d'une categorie.
     * @param category Categorie a verifier.
     */
    bool isCategoryEnabled(MarketCategory category) const;

    /**
     * @brief Teste si une categorie passe le filtre courant.
     *
     * Regle: si aucun filtre n'est coche, toutes les categories passent.
     *
     * @param category Categorie a tester.
     * @return `true` si la ligne doit etre visible.
     */
    bool passesCategoryFilter(MarketCategory category) const;

    /**
     * @brief Reinitialise tous les filtres de categories (tout decocher).
     */
    void resetCategoryFilters(void);

    /**
     * @brief Construit la liste des index source visibles pour un onglet.
     *
     * Cette liste tient compte de l'etat actuel des filtres de categories.
     *
     * @param tab Onglet cible.
     * @return Liste d'index source correspondant aux lignes visibles.
     */
    std::vector<int> buildFilteredRowIndices(ActiveTab tab) const;

public:
    /**
     * @brief Construit la fenetre marche.
     */
    MarcheWidget(void);

    /**
     * @brief Detruit la fenetre marche.
     */
    ~MarcheWidget(void);

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
     * @brief Remplace toutes les lignes du Bazar.
     */
    void setBazardRows(const std::vector<BazardItemData>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche noir.
     */
    void setMarcheNoirRows(const std::vector<MarketItemData>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche basique.
     */
    void setMarcheBasiqueRows(const std::vector<MarketItemData>& rows);

    /**
     * @brief Remplace toutes les lignes du Marche d'event.
     */
    void setMarcheEvenementRows(const std::vector<MarketItemData>& rows);

    /**
     * @brief Enleve le focus des champs de saisie.
     */
    void clearFocus(void);

    /**
     * @brief Retourne l'etat d'affichage du widget.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Active/desactive la gestion du curseur par le widget.
     */
    void setCursorEnabled(bool enabled) { this->cursorEnabled = enabled; }

    /**
     * @brief Teste si un point ecran est dans la fenetre.
     */
    bool containsPoint(float x, float y) const;
};
