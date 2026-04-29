
#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-creatormap.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <vector>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_storage.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_surface.h>
#include <cJSON.h>

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/scenes/editormap-scene-layout.h"
#include "game/render/world-render-clip.h"

// ---------------------------------------------------------------------------
// Constantes et helpers locaux (portee translation-unit uniquement).
// ---------------------------------------------------------------------------
struct OceanColorEntry
{
    OceanShader::WaterColor value;
    const char* label;
};

constexpr int kShipSpriteCount = 8;

constexpr std::array<OceanColorEntry, 27> kOceanColors = {{
    {OceanShader::WaterColor::BLUE, "BLUE"},
    {OceanShader::WaterColor::AMBER, "AMBER"},
    {OceanShader::WaterColor::BROWN, "BROWN"},
    {OceanShader::WaterColor::CORAL, "CORAL"},
    {OceanShader::WaterColor::CYAN, "CYAN"},
    {OceanShader::WaterColor::GREEN, "GREEN"},
    {OceanShader::WaterColor::JADE, "JADE"},
    {OceanShader::WaterColor::LAVENDER, "LAVENDER"},
    {OceanShader::WaterColor::LIME, "LIME"},
    {OceanShader::WaterColor::MAGENTA, "MAGENTA"},
    {OceanShader::WaterColor::MINT, "MINT"},
    {OceanShader::WaterColor::OBSIDIAN, "OBSIDIAN"},
    {OceanShader::WaterColor::ORANGE, "ORANGE"},
    {OceanShader::WaterColor::PEACH, "PEACH"},
    {OceanShader::WaterColor::PINK, "PINK"},
    {OceanShader::WaterColor::PLUM, "PLUM"},
    {OceanShader::WaterColor::PURPLE, "PURPLE"},
    {OceanShader::WaterColor::RED, "RED"},
    {OceanShader::WaterColor::ROSE, "ROSE"},
    {OceanShader::WaterColor::SEAWEED, "SEAWEED"},
    {OceanShader::WaterColor::SLATE, "SLATE"},
    {OceanShader::WaterColor::STORM, "STORM"},
    {OceanShader::WaterColor::SUNSET, "SUNSET"},
    {OceanShader::WaterColor::TEAL, "TEAL"},
    {OceanShader::WaterColor::TURQUOISE, "TURQUOISE"},
    {OceanShader::WaterColor::VIOLET, "VIOLET"},
    {OceanShader::WaterColor::YELLOW, "YELLOW"},
}};

constexpr RC2D_Color kBlockedTileColor = RC2D_Color{210, 55, 55, 108};
constexpr RC2D_Color kGridColor = RC2D_Color{240, 245, 250, 52};
constexpr RC2D_Color kHoverTileColor = RC2D_Color{255, 225, 110, 205};
constexpr RC2D_Color kHudTextColor = RC2D_Color{235, 240, 248, 245};
constexpr RC2D_Color kHudStatusColor = RC2D_Color{230, 200, 90, 250};
constexpr RC2D_Color kAssetPanelFillColor = RC2D_Color{20, 28, 36, 210};
constexpr RC2D_Color kAssetPanelBorderColor = RC2D_Color{135, 150, 168, 220};
constexpr RC2D_Color kAssetRowFillColor = RC2D_Color{32, 40, 50, 210};
constexpr RC2D_Color kAssetRowSelectedFillColor = RC2D_Color{86, 130, 174, 220};
constexpr RC2D_Color kAssetRowBorderColor = RC2D_Color{115, 128, 146, 210};
constexpr float kAssetListScrollBarWidth = 10.0f;
constexpr int kAssetListVisibleRows = 10;
// Minimap editeur: l'export PNG reprend la taille visible du rectangle ecran.
constexpr float kMiniMapVisualScale = 2.0f / 3.0f;

static float computeEditorMiniMapSize(const SDL_FRect& mapRect)
{
    return std::clamp(
        mapRect.h * 0.22f * kMiniMapVisualScale,
        120.0f * kMiniMapVisualScale,
        190.0f * kMiniMapVisualScale);
}

static int miniMapRectDimensionToPixels(float dimension)
{
    return (std::max)(static_cast<int>(std::lround(dimension)), 1);
}

constexpr RC2D_FileDialogFilter kImportFilters[] = {
    {"Images", "png;jpg;jpeg;bmp;webp;tga"},
    {"Tous les fichiers", "*"},
};
constexpr RC2D_FileDialogFilter kExportFilters[] = {
    {"JSON", "json"},
    {"Tous les fichiers", "*"},
};
constexpr RC2D_FileDialogFilter kMapImportFilters[] = {
    {"JSON", "json"},
    {"Tous les fichiers", "*"},
};

constexpr RC2D_FileDialogFilter kShipFolderFilters[] = {
    {"Dossier navire", "*"},
};

constexpr std::array<RC2D_Color, 6> kBlockedTilePalette = {{
    RC2D_Color{210, 55, 55, 108},
    RC2D_Color{120, 170, 255, 116},
    RC2D_Color{236, 190, 78, 118},
    RC2D_Color{180, 95, 220, 116},
    RC2D_Color{72, 196, 150, 114},
    RC2D_Color{255, 125, 90, 112},
}};
constexpr std::array<RC2D_Color, 6> kHotspotPalette = {{
    RC2D_Color{255, 94, 188, 145},
    RC2D_Color{255, 75, 75, 145},
    RC2D_Color{94, 175, 255, 145},
    RC2D_Color{255, 214, 75, 145},
    RC2D_Color{95, 230, 140, 145},
    RC2D_Color{193, 140, 255, 145},
}};

static std::string trimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
    {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }
    return value.substr(start, end - start);
}

static std::string upperAscii(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

static int findOceanColorIndexByLabel(const std::string& rawLabel)
{
    const std::string normalizedLabel = upperAscii(trimAscii(rawLabel));
    if (normalizedLabel.empty())
    {
        return -1;
    }

    for (size_t i = 0; i < kOceanColors.size(); ++i)
    {
        if (normalizedLabel == kOceanColors[i].label)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

static std::string makeAssetLabel(const std::string& name, int maxChars)
{
    if (maxChars <= 3 || static_cast<int>(name.size()) <= maxChars)
    {
        return name;
    }

    return name.substr(0, static_cast<size_t>(maxChars - 3)) + "...";
}


static std::string normalizePathSlashes(const std::string& path)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

static std::string extractFileName(const std::string& path)
{
    const std::string normalized = normalizePathSlashes(path);
    const size_t slashPos = normalized.find_last_of('/');
    if (slashPos == std::string::npos)
    {
        return normalized;
    }

    return normalized.substr(slashPos + 1);
}

static std::string buildRuntimeAssetPathFromSource(const std::string& sourcePath, const std::string& fallbackName)
{
    const std::string normalized = normalizePathSlashes(sourcePath);
    std::string lowered = normalized;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const size_t assetsPos = lowered.find("/assets/");
    if (assetsPos != std::string::npos)
    {
        // Garde toujours le chemin a partir du dossier assets.
        return normalized.substr(assetsPos + 1);
    }

    if (lowered.rfind("assets/", 0) == 0)
    {
        return normalized;
    }

    // Fallback defensif si la source ne vient pas du dossier assets du projet.
    const std::string fileName = extractFileName(fallbackName.empty() ? sourcePath : fallbackName);
    return "assets/" + (fileName.empty() ? std::string("unknown.png") : fileName);
}

static bool tryBuildTitleStoragePathFromAbsolutePath(const std::string& absolutePath, std::string* outStoragePath)
{
    if (outStoragePath == nullptr)
    {
        return false;
    }

    outStoragePath->clear();
    if (absolutePath.empty())
    {
        return false;
    }

    std::error_code rootError;
    std::filesystem::path assetsImagesRoot = std::filesystem::absolute("assets/images", rootError);
    if (rootError)
    {
        return false;
    }

    std::error_code absoluteError;
    std::filesystem::path candidatePath = std::filesystem::absolute(absolutePath, absoluteError);
    if (absoluteError)
    {
        candidatePath = std::filesystem::path(absolutePath);
    }

    const std::string normalizedRoot = normalizePathSlashes(assetsImagesRoot.string());
    const std::string normalizedCandidate = normalizePathSlashes(candidatePath.string());
    if (normalizedRoot.empty() || normalizedCandidate.empty())
    {
        return false;
    }

    std::string loweredRoot = normalizedRoot;
    std::string loweredCandidate = normalizedCandidate;
    std::transform(
        loweredRoot.begin(),
        loweredRoot.end(),
        loweredRoot.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(
        loweredCandidate.begin(),
        loweredCandidate.end(),
        loweredCandidate.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (loweredCandidate == loweredRoot)
    {
        *outStoragePath = "assets/images";
        return true;
    }

    if (loweredCandidate.size() <= loweredRoot.size() ||
        loweredCandidate.compare(0, loweredRoot.size(), loweredRoot) != 0 ||
        normalizedCandidate[normalizedRoot.size()] != '/')
    {
        return false;
    }

    *outStoragePath = "assets/images" + normalizedCandidate.substr(normalizedRoot.size());
    return true;
}

static std::string buildMiniMapPngPathFromJsonPath(const char* jsonAbsolutePath)
{
    if (jsonAbsolutePath == nullptr || jsonAbsolutePath[0] == '\0')
    {
        return std::string();
    }

    std::filesystem::path jsonPath(jsonAbsolutePath);
    const std::filesystem::path pngPath =
        jsonPath.parent_path() /
        (jsonPath.stem().string() + "_minimap.png");
    return pngPath.string();
}

static float miniMapNormalizedToSectorCenter(float normalizedValue, int sectorCount)
{
    const float span = static_cast<float>((std::max)(sectorCount, 1));
    const float maxSectorCenter = static_cast<float>((std::max)(sectorCount - 1, 0));
    return std::clamp((normalizedValue * span) - 0.5f, 0.0f, maxSectorCenter);
}

EditorMapCreateMapScene* EditorMapCreateMapScene::activeInstance = nullptr;

// ---------------------------------------------------------------------------
// Cycle de vie de la scene.
// ---------------------------------------------------------------------------
EditorMapCreateMapScene::EditorMapCreateMapScene(void)
    : backgroundWidget{},
      overlayFont{},
      scrollBarOverlay{},
      editorMode(EditorMode::MAP_CREATOR_MAP),
      editorTool(EditorTool::BLOCK_TILES),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      selectedAssetIndex(-1),
      selectedShipIndex(-1),
      assetListScrollOffset(0),
      shipListScrollOffset(0),
      showGrid(true),
      showBlockedTiles(true),
      showBottomRightLists(true),
      collisionPaintBlocks(true),
      mapNameInput{},
      mapNameInputFocused(false),
      blockedBrushRadiusTiles(0),
      assetTransparencyEnabled(true),
      assetOpacityPercent(100),
      selectedBlockedColorIndex(0),
      selectedHotspotColorIndex(0),
      shipScalePercent(100),
      hoveredTileValid(false),
      hoveredTile{},
      dragPaintActive(false),
      dragPaintBlockedValue(true),
      lastDragPaintTileValid(false),
      lastDragPaintTile{},
      importedAssets{},
      importedShips{},
      placedAssets{},
      towerHotspots{},
      historyActions{},
      historyCursor(0),
      importedAssetCounter(0U),
      statusMessage("Editor map pret."),
      pendingImportDialogCompleted(false),
      pendingImportDialogCanceled(false),
      pendingImportFilePaths{},
      pendingImportMutex{},
      importBatchActive(false),
      importBatchFilePaths{},
      importBatchNextIndex(0),
      importBatchImportedCount(0),
      importBatchFailedCount(0),
      assetListScrollDragActive(false),
      assetListScrollDragGrabOffsetY(0.0f),
      shipListScrollDragActive(false),
      shipListScrollDragGrabOffsetY(0.0f),
      clickMarker{},
      testShip{},
      testShipPreview{},
      testShipLoaded(false),
      testShipSpawned(false),
      testShipCameraFollowEnabled(false),
      loadedShipFolderAbsolute(),
      pendingShipFolderDialogCompleted(false),
      pendingShipFolderDialogCanceled(false),
      pendingShipFolderAbsolute(),
      pendingShipFolderMutex{},
      pendingMapImportDialogCompleted(false),
      pendingMapImportDialogCanceled(false),
      pendingMapImportAbsolutePath(),
      pendingMapImportMutex{},
      buttonImportRect{},
      buttonImportMapRect{},
      buttonImportShipRect{},
      buttonExportRect{},
      buttonUndoRect{},
      buttonRedoRect{},
      buttonToolBlockRect{},
      buttonToolUnblockRect{},
      buttonToolPlaceRect{},
      buttonToolRemoveRect{},
      buttonToolInteractRect{},
      buttonToolShipRect{},
      buttonToolShipControlRect{},
      buttonToolHotspotRect{},
      buttonTowerVariantsPickerRect{},
      buttonTowerDisplayModeRect{},
      buttonAssetPrevRect{},
      buttonAssetNextRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonGridRect{},
      buttonBlockedTilesRect{},
      buttonCenterRect{},
      buttonCenterShipRect{},
      buttonZoomOutRect{},
      buttonZoomInRect{},
      buttonBlockedBrushMinusRect{},
      buttonBlockedBrushPlusRect{},
      buttonBlockedColorPrevRect{},
      buttonBlockedColorNextRect{},
      buttonHotspotColorPrevRect{},
      buttonHotspotColorNextRect{},
      buttonAssetOpacityToggleRect{},
      buttonAssetOpacityMinusRect{},
      buttonAssetOpacityPlusRect{},
      buttonShipScaleMinusRect{},
      buttonShipScalePlusRect{},
      buttonShipReexportRect{},
      buttonListsVisibilityRect{},
      mapNameInputRect{},
      assetListRect{},
      shipListRect{},
      miniMapRect{},
      towerVariantPickerRect{},
      towerVariantLevelTabRects{},
      towerVariantConfirmRect{},
      towerVariantPickerVisible(false),
      towerDisplayPickerRect{},
      towerDisplayLevelTabRects{},
      towerDisplayConfirmRect{},
      towerDisplayPickerVisible(false),
      miniMapDragActive(false),
      miniMapDragOffsetX(0.0f),
      miniMapDragOffsetY(0.0f)
{
}

EditorMapCreateMapScene::~EditorMapCreateMapScene(void)
{
}

void EditorMapCreateMapScene::resetEditorState(void)
{
    this->editorMode = EditorMode::MAP_CREATOR_MAP;
    this->editorTool = EditorTool::BLOCK_TILES;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->selectedAssetIndex = -1;
    this->selectedShipIndex = -1;
    this->assetListScrollOffset = 0;
    this->shipListScrollOffset = 0;
    this->showGrid = true;
    this->showBlockedTiles = true;
    this->showBottomRightLists = true;
    this->collisionPaintBlocks = true;
    this->mapNameInput.clear();
    this->mapNameInputFocused = false;
    this->blockedBrushRadiusTiles = 0;
    this->assetTransparencyEnabled = true;
    this->assetOpacityPercent = 100;
    this->selectedBlockedColorIndex = 0;
    this->selectedHotspotColorIndex = 0;
    this->shipScalePercent = 100;
    this->selectedTowerHotspotNumber = 1;
    this->selectedTowerVariantLevel = 1;
    this->towerPreviewDisplayLevel = 1;
    this->towerPreviewDisplayMode = TowerPreviewDisplayMode::HOTSPOTS;
    this->towerVariantPickerVisible = false;
    this->towerDisplayPickerVisible = false;
    this->selectedAssetClickGuiTarget = EditorMapAssetClickGuiTarget::NONE;
    this->selectedAssetClickDistanceTiles = kEditorMapDefaultAssetClickDistanceTiles;
    this->selectedPlacedAssetIndex = -1;
    this->assetInteractionListScrollOffset = 0;
    this->assetInteractionEditMode = AssetInteractionEditMode::SELECT;
    this->hoveredTileValid = false;
    this->dragPaintActive = false;
    this->dragPaintBlockedValue = true;
    this->lastDragPaintTileValid = false;
    this->assetInteractionPaintActive = false;
    this->assetInteractionPaintValue = true;
    this->lastAssetInteractionPaintTileValid = false;
    this->historyActions.clear();
    this->historyCursor = 0;
    this->towerHotspots.clear();
    this->towerVisualSets.clear();
    this->importedShips.clear();
    this->importedAssetCounter = 0U;
    this->statusMessage = "Editor map pret.";
    this->pendingImportDialogCompleted = false;
    this->pendingImportDialogCanceled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingImportMutex);
        this->pendingImportFilePaths.clear();
    }
    this->importBatchActive = false;
    this->importBatchFilePaths.clear();
    this->importBatchNextIndex = 0;
    this->importBatchImportedCount = 0;
    this->importBatchFailedCount = 0;
    {
    }
    this->assetListScrollDragActive = false;
    this->assetListScrollDragGrabOffsetY = 0.0f;
    this->shipListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->assetInteractionPaintActive = false;
    this->assetInteractionPaintValue = true;
    this->lastAssetInteractionPaintTileValid = false;
    this->clickMarker.hide();
    this->clickMarker.setDurationSeconds(0.85);
    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();
    this->testShipLoaded = false;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->importedShips.clear();
    this->selectedShipIndex = -1;
    this->shipListScrollOffset = 0;
    this->shipListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->loadedShipFolderAbsolute.clear();
    this->pendingShipFolderDialogCompleted = false;
    this->pendingShipFolderDialogCanceled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        this->pendingShipFolderAbsolute.clear();
    }
    this->pendingMapImportDialogCompleted = false;
    this->pendingMapImportDialogCanceled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingMapImportMutex);
        this->pendingMapImportAbsolutePath.clear();
    }
    this->buttonImportRect = SDL_FRect{};
    this->buttonImportMapRect = SDL_FRect{};
    this->buttonImportShipRect = SDL_FRect{};
    this->buttonExportRect = SDL_FRect{};
    this->buttonUndoRect = SDL_FRect{};
    this->buttonRedoRect = SDL_FRect{};
    this->buttonToolBlockRect = SDL_FRect{};
    this->buttonToolUnblockRect = SDL_FRect{};
    this->buttonToolPlaceRect = SDL_FRect{};
    this->buttonToolRemoveRect = SDL_FRect{};
    this->buttonToolInteractRect = SDL_FRect{};
    this->buttonToolShipRect = SDL_FRect{};
    this->buttonToolShipControlRect = SDL_FRect{};
    this->buttonToolHotspotRect = SDL_FRect{};
    this->buttonTowerVariantsPickerRect = SDL_FRect{};
    this->buttonTowerDisplayModeRect = SDL_FRect{};
    this->buttonAssetPrevRect = SDL_FRect{};
    this->buttonAssetNextRect = SDL_FRect{};
    this->buttonOceanPrevRect = SDL_FRect{};
    this->buttonOceanNextRect = SDL_FRect{};
    this->buttonGridRect = SDL_FRect{};
    this->buttonBlockedTilesRect = SDL_FRect{};
    this->buttonCenterRect = SDL_FRect{};
    this->buttonCenterShipRect = SDL_FRect{};
    this->buttonZoomOutRect = SDL_FRect{};
    this->buttonZoomInRect = SDL_FRect{};
    this->buttonBlockedBrushMinusRect = SDL_FRect{};
    this->buttonBlockedBrushPlusRect = SDL_FRect{};
    this->buttonBlockedColorPrevRect = SDL_FRect{};
    this->buttonBlockedColorNextRect = SDL_FRect{};
    this->buttonHotspotColorPrevRect = SDL_FRect{};
    this->buttonHotspotColorNextRect = SDL_FRect{};
    this->buttonAssetOpacityToggleRect = SDL_FRect{};
    this->buttonAssetOpacityMinusRect = SDL_FRect{};
    this->buttonAssetOpacityPlusRect = SDL_FRect{};
    this->buttonShipScaleMinusRect = SDL_FRect{};
    this->buttonShipScalePlusRect = SDL_FRect{};
    this->buttonShipReexportRect = SDL_FRect{};
    this->buttonListsVisibilityRect = SDL_FRect{};
    this->mapNameInputRect = SDL_FRect{};
    this->assetListRect = SDL_FRect{};
    this->shipListRect = SDL_FRect{};
    this->miniMapRect = SDL_FRect{};
    this->towerVariantPickerRect = SDL_FRect{};
    this->towerVariantLevelTabRects = {};
    this->towerVariantConfirmRect = SDL_FRect{};
    this->towerDisplayPickerRect = SDL_FRect{};
    this->towerDisplayLevelTabRects = {};
    this->towerDisplayConfirmRect = SDL_FRect{};
    this->miniMapDragActive = false;
    this->miniMapDragOffsetX = 0.0f;
    this->miniMapDragOffsetY = 0.0f;
}

void EditorMapCreateMapScene::unloadImportedAssets(void)
{
    for (ImportedAsset& asset : this->importedAssets)
    {
        if (asset.useUserStorage)
        {
            ReleaseStorageImageData(&asset.imageData);
            ReleaseStorageImage(&asset.image);
        }
        else
        {
            ResetStorageImageDataRef(&asset.imageData);
            ResetStorageImageRef(&asset.image);
        }
    }

    this->importedAssets.clear();
    this->placedAssets.clear();
    this->towerVisualSets.clear();
    this->historyActions.clear();
    this->historyCursor = 0;
    this->selectedAssetIndex = -1;
    this->assetListScrollOffset = 0;
    {
        std::lock_guard<std::mutex> lock(this->pendingImportMutex);
        this->pendingImportFilePaths.clear();
    }
    this->pendingImportDialogCompleted = false;
    this->pendingImportDialogCanceled = false;
    this->importBatchActive = false;
    this->importBatchFilePaths.clear();
    this->importBatchNextIndex = 0;
    this->importBatchImportedCount = 0;
    this->importBatchFailedCount = 0;
    this->selectedPlacedAssetIndex = -1;
    this->assetInteractionListScrollOffset = 0;
    this->assetInteractionEditMode = AssetInteractionEditMode::SELECT;
    this->assetListScrollDragActive = false;
    this->assetListScrollDragGrabOffsetY = 0.0f;
}


void EditorMapCreateMapScene::ensureUserStorageFolders(void)
{
    rc2d_storage_userMkdir("editor-assets");
    rc2d_storage_userMkdir("editor-map-ship");
    rc2d_storage_userMkdir("editor-map-ship/current");
}

void EditorMapCreateMapScene::applySelectedOceanColor(void)
{
    if (this->selectedOceanColorIndex < 0)
    {
        this->selectedOceanColorIndex = 0;
    }
    if (this->selectedOceanColorIndex >= static_cast<int>(kOceanColors.size()))
    {
        this->selectedOceanColorIndex = static_cast<int>(kOceanColors.size()) - 1;
    }

    const OceanColorEntry& entry = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)];
    if (!GetOceanShader().load(entry.value))
    {
        RC2D_log(RC2D_LOG_WARN, "EditorMapCreateMapScene: echec chargement ocean shader (%s)", entry.label);
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapCreateMapScene::requestOceanColorStep(int delta)
{
    if (delta == 0)
    {
        return;
    }

    this->pendingOceanColorDelta += delta;
    // Evite une file trop longue si un device envoie plusieurs events d'un coup.
    this->pendingOceanColorDelta = (std::max)(this->pendingOceanColorDelta, -8);
    this->pendingOceanColorDelta = (std::min)(this->pendingOceanColorDelta, 8);
}

void EditorMapCreateMapScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapCreateMapScene::cycleOceanColor(int delta)
{
    const int colorCount = static_cast<int>(kOceanColors.size());

    int index = this->selectedOceanColorIndex + delta;
    while (index < 0)
    {
        index += colorCount;
    }
    while (index >= colorCount)
    {
        index -= colorCount;
    }

    this->selectedOceanColorIndex = index;
    this->applySelectedOceanColor();
}

void EditorMapCreateMapScene::convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const
{
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        *outX = windowX;
        *outY = windowY;
        return;
    }

    float renderX = windowX;
    float renderY = windowY;
    if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &renderX, &renderY))
    {
        renderX = windowX;
        renderY = windowY;
    }

    *outX = renderX;
    *outY = renderY;
}

bool EditorMapCreateMapScene::isInsideMapRect(float x, float y) const
{
    const Map& map = GetCurrentMap();
    return (
        x >= map.rect.x &&
        x <= (map.rect.x + map.rect.w) &&
        y >= map.rect.y &&
        y <= (map.rect.y + map.rect.h));
}

bool EditorMapCreateMapScene::getMouseRenderPosition(float* outX, float* outY) const
{
    if (outX == nullptr || outY == nullptr)
    {
        return false;
    }

    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);
    this->convertWindowToRender(windowX, windowY, outX, outY);
    return true;
}

bool EditorMapCreateMapScene::tryGetMouseTile(SDL_Point* outTile) const
{
    if (outTile == nullptr)
    {
        return false;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return false;
    }

    if (!this->isInsideMapRect(mouseX, mouseY))
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    const SDL_Point tile = map.screenToTileNearest(mouseX, mouseY);
    if (!map.isInside(tile.x, tile.y))
    {
        return false;
    }

    *outTile = tile;
    return true;
}

void EditorMapCreateMapScene::updateHoveredTile(void)
{
    SDL_Point tile{};
    if (this->tryGetMouseTile(&tile))
    {
        this->hoveredTileValid = true;
        this->hoveredTile = tile;
        return;
    }

    this->hoveredTileValid = false;
}


