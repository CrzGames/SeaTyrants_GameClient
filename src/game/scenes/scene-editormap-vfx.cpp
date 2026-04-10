#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-vfx.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_storage.h>
#include <SDL3/SDL_surface.h>
#include <cJSON.h>

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"

namespace
{
struct OceanColorEntry
{
    OceanShader::WaterColor value;
    const char* label;
};

struct RenderItem
{
    bool isShip;
    int drawOrder;
    uint32_t instanceId;
    int instanceIndex;
};

constexpr int kShipSpriteCount = 8;
constexpr float kListScrollBarWidth = 10.0f;
constexpr int kVisibleListRows = 10;
constexpr float kSfxFpsMin = 1.0f;
constexpr float kSfxFpsMax = 9999.0f;
constexpr int kLooseScaleMinPercent = 5;
constexpr int kLooseScaleMaxPercent = 100;
constexpr int kLooseScaleStepPercent = 5;
constexpr float kVfxMoveStepPx = 8.0f;
constexpr float kLoosePreviewZoomMin = 0.40f;
constexpr float kLoosePreviewZoomMax = 1.00f;
constexpr float kLoosePreviewZoomDefault = 1.00f;
constexpr float kLoosePreviewFpsDefault = 12.0f;

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

constexpr RC2D_Color kHudTextColor = RC2D_Color{235, 240, 248, 245};
constexpr RC2D_Color kHudStatusColor = RC2D_Color{230, 200, 90, 250};
constexpr RC2D_Color kPanelFillColor = RC2D_Color{20, 28, 36, 210};
constexpr RC2D_Color kPanelBorderColor = RC2D_Color{135, 150, 168, 220};
constexpr RC2D_Color kRowFillColor = RC2D_Color{32, 40, 50, 210};
constexpr RC2D_Color kRowSelectedFillColor = RC2D_Color{86, 130, 174, 220};
constexpr RC2D_Color kRowBorderColor = RC2D_Color{115, 128, 146, 210};

constexpr RC2D_FileDialogFilter kFolderFilters[] = {
    {"Dossier", "*"},
};

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

static std::string makeAssetLabel(const std::string& name, int maxChars)
{
    if (maxChars <= 3 || static_cast<int>(name.size()) <= maxChars)
    {
        return name;
    }

    return name.substr(0, static_cast<size_t>(maxChars - 3)) + "...";
}

static std::string formatSfxFpsValue(float fps)
{
    const float clamped = std::clamp(fps, kSfxFpsMin, kSfxFpsMax);
    char buffer[32] = {};
    if (std::fabs(clamped - std::round(clamped)) <= 0.001f)
    {
        SDL_snprintf(buffer, sizeof(buffer), "%.0f", clamped);
    }
    else
    {
        SDL_snprintf(buffer, sizeof(buffer), "%.2f", clamped);
    }

    return std::string(buffer);
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

static bool tryParseNumericFrameName(const std::string& frameName, unsigned long long* outValue)
{
    if (outValue == nullptr)
    {
        return false;
    }

    const std::string fileName = extractFileName(frameName);
    const size_t dotPos = fileName.find_last_of('.');
    const std::string stem = (dotPos == std::string::npos) ? fileName : fileName.substr(0, dotPos);
    if (stem.empty())
    {
        return false;
    }

    unsigned long long value = 0;
    for (char c : stem)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isdigit(uc))
        {
            return false;
        }
        value = (value * 10ULL) + static_cast<unsigned long long>(c - '0');
    }

    *outValue = value;
    return true;
}

static void sortFrameNamesByNumericOrder(std::vector<std::string>* frameNames)
{
    if (frameNames == nullptr)
    {
        return;
    }

    std::sort(frameNames->begin(), frameNames->end(), [](const std::string& a, const std::string& b) {
        unsigned long long aNum = 0;
        unsigned long long bNum = 0;
        const bool aIsNumeric = tryParseNumericFrameName(a, &aNum);
        const bool bIsNumeric = tryParseNumericFrameName(b, &bNum);
        if (aIsNumeric && bIsNumeric)
        {
            if (aNum != bNum)
            {
                return aNum < bNum;
            }
            return a < b;
        }
        if (aIsNumeric != bIsNumeric)
        {
            return aIsNumeric;
        }
        return a < b;
    });
}

static bool isPngFilePath(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".png";
}

static std::string makeExportAnimationSlug(const std::string& rawName)
{
    const std::string trimmed = trimAscii(rawName);
    if (trimmed.empty())
    {
        return {};
    }

    std::string slug;
    slug.reserve(trimmed.size());
    bool previousWasDash = false;
    for (char c : trimmed)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
        {
            slug.push_back(static_cast<char>(std::tolower(uc)));
            previousWasDash = false;
            continue;
        }

        if (c == ' ' || c == '_' || c == '-')
        {
            if (!slug.empty() && !previousWasDash)
            {
                slug.push_back('-');
                previousWasDash = true;
            }
        }
    }

    while (!slug.empty() && slug.front() == '-')
    {
        slug.erase(slug.begin());
    }
    while (!slug.empty() && slug.back() == '-')
    {
        slug.pop_back();
    }

    const std::string prefix = "vfx-";
    if (slug.rfind(prefix, 0U) == 0U)
    {
        slug.erase(0, prefix.size());
    }

    while (!slug.empty() && slug.front() == '-')
    {
        slug.erase(slug.begin());
    }

    return slug;
}
} // namespace

EditorMapVfxScene* EditorMapVfxScene::activeInstance = nullptr;

EditorMapVfxScene::EditorMapVfxScene(void)
    : backgroundUiImage{},
      overlayFont{},
      scrollBarOverlay{},
      editorMode(EditorMode::SHIP_VFX),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      importedShips{},
      importedSfx{},
      importedLooseFolders{},
      selectedShipIndex(-1),
      selectedSfxIndex(-1),
      selectedLooseFolderIndex(-1),
      shipListScrollOffset(0),
      sfxListScrollOffset(0),
      looseListScrollOffset(0),
      shipListScrollDragActive(false),
      sfxListScrollDragActive(false),
      looseListScrollDragActive(false),
      shipListScrollDragGrabOffsetY(0.0f),
      sfxListScrollDragGrabOffsetY(0.0f),
      looseListScrollDragGrabOffsetY(0.0f),
      previewShip{},
      previewShipLoaded(false),
      loadedShipFolderAbsolute{},
      previewShipTile{0.0f, 0.0f},
      shipDrawOrder(0),
      looseReferenceGuildIslandImage{},
      looseReferenceTowerLevel1Image{},
      looseReferenceTowerLevel2Image{},
      looseReferenceTowerLevel3Image{},
      looseReferenceTowerLevel4Image{},
      looseReferenceShipLeftImage{},
      looseReferenceShipRightImage{},
      looseReferencePreviewVisible(false),
      looseReferencePreviewLoaded(false),
      shipVfxInstances{},
      selectedVfxInstanceIndex(-1),
      nextVfxInstanceId(1U),
      vfxFpsInput("12"),
      vfxFpsInputFocused(false),
      looseScalePercent(100),
      loosePreviewZoomFactor(kLoosePreviewZoomDefault),
      loosePreviewMode(LoosePreviewMode::CENTER_SPRITESHEET),
      loosePreviewPlacements{},
      nextLoosePreviewPlacementId(1U),
      loosePreviewFpsInput("12"),
      loosePreviewFpsInputFocused(false),
      looseExportNamePopupVisible(false),
      looseExportNameInput{},
      pendingLooseExportAnimationName{},
      importedSfxCounter(0U),
      importedLooseFolderCounter(0U),
      statusMessage("Editor VFX pret."),
      pendingShipFolderDialogCompleted(false),
      pendingShipFolderDialogCanceled(false),
      pendingShipFolderAbsolute{},
      pendingShipFolderMutex{},
      pendingSfxFolderDialogCompleted(false),
      pendingSfxFolderDialogCanceled(false),
      pendingSfxFolderAbsolute{},
      pendingSfxFolderMutex{},
      pendingLooseFolderDialogCompleted(false),
      pendingLooseFolderDialogCanceled(false),
      pendingLooseFolderAbsolute{},
      pendingLooseFolderMutex{},
      pendingExportFolderDialogCompleted(false),
      pendingExportFolderDialogCanceled(false),
      pendingExportFolderAbsolute{},
      pendingExportMode(EditorMode::SHIP_VFX),
      pendingExportFolderMutex{},
      buttonModeShipVfxRect{},
      buttonModeLooseSpritesRect{},
      buttonImportShipRect{},
      buttonImportSfxRect{},
      buttonImportLooseRect{},
      buttonExportRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonShipOrderMinusRect{},
      buttonShipOrderPlusRect{},
      buttonVfxOrderMinusRect{},
      buttonVfxOrderPlusRect{},
      buttonRotateMinusRect{},
      buttonRotatePlusRect{},
      buttonFlipHorizontalRect{},
      buttonFlipVerticalRect{},
      buttonFollowShipRect{},
      buttonRemoveVfxRect{},
      buttonCenterVfxRect{},
      buttonVfxFpsInputRect{},
      buttonMoveULRect{},
      buttonMoveUpRect{},
      buttonMoveURRect{},
      buttonMoveLeftRect{},
      buttonMoveRightRect{},
      buttonMoveDLRect{},
      buttonMoveDownRect{},
      buttonMoveDRRect{},
      buttonLooseScaleMinusRect{},
      buttonLooseScalePlusRect{},
      buttonLooseReferencePreviewRect{},
      buttonLooseZoomMinusRect{},
      buttonLooseZoomPlusRect{},
      buttonLoosePreviewModeRect{},
      buttonLoosePreviewFpsInputRect{},
      buttonLooseClearAllVfxRect{},
      shipListRect{},
      sfxListRect{},
      looseListRect{}
{
}

EditorMapVfxScene::~EditorMapVfxScene(void)
{
}

void EditorMapVfxScene::resetEditorState(void)
{
    this->editorMode = EditorMode::SHIP_VFX;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->selectedShipIndex = -1;
    this->selectedSfxIndex = -1;
    this->selectedLooseFolderIndex = -1;
    this->shipListScrollOffset = 0;
    this->sfxListScrollOffset = 0;
    this->looseListScrollOffset = 0;
    this->shipListScrollDragActive = false;
    this->sfxListScrollDragActive = false;
    this->looseListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->sfxListScrollDragGrabOffsetY = 0.0f;
    this->looseListScrollDragGrabOffsetY = 0.0f;
    this->previewShip.unloadSprites();
    this->previewShipLoaded = false;
    this->loadedShipFolderAbsolute.clear();
    this->previewShipTile = SDL_FPoint{0.0f, 0.0f};
    this->shipDrawOrder = 0;
    this->shipVfxInstances.clear();
    this->selectedVfxInstanceIndex = -1;
    this->nextVfxInstanceId = 1U;
    this->vfxFpsInput = "12";
    this->vfxFpsInputFocused = false;
    this->looseScalePercent = 100;
    this->loosePreviewZoomFactor = kLoosePreviewZoomDefault;
    this->loosePreviewMode = LoosePreviewMode::CENTER_SPRITESHEET;
    this->loosePreviewPlacements.clear();
    this->nextLoosePreviewPlacementId = 1U;
    this->loosePreviewFpsInput = "12";
    this->loosePreviewFpsInputFocused = false;
    this->looseExportNamePopupVisible = false;
    this->looseExportNameInput.clear();
    this->pendingLooseExportAnimationName.clear();
    this->looseReferencePreviewVisible = false;
    this->importedShips.clear();
    this->unloadImportedSfx();
    this->unloadImportedLooseFolders();
    this->unloadLooseReferencePreviewAssets();
    this->importedSfxCounter = 0U;
    this->importedLooseFolderCounter = 0U;
    this->statusMessage = "Editor VFX pret.";
}

void EditorMapVfxScene::ensureUserStorageFolders(void)
{
    rc2d_storage_userMkdir("editor-vfx-ship");
    rc2d_storage_userMkdir("editor-vfx-ship/current");
    rc2d_storage_userMkdir("editor-vfx-sfx");
    rc2d_storage_userMkdir("editor-vfx-loose");
}

void EditorMapVfxScene::unloadImportedSfx(void)
{
    for (ImportedSfx& sfx : this->importedSfx)
    {
        rc2d_tp_freeAtlas(&sfx.atlas);
    }
    this->importedSfx.clear();
}

void EditorMapVfxScene::unloadImportedLooseFolders(void)
{
    for (ImportedLooseFolder& folder : this->importedLooseFolders)
    {
        for (ImportedLooseSprite& sprite : folder.sprites)
        {
            rc2d_graphics_freeImage(&sprite.image);
        }
        folder.sprites.clear();
    }
    this->importedLooseFolders.clear();
}

void EditorMapVfxScene::loadLooseReferencePreviewAssets(void)
{
    this->unloadLooseReferencePreviewAssets();

    this->looseReferenceGuildIslandImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/iles-guild/guild_island.png",
        RC2D_STORAGE_TITLE);
    if (this->looseReferenceGuildIslandImage.sdl_texture == nullptr)
    {
        // Compatibilite avec anciens assets.
        this->looseReferenceGuildIslandImage = rc2d_graphics_loadImageFromStorage(
            "assets/images/scene-editormap-vfx/iles-guild/test.png",
            RC2D_STORAGE_TITLE);
    }

    this->looseReferenceTowerLevel1Image = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl1.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel2Image = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl2.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel3Image = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl3.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel4Image = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl4.png",
        RC2D_STORAGE_TITLE);

    const bool islandLoaded = (this->looseReferenceGuildIslandImage.sdl_texture != nullptr);
    const bool tower1Loaded = (this->looseReferenceTowerLevel1Image.sdl_texture != nullptr);
    const bool tower2Loaded = (this->looseReferenceTowerLevel2Image.sdl_texture != nullptr);
    const bool tower3Loaded = (this->looseReferenceTowerLevel3Image.sdl_texture != nullptr);
    const bool tower4Loaded = (this->looseReferenceTowerLevel4Image.sdl_texture != nullptr);
    this->looseReferenceShipLeftImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/ships/test1/1.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceShipRightImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/scene-editormap-vfx/ships/test2/1.png",
        RC2D_STORAGE_TITLE);

    const bool shipLeftLoaded = (this->looseReferenceShipLeftImage.sdl_texture != nullptr);
    const bool shipRightLoaded = (this->looseReferenceShipRightImage.sdl_texture != nullptr);

    this->looseReferencePreviewLoaded =
        islandLoaded ||
        tower1Loaded ||
        tower2Loaded ||
        tower3Loaded ||
        tower4Loaded ||
        shipLeftLoaded ||
        shipRightLoaded;
    if (!this->looseReferencePreviewLoaded)
    {
        RC2D_log(RC2D_LOG_WARN, "EditorMapVfxScene: reference preview assets missing.");
    }
}

void EditorMapVfxScene::unloadLooseReferencePreviewAssets(void)
{
    rc2d_graphics_freeImage(&this->looseReferenceShipLeftImage);
    rc2d_graphics_freeImage(&this->looseReferenceShipRightImage);
    rc2d_graphics_freeImage(&this->looseReferenceGuildIslandImage);
    rc2d_graphics_freeImage(&this->looseReferenceTowerLevel1Image);
    rc2d_graphics_freeImage(&this->looseReferenceTowerLevel2Image);
    rc2d_graphics_freeImage(&this->looseReferenceTowerLevel3Image);
    rc2d_graphics_freeImage(&this->looseReferenceTowerLevel4Image);
    this->looseReferencePreviewLoaded = false;
}

