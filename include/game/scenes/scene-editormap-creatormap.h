#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/scenes/editormap-map-interaction.h"
#include "game/ships/ship.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/overlay/tile-click-marker-overlay.h"
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
        INTERACT_ASSETS = 3, /**< Edition des GUI/rayons d'interaction des assets poses. */
        SPAWN_SHIP = 4, /**< Spawn/repositionnement du navire de test via preview. */
        CONTROL_SHIP = 5, /**< Controle du navire de test sur la map. */
        HOTSPOT_TOWERS = 6, /**< Selection hotspots de tours via assets poses. */
        HOTSPOT_MORTARS = 7 /**< Selection hotspots de mortiers via assets poses. */
    };
    enum class AssetInteractionEditMode {
        SELECT = 0, /**< Selectionne l'asset et la GUI cible. */
        PAINT_ADD = 1, /**< Ajoute des tuiles d'interaction. */
        PAINT_REMOVE = 2 /**< Retire des tuiles d'interaction. */
    };
    enum class TowerPreviewDisplayMode {
        HOTSPOTS = 0, /**< Affiche les tuiles hotspots. */
        TOWERS = 1 /**< Affiche les tours d'un niveau choisi. */
    };
    enum class TowerPlacementStage {
        LEVEL_1 = 0, /**< Placement pixel perfect de la tour niveau 1. */
        LEVEL_2 = 1, /**< Placement pixel perfect de la tour niveau 2. */
        LEVEL_3 = 2, /**< Placement pixel perfect de la tour niveau 3. */
        LEVEL_4 = 3, /**< Placement pixel perfect de la tour niveau 4. */
        FIRE_HOTSPOT = 4 /**< Placement de la tuile de tir / hotspot. */
    };
    enum class MortarPlacementStage {
        LEVEL_1 = 0, /**< Placement pixel perfect du mortier niveau 1. */
        LEVEL_2 = 1, /**< Placement pixel perfect du mortier niveau 2. */
        LEVEL_3 = 2, /**< Placement pixel perfect du mortier niveau 3. */
        FIRE_HOTSPOT = 3 /**< Placement de la tuile de tir / hotspot du mortier. */
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
        EditorMapAssetClickGuiTarget clickGuiTarget = EditorMapAssetClickGuiTarget::NONE; /**< GUI ouverte au clic sur l'asset. */
        int clickGuiDistanceTiles = kEditorMapDefaultAssetClickDistanceTiles; /**< Rayon legacy importe depuis les anciens JSON. */
        bool clickGuiUsesLegacyRadius = false; /**< true si l'asset n'a pas encore de tuiles explicites et repose sur l'ancien rayon. */
        std::vector<SDL_Point> clickGuiTiles; /**< Tuiles autorisees pour ouvrir la GUI de cet asset. */
    };
    struct TowerHotspot {
        float anchorTileX = 0.0f; /**< Ancre sub-tile X du point de tir. */
        float anchorTileY = 0.0f; /**< Ancre sub-tile Y du point de tir. */
        int towerNumber = 1; /**< Numero logique de la tower [1..12]. */
    };
    struct TowerVisualSet {
        int towerNumber = 1; /**< Numero logique de la tower [1..12]. */
        std::array<int, 4> importedAssetIndices = {{-1, -1, -1, -1}}; /**< Variantes lvl1..lvl4. */
        std::array<float, 4> anchorTileX = {{0.0f, 0.0f, 0.0f, 0.0f}}; /**< Ancres pixel perfect par niveau. */
        std::array<float, 4> anchorTileY = {{0.0f, 0.0f, 0.0f, 0.0f}}; /**< Ancres pixel perfect par niveau. */
        std::array<bool, 4> hasPlacement = {{false, false, false, false}}; /**< true si l'ancre du niveau est definie. */
    };
    struct MortarHotspot {
        float anchorTileX = 0.0f; /**< Ancre sub-tile X du point de tir. */
        float anchorTileY = 0.0f; /**< Ancre sub-tile Y du point de tir. */
        int mortarNumber = 1; /**< Numero logique du mortier [1..12]. */
    };
    struct MortarVisualSet {
        int mortarNumber = 1; /**< Numero logique du mortier [1..12]. */
        std::array<int, 3> importedAssetIndices = {{-1, -1, -1}}; /**< Variantes lvl1..lvl3. */
        std::array<float, 3> anchorTileX = {{0.0f, 0.0f, 0.0f}}; /**< Ancres pixel perfect par niveau. */
        std::array<float, 3> anchorTileY = {{0.0f, 0.0f, 0.0f}}; /**< Ancres pixel perfect par niveau. */
        std::array<bool, 3> hasPlacement = {{false, false, false}}; /**< true si l'ancre du niveau est definie. */
    };
    enum class WorldMapTextField {
        NONE = 0, /**< Aucun champ texte selectionne. */
        MAP_NAME = 1, /**< Texte principal de la case (nom map / coordonnee). */
        PREVIEW_WIDTH = 2, /**< Largeur de la GUI carte du monde. */
        PREVIEW_HEIGHT = 3, /**< Hauteur de la GUI carte du monde. */
        AUTO_LAYOUT = 4 /**< Modele de lignes auto, ex: 5-3-2. */
    };
    struct WorldMapCase {
        float x = 0.0f; /**< Position X dans le canvas local. */
        float y = 0.0f; /**< Position Y dans le canvas local. */
        float width = 96.0f; /**< Largeur de la case en pixels canvas. */
        float height = 96.0f; /**< Hauteur de la case en pixels canvas. */
        std::string mapName; /**< Texte principal affiche au centre. */
        bool showGuildName = false; /**< true si un TAG GUILDE doit pouvoir etre affiche au-dessus. */
        bool isCity = false; /**< true si la case represente une ville (rouge), sinon une map normale (orange). */
    };
    enum class WorldMapLinkSide {
        LEFT = 0,
        TOP = 1,
        RIGHT = 2,
        BOTTOM = 3
    };
    struct WorldMapLink {
        int fromCaseIndex = -1; /**< Index de la case source. */
        int toCaseIndex = -1; /**< Index de la case cible. */
        WorldMapLinkSide fromSide = WorldMapLinkSide::RIGHT; /**< Cote source utilise pour sortir. */
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
    RC2D_Image worldMapScaleIcon; /**< Icone de redimensionnement pour la GUI gameplay. */
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
    std::string mapNameInput; /**< Nom map saisi dans l'input. */
    bool mapNameInputFocused; /**< true si l'input nom map a le focus clavier. */
    int blockedBrushRadiusTiles; /**< Rayon de paint collision (0 = 1 tuile). */
    bool assetTransparencyEnabled; /**< true si l'opacite globale assets est active. */
    int assetOpacityPercent; /**< Opacite globale assets en pourcentage [10..100]. */
    int selectedBlockedColorIndex; /**< Index couleur des tuiles bloquees. */
    int selectedHotspotColorIndex; /**< Index couleur des hotspots tours. */
    int shipScalePercent; /**< Echelle navire test en % [10..100]. */
    int selectedTowerHotspotNumber = 1; /**< Numero de tower assigne au prochain hotspot [1..12]. */
    int selectedTowerVariantLevel = 1; /**< Niveau de variante de tower en cours d'edition [1..4]. */
    int towerPreviewDisplayLevel = 1; /**< Niveau actuellement previsualise pour tous les hotspots [1..4]. */
    TowerPreviewDisplayMode towerPreviewDisplayMode = TowerPreviewDisplayMode::HOTSPOTS; /**< Mode d'affichage hotspot/towers. */
    TowerPlacementStage towerPlacementStage = TowerPlacementStage::LEVEL_1; /**< Etape courante du workflow de pose tower. */
    int selectedMortarHotspotNumber = 1; /**< Numero de mortier assigne au prochain hotspot [1..12]. */
    int selectedMortarVariantLevel = 1; /**< Niveau de variante de mortier en cours d'edition [1..3]. */
    int mortarPreviewDisplayLevel = 1; /**< Niveau actuellement previsualise pour tous les mortiers [1..3]. */
    TowerPreviewDisplayMode mortarPreviewDisplayMode = TowerPreviewDisplayMode::HOTSPOTS; /**< Mode d'affichage hotspot/mortiers. */
    MortarPlacementStage mortarPlacementStage = MortarPlacementStage::LEVEL_1; /**< Etape courante du workflow de pose mortier. */
    EditorMapAssetClickGuiTarget selectedAssetClickGuiTarget = EditorMapAssetClickGuiTarget::NONE; /**< GUI appliquee aux prochains assets poses. */
    int selectedAssetClickDistanceTiles = kEditorMapDefaultAssetClickDistanceTiles; /**< Rayon legacy conserve pour compatibilite import. */
    int selectedPlacedAssetIndex = -1; /**< Asset deja pose actuellement selectionne pour edition interaction. */
    int assetInteractionListScrollOffset = 0; /**< Scroll de la liste des GUI d'interaction quand un asset pose est selectionne. */
    AssetInteractionEditMode assetInteractionEditMode = AssetInteractionEditMode::SELECT; /**< Sous-mode de l'outil interaction asset. */

    bool hoveredTileValid; /**< true si la souris survole une tuile map. */
    SDL_Point hoveredTile; /**< Tuile actuellement survolee. */
    bool dragPaintActive; /**< true si un drag paint collision est actif. */
    bool dragPaintBlockedValue; /**< Valeur de paint collision du drag courant. */
    bool lastDragPaintTileValid; /**< true si la derniere tuile drag est valide. */
    SDL_Point lastDragPaintTile; /**< Derniere tuile peinte en drag. */
    bool assetInteractionPaintActive = false; /**< true si un paint de tuiles d'interaction est en cours. */
    bool assetInteractionPaintValue = true; /**< true=ajoute des tuiles, false=en retire. */
    bool lastAssetInteractionPaintTileValid = false; /**< true si la derniere tuile peinte pour l'interaction est valide. */
    SDL_Point lastAssetInteractionPaintTile; /**< Derniere tuile peinte pour l'interaction d'un asset. */

    std::vector<ImportedAsset> importedAssets; /**< Bibliotheque assets importes. */
    std::vector<ImportedShip> importedShips; /**< Bibliotheque navires importes depuis dossiers. */
    std::vector<PlacedAsset> placedAssets; /**< Assets poses sur la map. */
    std::vector<TowerHotspot> towerHotspots; /**< Hotspots tours poses sur la map. */
    std::vector<TowerVisualSet> towerVisualSets; /**< Variantes d'assets configurees par towerNumber. */
    std::vector<MortarHotspot> mortarHotspots; /**< Hotspots mortiers poses sur la map. */
    std::vector<MortarVisualSet> mortarVisualSets; /**< Variantes d'assets configurees par mortarNumber. */
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
    bool pendingWorldMapImportDialogCompleted; /**< true si callback import world map a publie un resultat. */
    bool pendingWorldMapImportDialogCanceled; /**< true si import world map annule. */
    std::string pendingWorldMapImportAbsolutePath; /**< Chemin worldmap.json a importer. */
    mutable std::mutex pendingWorldMapImportMutex; /**< Mutex callback import world map -> thread scene. */
    bool pendingWorldMapExportDialogCompleted; /**< true si callback export world map a publie un resultat. */
    bool pendingWorldMapExportDialogCanceled; /**< true si export world map annule. */
    std::string pendingWorldMapExportAbsolutePath; /**< Dossier cible du prochain export world map. */
    mutable std::mutex pendingWorldMapExportMutex; /**< Mutex callback export world map -> thread scene. */

    SDL_FRect buttonImportRect; /**< Bouton "IMPORTER ASSETS". */
    SDL_FRect buttonImportMapRect; /**< Bouton "IMPORTER MAP JSON". */
    SDL_FRect buttonImportShipRect; /**< Bouton "IMPORTER NAVIRES". */
    SDL_FRect buttonExportRect; /**< Bouton "EXPORTER MAP". */
    SDL_FRect buttonCreateWorldMapRect; /**< Bouton ouverture createur carte du monde. */
    SDL_FRect buttonUndoRect; /**< Bouton "Annuler". */
    SDL_FRect buttonRedoRect; /**< Bouton "Refaire". */
    SDL_FRect buttonToolBlockRect; /**< Bouton outil collision. */
    SDL_FRect buttonToolUnblockRect; /**< Bouton outil suppression collision. */
    SDL_FRect buttonToolPlaceRect; /**< Bouton outil pose asset. */
    SDL_FRect buttonToolRemoveRect; /**< Bouton outil suppression asset. */
    SDL_FRect buttonToolInteractRect; /**< Bouton outil edition interaction asset. */
    SDL_FRect buttonToolShipRect; /**< Bouton outil spawn navire. */
    SDL_FRect buttonToolShipControlRect; /**< Bouton outil controle navire. */
    SDL_FRect buttonToolHotspotRect; /**< Bouton outil hotspots tours. */
    SDL_FRect buttonTowerVariantsPickerRect; /**< Bouton ouverture popup variantes towers. */
    SDL_FRect buttonTowerDisplayModeRect; /**< Bouton affichage hotspots/towers. */
    SDL_FRect buttonToolMortarHotspotRect; /**< Bouton outil hotspots mortiers. */
    SDL_FRect buttonMortarVariantsPickerRect; /**< Bouton ouverture popup variantes mortiers. */
    SDL_FRect buttonMortarDisplayModeRect; /**< Bouton affichage hotspots/mortiers. */
    SDL_FRect buttonAssetPrevRect; /**< Bouton asset precedent. */
    SDL_FRect buttonAssetNextRect; /**< Bouton asset suivant. */
    SDL_FRect buttonOceanPrevRect; /**< Bouton ocean precedent. */
    SDL_FRect buttonOceanNextRect; /**< Bouton ocean suivant. */
    SDL_FRect buttonGridRect; /**< Bouton toggle lignes grille. */
    SDL_FRect buttonBlockedTilesRect; /**< Bouton toggle affichage des tiles bloquees. */
    SDL_FRect buttonCenterRect; /**< Bouton recentrage camera map. */
    SDL_FRect buttonCenterShipRect; /**< Bouton recentrage/suivi navire test. */
    SDL_FRect buttonZoomOutRect; /**< Bouton zoom -. */
    SDL_FRect buttonZoomInRect; /**< Bouton zoom +. */
    SDL_FRect buttonBlockedBrushMinusRect; /**< Bouton radius blocked -. */
    SDL_FRect buttonBlockedBrushPlusRect; /**< Bouton radius blocked +. */
    SDL_FRect buttonBlockedColorPrevRect; /**< Bouton couleur blocked -. */
    SDL_FRect buttonBlockedColorNextRect; /**< Bouton couleur blocked +. */
    SDL_FRect buttonHotspotColorPrevRect; /**< Bouton couleur hotspot -. */
    SDL_FRect buttonHotspotColorNextRect; /**< Bouton couleur hotspot +. */
    SDL_FRect buttonAssetOpacityToggleRect; /**< Bouton toggle opacite globale assets. */
    SDL_FRect buttonAssetOpacityMinusRect; /**< Bouton opacite assets -. */
    SDL_FRect buttonAssetOpacityPlusRect; /**< Bouton opacite assets +. */
    SDL_FRect buttonShipScaleMinusRect; /**< Bouton scale navire -. */
    SDL_FRect buttonShipScalePlusRect; /**< Bouton scale navire +. */
    SDL_FRect buttonShipReexportRect; /**< Bouton reexport navire scale. */
    SDL_FRect buttonListsVisibilityRect; /**< Bouton ON/OFF affichage des 3 listes. */
    SDL_FRect mapNameInputRect; /**< Champ de saisie nom map. */
    SDL_FRect assetListRect; /**< Panneau liste assets (bas droite). */
    SDL_FRect shipListRect; /**< Panneau liste navires (meme format que assets). */
    SDL_FRect miniMapRect; /**< Minimap editeur (haut droite). */
    SDL_FRect towerVariantPickerRect; /**< Popup de selection des variantes de towers. */
    std::array<SDL_FRect, 4> towerVariantLevelTabRects; /**< Onglets lvl1..lvl4 de la popup towers. */
    SDL_FRect towerVariantConfirmRect; /**< Bouton confirmer popup variantes tower. */
    bool towerVariantPickerVisible; /**< true si la popup de variantes towers est ouverte. */
    SDL_FRect towerDisplayPickerRect; /**< Popup de selection du niveau de preview tower. */
    std::array<SDL_FRect, 5> towerDisplayLevelTabRects; /**< Onglets tuile hotspot + niveaux de preview tower. */
    SDL_FRect towerDisplayConfirmRect; /**< Bouton confirmer popup affichage towers. */
    bool towerDisplayPickerVisible; /**< true si la popup de preview tower est ouverte. */
    SDL_FRect mortarVariantPickerRect; /**< Popup de selection des variantes de mortiers. */
    std::array<SDL_FRect, 3> mortarVariantLevelTabRects; /**< Onglets lvl1..lvl3 de la popup mortiers. */
    SDL_FRect mortarVariantConfirmRect; /**< Bouton confirmer popup variantes mortier. */
    bool mortarVariantPickerVisible; /**< true si la popup de variantes mortiers est ouverte. */
    SDL_FRect mortarDisplayPickerRect; /**< Popup de selection du niveau de preview mortier. */
    std::array<SDL_FRect, 4> mortarDisplayLevelTabRects; /**< Onglets tuile hotspot + niveaux de preview mortier. */
    SDL_FRect mortarDisplayConfirmRect; /**< Bouton confirmer popup affichage mortiers. */
    bool mortarDisplayPickerVisible; /**< true si la popup de preview mortier est ouverte. */
    bool miniMapDragActive; /**< true si drag minimap en cours. */
    float miniMapDragOffsetX; /**< Offset drag minimap en X. */
    float miniMapDragOffsetY; /**< Offset drag minimap en Y. */
    std::vector<WorldMapCase> worldMapCases; /**< Cases de la GUI carte du monde a exporter. */
    std::vector<WorldMapLink> worldMapLinks; /**< Liaisons entre cases de la carte du monde. */
    int selectedWorldMapCaseIndex; /**< Index de la case actuellement selectionnee. */
    bool worldMapLinkModeEnabled; /**< true si le mode creation de liaison est actif. */
    int pendingWorldMapLinkSourceIndex; /**< Index source en attente pour la prochaine liaison. */
    WorldMapLinkSide selectedWorldMapLinkSide; /**< Cote de sortie choisi pour la prochaine liaison. */
    bool worldMapEditorVisible; /**< true si la popup d'edition carte du monde est ouverte. */
    WorldMapTextField worldMapFocusedTextField; /**< Champ texte actuellement focus. */
    bool worldMapWindowDragging; /**< true pendant le deplacement de la fenetre. */
    float worldMapWindowDragOffsetX; /**< Offset souris/fenetre pendant le drag. */
    float worldMapWindowDragOffsetY; /**< Offset souris/fenetre pendant le drag. */
    SDL_FPoint worldMapWindowOffset; /**< Offset utilisateur applique a la fenetre carte du monde. */
    bool worldMapCaseDragging; /**< true pendant le drag d'une case. */
    float worldMapCaseDragOffsetX; /**< Offset souris/case en X pendant le drag. */
    float worldMapCaseDragOffsetY; /**< Offset souris/case en Y pendant le drag. */
    bool worldMapCaseSnapGuideVerticalVisible; /**< true si une aide verticale d'alignement est visible. */
    bool worldMapCaseSnapGuideHorizontalVisible; /**< true si une aide horizontale d'alignement est visible. */
    float worldMapCaseSnapGuideVerticalX; /**< Position X locale de l'aide verticale. */
    float worldMapCaseSnapGuideHorizontalY; /**< Position Y locale de l'aide horizontale. */
    bool worldMapCaseSpacingGuideHorizontalVisible; /**< true si une aide d'espacement horizontal egal est visible. */
    bool worldMapCaseSpacingGuideVerticalVisible; /**< true si une aide d'espacement vertical egal est visible. */
    SDL_FRect worldMapCaseSpacingGuideHorizontalRectA; /**< Premier segment horizontal d'aide d'espacement. */
    SDL_FRect worldMapCaseSpacingGuideHorizontalRectB; /**< Second segment horizontal d'aide d'espacement. */
    SDL_FRect worldMapCaseSpacingGuideVerticalRectA; /**< Premier segment vertical d'aide d'espacement. */
    SDL_FRect worldMapCaseSpacingGuideVerticalRectB; /**< Second segment vertical d'aide d'espacement. */
    bool worldMapCaseResizing; /**< true pendant le resize d'une case. */
    SDL_FPoint worldMapCaseResizeStartMouse; /**< Position souris au debut du resize. */
    SDL_FRect worldMapCaseResizeStartRect; /**< Rectangle local de la case au debut du resize. */
    float worldMapCanvasScrollX; /**< Scroll horizontal courant dans le canvas gauche. */
    float worldMapCanvasScrollY; /**< Scroll vertical courant dans le canvas gauche. */
    float worldMapCanvasVirtualWidth; /**< Largeur logique totale du canvas gauche. */
    float worldMapCanvasVirtualHeight; /**< Hauteur logique totale du canvas gauche. */
    SDL_FRect worldMapEditorRect; /**< Fenetre globale carte du monde. */
    SDL_FRect worldMapEditorHeaderRect; /**< Header drag de la fenetre carte du monde. */
    SDL_FRect worldMapEditorCanvasRect; /**< Zone de placement des cases. */
    SDL_FRect worldMapAddCaseRect; /**< Bouton ajout case. */
    SDL_FRect worldMapDeleteCaseRect; /**< Bouton suppression case. */
    SDL_FRect worldMapToggleGuildRect; /**< Bouton toggle texte guilde. */
    SDL_FRect worldMapCaseTypeRect; /**< Bouton choix type ville/map normale. */
    SDL_FRect worldMapLinkModeRect; /**< Bouton activation du mode liaison. */
    SDL_FRect worldMapLinkSideRect; /**< Bouton choix du cote source de liaison. */
    SDL_FRect worldMapPreviewLockRect; /**< Bouton verrouillage edition pour aperçu final. */
    SDL_FRect worldMapImportRect; /**< Bouton import JSON carte du monde. */
    SDL_FRect worldMapExportRect; /**< Bouton export JSON carte du monde. */
    SDL_FRect worldMapResetRect; /**< Bouton reset complet carte du monde. */
    SDL_FRect worldMapCloseRect; /**< Bouton fermeture popup carte du monde. */
    SDL_FRect worldMapMapNameInputRect; /**< Input texte principal de la case selectionnee. */
    SDL_FRect worldMapPreviewWidthInputRect; /**< Input largeur GUI a droite. */
    SDL_FRect worldMapPreviewHeightInputRect; /**< Input hauteur GUI a droite. */
    SDL_FRect worldMapAutoLayoutInputRect; /**< Input du modele de lignes auto. */
    SDL_FRect worldMapAutoGenerateRect; /**< Bouton de generation auto. */
    SDL_FRect worldMapCaseResizeHandleRect; /**< Handle de resize de la case selectionnee. */
    SDL_FRect worldMapPreviewWindowRect; /**< Fenetre d'aperçu gameplay a droite. */
    SDL_FRect worldMapPreviewHeaderRect; /**< Header de l'aperçu gameplay. */
    SDL_FRect worldMapPreviewViewportRect; /**< Zone de rendu des cases dans l'aperçu. */
    SDL_FRect worldMapPreviewResizeCornerRect; /**< Handle coin bas-droite pour resize largeur+hauteur. */
    SDL_FRect worldMapPreviewResizeHandleRect; /**< Handle pour redimensionner la hauteur de l'aperçu. */
    bool worldMapPreviewHeightResizing; /**< true pendant le resize vertical de l'aperçu. */
    bool worldMapPreviewCornerResizing; /**< true pendant le resize largeur+hauteur de l'aperçu. */
    float worldMapPreviewPreferredWidth; /**< Largeur cible retenue pour la fenetre d'aperçu. */
    float worldMapPreviewPreferredHeight; /**< Hauteur cible retenue pour la fenetre d'aperçu. */
    float worldMapPreviewResizeStartMouseX; /**< Souris X au debut du resize preview. */
    float worldMapPreviewResizeStartMouseY; /**< Souris Y au debut du resize preview. */
    float worldMapPreviewResizeStartWidth; /**< Largeur preview au debut du resize. */
    float worldMapPreviewResizeStartHeight; /**< Hauteur preview au debut du resize. */
    std::string worldMapPreviewWidthInput; /**< Buffer texte pour largeur GUI. */
    std::string worldMapPreviewHeightInput; /**< Buffer texte pour hauteur GUI. */
    std::string worldMapAutoLayoutInput; /**< Buffer texte du schema auto, ex: 5-3-2. */
    bool worldMapPreviewInteractionLocked; /**< true si l'aperçu final verrouille edition/resize. */
    bool editorTextInputEnabled; /**< Etat courant du SDL_StartTextInput/StopTextInput pour cette scene. */

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
    /** @brief Convertit la souris courante en tuile flottante + tuile arrondie. */
    bool tryGetMouseTileExact(SDL_FPoint* outTileFloat, SDL_Point* outNearestTile) const;
    /** @brief Convertit la souris courante en tuile map.
     *  @param outTile Tuile resultante.
     *  @return true si conversion valide et dans la map.
     */
    bool tryGetMouseTile(SDL_Point* outTile) const;
    /** @brief Retourne l'index de niveau associe a l'etape courante, sinon -1 pour le hotspot de tir. */
    int getTowerPlacementStageLevelIndex(void) const;
    /** @brief Retourne le libelle court de l'etape courante de pose tower. */
    const char* getTowerPlacementStageLabel(void) const;
    /** @brief Reinitialise le workflow de pose tower sur le niveau 1. */
    void resetTowerPlacementStage(void);
    /** @brief Passe a l'etape suivante du workflow de pose tower. */
    void advanceTowerPlacementStage(void);
    /** @brief Retourne l'index de niveau associe a l'etape courante du mortier, sinon -1 pour le hotspot de tir. */
    int getMortarPlacementStageLevelIndex(void) const;
    /** @brief Retourne le libelle court de l'etape courante de pose mortier. */
    const char* getMortarPlacementStageLabel(void) const;
    /** @brief Reinitialise le workflow de pose mortier sur le niveau 1. */
    void resetMortarPlacementStage(void);
    /** @brief Passe a l'etape suivante du workflow de pose mortier. */
    void advanceMortarPlacementStage(void);
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
    /** @brief Indique si un index d'asset pose est valide. */
    bool isPlacedAssetIndexValid(int index) const;
    /** @brief Deselectionne l'asset pose actuellement edite. */
    void clearSelectedPlacedAsset(void);
    /** @brief Selectionne l'asset pose sous un point ecran. */
    bool selectPlacedAssetAtScreenPoint(float x, float y);
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
    /** @brief Liste les tuiles autorisees pour l'interaction d'un asset. */
    std::vector<SDL_Point> computePlacedAssetClickInteractionTiles(const PlacedAsset& asset) const;
    /** @brief Indique si une tuile precise est deja autorisee pour l'asset. */
    bool placedAssetHasClickInteractionTile(const PlacedAsset& asset, int tileX, int tileY) const;
    /** @brief Ajoute ou retire une tuile d'interaction sur un asset pose. */
    bool setPlacedAssetClickInteractionTile(int placedAssetIndex, int tileX, int tileY, bool enabled);
    /** @brief Peint l'interaction de l'asset selectionne sous la souris. */
    void paintSelectedPlacedAssetInteractionAtMouse(bool enabled);
    /** @brief Trouve un hotspot a la tuile demandee. */
    int findTowerHotspotIndexAtTile(int tileX, int tileY) const;
    /** @brief Trouve un hotspot par numero de tower. */
    int findTowerHotspotIndexByNumber(int towerNumber) const;
    /** @brief Trouve une config visuelle de tower par numero. */
    int findTowerVisualSetIndexByNumber(int towerNumber) const;
    /** @brief Retourne la config visuelle de tower creee au besoin. */
    TowerVisualSet& ensureTowerVisualSet(int towerNumber);
    /** @brief Retourne le premier numero de tower libre, sinon 0. */
    int findFirstFreeTowerHotspotNumber(void) const;
    /** @brief Toggle un hotspot tour. */
    void toggleTowerHotspotAtTile(int tileX, int tileY);
    /** @brief Force le hotspot de tir d'une tower sur une ancre pixel perfect. */
    void setTowerHotspotAnchor(int towerNumber, float anchorTileX, float anchorTileY);
    /** @brief Place l'etape courante du workflow tower sous un point ecran. */
    void placeTowerWorkflowAtScreenPoint(float renderX, float renderY);
    /** @brief Trouve un hotspot mortier a la tuile demandee. */
    int findMortarHotspotIndexAtTile(int tileX, int tileY) const;
    /** @brief Trouve un hotspot mortier par numero. */
    int findMortarHotspotIndexByNumber(int mortarNumber) const;
    /** @brief Trouve une config visuelle de mortier par numero. */
    int findMortarVisualSetIndexByNumber(int mortarNumber) const;
    /** @brief Retourne la config visuelle de mortier creee au besoin. */
    MortarVisualSet& ensureMortarVisualSet(int mortarNumber);
    /** @brief Force le hotspot de tir d'un mortier sur une ancre pixel perfect. */
    void setMortarHotspotAnchor(int mortarNumber, float anchorTileX, float anchorTileY);
    /** @brief Place l'etape courante du workflow mortier sous un point ecran. */
    void placeMortarWorkflowAtScreenPoint(float renderX, float renderY);
    /** @brief Ajuste l'opacite globale assets en %. */
    void setAssetOpacityPercent(int value);
    /** @brief Ajuste l'echelle navire test en %. */
    void setShipScalePercent(int value);
    /** @brief Reexporte les 8 sprites navire avec scale. */
    bool reexportLoadedShipScaled(int scalePercent);
    /** @brief Gère la saisie clavier nom map.
     *  @return true si la touche est consommee.
     */
    bool handleMapNameInputKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
    /** @brief Gere la saisie clavier de la popup carte du monde.
     *  @return true si la touche est consommee.
     */
    bool handleWorldMapEditorKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
    /** @brief Traite le texte UTF-8 de la popup carte du monde. */
    void handleWorldMapEditorTextInput(const char* text);
    /** @brief Retourne true si une case carte du monde est selectionnee. */
    bool isWorldMapCaseSelectionValid(void) const;
    /** @brief Retourne le rect local de la case selectionnee. */
    SDL_FRect getWorldMapCaseLocalRect(int index) const;
    /** @brief Retourne le rect rendu de la case selectionnee. */
    SDL_FRect getWorldMapCaseScreenRect(int index) const;
    /** @brief Valide/applique les dimensions saisies pour la preview GUI. */
    void commitWorldMapPreviewDimensionInputs(void);
    /** @brief Retourne l'index de case sous un point rendu. */
    int findWorldMapCaseIndexAtPoint(float x, float y) const;
    /** @brief Mesure un texte avec la police overlay courante. */
    bool measureWorldMapTextSize(const std::string& text, int* outWidth, int* outHeight) const;
    /** @brief Parse le schema auto et retourne les tailles de lignes. */
    bool tryParseWorldMapAutoLayout(std::vector<int>* outRowCounts) const;
    /** @brief Genere automatiquement cases + liaisons depuis le schema auto. */
    void generateWorldMapAutoLayout(void);
    /** @brief Retourne la taille minimale utile d'une case selon son contenu texte. */
    SDL_FPoint getWorldMapCaseMinimumSize(const std::string& mapName, bool showGuildName) const;
    /** @brief Retourne le scroll horizontal max possible du canvas gauche. */
    float getWorldMapCanvasMaxScrollX(void) const;
    /** @brief Retourne le scroll vertical max possible du canvas gauche. */
    float getWorldMapCanvasMaxScrollY(void) const;
    /** @brief Recalcule la largeur utile de la carte du monde selon le contenu. */
    float getWorldMapCanvasUsedWidth(void) const;
    /** @brief Recalcule la hauteur utile de la carte du monde selon le contenu. */
    float getWorldMapCanvasUsedHeight(void) const;
    /** @brief Aligne le scroll horizontal dans les bornes valides. */
    void clampWorldMapCanvasScrollX(void);
    /** @brief Aligne le scroll vertical dans les bornes valides. */
    void clampWorldMapCanvasScroll(void);
    /** @brief Fait defiler la vue pour garder une case visible. */
    void ensureWorldMapCaseVisible(int index);
    /** @brief Aligne une case sur les limites du canvas. */
    void clampWorldMapCaseToCanvas(WorldMapCase& worldMapCase);
    /** @brief Ajoute une nouvelle case carte du monde. */
    void addWorldMapCase(void);
    /** @brief Supprime la case selectionnee. */
    void deleteSelectedWorldMapCase(void);
    /** @brief Ouvre ou ferme la popup createur carte du monde. */
    void toggleWorldMapEditor(void);
    /** @brief Recalcule les rects UI de la popup carte du monde. */
    void updateWorldMapEditorLayout(void);
    /** @brief Gere le drag/resize continus de la popup carte du monde. */
    void updateWorldMapEditorInteractions(void);
    /** @brief Dessine la popup carte du monde. */
    void drawWorldMapEditor(void) const;
    /** @brief Gere un clic dans la popup carte du monde.
     *  @return true si le clic est consomme.
     */
    bool handleWorldMapEditorClick(float x, float y, RC2D_MouseButton button);
    /** @brief Retourne le buffer texte actuellement focus dans la popup. */
    std::string* getWorldMapFocusedTextBuffer(void);
    /** @brief Synchronise l'activation SDL du text input selon les focus actifs. */
    void syncEditorTextInputState(void);
    /** @brief Ouvre le dialogue d'export JSON carte du monde. */
    void openImportWorldMapDialog(void);
    /** @brief Traite l'import world map publie par callback async. */
    void processPendingWorldMapImportRequest(void);
    /** @brief Importe les cases de la carte du monde depuis un JSON. */
    bool importWorldMapFromAbsolutePath(const char* absolutePath);
    /** @brief Ouvre le dialogue d'export JSON carte du monde. */
    void openExportWorldMapDialog(void);
    /** @brief Traite l'export world map publie par callback async. */
    void processPendingWorldMapExportRequest(void);
    /** @brief Exporte les cases de la carte du monde dans un JSON. */
    bool exportWorldMapToAbsolutePath(const char* absolutePath);
    /** @brief Traite l'import dossier navire publie par callback async. */
    void processPendingShipFolderRequest(void);
    /** @brief Charge un navire test depuis un dossier absolu.
     *  @param folderAbsolutePath Chemin absolu vers dossier 1..8.png.
     *  @return true si navire charge.
     */
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    /** @brief Selectionne un navire importe et le charge en navire test. */
    bool selectImportedShipAtIndex(int shipIndex);
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
    /** @brief Dessine les hotspots tours et les previews de towers. */
    void drawTowerHotspotsAndPreviews(void) const;
    /** @brief Dessine les hotspots mortiers et les previews de mortiers. */
    void drawMortarHotspotsAndPreviews(void) const;
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
    /** @brief Gere un clic dans la popup de variantes de towers. */
    bool handleTowerVariantPickerClick(float x, float y);
    /** @brief Gere un clic dans la popup d'affichage tower global. */
    bool handleTowerDisplayPickerClick(float x, float y);
    /** @brief Gere un clic dans la popup de variantes de mortiers. */
    bool handleMortarVariantPickerClick(float x, float y);
    /** @brief Gere un clic dans la popup d'affichage mortier global. */
    bool handleMortarDisplayPickerClick(float x, float y);
    /** @brief Gere un clic dans le panneau liste navires.
     *  @return true si le clic est consomme.
     */
    bool handleShipListClick(float x, float y);
    /** @brief Gere le drag continu de la scrollbar liste assets. */
    void handleAssetListScrollDragFromMouse(void);
    /** @brief Gere le drag continu de la scrollbar liste navires. */
    void handleShipListScrollDragFromMouse(void);
    /** @brief Gere le drag continu des tuiles d'interaction d'un asset pose selectionne. */
    void handleSelectedPlacedAssetInteractionPaintFromMouse(void);
    /** @brief Dessine le panneau liste assets. */
    void drawAssetListPanel(void) const;
    /** @brief Dessine la popup de variantes de towers. */
    void drawTowerVariantPickerPopup(void) const;
    /** @brief Dessine la popup d'affichage tower global. */
    void drawTowerDisplayPickerPopup(void) const;
    /** @brief Dessine la popup de variantes de mortiers. */
    void drawMortarVariantPickerPopup(void) const;
    /** @brief Dessine la popup d'affichage mortier global. */
    void drawMortarDisplayPickerPopup(void) const;
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
    /** @brief Retourne true si le point survole une zone UI qui doit bloquer le paint map. */
    bool isPointOverBlockingEditorUi(float x, float y) const;
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
    /** @brief Callback async de resultat import world map. */
    static void onImportWorldMapDialogResult(void* userdata, const char* const* filelist, int filter_index);
    /** @brief Callback async de resultat export world map. */
    static void onExportWorldMapDialogResult(void* userdata, const char* const* filelist, int filter_index);

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
    void textinput(const RC2D_TextInputEventInfo* info) override;
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