int EditorMapCreateMapScene::findPlacedAssetIndexAtTile(int tileX, int tileY) const
{
    for (size_t i = 0; i < this->placedAssets.size(); ++i)
    {
        const PlacedAsset& placedAsset = this->placedAssets[i];
        if (placedAsset.tileX == tileX && placedAsset.tileY == tileY)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool EditorMapCreateMapScene::getPlacedAssetAtTile(int tileX, int tileY, PlacedAsset* outAsset) const
{
    const int index = this->findPlacedAssetIndexAtTile(tileX, tileY);
    if (index < 0)
    {
        return false;
    }

    if (outAsset != nullptr)
    {
        *outAsset = this->placedAssets[static_cast<size_t>(index)];
    }
    return true;
}

bool EditorMapCreateMapScene::isPlacedAssetIndexValid(int index) const
{
    return (
        index >= 0 &&
        index < static_cast<int>(this->placedAssets.size()));
}

void EditorMapCreateMapScene::clearSelectedPlacedAsset(void)
{
    this->selectedPlacedAssetIndex = -1;
    this->assetInteractionListScrollOffset = 0;
    this->assetInteractionEditMode = AssetInteractionEditMode::SELECT;
}

bool EditorMapCreateMapScene::selectPlacedAssetAtScreenPoint(float x, float y)
{
    const int assetIndex = this->findPlacedAssetIndexAtScreenPoint(x, y);
    if (assetIndex < 0 || !this->isPlacedAssetIndexValid(assetIndex))
    {
        return false;
    }

    if (this->selectedPlacedAssetIndex == assetIndex)
    {
        return false;
    }

    this->selectedPlacedAssetIndex = assetIndex;
    const PlacedAsset& selectedPlacedAsset = this->placedAssets[static_cast<size_t>(assetIndex)];
    this->selectedAssetClickGuiTarget = selectedPlacedAsset.clickGuiTarget;
    this->selectedAssetClickDistanceTiles = ClampEditorMapAssetClickDistanceTiles(
        selectedPlacedAsset.clickGuiDistanceTiles);
    for (int i = 0; i < static_cast<int>(kEditorMapAssetClickGuiTargets.size()); ++i)
    {
        if (kEditorMapAssetClickGuiTargets[static_cast<size_t>(i)].target !=
            selectedPlacedAsset.clickGuiTarget)
        {
            continue;
        }

        this->assetInteractionListScrollOffset =
            std::clamp(i - (kAssetListVisibleRows / 2), 0, this->getAssetListMaxScrollOffset());
        break;
    }

    const EditorMapAssetClickGuiTargetInfo& guiInfo =
        GetEditorMapAssetClickGuiTargetInfo(selectedPlacedAsset.clickGuiTarget);
    this->statusMessage =
        "Asset selectionne pour interaction: GUI="
        + std::string(guiInfo.label) + " | Tiles="
        + std::to_string(static_cast<int>(
            this->computePlacedAssetClickInteractionTiles(selectedPlacedAsset).size()))
        + " tiles.";
    return true;
}

void EditorMapCreateMapScene::setPlacedAssetStateAtTile(int tileX, int tileY, bool hasAsset, const PlacedAsset* assetState)
{
    const int existingIndex = this->findPlacedAssetIndexAtTile(tileX, tileY);

    if (!hasAsset)
    {
        // Si on a l'etat exact a supprimer (undo/redo), on privilegie ce match
        // pour eviter toute ambiguite en cas de donnees incoherentes.
        if (assetState != nullptr)
        {
            constexpr float epsilon = 0.0001f;
            for (int i = static_cast<int>(this->placedAssets.size()) - 1; i >= 0; --i)
            {
                const PlacedAsset& placed = this->placedAssets[static_cast<size_t>(i)];
                if (placed.tileX != assetState->tileX || placed.tileY != assetState->tileY)
                {
                    continue;
                }
                if (placed.importedAssetIndex != assetState->importedAssetIndex)
                {
                    continue;
                }
                if (std::fabs(placed.anchorTileX - assetState->anchorTileX) > epsilon ||
                    std::fabs(placed.anchorTileY - assetState->anchorTileY) > epsilon ||
                    std::fabs(placed.scale - assetState->scale) > epsilon)
                {
                    continue;
                }

                this->placedAssets.erase(this->placedAssets.begin() + i);
                if (this->selectedPlacedAssetIndex == i)
                {
                    this->clearSelectedPlacedAsset();
                }
                else if (this->selectedPlacedAssetIndex > i)
                {
                    this->selectedPlacedAssetIndex -= 1;
                }
                return;
            }
        }

        // Fallback: suppression a la tuile.
        if (existingIndex >= 0)
        {
            this->placedAssets.erase(this->placedAssets.begin() + existingIndex);
            if (this->selectedPlacedAssetIndex == existingIndex)
            {
                this->clearSelectedPlacedAsset();
            }
            else if (this->selectedPlacedAssetIndex > existingIndex)
            {
                this->selectedPlacedAssetIndex -= 1;
            }
        }
        return;
    }

    if (assetState == nullptr)
    {
        return;
    }

    if (assetState->importedAssetIndex < 0 ||
        assetState->importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
    {
        // Garde-fou: ne pas poser une reference invalide.
        return;
    }

    PlacedAsset nextAsset = *assetState;
    nextAsset.tileX = tileX;
    nextAsset.tileY = tileY;

    if (existingIndex >= 0)
    {
        this->placedAssets[static_cast<size_t>(existingIndex)] = nextAsset;
        if (this->selectedPlacedAssetIndex == existingIndex)
        {
            this->selectedAssetClickGuiTarget = nextAsset.clickGuiTarget;
            this->selectedAssetClickDistanceTiles = ClampEditorMapAssetClickDistanceTiles(
                nextAsset.clickGuiDistanceTiles);
        }
        return;
    }

    this->placedAssets.push_back(nextAsset);
}

bool EditorMapCreateMapScene::canUndoHistory(void) const
{
    return (this->historyCursor > 0);
}

bool EditorMapCreateMapScene::canRedoHistory(void) const
{
    return (this->historyCursor < static_cast<int>(this->historyActions.size()));
}

void EditorMapCreateMapScene::pushHistoryAction(const HistoryAction& action)
{
    if (this->historyCursor < static_cast<int>(this->historyActions.size()))
    {
        this->historyActions.erase(this->historyActions.begin() + this->historyCursor, this->historyActions.end());
    }

    this->historyActions.push_back(action);
    this->historyCursor = static_cast<int>(this->historyActions.size());
}

void EditorMapCreateMapScene::applyHistoryAction(const HistoryAction& action, bool applyAfter)
{
    if (action.type == HistoryAction::Type::TILE_BLOCK)
    {
        Map& map = GetCurrentMap();
        const bool blockedValue = applyAfter ? action.afterBlocked : action.beforeBlocked;
        map.setTileBlocked(action.tileX, action.tileY, blockedValue);
        return;
    }
    if (action.type == HistoryAction::Type::TILE_BLOCK_BATCH)
    {
        Map& map = GetCurrentMap();
        for (const HistoryAction::TileBlockChange& change : action.tileBlockBatch)
        {
            const bool blockedValue = applyAfter ? change.afterBlocked : change.beforeBlocked;
            map.setTileBlocked(change.tileX, change.tileY, blockedValue);
        }
        return;
    }

    const bool hasAsset = applyAfter ? action.hadAfterAsset : action.hadBeforeAsset;
    if (!hasAsset)
    {
        // Pour les suppressions redo/undo, on transmet l'etat oppose pour
        // retirer le bon asset (et pas juste "quelque chose sur la tuile").
        const PlacedAsset* removedState = nullptr;
        if (applyAfter)
        {
            if (action.hadBeforeAsset)
            {
                removedState = &action.beforeAsset;
            }
        }
        else
        {
            if (action.hadAfterAsset)
            {
                removedState = &action.afterAsset;
            }
        }

        this->setPlacedAssetStateAtTile(action.tileX, action.tileY, false, removedState);
        return;
    }

    const PlacedAsset& assetState = applyAfter ? action.afterAsset : action.beforeAsset;
    this->setPlacedAssetStateAtTile(action.tileX, action.tileY, true, &assetState);
    if (!this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
    {
        this->clearSelectedPlacedAsset();
    }
}

void EditorMapCreateMapScene::undoHistoryAction(void)
{
    if (!this->canUndoHistory())
    {
        this->statusMessage = "Rien a annuler.";
        return;
    }

    this->historyCursor -= 1;
    this->applyHistoryAction(this->historyActions[static_cast<size_t>(this->historyCursor)], false);
    this->statusMessage = "Action annulee.";
}

void EditorMapCreateMapScene::redoHistoryAction(void)
{
    if (!this->canRedoHistory())
    {
        this->statusMessage = "Rien a refaire.";
        return;
    }

    this->applyHistoryAction(this->historyActions[static_cast<size_t>(this->historyCursor)], true);
    this->historyCursor += 1;
    this->statusMessage = "Action refaite.";
}

bool EditorMapCreateMapScene::setTileBlockedWithHistory(int tileX, int tileY, bool blocked)
{
    Map& map = GetCurrentMap();
    if (!map.isInside(tileX, tileY))
    {
        return false;
    }

    const bool beforeBlocked = map.isTileBlocked(tileX, tileY);
    if (beforeBlocked == blocked)
    {
        return false;
    }

    if (!map.setTileBlocked(tileX, tileY, blocked))
    {
        return false;
    }

    HistoryAction action{};
    action.type = HistoryAction::Type::TILE_BLOCK;
    action.tileX = tileX;
    action.tileY = tileY;
    action.beforeBlocked = beforeBlocked;
    action.afterBlocked = blocked;
    this->pushHistoryAction(action);
    return true;
}

bool EditorMapCreateMapScene::applyTileBrushWithHistory(int centerTileX, int centerTileY, bool blocked)
{
    Map& map = GetCurrentMap();
    HistoryAction batchAction{};
    batchAction.type = HistoryAction::Type::TILE_BLOCK_BATCH;

    for (int oy = -this->blockedBrushRadiusTiles; oy <= this->blockedBrushRadiusTiles; ++oy)
    {
        for (int ox = -this->blockedBrushRadiusTiles; ox <= this->blockedBrushRadiusTiles; ++ox)
        {
            const int tx = centerTileX + ox;
            const int ty = centerTileY + oy;
            if (!map.isInside(tx, ty))
            {
                continue;
            }
            const bool beforeBlocked = map.isTileBlocked(tx, ty);
            if (beforeBlocked == blocked)
            {
                continue;
            }
            if (!map.setTileBlocked(tx, ty, blocked))
            {
                continue;
            }

            HistoryAction::TileBlockChange change{};
            change.tileX = tx;
            change.tileY = ty;
            change.beforeBlocked = beforeBlocked;
            change.afterBlocked = blocked;
            batchAction.tileBlockBatch.push_back(change);
        }
    }

    if (batchAction.tileBlockBatch.empty())
    {
        return false;
    }

    // Compat legacy: renseigne aussi les champs scalaires pour debug/inspection.
    batchAction.tileX = centerTileX;
    batchAction.tileY = centerTileY;
    batchAction.beforeBlocked = false;
    batchAction.afterBlocked = blocked;
    this->pushHistoryAction(batchAction);
    return true;
}

void EditorMapCreateMapScene::paintTileAtMouse(bool blocked)
{
    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    if (this->applyTileBrushWithHistory(tile.x, tile.y, blocked))
    {
        this->lastDragPaintTileValid = true;
        this->lastDragPaintTile = tile;
    }
}
void EditorMapCreateMapScene::handleTilePaintFromMouseDrag(void)
{
    if (this->miniMapDragActive)
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    // Pendant le drag de la scrollbar assets, on bloque aussi le paint collision.
    if (this->assetListScrollDragActive)
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    // Pendant le drag de la scrollbar navires, on bloque aussi le paint collision.
    if (this->shipListScrollDragActive)
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    // Quand une barre de scroll est active, on bloque tout paint collision.
    if (this->scrollBarOverlay.isInteracting())
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    if (this->editorTool != EditorTool::BLOCK_TILES)
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    const bool leftDown = rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT);
    const bool rightDown = rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_RIGHT);
    if (!leftDown && !rightDown)
    {
        this->dragPaintActive = false;
        this->lastDragPaintTileValid = false;
        return;
    }

    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    const bool blockedValue = leftDown ? this->collisionPaintBlocks : !this->collisionPaintBlocks;
    if (this->dragPaintActive &&
        this->dragPaintBlockedValue == blockedValue &&
        this->lastDragPaintTileValid &&
        this->lastDragPaintTile.x == tile.x &&
        this->lastDragPaintTile.y == tile.y)
    {
        return;
    }

    if (!this->applyTileBrushWithHistory(tile.x, tile.y, blockedValue))
    {
        return;
    }

    this->dragPaintActive = true;
    this->dragPaintBlockedValue = blockedValue;
    this->lastDragPaintTileValid = true;
    this->lastDragPaintTile = tile;
}

void EditorMapCreateMapScene::placeSelectedAssetAtMouseTile(void)
{
    if (this->selectedAssetIndex < 0 ||
        this->selectedAssetIndex >= static_cast<int>(this->importedAssets.size()))
    {
        this->statusMessage = "Aucun asset selectionne.";
        return;
    }

    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    const float snappedAnchorTileX = static_cast<float>(tile.x);
    const float snappedAnchorTileY = static_cast<float>(tile.y);

    for (PlacedAsset& placedAsset : this->placedAssets)
    {
        if (placedAsset.tileX == tile.x && placedAsset.tileY == tile.y)
        {
            const PlacedAsset beforeAsset = placedAsset;

            placedAsset.importedAssetIndex = this->selectedAssetIndex;
            placedAsset.anchorTileX = snappedAnchorTileX;
            placedAsset.anchorTileY = snappedAnchorTileY;
            placedAsset.scale = 1.0f;
            placedAsset.clickGuiTarget = this->selectedAssetClickGuiTarget;
            placedAsset.clickGuiDistanceTiles = this->selectedAssetClickDistanceTiles;
            placedAsset.clickGuiUsesLegacyRadius = false;
            placedAsset.clickGuiTiles.clear();

            HistoryAction action{};
            action.type = HistoryAction::Type::ASSET_AT_TILE;
            action.tileX = tile.x;
            action.tileY = tile.y;
            action.hadBeforeAsset = true;
            action.hadAfterAsset = true;
            action.beforeAsset = beforeAsset;
            action.afterAsset = placedAsset;
            this->pushHistoryAction(action);

            const EditorMapAssetClickGuiTargetInfo& guiInfo =
                GetEditorMapAssetClickGuiTargetInfo(placedAsset.clickGuiTarget);
            if (placedAsset.clickGuiTarget == EditorMapAssetClickGuiTarget::NONE)
            {
                this->statusMessage = "Asset remplace (sans GUI au clic).";
            }
            else
            {
                this->statusMessage =
                    "Asset remplace + GUI " + std::string(guiInfo.label) +
                    " (0 tile).";
            }
            return;
        }
    }

    PlacedAsset placedAsset{};
    placedAsset.importedAssetIndex = this->selectedAssetIndex;
    placedAsset.tileX = tile.x;
    placedAsset.tileY = tile.y;
    placedAsset.anchorTileX = snappedAnchorTileX;
    placedAsset.anchorTileY = snappedAnchorTileY;
    placedAsset.scale = 1.0f;
    placedAsset.clickGuiTarget = this->selectedAssetClickGuiTarget;
    placedAsset.clickGuiDistanceTiles = this->selectedAssetClickDistanceTiles;
    placedAsset.clickGuiUsesLegacyRadius = false;
    placedAsset.clickGuiTiles.clear();
    this->placedAssets.push_back(placedAsset);

    HistoryAction action{};
    action.type = HistoryAction::Type::ASSET_AT_TILE;
    action.tileX = tile.x;
    action.tileY = tile.y;
    action.hadBeforeAsset = false;
    action.hadAfterAsset = true;
    action.afterAsset = placedAsset;
    this->pushHistoryAction(action);

    const EditorMapAssetClickGuiTargetInfo& guiInfo =
        GetEditorMapAssetClickGuiTargetInfo(placedAsset.clickGuiTarget);
    if (placedAsset.clickGuiTarget == EditorMapAssetClickGuiTarget::NONE)
    {
        this->statusMessage = "Asset pose (sans GUI au clic).";
    }
    else
    {
        this->statusMessage =
            "Asset pose + GUI " + std::string(guiInfo.label) +
            " (0 tile).";
    }
}

void EditorMapCreateMapScene::removeAssetAtMouseTile(void)
{
    Map& map = GetCurrentMap();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY) || !this->isInsideMapRect(mouseX, mouseY))
    {
        return;
    }

    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    std::vector<size_t> candidates;
    candidates.reserve(this->placedAssets.size());

    for (size_t i = 0; i < this->placedAssets.size(); ++i)
    {
        const PlacedAsset& placedAsset = this->placedAssets[i];
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];
        if (importedAsset.image.sdl_texture == nullptr)
        {
            continue;
        }

        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(placedAsset.anchorTileX, placedAsset.anchorTileY);
        const float effectiveScale = placedAsset.scale * worldZoom;
        const float drawX = anchorScreen.x;
        const float drawY = anchorScreen.y;
        const float drawW = importedAsset.widthPx * effectiveScale;
        const float drawH = importedAsset.heightPx * effectiveScale;
        if (drawW <= 0.0f || drawH <= 0.0f)
        {
            continue;
        }

        if (mouseX >= drawX &&
            mouseX <= (drawX + drawW) &&
            mouseY >= drawY &&
            mouseY <= (drawY + drawH))
        {
            candidates.push_back(i);
        }
    }

    // Si aucun clic direct sur sprite, fallback sur la logique "tile exacte".
    if (candidates.empty())
    {
        SDL_Point tile{};
        if (!this->tryGetMouseTile(&tile))
        {
            return;
        }

        for (int i = static_cast<int>(this->placedAssets.size()) - 1; i >= 0; --i)
        {
            const PlacedAsset& placedAsset = this->placedAssets[static_cast<size_t>(i)];
            if (placedAsset.tileX == tile.x && placedAsset.tileY == tile.y)
            {
                HistoryAction action{};
                action.type = HistoryAction::Type::ASSET_AT_TILE;
                action.tileX = placedAsset.tileX;
                action.tileY = placedAsset.tileY;
                action.hadBeforeAsset = true;
                action.hadAfterAsset = false;
                action.beforeAsset = placedAsset;

                this->placedAssets.erase(this->placedAssets.begin() + i);
                if (this->selectedPlacedAssetIndex == i)
                {
                    this->clearSelectedPlacedAsset();
                }
                else if (this->selectedPlacedAssetIndex > i)
                {
                    this->selectedPlacedAssetIndex -= 1;
                }
                this->pushHistoryAction(action);
                this->statusMessage = "Asset supprime.";
                return;
            }
        }
        return;
    }

    // Supprime l'asset visuellement "au-dessus" (dernier dans l'ordre de draw).
    std::sort(
        candidates.begin(),
        candidates.end(),
        [this](size_t a, size_t b) {
            const PlacedAsset& assetA = this->placedAssets[a];
            const PlacedAsset& assetB = this->placedAssets[b];
            const int depthA = assetA.tileX + assetA.tileY;
            const int depthB = assetB.tileX + assetB.tileY;
            if (depthA != depthB)
            {
                return depthA < depthB;
            }
            if (assetA.tileY != assetB.tileY)
            {
                return assetA.tileY < assetB.tileY;
            }
            return a < b;
        });

    const size_t targetIndex = candidates.back();
    if (targetIndex >= this->placedAssets.size())
    {
        return;
    }

    const PlacedAsset removedAsset = this->placedAssets[targetIndex];
    HistoryAction action{};
    action.type = HistoryAction::Type::ASSET_AT_TILE;
    action.tileX = removedAsset.tileX;
    action.tileY = removedAsset.tileY;
    action.hadBeforeAsset = true;
    action.hadAfterAsset = false;
    action.beforeAsset = removedAsset;

    this->placedAssets.erase(this->placedAssets.begin() + static_cast<std::ptrdiff_t>(targetIndex));
    if (this->selectedPlacedAssetIndex == static_cast<int>(targetIndex))
    {
        this->clearSelectedPlacedAsset();
    }
    else if (this->selectedPlacedAssetIndex > static_cast<int>(targetIndex))
    {
        this->selectedPlacedAssetIndex -= 1;
    }
    this->pushHistoryAction(action);
    this->statusMessage = "Asset supprime.";
}




bool EditorMapCreateMapScene::importAssetFromAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Import annule.";
        return false;
    }

    std::ifstream input(absolutePath, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        this->statusMessage = "Impossible de lire le fichier.";
        return false;
    }

    const std::streamsize fileSize = input.tellg();
    if (fileSize <= 0)
    {
        this->statusMessage = "Fichier vide.";
        return false;
    }

    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(fileSize));
    if (!input.read(bytes.data(), fileSize))
    {
        this->statusMessage = "Lecture asset echouee.";
        return false;
    }

    std::string sourcePath(absolutePath);
    const size_t lastSlash = sourcePath.find_last_of("/\\");
    const std::string fileName = (lastSlash == std::string::npos) ? sourcePath : sourcePath.substr(lastSlash + 1);

    std::string extension = ".png";
    const size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        extension = fileName.substr(dotPos);
    }

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extension.empty() || extension[0] != '.' || extension.size() > 8)
    {
        extension = ".png";
    }

    this->ensureUserStorageFolders();

    ++this->importedAssetCounter;
    char storagePath[256] = {};
    SDL_snprintf(
        storagePath,
        sizeof(storagePath),
        "editor-assets/imported-%04u%s",
        this->importedAssetCounter,
        extension.c_str());

    if (!rc2d_storage_userWriteFile(storagePath, bytes.data(), static_cast<Uint64>(bytes.size())))
    {
        this->statusMessage = "Echec copie en storage user.";
        return false;
    }

    RC2D_Image image = LoadStorageImage(storagePath, RC2D_STORAGE_USER);
    if (image.sdl_texture == nullptr)
    {
        this->statusMessage = "Echec chargement texture importee.";
        return false;
    }

    SDL_SetTextureScaleMode(image.sdl_texture, SDL_SCALEMODE_LINEAR);

    float widthPx = 0.0f;
    float heightPx = 0.0f;
    if (!SDL_GetTextureSize(image.sdl_texture, &widthPx, &heightPx))
    {
        widthPx = 0.0f;
        heightPx = 0.0f;
    }

    RC2D_ImageData imageData =
        LoadStorageImageData(storagePath, RC2D_STORAGE_USER);

    int alphaMaskWidth = 0;
    int alphaMaskHeight = 0;
    std::vector<Uint8> alphaMask;
    if (imageData.sdl_surface != nullptr)
    {
        alphaMaskWidth = imageData.sdl_surface->w;
        alphaMaskHeight = imageData.sdl_surface->h;
        if (alphaMaskWidth > 0 && alphaMaskHeight > 0)
        {
            alphaMask.assign(
                static_cast<size_t>(alphaMaskWidth * alphaMaskHeight),
                static_cast<Uint8>(0));

            for (int y = 0; y < alphaMaskHeight; ++y)
            {
                for (int x = 0; x < alphaMaskWidth; ++x)
                {
                    Uint8 r = 0;
                    Uint8 g = 0;
                    Uint8 b = 0;
                    Uint8 a = 0;
                    if (!SDL_ReadSurfacePixel(imageData.sdl_surface, x, y, &r, &g, &b, &a))
                    {
                        continue;
                    }

                    alphaMask[static_cast<size_t>((y * alphaMaskWidth) + x)] = a;
                }
            }
        }
    }

    ImportedAsset importedAsset{};
    importedAsset.id = "asset_" + std::to_string(this->importedAssetCounter);
    importedAsset.displayName = fileName.empty() ? importedAsset.id : fileName;
    importedAsset.sourcePath = sourcePath;
    importedAsset.storagePath = storagePath;
    importedAsset.useUserStorage = true;
    importedAsset.image = image;
    importedAsset.imageData = imageData;
    importedAsset.widthPx = widthPx;
    importedAsset.heightPx = heightPx;
    importedAsset.alphaMaskWidth = alphaMaskWidth;
    importedAsset.alphaMaskHeight = alphaMaskHeight;
    importedAsset.alphaMask = std::move(alphaMask);
    this->importedAssets.push_back(importedAsset);

    this->selectedAssetIndex = static_cast<int>(this->importedAssets.size()) - 1;
    this->ensureSelectedAssetVisible();
    this->statusMessage = "Asset importe: " + importedAsset.displayName;
    return true;
}



int EditorMapCreateMapScene::importAssetFromRuntimeStoragePath(const std::string& runtimePath)
{
    const std::string normalizedPath = normalizePathSlashes(runtimePath);
    if (normalizedPath.empty())
    {
        return -1;
    }

    for (size_t i = 0; i < this->importedAssets.size(); ++i)
    {
        if (normalizePathSlashes(this->importedAssets[i].storagePath) == normalizedPath)
        {
            return static_cast<int>(i);
        }
    }

    RC2D_Image image = LoadStorageImage(normalizedPath.c_str(), RC2D_STORAGE_TITLE);
    if (image.sdl_texture == nullptr)
    {
        return -1;
    }
    SDL_SetTextureScaleMode(image.sdl_texture, SDL_SCALEMODE_LINEAR);

    float widthPx = 0.0f;
    float heightPx = 0.0f;
    if (!SDL_GetTextureSize(image.sdl_texture, &widthPx, &heightPx))
    {
        widthPx = 0.0f;
        heightPx = 0.0f;
    }

    RC2D_ImageData imageData = LoadStorageImageData(normalizedPath.c_str(), RC2D_STORAGE_TITLE);
    int alphaMaskWidth = 0;
    int alphaMaskHeight = 0;
    std::vector<Uint8> alphaMask;
    if (imageData.sdl_surface != nullptr)
    {
        alphaMaskWidth = imageData.sdl_surface->w;
        alphaMaskHeight = imageData.sdl_surface->h;
        if (alphaMaskWidth > 0 && alphaMaskHeight > 0)
        {
            alphaMask.assign(static_cast<size_t>(alphaMaskWidth * alphaMaskHeight), static_cast<Uint8>(0));
            for (int y = 0; y < alphaMaskHeight; ++y)
            {
                for (int x = 0; x < alphaMaskWidth; ++x)
                {
                    Uint8 r = 0;
                    Uint8 g = 0;
                    Uint8 b = 0;
                    Uint8 a = 0;
                    if (!SDL_ReadSurfacePixel(imageData.sdl_surface, x, y, &r, &g, &b, &a))
                    {
                        continue;
                    }
                    alphaMask[static_cast<size_t>((y * alphaMaskWidth) + x)] = a;
                }
            }
        }
    }

    ImportedAsset importedAsset{};
    ++this->importedAssetCounter;
    importedAsset.id = "asset_runtime_" + std::to_string(this->importedAssetCounter);
    importedAsset.displayName = extractFileName(normalizedPath);
    importedAsset.sourcePath = normalizedPath;
    importedAsset.storagePath = normalizedPath;
    importedAsset.useUserStorage = false;
    importedAsset.image = image;
    importedAsset.imageData = imageData;
    importedAsset.widthPx = widthPx;
    importedAsset.heightPx = heightPx;
    importedAsset.alphaMaskWidth = alphaMaskWidth;
    importedAsset.alphaMaskHeight = alphaMaskHeight;
    importedAsset.alphaMask = std::move(alphaMask);
    this->importedAssets.push_back(importedAsset);
    return static_cast<int>(this->importedAssets.size()) - 1;
}


bool EditorMapCreateMapScene::isTowerAssetName(const std::string& displayName) const
{
    std::string lower = displayName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (lower.find("tower") != std::string::npos || lower.find("tour") != std::string::npos);
}

SDL_Point EditorMapCreateMapScene::computePlacedAssetCenterTile(const PlacedAsset& asset) const
{
    const Map& map = GetCurrentMap();
    SDL_Point fallback = SDL_Point{asset.tileX, asset.tileY};
    if (asset.importedAssetIndex < 0 || asset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
    {
        return fallback;
    }
    const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(asset.importedAssetIndex)];
    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float effectiveScale = asset.scale * worldZoom;
    const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(asset.anchorTileX, asset.anchorTileY);
    const float centerScreenX = anchorScreen.x + ((importedAsset.widthPx * effectiveScale) * 0.5f);
    const float centerScreenY = anchorScreen.y + ((importedAsset.heightPx * effectiveScale) * 0.5f);
    return map.screenToTileNearest(centerScreenX, centerScreenY);
}

std::vector<SDL_Point> EditorMapCreateMapScene::computePlacedAssetClickInteractionTiles(
    const PlacedAsset& asset) const
{
    if (!asset.clickGuiTiles.empty())
    {
        return asset.clickGuiTiles;
    }

    if (!asset.clickGuiUsesLegacyRadius)
    {
        return {};
    }

    std::vector<SDL_Point> coveredTiles;

    const Map& map = GetCurrentMap();
    if (map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return coveredTiles;
    }

    const SDL_Point centerTile = this->computePlacedAssetCenterTile(asset);
    const int radiusTiles = ClampEditorMapAssetClickDistanceTiles(asset.clickGuiDistanceTiles);

    for (int offsetY = -radiusTiles; offsetY <= radiusTiles; ++offsetY)
    {
        for (int offsetX = -radiusTiles; offsetX <= radiusTiles; ++offsetX)
        {
            if ((offsetX * offsetX) + (offsetY * offsetY) > (radiusTiles * radiusTiles))
            {
                continue;
            }

            const int tileX = centerTile.x + offsetX;
            const int tileY = centerTile.y + offsetY;
            if (!map.isInside(tileX, tileY))
            {
                continue;
            }

            coveredTiles.push_back(SDL_Point{tileX, tileY});
        }
    }

    return coveredTiles;
}

bool EditorMapCreateMapScene::placedAssetHasClickInteractionTile(
    const PlacedAsset& asset,
    int tileX,
    int tileY) const
{
    for (const SDL_Point& tile : asset.clickGuiTiles)
    {
        if (tile.x == tileX && tile.y == tileY)
        {
            return true;
        }
    }

    return false;
}

bool EditorMapCreateMapScene::setPlacedAssetClickInteractionTile(
    int placedAssetIndex,
    int tileX,
    int tileY,
    bool enabled)
{
    if (!this->isPlacedAssetIndexValid(placedAssetIndex))
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    if (!map.isInside(tileX, tileY))
    {
        return false;
    }

    PlacedAsset& asset = this->placedAssets[static_cast<size_t>(placedAssetIndex)];
    if (asset.clickGuiUsesLegacyRadius && asset.clickGuiTiles.empty())
    {
        asset.clickGuiTiles = this->computePlacedAssetClickInteractionTiles(asset);
    }
    asset.clickGuiUsesLegacyRadius = false;

    auto it = std::find_if(
        asset.clickGuiTiles.begin(),
        asset.clickGuiTiles.end(),
        [tileX, tileY](const SDL_Point& tile) {
            return tile.x == tileX && tile.y == tileY;
        });

    if (enabled)
    {
        if (it != asset.clickGuiTiles.end())
        {
            return false;
        }

        asset.clickGuiTiles.push_back(SDL_Point{tileX, tileY});
        return true;
    }

    if (it == asset.clickGuiTiles.end())
    {
        return false;
    }

    asset.clickGuiTiles.erase(it);
    return true;
}

void EditorMapCreateMapScene::paintSelectedPlacedAssetInteractionAtMouse(bool enabled)
{
    if (!this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
    {
        return;
    }

    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    if (!this->setPlacedAssetClickInteractionTile(
            this->selectedPlacedAssetIndex,
            tile.x,
            tile.y,
            enabled))
    {
        return;
    }

    this->lastAssetInteractionPaintTileValid = true;
    this->lastAssetInteractionPaintTile = tile;

    const PlacedAsset& asset =
        this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)];
    const EditorMapAssetClickGuiTargetInfo& guiInfo =
        GetEditorMapAssetClickGuiTargetInfo(asset.clickGuiTarget);
    this->statusMessage =
        std::string(enabled ? "Tile interaction ajoutee: " : "Tile interaction retiree: ")
        + std::to_string(tile.x) + "," + std::to_string(tile.y)
        + " | GUI=" + std::string(guiInfo.label)
        + " | Tiles=" + std::to_string(static_cast<int>(asset.clickGuiTiles.size()));
}

int EditorMapCreateMapScene::findTowerHotspotIndexAtTile(int tileX, int tileY) const
{
    for (int i = 0; i < static_cast<int>(this->towerHotspots.size()); ++i)
    {
        const TowerHotspot& hotspot = this->towerHotspots[static_cast<size_t>(i)];
        if (hotspot.tileX == tileX && hotspot.tileY == tileY)
        {
            return i;
        }
    }

    return -1;
}

int EditorMapCreateMapScene::findTowerHotspotIndexByNumber(int towerNumber) const
{
    const int clampedTowerNumber = ClampEditorMapTowerHotspotNumber(towerNumber);
    for (int i = 0; i < static_cast<int>(this->towerHotspots.size()); ++i)
    {
        if (this->towerHotspots[static_cast<size_t>(i)].towerNumber == clampedTowerNumber)
        {
            return i;
        }
    }

    return -1;
}

int EditorMapCreateMapScene::findTowerVisualSetIndexByNumber(int towerNumber) const
{
    const int clampedTowerNumber = ClampEditorMapTowerHotspotNumber(towerNumber);
    for (int i = 0; i < static_cast<int>(this->towerVisualSets.size()); ++i)
    {
        if (this->towerVisualSets[static_cast<size_t>(i)].towerNumber == clampedTowerNumber)
        {
            return i;
        }
    }

    return -1;
}

