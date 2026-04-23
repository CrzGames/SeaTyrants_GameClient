
#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-shipdownscale.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <vector>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_storage.h>
#include <SDL3/SDL_surface.h>
#include <cJSON.h>

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
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
constexpr int kSimulationNetworkCommandDistanceTiles = 14;
constexpr int kSimulationNetworkCommandFallbackMinDistanceTiles = 8;
constexpr double kSimulationPauseAfterArrivalSec = 0.22;
constexpr double kSimulationRetargetTickPeriodSec = 0.08;
constexpr float kSimulationDesiredMovingRatio = 0.90f;
constexpr int kSimulationRetargetBudgetPerTickMin = 10;
constexpr int kSimulationRetargetBudgetPerTickMax = 64;
constexpr int kSimulationMaxMovingShipsHardCap = 3200;
// Aligne le runtime sur la philosophie "crashtest": limiter le nombre
// de navires actifs pour eviter les explosions CPU/RAM quand des milliers
// de dossiers sont detectes dans assets/images/ships.
constexpr std::size_t kSimulationRuntimeShipsCap = 500U;

constexpr std::array<SDL_FPoint, 8> kSimulationClickDirections = {{
    SDL_FPoint{0.0f, -1.0f},  // haut
    SDL_FPoint{-1.0f, -1.0f}, // haut-gauche
    SDL_FPoint{-1.0f, 0.0f},  // gauche
    SDL_FPoint{-1.0f, 1.0f},  // bas-gauche
    SDL_FPoint{0.0f, 1.0f},   // bas
    SDL_FPoint{1.0f, 1.0f},   // bas-droite
    SDL_FPoint{1.0f, 0.0f},   // droite
    SDL_FPoint{1.0f, -1.0f},  // haut-droite
}};

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
constexpr std::size_t kDeferredShipPreviewLoadsPerFrame = 2U;
constexpr std::size_t kDeferredShipPreviewLoadsPerFrameSimulation = 2U;
constexpr std::size_t kDeferredAssetLoadsPerFrame = 2U;
constexpr std::size_t kDeferredAssetLoadsPerFrameDuringImport = 6U;

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

static char extractDigitFromKeyLabel(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    if (key[1] == '\0' && key[0] >= '0' && key[0] <= '9')
    {
        return key[0];
    }

    std::string lower(key);
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool looksLikeKeypad =
        (lower.find("kp") != std::string::npos) ||
        (lower.find("keypad") != std::string::npos) ||
        (lower.find("numpad") != std::string::npos);
    if (!looksLikeKeypad)
    {
        return '\0';
    }

    for (const char c : lower)
    {
        if (c >= '0' && c <= '9')
        {
            return c;
        }
    }

    return '\0';
}


static std::string normalizePathSlashes(const std::string& path)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

static uint32_t randomNextU32(uint32_t* state)
{
    if (state == nullptr)
    {
        return 0U;
    }

    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}

static float randomNext01(uint32_t* state)
{
    const uint32_t value = randomNextU32(state);
    return static_cast<float>((value >> 8) & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

static int randomIntInclusive(uint32_t* state, int minValue, int maxValue)
{
    if (minValue >= maxValue)
    {
        return minValue;
    }

    const float t = randomNext01(state);
    const float span = static_cast<float>((maxValue - minValue) + 1);
    const int value = minValue + static_cast<int>(std::floor(t * span));
    return std::clamp(value, minValue, maxValue);
}

static SDL_Point simulationDirectionToTileOffset(
    const SDL_FPoint& clickDirection,
    int clickDistanceTiles)
{
    if (clickDirection.x > 0.0f && clickDirection.y < 0.0f)
    {
        return SDL_Point{0, -clickDistanceTiles};
    }
    if (clickDirection.x > 0.0f && clickDirection.y > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, 0};
    }
    if (clickDirection.x < 0.0f && clickDirection.y > 0.0f)
    {
        return SDL_Point{0, clickDistanceTiles};
    }
    if (clickDirection.x < 0.0f && clickDirection.y < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, 0};
    }
    if (clickDirection.x > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, -clickDistanceTiles};
    }
    if (clickDirection.x < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, clickDistanceTiles};
    }
    if (clickDirection.y < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, -clickDistanceTiles};
    }
    if (clickDirection.y > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, clickDistanceTiles};
    }

    return SDL_Point{0, 0};
}

static int simulationPreviewDirectionToSpriteIndex(
    Ship::PreviewDirection direction,
    bool lowHp)
{
    const int baseSpriteIndex = lowHp ? 5 : 1;
    switch (direction)
    {
    case Ship::PreviewDirection::UP_RIGHT:
        return baseSpriteIndex + 1;
    case Ship::PreviewDirection::UP_LEFT:
        return baseSpriteIndex + 2;
    case Ship::PreviewDirection::DOWN_RIGHT:
        return baseSpriteIndex + 3;
    case Ship::PreviewDirection::DOWN_LEFT:
    default:
        return baseSpriteIndex;
    }
}

static bool pickSimulationSpawnTile(
    const Map& map,
    uint32_t* rngState,
    SDL_Point* outTile)
{
    if (outTile == nullptr || map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return false;
    }

    constexpr int kRandomSpawnAttempts = 64;
    for (int attempt = 0; attempt < kRandomSpawnAttempts; ++attempt)
    {
        const int sectorX = randomIntInclusive(rngState, 0, Map::NUM_SECTORS_X - 1);
        const int sectorY = randomIntInclusive(rngState, 0, Map::NUM_SECTORS_Y - 1);
        SDL_Point candidate = map.sectorToTile(sectorX, sectorY);
        candidate.x += randomIntInclusive(rngState, -2, 2);
        candidate.y += randomIntInclusive(rngState, -2, 2);
        candidate = map.clampTile(candidate.x, candidate.y);
        if (!map.isTileBlocked(candidate.x, candidate.y))
        {
            *outTile = candidate;
            return true;
        }
    }

    const int mapWidthTiles = map.getWidthTiles();
    const int mapHeightTiles = map.getHeightTiles();
    const int totalTiles = mapWidthTiles * mapHeightTiles;
    if (totalTiles <= 0)
    {
        return false;
    }

    const int startIndex = randomIntInclusive(rngState, 0, totalTiles - 1);
    for (int offset = 0; offset < totalTiles; ++offset)
    {
        const int tileIndex = (startIndex + offset) % totalTiles;
        const int tileX = tileIndex % mapWidthTiles;
        const int tileY = tileIndex / mapWidthTiles;
        if (!map.isTileBlocked(tileX, tileY))
        {
            *outTile = SDL_Point{tileX, tileY};
            return true;
        }
    }

    return false;
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

EditorMapShipDownscaleScene* EditorMapShipDownscaleScene::activeInstance = nullptr;

// ---------------------------------------------------------------------------
// Cycle de vie de la scene.
// ---------------------------------------------------------------------------
EditorMapShipDownscaleScene::EditorMapShipDownscaleScene(void)
    : backgroundWidget{},
      overlayFont{},
      scrollBarOverlay{},
      editorMode(EditorMode::MAP_CREATOR_MAP),
      editorTool(EditorTool::PLACE_ASSETS),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      selectedAssetIndex(-1),
      selectedShipIndex(-1),
      assetListScrollOffset(0),
      shipListScrollOffset(0),
      showGrid(false),
      showBlockedTiles(false),
      showBottomRightLists(true),
      collisionPaintBlocks(true),
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
      shipImportBatchActive(false),
      shipImportBatchFolders{},
      shipImportBatchNextIndex(0),
      shipImportBatchImportedCount(0),
      shipImportBatchFailedCount(0),
      spawnedShips{},
      importedShipPreviewCounter(0U),
      importedShipsLayoutDirty(false),
      shipsSimulationEnabled(false),
      shipSpawnPlacementEnabled(false),
      shipsLowHpEnabled(false),
      shipsPreviewAnimationAccumulator(0.0),
      shipsPreviewAnimationIntervalSeconds((1.0 / 60.0) * 3.0),
      shipsPreviewFrameOffset(0),
      shipsSimulationRetargetCursor(0U),
      shipsSimulationRetargetAccumulatorSec(0.0),
      deferredShipPreviewLoadCursor(0U),
      deferredAssetLoadCursor(0U),
      initialShipsLoadingScreenActive(false),
      initialShipsLoadingTotal(0),
      initialShipsLoadingProcessed(0),
      shipImportBatchFramesUntilNextShip(0),
      shipScaleInputActive(false),
      shipScaleInputBuffer(),
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
      buttonExportRect{},
      buttonUndoRect{},
      buttonRedoRect{},
      buttonToolPlaceRect{},
      buttonToolRemoveRect{},
      buttonAssetPrevRect{},
      buttonAssetNextRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonGridRect{},
      buttonCenterRect{},
      buttonZoomOutRect{},
      buttonZoomInRect{},
      buttonShipScaleMinusRect{},
      buttonShipScalePlusRect{},
      buttonShipSpawnRect{},
      buttonSimulationRect{},
      buttonShipsHpRect{},
      buttonListsVisibilityRect{},
      shipScaleInputRect{},
      assetListRect{},
      shipListRect{},
      miniMapRect{},
      miniMapDragActive(false),
      miniMapDragOffsetX(0.0f),
      miniMapDragOffsetY(0.0f)
{
}

EditorMapShipDownscaleScene::~EditorMapShipDownscaleScene(void)
{
}

void EditorMapShipDownscaleScene::resetEditorState(void)
{
    this->editorMode = EditorMode::MAP_CREATOR_MAP;
    this->editorTool = EditorTool::PLACE_ASSETS;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->selectedAssetIndex = -1;
    this->selectedShipIndex = -1;
    this->assetListScrollOffset = 0;
    this->shipListScrollOffset = 0;
    this->showGrid = false;
    this->showBlockedTiles = false;
    this->showBottomRightLists = true;
    this->collisionPaintBlocks = true;
    this->blockedBrushRadiusTiles = 0;
    this->assetTransparencyEnabled = true;
    this->assetOpacityPercent = 100;
    this->selectedBlockedColorIndex = 0;
    this->selectedHotspotColorIndex = 0;
    this->shipScalePercent = 100;
    this->hoveredTileValid = false;
    this->dragPaintActive = false;
    this->dragPaintBlockedValue = true;
    this->lastDragPaintTileValid = false;
    this->historyActions.clear();
    this->historyCursor = 0;
    this->towerHotspots.clear();
    this->unloadImportedShips();
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
    this->shipImportBatchActive = false;
    this->shipImportBatchFolders.clear();
    this->shipImportBatchNextIndex = 0;
    this->shipImportBatchImportedCount = 0;
    this->shipImportBatchFailedCount = 0;
    this->spawnedShips.clear();
    this->importedShipPreviewCounter = 0U;
    this->importedShipsLayoutDirty = false;
    this->shipsSimulationEnabled = false;
    this->shipSpawnPlacementEnabled = false;
    this->shipsLowHpEnabled = false;
    this->shipsPreviewAnimationAccumulator = 0.0;
    this->shipsPreviewAnimationIntervalSeconds = (1.0 / 60.0) * 3.0;
    this->shipsPreviewFrameOffset = 0;
    this->shipsSimulationRetargetCursor = 0U;
    this->shipsSimulationRetargetAccumulatorSec = 0.0;
    this->deferredShipPreviewLoadCursor = 0U;
    this->deferredAssetLoadCursor = 0U;
    this->initialShipsLoadingScreenActive = false;
    this->initialShipsLoadingTotal = 0;
    this->initialShipsLoadingProcessed = 0;
    this->shipImportBatchFramesUntilNextShip = 0;
    this->shipScaleInputActive = false;
    this->shipScaleInputBuffer.clear();
    this->assetListScrollDragActive = false;
    this->assetListScrollDragGrabOffsetY = 0.0f;
    this->shipListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->clickMarker.hide();
    this->clickMarker.setDurationSeconds(0.85);
    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();
    this->testShipLoaded = false;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->initialShipsLoadingScreenActive = false;
    this->initialShipsLoadingTotal = 0;
    this->initialShipsLoadingProcessed = 0;
    this->shipImportBatchFramesUntilNextShip = 0;
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
    this->buttonExportRect = SDL_FRect{};
    this->buttonUndoRect = SDL_FRect{};
    this->buttonRedoRect = SDL_FRect{};
    this->buttonToolPlaceRect = SDL_FRect{};
    this->buttonToolRemoveRect = SDL_FRect{};
    this->buttonAssetPrevRect = SDL_FRect{};
    this->buttonAssetNextRect = SDL_FRect{};
    this->buttonOceanPrevRect = SDL_FRect{};
    this->buttonOceanNextRect = SDL_FRect{};
    this->buttonGridRect = SDL_FRect{};
    this->buttonCenterRect = SDL_FRect{};
    this->buttonZoomOutRect = SDL_FRect{};
    this->buttonZoomInRect = SDL_FRect{};
    this->buttonShipScaleMinusRect = SDL_FRect{};
    this->buttonShipScalePlusRect = SDL_FRect{};
    this->buttonShipSpawnRect = SDL_FRect{};
    this->buttonSimulationRect = SDL_FRect{};
    this->buttonShipsHpRect = SDL_FRect{};
    this->buttonListsVisibilityRect = SDL_FRect{};
    this->shipScaleInputRect = SDL_FRect{};
    this->assetListRect = SDL_FRect{};
    this->shipListRect = SDL_FRect{};
    this->miniMapRect = SDL_FRect{};
    this->miniMapDragActive = false;
    this->miniMapDragOffsetX = 0.0f;
    this->miniMapDragOffsetY = 0.0f;
}

void EditorMapShipDownscaleScene::unloadImportedAssets(void)
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
    this->assetListScrollDragActive = false;
    this->assetListScrollDragGrabOffsetY = 0.0f;
    this->deferredAssetLoadCursor = 0U;
}

void EditorMapShipDownscaleScene::unloadImportedShips(void)
{
    for (ImportedShip& ship : this->importedShips)
    {
        for (int i = 0; i < kShipSpriteCount; ++i)
        {
            if (ship.previewUseTitleStorage)
            {
                ResetStorageImageDataRef(&ship.previewImageData[static_cast<size_t>(i)]);
                ResetStorageImageRef(&ship.previewImages[static_cast<size_t>(i)]);
            }
            else
            {
                ReleaseStorageImageData(&ship.previewImageData[static_cast<size_t>(i)]);
                ReleaseStorageImage(&ship.previewImages[static_cast<size_t>(i)]);
            }
            ship.previewSpriteLoaded[static_cast<size_t>(i)] = false;
        }
        ship.simulationShip.reset();
    }

    this->importedShips.clear();
    this->spawnedShips.clear();
    this->selectedShipIndex = -1;
    this->shipListScrollOffset = 0;
    this->shipScalePercent = 100;
    this->shipListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->importedShipsLayoutDirty = false;
    this->deferredShipPreviewLoadCursor = 0U;
    this->shipsSimulationRetargetCursor = 0U;
    this->shipsSimulationRetargetAccumulatorSec = 0.0;
}


void EditorMapShipDownscaleScene::ensureUserStorageFolders(void)
{
    rc2d_storage_userMkdir("editor-assets");
    rc2d_storage_userMkdir("editor-map-ship");
    rc2d_storage_userMkdir("editor-map-ship/current");
    rc2d_storage_userMkdir("editor-map-shipdownscale");
    rc2d_storage_userMkdir("editor-map-shipdownscale/preview");
    rc2d_storage_userMkdir("editor-map-shipdownscale/tmp");
}

