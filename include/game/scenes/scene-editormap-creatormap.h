#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/overlay/tile-click-marker.h"
#include "game/ui/overlay/scroll-bar-overlay.h"

/**
 * @class EditorMapCreateMapScene
 * @brief Scene d'editeur de map orientee creation.
 *
 * Cette scene permet de:
 * - peindre les collisions,
 * - importer/poser/supprimer des assets,
 * - importer un navire de test (dossier 1.png..8.png),
 * - tester le pathfinding A* du navire sur les collisions bloquees,
 * - changer la couleur ocean,
 * - exporter une map JSON.
 */
class EditorMapCreateMapScene : public Scene {
private:
    /**
     * @enum EditorMode
     * @brief Modes metier de l'editeur.
     */
    enum class EditorMode {
        MAP_CREATOR_MAP = 0 /**< Mode creation de map principal. */
    };

    /**
     * @enum EditorTool
     * @brief Outil actif de l'editeur.
     */
    enum class EditorTool {
        BLOCK_TILES = 0, /**< Peinture collision bloque/debloque. */
        PLACE_ASSETS = 1, /**< Pose/remplacement d'assets. */
        REMOVE_ASSETS = 2, /**< Suppression d'assets poses. */
        SPAWN_SHIP = 3, /**< Spawn/repositionnement du navire de test via preview. */
        CONTROL_SHIP = 4 /**< Controle du navire de test sur la map. */
    };

    /**
     * @struct ImportedAsset
     * @brief Donnees d'un asset importe dans la bibliotheque locale.
     */
    struct ImportedAsset {
        std::string id; /**< Identifiant unique interne de l'asset. */
        std::string displayName; /**< Nom affiche dans la liste. */
        std::string sourcePath; /**< Chemin source absolu choisi a l'import. */
        std::string storagePath; /**< Chemin en storage user pour la session. */
        RC2D_Image image; /**< Texture chargee par RC2D. */
        RC2D_ImageData imageData; /**< Surface source chargee (utile pour la minimap stylisee). */
        float widthPx; /**< Largeur native de l'image en pixels. */
        float heightPx; /**< Hauteur native de l'image en pixels. */
        int alphaMaskWidth; /**< Largeur du masque alpha source. */
        int alphaMaskHeight; /**< Hauteur du masque alpha source. */
        std::vector<Uint8> alphaMask; /**< Masque alpha linearise [0..255]. */
    };

    /**
     * @struct PlacedAsset
     * @brief Etat d'un asset pose sur la map.
     */
    struct PlacedAsset {
        int importedAssetIndex; /**< Index dans importedAssets. */
        int tileX; /**< Tuile metier (X) de reference. */
        int tileY; /**< Tuile metier (Y) de reference. */
        float anchorTileX; /**< Ancre sub-tile en X pour un pose precis. */
        float anchorTileY; /**< Ancre sub-tile en Y pour un pose precis. */
        float scale; /**< Echelle logique de pose. */
    };

    /**
     * @struct HistoryAction
     * @brief Action atomique pour undo/redo.
     */
    struct HistoryAction {
        /**
         * @enum Type
         * @brief Type d'action historisee.
         */
        enum class Type {
            TILE_BLOCK = 0, /**< Changement collision d'une tuile. */
            ASSET_AT_TILE = 1 /**< Changement d'asset pose sur une tuile. */
        };

        Type type; /**< Type de l'action. */
        int tileX; /**< Tuile cible X. */
        int tileY; /**< Tuile cible Y. */
        bool beforeBlocked; /**< Etat collision avant action. */
        bool afterBlocked; /**< Etat collision apres action. */
        bool hadBeforeAsset; /**< Presence asset avant action. */
        bool hadAfterAsset; /**< Presence asset apres action. */
        PlacedAsset beforeAsset; /**< Donnee asset avant action. */
        PlacedAsset afterAsset; /**< Donnee asset apres action. */
    };

    RC2D_Image backgroundUiImage; /**< Fond UI (haut/bas) de la scene. */
    RC2D_Font overlayFont; /**< Police des overlays et boutons. */
    ScrollBarOverlay scrollBarOverlay; /**< Overlay scrollbar reutilise en mode editeur. */