void EditorMapVfxScene::adjustLoosePreviewZoom(float delta)
{
    if (this->editorMode == EditorMode::LOOSE_SPRITES &&
        this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
    {
        Camera& lockedCamera = GetCamera();
        Map& lockedMap = GetCurrentMap();
        lockedCamera.setZoomFactor(kLoosePreviewZoomDefault);
        lockedCamera.update(lockedMap, lockedMap.rect);
        this->statusMessage = "Mode spritesheet: zoom verrouille a 1.00.";
        return;
    }

    const float currentZoom = this->loosePreviewZoomFactor;
    const float nextZoom = std::clamp(currentZoom + delta, kLoosePreviewZoomMin, kLoosePreviewZoomMax);
    this->loosePreviewZoomFactor = nextZoom;
    Camera& camera = GetCamera();
    Map& map = GetCurrentMap();
    camera.setZoomFactor(nextZoom);
    camera.update(map, map.rect);

    char status[128] = {};
    SDL_snprintf(status, sizeof(status), "Zoom downscale: %.2f", nextZoom);
    this->statusMessage = status;
}

void EditorMapVfxScene::applySelectedOceanColor(void)
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
        RC2D_log(RC2D_LOG_WARN, "EditorMapVfxScene: echec chargement ocean shader (%s)", entry.label);
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapVfxScene::requestOceanColorStep(int delta)
{
    if (delta == 0)
    {
        return;
    }

    this->pendingOceanColorDelta += delta;
    this->pendingOceanColorDelta = (std::max)(this->pendingOceanColorDelta, -8);
    this->pendingOceanColorDelta = (std::min)(this->pendingOceanColorDelta, 8);
}

void EditorMapVfxScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    const int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapVfxScene::cycleOceanColor(int delta)
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

void EditorMapVfxScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();
    const float startX = 12.0f;
    const float topY = map.rect.y - 24.0f;
    const float row1Y = map.rect.y + map.rect.h + 4.0f;
    const float row2Y = row1Y + 30.0f;
    const float h = 24.0f;
    const float gap = 8.0f;

    auto setNextButton = [h, gap](SDL_FRect* rect, float* x, float y, float w) {
        rect->x = *x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        *x += w + gap;
    };

    float x = startX;
    setNextButton(&this->buttonModeShipVfxRect, &x, topY, 210.0f);
    setNextButton(&this->buttonModeLooseSpritesRect, &x, topY, 460.0f);
    setNextButton(&this->buttonExportRect, &x, topY, 126.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, topY, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, topY, 92.0f);

    x = startX;
    setNextButton(&this->buttonImportShipRect, &x, row1Y, 260.0f);
    setNextButton(&this->buttonImportSfxRect, &x, row1Y, 230.0f);
    setNextButton(&this->buttonImportLooseRect, &x, row1Y, 372.0f);
    setNextButton(&this->buttonShipOrderMinusRect, &x, row1Y, 96.0f);
    setNextButton(&this->buttonShipOrderPlusRect, &x, row1Y, 96.0f);
    setNextButton(&this->buttonVfxOrderMinusRect, &x, row1Y, 96.0f);
    setNextButton(&this->buttonVfxOrderPlusRect, &x, row1Y, 96.0f);
    setNextButton(&this->buttonRotateMinusRect, &x, row1Y, 96.0f);
    setNextButton(&this->buttonRotatePlusRect, &x, row1Y, 96.0f);

    // Mode downscale: tout sur la meme ligne (gauche zoom+repere, centre import, droite scale).
    const float looseZoomW = 110.0f;
    const float looseReferenceW = 320.0f;
    const float looseScaleW = 200.0f;
    const float looseImportPreferredW = 500.0f;
    const float looseImportMinW = 180.0f;

    float looseLeftX = startX;
    setNextButton(&this->buttonLooseZoomMinusRect, &looseLeftX, row1Y, looseZoomW);
    setNextButton(&this->buttonLooseZoomPlusRect, &looseLeftX, row1Y, looseZoomW);
    setNextButton(&this->buttonLooseReferencePreviewRect, &looseLeftX, row1Y, looseReferenceW);
    const float looseLeftEnd = looseLeftX - gap;

    const float looseRightGroupW = (looseScaleW * 2.0f) + gap;
    const float looseRightStart = map.rect.x + map.rect.w - startX - looseRightGroupW;
    this->buttonLooseScaleMinusRect = SDL_FRect{looseRightStart, row1Y, looseScaleW, h};
    this->buttonLooseScalePlusRect = SDL_FRect{looseRightStart + looseScaleW + gap, row1Y, looseScaleW, h};

    const float looseImportAvailableW = looseRightStart - gap - (looseLeftEnd + gap);
    float looseImportW = (std::min)(looseImportPreferredW, looseImportAvailableW);
    if (looseImportW < looseImportMinW)
    {
        looseImportW = (std::max)(0.0f, looseImportAvailableW);
    }

    float looseImportX = map.rect.x + ((map.rect.w - looseImportW) * 0.5f);
    const float looseImportMinX = looseLeftEnd + gap;
    const float looseImportMaxX = looseRightStart - gap - looseImportW;
    if (looseImportMaxX >= looseImportMinX)
    {
        looseImportX = std::clamp(looseImportX, looseImportMinX, looseImportMaxX);
    }
    else
    {
        looseImportX = looseImportMinX;
    }
    this->buttonImportLooseRect = SDL_FRect{looseImportX, row1Y, looseImportW, h};

    float looseRow2X = startX;
    setNextButton(&this->buttonLoosePreviewModeRect, &looseRow2X, row2Y, 280.0f);
    setNextButton(&this->buttonLoosePreviewFpsInputRect, &looseRow2X, row2Y, 240.0f);
    setNextButton(&this->buttonLooseClearAllVfxRect, &looseRow2X, row2Y, 170.0f);

    x = startX;
    setNextButton(&this->buttonFlipHorizontalRect, &x, row2Y, 90.0f);
    setNextButton(&this->buttonFlipVerticalRect, &x, row2Y, 90.0f);
    setNextButton(&this->buttonRemoveVfxRect, &x, row2Y, 126.0f);
    setNextButton(&this->buttonVfxFpsInputRect, &x, row2Y, 220.0f);

    this->shipListRect.w = 250.0f;
    this->shipListRect.h = 276.0f;
    this->shipListRect.x = map.rect.x + map.rect.w - this->shipListRect.w - 40.0f;
    this->shipListRect.y = map.rect.y + map.rect.h - this->shipListRect.h - 40.0f;

    this->sfxListRect = this->shipListRect;
    this->sfxListRect.x = this->shipListRect.x - this->sfxListRect.w - 16.0f;
    this->sfxListRect.x = (std::max)(this->sfxListRect.x, map.rect.x + 12.0f);

    this->looseListRect = this->shipListRect;
}

void EditorMapVfxScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
{
    RC2D_Color fillColor = active ? RC2D_Color{95, 145, 190, 210} : RC2D_Color{36, 44, 52, 190};
    RC2D_Color borderColor = active ? RC2D_Color{160, 215, 255, 250} : RC2D_Color{140, 150, 165, 220};

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
    text.color = kHudTextColor;
    rc2d_graphics_setTextColor(&text);

    int textW = 0;
    int textH = 0;
    rc2d_graphics_getTextSize(&text, &textW, &textH);
    rc2d_graphics_drawText(&text, rect.x + ((rect.w - static_cast<float>(textW)) * 0.5f), rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f));
    rc2d_graphics_destroyText(&text);
}

bool EditorMapVfxScene::pointInRect(float x, float y, const SDL_FRect& rect) const
{
    return x >= rect.x &&
        y >= rect.y &&
        x <= (rect.x + rect.w) &&
        y <= (rect.y + rect.h);
}

void EditorMapVfxScene::convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const
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

bool EditorMapVfxScene::getMouseRenderPosition(float* outX, float* outY) const
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

int EditorMapVfxScene::getListMaxScrollOffset(int itemCount) const
{
    return (std::max)(itemCount - kVisibleListRows, 0);
}

void EditorMapVfxScene::clampListScrollOffset(int* scrollOffset, int itemCount) const
{
    if (scrollOffset == nullptr)
    {
        return;
    }
    *scrollOffset = std::clamp(*scrollOffset, 0, this->getListMaxScrollOffset(itemCount));
}

void EditorMapVfxScene::ensureSelectionVisible(int selectedIndex, int* scrollOffset, int itemCount) const
{
    if (scrollOffset == nullptr)
    {
        return;
    }

    this->clampListScrollOffset(scrollOffset, itemCount);
    if (selectedIndex < 0)
    {
        return;
    }

    if (selectedIndex < *scrollOffset)
    {
        *scrollOffset = selectedIndex;
        this->clampListScrollOffset(scrollOffset, itemCount);
        return;
    }

    const int lastVisibleIndex = *scrollOffset + kVisibleListRows - 1;
    if (selectedIndex > lastVisibleIndex)
    {
        *scrollOffset = selectedIndex - (kVisibleListRows - 1);
        this->clampListScrollOffset(scrollOffset, itemCount);
    }
}

int EditorMapVfxScene::computeListStartIndex(int scrollOffset, int itemCount) const
{
    return std::clamp(scrollOffset, 0, this->getListMaxScrollOffset(itemCount));
}

bool EditorMapVfxScene::handleListPanelClick(
    float x,
    float y,
    const SDL_FRect& panelRect,
    int itemCount,
    int* scrollOffset,
    bool* dragActive,
    float* dragGrabOffsetY,
    int* outClickedIndex)
{
    if (outClickedIndex != nullptr)
    {
        *outClickedIndex = -1;
    }
    if (!this->pointInRect(x, y, panelRect))
    {
        return false;
    }

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        const int maxOffset = (std::max)(itemCount - kVisibleListRows, 0);
        if (maxOffset <= 0)
        {
            if (dragActive != nullptr)
            {
                *dragActive = false;
            }
            return true;
        }

        float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kVisibleListRows)) / static_cast<float>(itemCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(this->computeListStartIndex(*scrollOffset, itemCount)) / static_cast<float>(maxOffset);
        const float thumbY = scrollTrackRect.y + (ratio * thumbTravel);

        SDL_FRect scrollThumbRect{};
        scrollThumbRect.x = scrollTrackRect.x + 1.0f;
        scrollThumbRect.y = thumbY;
        scrollThumbRect.w = scrollTrackRect.w - 2.0f;
        scrollThumbRect.h = thumbHeight;

        if (this->pointInRect(x, y, scrollThumbRect))
        {
            if (dragActive != nullptr)
            {
                *dragActive = true;
            }
            if (dragGrabOffsetY != nullptr)
            {
                *dragGrabOffsetY = y - scrollThumbRect.y;
            }
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
            if (scrollOffset != nullptr)
            {
                *scrollOffset = static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
                this->clampListScrollOffset(scrollOffset, itemCount);
            }
            if (dragActive != nullptr)
            {
                *dragActive = true;
            }
            if (dragGrabOffsetY != nullptr)
            {
                *dragGrabOffsetY = scrollThumbRect.h * 0.5f;
            }
        }
        return true;
    }

    if (dragActive != nullptr)
    {
        *dragActive = false;
    }
    if (itemCount <= 0)
    {
        return true;
    }

    const int startIndex = this->computeListStartIndex(*scrollOffset, itemCount);
    for (int i = 0; i < kVisibleListRows; ++i)
    {
        const int rowIndex = startIndex + i;
        if (rowIndex >= itemCount)
        {
            break;
        }

        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;
        if (this->pointInRect(x, y, rowRect))
        {
            if (outClickedIndex != nullptr)
            {
                *outClickedIndex = rowIndex;
            }
            return true;
        }
    }

    return true;
}

void EditorMapVfxScene::handleListPanelScrollDragFromMouse(
    const SDL_FRect& panelRect,
    int itemCount,
    int* scrollOffset,
    bool* dragActive,
    float* dragGrabOffsetY)
{
    if (dragActive == nullptr || !(*dragActive))
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        *dragActive = false;
        if (dragGrabOffsetY != nullptr)
        {
            *dragGrabOffsetY = 0.0f;
        }
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
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    const int maxOffset = (std::max)(itemCount - kVisibleListRows, 0);
    if (maxOffset <= 0)
    {
        *dragActive = false;
        if (dragGrabOffsetY != nullptr)
        {
            *dragGrabOffsetY = 0.0f;
        }
        return;
    }

    const float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kVisibleListRows)) / static_cast<float>(itemCount));
    const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
    const float targetThumbY = std::clamp(
        mouseY - ((dragGrabOffsetY != nullptr) ? *dragGrabOffsetY : 0.0f),
        scrollTrackRect.y,
        scrollTrackRect.y + thumbTravel);
    const float ratio = (thumbTravel > 0.0f)
        ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
        : 0.0f;

    if (scrollOffset != nullptr)
    {
        *scrollOffset = static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
        this->clampListScrollOffset(scrollOffset, itemCount);
    }
}

void EditorMapVfxScene::drawListPanel(
    const SDL_FRect& panelRect,
    const char* title,
    const std::vector<std::string>& labels,
    int selectedIndex,
    int scrollOffset) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kPanelFillColor);
    rc2d_graphics_rectangle("fill", &panelRect);
    rc2d_graphics_setColor(kPanelBorderColor);
    rc2d_graphics_rectangle("line", &panelRect);

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text headerText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title);
        headerText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&headerText);
        rc2d_graphics_drawText(&headerText, panelRect.x + panelPadding, panelRect.y + 1.0f);
        rc2d_graphics_destroyText(&headerText);
    }

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;
    rc2d_graphics_setColor(RC2D_Color{58, 68, 79, 220});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{110, 122, 136, 220});
    rc2d_graphics_rectangle("line", &scrollTrackRect);

    if (labels.empty())
    {
        if (this->overlayFont.sdl_font != nullptr)
        {
            RC2D_Text emptyText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Aucun element");
            emptyText.color = kHudStatusColor;
            rc2d_graphics_setTextColor(&emptyText);
            rc2d_graphics_drawText(&emptyText, panelRect.x + panelPadding, rowsTopY + 2.0f);
            rc2d_graphics_destroyText(&emptyText);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int itemCount = static_cast<int>(labels.size());
    const int startIndex = this->computeListStartIndex(scrollOffset, itemCount);
    const int maxOffset = (std::max)(itemCount - kVisibleListRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kVisibleListRows)) / static_cast<float>(itemCount));
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

    for (int i = 0; i < kVisibleListRows; ++i)
    {
        const int itemIndex = startIndex + i;
        if (itemIndex >= itemCount)
        {
            break;
        }

        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        const bool isSelected = (itemIndex == selectedIndex);
        rc2d_graphics_setColor(isSelected ? kRowSelectedFillColor : kRowFillColor);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kRowBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);

        if (this->overlayFont.sdl_font != nullptr)
        {
            char textBuffer[256] = {};
            SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", itemIndex + 1, makeAssetLabel(labels[static_cast<size_t>(itemIndex)], 24).c_str());
            RC2D_Text rowText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), textBuffer);
            rowText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&rowText);
            rc2d_graphics_drawText(&rowText, rowRect.x + 4.0f, rowRect.y + 1.0f);
            rc2d_graphics_destroyText(&rowText);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapVfxScene::openImportShipFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kFolderFilters;
    options.num_filters = static_cast<int>(std::size(kFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier racine des navires";
    options.accept_label = "Ouvrir";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapVfxScene::onImportShipFolderDialogResult, this, &options);
}

void EditorMapVfxScene::openImportSfxFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kFolderFilters;
    options.num_filters = static_cast<int>(std::size(kFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier racine des VFX atlas";
    options.accept_label = "Ouvrir";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapVfxScene::onImportSfxFolderDialogResult, this, &options);
}

void EditorMapVfxScene::openImportLooseFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kFolderFilters;
    options.num_filters = static_cast<int>(std::size(kFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier racine des sprites VFX";
    options.accept_label = "Ouvrir";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapVfxScene::onImportLooseFolderDialogResult, this, &options);
}

void EditorMapVfxScene::openExportFolderDialog(void)
{
    this->pendingExportMode = this->editorMode;

    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kFolderFilters;
    options.num_filters = static_cast<int>(std::size(kFolderFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Choisir le dossier d'export";
    options.accept_label = "Exporter";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapVfxScene::onExportFolderDialogResult, this, &options);
}

void EditorMapVfxScene::openLooseExportNamePopup(void)
{
    if (this->editorMode != EditorMode::LOOSE_SPRITES)
    {
        this->openExportFolderDialog();
        return;
    }

    if (!this->pendingLooseExportAnimationName.empty())
    {
        this->looseExportNameInput = this->pendingLooseExportAnimationName;
    }
    else
    {
        this->looseExportNameInput.clear();
    }

    this->looseExportNamePopupVisible = true;
    this->vfxFpsInputFocused = false;
    this->loosePreviewFpsInputFocused = false;
    this->statusMessage = "Nom animation export: tape un nom puis ENTREE.";
}

void EditorMapVfxScene::processPendingShipFolderRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string folder;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        hasResult = this->pendingShipFolderDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingShipFolderDialogCanceled;
            folder.swap(this->pendingShipFolderAbsolute);
            this->pendingShipFolderDialogCompleted = false;
            this->pendingShipFolderDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || folder.empty())
    {
        this->statusMessage = "Import navires annule.";
        return;
    }

    this->importShipsFromRootFolderAbsolutePath(folder.c_str());
}

void EditorMapVfxScene::processPendingSfxFolderRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string folder;
    {
        std::lock_guard<std::mutex> lock(this->pendingSfxFolderMutex);
        hasResult = this->pendingSfxFolderDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingSfxFolderDialogCanceled;
            folder.swap(this->pendingSfxFolderAbsolute);
            this->pendingSfxFolderDialogCompleted = false;
            this->pendingSfxFolderDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || folder.empty())
    {
        this->statusMessage = "Import VFX annule.";
        return;
    }

    this->importSfxFromRootFolderAbsolutePath(folder.c_str());
}

void EditorMapVfxScene::processPendingLooseFolderRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string folder;
    {
        std::lock_guard<std::mutex> lock(this->pendingLooseFolderMutex);
        hasResult = this->pendingLooseFolderDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingLooseFolderDialogCanceled;
            folder.swap(this->pendingLooseFolderAbsolute);
            this->pendingLooseFolderDialogCompleted = false;
            this->pendingLooseFolderDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || folder.empty())
    {
        this->statusMessage = "Import sprites annule.";
        return;
    }

    this->importLooseFoldersFromRootFolderAbsolutePath(folder.c_str());
}

void EditorMapVfxScene::processPendingExportFolderRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string folder;
    EditorMode exportMode = EditorMode::SHIP_VFX;
    std::string animationName;
    {
        std::lock_guard<std::mutex> lock(this->pendingExportFolderMutex);
        hasResult = this->pendingExportFolderDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingExportFolderDialogCanceled;
            folder.swap(this->pendingExportFolderAbsolute);
            exportMode = this->pendingExportMode;
            this->pendingExportFolderDialogCompleted = false;
            this->pendingExportFolderDialogCanceled = false;
        }
        animationName = this->pendingLooseExportAnimationName;
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || folder.empty())
    {
        this->statusMessage = "Export annule.";
        return;
    }

    if (exportMode == EditorMode::SHIP_VFX)
    {
        this->exportShipVfxJsonToFolder(folder.c_str());
    }
    else
    {
        if (makeExportAnimationSlug(animationName).empty())
        {
            this->statusMessage = "Nom animation manquant: export sprites annule.";
            return;
        }
        this->exportLooseFolderScaledToFolder(folder.c_str(), animationName);
    }
}

