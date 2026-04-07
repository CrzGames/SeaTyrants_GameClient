
#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-creatormap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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
// Export minimap:
// - taille derivee de la map secteurs (pas de taille fixe 1024x1024).
// - 1 secteur = SECTOR_STEP pixels dans l'image exportee.
// - ratio visuel commun minimap ecran + export PNG: 2/3 (soit 1/3 plus petit).
constexpr int kMiniMapScaleNumerator = 2;
constexpr int kMiniMapScaleDenominator = 3;
constexpr int kExportMiniMapPixelsPerSectorRaw =
    (Map::SECTOR_STEP * kMiniMapScaleNumerator) / kMiniMapScaleDenominator;
constexpr int kExportMiniMapPixelsPerSector =
    (kExportMiniMapPixelsPerSectorRaw > 0) ? kExportMiniMapPixelsPerSectorRaw : 1;

constexpr RC2D_FileDialogFilter kImportFilters[] = {
    {"Images", "png;jpg;jpeg;bmp;webp;tga"},
    {"Tous les fichiers", "*"},
};

constexpr RC2D_FileDialogFilter kExportFilters[] = {
    {"JSON", "json"},
    {"Tous les fichiers", "*"},
};

constexpr RC2D_FileDialogFilter kShipFolderFilters[] = {
    {"Dossier navire", "*"},
};

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
    : backgroundUiImage{},
      overlayFont{},
      scrollBarOverlay{},
      editorMode(EditorMode::MAP_CREATOR_MAP),
      editorTool(EditorTool::BLOCK_TILES),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      selectedAssetIndex(-1),
      assetListScrollOffset(0),
      showGrid(true),
      hoveredTileValid(false),
      hoveredTile{},
      dragPaintActive(false),
      dragPaintBlockedValue(true),
      lastDragPaintTileValid(false),
      lastDragPaintTile{},
      importedAssets{},
      placedAssets{},
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
      buttonImportRect{},
      buttonImportShipRect{},
      buttonExportRect{},
      buttonUndoRect{},
      buttonRedoRect{},
      buttonToolBlockRect{},
      buttonToolPlaceRect{},
      buttonToolRemoveRect{},
      buttonToolShipRect{},
      buttonToolShipControlRect{},
      buttonAssetPrevRect{},
      buttonAssetNextRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonGridRect{},
      buttonCenterRect{},
      buttonCenterShipRect{},
      buttonZoomOutRect{},
      buttonZoomInRect{},
      assetListRect{},
      miniMapRect{},
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
    this->assetListScrollOffset = 0;
    this->showGrid = true;
    this->hoveredTileValid = false;
    this->dragPaintActive = false;
    this->dragPaintBlockedValue = true;
    this->lastDragPaintTileValid = false;
    this->historyActions.clear();
    this->historyCursor = 0;
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
    this->assetListScrollDragActive = false;
    this->assetListScrollDragGrabOffsetY = 0.0f;
    this->clickMarker.hide();
    this->clickMarker.setDurationSeconds(0.85);
    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();
    this->testShipLoaded = false;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->loadedShipFolderAbsolute.clear();
    this->pendingShipFolderDialogCompleted = false;
    this->pendingShipFolderDialogCanceled = false;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        this->pendingShipFolderAbsolute.clear();
    }
    this->buttonImportRect = SDL_FRect{};
    this->buttonImportShipRect = SDL_FRect{};
    this->buttonExportRect = SDL_FRect{};
    this->buttonUndoRect = SDL_FRect{};
    this->buttonRedoRect = SDL_FRect{};
    this->buttonToolBlockRect = SDL_FRect{};
    this->buttonToolPlaceRect = SDL_FRect{};
    this->buttonToolRemoveRect = SDL_FRect{};
    this->buttonToolShipRect = SDL_FRect{};
    this->buttonToolShipControlRect = SDL_FRect{};
    this->buttonAssetPrevRect = SDL_FRect{};
    this->buttonAssetNextRect = SDL_FRect{};
    this->buttonOceanPrevRect = SDL_FRect{};
    this->buttonOceanNextRect = SDL_FRect{};
    this->buttonGridRect = SDL_FRect{};
    this->buttonCenterRect = SDL_FRect{};
    this->buttonCenterShipRect = SDL_FRect{};
    this->buttonZoomOutRect = SDL_FRect{};
    this->buttonZoomInRect = SDL_FRect{};
    this->assetListRect = SDL_FRect{};
    this->miniMapRect = SDL_FRect{};
    this->miniMapDragActive = false;
    this->miniMapDragOffsetX = 0.0f;
    this->miniMapDragOffsetY = 0.0f;
}

