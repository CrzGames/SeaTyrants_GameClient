#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/overlay/tile-click-marker-overlay.h"
#include "game/ui/overlay/scroll-bar-overlay.h"

/**
 * @class EditorMapShipDownscaleScene
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
class EditorMapShipDownscaleScene : public Scene {
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
        CONTROL_SHIP = 4, /**< Controle du navire de test sur la map. */
        HOTSPOT_TOWERS = 5 /**< Selection hotspots de tours via assets poses. */
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
        bool useUserStorage; /**< true si storagePath pointe vers RC2D_STORAGE_USER. */
        bool loadFailed; /**< true si un chargement lazy a deja echoue. */
        RC2D_Image image; /**< Texture chargee par RC2D. */
        RC2D_ImageData imageData; /**< Surface source chargee (utile pour la minimap stylisee). */
        float widthPx; /**< Largeur native de l'image en pixels. */
        float heightPx; /**< Hauteur native de l'image en pixels. */
        int alphaMaskWidth; /**< Largeur du masque alpha source. */
        int alphaMaskHeight; /**< Hauteur du masque alpha source. */
        std::vector<Uint8> alphaMask; /**< Masque alpha linearise [0..255]. */
    };
    /**
     * @struct ImportedShip
     * @brief Donnees d'un navire importable depuis un dossier atlas.
     */
    struct ImportedShip {
        std::string displayName; /**< Nom affiche dans la liste navires. */
        std::string folderAbsolutePath; /**< Chemin absolu vers le dossier contenant 1.png..8.png. */
        std::string relativeExportPath; /**< Chemin relatif conserve pour l'export global. */
        std::string previewStorageFolderPath; /**< Dossier storage TITLE si ce navire vient des assets du jeu. */
        bool previewUseTitleStorage; /**< true si les previews peuvent etre empruntees depuis le cache TITLE. */
        int scalePercent; /**< Echelle propre au navire en % [5..100]. */
        std::array<RC2D_Image, 8> previewImages; /**< Textures preview chargees (1.png..8.png). */
        std::array<RC2D_ImageData, 8> previewImageData; /**< Surfaces preview chargees (1.png..8.png). */
        std::array<bool, 8> previewSpriteLoaded; /**< true si le sprite correspondant est charge. */
        float previewWidthPx; /**< Largeur native max preview en pixels (pour layout). */
        float previewHeightPx; /**< Hauteur native max preview en pixels (pour layout). */
        int previewSpriteIndex; /**< Index sprite actuellement charge [1..8]. */
        float anchorTileX; /**< Position ancree X sur la map (layout auto). */
        float anchorTileY; /**< Position ancree Y sur la map (layout auto). */
        float layoutScreenX; /**< Position ecran X en mode OFF (grille spritesheet). */
        float layoutScreenY; /**< Position ecran Y en mode OFF (grille spritesheet). */
        float simulationVelocityTileX; /**< Vitesse simulation X en tuiles/s. */
        float simulationVelocityTileY; /**< Vitesse simulation Y en tuiles/s. */
        std::unique_ptr<Ship> simulationShip; /**< Runtime Ship utilise en mode simulation ON. */
        int simulationNextDirectionIndex; /**< Direction reseau suivante [0..7] (style crashtest). */
        double simulationPauseBeforeNextCommandSec; /**< Pause idle avant prochain move reseau. */
        uint32_t simulationCommandRngState; /**< RNG local pour fallback cibles reseau. */
        double simulationCommandCooldownSec; /**< Cooldown reseau avant prochain move. */
    };
    /**
     * @struct PendingShipImport
     * @brief Item de batch pour importer les dossiers navire progressivement.
     */
    struct PendingShipImport {
        std::string displayName; /**< Nom a afficher dans la liste navires. */
        std::string folderAbsolutePath; /**< Dossier absolu source du navire. */
        std::string relativeExportPath; /**< Chemin relatif a reproduire a l'export. */
        std::string previewStorageFolderPath; /**< Dossier storage TITLE si le dossier source est dans assets/images/. */
        bool previewUseTitleStorage; /**< true si les previews doivent lire directement le cache TITLE. */
    };
    /**
     * @struct SpawnedShipInstance
     * @brief Instance de navire placee manuellement sur la map (simulation OFF).
     */
    struct SpawnedShipInstance {
        int importedShipIndex; /**< Index du navire source dans importedShips. */
        float tileX; /**< Position map en tuiles (X). */
        float tileY; /**< Position map en tuiles (Y). */
        int scalePercent; /**< Scale capturee a la pose [%]. */
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
    struct TowerHotspot {
        int tileX; /**< Tuile hotspot X. */
        int tileY; /**< Tuile hotspot Y. */
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
            ASSET_AT_TILE = 1, /**< Changement d'asset pose sur une tuile. */
            TILE_BLOCK_BATCH = 2 /**< Changement collision multi-tuiles (brosse). */
        };
        struct TileBlockChange {
            int tileX;
            int tileY;
            bool beforeBlocked;
            bool afterBlocked;
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
        std::vector<TileBlockChange> tileBlockBatch; /**< Liste des changements collision d'une brosse. */
    };

    BackgroundWidget backgroundWidget; /**< Fond UI (haut/bas) de la scene. */
    RC2D_Font overlayFont; /**< Police des overlays et boutons. */
    ScrollBarOverlay scrollBarOverlay; /**< Overlay scrollbar reutilise en mode editeur. */

    EditorMode editorMode; /**< Mode metier courant. */
    EditorTool editorTool; /**< Outil courant selectionne. */
    int selectedOceanColorIndex; /**< Index couleur ocean active. */
    int pendingOceanColorDelta; /**< Delta ocean en attente d'application. */
    int selectedAssetIndex; /**< Index asset actuellement selectionne. */
    int selectedShipIndex; /**< Index navire actuellement selectionne. */
    int assetListScrollOffset; /**< Offset de scroll de la liste assets. */
    int shipListScrollOffset; /**< Offset de scroll de la liste navires. */
    bool showGrid; /**< Affichage grille isometrique ON/OFF. */
    bool showBlockedTiles; /**< Affichage visuel des tuiles bloquees ON/OFF. */
    bool showBottomRightLists; /**< Affichage des listes navires/assets. */
    bool collisionPaintBlocks; /**< true=mode peinture collision, false=mode suppression collision. */
    int blockedBrushRadiusTiles; /**< Rayon de paint collision (0 = 1 tuile). */
    bool assetTransparencyEnabled; /**< true si l'opacite globale assets est active. */
    int assetOpacityPercent; /**< Opacite globale assets en pourcentage [10..100]. */
    int selectedBlockedColorIndex; /**< Index couleur des tuiles bloquees. */
    int selectedHotspotColorIndex; /**< Index couleur des hotspots tours. */
    int shipScalePercent; /**< Echelle navire selectionne en % [5..100]. */

    bool hoveredTileValid; /**< true si la souris survole une tuile map. */
    SDL_Point hoveredTile; /**< Tuile actuellement survolee. */
    bool dragPaintActive; /**< true si un drag paint collision est actif. */
    bool dragPaintBlockedValue; /**< Valeur de paint collision du drag courant. */
    bool lastDragPaintTileValid; /**< true si la derniere tuile drag est valide. */
    SDL_Point lastDragPaintTile; /**< Derniere tuile peinte en drag. */

    std::vector<ImportedAsset> importedAssets; /**< Bibliotheque assets importes. */
    std::vector<ImportedShip> importedShips; /**< Bibliotheque navires importes depuis dossiers. */
    std::vector<PlacedAsset> placedAssets; /**< Assets poses sur la map. */
    std::vector<TowerHotspot> towerHotspots; /**< Hotspots tours poses sur la map. */
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
    bool shipImportBatchActive; /**< true si le batch de dossiers navires est actif. */
    std::vector<PendingShipImport> shipImportBatchFolders; /**< Queue navires a importer progressivement. */
    size_t shipImportBatchNextIndex; /**< Index du prochain dossier navire a importer. */
    int shipImportBatchImportedCount; /**< Nombre de navires importes dans le batch. */
    int shipImportBatchFailedCount; /**< Nombre de navires en echec dans le batch. */
    std::vector<SpawnedShipInstance> spawnedShips; /**< Navires poses manuellement (mode simulation OFF). */
    unsigned int importedShipPreviewCounter; /**< Compteur de previews navire en storage user. */
    bool importedShipsLayoutDirty; /**< true si le layout auto des navires doit etre recalcule. */
    bool shipsSimulationEnabled; /**< true si les navires se baladent en simulation. */
    bool shipSpawnPlacementEnabled; /**< true si clic gauche map = spawn navire selectionne. */
    bool shipsLowHpEnabled; /**< true si l'affichage navires utilise les sprites low HP. */
    double shipsPreviewAnimationAccumulator; /**< Accumulateur animation preview navires. */
    double shipsPreviewAnimationIntervalSeconds; /**< Periode animation preview navires. */
    int shipsPreviewFrameOffset; /**< Frame partagee [0..7] appliquee a tous les navires. */
    std::size_t shipsSimulationRetargetCursor; /**< Curseur round-robin retarget simulation. */
    double shipsSimulationRetargetAccumulatorSec; /**< Tick fixe pour budget A* simulation. */
    std::size_t deferredShipPreviewLoadCursor; /**< Curseur round-robin de chargement preview lazy. */
    std::size_t deferredAssetLoadCursor; /**< Curseur round-robin de chargement assets lazy. */
    bool initialShipsLoadingScreenActive; /**< true si l'ecran de chargement initial navires est actif. */
    int initialShipsLoadingTotal; /**< Nombre total de navires a charger au boot scene. */
    int initialShipsLoadingProcessed; /**< Nombre de navires deja traites pour la progression boot. */
    int shipImportBatchFramesUntilNextShip; /**< Throttle boot: nombre de frames a attendre avant le prochain navire. */
    bool shipScaleInputActive; /**< true si le champ % downscale est en edition clavier. */
    std::string shipScaleInputBuffer; /**< Buffer texte du champ % downscale. */
    bool assetListScrollDragActive; /**< true si le drag de la scrollbar assets est actif. */
    float assetListScrollDragGrabOffsetY; /**< Offset vertical curseur->thumb pour un drag precis. */
    bool shipListScrollDragActive; /**< true si le drag de la scrollbar navires est actif. */
    float shipListScrollDragGrabOffsetY; /**< Offset vertical curseur->thumb pour drag liste navires. */
    TileClickMarkerOverlay clickMarker; /**< Marqueur visuel de clic en mode controle navire. */
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
    bool pendingMapImportDialogCompleted; /**< true si callback import map a publie un resultat. */
    bool pendingMapImportDialogCanceled; /**< true si import map annule. */
    std::string pendingMapImportAbsolutePath; /**< Chemin map JSON a importer. */
    mutable std::mutex pendingMapImportMutex; /**< Mutex callback import map -> thread scene. */

    SDL_FRect buttonImportRect; /**< Bouton "IMPORTER ASSETS". */
    SDL_FRect buttonExportRect; /**< Bouton "EXPORTER MAP". */
    SDL_FRect buttonUndoRect; /**< Bouton "Annuler". */
    SDL_FRect buttonRedoRect; /**< Bouton "Refaire". */
    SDL_FRect buttonToolPlaceRect; /**< Bouton outil pose asset. */
    SDL_FRect buttonToolRemoveRect; /**< Bouton outil suppression asset. */
    SDL_FRect buttonAssetPrevRect; /**< Bouton asset precedent. */
    SDL_FRect buttonAssetNextRect; /**< Bouton asset suivant. */
    SDL_FRect buttonOceanPrevRect; /**< Bouton ocean precedent. */
    SDL_FRect buttonOceanNextRect; /**< Bouton ocean suivant. */
    SDL_FRect buttonGridRect; /**< Bouton toggle lignes grille. */
    SDL_FRect buttonCenterRect; /**< Bouton recentrage camera map. */
    SDL_FRect buttonZoomOutRect; /**< Bouton zoom -. */
    SDL_FRect buttonZoomInRect; /**< Bouton zoom +. */
    SDL_FRect buttonShipScaleMinusRect; /**< Bouton scale navire -. */
    SDL_FRect buttonShipScalePlusRect; /**< Bouton scale navire +. */
    SDL_FRect buttonShipSpawnRect; /**< Bouton ON/OFF spawn navire sur clic gauche map. */
    SDL_FRect buttonSimulationRect; /**< Bouton ON/OFF simulation navires. */
    SDL_FRect buttonShipsHpRect; /**< Bouton toggle FULL HP/BAS HP navires. */
    SDL_FRect buttonListsVisibilityRect; /**< Bouton ON/OFF affichage des 3 listes. */
    SDL_FRect shipScaleInputRect; /**< Champ de saisie % downscale navire. */
    SDL_FRect assetListRect; /**< Panneau liste assets (bas droite). */
    SDL_FRect shipListRect; /**< Panneau liste navires (meme format que assets). */
    SDL_FRect miniMapRect; /**< Minimap editeur (haut droite). */
    bool miniMapDragActive; /**< true si drag minimap en cours. */
    float miniMapDragOffsetX; /**< Offset drag minimap en X. */
    float miniMapDragOffsetY; /**< Offset drag minimap en Y. */

    static EditorMapShipDownscaleScene* activeInstance; /**< Instance active pour callbacks async de file dialog. */

    /** @brief Reinitialise l'etat runtime de l'editeur. */
    void resetEditorState(void);
    /** @brief Libere toutes les textures d'assets importes. */
    void unloadImportedAssets(void);
    /** @brief Libere toutes les previews de navires importes. */
    void unloadImportedShips(void);
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
    /** @brief Applique la brosse collision avec historique batch. */
    bool applyTileBrushWithHistory(int centerTileX, int centerTileY, bool blocked);
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
    /** @brief Charge a la demande un asset importe (texture + surface + alpha mask). */
    bool ensureImportedAssetLoaded(ImportedAsset* asset);
    /** @brief Charge progressivement les assets importes manquants pour eviter les freeze UI. */
    void processDeferredAssetLoads(void);
    /** @brief Traite les imports publies par le callback de file dialog. */
    void processPendingImportRequests(void);
    /** @brief Exporte la map JSON vers un chemin absolu.
     *  @param absolutePath Chemin destination.
     *  @return true si export ok.
     */
    bool exportMapToAbsolutePath(const char* absolutePath);
    /** @brief Exporte la map vers un dossier en generant map.json + minimap.png.
     *  @param absoluteFolderPath Dossier cible.
     *  @return true si export ok.
     */
    bool exportMapToFolder(const char* absoluteFolderPath);
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
    /** @brief Ouvre le dialogue d'import map.json. */
    void openImportMapDialog(void);
    /** @brief Ouvre le dialogue d'import d'un dossier racine navires (scan recursif). */
    void openImportShipFolderDialog(void);
    /** @brief Importe recursivement des dossiers navires depuis un dossier racine. */
    bool importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);
    /** @brief Traite le batch d'import navires de facon progressive. */
    void processShipImportBatch(void);
    /** @brief Importe un navire depuis un item de batch. */
    bool importSingleShipFromBatch(const PendingShipImport& pendingShip);
    /** @brief Ouvre le dialogue d'export map (selection dossier). */
    void openExportMapDialog(void);
    /** @brief Traite l'import map publie par callback async. */
    void processPendingMapImportRequest(void);
    /** @brief Importe une map depuis un JSON absolu. */
    bool importMapFromAbsolutePath(const char* absolutePath);
    /** @brief Importe (ou reutilise) un asset depuis un path runtime JSON. */
    int importAssetFromRuntimeStoragePath(const std::string& runtimePath);
    /** @brief Retourne true si un asset est considere comme une tour. */
    bool isTowerAssetName(const std::string& displayName) const;
    /** @brief Trouve l'asset pose le plus proche sous un point ecran. */
    int findPlacedAssetIndexAtScreenPoint(float x, float y) const;
    /** @brief Calcule la tuile centre d'un asset pose. */
    SDL_Point computePlacedAssetCenterTile(const PlacedAsset& asset) const;
    /** @brief Toggle un hotspot tour. */
    void toggleTowerHotspotAtTile(int tileX, int tileY);
    /** @brief Ajuste l'opacite globale assets en %. */
    void setAssetOpacityPercent(int value);
    /** @brief Ajuste l'echelle navire test en %. */
    void setShipScalePercent(int value);
    /** @brief Reexporte les 8 sprites navire avec scale. */
    bool reexportLoadedShipScaled(int scalePercent);
    /** @brief Exporte tous les dossiers navires avec scale par navire. */
    bool exportAllShipsScaledToFolder(const char* absoluteFolderPath);
    /** @brief Copie tous les fichiers d'un dossier navire vers un dossier destination. */
    bool copyShipFolderBaseFiles(const char* sourceFolderAbsolutePath, const char* destinationFolderAbsolutePath) const;
    /** @brief Ecrit les sprites 1..8 downscalees d'un navire dans un dossier destination. */
    bool exportScaledShipSpritesToFolder(const ImportedShip& ship, const char* destinationFolderAbsolutePath) const;
    /** @brief Charge une preview navire depuis un chemin storage RC2D. */
    bool loadShipPreviewImageFromStoragePath(
        ImportedShip* ship,
        int spriteIndex,
        const char* storagePath,
        RC2D_StorageKind storageKind);
    /** @brief Charge la preview 1.png d'un navire depuis un chemin absolu. */
    bool loadShipPreviewImageFromAbsolutePath(ImportedShip* ship, int spriteIndex, const char* spriteAbsolutePath);
    /** @brief Charge a la demande un sprite preview pour un navire importe. */
    bool ensureImportedShipPreviewSpriteLoaded(ImportedShip* ship, int spriteIndex);
    /** @brief Charge progressivement les previews navires manquantes pour eviter les freeze UI. */
    void processDeferredShipPreviewLoads(void);
    /** @brief Recalcule les positions de tous les navires importes sur la map. */
    void recomputeImportedShipLayout(void);
    /** @brief Met a jour les positions des navires lorsque la simulation est active. */
    void updateImportedShipsSimulation(double dt);
    /** @brief Active/desactive la simulation de deplacement des navires. */
    void setShipsSimulationEnabled(bool enabled);
    /** @brief Active/desactive le mode BAS HP pour tous les navires. */
    void setShipsLowHpEnabled(bool enabled);
    /** @brief Recharge les previews navire selon le mode FULL HP/BAS HP courant. */
    void applyShipsPreviewForCurrentHealthVisual(void);
    /** @brief Renvoie l'index sprite courant [1..8] pour l'animation OFF en grille. */
    int computeCurrentShipPreviewSpriteIndex(void) const;
    /** @brief Met a jour toutes les previews navires pour la frame OFF en grille. */
    void refreshImportedShipsPreviewForCurrentFrame(bool updateStatusMessageOnFailure);
    /** @brief Place chaque navire a une position aleatoire et vitesse aleatoire. */
    void randomizeImportedShipsSimulationState(void);
    /** @brief Calcule la prochaine cible reseau d'un navire simule (style crashtest). */
    bool computeNextSimulationTargetTileForImportedShip(ImportedShip* ship, SDL_Point* outTargetTile);
    /** @brief Envoie un ordre de move reseau simule a un navire (style crashtest). */
    bool issueNextSimulationMoveForImportedShip(ImportedShip* ship);
    /** @brief Fait avancer l'animation sprite partagee des navires. */
    void advanceImportedShipsSpriteAnimation(double dt);
    /** @brief Traite l'import dossier navire publie par callback async. */
    void processPendingShipFolderRequest(void);
    /** @brief Charge un navire test depuis un dossier absolu.
     *  @param folderAbsolutePath Chemin absolu vers dossier 1..8.png.
     *  @return true si navire charge.
     */
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    /** @brief Selectionne un navire importe et le charge en navire test. */
    bool selectImportedShipAtIndex(int shipIndex);
    /** @brief Centre la camera sur un navire importe (simulation ON/OFF). */
    void centerCameraOnImportedShipIndex(int shipIndex);
    /** @brief Spawn une instance du navire selectionne sur une tuile map. */
    void spawnSelectedImportedShipAtTile(int tileX, int tileY);
    /** @brief Active l'edition clavier du champ % downscale. */
    void beginShipScaleInputEdit(void);
    /** @brief Valide l'edition du champ % downscale. */
    void commitShipScaleInputEdit(void);
    /** @brief Annule l'edition du champ % downscale. */
    void cancelShipScaleInputEdit(void);
    /** @brief Traite une touche quand le champ % downscale est actif. */
    bool handleShipScaleInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
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
    /** @brief Dessine tous les navires importes (preview 1.png) sur la map. */
    void drawImportedShips(void) const;
    /** @brief Dessine le HUD, les boutons et infos editeur. */
    void drawEditorHud(void) const;
    /** @brief Recalcule les rects des boutons/panneaux UI. */
    void updateToolbarLayout(void);
    /** @brief Dessine un bouton standard de toolbar. */
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    /** @brief Renvoie le max de scroll de liste assets. */
    int getAssetListMaxScrollOffset(void) const;
    /** @brief Renvoie le max de scroll de liste navires. */
    int getShipListMaxScrollOffset(void) const;
    /** @brief Clamp l'offset de scroll assets dans ses bornes. */
    void clampAssetListScrollOffset(void);
    /** @brief Clamp l'offset de scroll navires dans ses bornes. */
    void clampShipListScrollOffset(void);
    /** @brief Garantit que l'asset selectionne est visible dans la liste. */
    void ensureSelectedAssetVisible(void);
    /** @brief Garantit que le navire selectionne est visible dans la liste. */
    void ensureSelectedShipVisible(void);
    /** @brief Calcule l'index de debut rendu de la liste assets. */
    int computeAssetListStartIndex(void) const;
    /** @brief Calcule l'index de debut rendu de la liste navires. */
    int computeShipListStartIndex(void) const;
    /** @brief Gere un clic dans le panneau liste assets.
     *  @return true si le clic est consomme.
     */
    bool handleAssetListClick(float x, float y);
    /** @brief Gere un clic dans le panneau liste navires.
     *  @return true si le clic est consomme.
     */
    bool handleShipListClick(float x, float y);
    /** @brief Gere le drag continu de la scrollbar liste assets. */
    void handleAssetListScrollDragFromMouse(void);
    /** @brief Gere le drag continu de la scrollbar liste navires. */
    void handleShipListScrollDragFromMouse(void);
    /** @brief Dessine le panneau liste assets. */
    void drawAssetListPanel(void) const;
    /** @brief Dessine le panneau liste navires. */
    void drawShipListPanel(void) const;
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
    /** @brief Callback async de resultat import map JSON. */
    static void onImportMapDialogResult(void* userdata, const char* const* filelist, int filter_index);
    /** @brief Callback async de resultat import dossier navire. */
    static void onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    /** @brief Callback async de resultat export dossier. */
    static void onExportMapDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    /** @brief Construit la scene editor map ship downscale. */
    EditorMapShipDownscaleScene(void);
    /** @brief Destructeur de la scene. */
    ~EditorMapShipDownscaleScene(void) override;

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
    /** @brief Callback molette souris pour zoom camera et scroll des listes. */
    void mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID) override;
};

#endif // GAME_ENV_DEV