EditorMapCreateMapScene::TowerVisualSet& EditorMapCreateMapScene::ensureTowerVisualSet(int towerNumber)
{
    const int clampedTowerNumber = ClampEditorMapTowerHotspotNumber(towerNumber);
    const int existingIndex = this->findTowerVisualSetIndexByNumber(clampedTowerNumber);
    if (existingIndex >= 0)
    {
        return this->towerVisualSets[static_cast<size_t>(existingIndex)];
    }

    TowerVisualSet visualSet{};
    visualSet.towerNumber = clampedTowerNumber;
    if (!this->towerVisualSets.empty())
    {
        visualSet.importedAssetIndices = this->towerVisualSets.front().importedAssetIndices;
    }
    this->towerVisualSets.push_back(visualSet);
    return this->towerVisualSets.back();
}

int EditorMapCreateMapScene::findFirstFreeTowerHotspotNumber(void) const
{
    for (int towerNumber = 1; towerNumber <= kEditorMapMaxTowerHotspots; ++towerNumber)
    {
        if (this->findTowerHotspotIndexByNumber(towerNumber) < 0)
        {
            return towerNumber;
        }
    }

    return 0;
}

void EditorMapCreateMapScene::toggleTowerHotspotAtTile(int tileX, int tileY)
{
    const int towerNumber = ClampEditorMapTowerHotspotNumber(this->selectedTowerHotspotNumber);
    const int hotspotAtTileIndex = this->findTowerHotspotIndexAtTile(tileX, tileY);
    if (hotspotAtTileIndex >= 0 &&
        this->towerHotspots[static_cast<size_t>(hotspotAtTileIndex)].towerNumber == towerNumber)
    {
        this->towerHotspots.erase(
            this->towerHotspots.begin() + static_cast<std::ptrdiff_t>(hotspotAtTileIndex));
        this->statusMessage = "Hotspot tour " + std::to_string(towerNumber) + " retire.";
        return;
    }

    const int hotspotForTowerIndex = this->findTowerHotspotIndexByNumber(towerNumber);
    if (hotspotForTowerIndex >= 0)
    {
        this->towerHotspots[static_cast<size_t>(hotspotForTowerIndex)].tileX = tileX;
        this->towerHotspots[static_cast<size_t>(hotspotForTowerIndex)].tileY = tileY;
        (void)this->ensureTowerVisualSet(towerNumber);

        if (hotspotAtTileIndex >= 0 && hotspotAtTileIndex != hotspotForTowerIndex)
        {
            this->towerHotspots.erase(
                this->towerHotspots.begin() + static_cast<std::ptrdiff_t>(hotspotAtTileIndex));
        }

        this->statusMessage = "Hotspot tour " + std::to_string(towerNumber) + " deplace.";
        return;
    }

    if (hotspotAtTileIndex >= 0)
    {
        this->towerHotspots[static_cast<size_t>(hotspotAtTileIndex)].towerNumber = towerNumber;
        (void)this->ensureTowerVisualSet(towerNumber);
        this->statusMessage = "Hotspot tour " + std::to_string(towerNumber) + " affecte.";
        return;
    }

    if (this->towerHotspots.size() >= static_cast<size_t>(kEditorMapMaxTowerHotspots))
    {
        this->statusMessage = "Limite atteinte: 12 hotspots tour max.";
        return;
    }

    this->towerHotspots.push_back(TowerHotspot{tileX, tileY, towerNumber});
    (void)this->ensureTowerVisualSet(towerNumber);
    this->statusMessage = "Hotspot tour " + std::to_string(towerNumber) + " ajoute.";
}

void EditorMapCreateMapScene::setAssetOpacityPercent(int value)
{
    this->assetOpacityPercent = std::clamp(value, 10, 100);
}

void EditorMapCreateMapScene::setShipScalePercent(int value)
{
    this->shipScalePercent = std::clamp(value, 10, 100);
    const float scale = static_cast<float>(this->shipScalePercent) / 100.0f;
    this->testShip.setDrawScale(scale);
    this->testShipPreview.setDrawScale(scale);
}

int EditorMapCreateMapScene::findPlacedAssetIndexAtScreenPoint(float x, float y) const
{
    const Map& map = GetCurrentMap();
    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    std::vector<std::pair<size_t, int>> candidates;
    candidates.reserve(this->placedAssets.size());

    for (size_t i = 0; i < this->placedAssets.size(); ++i)
    {
        const PlacedAsset& placedAsset = this->placedAssets[i];
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];
        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(placedAsset.anchorTileX, placedAsset.anchorTileY);
        const float effectiveScale = placedAsset.scale * worldZoom;
        const float drawW = importedAsset.widthPx * effectiveScale;
        const float drawH = importedAsset.heightPx * effectiveScale;
        if (x < anchorScreen.x || y < anchorScreen.y || x > (anchorScreen.x + drawW) || y > (anchorScreen.y + drawH))
        {
            continue;
        }

        const int depth = placedAsset.tileX + placedAsset.tileY;
        candidates.emplace_back(i, depth);
    }

    if (candidates.empty())
    {
        return -1;
    }
    std::sort(candidates.begin(), candidates.end(), [this](const auto& a, const auto& b) {
        const PlacedAsset& assetA = this->placedAssets[a.first];
        const PlacedAsset& assetB = this->placedAssets[b.first];
        if (a.second != b.second)
        {
            return a.second < b.second;
        }
        if (assetA.tileY != assetB.tileY)
        {
            return assetA.tileY < assetB.tileY;
        }
        return a.first < b.first;
    });
    return static_cast<int>(candidates.back().first);
}

void EditorMapCreateMapScene::processPendingImportRequests(void)
{
    // IMPORTANT:
    // Le callback de file dialog peut arriver a un moment ou un pass GPU est actif.
    // On differe donc tout chargement texture ici, dans l'update de scene.
    std::vector<std::string> newFilePaths;
    bool hasCompletedDialog = false;
    bool importCanceled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingImportMutex);
        hasCompletedDialog = this->pendingImportDialogCompleted;
        if (hasCompletedDialog)
        {
            importCanceled = this->pendingImportDialogCanceled;
            newFilePaths.swap(this->pendingImportFilePaths);
            this->pendingImportDialogCompleted = false;
            this->pendingImportDialogCanceled = false;
        }
    }

    if (hasCompletedDialog && importCanceled)
    {
        this->statusMessage = "Import annule.";
    }

    if (hasCompletedDialog && !importCanceled && !newFilePaths.empty())
    {
        // Demarre un nouveau batch s'il n'y en a pas en cours.
        if (!this->importBatchActive)
        {
            this->importBatchActive = true;
            this->importBatchFilePaths.clear();
            this->importBatchNextIndex = 0;
            this->importBatchImportedCount = 0;
            this->importBatchFailedCount = 0;
        }

        // Empile les fichiers a importer. Permet aussi d'ajouter un 2e lot.
        this->importBatchFilePaths.insert(
            this->importBatchFilePaths.end(),
            newFilePaths.begin(),
            newFilePaths.end());
    }

    if (!this->importBatchActive)
    {
        return;
    }

    const size_t totalCount = this->importBatchFilePaths.size();

    // Limite d'import par frame pour garder l'UI reactive.
    constexpr size_t kMaxImportsPerFrame = 2;
    size_t importedThisFrame = 0;
    while (importedThisFrame < kMaxImportsPerFrame &&
           this->importBatchNextIndex < totalCount)
    {
        const std::string& filePath = this->importBatchFilePaths[this->importBatchNextIndex];
        if (this->importAssetFromAbsolutePath(filePath.c_str()))
        {
            this->importBatchImportedCount += 1;
        }
        else
        {
            this->importBatchFailedCount += 1;
        }
        this->importBatchNextIndex += 1;
        importedThisFrame += 1;
    }

    const size_t processedNow = this->importBatchNextIndex;
    if (processedNow < totalCount)
    {
        // Message de progression visible pendant les gros imports.
        this->statusMessage =
            "Import assets: " +
            std::to_string(processedNow) + "/" +
            std::to_string(totalCount) + "...";
        return;
    }

    const int importedCount = this->importBatchImportedCount;
    const int failedCount = this->importBatchFailedCount;
    if (importedCount > 0 && failedCount == 0)
    {
        this->statusMessage = std::to_string(importedCount) + " asset(s) importe(s).";
    }
    else if (importedCount > 0 && failedCount > 0)
    {
        this->statusMessage =
            std::to_string(importedCount) + " asset(s) importe(s), " +
            std::to_string(failedCount) + " en echec.";
    }
    else
    {
        this->statusMessage = "Import en echec.";
    }

    this->importBatchActive = false;
    this->importBatchFilePaths.clear();
    this->importBatchNextIndex = 0;
    this->importBatchImportedCount = 0;
    this->importBatchFailedCount = 0;
}


bool EditorMapCreateMapScene::renderStyledMiniMapToSurface(SDL_Surface* targetSurface) const
{
    if (targetSurface == nullptr || targetSurface->w <= 0 || targetSurface->h <= 0)
    {
        return false;
    }

    const int outWidth = targetSurface->w;
    const int outHeight = targetSurface->h;
    std::vector<Uint8> landMask(
        static_cast<size_t>(outWidth * outHeight),
        static_cast<Uint8>(0));

    const Map& map = GetCurrentMap();
    const float sectorSpanX = static_cast<float>((std::max)(Map::NUM_SECTORS_X, 1));
    const float sectorSpanY = static_cast<float>((std::max)(Map::NUM_SECTORS_Y, 1));
    const float zoomFactor = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float baseTileWidth = (std::max)(map.getTileWidth() / zoomFactor, 1.0f);
    const float baseTileHeight = (std::max)(map.getTileHeight() / zoomFactor, 1.0f);
    const float sectorStep = static_cast<float>(Map::SECTOR_STEP);
    constexpr Uint8 alphaThreshold = 18;

    std::vector<size_t> drawOrder;
    drawOrder.reserve(this->placedAssets.size());
    for (size_t i = 0; i < this->placedAssets.size(); ++i)
    {
        const PlacedAsset& placedAsset = this->placedAssets[i];
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }
        drawOrder.push_back(i);
    }

    std::sort(
        drawOrder.begin(),
        drawOrder.end(),
        [this](size_t a, size_t b) {
            const PlacedAsset& assetA = this->placedAssets[a];
            const PlacedAsset& assetB = this->placedAssets[b];
            const int depthA = assetA.tileX + assetA.tileY;
            const int depthB = assetB.tileX + assetB.tileY;
            if (depthA != depthB)
            {
                return depthA < depthB;
            }
            if (assetA.tileY != assetB.tileY)
            {
                return assetA.tileY < assetB.tileY;
            }
            return a < b;
        });

    for (size_t drawIndex : drawOrder)
    {
        const PlacedAsset& placedAsset = this->placedAssets[drawIndex];
        const ImportedAsset& importedAsset =
            this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];

        if (importedAsset.alphaMask.empty() ||
            importedAsset.alphaMaskWidth <= 0 ||
            importedAsset.alphaMaskHeight <= 0)
        {
            continue;
        }

        const float assetScale = (std::max)(placedAsset.scale, 0.001f);
        const float assetWidthPx = (std::max)(
            importedAsset.widthPx > 0.0f ? importedAsset.widthPx : static_cast<float>(importedAsset.alphaMaskWidth),
            1.0f);
        const float assetHeightPx = (std::max)(
            importedAsset.heightPx > 0.0f ? importedAsset.heightPx : static_cast<float>(importedAsset.alphaMaskHeight),
            1.0f);
        const float assetSectorWidth = (assetWidthPx * assetScale) / (sectorStep * baseTileWidth);
        const float assetSectorHeight = (assetHeightPx * assetScale) / (sectorStep * baseTileHeight);

        const SDL_FPoint anchorSector = map.tileToSectorFloat(
            placedAsset.anchorTileX,
            placedAsset.anchorTileY);
        const float topLeftSectorX = anchorSector.x + 0.5f;
        const float topLeftSectorY = anchorSector.y + 0.5f;

        const float dstX = (topLeftSectorX / sectorSpanX) * static_cast<float>(outWidth);
        const float dstY = (topLeftSectorY / sectorSpanY) * static_cast<float>(outHeight);
        const float dstW = (assetSectorWidth / sectorSpanX) * static_cast<float>(outWidth);
        const float dstH = (assetSectorHeight / sectorSpanY) * static_cast<float>(outHeight);
        if (dstW <= 0.0f || dstH <= 0.0f)
        {
            continue;
        }

        const int startX = (std::max)(static_cast<int>(std::floor(dstX)), 0);
        const int startY = (std::max)(static_cast<int>(std::floor(dstY)), 0);
        const int endX = (std::min)(static_cast<int>(std::ceil(dstX + dstW)), outWidth);
        const int endY = (std::min)(static_cast<int>(std::ceil(dstY + dstH)), outHeight);
        if (startX >= endX || startY >= endY)
        {
            continue;
        }

        const int srcW = importedAsset.alphaMaskWidth;
        const int srcH = importedAsset.alphaMaskHeight;
        const bool useMultiSample = (dstW < 3.0f || dstH < 3.0f);

        auto sampleAlphaAt = [&](float samplePixelX, float samplePixelY) -> Uint8 {
            const float u = (samplePixelX - dstX) / dstW;
            const float v = (samplePixelY - dstY) / dstH;
            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
            {
                return static_cast<Uint8>(0);
            }

            const int srcX = std::clamp(
                static_cast<int>(std::floor(u * static_cast<float>(srcW))),
                0,
                srcW - 1);
            const int srcY = std::clamp(
                static_cast<int>(std::floor(v * static_cast<float>(srcH))),
                0,
                srcH - 1);
            return importedAsset.alphaMask[
                static_cast<size_t>(srcY * srcW) + static_cast<size_t>(srcX)];
        };

        for (int y = startY; y < endY; ++y)
        {
            const size_t dstRowOffset = static_cast<size_t>(y * outWidth);

            for (int x = startX; x < endX; ++x)
            {
                Uint8 maxAlpha = sampleAlphaAt(
                    static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f);
                if (useMultiSample && maxAlpha <= alphaThreshold)
                {
                    constexpr float kSampleOffsets[3] = {0.17f, 0.50f, 0.83f};
                    for (float oy : kSampleOffsets)
                    {
                        for (float ox : kSampleOffsets)
                        {
                            const Uint8 sampleAlpha = sampleAlphaAt(
                                static_cast<float>(x) + ox,
                                static_cast<float>(y) + oy);
                            maxAlpha = (std::max)(maxAlpha, sampleAlpha);
                            if (maxAlpha > alphaThreshold)
                            {
                                break;
                            }
                        }
                        if (maxAlpha > alphaThreshold)
                        {
                            break;
                        }
                    }
                }

                if (maxAlpha <= alphaThreshold)
                {
                    continue;
                }

                landMask[dstRowOffset + static_cast<size_t>(x)] = 255;
            }
        }
    }

    // Important: la minimap (ecran + export PNG) represente uniquement
    // la geometrie visuelle des assets/iles. Les tuiles de collision ne doivent
    // pas "peindre" la terre dans la minimap.

    // Masque source "verite terrain" avant stylisation.
    // On l'utilise pour proteger les poches d'eau internes des iles.
    const std::vector<Uint8> sourceLandMask = landMask;
    std::vector<Uint8> enclosedWaterMask(
        static_cast<size_t>(outWidth * outHeight),
        static_cast<Uint8>(0));

    if (outWidth >= 3 && outHeight >= 3)
    {
        std::vector<Uint8> visitedWater(
            static_cast<size_t>(outWidth * outHeight),
            static_cast<Uint8>(0));
        std::vector<int> floodStack;
        floodStack.reserve(static_cast<size_t>(outWidth * outHeight));

        auto pushExteriorWater = [&](int x, int y) {
            if (x < 0 || x >= outWidth || y < 0 || y >= outHeight)
            {
                return;
            }

            const size_t index = static_cast<size_t>((y * outWidth) + x);
            if (visitedWater[index] != 0 || sourceLandMask[index] != 0)
            {
                return;
            }

            visitedWater[index] = 1;
            floodStack.push_back(static_cast<int>(index));
        };

        // Flood fill depuis le bord: eau connectee au bord = mer exterieure.
        for (int x = 0; x < outWidth; ++x)
        {
            pushExteriorWater(x, 0);
            pushExteriorWater(x, outHeight - 1);
        }
        for (int y = 0; y < outHeight; ++y)
        {
            pushExteriorWater(0, y);
            pushExteriorWater(outWidth - 1, y);
        }

        while (!floodStack.empty())
        {
            const int packedIndex = floodStack.back();
            floodStack.pop_back();

            const int x = packedIndex % outWidth;
            const int y = packedIndex / outWidth;
            pushExteriorWater(x - 1, y);
            pushExteriorWater(x + 1, y);
            pushExteriorWater(x, y - 1);
            pushExteriorWater(x, y + 1);
        }

        // Eau non connectee au bord => poche interne a conserver en eau.
        for (int y = 1; y < outHeight - 1; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y * outWidth);
            for (int x = 1; x < outWidth - 1; ++x)
            {
                const size_t index = rowOffset + static_cast<size_t>(x);
                if (sourceLandMask[index] == 0 && visitedWater[index] == 0)
                {
                    enclosedWaterMask[index] = 255;
                }
            }
        }
    }

    auto dilateMask = [outWidth, outHeight](const std::vector<Uint8>& srcMask, int radius) -> std::vector<Uint8> {
        std::vector<Uint8> dstMask(
            static_cast<size_t>(outWidth * outHeight),
            static_cast<Uint8>(0));
        for (int y = 0; y < outHeight; ++y)
        {
            for (int x = 0; x < outWidth; ++x)
            {
                bool found = false;
                for (int oy = -radius; oy <= radius && !found; ++oy)
                {
                    const int sy = y + oy;
                    if (sy < 0 || sy >= outHeight)
                    {
                        continue;
                    }
                    const size_t srcRow = static_cast<size_t>(sy * outWidth);
                    for (int ox = -radius; ox <= radius; ++ox)
                    {
                        const int sx = x + ox;
                        if (sx < 0 || sx >= outWidth)
                        {
                            continue;
                        }
                        if (srcMask[srcRow + static_cast<size_t>(sx)] != 0)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                if (found)
                {
                    dstMask[static_cast<size_t>((y * outWidth) + x)] = 255;
                }
            }
        }
        return dstMask;
    };

    // 1) Epaissit un peu les terres pour une lecture "Seafight".
    const int baseSize = (std::min)(outWidth, outHeight);
    const int growIterations = (std::max)(1, baseSize / 220);
    std::vector<Uint8> grownMask = landMask;
    for (int i = 0; i < growIterations; ++i)
    {
        grownMask = dilateMask(grownMask, 1);
    }

    // 2) Pixelisation controlee en blocs pour le style minimap.
    const int blockSize = (std::max)(
        1,
        static_cast<int>(std::floor(static_cast<float>(baseSize) / 140.0f)));
    std::vector<Uint8> stylizedMask = grownMask;
    if (blockSize > 1)
    {
        stylizedMask.assign(
            static_cast<size_t>(outWidth * outHeight),
            static_cast<Uint8>(0));

        for (int by = 0; by < outHeight; by += blockSize)
        {
            for (int bx = 0; bx < outWidth; bx += blockSize)
            {
                const int ex = (std::min)(bx + blockSize, outWidth);
                const int ey = (std::min)(by + blockSize, outHeight);
                int sampleCount = 0;
                int landCount = 0;
                for (int y = by; y < ey; ++y)
                {
                    const size_t rowOffset = static_cast<size_t>(y * outWidth);
                    for (int x = bx; x < ex; ++x)
                    {
                        sampleCount += 1;
                        if (grownMask[rowOffset + static_cast<size_t>(x)] != 0)
                        {
                            landCount += 1;
                        }
                    }
                }

                const bool isLandBlock =
                    (sampleCount > 0) &&
                    (landCount * 100 >= sampleCount * 12);
                const Uint8 blockValue = isLandBlock ? static_cast<Uint8>(255) : static_cast<Uint8>(0);

                for (int y = by; y < ey; ++y)
                {
                    const size_t rowOffset = static_cast<size_t>(y * outWidth);
                    for (int x = bx; x < ex; ++x)
                    {
                        stylizedMask[rowOffset + static_cast<size_t>(x)] = blockValue;
                    }
                }
            }
        }
    }

    // 3) Petit filtrage majoritaire pour reduire les trous/points parasites.
    if (outWidth >= 3 && outHeight >= 3)
    {
        std::vector<Uint8> filteredMask = stylizedMask;
        for (int y = 1; y < outHeight - 1; ++y)
        {
            for (int x = 1; x < outWidth - 1; ++x)
            {
                int aroundLand = 0;
                for (int oy = -1; oy <= 1; ++oy)
                {
                    const size_t rowOffset = static_cast<size_t>((y + oy) * outWidth);
                    for (int ox = -1; ox <= 1; ++ox)
                    {
                        if (stylizedMask[rowOffset + static_cast<size_t>(x + ox)] != 0)
                        {
                            aroundLand += 1;
                        }
                    }
                }

                const size_t index = static_cast<size_t>((y * outWidth) + x);
                if (stylizedMask[index] == 0)
                {
                    if (aroundLand >= 5)
                    {
                        filteredMask[index] = 255;
                    }
                }
                else
                {
                    if (aroundLand <= 2)
                    {
                        filteredMask[index] = 0;
                    }
                }
            }
        }
        stylizedMask.swap(filteredMask);
    }

    // Protection finale des poches d'eau internes: on evite de les "remplir"
    // en marron par les etapes de stylisation.
    for (size_t index = 0; index < stylizedMask.size(); ++index)
    {
        if (enclosedWaterMask[index] != 0)
        {
            stylizedMask[index] = 0;
        }
    }

    const std::vector<Uint8> outlineMask = dilateMask(stylizedMask, 1);

    // Palette "flat" type Seafight: pas de texture de vagues/bruit.
    const Uint32 waterColor = SDL_MapSurfaceRGBA(targetSurface, 28, 63, 103, 255);
    const Uint32 landColor = SDL_MapSurfaceRGBA(targetSurface, 170, 126, 67, 255);
    const Uint32 coastColor = SDL_MapSurfaceRGBA(targetSurface, 114, 84, 46, 255);

    const bool lockOk = SDL_LockSurface(targetSurface);
    if (!lockOk)
    {
        return false;
    }

    Uint32* pixels = static_cast<Uint32*>(targetSurface->pixels);
    const int pitchPixels = targetSurface->pitch / static_cast<int>(sizeof(Uint32));
    for (int y = 0; y < outHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y * outWidth);
        for (int x = 0; x < outWidth; ++x)
        {
            const size_t index = rowOffset + static_cast<size_t>(x);
            const bool isLand = (stylizedMask[index] != 0);
            const bool isOutline = (!isLand && outlineMask[index] != 0);
            if (isLand)
            {
                pixels[(y * pitchPixels) + x] = landColor;
            }
            else if (isOutline)
            {
                pixels[(y * pitchPixels) + x] = coastColor;
            }
            else
            {
                pixels[(y * pitchPixels) + x] = waterColor;
            }
        }
    }

    SDL_UnlockSurface(targetSurface);
    return true;
}

bool EditorMapCreateMapScene::exportMiniMapPngFromJsonPath(
    const char* jsonAbsolutePath,
    std::string* outPngAbsolutePath) const
{
    if (outPngAbsolutePath != nullptr)
    {
        outPngAbsolutePath->clear();
    }

    const std::string pngAbsolutePath = buildMiniMapPngPathFromJsonPath(jsonAbsolutePath);
    if (pngAbsolutePath.empty())
    {
        return false;
    }

    const float miniMapSize = computeEditorMiniMapSize(GetCurrentMap().rect);
    const int miniMapWidthPx = miniMapRectDimensionToPixels(miniMapSize);
    const int miniMapHeightPx = miniMapRectDimensionToPixels(miniMapSize);

    SDL_Surface* miniMapSurface = SDL_CreateSurface(
        miniMapWidthPx,
        miniMapHeightPx,
        SDL_PIXELFORMAT_RGBA32);
    if (miniMapSurface == nullptr)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "EditorMapCreateMapScene: SDL_CreateSurface minimap export KO: %s",
            SDL_GetError());
        return false;
    }

    const bool renderOk = this->renderStyledMiniMapToSurface(miniMapSurface);
    const bool saveOk = renderOk && SDL_SavePNG(miniMapSurface, pngAbsolutePath.c_str());
    SDL_DestroySurface(miniMapSurface);

    if (!saveOk)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "EditorMapCreateMapScene: export minimap PNG KO '%s': %s",
            pngAbsolutePath.c_str(),
            SDL_GetError());
        return false;
    }

    if (outPngAbsolutePath != nullptr)
    {
        *outPngAbsolutePath = pngAbsolutePath;
    }

    return true;
}

bool EditorMapCreateMapScene::exportMapToAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Export annule.";
        return false;
    }
    if (trimAscii(this->mapNameInput).empty())
    {
        this->statusMessage = "Nom de map requis avant export.";
        return false;
    }

    const Map& map = GetCurrentMap();
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON.";
        return false;
    }

    cJSON_AddStringToObject(root, "mapName", trimAscii(this->mapNameInput).c_str());
    cJSON_AddStringToObject(root, "oceanColor", kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label);
    cJSON_AddNumberToObject(root, "worldWidthTiles", map.getWidthTiles());
    cJSON_AddNumberToObject(root, "worldHeightTiles", map.getHeightTiles());
    cJSON* blockedTilesArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "blockedTiles", blockedTilesArray);
    for (int tileY = 0; tileY < map.getHeightTiles(); ++tileY)
    {
        for (int tileX = 0; tileX < map.getWidthTiles(); ++tileX)
        {
            if (!map.isTileBlocked(tileX, tileY))
            {
                continue;
            }

            cJSON* blockedTileItem = cJSON_CreateObject();
            cJSON_AddNumberToObject(blockedTileItem, "x", tileX);
            cJSON_AddNumberToObject(blockedTileItem, "y", tileY);
            cJSON_AddItemToArray(blockedTilesArray, blockedTileItem);
        }
    }
    cJSON* placedAssetsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "placedAssets", placedAssetsArray);
    for (const PlacedAsset& placedAsset : this->placedAssets)
    {
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];
        cJSON* placedItem = cJSON_CreateObject();
        const std::string runtimeStoragePath =
            buildRuntimeAssetPathFromSource(importedAsset.sourcePath, importedAsset.displayName);
        cJSON_AddStringToObject(placedItem, "storagePath", runtimeStoragePath.c_str());
        cJSON_AddNumberToObject(placedItem, "tileX", placedAsset.tileX);
        cJSON_AddNumberToObject(placedItem, "tileY", placedAsset.tileY);
        if (placedAsset.clickGuiTarget != EditorMapAssetClickGuiTarget::NONE)
        {
            const EditorMapAssetClickGuiTargetInfo& guiInfo =
                GetEditorMapAssetClickGuiTargetInfo(placedAsset.clickGuiTarget);
            cJSON* clickGuiItem = cJSON_CreateObject();
            cJSON_AddStringToObject(clickGuiItem, "window", guiInfo.jsonId);
            cJSON* coveredTilesArray = cJSON_CreateArray();
            const std::vector<SDL_Point> coveredTiles =
                this->computePlacedAssetClickInteractionTiles(placedAsset);
            for (const SDL_Point& coveredTile : coveredTiles)
            {
                cJSON* coveredTileItem = cJSON_CreateObject();
                cJSON_AddNumberToObject(coveredTileItem, "tileX", coveredTile.x);
                cJSON_AddNumberToObject(coveredTileItem, "tileY", coveredTile.y);
                cJSON_AddItemToArray(coveredTilesArray, coveredTileItem);
            }
            cJSON_AddItemToObject(clickGuiItem, "tiles", coveredTilesArray);
            cJSON_AddItemToObject(placedItem, "clickGui", clickGuiItem);
        }
        cJSON_AddItemToArray(placedAssetsArray, placedItem);
    }
    cJSON* towerHotspotsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "placedTowers", towerHotspotsArray);
    for (const TowerHotspot& hotspot : this->towerHotspots)
    {
        cJSON* hotspotItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(hotspotItem, "tileX", hotspot.tileX);
        cJSON_AddNumberToObject(hotspotItem, "tileY", hotspot.tileY);
        cJSON_AddNumberToObject(
            hotspotItem,
            "towerNumber",
            ClampEditorMapTowerHotspotNumber(hotspot.towerNumber));
        const int visualSetIndex = this->findTowerVisualSetIndexByNumber(hotspot.towerNumber);
        if (visualSetIndex >= 0)
        {
            const TowerVisualSet& visualSet =
                this->towerVisualSets[static_cast<size_t>(visualSetIndex)];
            cJSON* assetVariantsItem = cJSON_CreateObject();
            bool hasAnyVariant = false;
            for (int levelIndex = 0; levelIndex < 4; ++levelIndex)
            {
                const int importedAssetIndex =
                    visualSet.importedAssetIndices[static_cast<size_t>(levelIndex)];
                if (importedAssetIndex < 0 ||
                    importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
                {
                    continue;
                }

                const ImportedAsset& importedAsset =
                    this->importedAssets[static_cast<size_t>(importedAssetIndex)];
                const std::string runtimeStoragePath =
                    buildRuntimeAssetPathFromSource(importedAsset.sourcePath, importedAsset.displayName);
                const std::string variantKey = "lvl" + std::to_string(levelIndex + 1);
                cJSON_AddStringToObject(
                    assetVariantsItem,
                    variantKey.c_str(),
                    runtimeStoragePath.c_str());
                hasAnyVariant = true;
            }

            if (hasAnyVariant)
            {
                cJSON_AddItemToObject(hotspotItem, "assetVariants", assetVariantsItem);
            }
            else
            {
                cJSON_Delete(assetVariantsItem);
            }
        }
        cJSON_AddItemToArray(towerHotspotsArray, hotspotItem);
    }

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);

    if (jsonText == nullptr)
    {
        this->statusMessage = "Echec serialisation JSON.";
        return false;
    }

    std::ofstream output(absolutePath, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        cJSON_free(jsonText);
        this->statusMessage = "Impossible d'ouvrir le fichier.";
        return false;
    }

    output.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
    const bool writeOk = output.good();
    output.close();
    cJSON_free(jsonText);

    if (!writeOk)
    {
        this->statusMessage = "Echec ecriture JSON.";
        return false;
    }

    std::string miniMapPngPath;
    const bool miniMapExportOk =
        this->exportMiniMapPngFromJsonPath(absolutePath, &miniMapPngPath);

    if (miniMapExportOk)
    {
        this->statusMessage =
            std::string("Map exportee: ") + absolutePath +
            " | Minimap PNG: " + miniMapPngPath;
    }
    else
    {
        this->statusMessage =
            std::string("Map exportee (PNG minimap en echec): ") + absolutePath;
    }

    return true;
}

