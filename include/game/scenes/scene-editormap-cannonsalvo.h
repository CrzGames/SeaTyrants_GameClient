#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/combat/maritime-cannon-salvo.h"
#include "game/ships/ship.h"
#include "game/state.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/overlay/scroll-bar-overlay.h"
#include "game/ui/overlay/tile-click-marker-overlay.h"
#include "game/vfx/vfx-classic.h"

class IlluminatedProjectileDebugPanel {
public:
    enum class DialogRequest {
        NONE = 0,
        GLOW_IMPORT_JSON,
        GLOW_EXPORT_JSON,
        TRAIL_IMPORT_JSON,
        TRAIL_EXPORT_JSON
    };

    IlluminatedProjectileDebugPanel(void);
    ~IlluminatedProjectileDebugPanel(void);

    void load(void);
    void unload(void);
    void update(double dt);
    void draw(void) const;

    bool mousepressed(float x, float y, RC2D_MouseButton button);
    bool mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID);

    void toggleVisibility(void);
    void setVisible(bool visible);
    bool isVisible(void) const;
    void setTrailPreviewSelection(
        const char* projectileFolderPath,
        const char* trailFolderPath,
        bool projectileIlluminated,
        bool trailEnabled);
    DialogRequest consumeDialogRequest(void);

private:
    bool loaded;
    bool visible;
    bool panelDragging;
    bool sliderDragging;
    int activeSliderIndex;
    int activePageIndex;
    int selectedDistanceBandIndex;
    int selectedAngleSectorIndex;
    unsigned int trajectoryDebugMask;
    bool trajectoryCircleVisible;
    bool trailScrollDragging;
    float sliderDragGrabOffsetX;
    float panelDragGrabOffsetX;
    float panelDragGrabOffsetY;
    float trailScrollOffsetY;
    float trailScrollDragGrabOffsetY;
    float statusMessageTimerSec;
    SDL_FPoint panelOffset;
    std::string statusMessage;
    RC2D_Font titleFont;
    RC2D_Font bodyFont;
    VFXClassic previewProjectileVfx;
    VFXClassic previewTrailVfx;
    std::string previewProjectileFolderPath;
    std::string previewTrailFolderPath;
    std::string loadedPreviewProjectileFolderPath;
    std::string loadedPreviewTrailFolderPath;
    bool previewProjectileIlluminated;
    bool previewTrailEnabled;
    bool previewAssetsDirty;
    float previewAnimTimerSec;
    int selectedManualTrailStampIndex;
    bool manualTrailStampDragActive;
    DialogRequest pendingDialogRequest;
};

/**
 * @class EditorMapCannonSalvoScene
 * @brief Scene outil pour tester les boulets, salves et VFX ship maritimes.
 */
class EditorMapCannonSalvoScene : public Scene {
public:
    enum class ProjectileGlowMode {
        LIVE = 0,
        DEFAULT_JSON
    };

    struct ListItem {
        std::string displayName{};
        std::string folderPath{};
        bool selected = false;
        std::uint32_t delayAfterImpactMs = 0U;
        bool illuminatedEnabled = false;
        ProjectileGlowMode projectileGlowMode = ProjectileGlowMode::LIVE;
        std::string illuminatedGlowConfigPath{};
        bool illuminatedGlowConfigInitialized = false;
        VFXClassic::IlluminatedProjectileGlowConfig illuminatedGlowConfig{};
        bool ribbonTrailEnabled = false;
        bool ribbonTrailConfigInitialized = false;
        MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig ribbonTrailConfig{};
        std::string ribbonTrailConfigPath{};
        std::string ribbonTrailVfxFolderPath{};
    };

private:
    enum class ImportTarget {
        NONE = 0,
        PROJECTILE,
        PROJECTILE_GLOW_JSON,
        PROJECTILE_GLOW_JSON_EXPORT,
        TRAIL_JSON_IMPORT,
        TRAIL_JSON_EXPORT,
        TRAIL,
        START_VFX,
        END_VFX
    };

