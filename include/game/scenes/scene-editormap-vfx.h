#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/overlay/scroll-bar-overlay.h"

/**
 * @class EditorMapVfxScene
 * @brief Editeur dedie a l'assemblage navire + VFX et au resize de sprites VFX.
 *
 * Deux modes principaux:
 * - SHIP_VFX: navire centre, deux listes (navires/VFX atlas), placement et edition
 *   de plusieurs instances d'animations avec export JSON.
 * - LOOSE_SPRITES: import de dossiers de sprites PNG sans spritesheet, preview
 *   de tout le dossier au centre, resize par pas de 5% et export des fichiers.
 */
class EditorMapVfxScene : public Scene {
private:
    enum class EditorMode {
        SHIP_VFX = 0,
        LOOSE_SPRITES = 1
    };

    enum class LoosePreviewMode {
        CENTER_SPRITESHEET = 0,
        PLACEMENT_PREVIEW = 1
    };

    struct ImportedShip {
        std::string displayName;
        std::string folderAbsolutePath;
        std::string configJsonPath;
    };

    struct ImportedSfxFrame {
        int index;
        std::string frameName;
        float x;
        float y;
        float w;
        float h;
    };

    struct ImportedSfx {
        std::string id;
        std::string displayName;
        std::string sourceJsonPath;
        std::string sourceImagePath;
        std::string sourceFolderAbsolutePath;
        std::string storageImagePath;
        RC2D_Image image;
        std::vector<ImportedSfxFrame> frames;
        float defaultFps;
    };

    struct DirectionOverride {
        bool enabled;
        float offsetX;
        float offsetY;
        float rotationDeg;
        bool flipHorizontal;
        bool flipVertical;
        int drawOrder;
        bool visible;
    };

    struct ShipVfxInstance {
        uint32_t instanceId;
        std::string label;
        std::string sourceJsonPath;
        std::string sourceDisplayName;
        int importedSfxIndex;
        float offsetX;
        float offsetY;
        float rotationDeg;
        bool flipHorizontal;
        bool flipVertical;
        int drawOrder;
        bool visible;
        bool debugBoundsVisible;
        bool locked;
        bool behindShip;
        bool followShip;
        bool sharedForAllDirections;
        bool sharedForAllStates;
        std::array<DirectionOverride, 4> directionOverrides;
    };

    struct InvalidAssetEntry {
        std::string folderName;
        std::string reason;
    };

    struct ImportedLooseSprite {
        std::string fileName;
        std::string storagePath;
        RC2D_Image image;
        float widthPx;
        float heightPx;
    };

    struct ImportedLooseFolder {
        std::string displayName;
        std::string folderAbsolutePath;
        std::string storageFolderPath;
        std::vector<ImportedLooseSprite> sprites;
        /** Union des pixels opaques (meme logique que l'export), pour preview mode spritesheet. */
        int looseUnionCropX = 0;
        int looseUnionCropY = 0;
        int looseUnionCropW = 0;
        int looseUnionCropH = 0;
        bool looseUnionCropReady = false;
    };

    struct LoosePreviewPlacement {
        uint32_t instanceId;
        float tileX;
        float tileY;
        float fps;
    };

    RC2D_Image backgroundUiImage;
    RC2D_Font overlayFont;
    ScrollBarOverlay scrollBarOverlay;

    EditorMode editorMode;
    int selectedOceanColorIndex;
    int pendingOceanColorDelta;

    std::vector<ImportedShip> importedShips;
    std::vector<ImportedSfx> importedSfx;
    std::vector<ImportedLooseFolder> importedLooseFolders;

    int selectedShipIndex;
    int selectedSfxIndex;
    int selectedLooseFolderIndex;
    int shipListScrollOffset;
    int sfxListScrollOffset;
    int looseListScrollOffset;
    bool shipListScrollDragActive;
    bool sfxListScrollDragActive;
    bool looseListScrollDragActive;
    float shipListScrollDragGrabOffsetY;
    float sfxListScrollDragGrabOffsetY;
    float looseListScrollDragGrabOffsetY;