    EditorMode editorMode; /**< Mode metier courant. */
    EditorTool editorTool; /**< Outil courant selectionne. */
    int selectedOceanColorIndex; /**< Index couleur ocean active. */
    int pendingOceanColorDelta; /**< Delta ocean en attente d'application. */
    int selectedAssetIndex; /**< Index asset actuellement selectionne. */
    int assetListScrollOffset; /**< Offset de scroll de la liste assets. */
    bool showGrid; /**< Affichage grille isometrique ON/OFF. */

    bool hoveredTileValid; /**< true si la souris survole une tuile map. */
    SDL_Point hoveredTile; /**< Tuile actuellement survolee. */
    bool dragPaintActive; /**< true si un drag paint collision est actif. */
    bool dragPaintBlockedValue; /**< Valeur de paint collision du drag courant. */
    bool lastDragPaintTileValid; /**< true si la derniere tuile drag est valide. */
    SDL_Point lastDragPaintTile; /**< Derniere tuile peinte en drag. */

    std::vector<ImportedAsset> importedAssets; /**< Bibliotheque assets importes. */
    std::vector<PlacedAsset> placedAssets; /**< Assets poses sur la map. */
    std::vector<HistoryAction> historyActions; /**< Pile d'historique undo/redo. */
    int historyCursor; /**< Curseur courant dans l'historique. */
    unsigned int importedAssetCounter; /**< Compteur auto pour ID d'import. */
    std::string statusMessage; /**< Message de statut affichable HUD. */
    bool pendingImportDialogCompleted; /**< true si le callback import a publie un resultat. */
    bool pendingImportDialogCanceled; /**< true si l'utilisateur a annule le dialog import. */
    std::vector<std::string> pendingImportFilePaths; /**< Liste des chemins absolus en attente d'import sur le thread scene. */
    mutable std::mutex pendingImportMutex; /**< Mutex de synchronisation callback/import differe. */
    bool importBatchActive; /**< true si un batch d'import est en cours de traitement. */
    std::vector<std::string> importBatchFilePaths; /**< Queue locale des fichiers a importer progressivement. */
    size_t importBatchNextIndex; /**< Index du prochain fichier a importer dans le batch. */
    int importBatchImportedCount; /**< Nombre de fichiers importes avec succes dans le batch courant. */
    int importBatchFailedCount; /**< Nombre de fichiers en echec dans le batch courant. */
    bool assetListScrollDragActive; /**< true si le drag de la scrollbar assets est actif. */
    float assetListScrollDragGrabOffsetY; /**< Offset vertical curseur->thumb pour un drag precis. */
    TileClickMarker clickMarker; /**< Marqueur visuel de clic en mode controle navire. */
    Ship testShip; /**< Navire de test pour validation collisions + A*. */
    Ship testShipPreview; /**< Navire de preview sous la souris en mode spawn. */
    bool testShipLoaded; /**< true si le dossier navire a ete importe et charge. */
    bool testShipSpawned; /**< true si le navire de test est pose sur une tuile map. */
    bool testShipCameraFollowEnabled; /**< true si la camera suit le navire de test. */
    std::string loadedShipFolderAbsolute; /**< Chemin absolu du dossier navire charge. */
    bool pendingShipFolderDialogCompleted; /**< true si le callback folder a publie un resultat. */
    bool pendingShipFolderDialogCanceled; /**< true si l'utilisateur a annule l'import dossier navire. */
    std::string pendingShipFolderAbsolute; /**< Chemin absolu dossier navire en attente de traitement scene. */
    mutable std::mutex pendingShipFolderMutex; /**< Mutex callback dossier navire -> thread scene. */