bool EditorMapVfxScene::importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath)
{
    if (rootFolderAbsolutePath == nullptr || rootFolderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier navires invalide.";
        return false;
    }

    std::filesystem::path rootPath(rootFolderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(rootPath, fsError) || !std::filesystem::is_directory(rootPath, fsError))
    {
        this->statusMessage = "Dossier navires introuvable.";
        return false;
    }

    auto isShipAtlasFolder = [](const std::filesystem::path& folderPath) -> bool {
        std::error_code localError;
        if (!std::filesystem::exists(folderPath, localError) || !std::filesystem::is_directory(folderPath, localError))
        {
            return false;
        }
        for (int i = 1; i <= kShipSpriteCount; ++i)
        {
            const std::filesystem::path pngPath = folderPath / (std::to_string(i) + ".png");
            if (!std::filesystem::exists(pngPath, localError) || !std::filesystem::is_regular_file(pngPath, localError))
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
                if (isShipAtlasFolder(it->path()))
                {
                    discoveredFolders.push_back(it->path());
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

    std::sort(discoveredFolders.begin(), discoveredFolders.end(), [](const auto& a, const auto& b) {
        return normalizePathSlashes(a.string()) < normalizePathSlashes(b.string());
    });

    auto makePathKey = [](const std::string& path) {
        std::string key = normalizePathSlashes(path);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
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
        const std::string key = makePathKey(normalizedAbsolutePath);
        if (std::find(knownPathKeys.begin(), knownPathKeys.end(), key) != knownPathKeys.end())
        {
            continue;
        }

        knownPathKeys.push_back(key);

        ImportedShip importedShip{};
        importedShip.folderAbsolutePath = normalizedAbsolutePath;
        importedShip.displayName = folderPath.filename().string();
        if (importedShip.displayName.empty())
        {
            importedShip.displayName = importedShip.folderAbsolutePath;
        }

        this->importedShips.push_back(importedShip);
        if (firstAddedIndex < 0)
        {
            firstAddedIndex = static_cast<int>(this->importedShips.size()) - 1;
        }
        addedCount += 1;
    }

    if (addedCount <= 0)
    {
        this->statusMessage = "Aucun nouveau navire importe (deja presents).";
        return false;
    }

    if ((this->selectedShipIndex < 0 || !this->previewShipLoaded) && firstAddedIndex >= 0)
    {
        if (!this->selectImportedShipAtIndex(firstAddedIndex))
        {
            return false;
        }
    }

    this->statusMessage = std::to_string(addedCount) + " navire(s) importe(s).";
    return true;
}

bool EditorMapVfxScene::loadShipFolderFromAbsolutePath(const char* folderAbsolutePath)
{
    if (folderAbsolutePath == nullptr || folderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier navire invalide.";
        return false;
    }

    std::filesystem::path folderPath(folderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(folderPath, fsError) || !std::filesystem::is_directory(folderPath, fsError))
    {
        this->statusMessage = "Dossier navire introuvable.";
        return false;
    }

    std::array<std::filesystem::path, kShipSpriteCount> sourcePngPaths{};
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        const std::filesystem::path pngPath = folderPath / (std::to_string(i + 1) + ".png");
        if (!std::filesystem::exists(pngPath, fsError) || !std::filesystem::is_regular_file(pngPath, fsError))
        {
            this->statusMessage = "Dossier invalide: fichier manquant '" + std::to_string(i + 1) + ".png'.";
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
        SDL_snprintf(userStoragePath, sizeof(userStoragePath), "editor-vfx-ship/current/%d.png", i + 1);
        if (!rc2d_storage_userWriteFile(userStoragePath, bytes.data(), static_cast<Uint64>(bytes.size())))
        {
            this->statusMessage = "Echec copie user storage: " + std::string(userStoragePath);
            return false;
        }
    }

    std::vector<char> anchorBytes;
    const std::filesystem::path anchorSourcePath = folderPath / "ship_anchor.json";
    if (std::filesystem::exists(anchorSourcePath, fsError) && std::filesystem::is_regular_file(anchorSourcePath, fsError))
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
    if (!rc2d_storage_userWriteFile("editor-vfx-ship/current/ship_anchor.json", anchorBytes.data(), static_cast<Uint64>(anchorBytes.size())))
    {
        this->statusMessage = "Echec copie ship_anchor.json dans user storage.";
        return false;
    }

    this->previewShip.unloadSprites();
    if (!this->previewShip.loadSpritesFromFolder("editor-vfx-ship/current", RC2D_STORAGE_USER))
    {
        this->statusMessage = "Echec chargement navire preview (sprites 1..8).";
        return false;
    }

    this->previewShip.setSpeedTilesPerSecond(4.0f);
    this->previewShip.setHealthVisual(Ship::HealthVisual::FULL);
    this->previewShip.setDrawAlpha(255);
    this->previewShip.setPositionTile(this->previewShipTile.x, this->previewShipTile.y);
    this->previewShipLoaded = true;
    this->loadedShipFolderAbsolute = normalizePathSlashes(folderPath.string());
    this->statusMessage = "Navire preview charge.";
    return true;
}

bool EditorMapVfxScene::selectImportedShipAtIndex(int shipIndex)
{
    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Selection navire invalide.";
        return false;
    }

    this->selectedShipIndex = shipIndex;
    this->ensureSelectionVisible(this->selectedShipIndex, &this->shipListScrollOffset, static_cast<int>(this->importedShips.size()));

    const ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    if (!this->loadShipFolderFromAbsolutePath(selectedShip.folderAbsolutePath.c_str()))
    {
        return false;
    }

    this->statusMessage = "Navire actif: " + selectedShip.displayName;
    return true;
}

bool EditorMapVfxScene::importSfxFromAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier VFX invalide.";
        return false;
    }

    std::filesystem::path sfxFolder(absolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(sfxFolder, fsError) || !std::filesystem::is_directory(sfxFolder, fsError))
    {
        this->statusMessage = "Dossier VFX introuvable.";
        return false;
    }

    const std::filesystem::path jsonSourcePath = sfxFolder / "texturepacker.json";
    if (!std::filesystem::exists(jsonSourcePath, fsError) || !std::filesystem::is_regular_file(jsonSourcePath, fsError))
    {
        this->statusMessage = "VFX ignore: texturepacker.json manquant.";
        return false;
    }

    std::ifstream jsonInput(jsonSourcePath, std::ios::binary | std::ios::ate);
    if (!jsonInput.is_open())
    {
        this->statusMessage = "Lecture texturepacker.json impossible.";
        return false;
    }
    const std::streamsize jsonSize = jsonInput.tellg();
    if (jsonSize <= 0)
    {
        this->statusMessage = "texturepacker.json vide.";
        return false;
    }
    jsonInput.seekg(0, std::ios::beg);
    std::vector<char> jsonBytes(static_cast<size_t>(jsonSize) + 1U, '\0');
    if (!jsonInput.read(jsonBytes.data(), jsonSize))
    {
        this->statusMessage = "Lecture texturepacker.json echouee.";
        return false;
    }

    cJSON* root = cJSON_Parse(jsonBytes.data());
    if (root == nullptr)
    {
        this->statusMessage = "texturepacker.json invalide.";
        return false;
    }

    cJSON* meta = cJSON_GetObjectItemCaseSensitive(root, "meta");
    cJSON* image = (meta != nullptr) ? cJSON_GetObjectItemCaseSensitive(meta, "image") : nullptr;
    if (!cJSON_IsObject(meta) || !cJSON_IsString(image) || image->valuestring == nullptr)
    {
        cJSON_Delete(root);
        this->statusMessage = "VFX invalide: meta.image manquant.";
        return false;
    }

    const std::string imageRelativePath = image->valuestring;
    const std::filesystem::path imageSourcePath = sfxFolder / std::filesystem::path(imageRelativePath);
    if (!std::filesystem::exists(imageSourcePath, fsError) || !std::filesystem::is_regular_file(imageSourcePath, fsError))
    {
        cJSON_Delete(root);
        this->statusMessage = "VFX invalide: image atlas introuvable.";
        return false;
    }

    const std::string imageFileName = extractFileName(imageRelativePath);
    if (imageFileName.empty())
    {
        cJSON_Delete(root);
        this->statusMessage = "VFX invalide: nom image atlas vide.";
        return false;
    }

    cJSON_ReplaceItemInObjectCaseSensitive(meta, "image", cJSON_CreateString(imageFileName.c_str()));
    char* patchedJson = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (patchedJson == nullptr)
    {
        this->statusMessage = "VFX invalide: serialisation JSON impossible.";
        return false;
    }

    std::ifstream imageInput(imageSourcePath, std::ios::binary | std::ios::ate);
    if (!imageInput.is_open())
    {
        cJSON_free(patchedJson);
        this->statusMessage = "Lecture image atlas impossible.";
        return false;
    }
    const std::streamsize imageSize = imageInput.tellg();
    if (imageSize <= 0)
    {
        cJSON_free(patchedJson);
        this->statusMessage = "Image atlas vide.";
        return false;
    }
    imageInput.seekg(0, std::ios::beg);
    std::vector<char> imageBytes(static_cast<size_t>(imageSize));
    if (!imageInput.read(imageBytes.data(), imageSize))
    {
        cJSON_free(patchedJson);
        this->statusMessage = "Lecture image atlas echouee.";
        return false;
    }

    this->ensureUserStorageFolders();
    ++this->importedSfxCounter;
    char storageFolderPath[256] = {};
    SDL_snprintf(storageFolderPath, sizeof(storageFolderPath), "editor-vfx-sfx/imported-%04u", this->importedSfxCounter);
    rc2d_storage_userMkdir(storageFolderPath);

    const std::string storageJsonPath = std::string(storageFolderPath) + "/texturepacker.json";
    const std::string storageImagePath = std::string(storageFolderPath) + "/" + imageFileName;
    const bool wroteJson = rc2d_storage_userWriteFile(storageJsonPath.c_str(), patchedJson, static_cast<Uint64>(std::strlen(patchedJson)));
    cJSON_free(patchedJson);
    if (!wroteJson)
    {
        this->statusMessage = "Echec copie texturepacker.json en storage user.";
        return false;
    }
    if (!rc2d_storage_userWriteFile(storageImagePath.c_str(), imageBytes.data(), static_cast<Uint64>(imageBytes.size())))
    {
        this->statusMessage = "Echec copie image atlas en storage user.";
        return false;
    }

    RC2D_TP_Atlas atlas = rc2d_tp_loadAtlasFromStorage(storageJsonPath.c_str(), RC2D_STORAGE_USER);
    if (atlas.atlas_image.sdl_texture == nullptr || atlas.frame_count <= 0)
    {
        rc2d_tp_freeAtlas(&atlas);
        this->statusMessage = "Echec chargement atlas VFX.";
        return false;
    }

    std::vector<std::string> frameNames;
    frameNames.reserve(static_cast<size_t>(atlas.frame_count));
    for (int i = 0; i < atlas.frame_count; ++i)
    {
        if (atlas.frames[i].filename == nullptr || atlas.frames[i].filename[0] == '\0')
        {
            continue;
        }
        frameNames.emplace_back(atlas.frames[i].filename);
    }
    sortFrameNamesByNumericOrder(&frameNames);
    if (frameNames.empty())
    {
        rc2d_tp_freeAtlas(&atlas);
        this->statusMessage = "Atlas VFX vide (aucune frame exploitable).";
        return false;
    }

    ImportedSfx imported{};
    imported.id = "sfx_" + std::to_string(this->importedSfxCounter);
    imported.displayName = sfxFolder.filename().string();
    if (imported.displayName.empty())
    {
        imported.displayName = jsonSourcePath.stem().string();
    }
    imported.sourceJsonPath = normalizePathSlashes(jsonSourcePath.string());
    imported.storageJsonPath = storageJsonPath;
    imported.atlas = atlas;
    imported.frameNames = std::move(frameNames);
    imported.defaultFps = 12.0f;

    this->importedSfx.push_back(std::move(imported));
    this->selectedSfxIndex = static_cast<int>(this->importedSfx.size()) - 1;
    this->ensureSelectionVisible(this->selectedSfxIndex, &this->sfxListScrollOffset, static_cast<int>(this->importedSfx.size()));
    this->statusMessage = "VFX importe: " + this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].displayName;
    return true;
}

bool EditorMapVfxScene::importSfxFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath)
{
    if (rootFolderAbsolutePath == nullptr || rootFolderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier VFX invalide.";
        return false;
    }

    std::filesystem::path rootPath(rootFolderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(rootPath, fsError) || !std::filesystem::is_directory(rootPath, fsError))
    {
        this->statusMessage = "Dossier VFX introuvable.";
        return false;
    }

    auto makePathKey = [](const std::string& path) {
        std::string key = normalizePathSlashes(path);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return key;
    };

    std::vector<std::string> knownPathKeys;
    knownPathKeys.reserve(this->importedSfx.size());
    for (const ImportedSfx& sfx : this->importedSfx)
    {
        knownPathKeys.push_back(makePathKey(sfx.sourceJsonPath));
    }

    auto isValidSfxFolder = [](const std::filesystem::path& folderPath) -> bool {
        std::error_code localError;
        const std::filesystem::path jsonPath = folderPath / "texturepacker.json";
        if (!std::filesystem::exists(jsonPath, localError) || !std::filesystem::is_regular_file(jsonPath, localError))
        {
            return false;
        }

        std::ifstream jsonInput(jsonPath, std::ios::binary | std::ios::ate);
        if (!jsonInput.is_open())
        {
            return false;
        }
        const std::streamsize size = jsonInput.tellg();
        if (size <= 0)
        {
            return false;
        }
        jsonInput.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(size) + 1U, '\0');
        if (!jsonInput.read(bytes.data(), size))
        {
            return false;
        }

        cJSON* root = cJSON_Parse(bytes.data());
        if (root == nullptr)
        {
            return false;
        }

        const cJSON* meta = cJSON_GetObjectItemCaseSensitive(root, "meta");
        const cJSON* image = (meta != nullptr) ? cJSON_GetObjectItemCaseSensitive(meta, "image") : nullptr;
        bool valid = false;
        if (cJSON_IsObject(meta) && cJSON_IsString(image) && image->valuestring != nullptr)
        {
            const std::filesystem::path imagePath = folderPath / std::filesystem::path(image->valuestring);
            valid = std::filesystem::exists(imagePath, localError) && std::filesystem::is_regular_file(imagePath, localError);
        }

        cJSON_Delete(root);
        return valid;
    };

    std::vector<std::filesystem::path> discoveredFolders;
    if (isValidSfxFolder(rootPath))
    {
        discoveredFolders.push_back(rootPath);
    }

    std::filesystem::recursive_directory_iterator it(
        rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        fsError);
    std::filesystem::recursive_directory_iterator end;
    while (!fsError && it != end)
    {
        if (it->is_directory(fsError) && !fsError)
        {
            if (isValidSfxFolder(it->path()))
            {
                discoveredFolders.push_back(it->path());
            }
        }
        it.increment(fsError);
    }

    std::sort(discoveredFolders.begin(), discoveredFolders.end(), [](const auto& a, const auto& b) {
        return normalizePathSlashes(a.string()) < normalizePathSlashes(b.string());
    });

    int addedCount = 0;
    int failedCount = 0;
    for (const std::filesystem::path& folderPath : discoveredFolders)
    {
        const std::string jsonKey = makePathKey((folderPath / "texturepacker.json").string());
        if (std::find(knownPathKeys.begin(), knownPathKeys.end(), jsonKey) != knownPathKeys.end())
        {
            continue;
        }

        if (this->importSfxFromAbsolutePath(folderPath.string().c_str()))
        {
            knownPathKeys.push_back(jsonKey);
            addedCount += 1;
        }
        else
        {
            failedCount += 1;
        }
    }

    if (addedCount > 0 && failedCount == 0)
    {
        this->statusMessage = std::to_string(addedCount) + " VFX importe(s).";
        return true;
    }
    if (addedCount > 0 && failedCount > 0)
    {
        this->statusMessage = std::to_string(addedCount) + " VFX importe(s), " + std::to_string(failedCount) + " en echec.";
        return true;
    }

    this->statusMessage = "Aucun dossier VFX valide trouve (texturepacker.json + image atlas).";
    return false;
}