void EditorMapShipDownscaleScene::applySelectedOceanColor(void)
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
        RC2D_log(RC2D_LOG_WARN, "EditorMapShipDownscaleScene: echec chargement ocean shader (%s)", entry.label);
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapShipDownscaleScene::requestOceanColorStep(int delta)
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

void EditorMapShipDownscaleScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapShipDownscaleScene::cycleOceanColor(int delta)
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

void EditorMapShipDownscaleScene::convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const
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

bool EditorMapShipDownscaleScene::isInsideMapRect(float x, float y) const
{
    const Map& map = GetCurrentMap();
    return (
        x >= map.rect.x &&
        x <= (map.rect.x + map.rect.w) &&
        y >= map.rect.y &&
        y <= (map.rect.y + map.rect.h));
}

bool EditorMapShipDownscaleScene::getMouseRenderPosition(float* outX, float* outY) const
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

bool EditorMapShipDownscaleScene::tryGetMouseTile(SDL_Point* outTile) const
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

void EditorMapShipDownscaleScene::updateHoveredTile(void)
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


int EditorMapShipDownscaleScene::findPlacedAssetIndexAtTile(int tileX, int tileY) const
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

bool EditorMapShipDownscaleScene::getPlacedAssetAtTile(int tileX, int tileY, PlacedAsset* outAsset) const
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

void EditorMapShipDownscaleScene::setPlacedAssetStateAtTile(int tileX, int tileY, bool hasAsset, const PlacedAsset* assetState)
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
                return;
            }
        }

        // Fallback: suppression a la tuile.
        if (existingIndex >= 0)
        {
            this->placedAssets.erase(this->placedAssets.begin() + existingIndex);
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
        return;
    }

    this->placedAssets.push_back(nextAsset);
}

bool EditorMapShipDownscaleScene::canUndoHistory(void) const
{
    return (this->historyCursor > 0);
}

bool EditorMapShipDownscaleScene::canRedoHistory(void) const
{
    return (this->historyCursor < static_cast<int>(this->historyActions.size()));
}

void EditorMapShipDownscaleScene::pushHistoryAction(const HistoryAction& action)
{
    if (this->historyCursor < static_cast<int>(this->historyActions.size()))
    {
        this->historyActions.erase(this->historyActions.begin() + this->historyCursor, this->historyActions.end());
    }

    this->historyActions.push_back(action);
    this->historyCursor = static_cast<int>(this->historyActions.size());
}

void EditorMapShipDownscaleScene::applyHistoryAction(const HistoryAction& action, bool applyAfter)
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
}

void EditorMapShipDownscaleScene::undoHistoryAction(void)
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

void EditorMapShipDownscaleScene::redoHistoryAction(void)
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

bool EditorMapShipDownscaleScene::setTileBlockedWithHistory(int tileX, int tileY, bool blocked)
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

bool EditorMapShipDownscaleScene::applyTileBrushWithHistory(int centerTileX, int centerTileY, bool blocked)
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

void EditorMapShipDownscaleScene::paintTileAtMouse(bool blocked)
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
void EditorMapShipDownscaleScene::handleTilePaintFromMouseDrag(void)
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

void EditorMapShipDownscaleScene::placeSelectedAssetAtMouseTile(void)
{
    if (this->selectedAssetIndex < 0 ||
        this->selectedAssetIndex >= static_cast<int>(this->importedAssets.size()))
    {
        this->statusMessage = "Aucun asset selectionne.";
        return;
    }
    ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
    if (!this->ensureImportedAssetLoaded(&selectedAsset))
    {
        this->statusMessage = "Asset indisponible (chargement lazy en cours/KO).";
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

            HistoryAction action{};
            action.type = HistoryAction::Type::ASSET_AT_TILE;
            action.tileX = tile.x;
            action.tileY = tile.y;
            action.hadBeforeAsset = true;
            action.hadAfterAsset = true;
            action.beforeAsset = beforeAsset;
            action.afterAsset = placedAsset;
            this->pushHistoryAction(action);

            this->statusMessage = "Asset remplace.";
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
    this->placedAssets.push_back(placedAsset);

    HistoryAction action{};
    action.type = HistoryAction::Type::ASSET_AT_TILE;
    action.tileX = tile.x;
    action.tileY = tile.y;
    action.hadBeforeAsset = false;
    action.hadAfterAsset = true;
    action.afterAsset = placedAsset;
    this->pushHistoryAction(action);

    this->statusMessage = "Asset pose.";
}

void EditorMapShipDownscaleScene::removeAssetAtMouseTile(void)
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
            return assetA.tileY < assetB.tileY;
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
    this->pushHistoryAction(action);
    this->statusMessage = "Asset supprime.";
}




bool EditorMapShipDownscaleScene::importAssetFromAbsolutePath(const char* absolutePath)
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

    ImportedAsset importedAsset{};
    importedAsset.id = "asset_" + std::to_string(this->importedAssetCounter);
    importedAsset.displayName = fileName.empty() ? importedAsset.id : fileName;
    importedAsset.sourcePath = sourcePath;
    importedAsset.storagePath = storagePath;
    importedAsset.useUserStorage = true;
    importedAsset.loadFailed = false;
    importedAsset.image = RC2D_Image{};
    importedAsset.imageData = RC2D_ImageData{};
    importedAsset.widthPx = 0.0f;
    importedAsset.heightPx = 0.0f;
    importedAsset.alphaMaskWidth = 0;
    importedAsset.alphaMaskHeight = 0;
    importedAsset.alphaMask.clear();
    this->importedAssets.push_back(importedAsset);

    this->selectedAssetIndex = static_cast<int>(this->importedAssets.size()) - 1;
    this->ensureSelectedAssetVisible();
    this->statusMessage = "Asset importe (chargement lazy): " + importedAsset.displayName;
    return true;
}

bool EditorMapShipDownscaleScene::ensureImportedAssetLoaded(ImportedAsset* asset)
{
    if (asset == nullptr)
    {
        return false;
    }
    if (asset->loadFailed)
    {
        return false;
    }
    if (asset->image.sdl_texture != nullptr &&
        asset->imageData.sdl_surface != nullptr &&
        asset->alphaMaskWidth > 0 &&
        asset->alphaMaskHeight > 0 &&
        !asset->alphaMask.empty())
    {
        return true;
    }

    const RC2D_StorageKind storageType = asset->useUserStorage
        ? RC2D_STORAGE_USER
        : RC2D_STORAGE_TITLE;
    const char* storagePath = asset->storagePath.c_str();

    if (asset->useUserStorage)
    {
        ReleaseStorageImageData(&asset->imageData);
        ReleaseStorageImage(&asset->image);
    }
    else
    {
        ResetStorageImageDataRef(&asset->imageData);
        ResetStorageImageRef(&asset->image);
    }
    asset->widthPx = 0.0f;
    asset->heightPx = 0.0f;
    asset->alphaMaskWidth = 0;
    asset->alphaMaskHeight = 0;
    asset->alphaMask.clear();

    asset->image = LoadStorageImage(storagePath, storageType);
    if (asset->image.sdl_texture == nullptr)
    {
        asset->loadFailed = true;
        return false;
    }
    SDL_SetTextureScaleMode(asset->image.sdl_texture, SDL_SCALEMODE_LINEAR);

    if (!SDL_GetTextureSize(asset->image.sdl_texture, &asset->widthPx, &asset->heightPx))
    {
        asset->widthPx = 0.0f;
        asset->heightPx = 0.0f;
    }

    asset->imageData = LoadStorageImageData(storagePath, storageType);
    if (asset->imageData.sdl_surface == nullptr)
    {
        if (asset->useUserStorage)
        {
            ReleaseStorageImage(&asset->image);
        }
        else
        {
            ResetStorageImageRef(&asset->image);
        }
        asset->loadFailed = true;
        return false;
    }

    asset->alphaMaskWidth = asset->imageData.sdl_surface->w;
    asset->alphaMaskHeight = asset->imageData.sdl_surface->h;
    if (asset->alphaMaskWidth <= 0 || asset->alphaMaskHeight <= 0)
    {
        if (asset->useUserStorage)
        {
            ReleaseStorageImageData(&asset->imageData);
            ReleaseStorageImage(&asset->image);
        }
        else
        {
            ResetStorageImageDataRef(&asset->imageData);
            ResetStorageImageRef(&asset->image);
        }
        asset->loadFailed = true;
        return false;
    }

    asset->alphaMask.assign(
        static_cast<size_t>(asset->alphaMaskWidth * asset->alphaMaskHeight),
        static_cast<Uint8>(0));
    for (int y = 0; y < asset->alphaMaskHeight; ++y)
    {
        for (int x = 0; x < asset->alphaMaskWidth; ++x)
        {
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 0;
            if (!SDL_ReadSurfacePixel(asset->imageData.sdl_surface, x, y, &r, &g, &b, &a))
            {
                continue;
            }
            asset->alphaMask[static_cast<size_t>((y * asset->alphaMaskWidth) + x)] = a;
        }
    }

    return true;
}

void EditorMapShipDownscaleScene::processDeferredAssetLoads(void)
{
    if (this->importedAssets.empty())
    {
        return;
    }

    std::size_t remainingBudget = this->importBatchActive
        ? kDeferredAssetLoadsPerFrameDuringImport
        : kDeferredAssetLoadsPerFrame;
    if (remainingBudget == 0U)
    {
        return;
    }

    if (this->selectedAssetIndex >= 0 &&
        this->selectedAssetIndex < static_cast<int>(this->importedAssets.size()))
    {
        ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        if (selectedAsset.image.sdl_texture == nullptr && !selectedAsset.loadFailed)
        {
            if (this->ensureImportedAssetLoaded(&selectedAsset))
            {
                remainingBudget -= 1U;
            }
            else
            {
                remainingBudget = (remainingBudget > 0U) ? (remainingBudget - 1U) : 0U;
            }
        }
    }

    const size_t assetCount = this->importedAssets.size();
    if (assetCount == 0U || remainingBudget == 0U)
    {
        return;
    }

    size_t scannedAssets = 0U;
    while (scannedAssets < assetCount && remainingBudget > 0U)
    {
        const size_t assetIndex =
            (this->deferredAssetLoadCursor + scannedAssets) % assetCount;
        ImportedAsset& importedAsset = this->importedAssets[assetIndex];
        scannedAssets += 1U;

        if (importedAsset.image.sdl_texture != nullptr || importedAsset.loadFailed)
        {
            continue;
        }

        (void)this->ensureImportedAssetLoaded(&importedAsset);
        remainingBudget -= 1U;
    }

    this->deferredAssetLoadCursor =
        (this->deferredAssetLoadCursor + scannedAssets) % assetCount;
}



int EditorMapShipDownscaleScene::importAssetFromRuntimeStoragePath(const std::string& runtimePath)
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

    ImportedAsset importedAsset{};
    ++this->importedAssetCounter;
    importedAsset.id = "asset_runtime_" + std::to_string(this->importedAssetCounter);
    importedAsset.displayName = extractFileName(normalizedPath);
    importedAsset.sourcePath = normalizedPath;
    importedAsset.storagePath = normalizedPath;
    importedAsset.useUserStorage = false;
    importedAsset.loadFailed = false;
    importedAsset.image = RC2D_Image{};
    importedAsset.imageData = RC2D_ImageData{};
    importedAsset.widthPx = 0.0f;
    importedAsset.heightPx = 0.0f;
    importedAsset.alphaMaskWidth = 0;
    importedAsset.alphaMaskHeight = 0;
    importedAsset.alphaMask.clear();
    this->importedAssets.push_back(importedAsset);
    ImportedAsset& insertedAsset = this->importedAssets.back();
    (void)this->ensureImportedAssetLoaded(&insertedAsset);
    return static_cast<int>(this->importedAssets.size()) - 1;
}


bool EditorMapShipDownscaleScene::isTowerAssetName(const std::string& displayName) const
{
    std::string lower = displayName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (lower.find("tower") != std::string::npos || lower.find("tour") != std::string::npos);
}

SDL_Point EditorMapShipDownscaleScene::computePlacedAssetCenterTile(const PlacedAsset& asset) const
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

void EditorMapShipDownscaleScene::toggleTowerHotspotAtTile(int tileX, int tileY)
{
    for (size_t i = 0; i < this->towerHotspots.size(); ++i)
    {
        if (this->towerHotspots[i].tileX == tileX && this->towerHotspots[i].tileY == tileY)
        {
            this->towerHotspots.erase(this->towerHotspots.begin() + static_cast<std::ptrdiff_t>(i));
            this->statusMessage = "Hotspot tour retire.";
            return;
        }
    }

    this->towerHotspots.push_back(TowerHotspot{tileX, tileY});
    this->statusMessage = "Hotspot tour ajoute.";
}

void EditorMapShipDownscaleScene::setAssetOpacityPercent(int value)
{
    this->assetOpacityPercent = std::clamp(value, 10, 100);
}

void EditorMapShipDownscaleScene::setShipScalePercent(int value)
{
    this->shipScalePercent = std::clamp(value, 5, 100);
    if (!this->shipScaleInputActive)
    {
        this->shipScaleInputBuffer = std::to_string(this->shipScalePercent);
    }
    if (this->selectedShipIndex >= 0 &&
        this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
    {
        ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
        selectedShip.scalePercent = this->shipScalePercent;
        if (selectedShip.simulationShip != nullptr)
        {
            selectedShip.simulationShip->setDrawScale(
                static_cast<float>(selectedShip.scalePercent) / 100.0f);
        }
        if (!this->shipsSimulationEnabled)
        {
            this->importedShipsLayoutDirty = true;
        }

        for (SpawnedShipInstance& spawnedShip : this->spawnedShips)
        {
            if (spawnedShip.importedShipIndex == this->selectedShipIndex)
            {
                spawnedShip.scalePercent = this->shipScalePercent;
            }
        }
    }

    const float scale = static_cast<float>(this->shipScalePercent) / 100.0f;
    this->testShip.setDrawScale(scale);
    this->testShipPreview.setDrawScale(scale);
}

int EditorMapShipDownscaleScene::findPlacedAssetIndexAtScreenPoint(float x, float y) const
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
        return assetA.tileY < assetB.tileY;
    });
    return static_cast<int>(candidates.back().first);
}

void EditorMapShipDownscaleScene::processPendingImportRequests(void)
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


bool EditorMapShipDownscaleScene::renderStyledMiniMapToSurface(SDL_Surface* targetSurface) const
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
            return assetA.tileY < assetB.tileY;
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

bool EditorMapShipDownscaleScene::exportMiniMapPngFromJsonPath(
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
            "EditorMapShipDownscaleScene: SDL_CreateSurface minimap export KO: %s",
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
            "EditorMapShipDownscaleScene: export minimap PNG KO '%s': %s",
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

bool EditorMapShipDownscaleScene::exportMapToAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Export annule.";
        return false;
    }

    const Map& map = GetCurrentMap();
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON.";
        return false;
    }

    cJSON_AddStringToObject(root, "mapName", "shipdownscale-export");
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
        cJSON_AddItemToArray(placedAssetsArray, placedItem);
    }
    cJSON* towerHotspotsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "towerHotspots", towerHotspotsArray);
    for (const TowerHotspot& hotspot : this->towerHotspots)
    {
        cJSON* hotspotItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(hotspotItem, "tileX", hotspot.tileX);
        cJSON_AddNumberToObject(hotspotItem, "tileY", hotspot.tileY);
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

void EditorMapShipDownscaleScene::openImportAssetDialog(void)
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
    rc2d_filedialog_openFile(&EditorMapShipDownscaleScene::onImportAssetDialogResult, this, &options);
}