void EditorMapCreateMapScene::unloadImportedAssets(void)
{
    for (ImportedAsset& asset : this->importedAssets)
    {
        rc2d_graphics_freeImageData(&asset.imageData);
        rc2d_graphics_freeImage(&asset.image);
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

void EditorMapCreateMapScene::paintTileAtMouse(bool blocked)
{
    SDL_Point tile{};
    if (!this->tryGetMouseTile(&tile))
    {
        return;
    }

    if (this->setTileBlockedWithHistory(tile.x, tile.y, blocked))
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

    const bool blockedValue = leftDown ? true : false;
    if (this->dragPaintActive &&
        this->dragPaintBlockedValue == blockedValue &&
        this->lastDragPaintTileValid &&
        this->lastDragPaintTile.x == tile.x &&
        this->lastDragPaintTile.y == tile.y)
    {
        return;
    }

    if (!this->setTileBlockedWithHistory(tile.x, tile.y, blockedValue))
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

    Map& map = GetCurrentMap();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY) || !this->isInsideMapRect(mouseX, mouseY))
    {
        return;
    }

    const SDL_FPoint anchorTile = map.screenToTile(mouseX, mouseY);
    SDL_Point tile = map.roundTile(anchorTile.x, anchorTile.y);
    tile = map.clampTile(tile.x, tile.y);

    for (PlacedAsset& placedAsset : this->placedAssets)
    {
        if (placedAsset.tileX == tile.x && placedAsset.tileY == tile.y)
        {
            const PlacedAsset beforeAsset = placedAsset;

            placedAsset.importedAssetIndex = this->selectedAssetIndex;
            placedAsset.anchorTileX = anchorTile.x;
            placedAsset.anchorTileY = anchorTile.y;
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
    placedAsset.anchorTileX = anchorTile.x;
    placedAsset.anchorTileY = anchorTile.y;
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

    RC2D_Image image = rc2d_graphics_loadImageFromStorage(storagePath, RC2D_STORAGE_USER);
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
        rc2d_graphics_loadImageDataFromStorage(storagePath, RC2D_STORAGE_USER);

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
        for (int y = startY; y < endY; ++y)
        {
            const float v = ((static_cast<float>(y) + 0.5f) - dstY) / dstH;
            if (v < 0.0f || v > 1.0f)
            {
                continue;
            }

            const int srcY = std::clamp(
                static_cast<int>(std::floor(v * static_cast<float>(srcH))),
                0,
                srcH - 1);
            const size_t srcRowOffset = static_cast<size_t>(srcY * srcW);
            const size_t dstRowOffset = static_cast<size_t>(y * outWidth);

            for (int x = startX; x < endX; ++x)
            {
                const float u = ((static_cast<float>(x) + 0.5f) - dstX) / dstW;
                if (u < 0.0f || u > 1.0f)
                {
                    continue;
                }

                const int srcX = std::clamp(
                    static_cast<int>(std::floor(u * static_cast<float>(srcW))),
                    0,
                    srcW - 1);

                const Uint8 alpha = importedAsset.alphaMask[srcRowOffset + static_cast<size_t>(srcX)];
                if (alpha <= alphaThreshold)
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

    const int miniMapWidthPx = (std::max)(
        Map::NUM_SECTORS_X * kExportMiniMapPixelsPerSector,
        1);
    const int miniMapHeightPx = (std::max)(
        Map::NUM_SECTORS_Y * kExportMiniMapPixelsPerSector,
        1);

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

    const Map& map = GetCurrentMap();
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON.";
        return false;
    }

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

void EditorMapCreateMapScene::openExportMapDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kExportFilters;
    options.num_filters = static_cast<int>(std::size(kExportFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Exporter la map en JSON";
    options.accept_label = "Exporter";
    options.cancel_label = "Annuler";
    rc2d_filedialog_saveFile(&EditorMapCreateMapScene::onExportMapDialogResult, this, &options);
}

void EditorMapCreateMapScene::openImportShipFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kShipFolderFilters;
    options.num_filters = static_cast<int>(std::size(kShipFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier navire (1.png..8.png)";
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
        this->statusMessage = "Import dossier navire annule.";
        return;
    }

    this->loadShipFolderFromAbsolutePath(selectedFolder.c_str());
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

    this->testShip.unloadSprites();
    this->testShipPreview.unloadSprites();
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

    this->testShip.setSpeedTilesPerSecond(4.0f);
    this->testShip.setHealthVisual(Ship::HealthVisual::FULL);
    this->testShipPreview.setSpeedTilesPerSecond(4.0f);
    this->testShipPreview.setHealthVisual(Ship::HealthVisual::FULL);
    this->testShipLoaded = true;
    this->testShipSpawned = false;
    this->testShipCameraFollowEnabled = false;
    this->loadedShipFolderAbsolute = normalizePathSlashes(folderPath.string());
    this->editorTool = EditorTool::SPAWN_SHIP;
    this->statusMessage = "Navire test charge. Clique gauche sur la map pour le spawn.";
    return true;
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

            if (map.isTileBlocked(tileX, tileY))
            {
                rc2d_graphics_setColor(kBlockedTileColor);
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
        const SDL_FPoint hoveredCenter = map.tileToScreenCenter(this->hoveredTile.x, this->hoveredTile.y);
        rc2d_graphics_setColor(kHoverTileColor);
        rc2d_graphics_drawTileIsometric("line", hoveredCenter.x, hoveredCenter.y, tileWidth, tileHeight);
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
    }

    if (this->editorTool == EditorTool::PLACE_ASSETS &&
        this->selectedAssetIndex >= 0 &&
        this->selectedAssetIndex < static_cast<int>(this->importedAssets.size()))
    {
        const ImportedAsset& selectedAsset = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)];
        if (selectedAsset.image.sdl_texture != nullptr)
        {
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            if (!this->getMouseRenderPosition(&mouseX, &mouseY) || !this->isInsideMapRect(mouseX, mouseY))
            {
                return;
            }

            const float drawX = mouseX;
            const float drawY = mouseY;
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

void EditorMapCreateMapScene::updateToolbarLayout(void)
{
    // Barre d'actions dans la bande UI basse (70 px reserves).
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

    // Ligne 1: actions principales (sans bouton quitter).
    float x = startX;
    setNextButton(&this->buttonImportRect, &x, row1Y, 160.0f);
    setNextButton(&this->buttonImportShipRect, &x, row1Y, 164.0f);
    setNextButton(&this->buttonExportRect, &x, row1Y, 138.0f);
    setNextButton(&this->buttonUndoRect, &x, row1Y, 92.0f);
    setNextButton(&this->buttonRedoRect, &x, row1Y, 92.0f);
    setNextButton(&this->buttonToolBlockRect, &x, row1Y, 100.0f);
    setNextButton(&this->buttonToolPlaceRect, &x, row1Y, 100.0f);
    setNextButton(&this->buttonToolRemoveRect, &x, row1Y, 156.0f);
    setNextButton(&this->buttonToolShipRect, &x, row1Y, 135.0f);
    setNextButton(&this->buttonToolShipControlRect, &x, row1Y, 155.0f);
    setNextButton(&this->buttonGridRect, &x, row1Y, 74.0f);
    setNextButton(&this->buttonCenterRect, &x, row1Y, 126.0f);

    // Ligne 2: centrage navire, selection d'asset, ocean et zoom.
    x = startX;
    setNextButton(&this->buttonCenterShipRect, &x, row2Y, 150.0f);
    setNextButton(&this->buttonAssetPrevRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonAssetNextRect, &x, row2Y, 124.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, row2Y, 92.0f);
    setNextButton(&this->buttonZoomOutRect, &x, row2Y, 64.0f);
    setNextButton(&this->buttonZoomInRect, &x, row2Y, 64.0f);

    // Mini-liste d'assets en bas a droite, dans la zone map.
    this->assetListRect.w = 250.0f;
    this->assetListRect.h = 276.0f;
    this->assetListRect.x = map.rect.x + map.rect.w - this->assetListRect.w - 40.0f;
    this->assetListRect.y = map.rect.y + map.rect.h - this->assetListRect.h - 40.0f;

    // Minimap maison en haut a droite dans la zone monde.
    // Minimap 1/3 plus petite (largeur + hauteur), tout en gardant le meme
    // comportement de clamp responsive.
    constexpr float miniMapScale = 2.0f / 3.0f;
    const float miniMapSize = std::clamp(
        map.rect.h * 0.22f * miniMapScale,
        120.0f * miniMapScale,
        190.0f * miniMapScale);
    this->miniMapRect.w = miniMapSize;
    this->miniMapRect.h = miniMapSize;
    this->miniMapRect.x = map.rect.x + map.rect.w - this->miniMapRect.w - 40.0f;
    this->miniMapRect.y = map.rect.y + 40.0f;
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
    const int assetCount = static_cast<int>(this->importedAssets.size());
    return (std::max)(assetCount - kAssetListVisibleRows, 0);
}

void EditorMapCreateMapScene::clampAssetListScrollOffset(void)
{
    this->assetListScrollOffset = std::clamp(this->assetListScrollOffset, 0, this->getAssetListMaxScrollOffset());
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
            (std::max)(static_cast<int>(std::lround(this->miniMapRect.w)), 1);
        const int miniMapHeightPx =
            (std::max)(static_cast<int>(std::lround(this->miniMapRect.h)), 1);

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

    if (this->pointInRect(x, y, this->buttonImportShipRect))
    {
        this->openImportShipFolderDialog();
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
        this->editorTool = EditorTool::BLOCK_TILES;
        this->statusMessage = "Mode Collision: clic gauche bloque, clic droit debloque.";
        return true;
    }

    if (this->pointInRect(x, y, this->buttonToolPlaceRect))
    {
        this->editorTool = EditorTool::PLACE_ASSETS;
        this->statusMessage = "Mode Pose asset: clic gauche pose, clic droit supprime.";
        return true;
    }

    if (this->pointInRect(x, y, this->buttonToolRemoveRect))
    {
        this->editorTool = EditorTool::REMOVE_ASSETS;
        this->statusMessage = "Mode Suppression asset: clic gauche supprime.";
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

    if (this->pointInRect(x, y, this->buttonGridRect))
    {
        this->showGrid = !this->showGrid;
        this->statusMessage = this->showGrid ? "Lignes de tuiles: ON" : "Lignes de tuiles: OFF";
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
        return true;
    }

    if (this->pointInRect(x, y, this->buttonZoomInRect))
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        camera.update(map, map.rect);
        return true;
    }

    if (this->handleAssetListClick(x, y))
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
        toolLabel = "Pose asset";
    }
    else if (this->editorTool == EditorTool::REMOVE_ASSETS)
    {
        toolLabel = "Suppression asset";
    }
    else if (this->editorTool == EditorTool::SPAWN_SHIP)
    {
        toolLabel = "Spawn navire";
    }
    else if (this->editorTool == EditorTool::CONTROL_SHIP)
    {
        toolLabel = "Control navire";
    }
    const char* oceanLabel = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label;

    // Barre de boutons cliquables.
    this->drawToolbarButton(this->buttonImportRect, "IMPORTER ASSETS", false);
    this->drawToolbarButton(this->buttonImportShipRect, "IMPORTER NAVIRE", false);
    this->drawToolbarButton(this->buttonExportRect, "EXPORTER MAP", false);
    this->drawToolbarButton(this->buttonUndoRect, "Annuler", this->canUndoHistory());
    this->drawToolbarButton(this->buttonRedoRect, "Refaire", this->canRedoHistory());
    this->drawToolbarButton(this->buttonToolBlockRect, "Collision", this->editorTool == EditorTool::BLOCK_TILES);
    this->drawToolbarButton(this->buttonToolPlaceRect, "Pose Asset", this->editorTool == EditorTool::PLACE_ASSETS);
    this->drawToolbarButton(this->buttonToolRemoveRect, "Supprimer asset", this->editorTool == EditorTool::REMOVE_ASSETS);
    this->drawToolbarButton(this->buttonToolShipRect, "SPAWN NAVIRE", this->editorTool == EditorTool::SPAWN_SHIP);
    this->drawToolbarButton(this->buttonToolShipControlRect, "CONTROL NAVIRE", this->editorTool == EditorTool::CONTROL_SHIP);
    this->drawToolbarButton(this->buttonAssetPrevRect, "Asset precedent", false);
    this->drawToolbarButton(this->buttonAssetNextRect, "Asset suivant", false);
    this->drawToolbarButton(this->buttonOceanPrevRect, "Ocean -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "Ocean +", false);
    this->drawToolbarButton(this->buttonGridRect, "Lignes", this->showGrid);
    this->drawToolbarButton(this->buttonCenterRect, "CENTRER MAP", false);
    this->drawToolbarButton(this->buttonCenterShipRect, "CENTRER NAVIRE", this->testShipCameraFollowEnabled);
    this->drawToolbarButton(this->buttonZoomOutRect, "Zoom-", false);
    this->drawToolbarButton(this->buttonZoomInRect, "Zoom+", false);

    char line0[1024] = {};
    SDL_snprintf(
        line0,
        sizeof(line0),
        "EDITOR MAP | Mode:MAP_CREATOR_MAP | Outil:%s | Ocean:%s | Grille:%s",
        toolLabel,
        oceanLabel,
        this->showGrid ? "ON" : "OFF");

    char line1[1024] = {};
    SDL_snprintf(
        line1,
        sizeof(line1),
        "Status: %s",
        this->statusMessage.c_str());

    char line2[1024] = {};
    const char* selectedAssetName = "Aucun";
    if (this->selectedAssetIndex >= 0 &&
        this->selectedAssetIndex < static_cast<int>(this->importedAssets.size()))
    {
        selectedAssetName = this->importedAssets[static_cast<size_t>(this->selectedAssetIndex)].displayName.c_str();
    }
    SDL_snprintf(
        line2,
        sizeof(line2),
        "Assets importes:%d | Assets poses:%d | Selection:%s",
        static_cast<int>(this->importedAssets.size()),
        static_cast<int>(this->placedAssets.size()),
        selectedAssetName);

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

    const Map& map = GetCurrentMap();
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    // Infos compactes en haut a gauche.
    drawLine(line0, gameScreenRect.x + 14.0f, gameScreenRect.y + 5.0f, kHudTextColor);
    drawLine(line1, gameScreenRect.x + 14.0f, gameScreenRect.y + 19.0f, kHudStatusColor);
    drawLine(line3, gameScreenRect.x + 14.0f, gameScreenRect.y + 33.0f, kHudTextColor);

    // Coordonnees tuile sous le pointeur en haut-centre.
    char tileHoverText[128] = {};
    if (this->hoveredTileValid)
    {
        SDL_snprintf(
            tileHoverText,
            sizeof(tileHoverText),
            "Tile X:%d | Y:%d",
            this->hoveredTile.x,
            this->hoveredTile.y);
    }
    else
    {
        SDL_snprintf(tileHoverText, sizeof(tileHoverText), "Tile: hors map");
    }

    RC2D_Text tileText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), tileHoverText);
    tileText.color = RC2D_Color{245, 250, 255, 248};
    rc2d_graphics_setTextColor(&tileText);
    int tileTextW = 0;
    int tileTextH = 0;
    rc2d_graphics_getTextSize(&tileText, &tileTextW, &tileTextH);
    SDL_FRect tileInfoRect{};
    tileInfoRect.w = (std::max)(static_cast<float>(tileTextW) + 24.0f, 180.0f);
    tileInfoRect.h = static_cast<float>(tileTextH) + 8.0f;
    tileInfoRect.x = gameScreenRect.x + ((gameScreenRect.w - tileInfoRect.w) * 0.5f);
    tileInfoRect.y = gameScreenRect.y + 3.0f;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{18, 28, 40, 205});
    rc2d_graphics_rectangle("fill", &tileInfoRect);
    rc2d_graphics_setColor(RC2D_Color{150, 178, 206, 240});
    rc2d_graphics_rectangle("line", &tileInfoRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    const float tileTextX = tileInfoRect.x + ((tileInfoRect.w - static_cast<float>(tileTextW)) * 0.5f);
    const float tileTextY = tileInfoRect.y + ((tileInfoRect.h - static_cast<float>(tileTextH)) * 0.5f);
    rc2d_graphics_drawText(&tileText, tileTextX, tileTextY);
    rc2d_graphics_destroyText(&tileText);

    // Infos detaillees en bas a gauche, dans la zone map.
    drawLine(line2, 14.0f, map.rect.y + map.rect.h - 20.0f, kHudTextColor);

    // Minimap maison en haut a droite.
    this->drawMiniMap();

    // Mini-liste assets cliquable en bas a droite.
    this->drawAssetListPanel();
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

    scene->exportMapToAbsolutePath(filelist[0]);
}

void EditorMapCreateMapScene::unload(void)
{
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
    this->unloadImportedAssets();
    rc2d_graphics_closeFont(&this->overlayFont);
    rc2d_graphics_freeImage(&this->backgroundUiImage);

    RC2D_log(RC2D_LOG_INFO, "EditorMapCreateMapScene: unloaded");
}

void EditorMapCreateMapScene::load(void)
{
    EditorMapCreateMapScene::activeInstance = this;
    this->resetEditorState();
    this->unloadImportedAssets();
    this->ensureUserStorageFolders();

    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/background-ui-ingame.png",
        RC2D_STORAGE_TITLE);

    this->overlayFont = rc2d_graphics_openFontFromStorage(
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
    this->statusMessage = "Editor map charge.";

    RC2D_log(RC2D_LOG_INFO, "EditorMapCreateMapScene: loaded");
}

void EditorMapCreateMapScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    // Traite d'abord les imports differees pour rester hors pass de rendu GPU.
    this->processPendingImportRequests();
    this->processPendingShipFolderRequest();

    map.update();
    this->updateToolbarLayout();
    this->clampAssetListScrollOffset();
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
    this->handleAssetListScrollDragFromMouse();
    this->updateTestShip(dt);
    camera.update(map, map.rect);

    this->updateHoveredTile();
    this->handleTilePaintFromMouseDrag();
}

void EditorMapCreateMapScene::draw(void)
{
    Map& map = GetCurrentMap();

    if (this->backgroundUiImage.sdl_texture != nullptr)
    {
        rc2d_graphics_drawImage(
            &this->backgroundUiImage,
            0.0f,
            0.0f,
            0.0,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            false,
            false);
    }

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
        this->statusMessage = "Mode Pose asset: clic gauche pose, clic droit supprime.";
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

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->assetListScrollDragActive = false;
        this->assetListScrollDragGrabOffsetY = 0.0f;
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

    // Les barres de scroll consomment le clic gauche dans la zone map.
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
            this->paintTileAtMouse(true);
        }
        else if (button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            this->paintTileAtMouse(false);
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
}

#endif // GAME_ENV_DEV