void EditorMapCreateMapScene::openImportAssetDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kImportFilters;
    options.num_filters = static_cast<int>(std::size(kImportFilters));
    options.default_location = nullptr;
    options.allow_many = true;
    options.title = "Importer un asset map";
    options.accept_label = "Importer";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFile(&EditorMapCreateMapScene::onImportAssetDialogResult, this, &options);
}


void EditorMapCreateMapScene::openImportMapDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kMapImportFilters;
    options.num_filters = static_cast<int>(std::size(kMapImportFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Importer map.json";
    options.accept_label = "Importer";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFile(&EditorMapCreateMapScene::onImportMapDialogResult, this, &options);
}

void EditorMapCreateMapScene::openExportMapDialog(void)
{
    if (trimAscii(this->mapNameInput).empty())
    {
        this->statusMessage = "Export impossible: ajoute d'abord un nom de map.";
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_WARNING,
            "Export map impossible",
            "Ajoute d'abord un nom dans l'input Map Name avant d'exporter la map.",
            rc2d_window_getWindow());
        return;
    }

    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kShipFolderFilters;
    options.num_filters = static_cast<int>(std::size(kShipFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Choisir dossier d'export";
    options.accept_label = "Exporter";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapCreateMapScene::onExportMapDialogResult, this, &options);
}

bool EditorMapCreateMapScene::exportMapToFolder(const char* absoluteFolderPath)
{
    if (absoluteFolderPath == nullptr || absoluteFolderPath[0] == '\0')
    {
        this->statusMessage = "Export annule.";
        return false;
    }

    std::filesystem::path folder(absoluteFolderPath);
    std::error_code fsError;
    if (!std::filesystem::exists(folder, fsError))
    {
        std::filesystem::create_directories(folder, fsError);
    }
    if (fsError || !std::filesystem::is_directory(folder, fsError))
    {
        this->statusMessage = "Dossier d'export invalide.";
        return false;
    }

    const std::filesystem::path mapJsonPath = folder / "map.json";
    if (!this->exportMapToAbsolutePath(mapJsonPath.string().c_str()))
    {
        return false;
    }
    this->statusMessage =
        std::string("Export dossier OK: ") + folder.string() + " (map.json + map_minimap.png)";
    return true;
}

void EditorMapCreateMapScene::openImportShipFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kShipFolderFilters;
    options.num_filters = static_cast<int>(std::size(kShipFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier racine des navires (scan recursif)";
    options.accept_label = "Ouvrir";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapCreateMapScene::onImportShipFolderDialogResult, this, &options);
}

void EditorMapCreateMapScene::processPendingShipFolderRequest(void)
{
    bool hasResult = false;
    bool isCanceled = false;
    std::string selectedFolder;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        hasResult = this->pendingShipFolderDialogCompleted;
        if (hasResult)
        {
            isCanceled = this->pendingShipFolderDialogCanceled;
            selectedFolder.swap(this->pendingShipFolderAbsolute);
            this->pendingShipFolderDialogCompleted = false;
            this->pendingShipFolderDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }

    if (isCanceled || selectedFolder.empty())
    {
        this->statusMessage = "Import navires annule.";
        return;
    }

    this->importShipsFromRootFolderAbsolutePath(selectedFolder.c_str());
}

bool EditorMapCreateMapScene::importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath)
{
    if (rootFolderAbsolutePath == nullptr || rootFolderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier navires invalide.";
        return false;
    }

    std::filesystem::path rootPath(rootFolderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(rootPath, fsError) ||
        !std::filesystem::is_directory(rootPath, fsError))
    {
        this->statusMessage = "Dossier navires introuvable.";
        return false;
    }

    auto isShipAtlasFolder = [](const std::filesystem::path& folderPath) -> bool {
        std::error_code localError;
        if (!std::filesystem::exists(folderPath, localError) ||
            !std::filesystem::is_directory(folderPath, localError))
        {
            return false;
        }

        for (int i = 1; i <= kShipSpriteCount; ++i)
        {
            const std::filesystem::path pngPath = folderPath / (std::to_string(i) + ".png");
            if (!std::filesystem::exists(pngPath, localError) ||
                !std::filesystem::is_regular_file(pngPath, localError))
            {
                return false;
            }
        }

        return true;
    };

    std::vector<std::filesystem::path> discoveredFolders;
    if (isShipAtlasFolder(rootPath))
    {
        discoveredFolders.push_back(rootPath);
    }

    std::filesystem::recursive_directory_iterator it(
        rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        fsError);
    if (!fsError)
    {
        std::filesystem::recursive_directory_iterator end;
        while (it != end)
        {
            std::error_code entryError;
            if (it->is_directory(entryError) && !entryError)
            {
                const std::filesystem::path folderPath = it->path();
                if (isShipAtlasFolder(folderPath))
                {
                    discoveredFolders.push_back(folderPath);
                }
            }

            it.increment(fsError);
            if (fsError)
            {
                fsError.clear();
            }
        }
    }

    if (discoveredFolders.empty())
    {
        this->statusMessage = "Aucun dossier navire valide trouve (1.png..8.png).";
        return false;
    }

    std::sort(
        discoveredFolders.begin(),
        discoveredFolders.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return normalizePathSlashes(a.string()) < normalizePathSlashes(b.string());
        });

    auto makePathKey = [](const std::string& absolutePath) -> std::string {
        std::string key = normalizePathSlashes(absolutePath);
        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return key;
    };

    std::vector<std::string> knownPathKeys;
    knownPathKeys.reserve(this->importedShips.size() + discoveredFolders.size());
    for (const ImportedShip& ship : this->importedShips)
    {
        knownPathKeys.push_back(makePathKey(ship.folderAbsolutePath));
    }

    int firstAddedIndex = -1;
    int addedCount = 0;
    for (const std::filesystem::path& folderPath : discoveredFolders)
    {
        std::error_code absError;
        std::filesystem::path absolutePath = std::filesystem::absolute(folderPath, absError);
        if (absError)
        {
            absolutePath = folderPath;
        }

        const std::string normalizedAbsolutePath = normalizePathSlashes(absolutePath.string());
        const std::string pathKey = makePathKey(normalizedAbsolutePath);
        if (std::find(knownPathKeys.begin(), knownPathKeys.end(), pathKey) != knownPathKeys.end())
        {
            continue;
        }

        knownPathKeys.push_back(pathKey);

        ImportedShip importedShip{};
        importedShip.folderAbsolutePath = normalizedAbsolutePath;
        importedShip.displayName = folderPath.filename().string();
        if (importedShip.displayName.empty())
        {
            importedShip.displayName = importedShip.folderAbsolutePath;
        }

        this->importedShips.push_back(importedShip);
        const int newIndex = static_cast<int>(this->importedShips.size()) - 1;
        if (firstAddedIndex < 0)
        {
            firstAddedIndex = newIndex;
        }
        addedCount += 1;
    }

    if (addedCount <= 0)
    {
        this->statusMessage = "Aucun nouveau navire importe (deja presents).";
        return false;
    }

    const bool hasValidSelection =
        this->selectedShipIndex >= 0 &&
        this->selectedShipIndex < static_cast<int>(this->importedShips.size());

    // Premier import (ou selection invalide): charge automatiquement le premier navire ajoute.
    if ((!hasValidSelection || !this->testShipLoaded) && firstAddedIndex >= 0)
    {
        if (!this->selectImportedShipAtIndex(firstAddedIndex))
        {
            return false;
        }

        const ImportedShip& activeShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
        this->statusMessage =
            std::to_string(addedCount) + " navire(s) importe(s). Navire actif: " + activeShip.displayName;
        return true;
    }

    this->statusMessage = std::to_string(addedCount) + " navire(s) importe(s).";
    return true;
}

void EditorMapCreateMapScene::processPendingMapImportRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string mapPath;
    {
        std::lock_guard<std::mutex> lock(this->pendingMapImportMutex);
        hasResult = this->pendingMapImportDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingMapImportDialogCanceled;
            mapPath.swap(this->pendingMapImportAbsolutePath);
            this->pendingMapImportDialogCompleted = false;
            this->pendingMapImportDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || mapPath.empty())
    {
        this->statusMessage = "Import map annule.";
        return;
    }

    this->importMapFromAbsolutePath(mapPath.c_str());
}

bool EditorMapCreateMapScene::importMapFromAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Fichier map invalide.";
        return false;
    }

    std::ifstream input(absolutePath, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        this->statusMessage = "Lecture map impossible.";
        return false;
    }
    const std::streamsize size = input.tellg();
    if (size <= 0)
    {
        this->statusMessage = "Map JSON vide.";
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(size) + 1U, '\0');
    if (!input.read(bytes.data(), size))
    {
        this->statusMessage = "Lecture map JSON echouee.";
        return false;
    }

    cJSON* root = cJSON_Parse(bytes.data());
    if (root == nullptr)
    {
        this->statusMessage = "JSON map invalide.";
        return false;
    }

    Map& map = GetCurrentMap();
    map.clearBlockedTiles();
    this->placedAssets.clear();
    this->towerHotspots.clear();
    this->towerVisualSets.clear();
    this->historyActions.clear();
    this->historyCursor = 0;

    const cJSON* mapName = cJSON_GetObjectItemCaseSensitive(root, "mapName");
    if (cJSON_IsString(mapName) && mapName->valuestring != nullptr)
    {
        this->mapNameInput = mapName->valuestring;
    }

    std::string importStatusSuffix;
    bool normalizedTowerHotspots = false;
    bool skippedTowerHotspots = false;
    bool normalizedAssetClickGui = false;
    bool shouldApplyOceanColor = false;
    const cJSON* oceanColor = cJSON_GetObjectItemCaseSensitive(root, "oceanColor");
    if (cJSON_IsString(oceanColor) && oceanColor->valuestring != nullptr)
    {
        const int oceanColorIndex = findOceanColorIndexByLabel(oceanColor->valuestring);
        if (oceanColorIndex >= 0)
        {
            this->selectedOceanColorIndex = oceanColorIndex;
            this->pendingOceanColorDelta = 0;
            shouldApplyOceanColor = true;
        }
        else
        {
            RC2D_log(
                RC2D_LOG_WARN,
                "EditorMapCreateMapScene: oceanColor JSON inconnu '%s'",
                oceanColor->valuestring);
            importStatusSuffix = " Ocean JSON inconnu, couleur courante conservee.";
        }
    }

    const cJSON* opacityPercent = cJSON_GetObjectItemCaseSensitive(root, "assetOpacityPercent");
    if (cJSON_IsNumber(opacityPercent))
    {
        this->setAssetOpacityPercent(static_cast<int>(std::lround(opacityPercent->valuedouble)));
    }
    const cJSON* transparencyEnabled = cJSON_GetObjectItemCaseSensitive(root, "assetTransparencyEnabled");
    if (cJSON_IsBool(transparencyEnabled))
    {
        this->assetTransparencyEnabled = cJSON_IsTrue(transparencyEnabled);
    }
    const cJSON* brushRadius = cJSON_GetObjectItemCaseSensitive(root, "blockedBrushRadiusTiles");
    if (cJSON_IsNumber(brushRadius))
    {
        this->blockedBrushRadiusTiles = std::clamp(static_cast<int>(std::lround(brushRadius->valuedouble)), 0, 8);
    }
    const cJSON* blockedColorIndex = cJSON_GetObjectItemCaseSensitive(root, "blockedColorIndex");
    if (cJSON_IsNumber(blockedColorIndex))
    {
        this->selectedBlockedColorIndex = std::clamp(
            static_cast<int>(std::lround(blockedColorIndex->valuedouble)),
            0,
            static_cast<int>(kBlockedTilePalette.size()) - 1);
    }
    const cJSON* hotspotColorIndex = cJSON_GetObjectItemCaseSensitive(root, "hotspotColorIndex");
    if (cJSON_IsNumber(hotspotColorIndex))
    {
        this->selectedHotspotColorIndex = std::clamp(
            static_cast<int>(std::lround(hotspotColorIndex->valuedouble)),
            0,
            static_cast<int>(kHotspotPalette.size()) - 1);
    }

    const cJSON* blockedTiles = cJSON_GetObjectItemCaseSensitive(root, "blockedTiles");
    if (cJSON_IsArray(blockedTiles))
    {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, blockedTiles)
        {
            const cJSON* x = cJSON_GetObjectItemCaseSensitive(item, "x");
            const cJSON* y = cJSON_GetObjectItemCaseSensitive(item, "y");
            if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y))
            {
                continue;
            }
            map.setTileBlocked(static_cast<int>(std::lround(x->valuedouble)), static_cast<int>(std::lround(y->valuedouble)), true);
        }
    }

    const cJSON* placedAssetsJson = cJSON_GetObjectItemCaseSensitive(root, "placedAssets");
    if (cJSON_IsArray(placedAssetsJson))
    {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, placedAssetsJson)
        {
            const cJSON* storagePath = cJSON_GetObjectItemCaseSensitive(item, "storagePath");
            const cJSON* tileX = cJSON_GetObjectItemCaseSensitive(item, "tileX");
            const cJSON* tileY = cJSON_GetObjectItemCaseSensitive(item, "tileY");
            if (!cJSON_IsString(storagePath) || storagePath->valuestring == nullptr ||
                !cJSON_IsNumber(tileX) || !cJSON_IsNumber(tileY))
            {
                continue;
            }

            const int importedIndex = this->importAssetFromRuntimeStoragePath(storagePath->valuestring);
            if (importedIndex < 0)
            {
                continue;
            }

            PlacedAsset placed{};
            placed.importedAssetIndex = importedIndex;
            placed.tileX = static_cast<int>(std::lround(tileX->valuedouble));
            placed.tileY = static_cast<int>(std::lround(tileY->valuedouble));
            placed.anchorTileX = static_cast<float>(placed.tileX);
            placed.anchorTileY = static_cast<float>(placed.tileY);
            placed.scale = 1.0f;
            const cJSON* clickGui = cJSON_GetObjectItemCaseSensitive(item, "clickGui");
            if (cJSON_IsObject(clickGui))
            {
                const cJSON* window = cJSON_GetObjectItemCaseSensitive(clickGui, "window");
                const cJSON* maxShipDistanceTiles =
                    cJSON_GetObjectItemCaseSensitive(clickGui, "maxShipDistanceTiles");
                const cJSON* tiles = cJSON_GetObjectItemCaseSensitive(clickGui, "tiles");
                if (cJSON_IsString(window) && window->valuestring != nullptr)
                {
                    EditorMapAssetClickGuiTarget clickGuiTarget = EditorMapAssetClickGuiTarget::NONE;
                    if (TryParseEditorMapAssetClickGuiTarget(window->valuestring, &clickGuiTarget))
                    {
                        placed.clickGuiTarget = clickGuiTarget;
                    }
                    else
                    {
                        normalizedAssetClickGui = true;
                    }
                }
                if (cJSON_IsNumber(maxShipDistanceTiles))
                {
                    placed.clickGuiDistanceTiles = ClampEditorMapAssetClickDistanceTiles(
                        static_cast<int>(std::lround(maxShipDistanceTiles->valuedouble)));
                    placed.clickGuiUsesLegacyRadius = true;
                }
                else if (cJSON_IsArray(tiles))
                {
                    cJSON* tileItem = nullptr;
                    cJSON_ArrayForEach(tileItem, tiles)
                    {
                        const cJSON* allowedTileX =
                            cJSON_GetObjectItemCaseSensitive(tileItem, "tileX");
                        const cJSON* allowedTileY =
                            cJSON_GetObjectItemCaseSensitive(tileItem, "tileY");
                        if (!cJSON_IsNumber(allowedTileX) || !cJSON_IsNumber(allowedTileY))
                        {
                            normalizedAssetClickGui = true;
                            continue;
                        }

                        const int parsedTileX =
                            static_cast<int>(std::lround(allowedTileX->valuedouble));
                        const int parsedTileY =
                            static_cast<int>(std::lround(allowedTileY->valuedouble));
                        if (std::find_if(
                                placed.clickGuiTiles.begin(),
                                placed.clickGuiTiles.end(),
                                [parsedTileX, parsedTileY](const SDL_Point& tile) {
                                    return tile.x == parsedTileX && tile.y == parsedTileY;
                                }) != placed.clickGuiTiles.end())
                        {
                            continue;
                        }

                        placed.clickGuiTiles.push_back(SDL_Point{parsedTileX, parsedTileY});
                    }
                    placed.clickGuiUsesLegacyRadius = false;
                }
            }
            this->placedAssets.push_back(placed);
        }
    }

    const cJSON* towerHotspotsJson = cJSON_GetObjectItemCaseSensitive(root, "placedTowers");
    if (cJSON_IsArray(towerHotspotsJson))
    {
        std::array<bool, static_cast<size_t>(kEditorMapMaxTowerHotspots + 1)> usedTowerNumbers{};
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, towerHotspotsJson)
        {
            const cJSON* tileX = cJSON_GetObjectItemCaseSensitive(item, "tileX");
            const cJSON* tileY = cJSON_GetObjectItemCaseSensitive(item, "tileY");
            const cJSON* assetVariantsJson =
                cJSON_GetObjectItemCaseSensitive(item, "assetVariants");
            if (!cJSON_IsNumber(tileX) || !cJSON_IsNumber(tileY))
            {
                continue;
            }

            const int hotspotTileX = static_cast<int>(std::lround(tileX->valuedouble));
            const int hotspotTileY = static_cast<int>(std::lround(tileY->valuedouble));
            if (this->findTowerHotspotIndexAtTile(hotspotTileX, hotspotTileY) >= 0)
            {
                skippedTowerHotspots = true;
                continue;
            }

            int towerNumber = 0;
            const cJSON* towerNumberJson = cJSON_GetObjectItemCaseSensitive(item, "towerNumber");
            if (cJSON_IsNumber(towerNumberJson))
            {
                const int parsedTowerNumber =
                    static_cast<int>(std::lround(towerNumberJson->valuedouble));
                if (parsedTowerNumber >= 1 && parsedTowerNumber <= kEditorMapMaxTowerHotspots)
                {
                    towerNumber = parsedTowerNumber;
                }
                else
                {
                    normalizedTowerHotspots = true;
                }
            }
            else
            {
                normalizedTowerHotspots = true;
            }

            if (towerNumber <= 0 ||
                usedTowerNumbers[static_cast<size_t>(towerNumber)])
            {
                towerNumber = 0;
                for (int candidate = 1; candidate <= kEditorMapMaxTowerHotspots; ++candidate)
                {
                    if (!usedTowerNumbers[static_cast<size_t>(candidate)])
                    {
                        towerNumber = candidate;
                        break;
                    }
                }
                normalizedTowerHotspots = true;
            }

            if (towerNumber <= 0)
            {
                skippedTowerHotspots = true;
                continue;
            }

            usedTowerNumbers[static_cast<size_t>(towerNumber)] = true;
            this->towerHotspots.push_back(
                TowerHotspot{
                    hotspotTileX,
                    hotspotTileY,
                    towerNumber});

            if (cJSON_IsObject(assetVariantsJson))
            {
                TowerVisualSet& visualSet = this->ensureTowerVisualSet(towerNumber);
                for (int levelIndex = 0; levelIndex < 4; ++levelIndex)
                {
                    const std::string variantKey = "lvl" + std::to_string(levelIndex + 1);
                    const cJSON* storagePathJson =
                        cJSON_GetObjectItemCaseSensitive(assetVariantsJson, variantKey.c_str());
                    if (!cJSON_IsString(storagePathJson) || storagePathJson->valuestring == nullptr)
                    {
                        continue;
                    }

                    const int importedIndex =
                        this->importAssetFromRuntimeStoragePath(storagePathJson->valuestring);
                    if (importedIndex < 0)
                    {
                        continue;
                    }

                    visualSet.importedAssetIndices[static_cast<size_t>(levelIndex)] = importedIndex;
                }
            }
        }

        if (!this->towerVisualSets.empty())
        {
            for (const TowerHotspot& hotspot : this->towerHotspots)
            {
                (void)this->ensureTowerVisualSet(hotspot.towerNumber);
            }
        }
    }

    cJSON_Delete(root);

    if (shouldApplyOceanColor)
    {
        const OceanColorEntry& entry = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)];
        this->applySelectedOceanColor();
        importStatusSuffix = " Ocean: " + std::string(entry.label) + ".";
    }
    if (normalizedTowerHotspots)
    {
        importStatusSuffix += " Hotspots tours renumerotes.";
    }
    if (skippedTowerHotspots)
    {
        importStatusSuffix += " Hotspots tours en trop/dupliques ignores.";
    }
    if (normalizedAssetClickGui)
    {
        importStatusSuffix += " Certaines GUI de clic sont inconnues et ignorees.";
    }

    this->statusMessage = "Map importee depuis JSON." + importStatusSuffix;
    return true;
}

bool EditorMapCreateMapScene::loadShipFolderFromAbsolutePath(const char* folderAbsolutePath)
{
    if (folderAbsolutePath == nullptr || folderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier navire invalide.";
        return false;
    }

    std::filesystem::path folderPath(folderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(folderPath, fsError) ||
        !std::filesystem::is_directory(folderPath, fsError))
    {
        this->statusMessage = "Dossier navire introuvable.";
        return false;
    }

    std::error_code absError;
    std::filesystem::path absoluteFolderPath = std::filesystem::absolute(folderPath, absError);
    if (absError)
    {
        absoluteFolderPath = folderPath;
    }
    const std::string normalizedAbsoluteFolderPath = normalizePathSlashes(absoluteFolderPath.string());
    std::string titleStorageFolderPath;
    const bool useTitleStorage = tryBuildTitleStoragePathFromAbsolutePath(
        normalizedAbsoluteFolderPath,
        &titleStorageFolderPath);

    std::array<std::filesystem::path, kShipSpriteCount> sourcePngPaths{};
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        const std::filesystem::path pngPath = folderPath / (std::to_string(i + 1) + ".png");
        if (!std::filesystem::exists(pngPath, fsError) ||
            !std::filesystem::is_regular_file(pngPath, fsError))
        {
            this->statusMessage =
                "Dossier invalide: fichier manquant '" + std::to_string(i + 1) + ".png'.";
            return false;
        }
        sourcePngPaths[static_cast<size_t>(i)] = pngPath;
    }

    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();

    if (useTitleStorage)
    {
        if (!this->testShip.loadSpritesFromFolder(titleStorageFolderPath.c_str(), RC2D_STORAGE_TITLE))
        {
            this->statusMessage = "Echec chargement navire test depuis le cache TITLE.";
            return false;
        }
        if (!this->testShipPreview.loadSpritesFromFolder(titleStorageFolderPath.c_str(), RC2D_STORAGE_TITLE))
        {
            this->testShip.unloadSprites();
            this->statusMessage = "Echec chargement preview navire test depuis le cache TITLE.";
            return false;
        }
    }
    else
    {
        this->ensureUserStorageFolders();

        for (int i = 0; i < kShipSpriteCount; ++i)
        {
            const std::filesystem::path& sourcePath = sourcePngPaths[static_cast<size_t>(i)];
            std::ifstream input(sourcePath, std::ios::binary | std::ios::ate);
            if (!input.is_open())
            {
                this->statusMessage = "Lecture impossible: " + sourcePath.string();
                return false;
            }

            const std::streamsize fileSize = input.tellg();
            if (fileSize <= 0)
            {
                this->statusMessage = "Fichier vide: " + sourcePath.string();
                return false;
            }

            input.seekg(0, std::ios::beg);
            std::vector<char> bytes(static_cast<size_t>(fileSize));
            if (!input.read(bytes.data(), fileSize))
            {
                this->statusMessage = "Lecture bytes echouee: " + sourcePath.string();
                return false;
            }

            char userStoragePath[128] = {};
            SDL_snprintf(
                userStoragePath,
                sizeof(userStoragePath),
                "editor-map-ship/current/%d.png",
                i + 1);

            if (!rc2d_storage_userWriteFile(userStoragePath, bytes.data(), static_cast<Uint64>(bytes.size())))
            {
                this->statusMessage = "Echec copie user storage: " + std::string(userStoragePath);
                return false;
            }
        }

        // Copie optionnelle du ship_anchor.json pour conserver un rendu navire
        // coherent avec le gameplay. Si absent, on ecrit un JSON vide pour
        // neutraliser un eventuel fichier stale d'un import precedent.
        std::vector<char> anchorBytes;
        const std::filesystem::path anchorSourcePath = folderPath / "ship_anchor.json";
        if (std::filesystem::exists(anchorSourcePath, fsError) &&
            std::filesystem::is_regular_file(anchorSourcePath, fsError))
        {
            std::ifstream anchorInput(anchorSourcePath, std::ios::binary | std::ios::ate);
            if (anchorInput.is_open())
            {
                const std::streamsize anchorSize = anchorInput.tellg();
                if (anchorSize > 0)
                {
                    anchorInput.seekg(0, std::ios::beg);
                    anchorBytes.resize(static_cast<size_t>(anchorSize));
                    if (!anchorInput.read(anchorBytes.data(), anchorSize))
                    {
                        anchorBytes.clear();
                    }
                }
            }
        }
        if (anchorBytes.empty())
        {
            constexpr const char* kEmptyAnchorJson = "{}";
            anchorBytes.assign(kEmptyAnchorJson, kEmptyAnchorJson + 2);
        }
        if (!rc2d_storage_userWriteFile(
                "editor-map-ship/current/ship_anchor.json",
                anchorBytes.data(),
                static_cast<Uint64>(anchorBytes.size())))
        {
            this->statusMessage = "Echec copie ship_anchor.json dans user storage.";
            return false;
        }

        if (!this->testShip.loadSpritesFromFolder("editor-map-ship/current", RC2D_STORAGE_USER))
        {
            this->statusMessage = "Echec chargement navire test (sprites 1..8).";
            return false;
        }
        if (!this->testShipPreview.loadSpritesFromFolder("editor-map-ship/current", RC2D_STORAGE_USER))
        {
            this->testShip.unloadSprites();
            this->statusMessage = "Echec chargement preview navire test.";
            return false;
        }
    }

    this->testShip.setSpeedTilesPerSecond(4.0f);
    this->testShip.setHealthVisual(Ship::HealthVisual::FULL);
    this->testShip.setDrawAlpha(255);
    this->testShipPreview.setSpeedTilesPerSecond(4.0f);
    this->testShipPreview.setHealthVisual(Ship::HealthVisual::FULL);
    this->testShipPreview.setDrawAlpha(255);
    this->setShipScalePercent(this->shipScalePercent);
    this->testShipLoaded = true;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->loadedShipFolderAbsolute = normalizedAbsoluteFolderPath;
    this->editorTool = EditorTool::SPAWN_SHIP;
    this->statusMessage = "Navire test charge. Clique gauche sur la map pour le spawn.";
    return true;
}

bool EditorMapCreateMapScene::selectImportedShipAtIndex(int shipIndex)
{
    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Selection navire invalide.";
        return false;
    }

    this->selectedShipIndex = shipIndex;
    this->ensureSelectedShipVisible();

    const ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    if (!this->loadShipFolderFromAbsolutePath(selectedShip.folderAbsolutePath.c_str()))
    {
        return false;
    }

    this->statusMessage = "Navire actif: " + selectedShip.displayName + ". Clique gauche pour spawn.";
    return true;
}

bool EditorMapCreateMapScene::reexportLoadedShipScaled(int scalePercent)
{
    if (!this->testShipLoaded || this->loadedShipFolderAbsolute.empty())
    {
        this->statusMessage = "Aucun navire charge a reexporter.";
        return false;
    }
    const int clampedPercent = std::clamp(scalePercent, 10, 100);
    std::filesystem::path outputFolder =
        std::filesystem::path(this->loadedShipFolderAbsolute) /
        ("reexport_" + std::to_string(clampedPercent) + "pct");
    std::error_code fsError;
    std::filesystem::create_directories(outputFolder, fsError);
    if (fsError)
    {
        this->statusMessage = "Impossible de creer le dossier de reexport.";
        return false;
    }

    for (int i = 1; i <= kShipSpriteCount; ++i)
    {
        char storagePath[96] = {};
        SDL_snprintf(storagePath, sizeof(storagePath), "editor-map-ship/current/%d.png", i);
        RC2D_ImageData src = LoadStorageImageData(storagePath, RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            this->statusMessage = "Reexport navire: sprite source manquant.";
            return false;
        }

        const int srcW = src.sdl_surface->w;
        const int srcH = src.sdl_surface->h;
        const int dstW = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcW) * clampedPercent) / 100.0)));
        const int dstH = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcH) * clampedPercent) / 100.0)));
        SDL_Surface* dst = SDL_CreateSurface(dstW, dstH, SDL_PIXELFORMAT_RGBA32);
        if (dst == nullptr)
        {
            ReleaseStorageImageData(&src);
            this->statusMessage = "Reexport navire: creation surface KO.";
            return false;
        }

        for (int y = 0; y < dstH; ++y)
        {
            const int srcY = std::clamp((y * srcH) / dstH, 0, srcH - 1);
            for (int x = 0; x < dstW; ++x)
            {
                const int srcX = std::clamp((x * srcW) / dstW, 0, srcW - 1);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 0;
                SDL_ReadSurfacePixel(src.sdl_surface, srcX, srcY, &r, &g, &b, &a);
                SDL_WriteSurfacePixel(dst, x, y, r, g, b, a);
            }
        }

        std::filesystem::path dstPath = outputFolder / (std::to_string(i) + ".png");
        const bool saveOk = SDL_SavePNG(dst, dstPath.string().c_str());
        SDL_DestroySurface(dst);
        ReleaseStorageImageData(&src);
        if (!saveOk)
        {
            this->statusMessage = "Reexport navire: echec ecriture PNG.";
            return false;
        }
    }

    this->statusMessage = "Navire reexporte en " + std::to_string(clampedPercent) + "%.";
    return true;
}