bool EditorMapVfxScene::importLooseFolderFromAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier sprites invalide.";
        return false;
    }

    std::filesystem::path folderPath(absolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(folderPath, fsError) || !std::filesystem::is_directory(folderPath, fsError))
    {
        this->statusMessage = "Dossier sprites introuvable.";
        return false;
    }

    if (std::filesystem::exists(folderPath / "texturepacker.json", fsError))
    {
        this->statusMessage = "Dossier ignore: texturepacker.json present.";
        return false;
    }

    std::vector<std::filesystem::path> spritePaths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folderPath, fsError))
    {
        if (fsError)
        {
            fsError.clear();
            continue;
        }
        if (entry.is_regular_file(fsError) && !fsError && isPngFilePath(entry.path()))
        {
            spritePaths.push_back(entry.path());
        }
    }

    if (spritePaths.empty())
    {
        this->statusMessage = "Aucun sprite PNG trouve dans le dossier.";
        return false;
    }

    std::sort(spritePaths.begin(), spritePaths.end(), [](const auto& a, const auto& b) {
        const std::string aName = a.filename().string();
        const std::string bName = b.filename().string();

        unsigned long long aNumericValue = 0;
        unsigned long long bNumericValue = 0;
        const bool aIsNumeric = tryParseNumericFrameName(aName, &aNumericValue);
        const bool bIsNumeric = tryParseNumericFrameName(bName, &bNumericValue);

        if (aIsNumeric && bIsNumeric)
        {
            if (aNumericValue != bNumericValue)
            {
                return aNumericValue < bNumericValue;
            }
            return aName < bName;
        }
        if (aIsNumeric != bIsNumeric)
        {
            return aIsNumeric;
        }
        return aName < bName;
    });

    this->ensureUserStorageFolders();
    ++this->importedLooseFolderCounter;
    char storageFolderPath[256] = {};
    SDL_snprintf(storageFolderPath, sizeof(storageFolderPath), "editor-vfx-loose/imported-%04u", this->importedLooseFolderCounter);
    rc2d_storage_userMkdir(storageFolderPath);

    ImportedLooseFolder folder{};
    folder.displayName = folderPath.filename().string();
    if (folder.displayName.empty())
    {
        folder.displayName = normalizePathSlashes(folderPath.string());
    }
    folder.folderAbsolutePath = normalizePathSlashes(folderPath.string());
    folder.storageFolderPath = storageFolderPath;

    for (const std::filesystem::path& spritePath : spritePaths)
    {
        std::ifstream input(spritePath, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            continue;
        }

        const std::streamsize size = input.tellg();
        if (size <= 0)
        {
            continue;
        }
        input.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(size));
        if (!input.read(bytes.data(), size))
        {
            continue;
        }

        const std::string fileName = spritePath.filename().string();
        const std::string storagePath = folder.storageFolderPath + "/" + fileName;
        if (!rc2d_storage_userWriteFile(storagePath.c_str(), bytes.data(), static_cast<Uint64>(bytes.size())))
        {
            continue;
        }

        RC2D_Image image = rc2d_graphics_loadImageFromStorage(storagePath.c_str(), RC2D_STORAGE_USER);
        if (image.sdl_texture == nullptr)
        {
            continue;
        }
        SDL_SetTextureScaleMode(image.sdl_texture, SDL_SCALEMODE_LINEAR);

        float widthPx = 0.0f;
        float heightPx = 0.0f;
        SDL_GetTextureSize(image.sdl_texture, &widthPx, &heightPx);

        ImportedLooseSprite sprite{};
        sprite.fileName = fileName;
        sprite.storagePath = storagePath;
        sprite.image = image;
        sprite.widthPx = widthPx;
        sprite.heightPx = heightPx;
        folder.sprites.push_back(std::move(sprite));
    }

    if (folder.sprites.empty())
    {
        this->statusMessage = "Import sprites echoue: aucune image chargeable.";
        return false;
    }

    this->importedLooseFolders.push_back(std::move(folder));
    this->selectedLooseFolderIndex = static_cast<int>(this->importedLooseFolders.size()) - 1;
    this->ensureSelectionVisible(this->selectedLooseFolderIndex, &this->looseListScrollOffset, static_cast<int>(this->importedLooseFolders.size()));
    this->loosePreviewPlacements.clear();
    this->nextLoosePreviewPlacementId = 1U;
    this->statusMessage = "Dossier sprites importe: " + this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)].displayName;
    return true;
}

bool EditorMapVfxScene::importLooseFoldersFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath)
{
    if (rootFolderAbsolutePath == nullptr || rootFolderAbsolutePath[0] == '\0')
    {
        this->statusMessage = "Dossier sprites invalide.";
        return false;
    }

    std::filesystem::path rootPath(rootFolderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(rootPath, fsError) || !std::filesystem::is_directory(rootPath, fsError))
    {
        this->statusMessage = "Dossier sprites introuvable.";
        return false;
    }

    auto makePathKey = [](const std::string& path) {
        std::string key = normalizePathSlashes(path);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return key;
    };

    std::vector<std::string> knownPathKeys;
    knownPathKeys.reserve(this->importedLooseFolders.size());
    for (const ImportedLooseFolder& folder : this->importedLooseFolders)
    {
        knownPathKeys.push_back(makePathKey(folder.folderAbsolutePath));
    }

    auto isValidLooseFolder = [](const std::filesystem::path& folderPath) -> bool {
        std::error_code localError;
        if (!std::filesystem::exists(folderPath, localError) || !std::filesystem::is_directory(folderPath, localError))
        {
            return false;
        }
        if (std::filesystem::exists(folderPath / "texturepacker.json", localError))
        {
            return false;
        }
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folderPath, localError))
        {
            if (localError)
            {
                localError.clear();
                continue;
            }
            if (entry.is_regular_file(localError) && !localError && isPngFilePath(entry.path()))
            {
                return true;
            }
        }
        return false;
    };

    std::vector<std::filesystem::path> discoveredFolders;
    if (isValidLooseFolder(rootPath))
    {
        discoveredFolders.push_back(rootPath);
    }

    std::filesystem::recursive_directory_iterator it(
        rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        fsError);
    std::filesystem::recursive_directory_iterator end;
    while (!fsError && it != end)
    {
        if (it->is_directory(fsError) && !fsError)
        {
            if (isValidLooseFolder(it->path()))
            {
                discoveredFolders.push_back(it->path());
            }
        }
        it.increment(fsError);
    }

    std::sort(discoveredFolders.begin(), discoveredFolders.end(), [](const auto& a, const auto& b) {
        return normalizePathSlashes(a.string()) < normalizePathSlashes(b.string());
    });

    int addedCount = 0;
    int failedCount = 0;
    for (const std::filesystem::path& folderPath : discoveredFolders)
    {
        const std::string key = makePathKey(folderPath.string());
        if (std::find(knownPathKeys.begin(), knownPathKeys.end(), key) != knownPathKeys.end())
        {
            continue;
        }

        if (this->importLooseFolderFromAbsolutePath(folderPath.string().c_str()))
        {
            knownPathKeys.push_back(key);
            addedCount += 1;
        }
        else
        {
            failedCount += 1;
        }
    }

    if (addedCount > 0 && failedCount == 0)
    {
        this->statusMessage = std::to_string(addedCount) + " dossier(s) sprites importe(s).";
        return true;
    }
    if (addedCount > 0 && failedCount > 0)
    {
        this->statusMessage = std::to_string(addedCount) + " dossier(s) sprites importe(s), " + std::to_string(failedCount) + " en echec.";
        return true;
    }

    this->statusMessage = "Aucun dossier sprites PNG valide trouve.";
    return false;
}

void EditorMapVfxScene::spawnSelectedSfxAtShipCenter(void)
{
    if (!this->previewShipLoaded)
    {
        this->statusMessage = "Charge d'abord un navire.";
        return;
    }
    if (this->selectedSfxIndex < 0 || this->selectedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        this->statusMessage = "Selection VFX invalide.";
        return;
    }

    const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)];
    ShipVfxInstance instance{};
    instance.instanceId = this->nextVfxInstanceId++;
    instance.importedSfxIndex = this->selectedSfxIndex;
    instance.offsetX = 0.0f;
    instance.offsetY = 0.0f;
    instance.rotationDeg = 0.0f;
    instance.flipHorizontal = false;
    instance.flipVertical = false;
    instance.drawOrder = static_cast<int>(this->shipVfxInstances.size()) + 1;
    instance.fps = imported.defaultFps;
    instance.followShip = true;
    this->shipVfxInstances.push_back(instance);
    this->setSelectedVfxInstanceIndex(static_cast<int>(this->shipVfxInstances.size()) - 1);
    this->statusMessage = "VFX ajoute au centre: " + imported.displayName;
}

void EditorMapVfxScene::setSelectedVfxInstanceIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->selectedVfxInstanceIndex = -1;
        this->vfxFpsInput = "12";
        return;
    }

    this->selectedVfxInstanceIndex = index;
    this->vfxFpsInput = formatSfxFpsValue(this->shipVfxInstances[static_cast<size_t>(index)].fps);
}

void EditorMapVfxScene::removeSelectedVfxInstance(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances.erase(this->shipVfxInstances.begin() + this->selectedVfxInstanceIndex);
    if (this->shipVfxInstances.empty())
    {
        this->setSelectedVfxInstanceIndex(-1);
    }
    else
    {
        const int nextIndex = std::clamp(this->selectedVfxInstanceIndex, 0, static_cast<int>(this->shipVfxInstances.size()) - 1);
        this->setSelectedVfxInstanceIndex(nextIndex);
    }
    this->statusMessage = "VFX retire.";
}

void EditorMapVfxScene::centerSelectedVfxInstance(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
    instance.offsetX = 0.0f;
    instance.offsetY = 0.0f;
    this->statusMessage = "VFX recentre.";
}

void EditorMapVfxScene::moveSelectedVfxInstance(float deltaX, float deltaY)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    // Les offsets sont stockes en "px base 1.00" pour rester stables entre les niveaux de zoom.
    instance.offsetX += (deltaX / zoom);
    instance.offsetY += (deltaY / zoom);
}

void EditorMapVfxScene::adjustSelectedVfxRotation(float deltaDegrees)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
    instance.rotationDeg += deltaDegrees;
    while (instance.rotationDeg < 0.0f)
    {
        instance.rotationDeg += 360.0f;
    }
    while (instance.rotationDeg >= 360.0f)
    {
        instance.rotationDeg -= 360.0f;
    }
}

void EditorMapVfxScene::toggleSelectedVfxFlipHorizontal(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].flipHorizontal =
        !this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].flipHorizontal;
}

void EditorMapVfxScene::toggleSelectedVfxFlipVertical(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].flipVertical =
        !this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].flipVertical;
}

void EditorMapVfxScene::toggleSelectedVfxFollowShip(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].followShip =
        !this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].followShip;
}

void EditorMapVfxScene::adjustSelectedVfxDrawOrder(int delta)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].drawOrder += delta;
}

void EditorMapVfxScene::adjustShipDrawOrder(int delta)
{
    this->shipDrawOrder += delta;
}

void EditorMapVfxScene::normalizeShipVfxDrawOrders(void)
{
}

bool EditorMapVfxScene::applyVfxFpsInput(void)
{
    float fallbackFps = kLoosePreviewFpsDefault;
    if (this->selectedVfxInstanceIndex >= 0 &&
        this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
    {
        fallbackFps = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].fps;
    }
    else if (this->selectedSfxIndex >= 0 &&
             this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        fallbackFps = this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].defaultFps;
    }

    const std::string trimmed = trimAscii(this->vfxFpsInput);
    if (trimmed.empty())
    {
        this->vfxFpsInput = formatSfxFpsValue(fallbackFps);
        this->statusMessage = "FPS VFX vide: valeur precedente conservee.";
        return false;
    }

    char* endPtr = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &endPtr);
    if (endPtr == trimmed.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
    {
        this->vfxFpsInput = formatSfxFpsValue(fallbackFps);
        this->statusMessage = "FPS VFX invalide.";
        return false;
    }

    const float clamped = std::clamp(parsed, kSfxFpsMin, kSfxFpsMax);
    this->vfxFpsInput = formatSfxFpsValue(clamped);

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
    {
        this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].fps = clamped;
        this->statusMessage = "FPS VFX applique sur l'instance.";
        return true;
    }

    if (this->selectedSfxIndex >= 0 && this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].defaultFps = clamped;
        this->statusMessage = "FPS VFX par defaut mis a jour.";
        return true;
    }

    this->statusMessage = "Aucun VFX cible pour appliquer les FPS.";
    return false;
}

float EditorMapVfxScene::getLoosePreviewFpsOrDefault(void) const
{
    const std::string trimmed = trimAscii(this->loosePreviewFpsInput);
    if (trimmed.empty())
    {
        return kLoosePreviewFpsDefault;
    }

    char* endPtr = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &endPtr);
    if (endPtr == trimmed.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
    {
        return kLoosePreviewFpsDefault;
    }

    return std::clamp(parsed, kSfxFpsMin, kSfxFpsMax);
}

bool EditorMapVfxScene::applyLoosePreviewFpsInput(void)
{
    const float previousFps = this->getLoosePreviewFpsOrDefault();
    const std::string trimmed = trimAscii(this->loosePreviewFpsInput);
    if (trimmed.empty())
    {
        this->loosePreviewFpsInput = formatSfxFpsValue(previousFps);
        this->statusMessage = "FPS preview vide: valeur precedente conservee.";
        return false;
    }

    char* endPtr = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &endPtr);
    if (endPtr == trimmed.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
    {
        this->loosePreviewFpsInput = formatSfxFpsValue(previousFps);
        this->statusMessage = "FPS preview invalide.";
        return false;
    }

    const float clamped = std::clamp(parsed, kSfxFpsMin, kSfxFpsMax);
    this->loosePreviewFpsInput = formatSfxFpsValue(clamped);
    for (LoosePreviewPlacement& placement : this->loosePreviewPlacements)
    {
        placement.fps = clamped;
    }
    this->statusMessage =
        "FPS preview applique (" +
        std::to_string(static_cast<int>(this->loosePreviewPlacements.size())) +
        " instances mises a jour).";
    return true;
}

bool EditorMapVfxScene::handleVfxFpsInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->vfxFpsInputFocused)
    {
        return false;
    }

    (void)mod;
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->vfxFpsInputFocused = false;
        float fallbackFps = kLoosePreviewFpsDefault;
        if (this->selectedVfxInstanceIndex >= 0 &&
            this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
        {
            fallbackFps = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].fps;
        }
        else if (this->selectedSfxIndex >= 0 &&
                 this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            fallbackFps = this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].defaultFps;
        }
        this->vfxFpsInput = formatSfxFpsValue(fallbackFps);
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->applyVfxFpsInput();
        this->vfxFpsInputFocused = false;
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->vfxFpsInput.empty() && !isrepeat)
    {
        this->vfxFpsInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->vfxFpsInput.empty() && !isrepeat)
    {
        this->vfxFpsInput.clear();
        return true;
    }

    auto appendCharIfAllowed = [this](char c) -> bool {
        if (this->vfxFpsInput.size() >= 8U)
        {
            return true;
        }
        if (c == ',')
        {
            c = '.';
        }
        if (c == '.')
        {
            if (this->vfxFpsInput.find('.') != std::string::npos)
            {
                return true;
            }
            if (this->vfxFpsInput.empty())
            {
                this->vfxFpsInput = "0";
            }
            this->vfxFpsInput.push_back('.');
            return true;
        }
        if (c >= '0' && c <= '9')
        {
            this->vfxFpsInput.push_back(c);
        }
        return true;
    };

    auto appendKeypadDigitIfAny = [&appendCharIfAllowed, keycode, scancode]() -> bool {
        switch (keycode)
        {
        case SDLK_KP_0: return appendCharIfAllowed('0');
        case SDLK_KP_1: return appendCharIfAllowed('1');
        case SDLK_KP_2: return appendCharIfAllowed('2');
        case SDLK_KP_3: return appendCharIfAllowed('3');
        case SDLK_KP_4: return appendCharIfAllowed('4');
        case SDLK_KP_5: return appendCharIfAllowed('5');
        case SDLK_KP_6: return appendCharIfAllowed('6');
        case SDLK_KP_7: return appendCharIfAllowed('7');
        case SDLK_KP_8: return appendCharIfAllowed('8');
        case SDLK_KP_9: return appendCharIfAllowed('9');
        default: break;
        }
        switch (scancode)
        {
        case SDL_SCANCODE_KP_0: return appendCharIfAllowed('0');
        case SDL_SCANCODE_KP_1: return appendCharIfAllowed('1');
        case SDL_SCANCODE_KP_2: return appendCharIfAllowed('2');
        case SDL_SCANCODE_KP_3: return appendCharIfAllowed('3');
        case SDL_SCANCODE_KP_4: return appendCharIfAllowed('4');
        case SDL_SCANCODE_KP_5: return appendCharIfAllowed('5');
        case SDL_SCANCODE_KP_6: return appendCharIfAllowed('6');
        case SDL_SCANCODE_KP_7: return appendCharIfAllowed('7');
        case SDL_SCANCODE_KP_8: return appendCharIfAllowed('8');
        case SDL_SCANCODE_KP_9: return appendCharIfAllowed('9');
        default: break;
        }
        return false;
    };

    if (keycode == SDLK_PERIOD || keycode == SDLK_KP_PERIOD ||
        scancode == SDL_SCANCODE_PERIOD || scancode == SDL_SCANCODE_KP_PERIOD)
    {
        return appendCharIfAllowed('.');
    }
    if (appendKeypadDigitIfAny())
    {
        return true;
    }
    if (key == nullptr || key[0] == '\0')
    {
        return true;
    }
    if (std::strlen(key) != 1U)
    {
        return true;
    }
    return appendCharIfAllowed(key[0]);
}

bool EditorMapVfxScene::handleLoosePreviewFpsInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->loosePreviewFpsInputFocused)
    {
        return false;
    }

    (void)mod;
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->loosePreviewFpsInputFocused = false;
        this->loosePreviewFpsInput = formatSfxFpsValue(this->getLoosePreviewFpsOrDefault());
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->applyLoosePreviewFpsInput();
        this->loosePreviewFpsInputFocused = false;
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->loosePreviewFpsInput.empty() && !isrepeat)
    {
        this->loosePreviewFpsInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->loosePreviewFpsInput.empty() && !isrepeat)
    {
        this->loosePreviewFpsInput.clear();
        return true;
    }

    auto appendCharIfAllowed = [this](char c) -> bool {
        if (this->loosePreviewFpsInput.size() >= 8U)
        {
            return true;
        }
        if (c == ',')
        {
            c = '.';
        }
        if (c == '.')
        {
            if (this->loosePreviewFpsInput.find('.') != std::string::npos)
            {
                return true;
            }
            if (this->loosePreviewFpsInput.empty())
            {
                this->loosePreviewFpsInput = "0";
            }
            this->loosePreviewFpsInput.push_back('.');
            return true;
        }
        if (c >= '0' && c <= '9')
        {
            this->loosePreviewFpsInput.push_back(c);
        }
        return true;
    };

    auto appendKeypadDigitIfAny = [&appendCharIfAllowed, keycode, scancode]() -> bool {
        switch (keycode)
        {
        case SDLK_KP_0: return appendCharIfAllowed('0');
        case SDLK_KP_1: return appendCharIfAllowed('1');
        case SDLK_KP_2: return appendCharIfAllowed('2');
        case SDLK_KP_3: return appendCharIfAllowed('3');
        case SDLK_KP_4: return appendCharIfAllowed('4');
        case SDLK_KP_5: return appendCharIfAllowed('5');
        case SDLK_KP_6: return appendCharIfAllowed('6');
        case SDLK_KP_7: return appendCharIfAllowed('7');
        case SDLK_KP_8: return appendCharIfAllowed('8');
        case SDLK_KP_9: return appendCharIfAllowed('9');
        default: break;
        }
        switch (scancode)
        {
        case SDL_SCANCODE_KP_0: return appendCharIfAllowed('0');
        case SDL_SCANCODE_KP_1: return appendCharIfAllowed('1');
        case SDL_SCANCODE_KP_2: return appendCharIfAllowed('2');
        case SDL_SCANCODE_KP_3: return appendCharIfAllowed('3');
        case SDL_SCANCODE_KP_4: return appendCharIfAllowed('4');
        case SDL_SCANCODE_KP_5: return appendCharIfAllowed('5');
        case SDL_SCANCODE_KP_6: return appendCharIfAllowed('6');
        case SDL_SCANCODE_KP_7: return appendCharIfAllowed('7');
        case SDL_SCANCODE_KP_8: return appendCharIfAllowed('8');
        case SDL_SCANCODE_KP_9: return appendCharIfAllowed('9');
        default: break;
        }
        return false;
    };

    if (keycode == SDLK_PERIOD || keycode == SDLK_KP_PERIOD ||
        scancode == SDL_SCANCODE_PERIOD || scancode == SDL_SCANCODE_KP_PERIOD)
    {
        return appendCharIfAllowed('.');
    }
    if (appendKeypadDigitIfAny())
    {
        return true;
    }
    if (key == nullptr || key[0] == '\0')
    {
        return true;
    }
    if (std::strlen(key) != 1U)
    {
        return true;
    }
    return appendCharIfAllowed(key[0]);
}