void EditorMapShipDownscaleScene::openImportMapDialog(void)
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
    rc2d_filedialog_openFile(&EditorMapShipDownscaleScene::onImportMapDialogResult, this, &options);
}

void EditorMapShipDownscaleScene::openExportMapDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kShipFolderFilters;
    options.num_filters = static_cast<int>(std::size(kShipFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Choisir dossier d'export des navires downscale";
    options.accept_label = "Exporter navires";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapShipDownscaleScene::onExportMapDialogResult, this, &options);
}

bool EditorMapShipDownscaleScene::exportMapToFolder(const char* absoluteFolderPath)
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

void EditorMapShipDownscaleScene::openImportShipFolderDialog(void)
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
    rc2d_filedialog_openFolder(&EditorMapShipDownscaleScene::onImportShipFolderDialogResult, this, &options);
}

void EditorMapShipDownscaleScene::processPendingShipFolderRequest(void)
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

bool EditorMapShipDownscaleScene::importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath)
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
    knownPathKeys.reserve(this->importedShips.size() + this->shipImportBatchFolders.size() + discoveredFolders.size());
    for (const ImportedShip& ship : this->importedShips)
    {
        knownPathKeys.push_back(makePathKey(ship.folderAbsolutePath));
    }
    for (const PendingShipImport& pendingShip : this->shipImportBatchFolders)
    {
        knownPathKeys.push_back(makePathKey(pendingShip.folderAbsolutePath));
    }

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

        PendingShipImport pendingShip{};
        pendingShip.folderAbsolutePath = normalizedAbsolutePath;
        pendingShip.displayName = folderPath.filename().string();
        if (pendingShip.displayName.empty())
        {
            pendingShip.displayName = pendingShip.folderAbsolutePath;
        }
        std::error_code relError;
        std::filesystem::path relativePath = std::filesystem::relative(folderPath, rootPath, relError);
        if (!relError)
        {
            pendingShip.relativeExportPath = normalizePathSlashes(relativePath.string());
        }
        if (pendingShip.relativeExportPath.empty() ||
            pendingShip.relativeExportPath == "." ||
            pendingShip.relativeExportPath == "./")
        {
            pendingShip.relativeExportPath = pendingShip.displayName;
        }

        pendingShip.previewUseTitleStorage =
            tryBuildTitleStoragePathFromAbsolutePath(
                normalizedAbsolutePath,
                &pendingShip.previewStorageFolderPath);

        this->shipImportBatchFolders.push_back(pendingShip);
        addedCount += 1;
    }

    if (addedCount <= 0)
    {
        this->statusMessage = "Aucun nouveau navire importe (deja presents).";
        return false;
    }

    this->shipImportBatchActive = true;
    this->statusMessage =
        "Batch navires initialise: " + std::to_string(addedCount) + " dossier(s) ajoutes.";
    return true;
}

void EditorMapShipDownscaleScene::processShipImportBatch(void)
{
    if (!this->shipImportBatchActive)
    {
        this->shipImportBatchFramesUntilNextShip = 0;
        return;
    }

    const size_t totalCount = this->shipImportBatchFolders.size();
    const bool batchUsesTitleStorage =
        this->shipImportBatchNextIndex < totalCount &&
        this->shipImportBatchFolders[this->shipImportBatchNextIndex].previewUseTitleStorage;
    const size_t maxShipsPerFrame = batchUsesTitleStorage ? 24U : 1U;
    size_t processedThisFrame = 0;
    while (processedThisFrame < maxShipsPerFrame &&
           this->shipImportBatchNextIndex < totalCount)
    {
        const PendingShipImport& pendingShip =
            this->shipImportBatchFolders[this->shipImportBatchNextIndex];
        if (this->importSingleShipFromBatch(pendingShip))
        {
            this->shipImportBatchImportedCount += 1;
        }
        else
        {
            this->shipImportBatchFailedCount += 1;
        }

        this->shipImportBatchNextIndex += 1;
        processedThisFrame += 1;
    }

    if (this->shipImportBatchNextIndex < totalCount)
    {
        this->initialShipsLoadingProcessed = static_cast<int>(this->shipImportBatchNextIndex);
        this->statusMessage =
            "Import navires: " +
            std::to_string(this->shipImportBatchNextIndex) + "/" +
            std::to_string(totalCount) + "...";
        return;
    }

    this->shipImportBatchActive = false;
    this->shipImportBatchFramesUntilNextShip = 0;
    this->initialShipsLoadingProcessed = static_cast<int>(totalCount);
    if (this->shipsSimulationEnabled)
    {
        this->randomizeImportedShipsSimulationState();
    }
    if (this->selectedShipIndex < 0 && !this->importedShips.empty())
    {
        this->selectImportedShipAtIndex(0);
    }

    const int importedCount = this->shipImportBatchImportedCount;
    const int failedCount = this->shipImportBatchFailedCount;
    if (importedCount > 0 && failedCount == 0)
    {
        this->statusMessage = std::to_string(importedCount) + " navire(s) importe(s).";
    }
    else if (importedCount > 0)
    {
        this->statusMessage =
            std::to_string(importedCount) + " navire(s) importe(s), " +
            std::to_string(failedCount) + " en echec.";
    }
    else
    {
        this->statusMessage = "Import navires en echec.";
    }

    this->shipImportBatchFolders.clear();
    this->shipImportBatchNextIndex = 0;
    this->shipImportBatchImportedCount = 0;
    this->shipImportBatchFailedCount = 0;
}

bool EditorMapShipDownscaleScene::importSingleShipFromBatch(const PendingShipImport& pendingShip)
{
    if (pendingShip.folderAbsolutePath.empty())
    {
        return false;
    }

    ImportedShip importedShip{};
    importedShip.displayName = pendingShip.displayName;
    importedShip.folderAbsolutePath = pendingShip.folderAbsolutePath;
    importedShip.relativeExportPath = pendingShip.relativeExportPath.empty()
        ? pendingShip.displayName
        : pendingShip.relativeExportPath;
    importedShip.previewStorageFolderPath = pendingShip.previewStorageFolderPath;
    importedShip.previewUseTitleStorage = pendingShip.previewUseTitleStorage;
    importedShip.scalePercent = 100;
    importedShip.previewImages.fill(RC2D_Image{});
    importedShip.previewImageData.fill(RC2D_ImageData{});
    importedShip.previewSpriteLoaded.fill(false);
    importedShip.previewWidthPx = 0.0f;
    importedShip.previewHeightPx = 0.0f;
    importedShip.previewSpriteIndex = this->computeCurrentShipPreviewSpriteIndex();
    importedShip.anchorTileX = 0.0f;
    importedShip.anchorTileY = 0.0f;
    importedShip.layoutScreenX = 0.0f;
    importedShip.layoutScreenY = 0.0f;
    importedShip.simulationVelocityTileX = 0.0f;
    importedShip.simulationVelocityTileY = 0.0f;
    importedShip.simulationShip.reset();
    importedShip.simulationNextDirectionIndex = 0;
    importedShip.simulationPauseBeforeNextCommandSec = 0.0;
    importedShip.simulationCommandRngState = 0U;
    importedShip.simulationCommandCooldownSec = 0.0;

    std::error_code fsError;
    for (int spriteIndex = 1; spriteIndex <= kShipSpriteCount; ++spriteIndex)
    {
        const std::filesystem::path spritePath =
            std::filesystem::path(importedShip.folderAbsolutePath) /
            (std::to_string(spriteIndex) + ".png");
        if (!std::filesystem::exists(spritePath, fsError) ||
            !std::filesystem::is_regular_file(spritePath, fsError))
        {
            return false;
        }
    }

    if (!this->ensureImportedShipPreviewSpriteLoaded(&importedShip, 1))
    {
        return false;
    }

    importedShip.previewWidthPx = (std::max)(importedShip.previewWidthPx, 0.0f);
    importedShip.previewHeightPx = (std::max)(importedShip.previewHeightPx, 0.0f);
    importedShip.previewSpriteIndex = 1;

    const float angle = static_cast<float>((this->importedShips.size() % 16) * 0.3926990817); // pi/8
    const float speed = 0.8f + static_cast<float>((this->importedShips.size() % 5) * 0.12f);
    importedShip.simulationVelocityTileX = std::cos(angle) * speed;
    importedShip.simulationVelocityTileY = std::sin(angle) * speed;

    this->importedShips.push_back(std::move(importedShip));
    this->importedShipsLayoutDirty = true;
    return true;
}

bool EditorMapShipDownscaleScene::loadShipPreviewImageFromStoragePath(
    ImportedShip* ship,
    int spriteIndex,
    const char* storagePath,
    RC2D_StorageKind storageKind)
{
    if (ship == nullptr || storagePath == nullptr || storagePath[0] == '\0' ||
        spriteIndex < 1 || spriteIndex > kShipSpriteCount)
    {
        return false;
    }

    const size_t spriteOffset = static_cast<size_t>(spriteIndex - 1);
    if (storageKind == RC2D_STORAGE_TITLE)
    {
        ResetStorageImageDataRef(&ship->previewImageData[spriteOffset]);
        ResetStorageImageRef(&ship->previewImages[spriteOffset]);
    }
    else
    {
        ReleaseStorageImageData(&ship->previewImageData[spriteOffset]);
        ReleaseStorageImage(&ship->previewImages[spriteOffset]);
    }
    ship->previewSpriteLoaded[spriteOffset] = false;

    ship->previewImageData[spriteOffset] = LoadStorageImageData(storagePath, storageKind);
    ship->previewImages[spriteOffset] = LoadStorageImage(storagePath, storageKind);
    if (ship->previewImages[spriteOffset].sdl_texture != nullptr)
    {
        SDL_SetTextureScaleMode(ship->previewImages[spriteOffset].sdl_texture, SDL_SCALEMODE_LINEAR);
    }

    if (ship->previewImageData[spriteOffset].sdl_surface == nullptr ||
        ship->previewImages[spriteOffset].sdl_texture == nullptr)
    {
        if (storageKind == RC2D_STORAGE_TITLE)
        {
            ResetStorageImageDataRef(&ship->previewImageData[spriteOffset]);
            ResetStorageImageRef(&ship->previewImages[spriteOffset]);
        }
        else
        {
            ReleaseStorageImageData(&ship->previewImageData[spriteOffset]);
            ReleaseStorageImage(&ship->previewImages[spriteOffset]);
        }
        ship->previewSpriteLoaded[spriteOffset] = false;
        return false;
    }

    ship->previewSpriteLoaded[spriteOffset] = true;
    return true;
}

bool EditorMapShipDownscaleScene::loadShipPreviewImageFromAbsolutePath(ImportedShip* ship, int spriteIndex, const char* spriteAbsolutePath)
{
    if (ship == nullptr || spriteAbsolutePath == nullptr || spriteAbsolutePath[0] == '\0' ||
        spriteIndex < 1 || spriteIndex > kShipSpriteCount)
    {
        return false;
    }

    std::ifstream input(spriteAbsolutePath, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        return false;
    }
    const std::streamsize fileSize = input.tellg();
    if (fileSize <= 0)
    {
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(fileSize));
    if (!input.read(bytes.data(), fileSize))
    {
        return false;
    }

    this->ensureUserStorageFolders();
    const unsigned int previewId = this->importedShipPreviewCounter++;
    char userStoragePath[128] = {};
    SDL_snprintf(
        userStoragePath,
        sizeof(userStoragePath),
        "editor-map-shipdownscale/preview/%u.png",
        previewId);
    if (!rc2d_storage_userWriteFile(userStoragePath, bytes.data(), static_cast<Uint64>(bytes.size())))
    {
        return false;
    }

    return this->loadShipPreviewImageFromStoragePath(ship, spriteIndex, userStoragePath, RC2D_STORAGE_USER);
}

bool EditorMapShipDownscaleScene::ensureImportedShipPreviewSpriteLoaded(ImportedShip* ship, int spriteIndex)
{
    if (ship == nullptr || spriteIndex < 1 || spriteIndex > kShipSpriteCount)
    {
        return false;
    }

    const size_t spriteOffset = static_cast<size_t>(spriteIndex - 1);
    if (ship->previewSpriteLoaded[spriteOffset] &&
        ship->previewImages[spriteOffset].sdl_texture != nullptr &&
        ship->previewImageData[spriteOffset].sdl_surface != nullptr)
    {
        return true;
    }

    if (ship->previewUseTitleStorage && !ship->previewStorageFolderPath.empty())
    {
        const std::string storagePath =
            ship->previewStorageFolderPath +
            "/" +
            std::to_string(spriteIndex) +
            ".png";
        if (!this->loadShipPreviewImageFromStoragePath(
                ship,
                spriteIndex,
                storagePath.c_str(),
                RC2D_STORAGE_TITLE))
        {
            return false;
        }
    }
    else
    {
        const std::filesystem::path spritePath =
            std::filesystem::path(ship->folderAbsolutePath) /
            (std::to_string(spriteIndex) + ".png");
        if (!this->loadShipPreviewImageFromAbsolutePath(ship, spriteIndex, spritePath.string().c_str()))
        {
            return false;
        }
    }

    const RC2D_ImageData& loadedImageData = ship->previewImageData[spriteOffset];
    if (loadedImageData.sdl_surface != nullptr)
    {
        ship->previewWidthPx = (std::max)(ship->previewWidthPx, static_cast<float>(loadedImageData.sdl_surface->w));
        ship->previewHeightPx = (std::max)(ship->previewHeightPx, static_cast<float>(loadedImageData.sdl_surface->h));
    }
    if (ship->previewSpriteIndex < 1 || ship->previewSpriteIndex > kShipSpriteCount)
    {
        ship->previewSpriteIndex = spriteIndex;
    }

    return true;
}

