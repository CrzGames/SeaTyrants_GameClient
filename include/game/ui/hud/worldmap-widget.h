#pragma once

#include <RC2D/RC2D.h>

#include <map>
#include <string>
#include <vector>

#include "game/ui/hud/hud-cursor.h"
#include "game/ui/hud/window-control-icons.h"

/**
 * @brief Widget gameplay dedie a l'affichage de la carte du monde.
 *
 * Le widget:
 * - charge `assets/data/worldmap.json` depuis le storage title;
 * - reconstruit la GUI gameplay (cases + liaisons) a partir du JSON;
 * - permet au gameplay de remplacer les tags de guild map par map;
 * - dessine une fenetre HUD flottante reprenant le style des autres widgets.
 *
 * Le fichier JSON attendu correspond au format exporte par l'editeur world map.
 */
class WorldMapWidget {
public:
    /**
     * @brief Construit le widget dans un etat vide.
     */
    WorldMapWidget(void);

    /**
     * @brief Destructeur par defaut.
     */
    ~WorldMapWidget(void);

    /**
     * @brief Charge les polices, icones et le JSON de carte du monde.
     *
     * Cette methode remet aussi l'etat runtime a zero: position de fenetre,
     * visibilite, drag et donnees precedemment chargees.
     */
    void load(void);

    /**
     * @brief Libere toutes les ressources graphiques et donnees runtime.
     */
    void unload(void);

    /**
     * @brief Met a jour la position de la fenetre pendant un drag actif.
     *
     * @param dt Delta time en secondes (non utilise actuellement).
     */
    void update(double dt);

    /**
     * @brief Dessine la fenetre HUD de carte du monde.
     */
    void draw(void) const;

    /**
     * @brief Traite un clic souris sur le widget.
     *
     * @param x Position X en coordonnees de rendu.
     * @param y Position Y en coordonnees de rendu.
     * @param button Bouton souris.
     * @param clicks Nombre de clics (non utilise ici).
     * @param mouseID Identifiant souris SDL (non utilise ici).
     * @return true si l'evenement est consomme par le widget.
     */
    bool mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);

    /**
     * @brief Indique si la fenetre est visible.
     */
    bool isVisible(void) const { return this->visible; }

    /**
     * @brief Affiche la fenetre.
     */
    void show(void);

    /**
     * @brief Cache la fenetre et interrompt tout drag en cours.
     */
    void hide(void);

    /**
     * @brief Teste si un point tombe dans la fenetre courante.
     *
     * @param x Position X de rendu.
     * @param y Position Y de rendu.
     * @return true si le point est dans le rectangle global du widget.
     */
    bool containsPoint(float x, float y) const;

    /**
     * @brief Retourne le curseur souhaite pour une position de rendu.
     *
     * @param x Position X de rendu.
     * @param y Position Y de rendu.
     * @return `MOVE` sur le header, `POINTER` sur la croix, sinon `NONE`.
     */
    HudCursorType getDesiredCursor(float x, float y) const;

    /**
     * @brief Recharge la carte du monde depuis un JSON du storage title.
     *
     * @param storagePath Chemin storage title du fichier JSON.
     * @return true si la lecture et le parsing ont reussi.
     */
    bool loadFromTitleStorageJson(const char* storagePath);

    /**
     * @brief Associe un tag de guild a une map par son nom.
     *
     * Cette methode est prevue pour les donnees serveur: quand le gameplay sait
     * qu'une map appartient a une guild, il peut appeler cette API pour forcer
     * le tag affiche dans la case correspondante.
     *
     * @param mapName Nom logique de la map (ex: `2/3`).
     * @param guildTag Tag a afficher. Une chaine vide retire l'override.
     */
    void setGuildTagForMapName(const std::string& mapName, const std::string& guildTag);

    /**
     * @brief Retire l'override de tag d'une map.
     *
     * @param mapName Nom logique de la map.
     */
    void clearGuildTagForMapName(const std::string& mapName);

    /**
     * @brief Supprime tous les overrides de tags de guild.
     */
    void clearAllGuildTags(void);