    SDL_FRect buttonImportRect; /**< Bouton "IMPORTER ASSETS". */
    SDL_FRect buttonImportShipRect; /**< Bouton "IMPORTER NAVIRE". */
    SDL_FRect buttonExportRect; /**< Bouton "EXPORTER MAP". */
    SDL_FRect buttonUndoRect; /**< Bouton "Annuler". */
    SDL_FRect buttonRedoRect; /**< Bouton "Refaire". */
    SDL_FRect buttonToolBlockRect; /**< Bouton outil collision. */
    SDL_FRect buttonToolPlaceRect; /**< Bouton outil pose asset. */
    SDL_FRect buttonToolRemoveRect; /**< Bouton outil suppression asset. */
    SDL_FRect buttonToolShipRect; /**< Bouton outil spawn navire. */
    SDL_FRect buttonToolShipControlRect; /**< Bouton outil controle navire. */
    SDL_FRect buttonAssetPrevRect; /**< Bouton asset precedent. */
    SDL_FRect buttonAssetNextRect; /**< Bouton asset suivant. */
    SDL_FRect buttonOceanPrevRect; /**< Bouton ocean precedent. */
    SDL_FRect buttonOceanNextRect; /**< Bouton ocean suivant. */
    SDL_FRect buttonGridRect; /**< Bouton toggle lignes grille. */
    SDL_FRect buttonCenterRect; /**< Bouton recentrage camera map. */
    SDL_FRect buttonCenterShipRect; /**< Bouton recentrage/suivi navire test. */
    SDL_FRect buttonZoomOutRect; /**< Bouton zoom -. */
    SDL_FRect buttonZoomInRect; /**< Bouton zoom +. */
    SDL_FRect assetListRect; /**< Panneau liste assets (bas droite). */
    SDL_FRect miniMapRect; /**< Minimap editeur (haut droite). */
    bool miniMapDragActive; /**< true si drag minimap en cours. */
    float miniMapDragOffsetX; /**< Offset drag minimap en X. */
    float miniMapDragOffsetY; /**< Offset drag minimap en Y. */

    static EditorMapCreateMapScene* activeInstance; /**< Instance active pour callbacks async de file dialog. */