bool EditorMapCreateMapScene::handleMapNameInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->mapNameInputFocused)
    {
        return false;
    }
    (void)mod;
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->mapNameInputFocused = false;
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->mapNameInputFocused = false;
        this->statusMessage = "Nom map valide.";
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->mapNameInput.empty() && !isrepeat)
    {
        this->mapNameInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->mapNameInput.empty() && !isrepeat)
    {
        this->mapNameInput.clear();
        return true;
    }

    auto appendIfRoom = [this](char c) -> bool {
        if (this->mapNameInput.size() >= 64U)
        {
            return true;
        }
        this->mapNameInput.push_back(c);
        return true;
    };

    // Force la prise en charge des caracteres selon scancode,
    // utile sur certains layouts clavier ou "key" est vide/different.
    const bool shiftDown = ((mod & SDL_KMOD_SHIFT) != 0);

    if (keycode == SDLK_MINUS || keycode == SDLK_KP_MINUS || keycode == SDLK_UNDERSCORE ||
        scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS ||
        // AZERTY: '-' souvent sur la touche '6' (sans shift)
        (scancode == SDL_SCANCODE_6 && !shiftDown))
    {
        return appendIfRoom('-');
    }
    if (keycode == SDLK_SLASH || keycode == SDLK_KP_DIVIDE || keycode == SDLK_QUESTION ||
        scancode == SDL_SCANCODE_SLASH || scancode == SDL_SCANCODE_KP_DIVIDE ||
        // AZERTY: '/' peut passer par la touche ponctuation avec shift.
        ((scancode == SDL_SCANCODE_PERIOD ||
          scancode == SDL_SCANCODE_COMMA ||
          scancode == SDL_SCANCODE_SEMICOLON ||
          scancode == SDL_SCANCODE_APOSTROPHE) && shiftDown))
    {
        return appendIfRoom('/');
    }

    if (key == nullptr || key[0] == '\0')
    {
        return true;
    }

    std::string keyNameLower = key;
    std::transform(
        keyNameLower.begin(),
        keyNameLower.end(),
        keyNameLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (keyNameLower.find("minus") != std::string::npos)
    {
        return appendIfRoom('-');
    }
    if (keyNameLower.find("slash") != std::string::npos || keyNameLower.find("divide") != std::string::npos)
    {
        return appendIfRoom('/');
    }
    if (std::strlen(key) != 1U)
    {
        return true;
    }
    const char c = key[0];
    const bool printable =
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '-' || c == '/' || c == ' ';
    if (!printable)
    {
        return true;
    }
    if (this->mapNameInput.size() >= 64U)
    {
        return true;
    }
    return appendIfRoom(c);
}



void EditorMapCreateMapScene::spawnTestShipAtTile(int tileX, int tileY)
{
    Map& map = GetCurrentMap();
    if (!this->testShipLoaded)
    {
        this->statusMessage = "Importe d'abord un dossier navire.";
        return;
    }

    if (!map.isInside(tileX, tileY))
    {
        this->statusMessage = "Spawn navire impossible: hors map.";
        return;
    }

    if (map.isTileBlocked(tileX, tileY))
    {
        this->statusMessage = "Spawn navire impossible: tuile bloquee.";
        return;
    }

    this->testShip.setPositionTileInt(tileX, tileY);
    this->testShipSpawned = true;
    this->statusMessage =
        "Navire spawn en (" + std::to_string(tileX) + "," + std::to_string(tileY) + ").";
}

void EditorMapCreateMapScene::moveTestShipToTile(int tileX, int tileY)
{
    Map& map = GetCurrentMap();
    if (!this->testShipLoaded || !this->testShipSpawned)
    {
        this->statusMessage = "Navire non spawn: place-le d'abord sur la map.";
        return;
    }

    if (!map.isInside(tileX, tileY))
    {
        this->statusMessage = "Cible navire hors map.";
        return;
    }

    if (map.isTileBlocked(tileX, tileY))
    {
        this->statusMessage = "Cible navire bloquee.";
        return;
    }

    this->testShip.moveToTile(map, tileX, tileY);
    if (this->testShip.isMoving())
    {
        this->statusMessage =
            "Navire deplace vers (" + std::to_string(tileX) + "," + std::to_string(tileY) + ").";
    }
    else
    {
        this->statusMessage = "Aucun chemin A* trouve vers la tuile cible.";
    }
}

void EditorMapCreateMapScene::updateTestShip(double dt)
{
    this->clickMarker.update(dt);

    if (!this->testShipLoaded || !this->testShipSpawned)
    {
        return;
    }

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    this->testShip.update(dt, map);

    if (this->testShipCameraFollowEnabled)
    {
        const SDL_FPoint shipTile = this->testShip.getPositionTile();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, map.rect);
    }
}

void EditorMapCreateMapScene::drawTestShip(void)
{
    // Marqueur visible principalement utile en mode controle navire.
    if (this->editorTool == EditorTool::CONTROL_SHIP)
    {
        this->clickMarker.draw(GetCurrentMap());
    }

    if (!this->testShipLoaded)
    {
        return;
    }

    // Navire effectif (si deja spawn).
    if (this->testShipSpawned)
    {
        this->testShip.draw(GetCurrentMap());
    }

    // Preview de spawn sous la souris.
    if (this->editorTool == EditorTool::SPAWN_SHIP && this->hoveredTileValid)
    {
        const Map& map = GetCurrentMap();
        if (map.isInside(this->hoveredTile.x, this->hoveredTile.y))
        {
            this->testShipPreview.setPositionTileInt(this->hoveredTile.x, this->hoveredTile.y);
            this->testShipPreview.setDrawAlpha(this->testShipSpawned ? static_cast<Uint8>(255) : static_cast<Uint8>(140));
            this->testShipPreview.draw(map);
        }
    }
}

bool EditorMapCreateMapScene::handleShipToolClick(float x, float y, RC2D_MouseButton button)
{
    if (this->editorTool != EditorTool::SPAWN_SHIP &&
        this->editorTool != EditorTool::CONTROL_SHIP)
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT && button != RC2D_MOUSE_BUTTON_RIGHT)
    {
        return true;
    }

    Map& map = GetCurrentMap();
    const SDL_Point tile = map.screenToTileNearest(x, y);
    if (!map.isInside(tile.x, tile.y))
    {
        this->statusMessage = "Hors map: clic navire ignore.";
        return true;
    }

    if (!this->testShipLoaded)
    {
        this->statusMessage = "Importe d'abord un dossier navire.";
        return true;
    }

    if (this->editorTool == EditorTool::SPAWN_SHIP)
    {
        // Mode spawn:
        // - clic gauche: spawn/re-spawn exactement sur la tuile pointee.
        // - clic droit: consomme sans action.
        if (button != RC2D_MOUSE_BUTTON_LEFT)
        {
            return true;
        }

        this->spawnTestShipAtTile(tile.x, tile.y);
        this->clickMarker.show(tile.x, tile.y);
        return true;
    }

    // Mode controle:
    // - clic gauche: deplacement A*.
    // - clic droit: repositionnement direct.
    if (!this->testShipSpawned)
    {
        this->statusMessage = "Navire non spawn: passe en mode SPAWN NAVIRE d'abord.";
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_RIGHT)
    {
        this->spawnTestShipAtTile(tile.x, tile.y);
        this->clickMarker.show(tile.x, tile.y);
        return true;
    }

    this->moveTestShipToTile(tile.x, tile.y);
    this->clickMarker.show(tile.x, tile.y);
    return true;
}

// ---------------------------------------------------------------------------
// Rendu monde (grille, collisions, assets).
// ---------------------------------------------------------------------------
void EditorMapCreateMapScene::drawWorldGridAndBlockedTiles(void) const
{
    const Map& map = GetCurrentMap();
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());

    // Zone visible de reference pour la grille: map rect, clamp a la taille
    // de sortie renderer pour ne dessiner que ce qui peut apparaitre a l'ecran.
    float viewportLeft = map.rect.x;
    float viewportTop = map.rect.y;
    float viewportRight = map.rect.x + map.rect.w;
    float viewportBottom = map.rect.y + map.rect.h;

    int outputWidth = 0;
    int outputHeight = 0;
    if (renderer != nullptr && SDL_GetCurrentRenderOutputSize(renderer, &outputWidth, &outputHeight))
    {
        viewportLeft = (std::max)(viewportLeft, 0.0f);
        viewportTop = (std::max)(viewportTop, 0.0f);
        viewportRight = (std::min)(viewportRight, static_cast<float>(outputWidth));
        viewportBottom = (std::min)(viewportBottom, static_cast<float>(outputHeight));
    }

    const SDL_FPoint corner0 = map.screenToTile(viewportLeft, viewportTop);
    const SDL_FPoint corner1 = map.screenToTile(viewportRight, viewportTop);
    const SDL_FPoint corner2 = map.screenToTile(viewportLeft, viewportBottom);
    const SDL_FPoint corner3 = map.screenToTile(viewportRight, viewportBottom);

    const float minTileXf = (std::min)((std::min)(corner0.x, corner1.x), (std::min)(corner2.x, corner3.x));
    const float maxTileXf = (std::max)((std::max)(corner0.x, corner1.x), (std::max)(corner2.x, corner3.x));
    const float minTileYf = (std::min)((std::min)(corner0.y, corner1.y), (std::min)(corner2.y, corner3.y));
    const float maxTileYf = (std::max)((std::max)(corner0.y, corner1.y), (std::max)(corner2.y, corner3.y));

    // Petite marge uniquement pour couvrir les bords partiellement visibles.
    const int marginTiles = 1;
    const int minTileX = (std::max)(static_cast<int>(std::floor(minTileXf)) - marginTiles, 0);
    const int maxTileX = (std::min)(static_cast<int>(std::ceil(maxTileXf)) + marginTiles, map.getWidthTiles() - 1);
    const int minTileY = (std::max)(static_cast<int>(std::floor(minTileYf)) - marginTiles, 0);
    const int maxTileY = (std::min)(static_cast<int>(std::ceil(maxTileYf)) + marginTiles, map.getHeightTiles() - 1);

    const float tileWidth = map.getTileWidth();
    const float tileHeight = map.getTileHeight();

    const float cullLeft = viewportLeft - tileWidth;
    const float cullTop = viewportTop - tileHeight;
    const float cullRight = viewportRight + tileWidth;
    const float cullBottom = viewportBottom + tileHeight;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    for (int tileY = minTileY; tileY <= maxTileY; ++tileY)
    {
        for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
        {
            const SDL_FPoint center = map.tileToScreenCenter(tileX, tileY);
            if (center.x < cullLeft || center.x > cullRight || center.y < cullTop || center.y > cullBottom)
            {
                continue;
            }

            if (this->showBlockedTiles && map.isTileBlocked(tileX, tileY))
            {
                const RC2D_Color blockedColor = kBlockedTilePalette[static_cast<size_t>(this->selectedBlockedColorIndex)];
                rc2d_graphics_setColor(blockedColor);
                rc2d_graphics_drawTileIsometric("fill", center.x, center.y, tileWidth, tileHeight);
            }

            if (this->showGrid)
            {
                rc2d_graphics_setColor(kGridColor);
                rc2d_graphics_drawTileIsometric("line", center.x, center.y, tileWidth, tileHeight);
            }
        }
    }

    if (this->hoveredTileValid)
    {
        if (this->editorTool == EditorTool::BLOCK_TILES && this->blockedBrushRadiusTiles > 0)
        {
            for (int oy = -this->blockedBrushRadiusTiles; oy <= this->blockedBrushRadiusTiles; ++oy)
            {
                for (int ox = -this->blockedBrushRadiusTiles; ox <= this->blockedBrushRadiusTiles; ++ox)
                {
                    const int tx = this->hoveredTile.x + ox;
                    const int ty = this->hoveredTile.y + oy;
                    if (!map.isInside(tx, ty))
                    {
                        continue;
                    }
                    const SDL_FPoint hoveredCenter = map.tileToScreenCenter(tx, ty);
                    rc2d_graphics_setColor(RC2D_Color{255, 225, 110, 62});
                    rc2d_graphics_drawTileIsometric("fill", hoveredCenter.x, hoveredCenter.y, tileWidth, tileHeight);
                    rc2d_graphics_setColor(kHoverTileColor);
                    rc2d_graphics_drawTileIsometric("line", hoveredCenter.x, hoveredCenter.y, tileWidth, tileHeight);
                }
            }
        }
        else
        {
            const SDL_FPoint hoveredCenter = map.tileToScreenCenter(this->hoveredTile.x, this->hoveredTile.y);
            rc2d_graphics_setColor(kHoverTileColor);
            rc2d_graphics_drawTileIsometric("line", hoveredCenter.x, hoveredCenter.y, tileWidth, tileHeight);
        }
    }

    const RC2D_Color hotspotColor = kHotspotPalette[static_cast<size_t>(this->selectedHotspotColorIndex)];
    const bool previewHotspots =
        this->towerVariantPickerVisible ||
        (this->towerPreviewDisplayMode == TowerPreviewDisplayMode::HOTSPOTS);
    const int previewTowerLevel = std::clamp(this->towerPreviewDisplayLevel, 1, 4) - 1;
    for (const TowerHotspot& hotspot : this->towerHotspots)
    {
        if (!map.isInside(hotspot.tileX, hotspot.tileY))
        {
            continue;
        }
        const SDL_FPoint center = map.tileToScreenCenter(hotspot.tileX, hotspot.tileY);
        if (previewHotspots)
        {
            rc2d_graphics_setColor(hotspotColor);
            rc2d_graphics_drawTileIsometric("fill", center.x, center.y, tileWidth, tileHeight);
            rc2d_graphics_setColor(RC2D_Color{245, 250, 255, 240});
            rc2d_graphics_drawTileIsometric("line", center.x, center.y, tileWidth, tileHeight);
        }

        if (this->overlayFont.sdl_font != nullptr)
        {
            const std::string towerNumberText =
                std::to_string(ClampEditorMapTowerHotspotNumber(hotspot.towerNumber));
            RC2D_Text hotspotText =
                rc2d_graphics_createText(
                    const_cast<RC2D_Font*>(&this->overlayFont),
                    towerNumberText.c_str());
            hotspotText.color = RC2D_Color{255, 255, 255, 250};
            rc2d_graphics_setTextColor(&hotspotText);

            int textW = 0;
            int textH = 0;
            rc2d_graphics_getTextSize(&hotspotText, &textW, &textH);
            const float textX = center.x - (static_cast<float>(textW) * 0.5f);
            const float textY = previewHotspots
                ? (center.y - (static_cast<float>(textH) * 0.5f))
                : (center.y - 28.0f - static_cast<float>(textH));
            rc2d_graphics_drawText(&hotspotText, textX, textY);
            rc2d_graphics_destroyText(&hotspotText);
        }

        int visualSetIndex = this->findTowerVisualSetIndexByNumber(hotspot.towerNumber);
        if (visualSetIndex < 0 && !this->towerVisualSets.empty())
        {
            visualSetIndex = 0;
        }
        if (visualSetIndex < 0)
        {
            continue;
        }

        const TowerVisualSet& visualSet = this->towerVisualSets[static_cast<size_t>(visualSetIndex)];
        if (previewHotspots)
        {
            continue;
        }

        const int importedAssetIndex =
            visualSet.importedAssetIndices[static_cast<size_t>(previewTowerLevel)];
        if (importedAssetIndex < 0 ||
            importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset =
            this->importedAssets[static_cast<size_t>(importedAssetIndex)];
        if (importedAsset.image.sdl_texture == nullptr)
        {
            continue;
        }

        const float previewScale = 0.55f * (std::max)(GetCamera().getZoomFactor(), 0.01f);
        const float drawX = center.x - ((importedAsset.widthPx * previewScale) * 0.5f);
        const float drawY = center.y - ((importedAsset.heightPx * previewScale) * 0.7f);
        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&importedAsset.image),
            0.0f,
            0.0f,
            importedAsset.widthPx,
            importedAsset.heightPx);
        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&importedAsset.image),
            &sourceQuad,
            drawX,
            drawY,
            0.0,
            previewScale,
            previewScale,
            0.0f,
            0.0f,
            false,
            false);

        if (this->overlayFont.sdl_font != nullptr)
        {
            const std::string levelLabel = "Niv " + std::to_string(previewTowerLevel + 1);
            RC2D_Text levelText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), levelLabel.c_str());
            levelText.color = RC2D_Color{255, 240, 170, 250};
            rc2d_graphics_setTextColor(&levelText);
            rc2d_graphics_drawText(&levelText, drawX, drawY - 12.0f);
            rc2d_graphics_destroyText(&levelText);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCreateMapScene::drawPlacedAssets(void) const
{
    const Map& map = GetCurrentMap();
    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float cullMargin = 80.0f;
    const float mapLeft = map.rect.x - cullMargin;
    const float mapTop = map.rect.y - cullMargin;
    const float mapRight = map.rect.x + map.rect.w + cullMargin;
    const float mapBottom = map.rect.y + map.rect.h + cullMargin;

    std::vector<size_t> drawOrder;
    drawOrder.reserve(this->placedAssets.size());

    for (size_t i = 0; i < this->placedAssets.size(); ++i)
    {
        const PlacedAsset& placedAsset = this->placedAssets[i];
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];
        if (importedAsset.image.sdl_texture == nullptr)
        {
            continue;
        }

        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(placedAsset.anchorTileX, placedAsset.anchorTileY);
        const float effectiveScale = placedAsset.scale * worldZoom;
        const float scaledWidth = importedAsset.widthPx * effectiveScale;
        const float scaledHeight = importedAsset.heightPx * effectiveScale;
        const float drawX = anchorScreen.x;
        const float drawY = anchorScreen.y;
        const float drawRight = drawX + scaledWidth;
        const float drawBottom = drawY + scaledHeight;

        if (drawRight < mapLeft || drawX > mapRight || drawBottom < mapTop || drawY > mapBottom)
        {
            continue;
        }

        drawOrder.push_back(i);
    }

    std::sort(
        drawOrder.begin(),
        drawOrder.end(),
        [this](size_t a, size_t b) {
            const PlacedAsset& assetA = this->placedAssets[a];
            const PlacedAsset& assetB = this->placedAssets[b];
            const int depthA = assetA.tileX + assetA.tileY;
            const int depthB = assetB.tileX + assetB.tileY;
            if (depthA != depthB)
            {
                return depthA < depthB;
            }
            if (assetA.tileY != assetB.tileY)
            {
                return assetA.tileY < assetB.tileY;
            }
            return a < b;
        });

    for (size_t drawIndex : drawOrder)
    {
        const PlacedAsset& placedAsset = this->placedAssets[drawIndex];
        if (placedAsset.importedAssetIndex < 0 ||
            placedAsset.importedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            continue;
        }

        const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(placedAsset.importedAssetIndex)];
        if (importedAsset.image.sdl_texture == nullptr)
        {
            continue;
        }

        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(placedAsset.anchorTileX, placedAsset.anchorTileY);
        const float effectiveScale = placedAsset.scale * worldZoom;
        const float drawX = anchorScreen.x;
        const float drawY = anchorScreen.y;
        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&importedAsset.image),
            0.0f,
            0.0f,
            importedAsset.widthPx,
            importedAsset.heightPx);

        Uint8 oldAlpha = 255;
        SDL_GetTextureAlphaMod(importedAsset.image.sdl_texture, &oldAlpha);
        const Uint8 alpha = this->assetTransparencyEnabled
            ? static_cast<Uint8>(std::clamp((this->assetOpacityPercent * 255) / 100, 0, 255))
            : static_cast<Uint8>(255);
        SDL_SetTextureAlphaMod(importedAsset.image.sdl_texture, alpha);

        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&importedAsset.image),
            &sourceQuad,
            drawX,
            drawY,
            0.0,
            effectiveScale,
            effectiveScale,
            0.0f,
            0.0f,
            false,
            false);
        SDL_SetTextureAlphaMod(importedAsset.image.sdl_texture, oldAlpha);
    }

    if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        const PlacedAsset& selectedPlacedAsset =
            this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)];
        const std::vector<SDL_Point> coveredTiles =
            this->computePlacedAssetClickInteractionTiles(selectedPlacedAsset);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        for (const SDL_Point& coveredTile : coveredTiles)
        {
            const SDL_FPoint tileCenter = map.tileToScreenCenter(coveredTile.x, coveredTile.y);
            rc2d_graphics_setColor(RC2D_Color{96, 190, 255, 36});
            rc2d_graphics_drawTileIsometric(
                "fill",
                tileCenter.x,
                tileCenter.y,
                map.getTileWidth(),
                map.getTileHeight());
            rc2d_graphics_setColor(RC2D_Color{140, 220, 255, 88});
            rc2d_graphics_drawTileIsometric(
                "line",
                tileCenter.x,
                tileCenter.y,
                map.getTileWidth(),
                map.getTileHeight());
        }

        const SDL_Point centerTile = this->computePlacedAssetCenterTile(selectedPlacedAsset);
        const SDL_FPoint centerScreen = map.tileToScreenCenter(centerTile.x, centerTile.y);
        rc2d_graphics_setColor(RC2D_Color{255, 240, 145, 235});
        rc2d_graphics_drawTileIsometric(
            "line",
            centerScreen.x,
            centerScreen.y,
            map.getTileWidth(),
            map.getTileHeight());
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }

    if (this->editorTool == EditorTool::PLACE_ASSETS &&
        this->selectedAssetIndex >= 0 &&
        this->selectedAssetIndex < static_cast<int>(this->importedAssets.size()))
    {
        const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        if (selectedAsset.image.sdl_texture != nullptr)
        {
            SDL_Point snappedTile{};
            if (this->tryGetMouseTile(&snappedTile))
            {
                const SDL_FPoint snappedAnchorScreen = map.tileToScreenCenterFloat(
                    static_cast<float>(snappedTile.x),
                    static_cast<float>(snappedTile.y));
                const float drawX = snappedAnchorScreen.x;
                const float drawY = snappedAnchorScreen.y;
                const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
                    const_cast<RC2D_Image*>(&selectedAsset.image),
                    0.0f,
                    0.0f,
                    selectedAsset.widthPx,
                    selectedAsset.heightPx);

                SDL_SetTextureAlphaMod(selectedAsset.image.sdl_texture, 170);
                rc2d_graphics_drawQuad(
                    const_cast<RC2D_Image*>(&selectedAsset.image),
                    &sourceQuad,
                    drawX,
                    drawY,
                    0.0,
                    worldZoom,
                    worldZoom,
                    0.0f,
                    0.0f,
                    false,
                    false);
                SDL_SetTextureAlphaMod(selectedAsset.image.sdl_texture, 255);
            }
        }
    }
}


void EditorMapCreateMapScene::updateToolbarLayout(void)
{
    // Barre d'actions dans la bande UI basse (70 px reserves).
    // Si certains boutons sortent du cadre (surtout sur petits ecrans),
    // on les replace a gauche du bouton LISTES ON, mais sous la zone deja
    // occupee en haut pour eviter qu'ils soient caches.
    const Map& map = GetCurrentMap();
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float startX = 12.0f;
    const float row1Y = map.rect.y + map.rect.h + 4.0f;
    const float row2Y = row1Y + 30.0f;
    const float h = 24.0f;
    const float gap = 8.0f;

    auto setNextButton = [h, gap](SDL_FRect* rect, float* x, float y, float w) {
        if (rect == nullptr || x == nullptr)
        {
            return;
        }
        rect->x = *x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        *x += w + gap;
    };

    // Ligne 1: actions principales.
    float x = startX;
    setNextButton(&this->buttonImportRect, &x, row1Y, 160.0f);
    setNextButton(&this->buttonImportMapRect, &x, row1Y, 150.0f);
    setNextButton(&this->buttonExportRect, &x, row1Y, 138.0f);
    setNextButton(&this->buttonUndoRect, &x, row1Y, 92.0f);
    setNextButton(&this->buttonRedoRect, &x, row1Y, 92.0f);
    setNextButton(&this->buttonToolBlockRect, &x, row1Y, 100.0f);
    setNextButton(&this->buttonToolUnblockRect, &x, row1Y, 156.0f);
    setNextButton(&this->buttonToolPlaceRect, &x, row1Y, 100.0f);
    setNextButton(&this->buttonToolRemoveRect, &x, row1Y, 156.0f);
    setNextButton(&this->buttonToolInteractRect, &x, row1Y, 170.0f);
    setNextButton(&this->buttonToolShipRect, &x, row1Y, 135.0f);
    setNextButton(&this->buttonToolShipControlRect, &x, row1Y, 155.0f);
    setNextButton(&this->buttonGridRect, &x, row1Y, 74.0f);
    setNextButton(&this->buttonCenterRect, &x, row1Y, 126.0f);

    // Ligne 2: outils towers, centrage navire, selection d'asset, ocean, collisions/opacite.
    x = startX;
    setNextButton(&this->buttonToolHotspotRect, &x, row2Y, 122.0f);
    setNextButton(&this->buttonTowerVariantsPickerRect, &x, row2Y, 256.0f);
    setNextButton(&this->buttonTowerDisplayModeRect, &x, row2Y, 238.0f);
    setNextButton(&this->buttonCenterShipRect, &x, row2Y, 150.0f);
    setNextButton(&this->buttonAssetPrevRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonAssetNextRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonBlockedBrushMinusRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonBlockedBrushPlusRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonBlockedColorPrevRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonBlockedColorNextRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonHotspotColorPrevRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonHotspotColorNextRect, &x, row2Y, 68.0f);
    setNextButton(&this->buttonAssetOpacityToggleRect, &x, row2Y, 126.0f);
    setNextButton(&this->buttonAssetOpacityMinusRect, &x, row2Y, 60.0f);
    setNextButton(&this->buttonAssetOpacityPlusRect, &x, row2Y, 60.0f);
    setNextButton(&this->buttonZoomOutRect, &x, row2Y, 64.0f);
    setNextButton(&this->buttonZoomInRect, &x, row2Y, 64.0f);
    setNextButton(&this->buttonBlockedTilesRect, &x, row2Y, 140.0f);

    this->mapNameInputRect = SDL_FRect{
        map.rect.x + map.rect.w - 318.0f,
        map.rect.y - 24.0f,
        278.0f,
        20.0f
    };

    const float topGap = 8.0f;
    this->buttonListsVisibilityRect = SDL_FRect{
        this->mapNameInputRect.x - topGap - 120.0f,
        this->mapNameInputRect.y,
        120.0f,
        this->mapNameInputRect.h
    };
    if (this->buttonListsVisibilityRect.x < (map.rect.x + 12.0f))
    {
        this->buttonListsVisibilityRect.x = map.rect.x + 12.0f;
    }

    const float screenRight = gameScreenRect.x + gameScreenRect.w;
    const float screenBottom = gameScreenRect.y + gameScreenRect.h;
    const float overflowStartX = map.rect.x + 12.0f;
    const float overflowMaxRight = (std::max)(
        this->buttonListsVisibilityRect.x - gap,
        overflowStartX + 120.0f);
    float overflowX = overflowStartX;
    float overflowY = this->buttonListsVisibilityRect.y + this->buttonListsVisibilityRect.h + 8.0f;

    auto moveOverflowButton = [&](SDL_FRect* rect) {
        if (rect == nullptr)
        {
            return;
        }

        const bool overflowsHorizontally = (rect->x + rect->w) > (screenRight - 12.0f);
        const bool overflowsVertically = (rect->y + rect->h) > (screenBottom - 6.0f);
        if (!overflowsHorizontally && !overflowsVertically)
        {
            return;
        }

        if ((overflowX + rect->w) > overflowMaxRight && overflowX > overflowStartX)
        {
            overflowX = overflowStartX;
            overflowY += (h + 4.0f);
        }

        rect->x = overflowX;
        rect->y = overflowY;
        overflowX += rect->w + gap;
    };

    const std::array<SDL_FRect*, 28> buttonsToClamp = {{
        &this->buttonImportRect,
        &this->buttonImportMapRect,
        &this->buttonExportRect,
        &this->buttonUndoRect,
        &this->buttonRedoRect,
        &this->buttonToolBlockRect,
        &this->buttonToolUnblockRect,
        &this->buttonToolPlaceRect,
        &this->buttonToolRemoveRect,
        &this->buttonToolInteractRect,
        &this->buttonToolShipRect,
        &this->buttonToolShipControlRect,
        &this->buttonToolHotspotRect,
        &this->buttonTowerVariantsPickerRect,
        &this->buttonTowerDisplayModeRect,
        &this->buttonGridRect,
        &this->buttonCenterShipRect,
        &this->buttonAssetPrevRect,
        &this->buttonAssetNextRect,
        &this->buttonOceanPrevRect,
        &this->buttonOceanNextRect,
        &this->buttonBlockedBrushMinusRect,
        &this->buttonBlockedBrushPlusRect,
        &this->buttonBlockedColorPrevRect,
        &this->buttonBlockedColorNextRect,
        &this->buttonHotspotColorPrevRect,
        &this->buttonHotspotColorNextRect,
        &this->buttonAssetOpacityToggleRect,
    }};

    for (SDL_FRect* rect : buttonsToClamp)
    {
        moveOverflowButton(rect);
    }

    const float utilityGap = 6.0f;
    const float utilityRow1Y = this->buttonListsVisibilityRect.y + this->buttonListsVisibilityRect.h + 8.0f;
    const float utilityRow2Y = utilityRow1Y + h + 4.0f;
    float utilityX = this->buttonListsVisibilityRect.x;

    auto placeUtilityFromLeft = [utilityGap](SDL_FRect* rect, float* leftX, float y) {
        if (rect == nullptr || leftX == nullptr)
        {
            return;
        }
        rect->x = *leftX;
        rect->y = y;
        *leftX += rect->w + utilityGap;
    };

    placeUtilityFromLeft(&this->buttonCenterRect, &utilityX, utilityRow1Y);
    placeUtilityFromLeft(&this->buttonBlockedTilesRect, &utilityX, utilityRow1Y);

    utilityX = this->buttonListsVisibilityRect.x;
    placeUtilityFromLeft(&this->buttonAssetOpacityMinusRect, &utilityX, utilityRow2Y);
    placeUtilityFromLeft(&this->buttonAssetOpacityPlusRect, &utilityX, utilityRow2Y);
    placeUtilityFromLeft(&this->buttonZoomOutRect, &utilityX, utilityRow2Y);
    placeUtilityFromLeft(&this->buttonZoomInRect, &utilityX, utilityRow2Y);

    // Mini-liste d'assets en bas a droite, dans la zone map.
    this->assetListRect.w = 250.0f;
    this->assetListRect.h = 276.0f;
    this->assetListRect.x = map.rect.x + map.rect.w - this->assetListRect.w - 40.0f;
    this->assetListRect.y = map.rect.y + map.rect.h - this->assetListRect.h - 40.0f;

    // Liste navires: meme dimensions que la liste assets, placee juste a gauche.
    this->shipListRect.w = this->assetListRect.w;
    this->shipListRect.h = this->assetListRect.h;
    this->shipListRect.x = this->assetListRect.x - this->shipListRect.w - 16.0f;
    this->shipListRect.y = this->assetListRect.y;
    this->shipListRect.x = (std::max)(this->shipListRect.x, map.rect.x + 12.0f);

    // Minimap maison en haut a droite dans la zone monde.
    const float miniMapSize = computeEditorMiniMapSize(map.rect);
    this->miniMapRect.w = miniMapSize;
    this->miniMapRect.h = miniMapSize;
    this->miniMapRect.x = map.rect.x + map.rect.w - this->miniMapRect.w - 40.0f;
    this->miniMapRect.y = map.rect.y + 40.0f;

    this->towerVariantPickerRect = SDL_FRect{
        map.rect.x + (map.rect.w * 0.5f) - 350.0f,
        map.rect.y + 36.0f,
        700.0f,
        452.0f
    };
    const float tabY = this->towerVariantPickerRect.y + 28.0f;
    const float tabX = this->towerVariantPickerRect.x + 10.0f;
    for (int i = 0; i < 4; ++i)
    {
        this->towerVariantLevelTabRects[static_cast<size_t>(i)] = SDL_FRect{
            tabX + (static_cast<float>(i) * 100.0f),
            tabY,
            92.0f,
            22.0f
        };
    }
    this->towerVariantConfirmRect = SDL_FRect{
        this->towerVariantPickerRect.x + this->towerVariantPickerRect.w - 130.0f,
        this->towerVariantPickerRect.y + this->towerVariantPickerRect.h - 34.0f,
        116.0f,
        24.0f
    };

    this->towerDisplayPickerRect = SDL_FRect{
        map.rect.x + (map.rect.w * 0.5f) - 224.0f,
        map.rect.y + 54.0f,
        448.0f,
        138.0f
    };
    const float displayTabY = this->towerDisplayPickerRect.y + 46.0f;
    const float displayTabX = this->towerDisplayPickerRect.x + 10.0f;
    for (int i = 0; i < 5; ++i)
    {
        this->towerDisplayLevelTabRects[static_cast<size_t>(i)] = SDL_FRect{
            displayTabX + (static_cast<float>(i) * 84.0f),
            displayTabY,
            78.0f,
            22.0f
        };
    }
    this->towerDisplayConfirmRect = SDL_FRect{
        this->towerDisplayPickerRect.x + this->towerDisplayPickerRect.w - 126.0f,
        this->towerDisplayPickerRect.y + this->towerDisplayPickerRect.h - 32.0f,
        112.0f,
        24.0f
    };
}

void EditorMapCreateMapScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
{
    RC2D_Color fillColor = active ? RC2D_Color{95, 145, 190, 210} : RC2D_Color{36, 44, 52, 190};
    RC2D_Color borderColor = active ? RC2D_Color{160, 215, 255, 250} : RC2D_Color{140, 150, 165, 220};
    RC2D_Color textColor = RC2D_Color{235, 242, 250, 250};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(fillColor);
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setColor(borderColor);
    rc2d_graphics_rectangle("line", &rect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    if (this->overlayFont.sdl_font == nullptr || label == nullptr)
    {
        return;
    }

    RC2D_Text text = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), label);
    text.color = textColor;
    rc2d_graphics_setTextColor(&text);

    int textW = 0;
    int textH = 0;
    rc2d_graphics_getTextSize(&text, &textW, &textH);

    const float drawX = rect.x + ((rect.w - static_cast<float>(textW)) * 0.5f);
    const float drawY = rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f);
    rc2d_graphics_drawText(&text, drawX, drawY);
    rc2d_graphics_destroyText(&text);
}

int EditorMapCreateMapScene::getAssetListMaxScrollOffset(void) const
{
    if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        const int guiCount = static_cast<int>(kEditorMapAssetClickGuiTargets.size());
        return (std::max)(guiCount - kAssetListVisibleRows, 0);
    }

    const int assetCount = static_cast<int>(this->importedAssets.size());
    return (std::max)(assetCount - kAssetListVisibleRows, 0);
}

void EditorMapCreateMapScene::clampAssetListScrollOffset(void)
{
    if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        this->assetInteractionListScrollOffset = std::clamp(
            this->assetInteractionListScrollOffset,
            0,
            this->getAssetListMaxScrollOffset());
        return;
    }

    this->assetListScrollOffset = std::clamp(
        this->assetListScrollOffset,
        0,
        this->getAssetListMaxScrollOffset());
}

void EditorMapCreateMapScene::ensureSelectedAssetVisible(void)
{
    this->clampAssetListScrollOffset();

    if (this->selectedAssetIndex < 0)
    {
        return;
    }

    if (this->selectedAssetIndex < this->assetListScrollOffset)
    {
        this->assetListScrollOffset = this->selectedAssetIndex;
        this->clampAssetListScrollOffset();
        return;
    }

    const int lastVisibleIndex = this->assetListScrollOffset + kAssetListVisibleRows - 1;
    if (this->selectedAssetIndex > lastVisibleIndex)
    {
        this->assetListScrollOffset = this->selectedAssetIndex - (kAssetListVisibleRows - 1);
        this->clampAssetListScrollOffset();
    }
}

int EditorMapCreateMapScene::computeAssetListStartIndex(void) const
{
    const int maxOffset = this->getAssetListMaxScrollOffset();
    if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        return std::clamp(this->assetInteractionListScrollOffset, 0, maxOffset);
    }

    return std::clamp(this->assetListScrollOffset, 0, maxOffset);
}

bool EditorMapCreateMapScene::handleAssetListClick(float x, float y)
{
    if (!this->pointInRect(x, y, this->assetListRect))
    {
        return false;
    }

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->assetListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->assetListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kAssetListVisibleRows);
    const float rowsLeftX = this->assetListRect.x + panelPadding;
    const float rowsWidth = this->assetListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    const bool editingPlacedAssetInteraction =
        this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS;
    const bool editingTowerVariants = false;
    const int rowCount = editingPlacedAssetInteraction
        ? static_cast<int>(kEditorMapAssetClickGuiTargets.size())
        : static_cast<int>(this->importedAssets.size());

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        const int maxOffset = (std::max)(rowCount - kAssetListVisibleRows, 0);
        if (maxOffset <= 0)
        {
            this->assetListScrollDragActive = false;
            return true;
        }

        float thumbHeight = scrollTrackRect.h;
        float thumbY = scrollTrackRect.y;
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(rowCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(this->computeAssetListStartIndex()) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;

        SDL_FRect scrollThumbRect{};
        scrollThumbRect.x = scrollTrackRect.x + 1.0f;
        scrollThumbRect.y = thumbY;
        scrollThumbRect.w = scrollTrackRect.w - 2.0f;
        scrollThumbRect.h = thumbHeight;

        if (this->pointInRect(x, y, scrollThumbRect))
        {
            // Drag precis: conserve l'offset exact curseur->thumb.
            this->assetListScrollDragActive = true;
            this->assetListScrollDragGrabOffsetY = y - scrollThumbRect.y;
        }
        else
        {
            // Clic track: on saute puis on autorise le drag continu.
            const float targetThumbY = std::clamp(
                y - (scrollThumbRect.h * 0.5f),
                scrollTrackRect.y,
                scrollTrackRect.y + thumbTravel);
            const float clickRatio = (thumbTravel > 0.0f)
                ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
                : 0.0f;
            if (editingPlacedAssetInteraction)
            {
                this->assetInteractionListScrollOffset =
                    static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
            }
            else
            {
                this->assetListScrollOffset =
                    static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
            }
            this->clampAssetListScrollOffset();

            this->assetListScrollDragActive = true;
            this->assetListScrollDragGrabOffsetY = scrollThumbRect.h * 0.5f;
        }
        return true;
    }

    // Clic dans la liste hors scrollbar: stop drag scrollbar.
    this->assetListScrollDragActive = false;

    if (!editingPlacedAssetInteraction && this->importedAssets.empty())
    {
        this->statusMessage = "Aucun asset importe.";
        return true;
    }

    const int startIndex = this->computeAssetListStartIndex();
    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int rowIndex = startIndex + i;
        if (rowIndex >= rowCount)
        {
            break;
        }

        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        if (!this->pointInRect(x, y, rowRect))
        {
            continue;
        }

        if (editingPlacedAssetInteraction)
        {
            if (!this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
            {
                this->clearSelectedPlacedAsset();
                return true;
            }

            PlacedAsset& selectedPlacedAsset =
                this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)];
            selectedPlacedAsset.clickGuiTarget =
                kEditorMapAssetClickGuiTargets[static_cast<size_t>(rowIndex)].target;
            this->selectedAssetClickGuiTarget = selectedPlacedAsset.clickGuiTarget;

            const EditorMapAssetClickGuiTargetInfo& guiInfo =
                GetEditorMapAssetClickGuiTargetInfo(selectedPlacedAsset.clickGuiTarget);
            this->statusMessage =
                "GUI liee a l'asset: " + std::string(guiInfo.label) +
                " | Tiles=" + std::to_string(static_cast<int>(
                    this->computePlacedAssetClickInteractionTiles(selectedPlacedAsset).size()));
            return true;
        }

        this->selectedAssetIndex = rowIndex;
        this->ensureSelectedAssetVisible();
        const ImportedAsset& selectedAsset =
            this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        return true;
    }

    // Consomme quand meme le clic dans le panneau pour eviter un paint tile.
    return true;
}

bool EditorMapCreateMapScene::handleTowerVariantPickerClick(float x, float y)
{
    if (!this->towerVariantPickerVisible)
    {
        return false;
    }

    if (!this->pointInRect(x, y, this->towerVariantPickerRect))
    {
        this->towerVariantPickerVisible = false;
        this->statusMessage = "Popup variantes tower fermee.";
        return true;
    }

    if (this->pointInRect(x, y, this->towerVariantConfirmRect))
    {
        this->towerVariantPickerVisible = false;
        this->statusMessage = "Variantes tower confirmees.";
        return true;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (!this->pointInRect(x, y, this->towerVariantLevelTabRects[static_cast<size_t>(i)]))
        {
            continue;
        }

        this->selectedTowerVariantLevel = i + 1;
        this->statusMessage =
            "Edition variantes tower: niv " + std::to_string(this->selectedTowerVariantLevel);
        return true;
    }

    const float panelPadding = 6.0f;
    const float rowsTopY = this->towerVariantPickerRect.y + 58.0f;
    const float rowsLeftX = this->towerVariantPickerRect.x + panelPadding;
    const float rowsWidth =
        this->towerVariantPickerRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);
    const float rowsHeight = this->towerVariantPickerRect.h - 154.0f;
    const float rowGap = 3.0f;
    const float rowHeight =
        (rowsHeight - ((kAssetListVisibleRows - 1) * rowGap)) / static_cast<float>(kAssetListVisibleRows);
    const int startIndex = std::clamp(this->assetListScrollOffset, 0, this->getAssetListMaxScrollOffset());

    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int rowIndex = startIndex + i;
        if (rowIndex >= static_cast<int>(this->importedAssets.size()))
        {
            break;
        }

        SDL_FRect rowRect{
            rowsLeftX,
            rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap)),
            rowsWidth,
            rowHeight
        };
        if (!this->pointInRect(x, y, rowRect))
        {
            continue;
        }

        this->selectedAssetIndex = rowIndex;
        this->ensureSelectedAssetVisible();
        if (this->towerVisualSets.empty())
        {
            (void)this->ensureTowerVisualSet(this->selectedTowerHotspotNumber);
        }
        for (const TowerHotspot& hotspot : this->towerHotspots)
        {
            (void)this->ensureTowerVisualSet(hotspot.towerNumber);
        }
        for (TowerVisualSet& visualSet : this->towerVisualSets)
        {
            visualSet.importedAssetIndices[static_cast<size_t>(this->selectedTowerVariantLevel - 1)] =
                this->selectedAssetIndex;
        }
        const ImportedAsset& selectedAsset =
            this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        this->statusMessage =
            "Toutes les towers: niv " + std::to_string(this->selectedTowerVariantLevel)
            + " assigne a " + selectedAsset.displayName;
        return true;
    }

    return true;
}

bool EditorMapCreateMapScene::handleTowerDisplayPickerClick(float x, float y)
{
    if (!this->towerDisplayPickerVisible)
    {
        return false;
    }

    if (!this->pointInRect(x, y, this->towerDisplayPickerRect))
    {
        this->towerDisplayPickerVisible = false;
        this->statusMessage = "Popup affichage towers fermee.";
        return true;
    }

    if (this->pointInRect(x, y, this->towerDisplayConfirmRect))
    {
        this->towerDisplayPickerVisible = false;
        this->statusMessage =
            "Affichage towers confirme: niv " + std::to_string(this->towerPreviewDisplayLevel) + ".";
        return true;
    }

    for (int i = 0; i < 5; ++i)
    {
        if (!this->pointInRect(x, y, this->towerDisplayLevelTabRects[static_cast<size_t>(i)]))
        {
            continue;
        }

        if (i == 0)
        {
            this->towerPreviewDisplayMode = TowerPreviewDisplayMode::HOTSPOTS;
            this->statusMessage = "Preview hotspots: tuile active.";
            return true;
        }

        this->towerPreviewDisplayMode = TowerPreviewDisplayMode::TOWERS;
        this->towerPreviewDisplayLevel = i;
        this->statusMessage =
            "Preview towers globale: niv " + std::to_string(this->towerPreviewDisplayLevel);
        return true;
    }

    return true;
}

void EditorMapCreateMapScene::handleAssetListScrollDragFromMouse(void)
{
    if (!this->assetListScrollDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    (void)mouseX;

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->assetListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->assetListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowsLeftX = this->assetListRect.x + panelPadding;
    const float rowsWidth = this->assetListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    const bool editingPlacedAssetInteraction =
        this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS;
    const int rowCount = editingPlacedAssetInteraction
        ? static_cast<int>(kEditorMapAssetClickGuiTargets.size())
        : static_cast<int>(this->importedAssets.size());
    const int maxOffset = (std::max)(rowCount - kAssetListVisibleRows, 0);
    if (maxOffset <= 0)
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        return;
    }

    float thumbHeight = (std::max)(
        14.0f,
        (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(rowCount));
    const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
    const float targetThumbY = std::clamp(
        mouseY - this->assetListScrollDragGrabOffsetY,
        scrollTrackRect.y,
        scrollTrackRect.y + thumbTravel);
    const float ratio = (thumbTravel > 0.0f)
        ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
        : 0.0f;

    if (editingPlacedAssetInteraction)
    {
        this->assetInteractionListScrollOffset =
            static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
    }
    else
    {
        this->assetListScrollOffset =
            static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
    }
    this->clampAssetListScrollOffset();
}

void EditorMapCreateMapScene::drawAssetListPanel(void) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kAssetPanelFillColor);
    rc2d_graphics_rectangle("fill", &this->assetListRect);
    rc2d_graphics_setColor(kAssetPanelBorderColor);
    rc2d_graphics_rectangle("line", &this->assetListRect);

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->assetListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->assetListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kAssetListVisibleRows);
    const float rowsLeftX = this->assetListRect.x + panelPadding;
    const float rowsWidth = this->assetListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);
    const bool editingPlacedAssetInteraction =
        this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
        this->editorTool == EditorTool::INTERACT_ASSETS;
    const bool editingTowerVariants = false;
    const int rowCount = editingPlacedAssetInteraction
        ? static_cast<int>(kEditorMapAssetClickGuiTargets.size())
        : static_cast<int>(this->importedAssets.size());

    if (this->overlayFont.sdl_font != nullptr)
    {
        const char* headerLabel = "Assets charges";
        if (editingPlacedAssetInteraction)
        {
            headerLabel = "GUI de l'asset";
        }
        else if (editingTowerVariants)
        {
            headerLabel = "Variants tower";
        }
        RC2D_Text headerText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), headerLabel);
        headerText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&headerText);
        rc2d_graphics_drawText(&headerText, this->assetListRect.x + panelPadding, this->assetListRect.y + 1.0f);
        rc2d_graphics_destroyText(&headerText);
    }

    // Scrollbar de la liste d'assets.
    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;
    rc2d_graphics_setColor(RC2D_Color{58, 68, 79, 220});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{110, 122, 136, 220});
    rc2d_graphics_rectangle("line", &scrollTrackRect);

    if (rowCount <= 0)
    {
        if (this->overlayFont.sdl_font != nullptr)
        {
            const char* emptyLabel = "Aucun asset";
            if (editingPlacedAssetInteraction)
            {
                emptyLabel = "Aucune GUI";
            }
            else if (editingTowerVariants)
            {
                emptyLabel = "Aucun asset tower";
            }
            RC2D_Text emptyText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), emptyLabel);
            emptyText.color = kHudStatusColor;
            rc2d_graphics_setTextColor(&emptyText);
            rc2d_graphics_drawText(&emptyText, this->assetListRect.x + panelPadding, rowsTopY + 2.0f);
            rc2d_graphics_destroyText(&emptyText);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int startIndex = this->computeAssetListStartIndex();
    const int maxOffset = (std::max)(rowCount - kAssetListVisibleRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(rowCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;
    }

    SDL_FRect scrollThumbRect{};
    scrollThumbRect.x = scrollTrackRect.x + 1.0f;
    scrollThumbRect.y = thumbY;
    scrollThumbRect.w = scrollTrackRect.w - 2.0f;
    scrollThumbRect.h = thumbHeight;
    rc2d_graphics_setColor(RC2D_Color{170, 188, 210, 235});
    rc2d_graphics_rectangle("fill", &scrollThumbRect);
    rc2d_graphics_setColor(RC2D_Color{205, 220, 238, 245});
    rc2d_graphics_rectangle("line", &scrollThumbRect);

    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int rowIndex = startIndex + i;
        if (rowIndex >= rowCount)
        {
            break;
        }

        bool isSelected = false;
        if (editingPlacedAssetInteraction)
        {
            if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
            {
                const PlacedAsset& selectedPlacedAsset =
                    this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)];
                isSelected =
                    (kEditorMapAssetClickGuiTargets[static_cast<size_t>(rowIndex)].target ==
                     selectedPlacedAsset.clickGuiTarget);
            }
        }
        else if (editingTowerVariants)
        {
            const int visualSetIndex = this->findTowerVisualSetIndexByNumber(this->selectedTowerHotspotNumber);
            if (visualSetIndex >= 0)
            {
                const TowerVisualSet& visualSet =
                    this->towerVisualSets[static_cast<size_t>(visualSetIndex)];
                isSelected =
                    visualSet.importedAssetIndices[static_cast<size_t>(this->selectedTowerVariantLevel - 1)] ==
                    rowIndex;
            }
            else
            {
                isSelected = (rowIndex == this->selectedAssetIndex);
            }
        }
        else
        {
            isSelected = (rowIndex == this->selectedAssetIndex);
        }
        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        rc2d_graphics_setColor(isSelected ? kAssetRowSelectedFillColor : kAssetRowFillColor);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kAssetRowBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);

        if (this->overlayFont.sdl_font == nullptr)
        {
            continue;
        }

        char textBuffer[256] = {};
        if (editingPlacedAssetInteraction)
        {
            const EditorMapAssetClickGuiTargetInfo& guiInfo =
                kEditorMapAssetClickGuiTargets[static_cast<size_t>(rowIndex)];
            SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", rowIndex + 1, guiInfo.label);
        }
        else
        {
            std::string rowLabel = makeAssetLabel(
                this->importedAssets[static_cast<size_t>(rowIndex)].displayName,
                24);
            SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", rowIndex + 1, rowLabel.c_str());
        }

        RC2D_Text rowText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), textBuffer);
        rowText.color = RC2D_Color{235, 242, 250, 248};
        rc2d_graphics_setTextColor(&rowText);
        rc2d_graphics_drawText(&rowText, rowRect.x + 4.0f, rowRect.y + 1.0f);
        rc2d_graphics_destroyText(&rowText);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCreateMapScene::drawTowerVariantPickerPopup(void) const
{
    if (!this->towerVariantPickerVisible)
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{8, 12, 18, 180});
    rc2d_graphics_rectangle("fill", &GetGameScreen().rect);
    rc2d_graphics_setColor(kAssetPanelFillColor);
    rc2d_graphics_rectangle("fill", &this->towerVariantPickerRect);
    rc2d_graphics_setColor(kAssetPanelBorderColor);
    rc2d_graphics_rectangle("line", &this->towerVariantPickerRect);

    if (this->overlayFont.sdl_font != nullptr)
    {
        const std::string title =
            "VARIANTES TOWERS - LIER NIVEAUX ET ASSETS";
        RC2D_Text titleText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title.c_str());
        titleText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&titleText);
        rc2d_graphics_drawText(
            &titleText,
            this->towerVariantPickerRect.x + 10.0f,
            this->towerVariantPickerRect.y + 6.0f);
        rc2d_graphics_destroyText(&titleText);
    }

    for (int i = 0; i < 4; ++i)
    {
        const bool active = (this->selectedTowerVariantLevel == (i + 1));
        const std::string label = "Niv " + std::to_string(i + 1);
        this->drawToolbarButton(
            this->towerVariantLevelTabRects[static_cast<size_t>(i)],
            label.c_str(),
            active);
    }

    const float panelPadding = 6.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->towerVariantPickerRect.y + 58.0f;
    const float rowsLeftX = this->towerVariantPickerRect.x + panelPadding;
    const float rowsWidth =
        this->towerVariantPickerRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);
    const float rowsHeight = this->towerVariantPickerRect.h - 154.0f;
    const float rowHeight =
        (rowsHeight - ((kAssetListVisibleRows - 1) * rowGap)) / static_cast<float>(kAssetListVisibleRows);
    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;
    rc2d_graphics_setColor(RC2D_Color{58, 68, 79, 220});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{110, 122, 136, 220});
    rc2d_graphics_rectangle("line", &scrollTrackRect);
    const int startIndex = std::clamp(this->assetListScrollOffset, 0, this->getAssetListMaxScrollOffset());
    const int visualSetIndex = this->findTowerVisualSetIndexByNumber(this->selectedTowerHotspotNumber);
    int selectedVariantAssetIndex = -1;
    if (visualSetIndex >= 0)
    {
        selectedVariantAssetIndex =
            this->towerVisualSets[static_cast<size_t>(visualSetIndex)]
                .importedAssetIndices[static_cast<size_t>(this->selectedTowerVariantLevel - 1)];
    }
    const int rowCount = static_cast<int>(this->importedAssets.size());
    const int maxOffset = (std::max)(rowCount - kAssetListVisibleRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(
            14.0f,
            (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(rowCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;
    }
    SDL_FRect scrollThumbRect{};
    scrollThumbRect.x = scrollTrackRect.x + 1.0f;
    scrollThumbRect.y = thumbY;
    scrollThumbRect.w = scrollTrackRect.w - 2.0f;
    scrollThumbRect.h = thumbHeight;
    rc2d_graphics_setColor(RC2D_Color{170, 188, 210, 235});
    rc2d_graphics_rectangle("fill", &scrollThumbRect);
    rc2d_graphics_setColor(RC2D_Color{205, 220, 238, 245});
    rc2d_graphics_rectangle("line", &scrollThumbRect);

    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int rowIndex = startIndex + i;
        if (rowIndex >= static_cast<int>(this->importedAssets.size()))
        {
            break;
        }

        SDL_FRect rowRect{
            rowsLeftX,
            rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap)),
            rowsWidth,
            rowHeight
        };
        const bool isSelected = (selectedVariantAssetIndex == rowIndex);
        rc2d_graphics_setColor(isSelected ? kAssetRowSelectedFillColor : kAssetRowFillColor);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kAssetRowBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);

        if (this->overlayFont.sdl_font != nullptr)
        {
            const ImportedAsset& importedAsset = this->importedAssets[static_cast<size_t>(rowIndex)];
            const std::string rowLabel = std::to_string(rowIndex + 1) + ". " +
                makeAssetLabel(importedAsset.displayName, 26);
            RC2D_Text rowText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), rowLabel.c_str());
            rowText.color = RC2D_Color{235, 242, 250, 248};
            rc2d_graphics_setTextColor(&rowText);
            rc2d_graphics_drawText(&rowText, rowRect.x + 6.0f, rowRect.y + 3.0f);
            rc2d_graphics_destroyText(&rowText);
        }

        for (int levelIndex = 0; levelIndex < 4; ++levelIndex)
        {
            if (visualSetIndex < 0)
            {
                break;
            }
            if (this->towerVisualSets[static_cast<size_t>(visualSetIndex)]
                    .importedAssetIndices[static_cast<size_t>(levelIndex)] != rowIndex)
            {
                continue;
            }

            SDL_FRect badgeRect{
                rowRect.x + rowRect.w - 44.0f - (static_cast<float>(3 - levelIndex) * 40.0f),
                rowRect.y + 3.0f,
                34.0f,
                rowRect.h - 6.0f
            };
            this->drawToolbarButton(
                badgeRect,
                ("N" + std::to_string(levelIndex + 1)).c_str(),
                levelIndex + 1 == this->selectedTowerVariantLevel);
        }
    }

    if (this->overlayFont.sdl_font != nullptr)
    {
        const std::string helpText =
            "Choisis un niveau puis clique l'asset a lier.";
        RC2D_Text help =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), helpText.c_str());
        help.color = kHudStatusColor;
        rc2d_graphics_setTextColor(&help);
        rc2d_graphics_drawText(
            &help,
            this->towerVariantPickerRect.x + 10.0f,
            this->towerVariantConfirmRect.y - 20.0f);
        rc2d_graphics_destroyText(&help);
    }

    this->drawToolbarButton(this->towerVariantConfirmRect, "CONFIRMER", false);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCreateMapScene::drawTowerDisplayPickerPopup(void) const
{
    if (!this->towerDisplayPickerVisible)
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{8, 12, 18, 180});
    rc2d_graphics_rectangle("fill", &GetGameScreen().rect);
    rc2d_graphics_setColor(kAssetPanelFillColor);
    rc2d_graphics_rectangle("fill", &this->towerDisplayPickerRect);
    rc2d_graphics_setColor(kAssetPanelBorderColor);
    rc2d_graphics_rectangle("line", &this->towerDisplayPickerRect);

    if (this->overlayFont.sdl_font != nullptr)
    {
        const std::string title = "CHOISIR AFFICHAGE HOTSPOT / TOWER";
        RC2D_Text titleText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title.c_str());
        titleText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&titleText);
        rc2d_graphics_drawText(
            &titleText,
            this->towerDisplayPickerRect.x + 10.0f,
            this->towerDisplayPickerRect.y + 8.0f);
        rc2d_graphics_destroyText(&titleText);
    }

    for (int i = 0; i < 5; ++i)
    {
        std::string label = "Niv " + std::to_string(i);
        bool active = false;
        if (i == 0)
        {
            label = "Tuile";
            active = (this->towerPreviewDisplayMode == TowerPreviewDisplayMode::HOTSPOTS);
        }
        else
        {
            active =
                this->towerPreviewDisplayMode == TowerPreviewDisplayMode::TOWERS &&
                this->towerPreviewDisplayLevel == i;
        }
        this->drawToolbarButton(
            this->towerDisplayLevelTabRects[static_cast<size_t>(i)],
            label.c_str(),
            active);
    }

    this->drawToolbarButton(this->towerDisplayConfirmRect, "CONFIRMER", false);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

int EditorMapCreateMapScene::getShipListMaxScrollOffset(void) const
{
    const int shipCount = static_cast<int>(this->importedShips.size());
    return (std::max)(shipCount - kAssetListVisibleRows, 0);
}

void EditorMapCreateMapScene::clampShipListScrollOffset(void)
{
    this->shipListScrollOffset = std::clamp(this->shipListScrollOffset, 0, this->getShipListMaxScrollOffset());
}