private:
    /**
     * @brief Cotes logiques utilises pour dessiner les liaisons.
     */
    enum class LinkSide {
        LEFT = 0,   /**< Sortie par le bord gauche. */
        TOP = 1,    /**< Sortie par le bord haut. */
        RIGHT = 2,  /**< Sortie par le bord droit. */
        BOTTOM = 3  /**< Sortie par le bord bas. */
    };

    /**
     * @brief Description d'une case de la world map.
     */
    struct MapCase {
        float x = 0.0f;              /**< Position X logique dans la GUI. */
        float y = 0.0f;              /**< Position Y logique dans la GUI. */
        float width = 80.0f;         /**< Largeur logique de la case. */
        float height = 67.0f;        /**< Hauteur logique de la case. */
        std::string mapName;         /**< Nom logique de la map affiche au centre. */
        bool showGuildTag = false;   /**< true si la case reserve une ligne pour le tag. */
        int guildTagMaxChars = 3;    /**< Limite de caracteres d'affichage du tag. */
        float guildTagHeightRatio = 0.22f; /**< Ratio legacy exporte par l'editeur. */
        bool isCity = false;         /**< true pour une map ville, false pour une map normale. */
    };

    /**
     * @brief Description d'une liaison entre deux cases.
     */
    struct MapLink {
        int fromCaseIndex = -1;      /**< Index de la case source. */
        int toCaseIndex = -1;        /**< Index de la case cible. */
        LinkSide fromSide = LinkSide::RIGHT; /**< Cote de sortie sur la case source. */
    };

    /**
     * @brief Retourne le rectangle de base du widget sur l'ecran.
     */
    SDL_FRect getBaseRectFromGameScreen(void) const;

    /**
     * @brief Recalcule la geometrie de la fenetre depuis l'ecran courant.
     */
    void updateWidgetRect(void);

    /**
     * @brief Recalcule les rectangles derives du widget.
     */
    void updateDerivedRects(void);

    /**
     * @brief Retourne le texte de tag a dessiner pour une case.
     */
    std::string getGuildTagLabelForCase(const MapCase& mapCase) const;

    /**
     * @brief Retourne l'ancre ecran d'une liaison sur une case.
     */
    SDL_FPoint getLinkAnchor(const SDL_FRect& caseRect, LinkSide side) const;

    /**
     * @brief Retourne le cote oppose pour la case cible.
     */
    LinkSide getOppositeLinkSide(LinkSide side) const;

    /**
     * @brief Tronque une chaine ASCII au nombre de caracteres demande.
     */
    std::string truncateAscii(const std::string& value, int maxChars) const;

    /**
     * @brief Lit un fichier texte entier depuis le storage title.
     */
    bool readTitleTextFile(const char* storagePath, std::string* outText) const;

    /**
     * @brief Reinitialise les donnees logiques chargees.
     */
    void clearLoadedWorldMapData(void);

    RC2D_Font titleFont;            /**< Police du titre de fenetre. */
    RC2D_Font bodyFont;             /**< Police principale des textes de cases. */
    RC2D_Font smallFont;            /**< Police secondaire pour les tags de guild. */
    WindowControlIcons controlIcons; /**< Helper commun de bouton fermer. */
    SDL_FRect widgetRect;           /**< Rectangle global courant du widget. */
    SDL_FRect headerRect;           /**< Header draggable. */
    SDL_FRect closeButtonRect;      /**< Bouton fermer. */
    SDL_FRect legendRect;           /**< Zone de legende map ville / map normale. */
    SDL_FRect viewportRect;         /**< Zone visible contenant la GUI world map. */
    bool visible;                   /**< Etat visible de la fenetre. */
    bool widgetDragging;            /**< true pendant un drag du header. */
    float widgetDragOffsetX;        /**< Offset souris -> widget pendant le drag. */
    float widgetDragOffsetY;        /**< Offset souris -> widget pendant le drag. */
    float widgetOffsetX;            /**< Decalage horizontal applique a la fenetre. */
    float widgetOffsetY;            /**< Decalage vertical applique a la fenetre. */
    float guiWidth;                 /**< Largeur logique lue depuis le JSON. */
    float guiHeight;                /**< Hauteur logique lue depuis le JSON. */
    float renderScale;              /**< Echelle de rendu appliquee pour fit dans la viewport. */
    SDL_FPoint renderOffset;        /**< Offset de centrage de la GUI dans la viewport. */
    std::string loadedStoragePath;  /**< Dernier chemin JSON charge. */
    std::vector<MapCase> mapCases;  /**< Cases logiques de la carte du monde. */
    std::vector<MapLink> mapLinks;  /**< Liaisons logiques entre cases. */
    std::map<std::string, std::string> guildTagsByMapName; /**< Overrides runtime des tags de guild. */
};