void EditorMapShipDownscaleScene::processDeferredShipPreviewLoads(void)
{
    if (this->importedShips.empty())
    {
        return;
    }

    std::size_t remainingBudget = this->shipsSimulationEnabled
        ? kDeferredShipPreviewLoadsPerFrameSimulation
        : kDeferredShipPreviewLoadsPerFrame;
    if (remainingBudget == 0U)
    {
        return;
    }
    const int desiredSpriteIndex = this->computeCurrentShipPreviewSpriteIndex();
    constexpr int kOffModeSpriteIndex = 1;
    auto isSpriteReady = [](const ImportedShip& importedShip, int spriteIndex) -> bool {
        if (spriteIndex < 1 || spriteIndex > kShipSpriteCount)
        {
            return false;
        }
        const std::size_t spriteOffset = static_cast<std::size_t>(spriteIndex - 1);
        return importedShip.previewSpriteLoaded[spriteOffset] &&
            importedShip.previewImages[spriteOffset].sdl_texture != nullptr &&
            importedShip.previewImageData[spriteOffset].sdl_surface != nullptr;
    };

    if (this->selectedShipIndex >= 0 &&
        this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
    {
        ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
        int selectedTargetSpriteIndex = kOffModeSpriteIndex;
        if (this->shipsSimulationEnabled)
        {
            selectedTargetSpriteIndex = desiredSpriteIndex;
            if (selectedShip.simulationShip != nullptr)
            {
                selectedTargetSpriteIndex = simulationPreviewDirectionToSpriteIndex(
                    selectedShip.simulationShip->getCurrentPreviewDirection(),
                    this->shipsLowHpEnabled);
            }
        }
        if (!isSpriteReady(selectedShip, selectedTargetSpriteIndex) &&
            this->ensureImportedShipPreviewSpriteLoaded(&selectedShip, selectedTargetSpriteIndex) &&
            remainingBudget > 0U)
        {
            remainingBudget -= 1U;
        }
    }

    if (this->shipsSimulationEnabled)
    {
        const std::size_t simulationCount =
            (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);
        if (simulationCount == 0U || remainingBudget == 0U)
        {
            return;
        }

        const std::size_t maxScansPerFrame =
            (std::min)(simulationCount, static_cast<std::size_t>(96U));
        std::size_t scannedShips = 0U;
        while (scannedShips < maxScansPerFrame && remainingBudget > 0U)
        {
            const size_t shipIndex =
                (this->deferredShipPreviewLoadCursor + scannedShips) % simulationCount;
            ImportedShip& importedShip = this->importedShips[shipIndex];
            if (importedShip.simulationShip == nullptr)
            {
                scannedShips += 1U;
                continue;
            }

            const int targetSpriteIndex = simulationPreviewDirectionToSpriteIndex(
                importedShip.simulationShip->getCurrentPreviewDirection(),
                this->shipsLowHpEnabled);
            if (!isSpriteReady(importedShip, targetSpriteIndex) &&
                this->ensureImportedShipPreviewSpriteLoaded(&importedShip, targetSpriteIndex))
            {
                remainingBudget -= 1U;
            }

            scannedShips += 1U;
        }

        this->deferredShipPreviewLoadCursor =
            (this->deferredShipPreviewLoadCursor + scannedShips) % simulationCount;
        return;
    }

    for (SpawnedShipInstance& spawnedShip : this->spawnedShips)
    {
        if (remainingBudget == 0U)
        {
            return;
        }
        if (spawnedShip.importedShipIndex < 0 ||
            spawnedShip.importedShipIndex >= static_cast<int>(this->importedShips.size()))
        {
            continue;
        }

        ImportedShip& importedShip = this->importedShips[static_cast<size_t>(spawnedShip.importedShipIndex)];
        if (isSpriteReady(importedShip, kOffModeSpriteIndex))
        {
            continue;
        }
        if (this->ensureImportedShipPreviewSpriteLoaded(&importedShip, kOffModeSpriteIndex))
        {
            remainingBudget -= 1U;
        }
    }
}

void EditorMapShipDownscaleScene::recomputeImportedShipLayout(void)
{
    const Map& map = GetCurrentMap();
    if (this->importedShips.empty() || map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        this->importedShipsLayoutDirty = false;
        return;
    }

    const float mapLeft = map.rect.x;
    const float mapTop = map.rect.y;
    const float mapRight = map.rect.x + map.rect.w;
    const float mapBottom = map.rect.y + map.rect.h;
    const float paddingPx = 10.0f;
    const float gapPx = 6.0f;
    float cursorX = mapLeft + paddingPx;
    float cursorY = mapTop + paddingPx;
    float rowMaxHeightPx = 1.0f;

    for (ImportedShip& ship : this->importedShips)
    {
        const float shipScale = static_cast<float>(std::clamp(ship.scalePercent, 5, 100)) / 100.0f;
        const float drawWidthPx = (std::max)(1.0f, ship.previewWidthPx * shipScale);
        const float drawHeightPx = (std::max)(1.0f, ship.previewHeightPx * shipScale);

        // Grille de type spritesheet: gauche -> droite, puis retour ligne suivante.
        if ((cursorX + drawWidthPx) > (mapRight - paddingPx))
        {
            cursorX = mapLeft + paddingPx;
            cursorY += rowMaxHeightPx + gapPx;
            rowMaxHeightPx = 1.0f;
        }

        float drawX = cursorX;
        float drawY = cursorY;
        if ((drawX + drawWidthPx) > (mapRight - paddingPx))
        {
            drawX = (mapRight - paddingPx) - drawWidthPx;
        }
        if ((drawY + drawHeightPx) > (mapBottom - paddingPx))
        {
            drawY = (mapBottom - paddingPx) - drawHeightPx;
        }
        const float minX = mapLeft + 1.0f;
        const float minY = mapTop + 1.0f;
        const float maxX = (mapRight - 1.0f) - drawWidthPx;
        const float maxY = (mapBottom - 1.0f) - drawHeightPx;
        if (maxX < minX)
        {
            drawX = minX;
        }
        else
        {
            drawX = std::clamp(drawX, minX, maxX);
        }
        if (maxY < minY)
        {
            drawY = minY;
        }
        else
        {
            drawY = std::clamp(drawY, minY, maxY);
        }

        ship.layoutScreenX = drawX;
        ship.layoutScreenY = drawY;

        // Conserve aussi une ancre tile coherente pour camera/minimap fallback.
        const SDL_FPoint anchorTile = map.screenToTile(drawX, drawY);
        ship.anchorTileX = std::clamp(anchorTile.x, 0.0f, static_cast<float>(map.getWidthTiles() - 1));
        ship.anchorTileY = std::clamp(anchorTile.y, 0.0f, static_cast<float>(map.getHeightTiles() - 1));

        cursorX = drawX + drawWidthPx + gapPx;
        rowMaxHeightPx = (std::max)(rowMaxHeightPx, drawHeightPx);
    }

    this->importedShipsLayoutDirty = false;
}

void EditorMapShipDownscaleScene::updateImportedShipsSimulation(double dt)
{
    if (!this->shipsSimulationEnabled || this->importedShips.empty())
    {
        return;
    }
    const std::size_t simulationCount =
        (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);
    if (simulationCount == 0U)
    {
        return;
    }

    Map& map = GetCurrentMap();
    if (map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return;
    }
    int movingShips = 0;
    for (std::size_t i = 0; i < simulationCount; ++i)
    {
        ImportedShip& ship = this->importedShips[i];
        if (ship.simulationShip == nullptr)
        {
            ship.simulationShip = std::make_unique<Ship>();
            ship.simulationShip->setSpeedTilesPerSecond(3.5f);
            ship.simulationShip->setPositionTile(ship.anchorTileX, ship.anchorTileY);
            ship.simulationShip->setPreviewDirection(Ship::PreviewDirection::DOWN_LEFT);
        }

        Ship& runtimeShip = *ship.simulationShip;
        runtimeShip.setDrawScale(static_cast<float>(std::clamp(ship.scalePercent, 5, 100)) / 100.0f);
        runtimeShip.setHealthVisual(this->shipsLowHpEnabled ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
        runtimeShip.update(dt, map);

        const SDL_FPoint runtimeTile = runtimeShip.getPositionTile();
        ship.anchorTileX = std::clamp(runtimeTile.x, 0.0f, static_cast<float>(map.getWidthTiles() - 1));
        ship.anchorTileY = std::clamp(runtimeTile.y, 0.0f, static_cast<float>(map.getHeightTiles() - 1));

        if (runtimeShip.isMoving())
        {
            movingShips += 1;
        }

        if (!runtimeShip.isMoving() && ship.simulationPauseBeforeNextCommandSec > 0.0)
        {
            ship.simulationPauseBeforeNextCommandSec =
                (std::max)(0.0, ship.simulationPauseBeforeNextCommandSec - dt);
        }

        if (ship.simulationCommandCooldownSec > 0.0)
        {
            ship.simulationCommandCooldownSec =
                (std::max)(0.0, ship.simulationCommandCooldownSec - dt);
        }
    }

    this->shipsSimulationRetargetAccumulatorSec += dt;
    if (this->shipsSimulationRetargetAccumulatorSec < kSimulationRetargetTickPeriodSec)
    {
        return;
    }
    this->shipsSimulationRetargetAccumulatorSec =
        std::fmod(this->shipsSimulationRetargetAccumulatorSec, kSimulationRetargetTickPeriodSec);

    int desiredMovingShips = static_cast<int>(
        std::ceil(static_cast<double>(simulationCount) * static_cast<double>(kSimulationDesiredMovingRatio)));
    desiredMovingShips = (std::max)(desiredMovingShips, 1);
    desiredMovingShips = (std::min)(desiredMovingShips, static_cast<int>(simulationCount));
    desiredMovingShips = (std::min)(desiredMovingShips, kSimulationMaxMovingShipsHardCap);

    const int missingMovingShips = (std::max)(0, desiredMovingShips - movingShips);
    const int retargetBudgetThisTick = std::clamp(
        missingMovingShips,
        kSimulationRetargetBudgetPerTickMin,
        kSimulationRetargetBudgetPerTickMax);

    int retargetedThisTick = 0;
    size_t scanned = 0U;
    while (scanned < simulationCount &&
           retargetedThisTick < retargetBudgetThisTick &&
           movingShips < desiredMovingShips)
    {
        const size_t shipIndex =
            (this->shipsSimulationRetargetCursor + scanned) % simulationCount;
        scanned += 1U;

        ImportedShip& importedShip = this->importedShips[shipIndex];
        if (importedShip.simulationShip == nullptr)
        {
            continue;
        }
        if (importedShip.simulationShip->isMoving())
        {
            continue;
        }
        if (importedShip.simulationPauseBeforeNextCommandSec > 0.0 ||
            importedShip.simulationCommandCooldownSec > 0.0)
        {
            continue;
        }

        if (this->issueNextSimulationMoveForImportedShip(&importedShip))
        {
            retargetedThisTick += 1;
            movingShips += 1;
        }
    }

    if (simulationCount > 0U)
    {
        this->shipsSimulationRetargetCursor =
            (this->shipsSimulationRetargetCursor + scanned) % simulationCount;
    }
}

int EditorMapShipDownscaleScene::computeCurrentShipPreviewSpriteIndex(void) const
{
    const int frameOffset = ((this->shipsPreviewFrameOffset % 4) + 4) % 4;
    const int base = this->shipsLowHpEnabled ? 5 : 1;
    return std::clamp(base + frameOffset, 1, kShipSpriteCount);
}

void EditorMapShipDownscaleScene::refreshImportedShipsPreviewForCurrentFrame(bool updateStatusMessageOnFailure)
{
    (void)updateStatusMessageOnFailure;
    const int spriteIndex = this->computeCurrentShipPreviewSpriteIndex();
    for (ImportedShip& ship : this->importedShips)
    {
        ship.previewSpriteIndex = spriteIndex;
    }
}

void EditorMapShipDownscaleScene::randomizeImportedShipsSimulationState(void)
{
    if (this->importedShips.empty())
    {
        return;
    }

    Map& map = GetCurrentMap();
    if (map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return;
    }
    this->shipsSimulationRetargetCursor = 0U;
    this->shipsSimulationRetargetAccumulatorSec = 0.0;
    uint32_t rng = 0xC0FFEE42U;

    const std::size_t simulationCount =
        (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);

    // Libere explicitement les navires hors budget runtime.
    for (std::size_t i = simulationCount; i < this->importedShips.size(); ++i)
    {
        this->importedShips[i].simulationShip.reset();
    }

    for (size_t i = 0; i < simulationCount; ++i)
    {
        ImportedShip& ship = this->importedShips[i];
        if (ship.simulationShip == nullptr)
        {
            ship.simulationShip = std::make_unique<Ship>();
        }

        Ship& runtimeShip = *ship.simulationShip;
        const float speed = 2.0f + (randomNext01(&rng) * 2.5f);
        runtimeShip.setSpeedTilesPerSecond(speed);
        runtimeShip.setDrawScale(static_cast<float>(std::clamp(ship.scalePercent, 5, 100)) / 100.0f);
        runtimeShip.setHealthVisual(this->shipsLowHpEnabled ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);

        SDL_Point spawnTile{};
        bool hasExplicitSpawnTile = false;
        for (auto it = this->spawnedShips.rbegin(); it != this->spawnedShips.rend(); ++it)
        {
            if (it->importedShipIndex != static_cast<int>(i))
            {
                continue;
            }

            const SDL_Point roundedSpawn = map.roundTile(it->tileX, it->tileY);
            spawnTile = map.clampTile(roundedSpawn.x, roundedSpawn.y);
            hasExplicitSpawnTile = true;
            break;
        }

        if ((!hasExplicitSpawnTile || map.isTileBlocked(spawnTile.x, spawnTile.y)) &&
            !pickSimulationSpawnTile(map, &rng, &spawnTile))
        {
            spawnTile = map.clampTile(0, 0);
        }

        runtimeShip.setPositionTileInt(spawnTile.x, spawnTile.y);

        const int previewDirectionIndex =
            randomIntInclusive(&rng, 0, static_cast<int>(kSimulationClickDirections.size()) - 1) % 4;
        switch (previewDirectionIndex)
        {
        case 1:
            runtimeShip.setPreviewDirection(Ship::PreviewDirection::UP_RIGHT);
            break;
        case 2:
            runtimeShip.setPreviewDirection(Ship::PreviewDirection::UP_LEFT);
            break;
        case 3:
            runtimeShip.setPreviewDirection(Ship::PreviewDirection::DOWN_RIGHT);
            break;
        default:
            runtimeShip.setPreviewDirection(Ship::PreviewDirection::DOWN_LEFT);
            break;
        }

        ship.anchorTileX = static_cast<float>(spawnTile.x);
        ship.anchorTileY = static_cast<float>(spawnTile.y);
        ship.simulationVelocityTileX = 0.0f;
        ship.simulationVelocityTileY = 0.0f;
        ship.simulationNextDirectionIndex =
            randomIntInclusive(&rng, 0, static_cast<int>(kSimulationClickDirections.size()) - 1);
        ship.simulationPauseBeforeNextCommandSec = static_cast<double>(randomNext01(&rng)) * 1.0;
        ship.simulationCommandRngState =
            randomNextU32(&rng) ^ (static_cast<uint32_t>(i + 1U) * 2654435761U);
        ship.simulationCommandCooldownSec = static_cast<double>(randomNext01(&rng)) * 0.18;
    }

    if (this->importedShips.size() > simulationCount)
    {
        this->statusMessage =
            "Simulation ON: " + std::to_string(simulationCount) +
            " navire(s) actifs (cap runtime).";
    }
}

bool EditorMapShipDownscaleScene::computeNextSimulationTargetTileForImportedShip(
    ImportedShip* ship,
    SDL_Point* outTargetTile)
{
    if (ship == nullptr || ship->simulationShip == nullptr || outTargetTile == nullptr)
    {
        return false;
    }

    Map& map = GetCurrentMap();
    if (map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return false;
    }
    Ship& runtimeShip = *ship->simulationShip;
    const SDL_FPoint shipPos = runtimeShip.getPositionTile();
    const SDL_Point shipTileRaw = map.roundTile(shipPos.x, shipPos.y);
    const SDL_Point shipTile = map.clampTile(shipTileRaw.x, shipTileRaw.y);

    const int directionCount = static_cast<int>(kSimulationClickDirections.size());
    const int baseDirection = ship->simulationNextDirectionIndex % directionCount;
    for (int attempt = 0; attempt < directionCount; ++attempt)
    {
        const int directionIndex = (baseDirection + attempt) % directionCount;
        const SDL_Point offset = simulationDirectionToTileOffset(
            kSimulationClickDirections[static_cast<size_t>(directionIndex)],
            kSimulationNetworkCommandDistanceTiles);
        if (offset.x == 0 && offset.y == 0)
        {
            continue;
        }

        const SDL_Point candidate = map.clampTile(
            shipTile.x + offset.x,
            shipTile.y + offset.y);
        if ((candidate.x == shipTile.x && candidate.y == shipTile.y) ||
            map.isTileBlocked(candidate.x, candidate.y))
        {
            continue;
        }

        ship->simulationNextDirectionIndex = (directionIndex + 1) % directionCount;
        *outTargetTile = candidate;
        return true;
    }

    // Fallback: cible pseudo-aleatoire locale (toujours dans la map),
    // puis Ship::moveToTile gere le vrai pathfinding A*.
    constexpr int kFallbackAttempts = 10;
    for (int attempt = 0; attempt < kFallbackAttempts; ++attempt)
    {
        const int dx = randomIntInclusive(
            &ship->simulationCommandRngState,
            -kSimulationNetworkCommandDistanceTiles,
            kSimulationNetworkCommandDistanceTiles);
        const int dy = randomIntInclusive(
            &ship->simulationCommandRngState,
            -kSimulationNetworkCommandDistanceTiles,
            kSimulationNetworkCommandDistanceTiles);
        if (dx == 0 && dy == 0)
        {
            continue;
        }

        const int manhattanDistance = std::abs(dx) + std::abs(dy);
        if (manhattanDistance < kSimulationNetworkCommandFallbackMinDistanceTiles)
        {
            continue;
        }

        const SDL_Point candidate = map.clampTile(shipTile.x + dx, shipTile.y + dy);
        if ((candidate.x == shipTile.x && candidate.y == shipTile.y) ||
            map.isTileBlocked(candidate.x, candidate.y))
        {
            continue;
        }

        *outTargetTile = candidate;
        return true;
    }

    return false;
}

bool EditorMapShipDownscaleScene::issueNextSimulationMoveForImportedShip(ImportedShip* ship)
{
    if (ship == nullptr || ship->simulationShip == nullptr)
    {
        return false;
    }

    Map& map = GetCurrentMap();
    Ship& runtimeShip = *ship->simulationShip;

    SDL_Point targetTile{};
    if (!this->computeNextSimulationTargetTileForImportedShip(ship, &targetTile))
    {
        ship->simulationCommandCooldownSec = 0.10;
        return false;
    }

    runtimeShip.moveToTile(map, targetTile.x, targetTile.y);
    if (runtimeShip.isMoving())
    {
        ship->simulationPauseBeforeNextCommandSec = kSimulationPauseAfterArrivalSec;
        const int pseudoLatencyMs =
            randomIntInclusive(&ship->simulationCommandRngState, 28, 145);
        ship->simulationCommandCooldownSec =
            static_cast<double>(pseudoLatencyMs) / 1000.0;
        return true;
    }

    ship->simulationPauseBeforeNextCommandSec = 0.0;
    ship->simulationCommandCooldownSec = 0.08;
    return false;
}

void EditorMapShipDownscaleScene::advanceImportedShipsSpriteAnimation(double dt)
{
    if (this->importedShips.empty())
    {
        return;
    }
    if (this->shipsSimulationEnabled)
    {
        return;
    }
    if (this->shipsPreviewAnimationIntervalSeconds <= 0.0)
    {
        return;
    }
    this->shipsPreviewAnimationAccumulator += (std::max)(dt, 0.0);
    if (this->shipsPreviewAnimationAccumulator < this->shipsPreviewAnimationIntervalSeconds)
    {
        return;
    }

    const int steps = static_cast<int>(
        std::floor(this->shipsPreviewAnimationAccumulator / this->shipsPreviewAnimationIntervalSeconds));
    this->shipsPreviewAnimationAccumulator -=
        static_cast<double>(steps) * this->shipsPreviewAnimationIntervalSeconds;
    if (steps <= 0)
    {
        return;
    }

    const int oldFrameOffset = this->shipsPreviewFrameOffset;
    this->shipsPreviewFrameOffset =
        (this->shipsPreviewFrameOffset + steps) % kShipSpriteCount;
    if (this->shipsPreviewFrameOffset != oldFrameOffset)
    {
        this->refreshImportedShipsPreviewForCurrentFrame(false);
    }
}

void EditorMapShipDownscaleScene::setShipsSimulationEnabled(bool enabled)
{
    if (this->shipsSimulationEnabled == enabled)
    {
        return;
    }

    this->shipsSimulationEnabled = enabled;
    if (this->shipsSimulationEnabled)
    {
        this->randomizeImportedShipsSimulationState();
    }
    else
    {
        this->shipsSimulationRetargetCursor = 0U;
        this->shipsSimulationRetargetAccumulatorSec = 0.0;
        this->refreshImportedShipsPreviewForCurrentFrame(false);
    }
}

void EditorMapShipDownscaleScene::setShipsLowHpEnabled(bool enabled)
{
    if (this->shipsLowHpEnabled == enabled)
    {
        return;
    }

    this->shipsLowHpEnabled = enabled;
    this->applyShipsPreviewForCurrentHealthVisual();

    const Ship::HealthVisual visual =
        this->shipsLowHpEnabled ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL;
    for (ImportedShip& importedShip : this->importedShips)
    {
        if (importedShip.simulationShip != nullptr)
        {
            importedShip.simulationShip->setHealthVisual(visual);
        }
    }
    this->testShip.setHealthVisual(visual);
    this->testShipPreview.setHealthVisual(visual);
}

void EditorMapShipDownscaleScene::applyShipsPreviewForCurrentHealthVisual(void)
{
    this->refreshImportedShipsPreviewForCurrentFrame(true);
}

void EditorMapShipDownscaleScene::processPendingMapImportRequest(void)
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

bool EditorMapShipDownscaleScene::importMapFromAbsolutePath(const char* absolutePath)
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
    this->historyActions.clear();
    this->historyCursor = 0;

    std::string importStatusSuffix;
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
                "EditorMapShipDownscaleScene: oceanColor JSON inconnu '%s'",
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
            const cJSON* scale = cJSON_GetObjectItemCaseSensitive(item, "scale");
            if (cJSON_IsNumber(scale) && std::isfinite(scale->valuedouble) && scale->valuedouble > 0.01)
            {
                placed.scale = static_cast<float>(scale->valuedouble);
            }
            this->placedAssets.push_back(placed);
        }
    }

    const cJSON* towerHotspotsJson = cJSON_GetObjectItemCaseSensitive(root, "towerHotspots");
    if (cJSON_IsArray(towerHotspotsJson))
    {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, towerHotspotsJson)
        {
            const cJSON* tileX = cJSON_GetObjectItemCaseSensitive(item, "tileX");
            const cJSON* tileY = cJSON_GetObjectItemCaseSensitive(item, "tileY");
            if (!cJSON_IsNumber(tileX) || !cJSON_IsNumber(tileY))
            {
                continue;
            }
            this->towerHotspots.push_back(
                TowerHotspot{
                    static_cast<int>(std::lround(tileX->valuedouble)),
                    static_cast<int>(std::lround(tileY->valuedouble))});
        }
    }

    cJSON_Delete(root);

    if (shouldApplyOceanColor)
    {
        const OceanColorEntry& entry = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)];
        this->applySelectedOceanColor();
        importStatusSuffix = " Ocean: " + std::string(entry.label) + ".";
    }

    this->statusMessage = "Map importee depuis JSON." + importStatusSuffix;
    return true;
}