void EditorMapCreateMapScene::ensureSelectedShipVisible(void)
{
    this->clampShipListScrollOffset();

    if (this->selectedShipIndex < 0)
    {
        return;
    }

    if (this->selectedShipIndex < this->shipListScrollOffset)
    {
        this->shipListScrollOffset = this->selectedShipIndex;
        this->clampShipListScrollOffset();
        return;
    }

    const int lastVisibleIndex = this->shipListScrollOffset + kAssetListVisibleRows - 1;
    if (this->selectedShipIndex > lastVisibleIndex)
    {
        this->shipListScrollOffset = this->selectedShipIndex - (kAssetListVisibleRows - 1);
        this->clampShipListScrollOffset();
    }
}

int EditorMapCreateMapScene::computeShipListStartIndex(void) const
{
    const int maxOffset = this->getShipListMaxScrollOffset();
    return std::clamp(this->shipListScrollOffset, 0, maxOffset);
}

bool EditorMapCreateMapScene::handleShipListClick(float x, float y)
{
    if (!this->pointInRect(x, y, this->shipListRect))
    {
        return false;
    }

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->shipListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->shipListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kAssetListVisibleRows);
    const float rowsLeftX = this->shipListRect.x + panelPadding;
    const float rowsWidth = this->shipListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        const int shipCount = static_cast<int>(this->importedShips.size());
        const int maxOffset = (std::max)(shipCount - kAssetListVisibleRows, 0);
        if (maxOffset <= 0)
        {
            this->shipListScrollDragActive = false;
            return true;
        }

        float thumbHeight = scrollTrackRect.h;
        float thumbY = scrollTrackRect.y;
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(shipCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(this->computeShipListStartIndex()) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;

        SDL_FRect scrollThumbRect{};
        scrollThumbRect.x = scrollTrackRect.x + 1.0f;
        scrollThumbRect.y = thumbY;
        scrollThumbRect.w = scrollTrackRect.w - 2.0f;
        scrollThumbRect.h = thumbHeight;

        if (this->pointInRect(x, y, scrollThumbRect))
        {
            this->shipListScrollDragActive = true;
            this->shipListScrollDragGrabOffsetY = y - scrollThumbRect.y;
        }
        else
        {
            const float targetThumbY = std::clamp(
                y - (scrollThumbRect.h * 0.5f),
                scrollTrackRect.y,
                scrollTrackRect.y + thumbTravel);
            const float clickRatio = (thumbTravel > 0.0f)
                ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
                : 0.0f;
            this->shipListScrollOffset = static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
            this->clampShipListScrollOffset();

            this->shipListScrollDragActive = true;
            this->shipListScrollDragGrabOffsetY = scrollThumbRect.h * 0.5f;
        }
        return true;
    }

    this->shipListScrollDragActive = false;

    if (this->importedShips.empty())
    {
        this->statusMessage = "Aucun navire importe.";
        return true;
    }

    const int startIndex = this->computeShipListStartIndex();
    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int shipIndex = startIndex + i;
        if (shipIndex >= static_cast<int>(this->importedShips.size()))
        {
            break;
        }

        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        if (!this->pointInRect(x, y, rowRect))
        {
            continue;
        }

        this->selectImportedShipAtIndex(shipIndex);
        return true;
    }

    return true;
}

void EditorMapCreateMapScene::handleShipListScrollDragFromMouse(void)
{
    if (!this->shipListScrollDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    (void)mouseX;

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->shipListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->shipListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowsLeftX = this->shipListRect.x + panelPadding;
    const float rowsWidth = this->shipListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    const int shipCount = static_cast<int>(this->importedShips.size());
    const int maxOffset = (std::max)(shipCount - kAssetListVisibleRows, 0);
    if (maxOffset <= 0)
    {
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
        return;
    }

    const float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(shipCount));
    const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
    const float targetThumbY = std::clamp(
        mouseY - this->shipListScrollDragGrabOffsetY,
        scrollTrackRect.y,
        scrollTrackRect.y + thumbTravel);
    const float ratio = (thumbTravel > 0.0f)
        ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
        : 0.0f;

    this->shipListScrollOffset = static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
    this->clampShipListScrollOffset();
}

void EditorMapCreateMapScene::handleSelectedPlacedAssetInteractionPaintFromMouse(void)
{
    if (!this->assetInteractionPaintActive)
    {
        return;
    }

    if (this->editorTool != EditorTool::INTERACT_ASSETS)
    {
        this->assetInteractionPaintActive = false;
        this->lastAssetInteractionPaintTileValid = false;
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->assetInteractionPaintActive = false;
        this->lastAssetInteractionPaintTileValid = false;
        return;
    }

    if (!this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
    {
        this->assetInteractionPaintActive = false;
        this->lastAssetInteractionPaintTileValid = false;
        return;
    }

    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    if (this->lastAssetInteractionPaintTileValid &&
        this->lastAssetInteractionPaintTile.x == tile.x &&
        this->lastAssetInteractionPaintTile.y == tile.y)
    {
        return;
    }

    this->paintSelectedPlacedAssetInteractionAtMouse(this->assetInteractionPaintValue);
}

void EditorMapCreateMapScene::drawShipListPanel(void) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kAssetPanelFillColor);
    rc2d_graphics_rectangle("fill", &this->shipListRect);
    rc2d_graphics_setColor(kAssetPanelBorderColor);
    rc2d_graphics_rectangle("line", &this->shipListRect);

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->shipListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->shipListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kAssetListVisibleRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kAssetListVisibleRows);
    const float rowsLeftX = this->shipListRect.x + panelPadding;
    const float rowsWidth = this->shipListRect.w - ((panelPadding * 2.0f) + kAssetListScrollBarWidth + 4.0f);

    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text headerText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Navires charges");
        headerText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&headerText);
        rc2d_graphics_drawText(&headerText, this->shipListRect.x + panelPadding, this->shipListRect.y + 1.0f);
        rc2d_graphics_destroyText(&headerText);
    }

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kAssetListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;
    rc2d_graphics_setColor(RC2D_Color{58, 68, 79, 220});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{110, 122, 136, 220});
    rc2d_graphics_rectangle("line", &scrollTrackRect);

    if (this->importedShips.empty())
    {
        if (this->overlayFont.sdl_font != nullptr)
        {
            RC2D_Text emptyText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Aucun navire");
            emptyText.color = kHudStatusColor;
            rc2d_graphics_setTextColor(&emptyText);
            rc2d_graphics_drawText(&emptyText, this->shipListRect.x + panelPadding, rowsTopY + 2.0f);
            rc2d_graphics_destroyText(&emptyText);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int shipCount = static_cast<int>(this->importedShips.size());
    const int startIndex = this->computeShipListStartIndex();
    const int maxOffset = (std::max)(shipCount - kAssetListVisibleRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(shipCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;
    }

    SDL_FRect scrollThumbRect{};
    scrollThumbRect.x = scrollTrackRect.x + 1.0f;
    scrollThumbRect.y = thumbY;
    scrollThumbRect.w = scrollTrackRect.w - 2.0f;
    scrollThumbRect.h = thumbHeight;
    rc2d_graphics_setColor(RC2D_Color{170, 188, 210, 235});
    rc2d_graphics_rectangle("fill", &scrollThumbRect);
    rc2d_graphics_setColor(RC2D_Color{205, 220, 238, 245});
    rc2d_graphics_rectangle("line", &scrollThumbRect);

    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int shipIndex = startIndex + i;
        if (shipIndex >= shipCount)
        {
            break;
        }

        const bool isSelected = (shipIndex == this->selectedShipIndex);
        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        rc2d_graphics_setColor(isSelected ? kAssetRowSelectedFillColor : kAssetRowFillColor);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kAssetRowBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);

        if (this->overlayFont.sdl_font == nullptr)
        {
            continue;
        }

        std::string rowLabel = makeAssetLabel(this->importedShips[static_cast<size_t>(shipIndex)].displayName, 24);
        char textBuffer[256] = {};
        SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", shipIndex + 1, rowLabel.c_str());

        RC2D_Text rowText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), textBuffer);
        rowText.color = RC2D_Color{235, 242, 250, 248};
        rc2d_graphics_setTextColor(&rowText);
        rc2d_graphics_drawText(&rowText, rowRect.x + 4.0f, rowRect.y + 1.0f);
        rc2d_graphics_destroyText(&rowText);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}








bool EditorMapCreateMapScene::tryBuildMiniMapViewRect(SDL_FRect* outRect) const
{
    if (outRect == nullptr)
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    const float sectorSpanX = static_cast<float>((std::max)(Map::NUM_SECTORS_X, 1));
    const float sectorSpanY = static_cast<float>((std::max)(Map::NUM_SECTORS_Y, 1));
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return false;
    }

    // On se base sur le vrai centre de vue (camera appliquee a la map),
    // puis on derive l'emprise visible en espace secteurs selon le zoom courant.
    const float viewCenterScreenX = map.rect.x + (map.rect.w * 0.5f);
    const float viewCenterScreenY = map.rect.y + (map.rect.h * 0.5f);
    const SDL_FPoint centerTile = map.screenToTile(viewCenterScreenX, viewCenterScreenY);
    const SDL_FPoint centerSector = map.tileToSectorFloat(centerTile.x, centerTile.y);

    // Etendue visible en espace iso (u/v), convertie en etendue secteurs.
    const float halfViewU = (map.rect.w * 0.5f) / halfTileW;
    const float halfViewV = (map.rect.h * 0.5f) / halfTileH;
    const float halfSectorX = halfViewU / (2.0f * static_cast<float>(Map::SECTOR_STEP));
    const float halfSectorY = halfViewV / (2.0f * static_cast<float>(Map::SECTOR_STEP));

    const float minSectorCenterX = centerSector.x - halfSectorX;
    const float maxSectorCenterXVisible = centerSector.x + halfSectorX;
    const float minSectorCenterY = centerSector.y - halfSectorY;
    const float maxSectorCenterYVisible = centerSector.y + halfSectorY;

    // Passage en "edges secteurs" [0..NUM_SECTORS], coherent avec la minimap.
    float minSectorEdgeX = std::clamp(minSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float maxSectorEdgeX = std::clamp(maxSectorCenterXVisible + 0.5f, 0.0f, sectorSpanX);
    float minSectorEdgeY = std::clamp(minSectorCenterY + 0.5f, 0.0f, sectorSpanY);
    float maxSectorEdgeY = std::clamp(maxSectorCenterYVisible + 0.5f, 0.0f, sectorSpanY);

    if (maxSectorEdgeX < minSectorEdgeX)
    {
        std::swap(minSectorEdgeX, maxSectorEdgeX);
    }
    if (maxSectorEdgeY < minSectorEdgeY)
    {
        std::swap(minSectorEdgeY, maxSectorEdgeY);
    }

    SDL_FRect viewRect{};
    viewRect.x = this->miniMapRect.x + ((minSectorEdgeX / sectorSpanX) * this->miniMapRect.w);
    viewRect.y = this->miniMapRect.y + ((minSectorEdgeY / sectorSpanY) * this->miniMapRect.h);
    viewRect.w = ((maxSectorEdgeX - minSectorEdgeX) / sectorSpanX) * this->miniMapRect.w;
    viewRect.h = ((maxSectorEdgeY - minSectorEdgeY) / sectorSpanY) * this->miniMapRect.h;

    viewRect.w = (std::max)(viewRect.w, 2.0f);
    viewRect.h = (std::max)(viewRect.h, 2.0f);
    viewRect.x = std::clamp(viewRect.x, this->miniMapRect.x, this->miniMapRect.x + this->miniMapRect.w - viewRect.w);
    viewRect.y = std::clamp(viewRect.y, this->miniMapRect.y, this->miniMapRect.y + this->miniMapRect.h - viewRect.h);

    *outRect = viewRect;
    return true;
}

void EditorMapCreateMapScene::moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    this->testShipCameraFollowEnabled = false;

    float localX = miniMapX - this->miniMapRect.x;
    float localY = miniMapY - this->miniMapRect.y;
    if (applyDragOffset)
    {
        localX -= this->miniMapDragOffsetX;
        localY -= this->miniMapDragOffsetY;
    }

    const float nx = std::clamp(localX / (std::max)(this->miniMapRect.w, 1.0f), 0.0f, 1.0f);
    const float ny = std::clamp(localY / (std::max)(this->miniMapRect.h, 1.0f), 0.0f, 1.0f);

    const float targetSectorX = miniMapNormalizedToSectorCenter(nx, Map::NUM_SECTORS_X);
    const float targetSectorY = miniMapNormalizedToSectorCenter(ny, Map::NUM_SECTORS_Y);
    const float targetTileX =
        static_cast<float>(Map::SECTOR_BASE_X) +
        ((targetSectorX + targetSectorY) * static_cast<float>(Map::SECTOR_STEP));
    const float targetTileY =
        static_cast<float>(Map::SECTOR_BASE_Y) +
        ((targetSectorY - targetSectorX) * static_cast<float>(Map::SECTOR_STEP));

    camera.centerCameraOnTile(targetTileX, targetTileY, map, map.rect);
    camera.update(map, map.rect);
}

bool EditorMapCreateMapScene::handleMiniMapClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->pointInRect(x, y, this->miniMapRect))
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        // On consomme le clic dans la minimap pour eviter les actions monde.
        this->testShipCameraFollowEnabled = false;
        return true;
    }

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect) && this->pointInRect(x, y, viewRect))
    {
        const float viewCenterX = viewRect.x + (viewRect.w * 0.5f);
        const float viewCenterY = viewRect.y + (viewRect.h * 0.5f);
        this->miniMapDragOffsetX = x - viewCenterX;
        this->miniMapDragOffsetY = y - viewCenterY;
    }
    else
    {
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        this->moveCameraFromMiniMapPoint(x, y, true);
    }

    this->miniMapDragActive = true;
    return true;
}

void EditorMapCreateMapScene::handleMiniMapDragFromMouse(void)
{
    if (!this->miniMapDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->miniMapDragActive = false;
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    this->moveCameraFromMiniMapPoint(mouseX, mouseY, true);
}

void EditorMapCreateMapScene::drawMiniMap(void) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{18, 24, 32, 220});
    rc2d_graphics_rectangle("fill", &this->miniMapRect);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        const int miniMapWidthPx =
            miniMapRectDimensionToPixels(this->miniMapRect.w);
        const int miniMapHeightPx =
            miniMapRectDimensionToPixels(this->miniMapRect.h);

        SDL_Surface* miniMapSurface = SDL_CreateSurface(
            miniMapWidthPx,
            miniMapHeightPx,
            SDL_PIXELFORMAT_RGBA32);
        if (miniMapSurface != nullptr)
        {
            if (this->renderStyledMiniMapToSurface(miniMapSurface))
            {
                SDL_Texture* miniMapTexture =
                    SDL_CreateTextureFromSurface(renderer, miniMapSurface);
                if (miniMapTexture != nullptr)
                {
                    SDL_RenderTexture(renderer, miniMapTexture, nullptr, &this->miniMapRect);
                    SDL_DestroyTexture(miniMapTexture);
                }
            }

            SDL_DestroySurface(miniMapSurface);
        }
    }

    rc2d_graphics_setColor(RC2D_Color{135, 150, 168, 235});
    rc2d_graphics_rectangle("line", &this->miniMapRect);

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect))
    {
        rc2d_graphics_setColor(RC2D_Color{125, 198, 255, 55});
        rc2d_graphics_rectangle("fill", &viewRect);
        rc2d_graphics_setColor(RC2D_Color{170, 222, 255, 245});
        rc2d_graphics_rectangle("line", &viewRect);
    }

    if (this->testShipLoaded && this->testShipSpawned)
    {
        const Map& map = GetCurrentMap();
        const SDL_FPoint shipTile = this->testShip.getPositionTile();
        const SDL_FPoint shipSector = map.tileToSectorFloat(shipTile.x, shipTile.y);
        const float nx = std::clamp((shipSector.x + 0.5f) / static_cast<float>(Map::NUM_SECTORS_X), 0.0f, 1.0f);
        const float ny = std::clamp((shipSector.y + 0.5f) / static_cast<float>(Map::NUM_SECTORS_Y), 0.0f, 1.0f);
        SDL_FRect pixelRect{};
        pixelRect.w = 2.0f;
        pixelRect.h = 2.0f;
        pixelRect.x = this->miniMapRect.x + (nx * this->miniMapRect.w) - 1.0f;
        pixelRect.y = this->miniMapRect.y + (ny * this->miniMapRect.h) - 1.0f;
        rc2d_graphics_setColor(RC2D_Color{245, 45, 45, 255});
        rc2d_graphics_rectangle("fill", &pixelRect);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool EditorMapCreateMapScene::pointInRect(float x, float y, const SDL_FRect& rect) const
{
    return (
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

// ---------------------------------------------------------------------------
// UI editeur (toolbar, infos, listes, minimap).
// ---------------------------------------------------------------------------
bool EditorMapCreateMapScene::handleToolbarClick(float x, float y)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    if (this->pointInRect(x, y, this->buttonImportRect))
    {
        this->openImportAssetDialog();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonListsVisibilityRect))
    {
        this->showBottomRightLists = !this->showBottomRightLists;
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
        this->statusMessage = this->showBottomRightLists
            ? "Listes navires/assets: ON"
            : "Listes navires/assets: OFF";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonImportMapRect))
    {
        this->openImportMapDialog();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonExportRect))
    {
        this->openExportMapDialog();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonUndoRect))
    {
        this->undoHistoryAction();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonRedoRect))
    {
        this->redoHistoryAction();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolBlockRect))
    {
        if (this->editorTool == EditorTool::INTERACT_ASSETS)
        {
            this->assetInteractionEditMode = AssetInteractionEditMode::PAINT_ADD;
            this->statusMessage = "Mode Interaction asset: ajout de tiles au clic gauche.";
            return true;
        }
        this->editorTool = EditorTool::BLOCK_TILES;
        this->collisionPaintBlocks = true;
        this->statusMessage = "Mode Collision: clic gauche bloque (brush actif).";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolUnblockRect))
    {
        if (this->editorTool == EditorTool::INTERACT_ASSETS)
        {
            this->assetInteractionEditMode = AssetInteractionEditMode::PAINT_REMOVE;
            this->statusMessage = "Mode Interaction asset: suppression de tiles au clic gauche.";
            return true;
        }
        this->editorTool = EditorTool::BLOCK_TILES;
        this->collisionPaintBlocks = false;
        this->statusMessage = "Mode Suppression collision: clic gauche debloque (brush actif).";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolPlaceRect))
    {
        this->editorTool = EditorTool::PLACE_ASSETS;
        this->statusMessage =
            "Mode Pose visuel: clic gauche vide=pose, clic gauche asset=selection interaction, clic droit=supprime.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolRemoveRect))
    {
        this->editorTool = EditorTool::REMOVE_ASSETS;
        this->statusMessage = "Mode Suppression visuel: clic gauche supprime asset.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolInteractRect))
    {
        this->editorTool = EditorTool::INTERACT_ASSETS;
        this->assetInteractionEditMode = AssetInteractionEditMode::SELECT;
        this->statusMessage =
            "Mode Interaction asset: clique un asset pour le selectionner puis choisis AJOUT TILE ou SUPPR TILE.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolShipRect))
    {
        this->editorTool = EditorTool::SPAWN_SHIP;
        if (!this->testShipLoaded)
        {
            this->statusMessage = "Mode SPAWN NAVIRE: importe d'abord un dossier navire.";
        }
        else
        {
            this->statusMessage = "Mode SPAWN NAVIRE: clique gauche pour spawn/re-spawn.";
        }
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolShipControlRect))
    {
        this->editorTool = EditorTool::CONTROL_SHIP;
        if (!this->testShipLoaded)
        {
            this->statusMessage = "Mode CONTROL NAVIRE: importe d'abord un dossier navire.";
        }
        else if (!this->testShipSpawned)
        {
            this->statusMessage = "Mode CONTROL NAVIRE: navire non spawn (utilise SPAWN NAVIRE).";
        }
        else
        {
            this->statusMessage = "Mode CONTROL NAVIRE: clic gauche deplace (A*), clic droit respawn.";
        }
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolHotspotRect))
    {
        this->editorTool = EditorTool::HOTSPOT_TOWERS;
        this->statusMessage =
            "Mode HOTSPOT TOUR: clique sur la tuile voulue pour la tower "
            + std::to_string(ClampEditorMapTowerHotspotNumber(this->selectedTowerHotspotNumber))
            + ".";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonTowerVariantsPickerRect))
    {
        this->editorTool = EditorTool::HOTSPOT_TOWERS;
        this->towerDisplayPickerVisible = false;
        this->towerVariantPickerVisible = !this->towerVariantPickerVisible;
        this->statusMessage = this->towerVariantPickerVisible
            ? "Popup variantes towers ouverte."
            : "Popup variantes tower fermee.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonTowerDisplayModeRect))
    {
        this->towerVariantPickerVisible = false;
        this->towerDisplayPickerVisible = !this->towerDisplayPickerVisible;
        this->statusMessage = this->towerDisplayPickerVisible
            ? "Choisis TUILE HOTSPOT ou un niveau global puis confirme."
            : "Popup affichage fermee.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonAssetPrevRect))
    {
        if (this->importedAssets.empty())
        {
            this->statusMessage = "Aucun asset importe.";
            return true;
        }
        this->selectedAssetIndex -= 1;
        if (this->selectedAssetIndex < 0)
        {
            this->selectedAssetIndex = static_cast<int>(this->importedAssets.size()) - 1;
        }
        this->ensureSelectedAssetVisible();
        const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonAssetNextRect))
    {
        if (this->importedAssets.empty())
        {
            this->statusMessage = "Aucun asset importe.";
            return true;
        }
        this->selectedAssetIndex += 1;
        if (this->selectedAssetIndex >= static_cast<int>(this->importedAssets.size()))
        {
            this->selectedAssetIndex = 0;
        }
        this->ensureSelectedAssetVisible();
        const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonOceanPrevRect))
    {
        this->requestOceanColorStep(-1);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonOceanNextRect))
    {
        this->requestOceanColorStep(1);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonBlockedBrushMinusRect))
    {
        if (this->editorTool == EditorTool::HOTSPOT_TOWERS)
        {
            this->selectedTowerHotspotNumber =
                ClampEditorMapTowerHotspotNumber(this->selectedTowerHotspotNumber - 1);
            this->statusMessage =
                "Tower hotspot selectionnee: "
                + std::to_string(this->selectedTowerHotspotNumber) + "/12";
            return true;
        }
        this->blockedBrushRadiusTiles = std::clamp(this->blockedBrushRadiusTiles - 1, 0, 8);
        this->statusMessage = "Rayon collision: " + std::to_string((this->blockedBrushRadiusTiles * 2) + 1) + "x";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonBlockedBrushPlusRect))
    {
        if (this->editorTool == EditorTool::HOTSPOT_TOWERS)
        {
            this->selectedTowerHotspotNumber =
                ClampEditorMapTowerHotspotNumber(this->selectedTowerHotspotNumber + 1);
            this->statusMessage =
                "Tower hotspot selectionnee: "
                + std::to_string(this->selectedTowerHotspotNumber) + "/12";
            return true;
        }
        this->blockedBrushRadiusTiles = std::clamp(this->blockedBrushRadiusTiles + 1, 0, 8);
        this->statusMessage = "Rayon collision: " + std::to_string((this->blockedBrushRadiusTiles * 2) + 1) + "x";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonBlockedColorPrevRect))
    {
        const int count = static_cast<int>(kBlockedTilePalette.size());
        this->selectedBlockedColorIndex = (this->selectedBlockedColorIndex - 1 + count) % count;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonBlockedColorNextRect))
    {
        const int count = static_cast<int>(kBlockedTilePalette.size());
        this->selectedBlockedColorIndex = (this->selectedBlockedColorIndex + 1) % count;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonHotspotColorPrevRect))
    {
        const int count = static_cast<int>(kHotspotPalette.size());
        this->selectedHotspotColorIndex = (this->selectedHotspotColorIndex - 1 + count) % count;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonHotspotColorNextRect))
    {
        const int count = static_cast<int>(kHotspotPalette.size());
        this->selectedHotspotColorIndex = (this->selectedHotspotColorIndex + 1) % count;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonAssetOpacityToggleRect))
    {
        this->assetTransparencyEnabled = !this->assetTransparencyEnabled;
        this->statusMessage = this->assetTransparencyEnabled ? "Opacite assets: ON" : "Opacite assets: OFF";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonAssetOpacityMinusRect))
    {
        this->setAssetOpacityPercent(this->assetOpacityPercent - 10);
        this->statusMessage = "Opacite assets: " + std::to_string(this->assetOpacityPercent) + "%";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonAssetOpacityPlusRect))
    {
        this->setAssetOpacityPercent(this->assetOpacityPercent + 10);
        this->statusMessage = "Opacite assets: " + std::to_string(this->assetOpacityPercent) + "%";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonGridRect))
    {
        this->showGrid = !this->showGrid;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonBlockedTilesRect))
    {
        this->showBlockedTiles = !this->showBlockedTiles;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonCenterRect))
    {
        const SDL_Point centerSectorTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        camera.centerCameraOnTile(
            static_cast<float>(centerSectorTile.x),
            static_cast<float>(centerSectorTile.y),
            map,
            map.rect);
        camera.update(map, map.rect);
        this->testShipCameraFollowEnabled = false;
        this->statusMessage = "Camera recadree sur la map.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonCenterShipRect))
    {
        if (!this->testShipLoaded || !this->testShipSpawned)
        {
            this->statusMessage = "Aucun navire teste/spawn a suivre.";
            return true;
        }
        this->testShipCameraFollowEnabled = !this->testShipCameraFollowEnabled;
        const SDL_FPoint shipTile = this->testShip.getPositionTile();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, map.rect);
        camera.update(map, map.rect);
        this->statusMessage = this->testShipCameraFollowEnabled
            ? "Suivi camera navire: ON"
            : "Suivi camera navire: OFF";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonZoomOutRect))
    {
        camera.setZoomFactor(camera.getZoomFactor() - 0.05f);
        camera.update(map, map.rect);
        this->testShipCameraFollowEnabled = false;
        return true;
    }
    if (this->pointInRect(x, y, this->buttonZoomInRect))
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        camera.update(map, map.rect);
        this->testShipCameraFollowEnabled = false;
        return true;
    }
    if (this->showBottomRightLists && this->handleAssetListClick(x, y))
    {
        return true;
    }
    if (this->showBottomRightLists && this->handleShipListClick(x, y))
    {
        return true;
    }
    return false;
}
void EditorMapCreateMapScene::drawEditorHud(void) const
{
    if (this->overlayFont.sdl_font == nullptr)
    {
        return;
    }
    auto drawLine = [this](const char* text, float x, float y, RC2D_Color color) {
        RC2D_Text renderedText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);
        rc2d_graphics_drawText(&renderedText, x, y);
        rc2d_graphics_destroyText(&renderedText);
    };
    const char* toolLabel = "Collision";
    if (this->editorTool == EditorTool::PLACE_ASSETS)
    {
        toolLabel = "Pose visuel";
    }
    else if (this->editorTool == EditorTool::REMOVE_ASSETS)
    {
        toolLabel = "Suppression visuel";
    }
    else if (this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        toolLabel = "Interaction asset";
    }
    else if (this->editorTool == EditorTool::SPAWN_SHIP)
    {
        toolLabel = "Spawn navire";
    }
    else if (this->editorTool == EditorTool::CONTROL_SHIP)
    {
        toolLabel = "Control navire";
    }
    else if (this->editorTool == EditorTool::HOTSPOT_TOWERS)
    {
        toolLabel = "Hotspot tours";
    }
    const char* oceanLabel = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label;
    const bool assetInteractionToolActive =
        (this->editorTool == EditorTool::INTERACT_ASSETS);
    const bool selectedPlacedAssetActive =
        assetInteractionToolActive &&
        this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex);
    const bool assetInteractionSelectModeActive =
        assetInteractionToolActive &&
        this->assetInteractionEditMode == AssetInteractionEditMode::SELECT;
    const bool assetInteractionPaintAddModeActive =
        assetInteractionToolActive &&
        this->assetInteractionEditMode == AssetInteractionEditMode::PAINT_ADD;
    const bool assetInteractionPaintRemoveModeActive =
        assetInteractionToolActive &&
        this->assetInteractionEditMode == AssetInteractionEditMode::PAINT_REMOVE;
    const bool hotspotToolActive = (this->editorTool == EditorTool::HOTSPOT_TOWERS);
    const bool towerPreviewShowsHotspots =
        (this->towerPreviewDisplayMode == TowerPreviewDisplayMode::HOTSPOTS);
    const std::string towerViewSummary = towerPreviewShowsHotspots
        ? std::string("TUILE HOTSPOT")
        : ("NIV " + std::to_string(std::clamp(this->towerPreviewDisplayLevel, 1, 4)));
    const char* blockedBrushMinusLabel =
        hotspotToolActive ? "Tour# -" : "Block -";
    const char* blockedBrushPlusLabel =
        hotspotToolActive ? "Tour# +" : "Block +";
    const char* oceanPrevLabel = "Ocean -";
    const char* oceanNextLabel = "Ocean +";
    const std::string towerDisplayModeLabel = towerPreviewShowsHotspots
        ? std::string("AFFICHAGE TOWERS : TUILE")
        : ("AFFICHAGE TOWERS : NIV " + std::to_string(std::clamp(this->towerPreviewDisplayLevel, 1, 4)));
    const char* toolBlockLabel = assetInteractionToolActive ? "TILE GUI" : "Collision";
    const char* toolUnblockLabel = assetInteractionToolActive ? "SUPPR TILE GUI" : "Suppr Collision";
    const EditorMapAssetClickGuiTargetInfo& selectedClickGuiInfo = selectedPlacedAssetActive
        ? GetEditorMapAssetClickGuiTargetInfo(
            this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)].clickGuiTarget)
        : GetEditorMapAssetClickGuiTargetInfo(this->selectedAssetClickGuiTarget);
    const int selectedClickTileCount = selectedPlacedAssetActive
        ? static_cast<int>(this->computePlacedAssetClickInteractionTiles(
              this->placedAssets[static_cast<size_t>(this->selectedPlacedAssetIndex)]).size())
        : 0;
    this->drawToolbarButton(this->buttonImportRect, "IMPORTER ASSETS", false);
    this->drawToolbarButton(
        this->buttonListsVisibilityRect,
        this->showBottomRightLists ? "LISTES ON" : "LISTES OFF",
        this->showBottomRightLists);
    this->drawToolbarButton(this->buttonImportMapRect, "IMPORTER MAP", false);
    this->drawToolbarButton(this->buttonExportRect, "EXPORTER MAP", false);
    this->drawToolbarButton(this->buttonUndoRect, "Annuler", this->canUndoHistory());
    this->drawToolbarButton(this->buttonRedoRect, "Refaire", this->canRedoHistory());
    this->drawToolbarButton(
        this->buttonToolBlockRect,
        toolBlockLabel,
        (this->editorTool == EditorTool::BLOCK_TILES && this->collisionPaintBlocks) ||
            assetInteractionPaintAddModeActive);
    this->drawToolbarButton(
        this->buttonToolUnblockRect,
        toolUnblockLabel,
        (this->editorTool == EditorTool::BLOCK_TILES && !this->collisionPaintBlocks) ||
            assetInteractionPaintRemoveModeActive);
    this->drawToolbarButton(this->buttonToolPlaceRect, "Pose Asset", this->editorTool == EditorTool::PLACE_ASSETS);
    this->drawToolbarButton(this->buttonToolRemoveRect, "Supprimer asset", this->editorTool == EditorTool::REMOVE_ASSETS);
    this->drawToolbarButton(this->buttonToolInteractRect, "INTERACTION ASSET", assetInteractionSelectModeActive);
    this->drawToolbarButton(this->buttonToolShipRect, "SPAWN NAVIRE", this->editorTool == EditorTool::SPAWN_SHIP);
    this->drawToolbarButton(this->buttonToolShipControlRect, "CONTROL NAVIRE", this->editorTool == EditorTool::CONTROL_SHIP);
    this->drawToolbarButton(this->buttonToolHotspotRect, "HOTSPOT TOUR", this->editorTool == EditorTool::HOTSPOT_TOWERS);
    this->drawToolbarButton(
        this->buttonTowerVariantsPickerRect,
        "CHOISIR VARIANTE TOWERS",
        this->editorTool == EditorTool::HOTSPOT_TOWERS && this->towerVariantPickerVisible);
    this->drawToolbarButton(
        this->buttonTowerDisplayModeRect,
        towerDisplayModeLabel.c_str(),
        this->towerDisplayPickerVisible);
    this->drawToolbarButton(this->buttonAssetPrevRect, "Asset precedent", false);
    this->drawToolbarButton(this->buttonAssetNextRect, "Asset suivant", false);
    this->drawToolbarButton(this->buttonOceanPrevRect, oceanPrevLabel, false);
    this->drawToolbarButton(this->buttonOceanNextRect, oceanNextLabel, false);
    this->drawToolbarButton(this->buttonBlockedBrushMinusRect, blockedBrushMinusLabel, false);
    this->drawToolbarButton(this->buttonBlockedBrushPlusRect, blockedBrushPlusLabel, false);
    this->drawToolbarButton(this->buttonBlockedColorPrevRect, "BCol -", false);
    this->drawToolbarButton(this->buttonBlockedColorNextRect, "BCol +", false);
    this->drawToolbarButton(this->buttonHotspotColorPrevRect, "HCol -", false);
    this->drawToolbarButton(this->buttonHotspotColorNextRect, "HCol +", false);
    this->drawToolbarButton(this->buttonAssetOpacityToggleRect, "Opacity", this->assetTransparencyEnabled);
    this->drawToolbarButton(this->buttonAssetOpacityMinusRect, "Op -", false);
    this->drawToolbarButton(this->buttonAssetOpacityPlusRect, "Op +", false);
    this->drawToolbarButton(this->buttonGridRect, "Lignes", this->showGrid);
    this->drawToolbarButton(this->buttonCenterRect, "CENTRER MAP", false);
    this->drawToolbarButton(this->buttonCenterShipRect, "CENTRER NAVIRE", this->testShipCameraFollowEnabled);
    this->drawToolbarButton(this->buttonZoomOutRect, "Zoom-", false);
    this->drawToolbarButton(this->buttonZoomInRect, "Zoom+", false);
    this->drawToolbarButton(this->buttonBlockedTilesRect, "COLLISION MASK", this->showBlockedTiles);
    char tileHudText[128] = {};
    if (this->hoveredTileValid)
    {
        SDL_snprintf(
            tileHudText,
            sizeof(tileHudText),
            "Tile X:%d Y:%d",
            this->hoveredTile.x,
            this->hoveredTile.y);
    }
    else
    {
        SDL_snprintf(tileHudText, sizeof(tileHudText), "Tile: hors map");
    }
    const char* selectedAssetName = "Aucun";
    if (this->selectedAssetIndex >= 0 &&
        this->selectedAssetIndex < static_cast<int>(this->importedAssets.size()))
    {
        selectedAssetName = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)].displayName.c_str();
    }
    const char* selectedShipName = "Aucun";
    if (this->selectedShipIndex >= 0 &&
        this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
    {
        selectedShipName = this->importedShips[static_cast<size_t>(this->selectedShipIndex)].displayName.c_str();
    }
    char line0[1024] = {};
    SDL_snprintf(
        line0,
        sizeof(line0),
        "EDITOR MAP | Outil:%s | Ocean:%s | Grille:%s | Blocked:%s | %s",
        toolLabel,
        oceanLabel,
        this->showGrid ? "ON" : "OFF",
        this->showBlockedTiles ? "ON" : "OFF",
        tileHudText);
    char line1[1024] = {};
    SDL_snprintf(
        line1,
        sizeof(line1),
        "Status: %s",
        this->statusMessage.c_str());
    char line2[1024] = {};
    SDL_snprintf(
        line2,
        sizeof(line2),
        "Assets importes:%d | Assets poses:%d | Hotspots:%d | Selection:%s | Navires:%d | Navire actif:%s",
        static_cast<int>(this->importedAssets.size()),
        static_cast<int>(this->placedAssets.size()),
        static_cast<int>(this->towerHotspots.size()),
        selectedAssetName,
        static_cast<int>(this->importedShips.size()),
        selectedShipName);
    char line3[1024] = {};
    if (!this->testShipLoaded)
    {
        SDL_snprintf(line3, sizeof(line3), "Navire test: non charge");
    }
    else if (!this->testShipSpawned)
    {
        SDL_snprintf(line3, sizeof(line3), "Navire test: charge, en attente de spawn");
    }
    else
    {
        const SDL_FPoint shipTile = this->testShip.getPositionTile();
        SDL_snprintf(
            line3,
            sizeof(line3),
            "Navire test: X=%.2f Y=%.2f | Deplacement=%s | Suivi camera=%s",
            shipTile.x,
            shipTile.y,
            this->testShip.isMoving() ? "ON" : "OFF",
            this->testShipCameraFollowEnabled ? "ON" : "OFF");
    }
    char line4[1024] = {};
    SDL_snprintf(
        line4,
        sizeof(line4),
        "MapName:%s | BlockBrush:%dx | AssetOpacity:%d%%(%s) | ShipScale:%d%% | TowerSel:%d/12 | TowerVar:Niv%d | TowerView:%s | AssetEdit:%s | ClickGui:%s | ClickTiles:%d",
        this->mapNameInput.empty() ? "<vide>" : this->mapNameInput.c_str(),
        (this->blockedBrushRadiusTiles * 2) + 1,
        this->assetOpacityPercent,
        this->assetTransparencyEnabled ? "ON" : "OFF",
        this->shipScalePercent,
        ClampEditorMapTowerHotspotNumber(this->selectedTowerHotspotNumber),
        std::clamp(this->selectedTowerVariantLevel, 1, 4),
        towerViewSummary.c_str(),
        selectedPlacedAssetActive ? "ON" : "OFF",
        selectedClickGuiInfo.label,
        selectedClickTileCount);
    const Map& map = GetCurrentMap();
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    drawLine(line0, gameScreenRect.x + 14.0f, gameScreenRect.y + 5.0f, kHudTextColor);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(this->mapNameInputFocused ? RC2D_Color{58, 88, 122, 210} : RC2D_Color{28, 38, 50, 205});
    rc2d_graphics_rectangle("fill", &this->mapNameInputRect);
    rc2d_graphics_setColor(RC2D_Color{170, 198, 225, 235});
    rc2d_graphics_rectangle("line", &this->mapNameInputRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    std::string inputLabel = "Map Name: " + this->mapNameInput;
    if (this->mapNameInputFocused)
    {
        inputLabel += "_";
    }
    drawLine(
        inputLabel.c_str(),
        this->mapNameInputRect.x + 6.0f,
        this->mapNameInputRect.y + 2.0f,
        RC2D_Color{235, 245, 255, 250});
    char lineBottom[3072] = {};
    SDL_snprintf(
        lineBottom,
        sizeof(lineBottom),
        "%s | %s | %s | %s",
        line2,
        line1,
        line3,
        line4);
    drawLine(lineBottom, 14.0f, map.rect.y + map.rect.h - 20.0f, kHudTextColor);
    this->drawMiniMap();
    if (this->showBottomRightLists)
    {
        this->drawShipListPanel();
        this->drawAssetListPanel();
    }
    if (this->towerVariantPickerVisible)
    {
        this->drawTowerVariantPickerPopup();
    }
    if (this->towerDisplayPickerVisible)
    {
        this->drawTowerDisplayPickerPopup();
    }
}
void EditorMapCreateMapScene::onImportAssetDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapCreateMapScene* scene = static_cast<EditorMapCreateMapScene*>(userdata);
    if (scene == nullptr || scene != EditorMapCreateMapScene::activeInstance)
    {
        return;
    }

    // Ne pas charger les textures ici:
    // on enfile juste le resultat et l'update() fera l'import de facon sure.
    std::lock_guard<std::mutex> lock(scene->pendingImportMutex);
    scene->pendingImportFilePaths.clear();
    scene->pendingImportDialogCompleted = true;

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingImportDialogCanceled = true;
        return;
    }

    scene->pendingImportDialogCanceled = false;
    for (int i = 0; filelist[i] != nullptr; ++i)
    {
        scene->pendingImportFilePaths.emplace_back(filelist[i]);
    }
}