    Ship previewShip;
    bool previewShipLoaded;
    std::string loadedShipFolderAbsolute;
    SDL_FPoint previewShipTile;
    int previewDirectionIndex;
    int previewShipStateIndex;
    bool shipLayerVisible;
    bool shipLayerLocked;
    bool shipLayerSelected;
    bool shipDebugBoundsVisible;
    int shipDrawOrder;
    RC2D_Image looseReferenceGuildIslandImage;
    RC2D_Image looseReferenceTowerLevel1Image;
    RC2D_Image looseReferenceTowerLevel2Image;
    RC2D_Image looseReferenceTowerLevel3Image;
    RC2D_Image looseReferenceTowerLevel4Image;
    RC2D_Image looseReferenceShipLeftImage;
    RC2D_Image looseReferenceShipRightImage;
    bool looseReferencePreviewVisible;
    bool looseReferencePreviewLoaded;

    std::vector<ShipVfxInstance> shipVfxInstances;
    int selectedVfxInstanceIndex;
    uint32_t nextVfxInstanceId;
    std::string layerNameInput;
    bool layerNameInputFocused;
    bool vfxDragActive;
    float vfxDragStartMouseX;
    float vfxDragStartMouseY;
    float vfxDragStartOffsetX;
    float vfxDragStartOffsetY;
    bool shipVfxDirty;
    std::string loadedShipVfxConfigPath;

    int looseScalePercent;
    float loosePreviewZoomFactor;
    LoosePreviewMode loosePreviewMode;
    /** Si true, le clic preview ancre sur le centre de la tuile la plus proche ; sinon tuile flottante (sous-pixel). */
    bool loosePreviewPlacementSnapToTile;
    std::vector<LoosePreviewPlacement> loosePreviewPlacements;
    uint32_t nextLoosePreviewPlacementId;
    std::string loosePreviewFpsInput;
    bool loosePreviewFpsInputFocused;
    bool looseExportNamePopupVisible;
    std::string looseExportNameInput;
    std::string pendingLooseExportAnimationName;
    unsigned int importedSfxCounter;
    unsigned int importedLooseFolderCounter;

    std::string statusMessage;
    std::vector<InvalidAssetEntry> invalidShipFolders;
    std::vector<InvalidAssetEntry> invalidVfxFolders;

    bool pendingShipFolderDialogCompleted;
    bool pendingShipFolderDialogCanceled;
    std::string pendingShipFolderAbsolute;
    mutable std::mutex pendingShipFolderMutex;

    bool pendingSfxFolderDialogCompleted;
    bool pendingSfxFolderDialogCanceled;
    std::string pendingSfxFolderAbsolute;
    mutable std::mutex pendingSfxFolderMutex;

    bool pendingLooseFolderDialogCompleted;
    bool pendingLooseFolderDialogCanceled;
    std::string pendingLooseFolderAbsolute;
    mutable std::mutex pendingLooseFolderMutex;

    bool pendingExportFolderDialogCompleted;
    bool pendingExportFolderDialogCanceled;
    std::string pendingExportFolderAbsolute;
    EditorMode pendingExportMode;
    mutable std::mutex pendingExportFolderMutex;

    SDL_FRect buttonModeShipVfxRect;
    SDL_FRect buttonModeLooseSpritesRect;
    SDL_FRect buttonImportShipRect;
    SDL_FRect buttonImportSfxRect;
    SDL_FRect buttonReloadAssetsRect;
    SDL_FRect buttonImportLooseRect;
    SDL_FRect buttonExportRect;
    SDL_FRect buttonOceanPrevRect;
    SDL_FRect buttonOceanNextRect;

    SDL_FRect buttonDirectionPrevRect;
    SDL_FRect buttonDirectionNextRect;
    SDL_FRect buttonShipStateToggleRect;
    SDL_FRect buttonShipOrderMinusRect;
    SDL_FRect buttonShipOrderPlusRect;
    SDL_FRect buttonLayerOrderMinusRect;
    SDL_FRect buttonLayerOrderPlusRect;
    SDL_FRect buttonVfxOrderMinusRect;
    SDL_FRect buttonVfxOrderPlusRect;
    SDL_FRect buttonRotateMinusRect;
    SDL_FRect buttonRotatePlusRect;
    SDL_FRect buttonFlipHorizontalRect;
    SDL_FRect buttonFlipVerticalRect;
    SDL_FRect buttonVisibleRect;
    SDL_FRect buttonLockedRect;
    SDL_FRect buttonBehindShipRect;
    SDL_FRect buttonFollowShipRect;
    SDL_FRect buttonRemoveVfxRect;
    SDL_FRect buttonDuplicateVfxRect;
    SDL_FRect buttonCenterVfxRect;
    SDL_FRect buttonLayerNameInputRect;
    SDL_FRect buttonSharedDirectionsRect;
    SDL_FRect buttonDirectionOverrideRect;
    SDL_FRect buttonResetTransformRect;
    SDL_FRect buttonMoveULRect;
    SDL_FRect buttonMoveUpRect;
    SDL_FRect buttonMoveURRect;
    SDL_FRect buttonMoveLeftRect;
    SDL_FRect buttonMoveRightRect;
    SDL_FRect buttonMoveDLRect;
    SDL_FRect buttonMoveDownRect;
    SDL_FRect buttonMoveDRRect;