bool EditorMapShipDownscaleScene::loadShipFolderFromAbsolutePath(const char* folderAbsolutePath)
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
    this->testShip.setHealthVisual(this->shipsLowHpEnabled ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
    this->testShip.setDrawAlpha(255);
    this->testShipPreview.setSpeedTilesPerSecond(4.0f);
    this->testShipPreview.setHealthVisual(this->shipsLowHpEnabled ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
    this->testShipPreview.setDrawAlpha(255);
    this->setShipScalePercent(this->shipScalePercent);
    this->testShipLoaded = true;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->loadedShipFolderAbsolute = normalizedAbsoluteFolderPath;
    this->editorTool = EditorTool::PLACE_ASSETS;
    this->statusMessage = "Navire test charge.";
    return true;
}

bool EditorMapShipDownscaleScene::selectImportedShipAtIndex(int shipIndex)
{
    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Selection navire invalide.";
        return false;
    }

    this->selectedShipIndex = shipIndex;
    this->ensureSelectedShipVisible();

    ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    this->setShipScalePercent(std::clamp(selectedShip.scalePercent, 5, 100));
    const bool previewLoaded = this->ensureImportedShipPreviewSpriteLoaded(&selectedShip, 1);

    if (this->shipsSimulationEnabled)
    {
        this->centerCameraOnImportedShipIndex(this->selectedShipIndex);
        if (selectedShip.simulationShip == nullptr)
        {
            this->statusMessage =
                "Navire actif: " + selectedShip.displayName +
                " | non simule (cap runtime atteint)";
            return true;
        }
    }

    if (!previewLoaded)
    {
        this->statusMessage =
            "Navire actif: " + selectedShip.displayName +
            " | Scale: " + std::to_string(this->shipScalePercent) +
            "% | preview 1.png manquante";
        return true;
    }

    this->statusMessage =
        "Navire actif: " + selectedShip.displayName +
        " | Scale: " + std::to_string(this->shipScalePercent) + "%";
    return true;
}

void EditorMapShipDownscaleScene::centerCameraOnImportedShipIndex(int shipIndex)
{
    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()))
    {
        return;
    }

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    const ImportedShip& importedShip = this->importedShips[static_cast<size_t>(shipIndex)];

    float targetTileX = importedShip.anchorTileX;
    float targetTileY = importedShip.anchorTileY;
    if (this->shipsSimulationEnabled)
    {
        if (importedShip.simulationShip != nullptr)
        {
            const SDL_FPoint simulationTile = importedShip.simulationShip->getPositionTile();
            targetTileX = simulationTile.x;
            targetTileY = simulationTile.y;
        }
    }
    else
    {
        for (auto it = this->spawnedShips.rbegin(); it != this->spawnedShips.rend(); ++it)
        {
            if (it->importedShipIndex != shipIndex)
            {
                continue;
            }
            targetTileX = it->tileX;
            targetTileY = it->tileY;
            break;
        }
    }

    targetTileX = std::clamp(targetTileX, 0.0f, static_cast<float>((std::max)(map.getWidthTiles() - 1, 0)));
    targetTileY = std::clamp(targetTileY, 0.0f, static_cast<float>((std::max)(map.getHeightTiles() - 1, 0)));
    camera.centerCameraOnTile(targetTileX, targetTileY, map, map.rect);
    camera.update(map, map.rect);
    this->testShipCameraFollowEnabled = false;
}

void EditorMapShipDownscaleScene::spawnSelectedImportedShipAtTile(int tileX, int tileY)
{
    if (this->selectedShipIndex < 0 ||
        this->selectedShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Selectionne d'abord un navire dans la liste.";
        return;
    }

    Map& map = GetCurrentMap();
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

    ImportedShip& importedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    if (!this->ensureImportedShipPreviewSpriteLoaded(&importedShip, 1))
    {
        this->statusMessage = "Spawn navire impossible: 1.png introuvable.";
        return;
    }

    SpawnedShipInstance spawnedShip{};
    spawnedShip.importedShipIndex = this->selectedShipIndex;
    spawnedShip.tileX = static_cast<float>(tileX);
    spawnedShip.tileY = static_cast<float>(tileY);
    spawnedShip.scalePercent = std::clamp(importedShip.scalePercent, 5, 100);
    this->spawnedShips.push_back(spawnedShip);

    this->statusMessage =
        "Spawn navire: " + importedShip.displayName +
        " en (" + std::to_string(tileX) + "," + std::to_string(tileY) + ").";
}

void EditorMapShipDownscaleScene::beginShipScaleInputEdit(void)
{
    this->shipScaleInputActive = true;
    this->shipScaleInputBuffer = std::to_string(this->shipScalePercent);
}

void EditorMapShipDownscaleScene::commitShipScaleInputEdit(void)
{
    if (!this->shipScaleInputActive)
    {
        return;
    }

    int parsedValue = this->shipScalePercent;
    if (!this->shipScaleInputBuffer.empty())
    {
        parsedValue = std::atoi(this->shipScaleInputBuffer.c_str());
    }
    this->shipScaleInputActive = false;
    this->setShipScalePercent(parsedValue);
    this->statusMessage = "Echelle navire: " + std::to_string(this->shipScalePercent) + "%";
}

void EditorMapShipDownscaleScene::cancelShipScaleInputEdit(void)
{
    this->shipScaleInputActive = false;
    this->shipScaleInputBuffer = std::to_string(this->shipScalePercent);
}

bool EditorMapShipDownscaleScene::handleShipScaleInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)mod;
    if (!this->shipScaleInputActive)
    {
        return false;
    }

    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->commitShipScaleInputEdit();
        return true;
    }
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->cancelShipScaleInputEdit();
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE)
    {
        if (!this->shipScaleInputBuffer.empty())
        {
            this->shipScaleInputBuffer.pop_back();
        }
        return true;
    }
    if (isrepeat)
    {
        return true;
    }

    char digit = extractDigitFromKeyLabel(key);
    if (digit == '\0' && scancode >= SDL_SCANCODE_KP_0 && scancode <= SDL_SCANCODE_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(scancode - SDL_SCANCODE_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_KP_0 && keycode <= SDLK_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_0 && keycode <= SDLK_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_0));
    }
    if (digit == '\0')
    {
        return true;
    }

    if (this->shipScaleInputBuffer.size() >= 3U)
    {
        return true;
    }
    if (this->shipScaleInputBuffer.size() == 1U &&
        this->shipScaleInputBuffer[0] == '0')
    {
        this->shipScaleInputBuffer.clear();
    }
    this->shipScaleInputBuffer.push_back(digit);
    return true;
}

