#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

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
    };

    struct ImportedSfx {
        std::string id;
        std::string displayName;
        std::string sourceJsonPath;
        std::string storageJsonPath;
        RC2D_TP_Atlas atlas;
        std::vector<std::string> frameNames;
        float defaultFps;
    };

    struct ShipVfxInstance {
        uint32_t instanceId;
        int importedSfxIndex;
        float offsetX;
        float offsetY;
        float rotationDeg;
        bool flipHorizontal;
        bool flipVertical;
        int drawOrder;
        float fps;
        bool followShip;
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
    std::string vfxFpsInput;
    bool vfxFpsInputFocused;

    int looseScalePercent;
    float loosePreviewZoomFactor;
    LoosePreviewMode loosePreviewMode;
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
    SDL_FRect buttonImportLooseRect;
    SDL_FRect buttonExportRect;
    SDL_FRect buttonOceanPrevRect;
    SDL_FRect buttonOceanNextRect;

    SDL_FRect buttonShipOrderMinusRect;
    SDL_FRect buttonShipOrderPlusRect;
    SDL_FRect buttonVfxOrderMinusRect;
    SDL_FRect buttonVfxOrderPlusRect;
    SDL_FRect buttonRotateMinusRect;
    SDL_FRect buttonRotatePlusRect;
    SDL_FRect buttonFlipHorizontalRect;
    SDL_FRect buttonFlipVerticalRect;
    SDL_FRect buttonFollowShipRect;
    SDL_FRect buttonRemoveVfxRect;
    SDL_FRect buttonCenterVfxRect;
    SDL_FRect buttonVfxFpsInputRect;
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

    SDL_FRect shipListRect;
    SDL_FRect sfxListRect;
    SDL_FRect looseListRect;

    static EditorMapVfxScene* activeInstance;

    void resetEditorState(void);
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

    void spawnSelectedSfxAtShipCenter(void);
    void setSelectedVfxInstanceIndex(int index);
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
    bool applyVfxFpsInput(void);
    bool applyLoosePreviewFpsInput(void);
    float getLoosePreviewFpsOrDefault(void) const;
    bool handleVfxFpsInputKey(
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

    bool handleShipListClick(float x, float y);
    bool handleSfxListClick(float x, float y);
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
};

#endif // GAME_ENV_DEV