    /** @brief Reinitialise l'etat runtime de l'editeur. */
    void resetEditorState(void);
    /** @brief Libere toutes les textures d'assets importes. */
    void unloadImportedAssets(void);
    /** @brief Cree les dossiers user requis pour les imports. */
    void ensureUserStorageFolders(void);
    /** @brief Applique la couleur ocean selectionnee sur le shader ocean. */
    void applySelectedOceanColor(void);
    /** @brief Empile une demande de decalage de couleur ocean.
     *  @param delta Pas de rotation couleur (+1/-1).
     */
    void requestOceanColorStep(int delta);
    /** @brief Applique le delta ocean en attente (si present). */
    void applyPendingOceanColorStep(void);
    /** @brief Fait tourner directement la couleur ocean.
     *  @param delta Pas de rotation couleur (+1/-1).
     */
    void cycleOceanColor(int delta);
    /** @brief Convertit des coordonnees fenetre vers coordonnees render.
     *  @param windowX X fenetre.
     *  @param windowY Y fenetre.
     *  @param outX X rendu converti.
     *  @param outY Y rendu converti.
     */
    void convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const;
    /** @brief Verifie qu'un point rendu est dans le rect map.
     *  @param x Coord X render.
     *  @param y Coord Y render.
     *  @return true si le point est dans map.rect.
     */
    bool isInsideMapRect(float x, float y) const;
    /** @brief Convertit la souris courante en tuile map.
     *  @param outTile Tuile resultante.
     *  @return true si conversion valide et dans la map.
     */
    bool tryGetMouseTile(SDL_Point* outTile) const;
    /** @brief Met a jour la tuile survolee par la souris. */
    void updateHoveredTile(void);
    /** @brief Gere la peinture collision en drag souris. */
    void handleTilePaintFromMouseDrag(void);
    /** @brief Applique une collision avec historique.
     *  @param tileX Tuile X.
     *  @param tileY Tuile Y.
     *  @param blocked true=block, false=unblock.
     *  @return true si une modification a ete faite.
     */
    bool setTileBlockedWithHistory(int tileX, int tileY, bool blocked);
    /** @brief Peint la collision sous la souris.
     *  @param blocked true=block, false=unblock.
     */
    void paintTileAtMouse(bool blocked);
    /** @brief Pose l'asset selectionne sous la souris. */
    void placeSelectedAssetAtMouseTile(void);
    /** @brief Supprime un asset a la souris (hit visuel prioritaire). */
    void removeAssetAtMouseTile(void);
    /** @brief Cherche un asset pose exactement sur une tuile.
     *  @return Index dans placedAssets, sinon -1.
     */
    int findPlacedAssetIndexAtTile(int tileX, int tileY) const;
    /** @brief Recupere l'asset pose a une tuile.
     *  @return true si un asset est trouve.
     */
    bool getPlacedAssetAtTile(int tileX, int tileY, PlacedAsset* outAsset) const;
    /** @brief Ecrit l'etat asset sur une tuile.
     *  @param tileX Tuile X.
     *  @param tileY Tuile Y.
     *  @param hasAsset true si on veut un asset, false pour supprimer.
     *  @param assetState Etat asset source (si hasAsset=true).
     */
    void setPlacedAssetStateAtTile(int tileX, int tileY, bool hasAsset, const PlacedAsset* assetState);
    /** @brief Indique si undo est possible. */
    bool canUndoHistory(void) const;
    /** @brief Indique si redo est possible. */
    bool canRedoHistory(void) const;
    /** @brief Ajoute une action dans l'historique. */
    void pushHistoryAction(const HistoryAction& action);
    /** @brief Applique une action historique.
     *  @param action Action a appliquer.
     *  @param applyAfter true=etat apres, false=etat avant.
     */
    void applyHistoryAction(const HistoryAction& action, bool applyAfter);
    /** @brief Annule la derniere action. */
    void undoHistoryAction(void);
    /** @brief Reapplique la derniere action annulee. */
    void redoHistoryAction(void);
    /** @brief Importe un asset depuis un chemin absolu.
     *  @param absolutePath Chemin source image.
     *  @return true si import ok.
     */
    bool importAssetFromAbsolutePath(const char* absolutePath);
    /** @brief Traite les imports publies par le callback de file dialog. */
    void processPendingImportRequests(void);
    /** @brief Exporte la map JSON vers un chemin absolu.
     *  @param absolutePath Chemin destination.
     *  @return true si export ok.
     */
    bool exportMapToAbsolutePath(const char* absolutePath);
    /** @brief Exporte une image PNG de la minimap a partir de l'etat courant.
     *  @param jsonAbsolutePath Chemin JSON exporte (utilise pour deduire le nom PNG).
     *  @param outPngAbsolutePath [out] Recoit le chemin PNG genere si non nul.
     *  @return true si export PNG ok.
     */
    bool exportMiniMapPngFromJsonPath(const char* jsonAbsolutePath, std::string* outPngAbsolutePath) const;
    /** @brief Genere le rendu stylise minimap (terre/eau) dans une surface RGBA32.
     *  @param targetSurface Surface destination (taille finale voulue).
     *  @return true si generation ok.
     */
    bool renderStyledMiniMapToSurface(SDL_Surface* targetSurface) const;
    /** @brief Ouvre le dialogue d'import assets. */
    void openImportAssetDialog(void);
    /** @brief Ouvre le dialogue d'import d'un dossier navire (1.png..8.png). */
    void openImportShipFolderDialog(void);
    /** @brief Ouvre le dialogue d'export map. */
    void openExportMapDialog(void);
    /** @brief Traite l'import dossier navire publie par callback async. */
    void processPendingShipFolderRequest(void);
    /** @brief Charge un navire test depuis un dossier absolu.
     *  @param folderAbsolutePath Chemin absolu vers dossier 1..8.png.
     *  @return true si navire charge.
     */
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    /** @brief Place le navire test sur une tuile.
     *  @param tileX Tuile X.
     *  @param tileY Tuile Y.
     */
    void spawnTestShipAtTile(int tileX, int tileY);
    /** @brief Demande un deplacement A* du navire test.
     *  @param tileX Tuile cible X.
     *  @param tileY Tuile cible Y.
     */
    void moveTestShipToTile(int tileX, int tileY);
    /** @brief Met a jour le navire test et le suivi camera. */
    void updateTestShip(double dt);
    /** @brief Dessine le navire test. */
    void drawTestShip(void);
    /** @brief Gere les clics map en mode navire test.
     *  @return true si consomme.
     */
    bool handleShipToolClick(float x, float y, RC2D_MouseButton button);
    /** @brief Dessine la grille visible et les tuiles bloquees. */
    void drawWorldGridAndBlockedTiles(void) const;
    /** @brief Dessine les assets poses et leur preview de pose. */
    void drawPlacedAssets(void) const;
    /** @brief Dessine le HUD, les boutons et infos editeur. */
    void drawEditorHud(void) const;
    /** @brief Recalcule les rects des boutons/panneaux UI. */
    void updateToolbarLayout(void);
    /** @brief Dessine un bouton standard de toolbar. */
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    /** @brief Renvoie le max de scroll de liste assets. */
    int getAssetListMaxScrollOffset(void) const;
    /** @brief Clamp l'offset de scroll assets dans ses bornes. */
    void clampAssetListScrollOffset(void);
    /** @brief Garantit que l'asset selectionne est visible dans la liste. */
    void ensureSelectedAssetVisible(void);
    /** @brief Calcule l'index de debut rendu de la liste assets. */
    int computeAssetListStartIndex(void) const;
    /** @brief Gere un clic dans le panneau liste assets.
     *  @return true si le clic est consomme.
     */
    bool handleAssetListClick(float x, float y);
    /** @brief Gere le drag continu de la scrollbar liste assets. */
    void handleAssetListScrollDragFromMouse(void);
    /** @brief Dessine le panneau liste assets. */
    void drawAssetListPanel(void) const;
    /** @brief Construit le rect de vue courante pour la minimap.
     *  @return true si le rect est valide.
     */
    bool tryBuildMiniMapViewRect(SDL_FRect* outRect) const;
    /** @brief Deplace la camera depuis un point minimap. */
    void moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset);
    /** @brief Gere le clic minimap.
     *  @return true si le clic est consomme.
     */
    bool handleMiniMapClick(float x, float y, RC2D_MouseButton button);
    /** @brief Gere le drag minimap en continu. */
    void handleMiniMapDragFromMouse(void);
    /** @brief Dessine la minimap. */
    void drawMiniMap(void) const;
    /** @brief Test point dans rect. */
    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    /** @brief Gere un clic sur la toolbar/panneaux.
     *  @return true si consomme.
     */
    bool handleToolbarClick(float x, float y);
    /** @brief Recupere la souris en coordonnees render.
     *  @return true si disponible.
     */
    bool getMouseRenderPosition(float* outX, float* outY) const;

    /** @brief Callback async de resultat import fichier. */
    static void onImportAssetDialogResult(void* userdata, const char* const* filelist, int filter_index);
    /** @brief Callback async de resultat import dossier navire. */
    static void onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    /** @brief Callback async de resultat export fichier. */
    static void onExportMapDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    /** @brief Construit la scene editor map create map. */
    EditorMapCreateMapScene(void);
    /** @brief Destructeur de la scene. */
    ~EditorMapCreateMapScene(void) override;

    /** @brief Decharge les ressources de scene. */
    void unload(void) override;
    /** @brief Charge les ressources et initialise la scene. */
    void load(void) override;
    /** @brief Update frame de la scene.
     *  @param dt Delta time en secondes.
     */
    void update(double dt) override;
    /** @brief Dessine la scene. */
    void draw(void) override;
    /** @brief Callback clavier.
     *  @param key Texte touche.
     *  @param scancode Scancode SDL.
     *  @param keycode Keycode SDL.
     *  @param mod Modificateurs clavier.
     *  @param isrepeat true si repetition auto.
     *  @param keyboardID Identifiant clavier SDL.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    /** @brief Callback clic souris.
     *  @param x X souris en coordonnees render.
     *  @param y Y souris en coordonnees render.
     *  @param button Bouton clique.
     *  @param clicks Nombre de clics.
     *  @param mouseID Identifiant souris SDL.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};

#endif // GAME_ENV_DEV