bool EditorMapShipDownscaleScene::reexportLoadedShipScaled(int scalePercent)
{
    if (!this->testShipLoaded || this->loadedShipFolderAbsolute.empty())
    {
        this->statusMessage = "Aucun navire charge a reexporter.";
        return false;
    }
    const int clampedPercent = std::clamp(scalePercent, 5, 100);
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

bool EditorMapShipDownscaleScene::copyShipFolderBaseFiles(
    const char* sourceFolderAbsolutePath,
    const char* destinationFolderAbsolutePath) const
{
    if (sourceFolderAbsolutePath == nullptr || destinationFolderAbsolutePath == nullptr)
    {
        return false;
    }

    std::error_code fsError;
    const std::filesystem::path sourceFolder(sourceFolderAbsolutePath);
    const std::filesystem::path destinationFolder(destinationFolderAbsolutePath);
    if (!std::filesystem::exists(sourceFolder, fsError) ||
        !std::filesystem::is_directory(sourceFolder, fsError))
    {
        return false;
    }

    std::filesystem::create_directories(destinationFolder, fsError);
    if (fsError)
    {
        return false;
    }

    std::filesystem::recursive_directory_iterator it(
        sourceFolder,
        std::filesystem::directory_options::skip_permission_denied,
        fsError);
    if (fsError)
    {
        return false;
    }

    std::filesystem::recursive_directory_iterator end;
    while (it != end)
    {
        std::error_code entryError;
        const std::filesystem::path sourcePath = it->path();
        const std::filesystem::path relativePath =
            std::filesystem::relative(sourcePath, sourceFolder, entryError);
        if (entryError)
        {
            it.increment(fsError);
            continue;
        }

        const std::filesystem::path destinationPath = destinationFolder / relativePath;
        if (it->is_directory(entryError) && !entryError)
        {
            std::filesystem::create_directories(destinationPath, entryError);
        }
        else if (it->is_regular_file(entryError) && !entryError)
        {
            std::filesystem::create_directories(destinationPath.parent_path(), entryError);
            if (!entryError)
            {
                std::filesystem::copy_file(
                    sourcePath,
                    destinationPath,
                    std::filesystem::copy_options::overwrite_existing,
                    entryError);
            }
        }

        it.increment(fsError);
        if (fsError)
        {
            fsError.clear();
        }
    }

    return true;
}

bool EditorMapShipDownscaleScene::exportScaledShipSpritesToFolder(
    const ImportedShip& ship,
    const char* destinationFolderAbsolutePath) const
{
    if (destinationFolderAbsolutePath == nullptr || destinationFolderAbsolutePath[0] == '\0')
    {
        return false;
    }

    rc2d_storage_userMkdir("editor-map-shipdownscale");
    rc2d_storage_userMkdir("editor-map-shipdownscale/tmp");

    const int clampedPercent = std::clamp(ship.scalePercent, 5, 100);
    const std::filesystem::path destinationFolder(destinationFolderAbsolutePath);
    for (int i = 1; i <= kShipSpriteCount; ++i)
    {
        const std::filesystem::path sourcePngPath =
            std::filesystem::path(ship.folderAbsolutePath) / (std::to_string(i) + ".png");
        std::ifstream input(sourcePngPath, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            return false;
        }
        const std::streamsize fileSize = input.tellg();
        if (fileSize <= 0)
        {
            return false;
        }
        input.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(fileSize));
        if (!input.read(bytes.data(), fileSize))
        {
            return false;
        }

        char tmpUserStoragePath[128] = {};
        SDL_snprintf(
            tmpUserStoragePath,
            sizeof(tmpUserStoragePath),
            "editor-map-shipdownscale/tmp/tmp_%d.png",
            i);
        if (!rc2d_storage_userWriteFile(tmpUserStoragePath, bytes.data(), static_cast<Uint64>(bytes.size())))
        {
            return false;
        }

        RC2D_ImageData src = LoadStorageImageData(tmpUserStoragePath, RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
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

        const std::filesystem::path dstPath = destinationFolder / (std::to_string(i) + ".png");
        const bool saveOk = SDL_SavePNG(dst, dstPath.string().c_str());
        SDL_DestroySurface(dst);
        ReleaseStorageImageData(&src);
        if (!saveOk)
        {
            return false;
        }
    }

    return true;
}

bool EditorMapShipDownscaleScene::exportAllShipsScaledToFolder(const char* absoluteFolderPath)
{
    if (absoluteFolderPath == nullptr || absoluteFolderPath[0] == '\0')
    {
        this->statusMessage = "Export navires annule.";
        return false;
    }
    if (this->importedShips.empty())
    {
        this->statusMessage = "Aucun navire importe a exporter.";
        return false;
    }

    std::filesystem::path destinationRoot(absoluteFolderPath);
    std::error_code fsError;
    std::filesystem::create_directories(destinationRoot, fsError);
    if (fsError || !std::filesystem::is_directory(destinationRoot, fsError))
    {
        this->statusMessage = "Dossier export navires invalide.";
        return false;
    }

    int exportedCount = 0;
    int failedCount = 0;
    for (const ImportedShip& ship : this->importedShips)
    {
        std::filesystem::path relativePath(ship.relativeExportPath.empty() ? ship.displayName : ship.relativeExportPath);
        if (relativePath.empty())
        {
            relativePath = std::filesystem::path(ship.displayName.empty() ? "ship" : ship.displayName);
        }
        if (relativePath.is_absolute() || relativePath.string().find("..") != std::string::npos)
        {
            relativePath = std::filesystem::path(ship.displayName.empty() ? "ship" : ship.displayName);
        }
        const std::filesystem::path destinationShipFolder = destinationRoot / relativePath;
        if (!this->copyShipFolderBaseFiles(
                ship.folderAbsolutePath.c_str(),
                destinationShipFolder.string().c_str()) ||
            !this->exportScaledShipSpritesToFolder(
                ship,
                destinationShipFolder.string().c_str()))
        {
            failedCount += 1;
            continue;
        }

        exportedCount += 1;
    }

    if (exportedCount > 0 && failedCount == 0)
    {
        this->statusMessage = "Export navires OK: " + std::to_string(exportedCount) + " dossier(s).";
        return true;
    }
    if (exportedCount > 0)
    {
        this->statusMessage =
            "Export navires partiel: " +
            std::to_string(exportedCount) + " OK, " +
            std::to_string(failedCount) + " echec(s).";
        return true;
    }

    this->statusMessage = "Export navires en echec.";
    return false;
}

void EditorMapShipDownscaleScene::spawnTestShipAtTile(int tileX, int tileY)
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

void EditorMapShipDownscaleScene::moveTestShipToTile(int tileX, int tileY)
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

void EditorMapShipDownscaleScene::updateTestShip(double dt)
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

void EditorMapShipDownscaleScene::drawTestShip(void)
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

bool EditorMapShipDownscaleScene::handleShipToolClick(float x, float y, RC2D_MouseButton button)
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
void EditorMapShipDownscaleScene::drawWorldGridAndBlockedTiles(void) const
{
    const Map& map = GetCurrentMap();
    const int minTileX = 0;
    const int maxTileX = map.getWidthTiles() - 1;
    const int minTileY = 0;
    const int maxTileY = map.getHeightTiles() - 1;

    const float tileWidth = map.getTileWidth();
    const float tileHeight = map.getTileHeight();

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    for (int tileY = minTileY; tileY <= maxTileY; ++tileY)
    {
        for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
        {
            const SDL_FPoint center = map.tileToScreenCenter(tileX, tileY);

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
    for (const TowerHotspot& hotspot : this->towerHotspots)
    {
        if (!map.isInside(hotspot.tileX, hotspot.tileY))
        {
            continue;
        }
        const SDL_FPoint center = map.tileToScreenCenter(hotspot.tileX, hotspot.tileY);
        rc2d_graphics_setColor(hotspotColor);
        rc2d_graphics_drawTileIsometric("fill", center.x, center.y, tileWidth, tileHeight);
        rc2d_graphics_setColor(RC2D_Color{245, 250, 255, 240});
        rc2d_graphics_drawTileIsometric("line", center.x, center.y, tileWidth, tileHeight);

        if (this->overlayFont.sdl_font != nullptr)
        {
            RC2D_Text hotspotText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "H");
            hotspotText.color = RC2D_Color{255, 255, 255, 250};
            rc2d_graphics_setTextColor(&hotspotText);

            int textW = 0;
            int textH = 0;
            rc2d_graphics_getTextSize(&hotspotText, &textW, &textH);
            const float textX = center.x - (static_cast<float>(textW) * 0.5f);
            const float textY = center.y - (static_cast<float>(textH) * 0.5f);
            rc2d_graphics_drawText(&hotspotText, textX, textY);
            rc2d_graphics_destroyText(&hotspotText);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapShipDownscaleScene::drawPlacedAssets(void) const
{
    const Map& map = GetCurrentMap();
    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);

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
            return assetA.tileY < assetB.tileY;
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

    if (!this->shipSpawnPlacementEnabled &&
        this->editorTool == EditorTool::PLACE_ASSETS &&
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

void EditorMapShipDownscaleScene::drawImportedShips(void) const
{
    const Map& map = GetCurrentMap();
    if (map.getWidthTiles() <= 0 || map.getHeightTiles() <= 0)
    {
        return;
    }
    const float worldZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const int desiredSpriteIndex = this->computeCurrentShipPreviewSpriteIndex();
    const int hpSpriteStart = this->shipsLowHpEnabled ? 5 : 1;
    const int hpSpriteEnd = hpSpriteStart + 3;
    auto isSpriteReady = [](const ImportedShip& ship, int spriteIndex) -> bool {
        if (spriteIndex < 1 || spriteIndex > kShipSpriteCount)
        {
            return false;
        }
        const size_t spriteOffset = static_cast<size_t>(spriteIndex - 1);
        return ship.previewSpriteLoaded[spriteOffset] &&
            ship.previewImages[spriteOffset].sdl_texture != nullptr &&
            ship.previewImageData[spriteOffset].sdl_surface != nullptr;
    };
    auto resolveSpriteOffset = [&](const ImportedShip& ship, int preferredIndex) -> int {
        if (preferredIndex < 1 || preferredIndex > kShipSpriteCount)
        {
            preferredIndex = desiredSpriteIndex;
        }
        if (isSpriteReady(ship, preferredIndex))
        {
            return preferredIndex - 1;
        }
        for (int spriteIndex = hpSpriteStart; spriteIndex <= hpSpriteEnd; ++spriteIndex)
        {
            if (isSpriteReady(ship, spriteIndex))
            {
                return spriteIndex - 1;
            }
        }
        for (int spriteIndex = 1; spriteIndex <= kShipSpriteCount; ++spriteIndex)
        {
            if (isSpriteReady(ship, spriteIndex))
            {
                return spriteIndex - 1;
            }
        }
        return -1;
    };

    if (!this->shipsSimulationEnabled)
    {
        std::vector<size_t> drawOrder;
        drawOrder.reserve(this->spawnedShips.size());
        for (size_t i = 0; i < this->spawnedShips.size(); ++i)
        {
            const SpawnedShipInstance& spawnedShip = this->spawnedShips[i];
            if (spawnedShip.importedShipIndex < 0 ||
                spawnedShip.importedShipIndex >= static_cast<int>(this->importedShips.size()))
            {
                continue;
            }
            const ImportedShip& importedShip = this->importedShips[static_cast<size_t>(spawnedShip.importedShipIndex)];
            if (!importedShip.previewSpriteLoaded[0] ||
                importedShip.previewImages[0].sdl_texture == nullptr ||
                importedShip.previewImageData[0].sdl_surface == nullptr)
            {
                continue;
            }
            drawOrder.push_back(i);
        }

        std::sort(
            drawOrder.begin(),
            drawOrder.end(),
            [this](size_t a, size_t b) {
                const SpawnedShipInstance& spawnA = this->spawnedShips[a];
                const SpawnedShipInstance& spawnB = this->spawnedShips[b];
                const float depthA = spawnA.tileX + spawnA.tileY;
                const float depthB = spawnB.tileX + spawnB.tileY;
                if (std::fabs(depthA - depthB) > 0.0001f)
                {
                    return depthA < depthB;
                }
                return spawnA.tileY < spawnB.tileY;
            });

        for (const size_t orderIndex : drawOrder)
        {
            const SpawnedShipInstance& spawnedShip = this->spawnedShips[orderIndex];
            const ImportedShip& importedShip = this->importedShips[static_cast<size_t>(spawnedShip.importedShipIndex)];
            const RC2D_Image& previewImage = importedShip.previewImages[0];
            const RC2D_ImageData& previewImageData = importedShip.previewImageData[0];
            const float spriteWidthPx = static_cast<float>(previewImageData.sdl_surface->w);
            const float spriteHeightPx = static_cast<float>(previewImageData.sdl_surface->h);
            const float drawScale =
                static_cast<float>(std::clamp(spawnedShip.scalePercent, 5, 100)) / 100.0f * worldZoom;
            const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(spawnedShip.tileX, spawnedShip.tileY);
            const float drawX = anchorScreen.x;
            const float drawY = anchorScreen.y;

            const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
                const_cast<RC2D_Image*>(&previewImage),
                0.0f,
                0.0f,
                spriteWidthPx,
                spriteHeightPx);

            rc2d_graphics_drawQuad(
                const_cast<RC2D_Image*>(&previewImage),
                &sourceQuad,
                drawX,
                drawY,
                0.0,
                drawScale,
                drawScale,
                0.0f,
                0.0f,
                false,
                false);

            if (spawnedShip.importedShipIndex == this->selectedShipIndex)
            {
                const float drawWidth = spriteWidthPx * drawScale;
                const float drawHeight = spriteHeightPx * drawScale;
                SDL_FRect highlightRect{
                    drawX - 2.0f,
                    drawY - 2.0f,
                    drawWidth + 4.0f,
                    drawHeight + 4.0f};
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
                rc2d_graphics_setColor(RC2D_Color{240, 225, 120, 220});
                rc2d_graphics_rectangle("line", &highlightRect);
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
            }
        }

        if (this->shipSpawnPlacementEnabled &&
            this->hoveredTileValid &&
            this->selectedShipIndex >= 0 &&
            this->selectedShipIndex < static_cast<int>(this->importedShips.size()) &&
            map.isInside(this->hoveredTile.x, this->hoveredTile.y))
        {
            const ImportedShip& selectedShip =
                this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
            if (selectedShip.previewSpriteLoaded[0] &&
                selectedShip.previewImages[0].sdl_texture != nullptr &&
                selectedShip.previewImageData[0].sdl_surface != nullptr)
            {
                const RC2D_Image& previewImage = selectedShip.previewImages[0];
                const RC2D_ImageData& previewImageData = selectedShip.previewImageData[0];
                const float spriteWidthPx = static_cast<float>(previewImageData.sdl_surface->w);
                const float spriteHeightPx = static_cast<float>(previewImageData.sdl_surface->h);
                const float drawScale =
                    static_cast<float>(std::clamp(selectedShip.scalePercent, 5, 100)) / 100.0f * worldZoom;
                const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(
                    static_cast<float>(this->hoveredTile.x),
                    static_cast<float>(this->hoveredTile.y));
                const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
                    const_cast<RC2D_Image*>(&previewImage),
                    0.0f,
                    0.0f,
                    spriteWidthPx,
                    spriteHeightPx);

                Uint8 oldAlpha = 255;
                SDL_GetTextureAlphaMod(previewImage.sdl_texture, &oldAlpha);
                SDL_SetTextureAlphaMod(previewImage.sdl_texture, 170);
                rc2d_graphics_drawQuad(
                    const_cast<RC2D_Image*>(&previewImage),
                    &sourceQuad,
                    anchorScreen.x,
                    anchorScreen.y,
                    0.0,
                    drawScale,
                    drawScale,
                    0.0f,
                    0.0f,
                    false,
                    false);
                SDL_SetTextureAlphaMod(previewImage.sdl_texture, oldAlpha);
            }
        }
        return;
    }

    const std::size_t simulationCount =
        (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);
    if (simulationCount == 0U)
    {
        return;
    }

    std::vector<size_t> drawOrder;
    drawOrder.reserve(simulationCount);
    for (size_t i = 0; i < simulationCount; ++i)
    {
        if (this->importedShips[i].simulationShip == nullptr)
        {
            continue;
        }

        drawOrder.push_back(i);
    }
    if (drawOrder.empty())
    {
        return;
    }

    std::sort(
        drawOrder.begin(),
        drawOrder.end(),
        [this](size_t a, size_t b) {
            const SDL_FPoint posA = this->importedShips[a].simulationShip->getPositionTile();
            const SDL_FPoint posB = this->importedShips[b].simulationShip->getPositionTile();
            const float depthA = posA.x + posA.y;
            const float depthB = posB.x + posB.y;
            if (std::fabs(depthA - depthB) > 0.0001f)
            {
                return depthA < depthB;
            }
            return posA.y < posB.y;
        });

    for (const size_t i : drawOrder)
    {
        const ImportedShip& ship = this->importedShips[i];
        const int preferredSpriteIndex = simulationPreviewDirectionToSpriteIndex(
            ship.simulationShip->getCurrentPreviewDirection(),
            this->shipsLowHpEnabled);
        const int spriteOffset = resolveSpriteOffset(ship, preferredSpriteIndex);
        if (spriteOffset < 0)
        {
            continue;
        }

        const RC2D_Image& previewImage = ship.previewImages[static_cast<size_t>(spriteOffset)];
        const RC2D_ImageData& previewImageData = ship.previewImageData[static_cast<size_t>(spriteOffset)];
        const float spriteWidthPx = static_cast<float>(previewImageData.sdl_surface->w);
        const float spriteHeightPx = static_cast<float>(previewImageData.sdl_surface->h);

        const SDL_FPoint simPos = ship.simulationShip->getPositionTile();
        float drawTileX = simPos.x;
        float drawTileY = simPos.y;
        drawTileX = std::clamp(drawTileX, 0.0f, static_cast<float>(map.getWidthTiles() - 1));
        drawTileY = std::clamp(drawTileY, 0.0f, static_cast<float>(map.getHeightTiles() - 1));
        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(drawTileX, drawTileY);
        const float shipScale = static_cast<float>(std::clamp(ship.scalePercent, 5, 100)) / 100.0f;
        const float drawScale = shipScale * worldZoom;
        const float drawX = anchorScreen.x;
        const float drawY = anchorScreen.y;

        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&previewImage),
            0.0f,
            0.0f,
            spriteWidthPx,
            spriteHeightPx);

        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&previewImage),
            &sourceQuad,
            drawX,
            drawY,
            0.0,
            drawScale,
            drawScale,
            0.0f,
            0.0f,
            false,
            false);

        if (static_cast<int>(i) == this->selectedShipIndex)
        {
            const float drawWidth = spriteWidthPx * drawScale;
            const float drawHeight = spriteHeightPx * drawScale;
            SDL_FRect highlightRect{
                drawX - 2.0f,
                drawY - 2.0f,
                drawWidth + 4.0f,
                drawHeight + 4.0f};
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            rc2d_graphics_setColor(RC2D_Color{240, 225, 120, 220});
            rc2d_graphics_rectangle("line", &highlightRect);
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        }
    }
}