    SDL_FRect buttonLooseScaleMinusRect;
    SDL_FRect buttonLooseScalePlusRect;
    SDL_FRect buttonLooseReferencePreviewRect;
    SDL_FRect buttonLooseZoomMinusRect;
    SDL_FRect buttonLooseZoomPlusRect;
    SDL_FRect buttonLoosePreviewModeRect;
    SDL_FRect buttonLoosePreviewFpsInputRect;
    SDL_FRect buttonLooseClearAllVfxRect;
    SDL_FRect buttonLoosePreviewPlacementSnapRect;

    SDL_FRect shipListRect;
    SDL_FRect sfxListRect;
    SDL_FRect layerListRect;
    SDL_FRect looseListRect;
    SDL_FRect invalidVfxListRect;
    SDL_FRect invalidShipListRect;
    int layerListScrollOffset;
    bool layerListScrollDragActive;
    float layerListScrollDragGrabOffsetY;
    bool layerRowDragActive;
    bool layerRowDragMoved;
    int layerRowDragSourceDisplayIndex;
    int layerRowDragTargetInsertIndex;
    float layerRowDragStartMouseY;
    int invalidVfxListScrollOffset;
    int invalidShipListScrollOffset;
    bool invalidVfxListScrollDragActive;
    bool invalidShipListScrollDragActive;
    float invalidVfxListScrollDragGrabOffsetY;
    float invalidShipListScrollDragGrabOffsetY;

    static EditorMapVfxScene* activeInstance;

    void resetEditorState(void);
    /** Remet interactions (drag, focus) sans toucher aux imports ni au mode courant. */
    void clearEditorTransientInteractionState(void);
    /** Vue caméra + tuile navire comme au chargement (mode Ship / VFX). */
    void applyShipVfxModeViewportReset(void);
    /** Recharge le navire preview si decharge, sans vider les calques VFX. */
    void reloadPreviewShipIfUnloadedKeepVfxLayers(void);
    /** Etat loose (sous-modes, placements, zoom) remis a zero ; camera centree comme au chargement. */
    void applyLooseSpritesModeEntryReset(void);
    void ensureUserStorageFolders(void);
    void unloadImportedSfx(void);
    void unloadImportedLooseFolders(void);
    void loadLooseReferencePreviewAssets(void);
    void unloadLooseReferencePreviewAssets(void);
    void adjustLoosePreviewZoom(float delta);
    void applySelectedOceanColor(void);
    void requestOceanColorStep(int delta);
    void applyPendingOceanColorStep(void);
    void cycleOceanColor(int delta);
    void autoImportAssetsFromDefaultFolders(void);
    std::string buildShipConfigJsonPath(const ImportedShip& ship) const;
    void applyPreviewDirectionToShip(void);
    void setPreviewDirectionIndex(int directionIndex);
    void cyclePreviewDirection(int delta);
    void cyclePreviewShipState(int delta);
    const char* getPreviewDirectionLabel(void) const;
    const char* getPreviewShipStateLabel(void) const;
    void markShipVfxDirty(void);
    int getActiveDirectionIndexForOverrides(void) const;
    bool hasSelectedVfxInstance(void) const;
    ShipVfxInstance* getSelectedVfxInstance(void);
    const ShipVfxInstance* getSelectedVfxInstance(void) const;
    DirectionOverride* getEditableDirectionOverride(ShipVfxInstance* instance);
    const DirectionOverride* getResolvedDirectionOverride(const ShipVfxInstance* instance) const;
    void resetSelectedVfxTransform(void);
    void toggleSelectedVfxVisibility(void);
    void toggleSelectedVfxLock(void);
    void toggleSelectedVfxBehindShip(void);
    void toggleSelectedVfxSharedForAllDirections(void);
    void toggleSelectedVfxDirectionOverride(void);
    void duplicateSelectedVfxInstance(void);
    void moveSelectedLayerOrder(int delta);
    bool applyLayerNameInput(void);
    bool importShipVfxConfigFromPath(const char* absoluteFilePath);
    bool loadShipVfxConfigForSelectedShip(void);