    BackgroundWidget backgroundWidget;
    ScrollBarOverlay scrollBarOverlay;
    TileClickMarkerOverlay clickMarker;
    IlluminatedProjectileDebugPanel illuminatedProjectileDebugPanel;
    RC2D_Font overlayFont;

    Ship attackerShip;
    Ship targetShip;
    std::vector<GameplayVfxShipSlot> localVfxShips;

    std::vector<ListItem> shipOptions;
    std::vector<ListItem> projectileOptions;
    std::vector<ListItem> trailOptions;
    std::vector<ListItem> startVfxOptions;
    std::vector<ListItem> endVfxOptions;

    int selectedShipIndex;
    int selectedProjectileIndex;
    int selectedTrailIndex;
    int selectedStartVfxIndex;
    int focusedEndVfxIndex;
    int shipListScrollOffset;
    int projectileListScrollOffset;
    int trailListScrollOffset;
    int startVfxListScrollOffset;
    int endVfxListScrollOffset;
    bool shipListScrollDragActive;
    bool projectileListScrollDragActive;
    bool trailListScrollDragActive;
    bool startVfxListScrollDragActive;
    bool endVfxListScrollDragActive;
    float shipListScrollDragGrabOffsetY;
    float projectileListScrollDragGrabOffsetY;
    float trailListScrollDragGrabOffsetY;
    float startVfxListScrollDragGrabOffsetY;
    float endVfxListScrollDragGrabOffsetY;

    int selectedOceanColorIndex;
    int pendingOceanColorDelta;
    int selectedSalvoBallCount;
    float salvoIntervalSec;
    float salvoTimerSec;
    bool lastObservedGlowConfigValid;
    VFXClassic::IlluminatedProjectileGlowConfig lastObservedGlowConfig;
    bool showLists;
    bool editorTextInputEnabled;
    bool endDelayInputFocused;
    std::string endDelayInputBuffer;
    std::string statusMessage;
    bool blockingPopupVisible;
    std::string blockingPopupMessage;

    SDL_FRect buttonDebugPanelRect;
    SDL_FRect buttonListsVisibilityRect;
    SDL_FRect buttonOceanPrevRect;
    SDL_FRect buttonOceanNextRect;
    SDL_FRect buttonCenterAttackerRect;
    SDL_FRect buttonZoomOutRect;
    SDL_FRect buttonZoomInRect;
    SDL_FRect buttonSalvoOneRect;
    SDL_FRect buttonSalvoFiveRect;
    SDL_FRect buttonSalvoTenRect;
    SDL_FRect buttonShipSpeedDownRect;
    SDL_FRect buttonShipSpeedUpRect;
    SDL_FRect buttonCadenceDownRect;
    SDL_FRect buttonCadenceUpRect;
    SDL_FRect buttonProjectileIlluminatedRect;
    SDL_FRect buttonProjectileTrailRect;
    SDL_FRect buttonProjectileGlowModeRect;
    SDL_FRect buttonProjectileGlowImportRect;
    SDL_FRect buttonProjectileGlowExportRect;
    SDL_FRect endDelayInputRect;

    SDL_FRect shipListRect;
    SDL_FRect projectileListRect;
    SDL_FRect trailListRect;
    SDL_FRect startVfxListRect;
    SDL_FRect endVfxListRect;
    SDL_FRect projectileImportButtonRect;
    SDL_FRect trailImportButtonRect;
    SDL_FRect startVfxImportButtonRect;
    SDL_FRect endVfxImportButtonRect;
    SDL_FRect miniMapRect;
    bool miniMapDragActive;
    float miniMapDragOffsetX;
    float miniMapDragOffsetY;

    bool pendingFolderDialogCompleted;
    bool pendingFolderDialogCanceled;
    ImportTarget pendingImportTarget;
    std::string pendingFolderAbsolute;
    mutable std::mutex pendingFolderMutex;