bool EditorMapVfxScene::handleLooseExportNameInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->looseExportNamePopupVisible)
    {
        return false;
    }

    (void)mod;

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->looseExportNamePopupVisible = false;
        this->statusMessage = "Export sprites annule.";
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        const std::string animationSlug = makeExportAnimationSlug(this->looseExportNameInput);
        if (animationSlug.empty())
        {
            this->statusMessage = "Nom animation invalide. Exemple: coup-critique.";
            return true;
        }
        this->pendingLooseExportAnimationName = animationSlug;
        this->looseExportNamePopupVisible = false;
        this->openExportFolderDialog();
        this->statusMessage = "Nom animation OK: " + animationSlug + ". Choisis le dossier d'export.";
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->looseExportNameInput.empty() && !isrepeat)
    {
        this->looseExportNameInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->looseExportNameInput.empty() && !isrepeat)
    {
        this->looseExportNameInput.clear();
        return true;
    }

    auto appendIfRoom = [this](char c) -> bool {
        if (this->looseExportNameInput.size() >= 64U)
        {
            return true;
        }
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32U && uc <= 126U)
        {
            this->looseExportNameInput.push_back(static_cast<char>(uc));
        }
        return true;
    };

    if (keycode == SDLK_MINUS || keycode == SDLK_KP_MINUS || keycode == SDLK_UNDERSCORE ||
        scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS)
    {
        return appendIfRoom('-');
    }
    if (keycode == SDLK_SPACE || scancode == SDL_SCANCODE_SPACE)
    {
        return appendIfRoom(' ');
    }
    if (key == nullptr || key[0] == '\0')
    {
        return true;
    }
    if (std::strlen(key) != 1U)
    {
        return true;
    }

    return appendIfRoom(key[0]);
}

int EditorMapVfxScene::findTopmostVfxInstanceIndexAtPoint(float x, float y) const
{
    if (!this->previewShipLoaded)
    {
        return -1;
    }

    const Map& map = GetCurrentMap();
    const SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    std::vector<std::pair<int, RenderItem>> candidates;
    candidates.reserve(this->shipVfxInstances.size());
    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        candidates.push_back({static_cast<int>(i), RenderItem{false, instance.drawOrder, instance.instanceId, static_cast<int>(i)}});
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.second.drawOrder != b.second.drawOrder)
        {
            return a.second.drawOrder > b.second.drawOrder;
        }
        return a.second.instanceId > b.second.instanceId;
    });

    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    for (const auto& candidate : candidates)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(candidate.first)];
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }

        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        if (imported.frameNames.empty() || imported.atlas.atlas_image.sdl_texture == nullptr)
        {
            continue;
        }

        const int frameCount = static_cast<int>(imported.frameNames.size());
        const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(instance.fps, 1.0f))) % frameCount;
        const RC2D_TP_Frame* frame = rc2d_tp_getFrame(&imported.atlas, imported.frameNames[static_cast<size_t>(frameIndex)].c_str());
        if (frame == nullptr)
        {
            continue;
        }

        const float sourceW = (frame->sourceSize.x > 0.0f) ? frame->sourceSize.x : frame->frame.w;
        const float sourceH = (frame->sourceSize.y > 0.0f) ? frame->sourceSize.y : frame->frame.h;
        const float offsetX = instance.offsetX * scale;
        const float offsetY = instance.offsetY * scale;
        const SDL_FRect bounds = SDL_FRect{
            shipCenter.x + offsetX - ((sourceW * scale) * 0.5f),
            shipCenter.y + offsetY - ((sourceH * scale) * 0.5f),
            sourceW * scale,
            sourceH * scale};
        if (this->pointInRect(x, y, bounds))
        {
            return candidate.first;
        }
    }

    return -1;
}

bool EditorMapVfxScene::exportShipVfxJsonToFolder(const char* absoluteFolderPath)
{
    if (!this->previewShipLoaded || this->selectedShipIndex < 0 || this->selectedShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Charge d'abord un navire avant export JSON.";
        return false;
    }
    if (absoluteFolderPath == nullptr || absoluteFolderPath[0] == '\0')
    {
        this->statusMessage = "Dossier export invalide.";
        return false;
    }

    std::filesystem::path folderPath(absoluteFolderPath);
    std::error_code fsError;
    std::filesystem::create_directories(folderPath, fsError);
    if (fsError)
    {
        this->statusMessage = "Impossible de creer le dossier export.";
        return false;
    }

    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON.";
        return false;
    }

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    cJSON_AddStringToObject(root, "mode", "ship_vfx");
    cJSON_AddStringToObject(root, "shipDisplayName", ship.displayName.c_str());
    cJSON_AddStringToObject(root, "shipFolderAbsolutePath", ship.folderAbsolutePath.c_str());
    cJSON_AddNumberToObject(root, "shipDrawOrder", this->shipDrawOrder);
    cJSON* instancesArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "vfxInstances", instancesArray);

    for (const ShipVfxInstance& instance : this->shipVfxInstances)
    {
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }
        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "instanceId", static_cast<double>(instance.instanceId));
        cJSON_AddStringToObject(item, "displayName", imported.displayName.c_str());
        cJSON_AddStringToObject(item, "sourceJsonPath", imported.sourceJsonPath.c_str());
        cJSON_AddStringToObject(item, "storageJsonPath", imported.storageJsonPath.c_str());
        cJSON_AddNumberToObject(item, "offsetX", instance.offsetX);
        cJSON_AddNumberToObject(item, "offsetY", instance.offsetY);
        cJSON_AddNumberToObject(item, "rotationDeg", instance.rotationDeg);
        cJSON_AddBoolToObject(item, "flipHorizontal", instance.flipHorizontal);
        cJSON_AddBoolToObject(item, "flipVertical", instance.flipVertical);
        cJSON_AddNumberToObject(item, "drawOrder", instance.drawOrder);
        cJSON_AddNumberToObject(item, "fps", instance.fps);
        cJSON_AddBoolToObject(item, "followShip", instance.followShip);
        cJSON_AddItemToArray(instancesArray, item);
    }

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);
    if (jsonText == nullptr)
    {
        this->statusMessage = "Echec serialisation JSON.";
        return false;
    }

    const std::filesystem::path jsonPath = folderPath / "ship_vfx.json";
    std::ofstream output(jsonPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        cJSON_free(jsonText);
        this->statusMessage = "Impossible d'ouvrir ship_vfx.json.";
        return false;
    }
    output.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
    const bool ok = output.good();
    output.close();
    cJSON_free(jsonText);

    if (!ok)
    {
        this->statusMessage = "Ecriture ship_vfx.json echouee.";
        return false;
    }

    this->statusMessage = "ship_vfx.json exporte.";
    return true;
}

bool EditorMapVfxScene::exportLooseFolderScaledToFolder(
    const char* absoluteFolderPath,
    const std::string& animationName)
{
    if (this->selectedLooseFolderIndex < 0 || this->selectedLooseFolderIndex >= static_cast<int>(this->importedLooseFolders.size()))
    {
        this->statusMessage = "Selectionne un dossier sprites avant export.";
        return false;
    }
    if (absoluteFolderPath == nullptr || absoluteFolderPath[0] == '\0')
    {
        this->statusMessage = "Dossier export invalide.";
        return false;
    }

    const std::string animationSlug = makeExportAnimationSlug(animationName);
    if (animationSlug.empty())
    {
        this->statusMessage = "Nom animation invalide (lettres/chiffres requis).";
        return false;
    }

    const ImportedLooseFolder& folder = this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)];
    const int clampedPercent = std::clamp(this->looseScalePercent, kLooseScaleMinPercent, kLooseScaleMaxPercent);
    const std::string exportPrefix = "vfx-" + animationSlug;
    const std::string exportSpritesFolderName = exportPrefix + "-sprites-original-downscale";
    const std::filesystem::path outputFolder =
        std::filesystem::path(absoluteFolderPath) / exportPrefix;
    const std::filesystem::path outputSpritesFolder =
        outputFolder / exportSpritesFolderName;
    std::error_code fsError;
    std::filesystem::create_directories(outputSpritesFolder, fsError);
    if (fsError)
    {
        this->statusMessage = "Impossible de creer le dossier export sprites.";
        return false;
    }

    struct ExportedScaledFrame {
        int index;
        int widthPx;
        int heightPx;
        int sheetX;
        int sheetY;
        SDL_Surface* surface;
    };

    std::vector<ExportedScaledFrame> exportedFrames;
    exportedFrames.reserve(folder.sprites.size());

    auto cleanupExportedFrames = [&exportedFrames]() {
        for (ExportedScaledFrame& frame : exportedFrames)
        {
            if (frame.surface != nullptr)
            {
                SDL_DestroySurface(frame.surface);
                frame.surface = nullptr;
            }
        }
    };

    std::vector<size_t> orderedSpriteIndices;
    orderedSpriteIndices.reserve(folder.sprites.size());
    for (size_t i = 0; i < folder.sprites.size(); ++i)
    {
        orderedSpriteIndices.push_back(i);
    }
    std::sort(orderedSpriteIndices.begin(), orderedSpriteIndices.end(), [&folder](size_t lhs, size_t rhs) {
        const std::string& lhsName = folder.sprites[lhs].fileName;
        const std::string& rhsName = folder.sprites[rhs].fileName;

        unsigned long long lhsNumericValue = 0;
        unsigned long long rhsNumericValue = 0;
        const bool lhsIsNumeric = tryParseNumericFrameName(lhsName, &lhsNumericValue);
        const bool rhsIsNumeric = tryParseNumericFrameName(rhsName, &rhsNumericValue);

        if (lhsIsNumeric && rhsIsNumeric)
        {
            if (lhsNumericValue != rhsNumericValue)
            {
                return lhsNumericValue < rhsNumericValue;
            }
            return lhsName < rhsName;
        }
        if (lhsIsNumeric != rhsIsNumeric)
        {
            return lhsIsNumeric;
        }
        return lhsName < rhsName;
    });

    for (size_t orderedIndex = 0; orderedIndex < orderedSpriteIndices.size(); ++orderedIndex)
    {
        const ImportedLooseSprite& sprite =
            folder.sprites[orderedSpriteIndices[orderedIndex]];
        RC2D_ImageData src = rc2d_graphics_loadImageDataFromStorage(sprite.storagePath.c_str(), RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            cleanupExportedFrames();
            this->statusMessage = "Sprite source manquant pour export.";
            return false;
        }

        const int srcW = src.sdl_surface->w;
        const int srcH = src.sdl_surface->h;
        const int dstW = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcW) * clampedPercent) / 100.0)));
        const int dstH = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcH) * clampedPercent) / 100.0)));
        SDL_Surface* dst = SDL_CreateSurface(dstW, dstH, SDL_PIXELFORMAT_RGBA32);
        if (dst == nullptr)
        {
            rc2d_graphics_freeImageData(&src);
            cleanupExportedFrames();
            this->statusMessage = "Creation surface export KO.";
            return false;
        }

        for (int py = 0; py < dstH; ++py)
        {
            const int srcY = std::clamp((py * srcH) / dstH, 0, srcH - 1);
            for (int px = 0; px < dstW; ++px)
            {
                const int srcX = std::clamp((px * srcW) / dstW, 0, srcW - 1);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 0;
                SDL_ReadSurfacePixel(src.sdl_surface, srcX, srcY, &r, &g, &b, &a);
                SDL_WriteSurfacePixel(dst, px, py, r, g, b, a);
            }
        }
        rc2d_graphics_freeImageData(&src);

        const std::filesystem::path dstPath = outputSpritesFolder / sprite.fileName;
        if (!SDL_SavePNG(dst, dstPath.string().c_str()))
        {
            SDL_DestroySurface(dst);
            cleanupExportedFrames();
            this->statusMessage = "Echec ecriture PNG export.";
            return false;
        }

        ExportedScaledFrame frame{};
        frame.index = static_cast<int>(orderedIndex);
        frame.widthPx = dstW;
        frame.heightPx = dstH;
        frame.sheetX = 0;
        frame.sheetY = 0;
        frame.surface = dst;
        exportedFrames.push_back(frame);
    }

    if (exportedFrames.empty())
    {
        this->statusMessage = "Aucun sprite exporte.";
        return false;
    }

    struct FreeRect {
        int x;
        int y;
        int w;
        int h;
    };

    auto nextPowerOfTwo = [](int value) -> int {
        if (value <= 1)
        {
            return 1;
        }
        int v = 1;
        while (v < value && v < (1 << 30))
        {
            v <<= 1;
        }
        return v;
    };

    auto containsRect = [](const FreeRect& outer, const FreeRect& inner) -> bool {
        return inner.x >= outer.x &&
            inner.y >= outer.y &&
            (inner.x + inner.w) <= (outer.x + outer.w) &&
            (inner.y + inner.h) <= (outer.y + outer.h);
    };

    std::vector<int> packingOrder;
    packingOrder.reserve(exportedFrames.size());
    for (size_t i = 0; i < exportedFrames.size(); ++i)
    {
        packingOrder.push_back(static_cast<int>(i));
    }
    std::sort(packingOrder.begin(), packingOrder.end(), [&exportedFrames](int lhs, int rhs) {
        const ExportedScaledFrame& a = exportedFrames[static_cast<size_t>(lhs)];
        const ExportedScaledFrame& b = exportedFrames[static_cast<size_t>(rhs)];
        const int aMaxDim = (std::max)(a.widthPx, a.heightPx);
        const int bMaxDim = (std::max)(b.widthPx, b.heightPx);
        if (aMaxDim != bMaxDim)
        {
            return aMaxDim > bMaxDim;
        }
        const int aArea = a.widthPx * a.heightPx;
        const int bArea = b.widthPx * b.heightPx;
        if (aArea != bArea)
        {
            return aArea > bArea;
        }
        return a.index < b.index;
    });

    auto tryPackWithMaxRects = [&exportedFrames, &packingOrder, &containsRect](int binWidth, int binHeight) -> bool {
        std::vector<FreeRect> freeRects;
        freeRects.push_back(FreeRect{0, 0, binWidth, binHeight});

        for (int packedFrameIndex : packingOrder)
        {
            const ExportedScaledFrame& frame = exportedFrames[static_cast<size_t>(packedFrameIndex)];
            int bestFreeRectIndex = -1;
            int bestShortSideFit = (std::numeric_limits<int>::max)();
            int bestLongSideFit = (std::numeric_limits<int>::max)();

            for (size_t freeIndex = 0; freeIndex < freeRects.size(); ++freeIndex)
            {
                const FreeRect& freeRect = freeRects[freeIndex];
                if (frame.widthPx > freeRect.w || frame.heightPx > freeRect.h)
                {
                    continue;
                }

                const int leftoverHoriz = freeRect.w - frame.widthPx;
                const int leftoverVert = freeRect.h - frame.heightPx;
                const int shortSideFit = (std::min)(leftoverHoriz, leftoverVert);
                const int longSideFit = (std::max)(leftoverHoriz, leftoverVert);

                if (shortSideFit < bestShortSideFit ||
                    (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit))
                {
                    bestShortSideFit = shortSideFit;
                    bestLongSideFit = longSideFit;
                    bestFreeRectIndex = static_cast<int>(freeIndex);
                }
            }

            if (bestFreeRectIndex < 0)
            {
                return false;
            }

            const FreeRect selectedRect = freeRects[static_cast<size_t>(bestFreeRectIndex)];
            const FreeRect usedRect = FreeRect{
                selectedRect.x,
                selectedRect.y,
                frame.widthPx,
                frame.heightPx};
            exportedFrames[static_cast<size_t>(packedFrameIndex)].sheetX = usedRect.x;
            exportedFrames[static_cast<size_t>(packedFrameIndex)].sheetY = usedRect.y;

            std::vector<FreeRect> updatedFreeRects;
            updatedFreeRects.reserve(freeRects.size() + 4U);
            for (const FreeRect& freeRect : freeRects)
            {
                const bool noIntersection =
                    usedRect.x >= (freeRect.x + freeRect.w) ||
                    (usedRect.x + usedRect.w) <= freeRect.x ||
                    usedRect.y >= (freeRect.y + freeRect.h) ||
                    (usedRect.y + usedRect.h) <= freeRect.y;
                if (noIntersection)
                {
                    updatedFreeRects.push_back(freeRect);
                    continue;
                }

                if (usedRect.x > freeRect.x)
                {
                    updatedFreeRects.push_back(
                        FreeRect{freeRect.x, freeRect.y, usedRect.x - freeRect.x, freeRect.h});
                }
                if ((usedRect.x + usedRect.w) < (freeRect.x + freeRect.w))
                {
                    updatedFreeRects.push_back(FreeRect{
                        usedRect.x + usedRect.w,
                        freeRect.y,
                        (freeRect.x + freeRect.w) - (usedRect.x + usedRect.w),
                        freeRect.h});
                }
                if (usedRect.y > freeRect.y)
                {
                    updatedFreeRects.push_back(
                        FreeRect{freeRect.x, freeRect.y, freeRect.w, usedRect.y - freeRect.y});
                }
                if ((usedRect.y + usedRect.h) < (freeRect.y + freeRect.h))
                {
                    updatedFreeRects.push_back(FreeRect{
                        freeRect.x,
                        usedRect.y + usedRect.h,
                        freeRect.w,
                        (freeRect.y + freeRect.h) - (usedRect.y + usedRect.h)});
                }
            }

            freeRects.swap(updatedFreeRects);

            for (size_t i = 0; i < freeRects.size();)
            {
                FreeRect& candidate = freeRects[i];
                if (candidate.w <= 0 || candidate.h <= 0)
                {
                    freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(i));
                    continue;
                }

                bool covered = false;
                for (size_t j = 0; j < freeRects.size(); ++j)
                {
                    if (i == j)
                    {
                        continue;
                    }
                    if (containsRect(freeRects[j], candidate))
                    {
                        covered = true;
                        break;
                    }
                }
                if (covered)
                {
                    freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(i));
                    continue;
                }

                ++i;
            }
        }

        return true;
    };

    constexpr int kLooseExportSheetMaxDimension = 8192;

    int maxFrameWidth = 1;
    int maxFrameHeight = 1;
    long long totalPixels = 0;
    for (const ExportedScaledFrame& frame : exportedFrames)
    {
        maxFrameWidth = (std::max)(maxFrameWidth, frame.widthPx);
        maxFrameHeight = (std::max)(maxFrameHeight, frame.heightPx);
        totalPixels += static_cast<long long>(frame.widthPx) * static_cast<long long>(frame.heightPx);
    }

    if (maxFrameWidth > kLooseExportSheetMaxDimension || maxFrameHeight > kLooseExportSheetMaxDimension)
    {
        this->statusMessage = "Sprites trop grands pour la limite max sheet (8192).";
        return false;
    }

    const double estimatedSide = std::sqrt(static_cast<double>((std::max)(1LL, totalPixels)));
    int binWidth = nextPowerOfTwo((std::max)(maxFrameWidth, static_cast<int>(std::ceil(estimatedSide))));
    int binHeight = nextPowerOfTwo((std::max)(maxFrameHeight, static_cast<int>(std::ceil(estimatedSide))));
    binWidth = (std::min)(binWidth, kLooseExportSheetMaxDimension);
    binHeight = (std::min)(binHeight, kLooseExportSheetMaxDimension);

    bool packed = tryPackWithMaxRects(binWidth, binHeight);
    if (!packed)
    {
        while (!(binWidth == kLooseExportSheetMaxDimension && binHeight == kLooseExportSheetMaxDimension))
        {
            if (binWidth <= binHeight && binWidth < kLooseExportSheetMaxDimension)
            {
                binWidth = (std::min)(binWidth * 2, kLooseExportSheetMaxDimension);
            }
            else if (binHeight < kLooseExportSheetMaxDimension)
            {
                binHeight = (std::min)(binHeight * 2, kLooseExportSheetMaxDimension);
            }
            else
            {
                break;
            }

            if (tryPackWithMaxRects(binWidth, binHeight))
            {
                packed = true;
                break;
            }
        }
    }

    if (!packed)
    {
        this->statusMessage = "Impossible de packer la spritesheet (max 8192x8192).";
        return false;
    }

    int sheetWidth = 1;
    int sheetHeight = 1;
    for (const ExportedScaledFrame& frame : exportedFrames)
    {
        sheetWidth = (std::max)(sheetWidth, frame.sheetX + frame.widthPx);
        sheetHeight = (std::max)(sheetHeight, frame.sheetY + frame.heightPx);
    }

    SDL_Surface* spriteSheet = SDL_CreateSurface(sheetWidth, sheetHeight, SDL_PIXELFORMAT_RGBA32);
    if (spriteSheet == nullptr)
    {
        cleanupExportedFrames();
        this->statusMessage = "Creation spritesheet export KO.";
        return false;
    }

    for (int y = 0; y < sheetHeight; ++y)
    {
        for (int x = 0; x < sheetWidth; ++x)
        {
            SDL_WriteSurfacePixel(spriteSheet, x, y, 0, 0, 0, 0);
        }
    }

    for (const ExportedScaledFrame& frame : exportedFrames)
    {
        if (frame.surface == nullptr)
        {
            continue;
        }

        for (int py = 0; py < frame.heightPx; ++py)
        {
            for (int px = 0; px < frame.widthPx; ++px)
            {
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 0;
                SDL_ReadSurfacePixel(frame.surface, px, py, &r, &g, &b, &a);
                SDL_WriteSurfacePixel(spriteSheet, frame.sheetX + px, frame.sheetY + py, r, g, b, a);
            }
        }
    }

    const std::filesystem::path sheetPath = outputFolder / (exportPrefix + "-spritesheet.png");
    if (!SDL_SavePNG(spriteSheet, sheetPath.string().c_str()))
    {
        SDL_DestroySurface(spriteSheet);
        cleanupExportedFrames();
        this->statusMessage = "Echec ecriture spritesheet.png.";
        return false;
    }

    cJSON* jsonRoot = cJSON_CreateObject();
    if (jsonRoot == nullptr)
    {
        SDL_DestroySurface(spriteSheet);
        cleanupExportedFrames();
        this->statusMessage = "Echec allocation JSON spritesheet.";
        return false;
    }

    cJSON_AddNumberToObject(jsonRoot, "fps", this->getLoosePreviewFpsOrDefault());

    cJSON* framesArray = cJSON_CreateArray();
    cJSON_AddItemToObject(jsonRoot, "frames", framesArray);
    for (const ExportedScaledFrame& frame : exportedFrames)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "index", frame.index);
        cJSON_AddNumberToObject(item, "x", frame.sheetX);
        cJSON_AddNumberToObject(item, "y", frame.sheetY);
        cJSON_AddNumberToObject(item, "w", frame.widthPx);
        cJSON_AddNumberToObject(item, "h", frame.heightPx);
        cJSON_AddItemToArray(framesArray, item);
    }

    char* jsonText = cJSON_Print(jsonRoot);
    cJSON_Delete(jsonRoot);
    if (jsonText == nullptr)
    {
        SDL_DestroySurface(spriteSheet);
        cleanupExportedFrames();
        this->statusMessage = "Echec serialisation spritesheet.json.";
        return false;
    }

    const std::filesystem::path jsonPath = outputFolder / (exportPrefix + "-spritesheet.json");
    std::ofstream jsonOutput(jsonPath, std::ios::binary | std::ios::trunc);
    if (!jsonOutput.is_open())
    {
        cJSON_free(jsonText);
        SDL_DestroySurface(spriteSheet);
        cleanupExportedFrames();
        this->statusMessage = "Impossible d'ouvrir spritesheet.json.";
        return false;
    }
    jsonOutput.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
    const bool jsonOk = jsonOutput.good();
    jsonOutput.close();
    cJSON_free(jsonText);

    SDL_DestroySurface(spriteSheet);
    cleanupExportedFrames();

    if (!jsonOk)
    {
        this->statusMessage = "Ecriture spritesheet.json echouee.";
        return false;
    }

    this->statusMessage =
        "Export VFX OK: " + exportPrefix +
        " (" + std::to_string(clampedPercent) +
        "%, " +
        std::to_string(static_cast<int>(exportedFrames.size())) +
        " frames 0.." +
        std::to_string(static_cast<int>(exportedFrames.size()) - 1) +
        ").";
    return true;
}