    void updateToolbarLayout(void);
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    void convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const;
    bool getMouseRenderPosition(float* outX, float* outY) const;

    int getListMaxScrollOffset(int itemCount) const;
    void clampListScrollOffset(int* scrollOffset, int itemCount) const;
    void ensureSelectionVisible(int selectedIndex, int* scrollOffset, int itemCount) const;
    int computeListStartIndex(int scrollOffset, int itemCount) const;
    bool handleListPanelClick(
        float x,
        float y,
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY,
        int* outClickedIndex);
    void handleListPanelScrollDragFromMouse(
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);
    void drawListPanel(
        const SDL_FRect& panelRect,
        const char* title,
        const std::vector<std::string>& labels,
        int selectedIndex,
        int scrollOffset) const;
    void drawLayerListPanel(const std::vector<int>& orderedLayerIndices) const;
    void updateLayerListRowDragFromMouse(void);
    void applyLayerPanelReorder(int sourceDisplayIndex, int targetDisplayIndex);
    void drawInvalidAssetPanel(
        const SDL_FRect& panelRect,
        const char* title,
        const std::vector<InvalidAssetEntry>& entries,
        int scrollOffset) const;
    bool handleInvalidAssetPanelClick(
        float x,
        float y,
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);
    void handleInvalidAssetPanelScrollDragFromMouse(
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);

    void openImportShipFolderDialog(void);
    void openImportSfxFolderDialog(void);
    void openImportLooseFolderDialog(void);
    void openExportFolderDialog(void);
    void openLooseExportNamePopup(void);
    void processPendingShipFolderRequest(void);
    void processPendingSfxFolderRequest(void);
    void processPendingLooseFolderRequest(void);
    void processPendingExportFolderRequest(void);

    bool importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    bool selectImportedShipAtIndex(int shipIndex);

    bool importSfxFromAbsolutePath(const char* absolutePath);
    bool importSfxFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);

    bool importLooseFolderFromAbsolutePath(const char* absolutePath);
    bool importLooseFoldersFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);
    void refreshLooseFolderUnionCrop(ImportedLooseFolder& folder);

    void spawnSelectedSfxAtShipCenter(void);
    void setSelectedVfxInstanceIndex(int index);
    void rebuildVfxLayerLabelsFromCurrentInstances(void);
    void removeSelectedVfxInstance(void);
    void centerSelectedVfxInstance(void);
    void moveSelectedVfxInstance(float deltaX, float deltaY);
    void adjustSelectedVfxRotation(float deltaDegrees);
    void toggleSelectedVfxFlipHorizontal(void);
    void toggleSelectedVfxFlipVertical(void);
    void toggleSelectedVfxFollowShip(void);
    void adjustSelectedVfxDrawOrder(int delta);
    void adjustShipDrawOrder(int delta);
    void normalizeShipVfxDrawOrders(void);
    int findTopmostVfxInstanceIndexAtPoint(float x, float y) const;
    bool applyLoosePreviewFpsInput(void);
    float getLoosePreviewFpsOrDefault(void) const;
    bool handleLayerNameInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    bool handleLoosePreviewFpsInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    bool handleLooseExportNameInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);

    bool exportShipVfxJsonToFolder(const char* absoluteFolderPath);
    bool exportLooseFolderScaledToFolder(const char* absoluteFolderPath, const std::string& animationName);

    void drawShipVfxPreview(void) const;
    void drawLooseReferencePreview(void) const;
    void drawLooseSpritesPreview(void) const;
    void drawLoosePlacementPreview(void) const;
    void drawLooseExportNamePopup(void) const;
    void drawHud(void) const;
    int getSelectedLayerRowIndexForDisplay(const std::vector<int>& orderedInstanceIndices) const;
    std::vector<int> getOrderedVfxInstanceIndicesForLayerPanel(void) const;

    bool handleShipListClick(float x, float y);
    bool handleSfxListClick(float x, float y);
    bool handleLayerListClick(float x, float y);
    bool handleLooseListClick(float x, float y);
    bool handleToolbarClick(float x, float y);
    bool handlePreviewClick(float x, float y, RC2D_MouseButton button);
    bool handleLoosePlacementPreviewClick(float x, float y, RC2D_MouseButton button);

    static void onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onImportSfxFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onImportLooseFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onExportFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    EditorMapVfxScene(void);
    ~EditorMapVfxScene(void) override;

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
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