void EditorMapShipDownscaleScene::updateToolbarLayout(void)
{
    // Barre d'actions simplifiee dans la bande UI basse (70 px reserves).
    const Map& map = GetCurrentMap();
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

    // Ligne 1: actions principales de l'editeur ship-downscale.
    float x = startX;
    setNextButton(&this->buttonImportRect, &x, row1Y, 160.0f);
    setNextButton(&this->buttonExportRect, &x, row1Y, 138.0f);
    setNextButton(&this->buttonSimulationRect, &x, row1Y, 170.0f);
    setNextButton(&this->buttonShipSpawnRect, &x, row1Y, 172.0f);
    setNextButton(&this->buttonShipsHpRect, &x, row1Y, 120.0f);
    setNextButton(&this->buttonListsVisibilityRect, &x, row1Y, 120.0f);
    setNextButton(&this->buttonToolPlaceRect, &x, row1Y, 110.0f);
    setNextButton(&this->buttonToolRemoveRect, &x, row1Y, 150.0f);
    setNextButton(&this->buttonUndoRect, &x, row1Y, 92.0f);
    setNextButton(&this->buttonRedoRect, &x, row1Y, 92.0f);

    // Ligne 2: navigation / selection / zoom.
    x = startX;
    setNextButton(&this->buttonAssetPrevRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonAssetNextRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonShipScaleMinusRect, &x, row2Y, 90.0f);
    setNextButton(&this->buttonShipScalePlusRect, &x, row2Y, 90.0f);
    this->shipScaleInputRect = SDL_FRect{x, row2Y, 128.0f, h};
    x += this->shipScaleInputRect.w + gap;
    setNextButton(&this->buttonOceanPrevRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonGridRect, &x, row2Y, 74.0f);
    setNextButton(&this->buttonCenterRect, &x, row2Y, 126.0f);
    setNextButton(&this->buttonZoomOutRect, &x, row2Y, 64.0f);
    setNextButton(&this->buttonZoomInRect, &x, row2Y, 64.0f);

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
}

void EditorMapShipDownscaleScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
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

int EditorMapShipDownscaleScene::getAssetListMaxScrollOffset(void) const
{
    const int assetCount = static_cast<int>(this->importedAssets.size());
    return (std::max)(assetCount - kAssetListVisibleRows, 0);
}

void EditorMapShipDownscaleScene::clampAssetListScrollOffset(void)
{
    this->assetListScrollOffset = std::clamp(this->assetListScrollOffset, 0, this->getAssetListMaxScrollOffset());
}

void EditorMapShipDownscaleScene::ensureSelectedAssetVisible(void)
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

int EditorMapShipDownscaleScene::computeAssetListStartIndex(void) const
{
    const int maxOffset = this->getAssetListMaxScrollOffset();
    return std::clamp(this->assetListScrollOffset, 0, maxOffset);
}

bool EditorMapShipDownscaleScene::handleAssetListClick(float x, float y)
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

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        const int assetCount = static_cast<int>(this->importedAssets.size());
        const int maxOffset = (std::max)(assetCount - kAssetListVisibleRows, 0);
        if (maxOffset <= 0)
        {
            this->assetListScrollDragActive = false;
            return true;
        }

        float thumbHeight = scrollTrackRect.h;
        float thumbY = scrollTrackRect.y;
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(assetCount));
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
            this->assetListScrollOffset = static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
            this->clampAssetListScrollOffset();

            this->assetListScrollDragActive = true;
            this->assetListScrollDragGrabOffsetY = scrollThumbRect.h * 0.5f;
        }
        return true;
    }

    // Clic dans la liste hors scrollbar: stop drag scrollbar.
    this->assetListScrollDragActive = false;

    if (this->importedAssets.empty())
    {
        this->statusMessage = "Aucun asset importe.";
        return true;
    }

    const int startIndex = this->computeAssetListStartIndex();
    for (int i = 0; i < kAssetListVisibleRows; ++i)
    {
        const int assetIndex = startIndex + i;
        if (assetIndex >= static_cast<int>(this->importedAssets.size()))
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

        this->selectedAssetIndex = assetIndex;
        this->ensureSelectedAssetVisible();
        const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        this->statusMessage = "Asset selectionne: " + selectedAsset.displayName;
        return true;
    }

    // Consomme quand meme le clic dans le panneau pour eviter un paint tile.
    return true;
}

void EditorMapShipDownscaleScene::handleAssetListScrollDragFromMouse(void)
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

    const int assetCount = static_cast<int>(this->importedAssets.size());
    const int maxOffset = (std::max)(assetCount - kAssetListVisibleRows, 0);
    if (maxOffset <= 0)
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        return;
    }

    float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(assetCount));
    const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
    const float targetThumbY = std::clamp(
        mouseY - this->assetListScrollDragGrabOffsetY,
        scrollTrackRect.y,
        scrollTrackRect.y + thumbTravel);
    const float ratio = (thumbTravel > 0.0f)
        ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
        : 0.0f;

    this->assetListScrollOffset = static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
    this->clampAssetListScrollOffset();
}

void EditorMapShipDownscaleScene::drawAssetListPanel(void) const
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

    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text headerText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Assets charges");
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

    if (this->importedAssets.empty())
    {
        if (this->overlayFont.sdl_font != nullptr)
        {
            RC2D_Text emptyText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Aucun asset");
            emptyText.color = kHudStatusColor;
            rc2d_graphics_setTextColor(&emptyText);
            rc2d_graphics_drawText(&emptyText, this->assetListRect.x + panelPadding, rowsTopY + 2.0f);
            rc2d_graphics_destroyText(&emptyText);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int assetCount = static_cast<int>(this->importedAssets.size());
    const int startIndex = this->computeAssetListStartIndex();
    const int maxOffset = (std::max)(assetCount - kAssetListVisibleRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kAssetListVisibleRows)) / static_cast<float>(assetCount));
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
        const int assetIndex = startIndex + i;
        if (assetIndex >= assetCount)
        {
            break;
        }

        const bool isSelected = (assetIndex == this->selectedAssetIndex);
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

        std::string rowLabel = makeAssetLabel(this->importedAssets[static_cast<size_t>(assetIndex)].displayName, 24);
        char textBuffer[256] = {};
        SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", assetIndex + 1, rowLabel.c_str());

        RC2D_Text rowText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), textBuffer);
        rowText.color = RC2D_Color{235, 242, 250, 248};
        rc2d_graphics_setTextColor(&rowText);
        rc2d_graphics_drawText(&rowText, rowRect.x + 4.0f, rowRect.y + 1.0f);
        rc2d_graphics_destroyText(&rowText);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

int EditorMapShipDownscaleScene::getShipListMaxScrollOffset(void) const
{
    const int shipCount = static_cast<int>(this->importedShips.size());
    return (std::max)(shipCount - kAssetListVisibleRows, 0);
}

void EditorMapShipDownscaleScene::clampShipListScrollOffset(void)
{
    this->shipListScrollOffset = std::clamp(this->shipListScrollOffset, 0, this->getShipListMaxScrollOffset());
}

void EditorMapShipDownscaleScene::ensureSelectedShipVisible(void)
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

int EditorMapShipDownscaleScene::computeShipListStartIndex(void) const
{
    const int maxOffset = this->getShipListMaxScrollOffset();
    return std::clamp(this->shipListScrollOffset, 0, maxOffset);
}

bool EditorMapShipDownscaleScene::handleShipListClick(float x, float y)
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

void EditorMapShipDownscaleScene::handleShipListScrollDragFromMouse(void)
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

void EditorMapShipDownscaleScene::drawShipListPanel(void) const
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








bool EditorMapShipDownscaleScene::tryBuildMiniMapViewRect(SDL_FRect* outRect) const
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

void EditorMapShipDownscaleScene::moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset)
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

bool EditorMapShipDownscaleScene::handleMiniMapClick(float x, float y, RC2D_MouseButton button)
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

void EditorMapShipDownscaleScene::handleMiniMapDragFromMouse(void)
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