void EditorMapVfxScene::drawShipVfxPreview(void) const
{
    const Map& map = GetCurrentMap();
    const SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;

    std::vector<RenderItem> items;
    items.reserve(this->shipVfxInstances.size() + 1U);
    items.push_back(RenderItem{true, this->shipDrawOrder, 0U, -1});
    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        items.push_back(RenderItem{false, instance.drawOrder, instance.instanceId, static_cast<int>(i)});
    }

    std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
        if (a.drawOrder != b.drawOrder)
        {
            return a.drawOrder < b.drawOrder;
        }
        return a.instanceId < b.instanceId;
    });

    for (const RenderItem& item : items)
    {
        if (item.isShip)
        {
            if (this->previewShipLoaded)
            {
                this->previewShip.draw(map);
            }
            continue;
        }

        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(item.instanceIndex)];
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }

        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        if (imported.atlas.atlas_image.sdl_texture == nullptr || imported.frameNames.empty())
        {
            continue;
        }

        const int frameCount = static_cast<int>(imported.frameNames.size());
        const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(instance.fps, 1.0f))) % frameCount;
        const RC2D_TP_Frame* frame = rc2d_tp_getFrame(&imported.atlas, imported.frameNames[static_cast<size_t>(frameIndex)].c_str());
        if (frame == nullptr)
        {
            continue;
        }

        const float sourceW = (frame->sourceSize.x > 0.0f) ? frame->sourceSize.x : frame->frame.w;
        const float sourceH = (frame->sourceSize.y > 0.0f) ? frame->sourceSize.y : frame->frame.h;
        const float offsetX = instance.offsetX * scale;
        const float offsetY = instance.offsetY * scale;
        const float sourceLeft = shipCenter.x + offsetX - ((sourceW * scale) * 0.5f);
        const float sourceTop = shipCenter.y + offsetY - ((sourceH * scale) * 0.5f);
        const float drawX = sourceLeft + (frame->spriteSourceSize.x * scale);
        const float drawY = sourceTop + (frame->spriteSourceSize.y * scale);
        const float pivotX = (sourceW * 0.5f) - frame->spriteSourceSize.x;
        const float pivotY = (sourceH * 0.5f) - frame->spriteSourceSize.y;

        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&imported.atlas.atlas_image),
            frame->frame.x,
            frame->frame.y,
            frame->frame.w,
            frame->frame.h);

        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&imported.atlas.atlas_image),
            &sourceQuad,
            drawX,
            drawY,
            instance.rotationDeg,
            scale,
            scale,
            pivotX,
            pivotY,
            instance.flipHorizontal,
            instance.flipVertical);
    }

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
        if (instance.importedSfxIndex >= 0 && instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
            if (!imported.frameNames.empty())
            {
                const int frameCount = static_cast<int>(imported.frameNames.size());
                const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(instance.fps, 1.0f))) % frameCount;
                    const RC2D_TP_Frame* frame = rc2d_tp_getFrame(&imported.atlas, imported.frameNames[static_cast<size_t>(frameIndex)].c_str());
                if (frame != nullptr)
                {
                    const float sourceW = (frame->sourceSize.x > 0.0f) ? frame->sourceSize.x : frame->frame.w;
                    const float sourceH = (frame->sourceSize.y > 0.0f) ? frame->sourceSize.y : frame->frame.h;
                    const float offsetX = instance.offsetX * scale;
                    const float offsetY = instance.offsetY * scale;
                    SDL_FRect bounds{};
                    bounds.x = shipCenter.x + offsetX - ((sourceW * scale) * 0.5f);
                    bounds.y = shipCenter.y + offsetY - ((sourceH * scale) * 0.5f);
                    bounds.w = sourceW * scale;
                    bounds.h = sourceH * scale;

                    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
                    rc2d_graphics_setColor(RC2D_Color{255, 220, 120, 220});
                    rc2d_graphics_rectangle("line", &bounds);
                    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
                }
            }
        }
    }
}

void EditorMapVfxScene::drawLooseReferencePreview(void) const
{
    if (!this->looseReferencePreviewVisible || !this->looseReferencePreviewLoaded)
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const float zoom = std::clamp(GetCamera().getZoomFactor(), kLoosePreviewZoomMin, kLoosePreviewZoomMax);

    // Meme logique que EditorMapCreateMapScene::drawPlacedAssets:
    // ancrage monde en tuile + drawQuad top-left + scale = world zoom.
    auto drawReferenceAtTileTopLeft = [&map, zoom](const RC2D_Image* image, float tileX, float tileY, float screenOffsetX, float screenOffsetY) {
        if (image == nullptr || image->sdl_texture == nullptr)
        {
            return;
        }

        const SDL_FPoint anchorScreen = map.tileToScreenCenterFloat(tileX, tileY);
        const float texW = static_cast<float>(image->sdl_texture->w);
        const float texH = static_cast<float>(image->sdl_texture->h);
        RC2D_Image* mutableImage = const_cast<RC2D_Image*>(image);
        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            mutableImage,
            0.0f,
            0.0f,
            texW,
            texH);
        rc2d_graphics_drawQuad(
            mutableImage,
            &sourceQuad,
            anchorScreen.x + (screenOffsetX * zoom),
            anchorScreen.y + (screenOffsetY * zoom),
            0.0,
            zoom,
            zoom,
            0.0f,
            0.0f,
            false,
            false);
    };

    const SDL_FPoint islandTile = SDL_FPoint{
        this->previewShipTile.x - 9.0f,
        this->previewShipTile.y};
    drawReferenceAtTileTopLeft(&this->looseReferenceGuildIslandImage, islandTile.x, islandTile.y, 0.0f, 0.0f);

    drawReferenceAtTileTopLeft(&this->looseReferenceTowerLevel1Image, islandTile.x - 7.0f, islandTile.y - 7.0f, 0.0f, 0.0f);
    drawReferenceAtTileTopLeft(&this->looseReferenceTowerLevel2Image, islandTile.x + 7.0f, islandTile.y - 7.0f, 0.0f, 0.0f);
    drawReferenceAtTileTopLeft(&this->looseReferenceTowerLevel3Image, islandTile.x - 7.0f, islandTile.y + 7.0f, 0.0f, 0.0f);
    drawReferenceAtTileTopLeft(&this->looseReferenceTowerLevel4Image, islandTile.x + 7.0f, islandTile.y + 7.0f, 0.0f, 0.0f);

    drawReferenceAtTileTopLeft(
        &this->looseReferenceShipLeftImage,
        this->previewShipTile.x + 20.0f,
        this->previewShipTile.y + 10.0f,
        -225.0f,
        500.0f);
    drawReferenceAtTileTopLeft(
        &this->looseReferenceShipRightImage,
        this->previewShipTile.x + 31.0f,
        this->previewShipTile.y + 14.0f,
        225.0f,
        500.0f);
}

void EditorMapVfxScene::drawLooseSpritesPreview(void) const
{
    this->drawLooseReferencePreview();

    if (this->selectedLooseFolderIndex < 0 || this->selectedLooseFolderIndex >= static_cast<int>(this->importedLooseFolders.size()))
    {
        return;
    }

    const ImportedLooseFolder& folder = this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)];
    if (folder.sprites.empty())
    {
        return;
    }

    const Map& map = GetCurrentMap();
    float maxSpriteW = 1.0f;
    float maxSpriteH = 1.0f;
    for (const ImportedLooseSprite& sprite : folder.sprites)
    {
        maxSpriteW = (std::max)(maxSpriteW, sprite.widthPx);
        maxSpriteH = (std::max)(maxSpriteH, sprite.heightPx);
    }

    const int spriteCount = static_cast<int>(folder.sprites.size());
    const int columns = (std::max)(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(spriteCount)))));
    const int rows = (std::max)(1, static_cast<int>(std::ceil(static_cast<float>(spriteCount) / static_cast<float>(columns))));
    const float requestedScale = static_cast<float>(this->looseScalePercent) / 100.0f;
    const float zoomMultiplier = std::clamp(GetCamera().getZoomFactor(), kLoosePreviewZoomMin, kLoosePreviewZoomMax);
    const float previewScale = requestedScale * zoomMultiplier;
    const float cellPadding = 18.0f * zoomMultiplier;
    const float cellW = (maxSpriteW * previewScale) + cellPadding;
    const float cellH = (maxSpriteH * previewScale) + cellPadding;
    const float gridW = (static_cast<float>(columns) * cellW) - cellPadding;
    const float gridH = (static_cast<float>(rows) * cellH) - cellPadding;
    const SDL_FPoint anchorCenter =
        (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
        ? SDL_FPoint{
            map.rect.x + (map.rect.w * 0.5f),
            map.rect.y + (map.rect.h * 0.5f)}
        : map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    const float startX = anchorCenter.x - (gridW * 0.5f);
    const float startY = anchorCenter.y - (gridH * 0.5f);

    for (int i = 0; i < spriteCount; ++i)
    {
        const ImportedLooseSprite& sprite = folder.sprites[static_cast<size_t>(i)];
        const int col = i % columns;
        const int row = i / columns;
        const float cellX = startX + (static_cast<float>(col) * cellW);
        const float cellY = startY + (static_cast<float>(row) * cellH);
        const float cellCenterX = cellX + ((maxSpriteW * previewScale) * 0.5f);
        const float cellCenterY = cellY + ((maxSpriteH * previewScale) * 0.5f);
        const float drawW = sprite.widthPx * previewScale;
        const float drawH = sprite.heightPx * previewScale;
        const float drawX = cellCenterX - (drawW * 0.5f);
        const float drawY = cellCenterY - (drawH * 0.5f);
        RC2D_Image* spriteImage = const_cast<RC2D_Image*>(&sprite.image);
        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            spriteImage,
            0.0f,
            0.0f,
            sprite.widthPx,
            sprite.heightPx);
        rc2d_graphics_drawQuad(
            spriteImage,
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
    }
}