    void resetEditorState(void);
    void clearLocalVfxShips(void);
    void collectAssetLists(void);
    void collectShipFolders(void);
    void collectProjectileFolders(void);
    void collectTrailFolders(void);
    void collectShipVfxFolders(void);
    bool selectShipAtIndex(int index);
    bool selectProjectileAtIndex(int index);
    bool selectTrailAtIndex(int index);
    bool selectStartVfxAtIndex(int index);
    bool toggleEndVfxAtIndex(int index);
    void refreshEndDelayInputFromFocus(void);
    bool applyFocusedEndVfxDelayInput(void);
    void setFocusedEndVfxDelay(std::uint32_t delayMs);

    void applySelectedOceanColor(void);
    void requestOceanColorStep(int delta);
    void applyPendingOceanColorStep(void);
    void cycleOceanColor(int delta);
    void adjustMapZoom(float delta);
    void centerCameraOnAttacker(void);
    void selectSalvoBallCount(int ballCount);
    void adjustShipsSpeed(float delta);
    bool syncSelectedProjectileGlowConfigFromPanel(void);
    bool applySelectedProjectileGlowConfigToPanel(bool showStatusMessage);
    bool syncSelectedProjectileRibbonTrailConfigFromPanel(void);
    bool applySelectedProjectileRibbonTrailConfigToPanel(bool showStatusMessage);
    void refreshDebugPanelTrailPreview(void);
    bool exportSelectedProjectileGlowConfig(void);
    void toggleSelectedProjectileIlluminated(void);
    void toggleSelectedProjectileRibbonTrail(void);
    void cycleSelectedProjectileGlowMode(void);

    void positionShipsForPreview(bool keepExistingPositions);
    void fireCurrentSalvo(void);
    void updateLocalVfxShipSlotsForDraw(void);
    void drawLocalVfxShipSlots(bool drawBehindShip) const;

    void updateToolbarLayout(void);
    void drawHud(void) const;
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    void drawTextLine(const char* text, float x, float y, RC2D_Color color) const;
    void drawDelayInput(void) const;
    void drawListPanel(
        const SDL_FRect& rect,
        const char* title,
        const std::vector<ListItem>& items,
        int selectedIndex,
        int scrollOffset,
        bool scrollDragActive,
        bool multiSelect,
        bool showDelay,
        const SDL_FRect* importButtonRect,
        const char* importLabel) const;

    int visibleRowsForListPanel(const SDL_FRect& rect, bool hasImportButton) const;
    int maxScrollOffsetForList(const SDL_FRect& rect, bool hasImportButton, int itemCount) const;
    void clampListScrollOffset(int* scrollOffset, const SDL_FRect& rect, bool hasImportButton, int itemCount) const;
    bool handleListPanelClick(
        float x,
        float y,
        const SDL_FRect& rect,
        bool hasImportButton,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY,
        int* outClickedIndex) const;
    void handleListPanelScrollDragFromMouse(
        const SDL_FRect& rect,
        bool hasImportButton,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);
    bool handleToolbarClick(float x, float y);
    bool handleAssetListClick(float x, float y);
    bool handleMapClick(float x, float y, RC2D_MouseButton button);
    bool handleListMouseWheel(float mouseX, float mouseY, int delta);
    bool handleEndDelayInputKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, bool isrepeat);
    void syncEditorTextInputState(void);

    bool tryBuildMiniMapViewRect(SDL_FRect* outRect) const;
    void moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset);
    bool handleMiniMapClick(float x, float y, RC2D_MouseButton button);
    void handleMiniMapDragFromMouse(void);
    void drawMiniMap(void) const;
    void showBlockingPopup(const std::string& message);
    bool handleBlockingPopupClick(float x, float y, RC2D_MouseButton button);
    void drawBlockingPopup(void) const;

    void openImportFolderDialog(ImportTarget target);
    void processPendingFolderRequest(void);
    bool appendImportedFolder(ImportTarget target, const std::string& folderAbsolutePath);

    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    void convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const;
    bool getMouseRenderPosition(float* outX, float* outY) const;
    static void onImportFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    EditorMapCannonSalvoScene(void);
    ~EditorMapCannonSalvoScene(void) override;

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void textinput(const RC2D_TextInputEventInfo* info) override;
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