void EditorMapShipDownscaleScene::drawMiniMap(void) const
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

    const Map& map = GetCurrentMap();
    const int mapWidthTiles = (std::max)(map.getWidthTiles(), 1);
    const int mapHeightTiles = (std::max)(map.getHeightTiles(), 1);
    if (this->shipsSimulationEnabled)
    {
        const std::size_t simulationCount =
            (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);
        for (std::size_t i = 0; i < simulationCount; ++i)
        {
            const ImportedShip& ship = this->importedShips[i];
            if (ship.simulationShip == nullptr)
            {
                continue;
            }
            const SDL_FPoint simPos = ship.simulationShip->getPositionTile();
            float markerTileX = simPos.x;
            float markerTileY = simPos.y;
            markerTileX = std::clamp(markerTileX, 0.0f, static_cast<float>(mapWidthTiles - 1));
            markerTileY = std::clamp(markerTileY, 0.0f, static_cast<float>(mapHeightTiles - 1));

            const SDL_FPoint shipSector = map.tileToSectorFloat(markerTileX, markerTileY);
            const float nx = std::clamp((shipSector.x + 0.5f) / static_cast<float>(Map::NUM_SECTORS_X), 0.0f, 1.0f);
            const float ny = std::clamp((shipSector.y + 0.5f) / static_cast<float>(Map::NUM_SECTORS_Y), 0.0f, 1.0f);

            SDL_FRect pixelRect{};
            pixelRect.w = 2.0f;
            pixelRect.h = 2.0f;
            pixelRect.x = this->miniMapRect.x + (nx * this->miniMapRect.w) - 1.0f;
            pixelRect.y = this->miniMapRect.y + (ny * this->miniMapRect.h) - 1.0f;
            rc2d_graphics_setColor(RC2D_Color{245, 45, 45, 235});
            rc2d_graphics_rectangle("fill", &pixelRect);
        }
    }
    else
    {
        for (const SpawnedShipInstance& spawnedShip : this->spawnedShips)
        {
            const float markerTileX = std::clamp(spawnedShip.tileX, 0.0f, static_cast<float>(mapWidthTiles - 1));
            const float markerTileY = std::clamp(spawnedShip.tileY, 0.0f, static_cast<float>(mapHeightTiles - 1));
            const SDL_FPoint shipSector = map.tileToSectorFloat(markerTileX, markerTileY);
            const float nx = std::clamp((shipSector.x + 0.5f) / static_cast<float>(Map::NUM_SECTORS_X), 0.0f, 1.0f);
            const float ny = std::clamp((shipSector.y + 0.5f) / static_cast<float>(Map::NUM_SECTORS_Y), 0.0f, 1.0f);

            SDL_FRect pixelRect{};
            pixelRect.w = 2.0f;
            pixelRect.h = 2.0f;
            pixelRect.x = this->miniMapRect.x + (nx * this->miniMapRect.w) - 1.0f;
            pixelRect.y = this->miniMapRect.y + (ny * this->miniMapRect.h) - 1.0f;
            rc2d_graphics_setColor(RC2D_Color{245, 45, 45, 235});
            rc2d_graphics_rectangle("fill", &pixelRect);
        }
    }

    if (this->testShipLoaded && this->testShipSpawned)
    {
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

bool EditorMapShipDownscaleScene::pointInRect(float x, float y, const SDL_FRect& rect) const
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
bool EditorMapShipDownscaleScene::handleToolbarClick(float x, float y)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    if (this->shipScaleInputActive && !this->pointInRect(x, y, this->shipScaleInputRect))
    {
        this->commitShipScaleInputEdit();
    }
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
    if (this->pointInRect(x, y, this->buttonExportRect))
    {
        this->openExportMapDialog();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonSimulationRect))
    {
        this->setShipsSimulationEnabled(!this->shipsSimulationEnabled);
        if (this->shipsSimulationEnabled)
        {
            this->shipSpawnPlacementEnabled = false;
            const std::size_t simulationCount =
                (std::min)(this->importedShips.size(), kSimulationRuntimeShipsCap);
            if (this->importedShips.size() > simulationCount)
            {
                this->statusMessage =
                    "Simulation ON: " + std::to_string(simulationCount) +
                    " navire(s) actifs (cap runtime).";
            }
            else
            {
                this->statusMessage = "Simulation navires: ON";
            }
        }
        else
        {
            this->statusMessage = "Simulation navires: OFF";
        }
        return true;
    }
    if (this->pointInRect(x, y, this->buttonShipSpawnRect))
    {
        if (!this->shipSpawnPlacementEnabled && this->shipsSimulationEnabled)
        {
            this->statusMessage = "Desactive d'abord SIMULATION pour activer SPAWN NAVIRE.";
            return true;
        }
        this->shipSpawnPlacementEnabled = !this->shipSpawnPlacementEnabled;
        if (this->shipSpawnPlacementEnabled)
        {
            if (this->editorTool == EditorTool::PLACE_ASSETS)
            {
                this->editorTool = EditorTool::REMOVE_ASSETS;
            }
        }
        this->statusMessage = this->shipSpawnPlacementEnabled
            ? "SPAWN NAVIRE: ON (Pose asset desactivee)"
            : "SPAWN NAVIRE: OFF";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonShipsHpRect))
    {
        this->setShipsLowHpEnabled(!this->shipsLowHpEnabled);
        this->statusMessage = this->shipsLowHpEnabled
            ? "Mode navires: BAS HP"
            : "Mode navires: FULL HP";
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
    if (this->pointInRect(x, y, this->buttonToolPlaceRect))
    {
        this->shipSpawnPlacementEnabled = false;
        this->editorTool = EditorTool::PLACE_ASSETS;
        this->statusMessage = "Mode Pose visuel: clic gauche pose asset, clic droit supprime.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonToolRemoveRect))
    {
        this->shipSpawnPlacementEnabled = false;
        this->editorTool = EditorTool::REMOVE_ASSETS;
        this->statusMessage = "Mode Suppression visuel: clic gauche supprime asset.";
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
    if (this->pointInRect(x, y, this->buttonShipScaleMinusRect))
    {
        this->setShipScalePercent(this->shipScalePercent - 5);
        this->statusMessage = "Echelle navire: " + std::to_string(this->shipScalePercent) + "%";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonShipScalePlusRect))
    {
        this->setShipScalePercent(this->shipScalePercent + 5);
        this->statusMessage = "Echelle navire: " + std::to_string(this->shipScalePercent) + "%";
        return true;
    }
    if (this->pointInRect(x, y, this->shipScaleInputRect))
    {
        this->beginShipScaleInputEdit();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonGridRect))
    {
        this->showGrid = !this->showGrid;
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
void EditorMapShipDownscaleScene::drawEditorHud(void) const
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
    const char* toolLabel = "Navigation";
    if (this->shipSpawnPlacementEnabled)
    {
        toolLabel = "Spawn navire";
    }
    else if (this->editorTool == EditorTool::PLACE_ASSETS)
    {
        toolLabel = "Pose visuel";
    }
    else if (this->editorTool == EditorTool::REMOVE_ASSETS)
    {
        toolLabel = "Suppression visuel";
    }
    const char* oceanLabel = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label;
    this->drawToolbarButton(this->buttonImportRect, "IMPORTER ASSETS", false);
    this->drawToolbarButton(this->buttonExportRect, "EXPORTER SHIPS", false);
    this->drawToolbarButton(
        this->buttonSimulationRect,
        this->shipsSimulationEnabled ? "SIMULATION ON" : "SIMULATION OFF",
        this->shipsSimulationEnabled);
    this->drawToolbarButton(
        this->buttonShipSpawnRect,
        this->shipSpawnPlacementEnabled ? "SPAWN NAVIRE ON" : "SPAWN NAVIRE OFF",
        this->shipSpawnPlacementEnabled);
    this->drawToolbarButton(
        this->buttonShipsHpRect,
        this->shipsLowHpEnabled ? "BAS HP" : "FULL HP",
        this->shipsLowHpEnabled);
    this->drawToolbarButton(
        this->buttonListsVisibilityRect,
        this->showBottomRightLists ? "LISTES ON" : "LISTES OFF",
        this->showBottomRightLists);
    this->drawToolbarButton(this->buttonUndoRect, "Annuler", this->canUndoHistory());
    this->drawToolbarButton(this->buttonRedoRect, "Refaire", this->canRedoHistory());
    this->drawToolbarButton(
        this->buttonToolPlaceRect,
        "Pose Asset",
        this->editorTool == EditorTool::PLACE_ASSETS && !this->shipSpawnPlacementEnabled);
    this->drawToolbarButton(this->buttonToolRemoveRect, "Supprimer asset", this->editorTool == EditorTool::REMOVE_ASSETS);
    this->drawToolbarButton(this->buttonAssetPrevRect, "Asset precedent", false);
    this->drawToolbarButton(this->buttonAssetNextRect, "Asset suivant", false);
    this->drawToolbarButton(this->buttonShipScaleMinusRect, "Ship -5%", false);
    this->drawToolbarButton(this->buttonShipScalePlusRect, "Ship +5%", false);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(
        this->shipScaleInputActive
            ? RC2D_Color{95, 145, 190, 210}
            : RC2D_Color{36, 44, 52, 190});
    rc2d_graphics_rectangle("fill", &this->shipScaleInputRect);
    rc2d_graphics_setColor(
        this->shipScaleInputActive
            ? RC2D_Color{160, 215, 255, 250}
            : RC2D_Color{140, 150, 165, 220});
    rc2d_graphics_rectangle("line", &this->shipScaleInputRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    if (this->overlayFont.sdl_font != nullptr)
    {
        const std::string valueText = this->shipScaleInputActive
            ? this->shipScaleInputBuffer
            : std::to_string(this->shipScalePercent);
        const std::string labelText = "Scale %: " + valueText;
        RC2D_Text scaleText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), labelText.c_str());
        scaleText.color = RC2D_Color{235, 242, 250, 248};
        rc2d_graphics_setTextColor(&scaleText);
        rc2d_graphics_drawText(&scaleText, this->shipScaleInputRect.x + 7.0f, this->shipScaleInputRect.y + 3.0f);
        rc2d_graphics_destroyText(&scaleText);
    }
    this->drawToolbarButton(this->buttonOceanPrevRect, "Ocean -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "Ocean +", false);
    this->drawToolbarButton(this->buttonGridRect, "Lignes", this->showGrid);
    this->drawToolbarButton(this->buttonCenterRect, "CENTRER MAP", false);
    this->drawToolbarButton(this->buttonZoomOutRect, "Zoom-", false);
    this->drawToolbarButton(this->buttonZoomInRect, "Zoom+", false);
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
        "EDITOR SHIP DOWNSCALE | Outil:%s | Ocean:%s | Grille:%s | %s",
        toolLabel,
        oceanLabel,
        this->showGrid ? "ON" : "OFF",
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
        "Assets importes:%d | Assets poses:%d | Selection:%s | Navires:%d | Navire actif:%s | Spawns:%d",
        static_cast<int>(this->importedAssets.size()),
        static_cast<int>(this->placedAssets.size()),
        selectedAssetName,
        static_cast<int>(this->importedShips.size()),
        selectedShipName,
        static_cast<int>(this->spawnedShips.size()));
    char line3[1024] = {};
    SDL_snprintf(
        line3,
        sizeof(line3),
        "Spawn navire:%s | Clic gauche map:%s | Input %%:%s",
        this->shipSpawnPlacementEnabled ? "ON" : "OFF",
        this->shipSpawnPlacementEnabled ? "SPAWN" : "DESACTIVE",
        this->shipScaleInputActive ? "EDIT" : "IDLE");
    char line4[512] = {};
    SDL_snprintf(
        line4,
        sizeof(line4),
        "Simulation:%s | HP:%s | ShipScaleSel:%d%% | Cache TITLE navires:%s",
        this->shipsSimulationEnabled ? "ON" : "OFF",
        this->shipsLowHpEnabled ? "BAS" : "FULL",
        this->shipScalePercent,
        "actif");
    const Map& map = GetCurrentMap();
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    drawLine(line0, gameScreenRect.x + 14.0f, gameScreenRect.y + 5.0f, kHudTextColor);
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
}
void EditorMapShipDownscaleScene::onImportAssetDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapShipDownscaleScene* scene = static_cast<EditorMapShipDownscaleScene*>(userdata);
    if (scene == nullptr || scene != EditorMapShipDownscaleScene::activeInstance)
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


void EditorMapShipDownscaleScene::onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapShipDownscaleScene* scene = static_cast<EditorMapShipDownscaleScene*>(userdata);
    if (scene == nullptr || scene != EditorMapShipDownscaleScene::activeInstance)
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

void EditorMapShipDownscaleScene::onImportMapDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapShipDownscaleScene* scene = static_cast<EditorMapShipDownscaleScene*>(userdata);
    if (scene == nullptr || scene != EditorMapShipDownscaleScene::activeInstance)
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

void EditorMapShipDownscaleScene::onExportMapDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapShipDownscaleScene* scene = static_cast<EditorMapShipDownscaleScene*>(userdata);
    if (scene == nullptr || scene != EditorMapShipDownscaleScene::activeInstance)
    {
        return;
    }

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->statusMessage = "Export annule.";
        return;
    }

    scene->exportAllShipsScaledToFolder(filelist[0]);
}

void EditorMapShipDownscaleScene::unload(void)
{
    if (EditorMapShipDownscaleScene::activeInstance == this)
    {
        EditorMapShipDownscaleScene::activeInstance = nullptr;
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
    this->unloadImportedShips();
    this->unloadImportedAssets();
    ResetStorageFontRef(&this->overlayFont);
    this->backgroundWidget.unload();

    RC2D_log(RC2D_LOG_INFO, "EditorMapShipDownscaleScene: unloaded");
}

void EditorMapShipDownscaleScene::load(void)
{
    EditorMapShipDownscaleScene::activeInstance = this;
    this->resetEditorState();
    this->unloadImportedShips();
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
    this->statusMessage = "Editor ship downscale charge.";
    {
        std::error_code fsError;
        const std::filesystem::path defaultShipsRoot("assets/images/ships");
        if (std::filesystem::exists(defaultShipsRoot, fsError) &&
            std::filesystem::is_directory(defaultShipsRoot, fsError))
        {
            this->importShipsFromRootFolderAbsolutePath(defaultShipsRoot.string().c_str());
        }
    }

    RC2D_log(RC2D_LOG_INFO, "EditorMapShipDownscaleScene: loaded");
}

void EditorMapShipDownscaleScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    // Traite d'abord les imports differees pour rester hors pass de rendu GPU.
    this->processPendingImportRequests();
    this->processDeferredAssetLoads();
    this->processPendingShipFolderRequest();
    this->processShipImportBatch();
    this->processPendingMapImportRequest();
    if (this->initialShipsLoadingScreenActive)
    {
        this->initialShipsLoadingProcessed = static_cast<int>(this->shipImportBatchNextIndex);
        if (!this->shipImportBatchActive)
        {
            this->initialShipsLoadingScreenActive = false;
            this->initialShipsLoadingProcessed = this->initialShipsLoadingTotal;
        }
        this->updateToolbarLayout();
        return;
    }

    this->advanceImportedShipsSpriteAnimation(dt);
    this->processDeferredShipPreviewLoads();

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

    this->updateImportedShipsSimulation(dt);
    this->updateTestShip(dt);
    camera.update(map, map.rect);

    this->updateHoveredTile();
    this->handleTilePaintFromMouseDrag();
}

void EditorMapShipDownscaleScene::draw(void)
{
    Map& map = GetCurrentMap();

    if (this->initialShipsLoadingScreenActive)
    {
        const SDL_FRect fullRect = GetGameScreen().rect;
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 255});
        rc2d_graphics_rectangle("fill", &fullRect);

        const int total = (std::max)(this->initialShipsLoadingTotal, 1);
        const int processed = std::clamp(this->initialShipsLoadingProcessed, 0, total);
        const float progress = static_cast<float>(processed) / static_cast<float>(total);
        const int percent = static_cast<int>(std::lround(progress * 100.0f));

        SDL_FRect barOuter{};
        barOuter.w = (std::max)(420.0f, fullRect.w * 0.48f);
        barOuter.h = 34.0f;
        barOuter.x = fullRect.x + ((fullRect.w - barOuter.w) * 0.5f);
        barOuter.y = fullRect.y + ((fullRect.h - barOuter.h) * 0.5f);

        SDL_FRect barFill = barOuter;
        barFill.w = std::clamp(barOuter.w * progress, 0.0f, barOuter.w);

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{24, 24, 24, 255});
        rc2d_graphics_rectangle("fill", &barOuter);
        rc2d_graphics_setColor(RC2D_Color{210, 210, 210, 240});
        rc2d_graphics_rectangle("line", &barOuter);
        rc2d_graphics_setColor(RC2D_Color{235, 235, 235, 250});
        rc2d_graphics_rectangle("fill", &barFill);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        if (this->overlayFont.sdl_font != nullptr)
        {
            auto drawLine = [this](const char* text, float x, float y, RC2D_Color color) {
                RC2D_Text renderedText =
                    rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
                renderedText.color = color;
                rc2d_graphics_setTextColor(&renderedText);
                rc2d_graphics_drawText(&renderedText, x, y);
                rc2d_graphics_destroyText(&renderedText);
            };

            char line1[256] = {};
            SDL_snprintf(
                line1,
                sizeof(line1),
                "CHARGEMENT NAVIRES... %d%%",
                percent);
            char line2[256] = {};
            SDL_snprintf(
                line2,
                sizeof(line2),
                "%d / %d dossiers navires charges",
                processed,
                total);

            drawLine(
                line1,
                barOuter.x,
                barOuter.y - 40.0f,
                RC2D_Color{245, 245, 245, 255});
            drawLine(
                line2,
                barOuter.x,
                barOuter.y + barOuter.h + 10.0f,
                RC2D_Color{215, 215, 215, 255});
        }
        return;
    }

    this->backgroundWidget.draw();

    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    if (GetOceanShader().isReady())
    {
        GetOceanShader().draw(map.rect);
    }

    this->drawWorldGridAndBlockedTiles();
    this->drawPlacedAssets();
    this->drawImportedShips();
    this->drawTestShip();
    this->scrollBarOverlay.draw(map.rect, map);

    WorldRenderClip::end(renderer);
    this->drawEditorHud();
}
// ---------------------------------------------------------------------------
// Entrees utilisateur.
// ---------------------------------------------------------------------------
void EditorMapShipDownscaleScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)keyboardID;
    if (this->initialShipsLoadingScreenActive)
    {
        return;
    }
    if (this->handleShipScaleInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    // Echap desactive dans l'editor pour eviter toute fermeture involontaire.
    if (scancode == SDL_SCANCODE_ESCAPE)
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
    if (scancode == SDL_SCANCODE_P && !isrepeat)
    {
        this->shipSpawnPlacementEnabled = false;
        this->editorTool = EditorTool::PLACE_ASSETS;
        this->statusMessage = "Mode Pose visuel: clic gauche pose asset, clic droit supprime.";
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
    if (scancode == SDL_SCANCODE_DELETE && !isrepeat)
    {
        this->placedAssets.clear();
        this->historyActions.clear();
        this->historyCursor = 0;
        this->statusMessage = "Assets poses supprimes (historique reset).";
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
void EditorMapShipDownscaleScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;
    if (this->initialShipsLoadingScreenActive)
    {
        return;
    }
    Map& map = GetCurrentMap();
    // RC2D convertit deja les events via SDL_ConvertEventToRenderCoordinates.
    const float renderX = x;
    const float renderY = y;
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
        this->shipListScrollDragActive = false;
        this->shipListScrollDragGrabOffsetY = 0.0f;
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
    if (this->shipSpawnPlacementEnabled)
    {
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            if (this->shipsSimulationEnabled)
            {
                this->statusMessage = "SPAWN NAVIRE indisponible pendant SIMULATION ON.";
                return;
            }
            const SDL_Point tile = map.screenToTileNearest(renderX, renderY);
            this->spawnSelectedImportedShipAtTile(tile.x, tile.y);
            return;
        }
        if (button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            return;
        }
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

void EditorMapShipDownscaleScene::mousewheelmoved(
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
    if (this->initialShipsLoadingScreenActive)
    {
        return;
    }

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
    if (this->showBottomRightLists && this->pointInRect(renderX, renderY, this->assetListRect))
    {
        this->assetListScrollOffset += (delta > 0) ? -step : step;
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