void EditorMapVfxScene::drawLoosePlacementPreview(void) const
{
    this->drawLooseReferencePreview();

    if (this->selectedLooseFolderIndex < 0 || this->selectedLooseFolderIndex >= static_cast<int>(this->importedLooseFolders.size()))
    {
        return;
    }

    const ImportedLooseFolder& folder = this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)];
    if (folder.sprites.empty())
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const float requestedScale = static_cast<float>(this->looseScalePercent) / 100.0f;
    const float zoomMultiplier = std::clamp(GetCamera().getZoomFactor(), kLoosePreviewZoomMin, kLoosePreviewZoomMax);
    const float previewScale = requestedScale * zoomMultiplier;
    const float nowSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const int frameCount = static_cast<int>(folder.sprites.size());

    auto drawAnimatedAtTile = [&](float tileX, float tileY, float fps) {
        if (frameCount <= 0)
        {
            return;
        }

        const float clampedFps = std::clamp(fps, kSfxFpsMin, kSfxFpsMax);
        const int frameIndex = static_cast<int>(std::floor(nowSeconds * (std::max)(clampedFps, 1.0f))) % frameCount;
        const ImportedLooseSprite& sprite = folder.sprites[static_cast<size_t>(frameIndex)];
        RC2D_Image* spriteImage = const_cast<RC2D_Image*>(&sprite.image);

        const SDL_FPoint screenPos = map.tileToScreenCenterFloat(tileX, tileY);
        const float drawW = sprite.widthPx * previewScale;
        const float drawH = sprite.heightPx * previewScale;
        const float drawX = screenPos.x - (drawW * 0.5f);
        const float drawY = screenPos.y - (drawH * 0.5f);

        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            spriteImage,
            0.0f,
            0.0f,
            sprite.widthPx,
            sprite.heightPx);
        rc2d_graphics_drawQuad(
            spriteImage,
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
    };

    for (const LoosePreviewPlacement& placement : this->loosePreviewPlacements)
    {
        drawAnimatedAtTile(placement.tileX, placement.tileY, placement.fps);
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (this->getMouseRenderPosition(&mouseX, &mouseY) && this->pointInRect(mouseX, mouseY, map.rect))
    {
        const SDL_Point hoveredTile = map.screenToTileNearest(mouseX, mouseY);
        drawAnimatedAtTile(
            static_cast<float>(hoveredTile.x),
            static_cast<float>(hoveredTile.y),
            this->getLoosePreviewFpsOrDefault());
    }
}

void EditorMapVfxScene::drawHud(void) const
{
    const bool shipMode = (this->editorMode == EditorMode::SHIP_VFX);

    this->drawToolbarButton(this->buttonModeShipVfxRect, "MODE : SHIP / VFX", shipMode);
    this->drawToolbarButton(this->buttonModeLooseSpritesRect, "MODE : DOWNSCALE SPRITES VFX / SPRITESHEET", !shipMode);
    this->drawToolbarButton(this->buttonExportRect, "EXPORTER", false);
    this->drawToolbarButton(this->buttonOceanPrevRect, "OCEAN -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "OCEAN +", false);

    if (shipMode)
    {
        std::vector<std::string> shipLabels;
        shipLabels.reserve(this->importedShips.size());
        for (const ImportedShip& ship : this->importedShips)
        {
            shipLabels.push_back(ship.displayName);
        }
        this->drawListPanel(this->shipListRect, "Navires", shipLabels, this->selectedShipIndex, this->shipListScrollOffset);

        std::vector<std::string> sfxLabels;
        sfxLabels.reserve(this->importedSfx.size());
        for (const ImportedSfx& sfx : this->importedSfx)
        {
            sfxLabels.push_back(sfx.displayName);
        }
        this->drawListPanel(this->sfxListRect, "VFX", sfxLabels, this->selectedSfxIndex, this->sfxListScrollOffset);
    }
    else
    {
        this->drawToolbarButton(this->buttonImportLooseRect, "IMPORTER DES DOSSIERS DE SPRITES VFX", false);
        this->drawToolbarButton(this->buttonLooseScaleMinusRect, "SCALE SPRITES VFX -5%", false);
        this->drawToolbarButton(this->buttonLooseScalePlusRect, "SCALE SPRITES VFX +5%", false);
        this->drawToolbarButton(this->buttonLooseZoomMinusRect, "ZOOM -", false);
        this->drawToolbarButton(this->buttonLooseZoomPlusRect, "ZOOM +", false);
        this->drawToolbarButton(
            this->buttonLoosePreviewModeRect,
            (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
                ? "SOUS MODE : PREVIEW"
                : "SOUS MODE : SPRITESHEET",
            false);
        this->drawToolbarButton(
            this->buttonLooseReferencePreviewRect,
            "AFFICHER ILE+NAVIRES+TOURS",
            this->looseReferencePreviewVisible);

        if (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            const RC2D_Color fpsFillColor = this->loosePreviewFpsInputFocused
                ? RC2D_Color{58, 88, 122, 210}
                : RC2D_Color{28, 38, 50, 205};
            const RC2D_Color fpsBorderColor = this->loosePreviewFpsInputFocused
                ? RC2D_Color{124, 186, 236, 245}
                : RC2D_Color{108, 126, 148, 220};
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            rc2d_graphics_setColor(fpsFillColor);
            rc2d_graphics_rectangle("fill", &this->buttonLoosePreviewFpsInputRect);
            rc2d_graphics_setColor(fpsBorderColor);
            rc2d_graphics_rectangle("line", &this->buttonLoosePreviewFpsInputRect);
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

            std::string fpsValue = this->loosePreviewFpsInputFocused
                ? this->loosePreviewFpsInput
                : formatSfxFpsValue(this->getLoosePreviewFpsOrDefault());
            std::string fpsLabel = "Frame/S Preview: " + fpsValue;
            if (this->loosePreviewFpsInputFocused)
            {
                fpsLabel += "_";
            }
            RC2D_Text fpsInputText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), fpsLabel.c_str());
            fpsInputText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&fpsInputText);
            rc2d_graphics_drawText(&fpsInputText, this->buttonLoosePreviewFpsInputRect.x + 6.0f, this->buttonLoosePreviewFpsInputRect.y + 2.0f);
            rc2d_graphics_destroyText(&fpsInputText);

            this->drawToolbarButton(this->buttonLooseClearAllVfxRect, "CLEAR ALL VFX", false);
        }

        std::vector<std::string> looseLabels;
        looseLabels.reserve(this->importedLooseFolders.size());
        for (const ImportedLooseFolder& folder : this->importedLooseFolders)
        {
            looseLabels.push_back(folder.displayName);
        }
        this->drawListPanel(this->looseListRect, "SPRITES VFX PNG - DOSSIERS", looseLabels, this->selectedLooseFolderIndex, this->looseListScrollOffset);
    }

    if (this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    char infoBuffer[512] = {};
    const char* oceanLabel = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label;
    if (shipMode)
    {
        const char* shipName =
            (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
            ? this->importedShips[static_cast<size_t>(this->selectedShipIndex)].displayName.c_str()
            : "Aucun";
        SDL_snprintf(
            infoBuffer,
            sizeof(infoBuffer),
            "Mode Ship / VFX | Ship: %s | Instances: %d | ShipOrder: %d | Ocean: %s | Frame/S: %s",
            shipName,
            static_cast<int>(this->shipVfxInstances.size()),
            this->shipDrawOrder,
            oceanLabel,
            this->vfxFpsInput.c_str());
    }
    else
    {
        const char* folderName =
            (this->selectedLooseFolderIndex >= 0 && this->selectedLooseFolderIndex < static_cast<int>(this->importedLooseFolders.size()))
            ? this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)].displayName.c_str()
            : "Aucun";
        const char* looseSubMode =
            (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
            ? "Preview clic"
            : "Spritesheet centree";
        SDL_snprintf(
            infoBuffer,
            sizeof(infoBuffer),
            "Mode Downscale Sprites VFX | Sous-mode: %s | Dossier: %s | Scale export: %d%% | Zoom: %.2f | Placements: %d | Frame/S: %s | Ocean: %s",
            looseSubMode,
            folderName,
            this->looseScalePercent,
            this->loosePreviewZoomFactor,
            static_cast<int>(this->loosePreviewPlacements.size()),
            this->loosePreviewFpsInput.c_str(),
            oceanLabel);
    }

    RC2D_Text infoText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), infoBuffer);
    infoText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&infoText);
    rc2d_graphics_drawText(&infoText, GetCurrentMap().rect.x + 12.0f, GetCurrentMap().rect.y + 10.0f);
    rc2d_graphics_destroyText(&infoText);

    RC2D_Text statusText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), this->statusMessage.c_str());
    statusText.color = kHudStatusColor;
    rc2d_graphics_setTextColor(&statusText);
    rc2d_graphics_drawText(&statusText, GetCurrentMap().rect.x + 12.0f, GetCurrentMap().rect.y + 30.0f);
    rc2d_graphics_destroyText(&statusText);

    this->drawLooseExportNamePopup();
}

void EditorMapVfxScene::drawLooseExportNamePopup(void) const
{
    if (!this->looseExportNamePopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    const SDL_FRect mapRect = GetCurrentMap().rect;
    const float popupW = std::clamp(mapRect.w - 140.0f, 420.0f, 860.0f);
    const float popupH = 165.0f;
    const SDL_FRect popupRect = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};
    const SDL_FRect inputRect = SDL_FRect{
        popupRect.x + 18.0f,
        popupRect.y + 64.0f,
        popupRect.w - 36.0f,
        32.0f};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 150});
    rc2d_graphics_rectangle("fill", &mapRect);
    rc2d_graphics_setColor(RC2D_Color{22, 30, 40, 235});
    rc2d_graphics_rectangle("fill", &popupRect);
    rc2d_graphics_setColor(RC2D_Color{145, 168, 194, 245});
    rc2d_graphics_rectangle("line", &popupRect);
    rc2d_graphics_setColor(RC2D_Color{40, 54, 70, 245});
    rc2d_graphics_rectangle("fill", &inputRect);
    rc2d_graphics_setColor(RC2D_Color{124, 186, 236, 245});
    rc2d_graphics_rectangle("line", &inputRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    RC2D_Text titleText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "EXPORT -  NOM ANIMATION");
    titleText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&titleText);
    rc2d_graphics_drawText(&titleText, popupRect.x + 18.0f, popupRect.y + 14.0f);
    rc2d_graphics_destroyText(&titleText);

    std::string inputLabel = "Nom: " + this->looseExportNameInput + "_";
    RC2D_Text inputText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), inputLabel.c_str());
    inputText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&inputText);
    rc2d_graphics_drawText(&inputText, inputRect.x + 6.0f, inputRect.y + 6.0f);
    rc2d_graphics_destroyText(&inputText);

    RC2D_Text hintText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "ENTREE: valider | ECHAP: annuler | caracteres recommandes: a-z 0-9 - _ espace");
    hintText.color = RC2D_Color{208, 220, 236, 230};
    rc2d_graphics_setTextColor(&hintText);
    rc2d_graphics_drawText(&hintText, popupRect.x + 18.0f, popupRect.y + 110.0f);
    rc2d_graphics_destroyText(&hintText);
}

bool EditorMapVfxScene::handleShipListClick(float x, float y)
{
    int clickedIndex = -1;
    const bool consumed = this->handleListPanelClick(
        x,
        y,
        this->shipListRect,
        static_cast<int>(this->importedShips.size()),
        &this->shipListScrollOffset,
        &this->shipListScrollDragActive,
        &this->shipListScrollDragGrabOffsetY,
        &clickedIndex);
    if (!consumed)
    {
        return false;
    }
    if (clickedIndex >= 0)
    {
        this->selectImportedShipAtIndex(clickedIndex);
    }
    return true;
}

bool EditorMapVfxScene::handleSfxListClick(float x, float y)
{
    int clickedIndex = -1;
    const bool consumed = this->handleListPanelClick(
        x,
        y,
        this->sfxListRect,
        static_cast<int>(this->importedSfx.size()),
        &this->sfxListScrollOffset,
        &this->sfxListScrollDragActive,
        &this->sfxListScrollDragGrabOffsetY,
        &clickedIndex);
    if (!consumed)
    {
        return false;
    }
    if (clickedIndex >= 0)
    {
        this->selectedSfxIndex = clickedIndex;
        this->ensureSelectionVisible(this->selectedSfxIndex, &this->sfxListScrollOffset, static_cast<int>(this->importedSfx.size()));
        this->spawnSelectedSfxAtShipCenter();
    }
    return true;
}

bool EditorMapVfxScene::handleLooseListClick(float x, float y)
{
    int clickedIndex = -1;
    const bool consumed = this->handleListPanelClick(
        x,
        y,
        this->looseListRect,
        static_cast<int>(this->importedLooseFolders.size()),
        &this->looseListScrollOffset,
        &this->looseListScrollDragActive,
        &this->looseListScrollDragGrabOffsetY,
        &clickedIndex);
    if (!consumed)
    {
        return false;
    }
    if (clickedIndex >= 0)
    {
        this->selectedLooseFolderIndex = clickedIndex;
        this->ensureSelectionVisible(this->selectedLooseFolderIndex, &this->looseListScrollOffset, static_cast<int>(this->importedLooseFolders.size()));
        this->loosePreviewPlacements.clear();
        this->nextLoosePreviewPlacementId = 1U;
        this->statusMessage = "Dossier sprites actif: " + this->importedLooseFolders[static_cast<size_t>(this->selectedLooseFolderIndex)].displayName;
    }
    return true;
}