void EditorMapCreateMapScene::onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapCreateMapScene* scene = static_cast<EditorMapCreateMapScene*>(userdata);
    if (scene == nullptr || scene != EditorMapCreateMapScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingShipFolderMutex);
    scene->pendingShipFolderDialogCompleted = true;
    scene->pendingShipFolderAbsolute.clear();

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingShipFolderDialogCanceled = true;
        return;
    }

    scene->pendingShipFolderDialogCanceled = false;
    scene->pendingShipFolderAbsolute = filelist[0];
}

void EditorMapCreateMapScene::onImportMapDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapCreateMapScene* scene = static_cast<EditorMapCreateMapScene*>(userdata);
    if (scene == nullptr || scene != EditorMapCreateMapScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingMapImportMutex);
    scene->pendingMapImportDialogCompleted = true;
    scene->pendingMapImportAbsolutePath.clear();

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingMapImportDialogCanceled = true;
        return;
    }

    scene->pendingMapImportDialogCanceled = false;
    scene->pendingMapImportAbsolutePath = filelist[0];
}

void EditorMapCreateMapScene::onExportMapDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapCreateMapScene* scene = static_cast<EditorMapCreateMapScene*>(userdata);
    if (scene == nullptr || scene != EditorMapCreateMapScene::activeInstance)
    {
        return;
    }

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->statusMessage = "Export annule.";
        return;
    }

    scene->exportMapToFolder(filelist[0]);
}

void EditorMapCreateMapScene::unload(void)
{
    EditorMapSceneLayout::popBottomToolbarPlayfieldMargins();

    if (EditorMapCreateMapScene::activeInstance == this)
    {
        EditorMapCreateMapScene::activeInstance = nullptr;
    }

    GetOceanShader().unload();
    this->scrollBarOverlay.unload();
    this->clickMarker.hide();
    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();
    this->testShipLoaded = false;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        this->pendingShipFolderDialogCompleted = false;
        this->pendingShipFolderDialogCanceled = false;
        this->pendingShipFolderAbsolute.clear();
    }
    {
        std::lock_guard<std::mutex> lock(this->pendingMapImportMutex);
        this->pendingMapImportDialogCompleted = false;
        this->pendingMapImportDialogCanceled = false;
        this->pendingMapImportAbsolutePath.clear();
    }
    this->unloadImportedAssets();
    ResetStorageFontRef(&this->overlayFont);
    this->backgroundWidget.unload();

    RC2D_log(RC2D_LOG_INFO, "EditorMapCreateMapScene: unloaded");
}

void EditorMapCreateMapScene::load(void)
{
    EditorMapCreateMapScene::activeInstance = this;
    EditorMapSceneLayout::pushBottomToolbarPlayfieldMargins();
    this->resetEditorState();
    this->unloadImportedAssets();
    this->ensureUserStorageFolders();

    this->backgroundWidget.load();

    this->overlayFont = OpenStorageFont(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);
    this->scrollBarOverlay.load();

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    map.update();
    this->updateToolbarLayout();
    map.clearBlockedTiles();

    camera.setZoomFactor(0.60f);
    const SDL_Point centerSectorTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerSectorTile.x),
        static_cast<float>(centerSectorTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    this->applySelectedOceanColor();
    this->setShipScalePercent(this->shipScalePercent);
    int autoImportedShipCount = 0;
    {
        std::error_code fsError;
        const std::filesystem::path defaultShipsRoot("assets/images/ships");
        if (std::filesystem::exists(defaultShipsRoot, fsError) &&
            std::filesystem::is_directory(defaultShipsRoot, fsError))
        {
            const std::size_t previousShipCount = this->importedShips.size();
            this->importShipsFromRootFolderAbsolutePath(defaultShipsRoot.string().c_str());
            autoImportedShipCount = static_cast<int>(this->importedShips.size() - previousShipCount);
            this->editorTool = EditorTool::BLOCK_TILES;
        }
    }
    if (autoImportedShipCount > 0)
    {
        this->statusMessage =
            "Editor map charge. " + std::to_string(autoImportedShipCount) + " navire(s) detecte(s) dans assets/images/ships.";
    }
    else
    {
        this->statusMessage = "Editor map charge.";
    }

    RC2D_log(RC2D_LOG_INFO, "EditorMapCreateMapScene: loaded");
}

void EditorMapCreateMapScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    // Traite d'abord les imports differees pour rester hors pass de rendu GPU.
    this->processPendingImportRequests();
    this->processPendingShipFolderRequest();
    this->processPendingMapImportRequest();

    map.update();
    this->updateToolbarLayout();
    this->clampAssetListScrollOffset();
    this->clampShipListScrollOffset();
    this->applyPendingOceanColorStep();
    GetOceanShader().update(dt);
    this->scrollBarOverlay.update(dt, camera, map, map.rect);

    if (GameplayCameraController::updateKeyboardScroll(dt, camera, map, map.rect))
    {
        this->testShipCameraFollowEnabled = false;
    }
    if (this->scrollBarOverlay.isInteracting())
    {
        this->testShipCameraFollowEnabled = false;
    }
    this->handleMiniMapDragFromMouse();
    if (this->showBottomRightLists)
    {
        this->handleAssetListScrollDragFromMouse();
        this->handleShipListScrollDragFromMouse();
    }
    else
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
    }
    this->handleSelectedPlacedAssetInteractionPaintFromMouse();
    this->updateTestShip(dt);
    camera.update(map, map.rect);

    this->updateHoveredTile();
    this->handleTilePaintFromMouseDrag();
}

void EditorMapCreateMapScene::draw(void)
{
    Map& map = GetCurrentMap();

    this->backgroundWidget.draw();

    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    if (GetOceanShader().isReady())
    {
        GetOceanShader().draw(map.rect);
    }

    this->drawWorldGridAndBlockedTiles();
    this->drawPlacedAssets();
    this->drawTestShip();
    this->scrollBarOverlay.draw(map.rect, map);

    WorldRenderClip::end(renderer);
    this->drawEditorHud();
}
// ---------------------------------------------------------------------------
// Entrees utilisateur.
// ---------------------------------------------------------------------------
void EditorMapCreateMapScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)keycode;
    (void)keyboardID;
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    // Echap desactive dans l'editor pour eviter toute fermeture involontaire.
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->mapNameInputFocused = false;
        return;
    }
    if (this->handleMapNameInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    const bool ctrlDown = ((mod & SDL_KMOD_CTRL) != 0);
    if (!isrepeat && ctrlDown && scancode == SDL_SCANCODE_Z)
    {
        this->undoHistoryAction();
        return;
    }
    if (!isrepeat && ctrlDown && (scancode == SDL_SCANCODE_Y || ((mod & SDL_KMOD_SHIFT) != 0 && scancode == SDL_SCANCODE_Z)))
    {
        this->redoHistoryAction();
        return;
    }
    if (scancode == SDL_SCANCODE_F5 && !isrepeat)
    {
        this->openImportAssetDialog();
        return;
    }
    if (scancode == SDL_SCANCODE_F6 && !isrepeat)
    {
        this->openExportMapDialog();
        return;
    }
    if (scancode == SDL_SCANCODE_F7 && !isrepeat)
    {
        this->openImportShipFolderDialog();
        return;
    }
    if (scancode == SDL_SCANCODE_F8 && !isrepeat)
    {
        this->openImportMapDialog();
        return;
    }
    if (scancode == SDL_SCANCODE_O && !isrepeat)
    {
        const bool reverse = ((mod & SDL_KMOD_SHIFT) != 0);
        this->requestOceanColorStep(reverse ? -1 : 1);
        return;
    }
    if (scancode == SDL_SCANCODE_LEFTBRACKET && !isrepeat)
    {
        if (!this->importedAssets.empty())
        {
            this->selectedAssetIndex -= 1;
            if (this->selectedAssetIndex < 0)
            {
                this->selectedAssetIndex = static_cast<int>(this->importedAssets.size()) - 1;
            }
            this->ensureSelectedAssetVisible();
            const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
            this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        }
        else
        {
            this->statusMessage = "Aucun asset importe.";
        }
        return;
    }
    if (scancode == SDL_SCANCODE_RIGHTBRACKET && !isrepeat)
    {
        if (!this->importedAssets.empty())
        {
            this->selectedAssetIndex += 1;
            if (this->selectedAssetIndex >= static_cast<int>(this->importedAssets.size()))
            {
                this->selectedAssetIndex = 0;
            }
            this->ensureSelectedAssetVisible();
            const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
            this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        }
        else
        {
            this->statusMessage = "Aucun asset importe.";
        }
        return;
    }
    if (scancode == SDL_SCANCODE_B && !isrepeat)
    {
        this->editorTool = EditorTool::BLOCK_TILES;
        this->statusMessage = "Mode Collision: clic gauche bloque, clic droit debloque.";
        return;
    }
    if (scancode == SDL_SCANCODE_P && !isrepeat)
    {
        this->editorTool = EditorTool::PLACE_ASSETS;
        this->statusMessage = "Mode Pose visuel: clic gauche pose asset, clic droit supprime.";
        return;
    }
    if (scancode == SDL_SCANCODE_I && !isrepeat)
    {
        this->editorTool = EditorTool::INTERACT_ASSETS;
        this->assetInteractionEditMode = AssetInteractionEditMode::SELECT;
        this->statusMessage =
            "Mode Interaction asset: clique un asset pour le selectionner puis choisis AJOUT TILE ou SUPPR TILE.";
        return;
    }
    if (scancode == SDL_SCANCODE_N && !isrepeat)
    {
        this->editorTool = EditorTool::SPAWN_SHIP;
        if (!this->testShipLoaded)
        {
            this->statusMessage = "Mode SPAWN NAVIRE: importe d'abord un dossier navire.";
        }
        else
        {
            this->statusMessage = "Mode SPAWN NAVIRE: clique gauche pour spawn/re-spawn.";
        }
        return;
    }
    if (scancode == SDL_SCANCODE_V && !isrepeat)
    {
        this->editorTool = EditorTool::CONTROL_SHIP;
        if (!this->testShipLoaded)
        {
            this->statusMessage = "Mode CONTROL NAVIRE: importe d'abord un dossier navire.";
        }
        else if (!this->testShipSpawned)
        {
            this->statusMessage = "Mode CONTROL NAVIRE: navire non spawn (utilise SPAWN NAVIRE).";
        }
        else
        {
            this->statusMessage = "Mode CONTROL NAVIRE: clic gauche deplace (A*), clic droit respawn.";
        }
        return;
    }
    if (scancode == SDL_SCANCODE_T && !isrepeat)
    {
        this->editorTool = EditorTool::HOTSPOT_TOWERS;
        this->statusMessage = "Mode HOTSPOT TOUR: clique sur la tuile voulue.";
        return;
    }
    if (scancode == SDL_SCANCODE_COMMA && !isrepeat)
    {
        this->blockedBrushRadiusTiles = std::clamp(this->blockedBrushRadiusTiles - 1, 0, 8);
        return;
    }
    if (scancode == SDL_SCANCODE_PERIOD && !isrepeat)
    {
        this->blockedBrushRadiusTiles = std::clamp(this->blockedBrushRadiusTiles + 1, 0, 8);
        return;
    }
    if (scancode == SDL_SCANCODE_G && !isrepeat)
    {
        this->showGrid = !this->showGrid;
        return;
    }
    if (scancode == SDL_SCANCODE_C && !isrepeat)
    {
        const SDL_Point centerSectorTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        camera.centerCameraOnTile(
            static_cast<float>(centerSectorTile.x),
            static_cast<float>(centerSectorTile.y),
            map,
            map.rect);
        camera.update(map, map.rect);
        this->testShipCameraFollowEnabled = false;
        this->statusMessage = "Camera recadree sur la map.";
        return;
    }
    if (scancode == SDL_SCANCODE_H && !isrepeat)
    {
        if (!this->testShipLoaded || !this->testShipSpawned)
        {
            this->statusMessage = "Aucun navire teste/spawn a suivre.";
            return;
        }
        this->testShipCameraFollowEnabled = !this->testShipCameraFollowEnabled;
        const SDL_FPoint shipTile = this->testShip.getPositionTile();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, map.rect);
        camera.update(map, map.rect);
        this->statusMessage = this->testShipCameraFollowEnabled
            ? "Suivi camera navire: ON"
            : "Suivi camera navire: OFF";
        return;
    }
    if (scancode == SDL_SCANCODE_DELETE && !isrepeat)
    {
        if ((mod & SDL_KMOD_SHIFT) != 0)
        {
            this->placedAssets.clear();
            this->historyActions.clear();
            this->historyCursor = 0;
            this->statusMessage = "Assets poses supprimes (historique reset).";
        }
        else
        {
            map.clearBlockedTiles();
            this->historyActions.clear();
            this->historyCursor = 0;
            this->statusMessage = "Collisions remises a zero (historique reset).";
        }
        return;
    }
    bool cameraChanged = false;
    if (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_EQUALS)
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        cameraChanged = true;
    }
    else if (scancode == SDL_SCANCODE_KP_MINUS || scancode == SDL_SCANCODE_MINUS)
    {
        camera.setZoomFactor(camera.getZoomFactor() - 0.05f);
        cameraChanged = true;
    }
    if (cameraChanged)
    {
        this->testShipCameraFollowEnabled = false;
        camera.update(map, map.rect);
    }
}
void EditorMapCreateMapScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;
    Map& map = GetCurrentMap();
    // RC2D convertit deja les events via SDL_ConvertEventToRenderCoordinates.
    const float renderX = x;
    const float renderY = y;
    this->mapNameInputFocused = this->pointInRect(renderX, renderY, this->mapNameInputRect);
    if (this->mapNameInputFocused)
    {
        return;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
        this->assetInteractionPaintActive = false;
        this->lastAssetInteractionPaintTileValid = false;
    }
    if (this->handleTowerVariantPickerClick(renderX, renderY))
    {
        return;
    }
    if (this->handleTowerDisplayPickerClick(renderX, renderY))
    {
        return;
    }
    if (this->handleToolbarClick(renderX, renderY))
    {
        return;
    }
    if (this->handleMiniMapClick(renderX, renderY, button))
    {
        return;
    }
    if (!this->isInsideMapRect(renderX, renderY))
    {
        return;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT && this->scrollBarOverlay.handleClick(renderX, renderY, map.rect))
    {
        this->testShipCameraFollowEnabled = false;
        return;
    }
    if (this->handleShipToolClick(renderX, renderY, button))
    {
        return;
    }
    if (this->editorTool == EditorTool::BLOCK_TILES)
    {
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            this->paintTileAtMouse(this->collisionPaintBlocks);
        }
        else if (button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            this->paintTileAtMouse(!this->collisionPaintBlocks);
        }
        return;
    }
    if (this->editorTool == EditorTool::PLACE_ASSETS)
    {
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            this->placeSelectedAssetAtMouseTile();
        }
        else if (button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            this->removeAssetAtMouseTile();
        }
        return;
    }
    if (this->editorTool == EditorTool::REMOVE_ASSETS)
    {
        if (button == RC2D_MOUSE_BUTTON_LEFT || button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            this->removeAssetAtMouseTile();
        }
        return;
    }
    if (this->editorTool == EditorTool::INTERACT_ASSETS)
    {
        if (this->assetInteractionEditMode == AssetInteractionEditMode::SELECT)
        {
            if (button == RC2D_MOUSE_BUTTON_RIGHT)
            {
                this->clearSelectedPlacedAsset();
                this->statusMessage = "Selection interaction asset effacee.";
                return;
            }
            if (button != RC2D_MOUSE_BUTTON_LEFT)
            {
                return;
            }
            if (this->selectPlacedAssetAtScreenPoint(renderX, renderY))
            {
                return;
            }
            this->statusMessage = "Clique un asset pose pour l'editer.";
            return;
        }

        if (button != RC2D_MOUSE_BUTTON_LEFT)
        {
            return;
        }

        if (!this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex))
        {
            this->statusMessage = "Selectionne d'abord un asset a regler.";
            return;
        }

        const SDL_Point targetTile = map.screenToTileNearest(renderX, renderY);
        if (!map.isInside(targetTile.x, targetTile.y))
        {
            this->statusMessage = "Cible interaction hors map.";
            return;
        }

        this->assetInteractionPaintValue =
            (this->assetInteractionEditMode != AssetInteractionEditMode::PAINT_REMOVE);
        this->assetInteractionPaintActive = true;
        this->lastAssetInteractionPaintTileValid = false;
        this->paintSelectedPlacedAssetInteractionAtMouse(this->assetInteractionPaintValue);
        return;
    }
    if (this->editorTool == EditorTool::HOTSPOT_TOWERS)
    {
        if (button != RC2D_MOUSE_BUTTON_LEFT)
        {
            return;
        }
        const SDL_Point tile = map.screenToTileNearest(renderX, renderY);
        if (!map.isInside(tile.x, tile.y))
        {
            this->statusMessage = "Hotspot hors map: ignore.";
            return;
        }
        this->toggleTowerHotspotAtTile(tile.x, tile.y);
        return;
    }
}

void EditorMapCreateMapScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)integer_x;
    (void)mouseID;

    int delta = static_cast<int>(integer_y);
    if (delta == 0)
    {
        if (direction == RC2D_SCROLL_UP)
        {
            delta = 1;
        }
        else if (direction == RC2D_SCROLL_DOWN)
        {
            delta = -1;
        }
    }
    if (delta == 0)
    {
        return;
    }

    float renderX = x;
    float renderY = y;
    if (std::isfinite(mouse_x) && std::isfinite(mouse_y))
    {
        this->convertWindowToRender(mouse_x, mouse_y, &renderX, &renderY);
    }
    else
    {
        (void)this->getMouseRenderPosition(&renderX, &renderY);
    }
    const int step = (std::max)(1, std::abs(delta));

    if (this->showBottomRightLists && this->pointInRect(renderX, renderY, this->shipListRect))
    {
        this->shipListScrollOffset += (delta > 0) ? -step : step;
        this->clampShipListScrollOffset();
        return;
    }
    if (this->towerVariantPickerVisible && this->pointInRect(renderX, renderY, this->towerVariantPickerRect))
    {
        this->assetListScrollOffset += (delta > 0) ? -step : step;
        this->clampAssetListScrollOffset();
        return;
    }
    if (this->towerDisplayPickerVisible && this->pointInRect(renderX, renderY, this->towerDisplayPickerRect))
    {
        return;
    }
    if (this->showBottomRightLists && this->pointInRect(renderX, renderY, this->assetListRect))
    {
        if (this->isPlacedAssetIndexValid(this->selectedPlacedAssetIndex) &&
            this->editorTool == EditorTool::INTERACT_ASSETS)
        {
            this->assetInteractionListScrollOffset += (delta > 0) ? -step : step;
        }
        else
        {
            this->assetListScrollOffset += (delta > 0) ? -step : step;
        }
        this->clampAssetListScrollOffset();
        return;
    }

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    camera.setZoomFactor(camera.getZoomFactor() + ((delta > 0) ? 0.05f : -0.05f));
    camera.update(map, map.rect);
    this->testShipCameraFollowEnabled = false;
}
#endif // GAME_ENV_DEV