bool EditorMapVfxScene::handleToolbarClick(float x, float y)
{
    if (this->pointInRect(x, y, this->buttonModeShipVfxRect))
    {
        this->editorMode = EditorMode::SHIP_VFX;
        this->loosePreviewFpsInputFocused = false;
        this->statusMessage = "Mode Ship / VFX actif.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonModeLooseSpritesRect))
    {
        this->editorMode = EditorMode::LOOSE_SPRITES;
        this->vfxFpsInputFocused = false;
        this->statusMessage = "Mode Downscale Sprites VFX actif.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonExportRect))
    {
        if (this->editorMode == EditorMode::LOOSE_SPRITES)
        {
            this->openLooseExportNamePopup();
        }
        else
        {
            this->openExportFolderDialog();
        }
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

    if (this->editorMode == EditorMode::LOOSE_SPRITES)
    {
        if (this->pointInRect(x, y, this->buttonImportLooseRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->openImportLooseFolderDialog();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLoosePreviewModeRect))
        {
            this->loosePreviewMode = (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
                ? LoosePreviewMode::PLACEMENT_PREVIEW
                : LoosePreviewMode::CENTER_SPRITESHEET;
            this->loosePreviewFpsInputFocused = false;
            if (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
            {
                Camera& camera = GetCamera();
                Map& map = GetCurrentMap();
                camera.setZoomFactor(kLoosePreviewZoomDefault);
                camera.update(map, map.rect);
                this->looseReferencePreviewVisible = false;
                this->statusMessage = "Mode spritesheet centree actif (references OFF).";
            }
            else
            {
                Camera& camera = GetCamera();
                Map& map = GetCurrentMap();
                const float restoredZoom = std::clamp(
                    this->loosePreviewZoomFactor,
                    kLoosePreviewZoomMin,
                    kLoosePreviewZoomMax);
                camera.setZoomFactor(restoredZoom);
                camera.update(map, map.rect);
                this->looseReferencePreviewVisible = this->looseReferencePreviewLoaded;
                this->statusMessage = "Mode preview clic VFX actif (references ON, clic gauche pour poser, clic droit pour retirer).";
            }
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLoosePreviewFpsInputRect) &&
            this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->loosePreviewFpsInput = formatSfxFpsValue(this->getLoosePreviewFpsOrDefault());
            this->loosePreviewFpsInputFocused = true;
            this->vfxFpsInputFocused = false;
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseClearAllVfxRect) &&
            this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewPlacements.clear();
            this->nextLoosePreviewPlacementId = 1U;
            this->statusMessage = "Toutes les instances preview VFX ont ete supprimees.";
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseScaleMinusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->looseScalePercent = std::clamp(this->looseScalePercent - kLooseScaleStepPercent, kLooseScaleMinPercent, kLooseScaleMaxPercent);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseScalePlusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->looseScalePercent = std::clamp(this->looseScalePercent + kLooseScaleStepPercent, kLooseScaleMinPercent, kLooseScaleMaxPercent);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseZoomMinusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->adjustLoosePreviewZoom(-0.05f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseZoomPlusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->adjustLoosePreviewZoom(0.05f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseReferencePreviewRect))
        {
            this->loosePreviewFpsInputFocused = false;
            if (!this->looseReferencePreviewLoaded)
            {
                this->statusMessage = "References visuelles indisponibles (assets manquants).";
                return true;
            }
            this->looseReferencePreviewVisible = !this->looseReferencePreviewVisible;
            this->statusMessage = this->looseReferencePreviewVisible
                ? "References visuelles activees."
                : "References visuelles masquees.";
            return true;
        }
    }

    this->vfxFpsInputFocused = false;
    this->loosePreviewFpsInputFocused = false;
    return false;
}

bool EditorMapVfxScene::handlePreviewClick(float x, float y, RC2D_MouseButton button)
{
    if (this->editorMode != EditorMode::SHIP_VFX)
    {
        return false;
    }
    if (!this->pointInRect(x, y, GetCurrentMap().rect))
    {
        return false;
    }

    const int hitIndex = this->findTopmostVfxInstanceIndexAtPoint(x, y);
    if (button == RC2D_MOUSE_BUTTON_RIGHT)
    {
        if (hitIndex >= 0)
        {
            this->setSelectedVfxInstanceIndex(hitIndex);
            this->removeSelectedVfxInstance();
        }
        return true;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->setSelectedVfxInstanceIndex(hitIndex);
        if (hitIndex >= 0)
        {
            this->statusMessage = "VFX instance selectionnee.";
        }
        return true;
    }
    return false;
}

bool EditorMapVfxScene::handleLoosePlacementPreviewClick(float x, float y, RC2D_MouseButton button)
{
    if (this->editorMode != EditorMode::LOOSE_SPRITES ||
        this->loosePreviewMode != LoosePreviewMode::PLACEMENT_PREVIEW)
    {
        return false;
    }

    Map& map = GetCurrentMap();
    if (!this->pointInRect(x, y, map.rect))
    {
        return false;
    }

    if (this->selectedLooseFolderIndex < 0 || this->selectedLooseFolderIndex >= static_cast<int>(this->importedLooseFolders.size()))
    {
        this->statusMessage = "Selectionne d'abord un dossier sprites.";
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        if (!this->applyLoosePreviewFpsInput())
        {
            return true;
        }

        const SDL_Point tile = map.screenToTileNearest(x, y);
        LoosePreviewPlacement placement{};
        placement.instanceId = this->nextLoosePreviewPlacementId++;
        placement.tileX = static_cast<float>(tile.x);
        placement.tileY = static_cast<float>(tile.y);
        placement.fps = this->getLoosePreviewFpsOrDefault();
        this->loosePreviewPlacements.push_back(placement);

        this->statusMessage =
            "Preview VFX place (instances: " +
            std::to_string(static_cast<int>(this->loosePreviewPlacements.size())) +
            ").";
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_RIGHT)
    {
        if (this->loosePreviewPlacements.empty())
        {
            this->statusMessage = "Aucune instance preview a retirer.";
            return true;
        }

        int bestIndex = -1;
        float bestDistSq = 1.0e30f;
        for (size_t i = 0; i < this->loosePreviewPlacements.size(); ++i)
        {
            const LoosePreviewPlacement& placement = this->loosePreviewPlacements[i];
            const SDL_FPoint screenPos = map.tileToScreenCenterFloat(placement.tileX, placement.tileY);
            const float dx = screenPos.x - x;
            const float dy = screenPos.y - y;
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestIndex = static_cast<int>(i);
            }
        }

        constexpr float kRemoveRadiusPx = 80.0f;
        if (bestIndex >= 0 && bestDistSq <= (kRemoveRadiusPx * kRemoveRadiusPx))
        {
            this->loosePreviewPlacements.erase(this->loosePreviewPlacements.begin() + bestIndex);
            this->statusMessage =
                "Instance preview retiree (restant: " +
                std::to_string(static_cast<int>(this->loosePreviewPlacements.size())) +
                ").";
        }
        else
        {
            this->statusMessage = "Aucune instance preview proche du clic.";
        }
        return true;
    }

    return false;
}

void EditorMapVfxScene::onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapVfxScene* scene = static_cast<EditorMapVfxScene*>(userdata);
    if (scene == nullptr || scene != EditorMapVfxScene::activeInstance)
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

void EditorMapVfxScene::onImportSfxFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapVfxScene* scene = static_cast<EditorMapVfxScene*>(userdata);
    if (scene == nullptr || scene != EditorMapVfxScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingSfxFolderMutex);
    scene->pendingSfxFolderDialogCompleted = true;
    scene->pendingSfxFolderAbsolute.clear();
    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingSfxFolderDialogCanceled = true;
        return;
    }
    scene->pendingSfxFolderDialogCanceled = false;
    scene->pendingSfxFolderAbsolute = filelist[0];
}

void EditorMapVfxScene::onImportLooseFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapVfxScene* scene = static_cast<EditorMapVfxScene*>(userdata);
    if (scene == nullptr || scene != EditorMapVfxScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingLooseFolderMutex);
    scene->pendingLooseFolderDialogCompleted = true;
    scene->pendingLooseFolderAbsolute.clear();
    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingLooseFolderDialogCanceled = true;
        return;
    }
    scene->pendingLooseFolderDialogCanceled = false;
    scene->pendingLooseFolderAbsolute = filelist[0];
}

void EditorMapVfxScene::onExportFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapVfxScene* scene = static_cast<EditorMapVfxScene*>(userdata);
    if (scene == nullptr || scene != EditorMapVfxScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingExportFolderMutex);
    scene->pendingExportFolderDialogCompleted = true;
    scene->pendingExportFolderAbsolute.clear();
    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingExportFolderDialogCanceled = true;
        return;
    }
    scene->pendingExportFolderDialogCanceled = false;
    scene->pendingExportFolderAbsolute = filelist[0];
}

void EditorMapVfxScene::unload(void)
{
    if (EditorMapVfxScene::activeInstance == this)
    {
        EditorMapVfxScene::activeInstance = nullptr;
    }

    GetOceanShader().unload();
    this->previewShip.unloadSprites();
    this->previewShipLoaded = false;
    this->loadedShipFolderAbsolute.clear();
    this->setSelectedVfxInstanceIndex(-1);
    this->importedShips.clear();
    this->unloadImportedSfx();
    this->unloadImportedLooseFolders();
    this->unloadLooseReferencePreviewAssets();

    {
        std::lock_guard<std::mutex> lock(this->pendingShipFolderMutex);
        this->pendingShipFolderDialogCompleted = false;
        this->pendingShipFolderDialogCanceled = false;
        this->pendingShipFolderAbsolute.clear();
    }
    {
        std::lock_guard<std::mutex> lock(this->pendingSfxFolderMutex);
        this->pendingSfxFolderDialogCompleted = false;
        this->pendingSfxFolderDialogCanceled = false;
        this->pendingSfxFolderAbsolute.clear();
    }
    {
        std::lock_guard<std::mutex> lock(this->pendingLooseFolderMutex);
        this->pendingLooseFolderDialogCompleted = false;
        this->pendingLooseFolderDialogCanceled = false;
        this->pendingLooseFolderAbsolute.clear();
    }
    {
        std::lock_guard<std::mutex> lock(this->pendingExportFolderMutex);
        this->pendingExportFolderDialogCompleted = false;
        this->pendingExportFolderDialogCanceled = false;
        this->pendingExportFolderAbsolute.clear();
        this->pendingExportMode = EditorMode::SHIP_VFX;
    }
    this->looseExportNamePopupVisible = false;
    this->looseExportNameInput.clear();
    this->pendingLooseExportAnimationName.clear();

    this->scrollBarOverlay.unload();
    rc2d_graphics_closeFont(&this->overlayFont);
    rc2d_graphics_freeImage(&this->backgroundUiImage);

    RC2D_log(RC2D_LOG_INFO, "EditorMapVfxScene: unloaded");
}

void EditorMapVfxScene::load(void)
{
    EditorMapVfxScene::activeInstance = this;
    this->resetEditorState();
    this->ensureUserStorageFolders();

    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/background.png",
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

    camera.setZoomFactor(kLoosePreviewZoomDefault);
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    this->previewShipTile = SDL_FPoint{
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y)};

    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    this->loadLooseReferencePreviewAssets();
    this->applySelectedOceanColor();
    this->statusMessage = "Editor VFX charge. Importe navires, VFX atlas ou sprites.";

    RC2D_log(RC2D_LOG_INFO, "EditorMapVfxScene: loaded");
}

void EditorMapVfxScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    map.update();
    this->updateToolbarLayout();
    this->processPendingShipFolderRequest();
    this->processPendingSfxFolderRequest();
    this->processPendingLooseFolderRequest();
    this->processPendingExportFolderRequest();
    this->applyPendingOceanColorStep();

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        this->handleListPanelScrollDragFromMouse(
            this->shipListRect,
            static_cast<int>(this->importedShips.size()),
            &this->shipListScrollOffset,
            &this->shipListScrollDragActive,
            &this->shipListScrollDragGrabOffsetY);
        this->handleListPanelScrollDragFromMouse(
            this->sfxListRect,
            static_cast<int>(this->importedSfx.size()),
            &this->sfxListScrollOffset,
            &this->sfxListScrollDragActive,
            &this->sfxListScrollDragGrabOffsetY);

        if (!this->vfxFpsInputFocused &&
            this->selectedVfxInstanceIndex >= 0 &&
            this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
        {
            const bool upPressed = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_UP);
            const bool downPressed = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_DOWN);
            const bool leftPressed = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LEFT);
            const bool rightPressed = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RIGHT);

            float deltaX = 0.0f;
            float deltaY = 0.0f;
            if (leftPressed)
            {
                deltaX -= 1.0f;
            }
            if (rightPressed)
            {
                deltaX += 1.0f;
            }
            if (upPressed)
            {
                deltaY -= 1.0f;
            }
            if (downPressed)
            {
                deltaY += 1.0f;
            }

            if (deltaX != 0.0f || deltaY != 0.0f)
            {
                if (deltaX != 0.0f && deltaY != 0.0f)
                {
                    constexpr float kDiagonalFactor = 0.70710678f;
                    deltaX *= kDiagonalFactor;
                    deltaY *= kDiagonalFactor;
                }

                float moveSpeed = 220.0f;
                if (rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LSHIFT) ||
                    rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RSHIFT))
                {
                    moveSpeed *= 2.0f;
                }
                else if (rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LCTRL) ||
                    rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RCTRL))
                {
                    moveSpeed *= 0.5f;
                }

                const float distance = moveSpeed * static_cast<float>(dt);
                this->moveSelectedVfxInstance(deltaX * distance, deltaY * distance);
            }
        }
    }
    else
    {
        this->handleListPanelScrollDragFromMouse(
            this->looseListRect,
            static_cast<int>(this->importedLooseFolders.size()),
            &this->looseListScrollOffset,
            &this->looseListScrollDragActive,
            &this->looseListScrollDragGrabOffsetY);

        this->scrollBarOverlay.update(dt, camera, map, map.rect);
        GameplayCameraController::updateKeyboardScroll(dt, camera, map, map.rect);

        if (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET &&
            std::fabs(camera.getZoomFactor() - kLoosePreviewZoomDefault) > 0.0001f)
        {
            camera.setZoomFactor(kLoosePreviewZoomDefault);
        }
    }

    GetOceanShader().update(dt);
    if (this->editorMode == EditorMode::LOOSE_SPRITES &&
        this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
    {
        this->loosePreviewZoomFactor = std::clamp(
            camera.getZoomFactor(),
            kLoosePreviewZoomMin,
            kLoosePreviewZoomMax);
    }
    else
    {
        this->loosePreviewZoomFactor = std::clamp(
            this->loosePreviewZoomFactor,
            kLoosePreviewZoomMin,
            kLoosePreviewZoomMax);
    }
    camera.update(map, map.rect);
}

void EditorMapVfxScene::draw(void)
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

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        this->drawShipVfxPreview();
    }
    else
    {
        if (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->drawLoosePlacementPreview();
        }
        else
        {
            this->drawLooseSpritesPreview();
        }
        this->scrollBarOverlay.draw(map.rect, map);
    }

    WorldRenderClip::end(renderer);
    this->drawHud();
}

void EditorMapVfxScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)keycode;
    (void)keyboardID;

    if (this->handleLooseExportNameInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleVfxFpsInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLoosePreviewFpsInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->vfxFpsInputFocused = false;
        this->loosePreviewFpsInputFocused = false;
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_TAB)
    {
        this->editorMode = (this->editorMode == EditorMode::SHIP_VFX)
            ? EditorMode::LOOSE_SPRITES
            : EditorMode::SHIP_VFX;
        this->vfxFpsInputFocused = false;
        this->loosePreviewFpsInputFocused = false;
        this->statusMessage = (this->editorMode == EditorMode::SHIP_VFX)
            ? "Mode Ship / VFX actif."
            : "Mode Downscale Sprites VFX actif.";
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_O)
    {
        const bool reverse = ((mod & SDL_KMOD_SHIFT) != 0);
        this->requestOceanColorStep(reverse ? -1 : 1);
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_E)
    {
        if (this->editorMode == EditorMode::LOOSE_SPRITES)
        {
            this->openLooseExportNamePopup();
        }
        else
        {
            this->openExportFolderDialog();
        }
        return;
    }

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        if (!isrepeat && scancode == SDL_SCANCODE_F5)
        {
            this->openImportShipFolderDialog();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_F6)
        {
            this->openImportSfxFolderDialog();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_H)
        {
            this->toggleSelectedVfxFlipHorizontal();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_V)
        {
            this->toggleSelectedVfxFlipVertical();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_F)
        {
            this->toggleSelectedVfxFollowShip();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_C)
        {
            this->centerSelectedVfxInstance();
            return;
        }
        if (!isrepeat && (scancode == SDL_SCANCODE_DELETE || scancode == SDL_SCANCODE_BACKSPACE))
        {
            this->removeSelectedVfxInstance();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_LEFTBRACKET)
        {
            if ((mod & SDL_KMOD_SHIFT) != 0)
            {
                this->adjustShipDrawOrder(-1);
            }
            else
            {
                this->adjustSelectedVfxDrawOrder(-1);
            }
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_RIGHTBRACKET)
        {
            if ((mod & SDL_KMOD_SHIFT) != 0)
            {
                this->adjustShipDrawOrder(1);
            }
            else
            {
                this->adjustSelectedVfxDrawOrder(1);
            }
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_COMMA)
        {
            this->adjustSelectedVfxRotation(-15.0f);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_PERIOD)
        {
            this->adjustSelectedVfxRotation(15.0f);
            return;
        }

    }
    else
    {
        if (!isrepeat && scancode == SDL_SCANCODE_F7)
        {
            this->openImportLooseFolderDialog();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_P)
        {
            this->loosePreviewMode = (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
                ? LoosePreviewMode::PLACEMENT_PREVIEW
                : LoosePreviewMode::CENTER_SPRITESHEET;
            if (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
            {
                Camera& camera = GetCamera();
                Map& map = GetCurrentMap();
                camera.setZoomFactor(kLoosePreviewZoomDefault);
                camera.update(map, map.rect);
                this->looseReferencePreviewVisible = false;
                this->statusMessage = "Mode spritesheet centree actif (references OFF).";
            }
            else
            {
                Camera& camera = GetCamera();
                Map& map = GetCurrentMap();
                const float restoredZoom = std::clamp(
                    this->loosePreviewZoomFactor,
                    kLoosePreviewZoomMin,
                    kLoosePreviewZoomMax);
                camera.setZoomFactor(restoredZoom);
                camera.update(map, map.rect);
                this->looseReferencePreviewVisible = this->looseReferencePreviewLoaded;
                this->statusMessage = "Mode preview clic VFX actif (references ON).";
            }
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_PAGEUP)
        {
            this->adjustLoosePreviewZoom(0.05f);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_PAGEDOWN)
        {
            this->adjustLoosePreviewZoom(-0.05f);
            return;
        }
        if (!isrepeat &&
            (scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS))
        {
            if ((mod & SDL_KMOD_CTRL) != 0)
            {
                this->looseScalePercent = std::clamp(
                    this->looseScalePercent - kLooseScaleStepPercent,
                    kLooseScaleMinPercent,
                    kLooseScaleMaxPercent);
            }
            else
            {
                this->adjustLoosePreviewZoom(-0.05f);
            }
            return;
        }
        if (!isrepeat &&
            (scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS))
        {
            if ((mod & SDL_KMOD_CTRL) != 0)
            {
                this->looseScalePercent = std::clamp(
                    this->looseScalePercent + kLooseScaleStepPercent,
                    kLooseScaleMinPercent,
                    kLooseScaleMaxPercent);
            }
            else
            {
                this->adjustLoosePreviewZoom(0.05f);
            }
            return;
        }
    }
}

void EditorMapVfxScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;
    Map& map = GetCurrentMap();

    if (this->looseExportNamePopupVisible)
    {
        (void)x;
        (void)y;
        (void)button;
        return;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        const bool wasVfxFpsInputFocused = this->vfxFpsInputFocused;
        const bool wasLoosePreviewFpsInputFocused = this->loosePreviewFpsInputFocused;
        const bool clickInVfxFpsInput =
            (this->editorMode == EditorMode::SHIP_VFX) &&
            this->pointInRect(x, y, this->buttonVfxFpsInputRect);
        const bool clickInLoosePreviewFpsInput =
            (this->editorMode == EditorMode::LOOSE_SPRITES) &&
            (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW) &&
            this->pointInRect(x, y, this->buttonLoosePreviewFpsInputRect);

        if (wasVfxFpsInputFocused && !clickInVfxFpsInput)
        {
            this->vfxFpsInputFocused = false;
            this->applyVfxFpsInput();
        }
        if (wasLoosePreviewFpsInputFocused && !clickInLoosePreviewFpsInput)
        {
            this->loosePreviewFpsInputFocused = false;
            this->applyLoosePreviewFpsInput();
        }
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        if (this->handleToolbarClick(x, y))
        {
            return;
        }

        if (this->editorMode == EditorMode::SHIP_VFX)
        {
            if (this->handleShipListClick(x, y))
            {
                return;
            }
            if (this->handleSfxListClick(x, y))
            {
                return;
            }
        }
        else
        {
            if (this->handleLooseListClick(x, y))
            {
                return;
            }
        }
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT &&
        this->pointInRect(x, y, map.rect) &&
        this->editorMode == EditorMode::LOOSE_SPRITES &&
        this->scrollBarOverlay.handleClick(x, y, map.rect))
    {
        return;
    }

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        this->handlePreviewClick(x, y, button);
        return;
    }

    this->handleLoosePlacementPreviewClick(x, y, button);
}

#endif // GAME_ENV_DEV
