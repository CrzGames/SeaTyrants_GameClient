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
constexpr int kInvalidVisibleRows = 3;
constexpr float kInvalidPanelPadding = 8.0f;
constexpr float kInvalidPanelHeaderHeight = 22.0f;
constexpr float kInvalidPanelRowGap = 6.0f;
constexpr float kSfxFpsMin = 1.0f;
constexpr float kSfxFpsMax = 9999.0f;
constexpr int kLooseScaleMinPercent = 5;
constexpr int kLooseScaleMaxPercent = 100;
constexpr int kLooseScaleStepPercent = 5;
constexpr float kVfxMoveStepPx = 1.0f;
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
constexpr RC2D_Color kLooseSpritesheetGridColor = RC2D_Color{255, 230, 120, 200};

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

static std::string makePathKeyLower(const std::string& path)
{
    std::string key = normalizePathSlashes(path);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return key;
}

static std::string stripListPrefix(const std::string& rawName, const char* expectedPrefix)
{
    std::string label = trimAscii(extractFileName(rawName));
    if (label.empty())
    {
        return rawName;
    }
    if (expectedPrefix == nullptr || expectedPrefix[0] == '\0')
    {
        return label;
    }

    std::string lower = makePathKeyLower(label);
    std::string prefixLower = makePathKeyLower(expectedPrefix);
    if (lower.rfind(prefixLower, 0U) == 0U)
    {
        label.erase(0, prefixLower.size());
        while (!label.empty() && (label[0] == '-' || label[0] == '_' || std::isspace(static_cast<unsigned char>(label[0]))))
        {
            label.erase(label.begin());
        }
    }

    label = trimAscii(label);
    if (label.empty())
    {
        return rawName;
    }
    return label;
}

static std::string makeDefaultVfxLayerBaseName(const std::string& sourceDisplayName)
{
    std::string folderName = trimAscii(extractFileName(sourceDisplayName));
    if (folderName.empty())
    {
        folderName = "vfx";
    }

    std::string baseName = folderName;
    const std::string lower = makePathKeyLower(folderName);
    if (lower.rfind("vfx-", 0U) == 0U)
    {
        const std::string suffix = folderName.substr(4);
        const size_t dashPos = suffix.find('-');
        if (dashPos != std::string::npos)
        {
            baseName = suffix.substr(0, dashPos);
        }
        else if (!suffix.empty())
        {
            baseName = suffix;
        }
    }

    baseName = trimAscii(baseName);
    if (baseName.empty())
    {
        baseName = "vfx";
    }

    return baseName;
}

static std::string makeDefaultVfxLayerLabel(const std::string& sourceDisplayName, int sourceNumber, int sourceInstanceNumber)
{
    const std::string baseName = makeDefaultVfxLayerBaseName(sourceDisplayName);
    const int clampedSourceNumber = (std::max)(1, sourceNumber);
    const int clampedInstanceNumber = (std::max)(1, sourceInstanceNumber);
    return baseName + " #" + std::to_string(clampedSourceNumber) + " (" + std::to_string(clampedInstanceNumber) + ")";
}

static std::string makeShipConfigSlug(const std::string& rawName)
{
    std::string slug = makeExportAnimationSlug(rawName);
    if (slug.empty())
    {
        slug = "ship";
    }
    return slug;
}

static const char* kDirectionIdDownLeft = "down_left";
static const char* kDirectionIdUpRight = "up_right";
static const char* kDirectionIdUpLeft = "up_left";
static const char* kDirectionIdDownRight = "down_right";

static const char* getDirectionIdByIndex(int index)
{
    switch (index)
    {
    case 0:
        return kDirectionIdDownLeft;
    case 1:
        return kDirectionIdUpRight;
    case 2:
        return kDirectionIdUpLeft;
    case 3:
        return kDirectionIdDownRight;
    default:
        return kDirectionIdDownLeft;
    }
}

static int getDirectionIndexById(const char* directionId)
{
    if (directionId == nullptr)
    {
        return 0;
    }
    if (SDL_strcasecmp(directionId, kDirectionIdDownLeft) == 0)
    {
        return 0;
    }
    if (SDL_strcasecmp(directionId, kDirectionIdUpRight) == 0)
    {
        return 1;
    }
    if (SDL_strcasecmp(directionId, kDirectionIdUpLeft) == 0)
    {
        return 2;
    }
    if (SDL_strcasecmp(directionId, kDirectionIdDownRight) == 0)
    {
        return 3;
    }
    return 0;
}

struct CustomSpritesheetFrame
{
    int index;
    float x;
    float y;
    float w;
    float h;
};

struct CustomSpritesheetParseResult
{
    std::string imageFileName;
    float fps;
    std::vector<CustomSpritesheetFrame> frames;
};

static bool tryParseCustomSpritesheetJson(
    const char* jsonText,
    const std::vector<std::string>& pngFileNames,
    CustomSpritesheetParseResult* outResult,
    std::string* outError)
{
    if (outResult == nullptr)
    {
        return false;
    }
    if (jsonText == nullptr)
    {
        if (outError != nullptr)
        {
            *outError = "JSON null.";
        }
        return false;
    }

    cJSON* root = cJSON_Parse(jsonText);
    if (root == nullptr)
    {
        if (outError != nullptr)
        {
            *outError = "JSON invalide.";
        }
        return false;
    }

    cJSON* framesArray = cJSON_GetObjectItemCaseSensitive(root, "frames");
    if (!cJSON_IsArray(framesArray))
    {
        cJSON_Delete(root);
        if (outError != nullptr)
        {
            *outError = "Pas un JSON spritesheet custom (frames[] absent).";
        }
        return false;
    }

    float fps = 12.0f;
    const cJSON* fpsNode = cJSON_GetObjectItemCaseSensitive(root, "fps");
    if (cJSON_IsNumber(fpsNode) && std::isfinite(fpsNode->valuedouble))
    {
        fps = std::clamp(static_cast<float>(fpsNode->valuedouble), kSfxFpsMin, kSfxFpsMax);
    }

    std::string imageFileName;
    const cJSON* imageNode = cJSON_GetObjectItemCaseSensitive(root, "image");
    if (cJSON_IsString(imageNode) && imageNode->valuestring != nullptr)
    {
        imageFileName = extractFileName(imageNode->valuestring);
    }
    if (imageFileName.empty())
    {
        const cJSON* metaNode = cJSON_GetObjectItemCaseSensitive(root, "meta");
        const cJSON* metaImageNode =
            (metaNode != nullptr) ? cJSON_GetObjectItemCaseSensitive(metaNode, "image") : nullptr;
        if (cJSON_IsString(metaImageNode) && metaImageNode->valuestring != nullptr)
        {
            imageFileName = extractFileName(metaImageNode->valuestring);
        }
    }

    if (imageFileName.empty())
    {
        std::vector<std::string> sortedPng = pngFileNames;
        std::sort(sortedPng.begin(), sortedPng.end());
        if (!sortedPng.empty())
        {
            imageFileName = sortedPng[0];
        }
    }

    if (imageFileName.empty())
    {
        cJSON_Delete(root);
        if (outError != nullptr)
        {
            *outError = "Aucun PNG detecte pour le spritesheet custom.";
        }
        return false;
    }

    std::vector<CustomSpritesheetFrame> parsedFrames;
    parsedFrames.reserve(static_cast<size_t>(cJSON_GetArraySize(framesArray)));
    int generatedIndex = 0;
    cJSON* frameNode = nullptr;
    cJSON_ArrayForEach(frameNode, framesArray)
    {
        if (!cJSON_IsObject(frameNode))
        {
            continue;
        }

        const cJSON* xNode = cJSON_GetObjectItemCaseSensitive(frameNode, "x");
        const cJSON* yNode = cJSON_GetObjectItemCaseSensitive(frameNode, "y");
        const cJSON* wNode = cJSON_GetObjectItemCaseSensitive(frameNode, "w");
        const cJSON* hNode = cJSON_GetObjectItemCaseSensitive(frameNode, "h");
        const cJSON* indexNode = cJSON_GetObjectItemCaseSensitive(frameNode, "index");
        if (!cJSON_IsNumber(xNode) || !cJSON_IsNumber(yNode) || !cJSON_IsNumber(wNode) || !cJSON_IsNumber(hNode))
        {
            continue;
        }

        const float x = static_cast<float>(xNode->valuedouble);
        const float y = static_cast<float>(yNode->valuedouble);
        const float w = static_cast<float>(wNode->valuedouble);
        const float h = static_cast<float>(hNode->valuedouble);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) || w <= 0.0f || h <= 0.0f)
        {
            continue;
        }

        int index = generatedIndex;
        if (cJSON_IsNumber(indexNode) && std::isfinite(indexNode->valuedouble))
        {
            index = static_cast<int>(std::llround(indexNode->valuedouble));
        }
        generatedIndex += 1;

        parsedFrames.push_back(CustomSpritesheetFrame{index, x, y, w, h});
    }

    if (parsedFrames.empty())
    {
        cJSON_Delete(root);
        if (outError != nullptr)
        {
            *outError = "frames[] vide ou invalide.";
        }
        return false;
    }

    std::sort(parsedFrames.begin(), parsedFrames.end(), [](const CustomSpritesheetFrame& a, const CustomSpritesheetFrame& b) {
        if (a.index != b.index)
        {
            return a.index < b.index;
        }
        if (a.y != b.y)
        {
            return a.y < b.y;
        }
        return a.x < b.x;
    });

    cJSON_Delete(root);
    outResult->imageFileName = imageFileName;
    outResult->fps = fps;
    outResult->frames = std::move(parsedFrames);
    return true;
}

static void unionOpaquePixelBounds(SDL_Surface* surface, int* minX, int* minY, int* maxX, int* maxY, bool* anyOpaque)
{
    if (surface == nullptr || minX == nullptr || minY == nullptr || maxX == nullptr || maxY == nullptr || anyOpaque == nullptr)
    {
        return;
    }

    const int w = surface->w;
    const int h = surface->h;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 0;
            SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);
            if (a == 0)
            {
                continue;
            }
            *anyOpaque = true;
            *minX = (std::min)(*minX, x);
            *minY = (std::min)(*minY, y);
            *maxX = (std::max)(*maxX, x);
            *maxY = (std::max)(*maxY, y);
        }
    }
}

static SDL_Surface* createCroppedSurfaceFromSource(
    SDL_Surface* source,
    int cropX,
    int cropY,
    int cropW,
    int cropH)
{
    if (source == nullptr || cropW <= 0 || cropH <= 0)
    {
        return nullptr;
    }

    SDL_Surface* out = SDL_CreateSurface(cropW, cropH, SDL_PIXELFORMAT_RGBA32);
    if (out == nullptr)
    {
        return nullptr;
    }

    const int srcW = source->w;
    const int srcH = source->h;
    for (int cy = 0; cy < cropH; ++cy)
    {
        for (int cx = 0; cx < cropW; ++cx)
        {
            const int sx = cropX + cx;
            const int sy = cropY + cy;
            if (sx < 0 || sy < 0 || sx >= srcW || sy >= srcH)
            {
                SDL_WriteSurfacePixel(out, cx, cy, 0, 0, 0, 0);
            }
            else
            {
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 0;
                SDL_ReadSurfacePixel(source, sx, sy, &r, &g, &b, &a);
                SDL_WriteSurfacePixel(out, cx, cy, r, g, b, a);
            }
        }
    }

    return out;
}

static void computeOpaqueUnionCropRectFromSurfaces(
    const std::vector<SDL_Surface*>& surfaces,
    int* outCropX,
    int* outCropY,
    int* outCropW,
    int* outCropH)
{
    if (outCropX == nullptr || outCropY == nullptr || outCropW == nullptr || outCropH == nullptr)
    {
        return;
    }
    *outCropX = 0;
    *outCropY = 0;
    *outCropW = 1;
    *outCropH = 1;
    if (surfaces.empty())
    {
        return;
    }

    int maxSourceW = 1;
    int maxSourceH = 1;
    for (SDL_Surface* surface : surfaces)
    {
        if (surface != nullptr)
        {
            maxSourceW = (std::max)(maxSourceW, surface->w);
            maxSourceH = (std::max)(maxSourceH, surface->h);
        }
    }

    int unionMinX = maxSourceW;
    int unionMinY = maxSourceH;
    int unionMaxX = -1;
    int unionMaxY = -1;
    bool anyOpaquePixel = false;
    for (SDL_Surface* surface : surfaces)
    {
        unionOpaquePixelBounds(surface, &unionMinX, &unionMinY, &unionMaxX, &unionMaxY, &anyOpaquePixel);
    }

    if (anyOpaquePixel && unionMaxX >= unionMinX && unionMaxY >= unionMinY)
    {
        *outCropX = unionMinX;
        *outCropY = unionMinY;
        *outCropW = unionMaxX - unionMinX + 1;
        *outCropH = unionMaxY - unionMinY + 1;
    }
    else
    {
        *outCropX = 0;
        *outCropY = 0;
        *outCropW = maxSourceW;
        *outCropH = maxSourceH;
    }
}

static void destroySurfaceVector(std::vector<SDL_Surface*>& surfaces)
{
    for (SDL_Surface* s : surfaces)
    {
        if (s != nullptr)
        {
            SDL_DestroySurface(s);
        }
    }
    surfaces.clear();
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
      looseScalePercent(100),
      loosePreviewZoomFactor(kLoosePreviewZoomDefault),
      loosePreviewMode(LoosePreviewMode::CENTER_SPRITESHEET),
      loosePreviewPlacementSnapToTile(true),
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
      buttonLoosePreviewPlacementSnapRect{},
      shipListRect{},
      sfxListRect{},
      looseListRect{},
      invalidVfxListRect{},
      invalidShipListRect{},
      layerRowDragActive(false),
      layerRowDragMoved(false),
      layerRowDragSourceDisplayIndex(-1),
      layerRowDragTargetInsertIndex(-1),
      layerRowDragStartMouseY(0.0f),
      invalidVfxListScrollOffset(0),
      invalidShipListScrollOffset(0),
      invalidVfxListScrollDragActive(false),
      invalidShipListScrollDragActive(false),
      invalidVfxListScrollDragGrabOffsetY(0.0f),
      invalidShipListScrollDragGrabOffsetY(0.0f)
{
    this->previewDirectionIndex = 0;
    this->previewShipStateIndex = 0;
    this->shipLayerVisible = true;
    this->shipLayerLocked = false;
    this->shipLayerSelected = false;
    this->shipDebugBoundsVisible = true;
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->vfxDragStartMouseX = 0.0f;
    this->vfxDragStartMouseY = 0.0f;
    this->vfxDragStartOffsetX = 0.0f;
    this->vfxDragStartOffsetY = 0.0f;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath.clear();
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();
    this->buttonReloadAssetsRect = SDL_FRect{};
    this->buttonDirectionPrevRect = SDL_FRect{};
    this->buttonDirectionNextRect = SDL_FRect{};
    this->buttonShipStateToggleRect = SDL_FRect{};
    this->buttonLayerOrderMinusRect = SDL_FRect{};
    this->buttonLayerOrderPlusRect = SDL_FRect{};
    this->buttonVisibleRect = SDL_FRect{};
    this->buttonLockedRect = SDL_FRect{};
    this->buttonBehindShipRect = SDL_FRect{};
    this->buttonDuplicateVfxRect = SDL_FRect{};
    this->buttonLayerNameInputRect = SDL_FRect{};
    this->buttonSharedDirectionsRect = SDL_FRect{};
    this->buttonDirectionOverrideRect = SDL_FRect{};
    this->buttonResetTransformRect = SDL_FRect{};
    this->layerListRect = SDL_FRect{};
    this->layerListScrollOffset = 0;
    this->layerListScrollDragActive = false;
    this->layerListScrollDragGrabOffsetY = 0.0f;
    this->layerRowDragActive = false;
    this->layerRowDragMoved = false;
    this->layerRowDragSourceDisplayIndex = -1;
    this->layerRowDragTargetInsertIndex = -1;
    this->layerRowDragStartMouseY = 0.0f;
    this->invalidVfxListRect = SDL_FRect{};
    this->invalidShipListRect = SDL_FRect{};
    this->invalidVfxListScrollOffset = 0;
    this->invalidShipListScrollOffset = 0;
    this->invalidVfxListScrollDragActive = false;
    this->invalidShipListScrollDragActive = false;
    this->invalidVfxListScrollDragGrabOffsetY = 0.0f;
    this->invalidShipListScrollDragGrabOffsetY = 0.0f;
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
    this->previewDirectionIndex = 0;
    this->previewShipStateIndex = 0;
    this->shipLayerVisible = true;
    this->shipLayerLocked = false;
    this->shipLayerSelected = false;
    this->shipDebugBoundsVisible = true;
    this->previewShipTile = SDL_FPoint{0.0f, 0.0f};
    this->shipDrawOrder = 0;
    this->shipVfxInstances.clear();
    this->selectedVfxInstanceIndex = -1;
    this->nextVfxInstanceId = 1U;
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->vfxDragStartMouseX = 0.0f;
    this->vfxDragStartMouseY = 0.0f;
    this->vfxDragStartOffsetX = 0.0f;
    this->vfxDragStartOffsetY = 0.0f;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath.clear();
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();
    this->layerListScrollOffset = 0;
    this->layerListScrollDragActive = false;
    this->layerListScrollDragGrabOffsetY = 0.0f;
    this->layerRowDragActive = false;
    this->layerRowDragMoved = false;
    this->layerRowDragSourceDisplayIndex = -1;
    this->layerRowDragTargetInsertIndex = -1;
    this->layerRowDragStartMouseY = 0.0f;
    this->invalidVfxListScrollOffset = 0;
    this->invalidShipListScrollOffset = 0;
    this->invalidVfxListScrollDragActive = false;
    this->invalidShipListScrollDragActive = false;
    this->invalidVfxListScrollDragGrabOffsetY = 0.0f;
    this->invalidShipListScrollDragGrabOffsetY = 0.0f;
    this->looseScalePercent = 100;
    this->loosePreviewZoomFactor = kLoosePreviewZoomDefault;
    this->loosePreviewMode = LoosePreviewMode::CENTER_SPRITESHEET;
    this->loosePreviewPlacementSnapToTile = true;
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

void EditorMapVfxScene::clearEditorTransientInteractionState(void)
{
    this->layerNameInputFocused = false;
    this->loosePreviewFpsInputFocused = false;
    this->vfxDragActive = false;
    this->layerRowDragActive = false;
    this->layerRowDragMoved = false;
    this->layerRowDragSourceDisplayIndex = -1;
    this->layerRowDragTargetInsertIndex = -1;
    this->layerRowDragStartMouseY = 0.0f;
    this->shipListScrollDragActive = false;
    this->sfxListScrollDragActive = false;
    this->looseListScrollDragActive = false;
    this->layerListScrollDragActive = false;
    this->invalidVfxListScrollDragActive = false;
    this->invalidShipListScrollDragActive = false;
}

void EditorMapVfxScene::applyShipVfxModeViewportReset(void)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    map.update();
    camera.setZoomFactor(kLoosePreviewZoomDefault);
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    const float shipScreenX = map.rect.x + (map.rect.w * 0.5f) + 250.0f;
    const float shipScreenY = map.rect.y + (map.rect.h * 0.5f);
    const SDL_Point shiftedShipTile = map.screenToTileNearest(shipScreenX, shipScreenY);
    this->previewShipTile = SDL_FPoint{
        static_cast<float>(shiftedShipTile.x),
        static_cast<float>(shiftedShipTile.y)};

    if (this->previewShipLoaded)
    {
        this->previewShip.setPositionTile(this->previewShipTile.x, this->previewShipTile.y);
    }
}

void EditorMapVfxScene::reloadPreviewShipIfUnloadedKeepVfxLayers(void)
{
    if (this->previewShipLoaded)
    {
        return;
    }
    if (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
    {
        const ImportedShip& ship = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
        this->loadShipFolderFromAbsolutePath(ship.folderAbsolutePath.c_str());
        return;
    }
    if (!this->importedShips.empty())
    {
        this->selectImportedShipAtIndex(0);
    }
}

void EditorMapVfxScene::applyLooseSpritesModeEntryReset(void)
{
    this->loosePreviewMode = LoosePreviewMode::CENTER_SPRITESHEET;
    this->loosePreviewPlacements.clear();
    this->nextLoosePreviewPlacementId = 1U;
    this->loosePreviewPlacementSnapToTile = true;
    this->looseReferencePreviewVisible = false;
    this->looseScalePercent = 100;
    this->loosePreviewZoomFactor = kLoosePreviewZoomDefault;
    this->looseListScrollOffset = 0;
    this->looseExportNamePopupVisible = false;
    this->looseExportNameInput.clear();
    this->pendingLooseExportAnimationName.clear();

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    map.update();
    camera.setZoomFactor(kLoosePreviewZoomDefault);
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);
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
        rc2d_graphics_freeImage(&sfx.image);
        sfx.frames.clear();
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

void EditorMapVfxScene::autoImportAssetsFromDefaultFolders(void)
{
    this->previewShip.unloadSprites();
    this->previewShipLoaded = false;
    this->loadedShipFolderAbsolute.clear();
    this->shipVfxInstances.clear();
    this->setSelectedVfxInstanceIndex(-1);
    this->nextVfxInstanceId = 1U;
    this->shipLayerVisible = true;
    this->shipLayerLocked = false;
    this->shipDebugBoundsVisible = true;
    this->shipDrawOrder = 0;
    this->importedShips.clear();
    this->unloadImportedSfx();
    this->selectedShipIndex = -1;
    this->selectedSfxIndex = -1;
    this->shipListScrollOffset = 0;
    this->sfxListScrollOffset = 0;
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();
    this->importShipsFromRootFolderAbsolutePath("assets/images/ships");
    this->importSfxFromRootFolderAbsolutePath("assets/images/vfx");
}

std::string EditorMapVfxScene::buildShipConfigJsonPath(const ImportedShip& ship) const
{
    std::filesystem::path base(ship.folderAbsolutePath);
    const std::string slug = makeShipConfigSlug(ship.displayName);
    const std::string fileName = "animations_vfx_" + slug + ".json";
    return normalizePathSlashes((base / fileName).string());
}

void EditorMapVfxScene::applyPreviewDirectionToShip(void)
{
    switch (this->previewDirectionIndex)
    {
    case 0:
        this->previewShip.setPreviewDirection(Ship::PreviewDirection::DOWN_LEFT);
        break;
    case 1:
        this->previewShip.setPreviewDirection(Ship::PreviewDirection::UP_RIGHT);
        break;
    case 2:
        this->previewShip.setPreviewDirection(Ship::PreviewDirection::UP_LEFT);
        break;
    case 3:
        this->previewShip.setPreviewDirection(Ship::PreviewDirection::DOWN_RIGHT);
        break;
    default:
        this->previewDirectionIndex = 0;
        this->previewShip.setPreviewDirection(Ship::PreviewDirection::DOWN_LEFT);
        break;
    }
}

void EditorMapVfxScene::setPreviewDirectionIndex(int directionIndex)
{
    this->previewDirectionIndex = std::clamp(directionIndex, 0, 3);
    this->applyPreviewDirectionToShip();
}

void EditorMapVfxScene::cyclePreviewDirection(int delta)
{
    const int count = 4;
    int index = this->previewDirectionIndex + delta;
    while (index < 0)
    {
        index += count;
    }
    while (index >= count)
    {
        index -= count;
    }
    this->setPreviewDirectionIndex(index);
}

void EditorMapVfxScene::cyclePreviewShipState(int delta)
{
    const int count = 2;
    int index = this->previewShipStateIndex + delta;
    while (index < 0)
    {
        index += count;
    }
    while (index >= count)
    {
        index -= count;
    }
    this->previewShipStateIndex = index;
    this->previewShip.setHealthVisual((index == 1) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
}

const char* EditorMapVfxScene::getPreviewDirectionLabel(void) const
{
    switch (this->previewDirectionIndex)
    {
    case 0:
        return "Bas Gauche (1/5)";
    case 1:
        return "Haut Droite (2/6)";
    case 2:
        return "Haut Gauche (3/7)";
    case 3:
        return "Bas Droite (4/8)";
    default:
        return "Bas Gauche (1/5)";
    }
}

const char* EditorMapVfxScene::getPreviewShipStateLabel(void) const
{
    return (this->previewShipStateIndex == 1)
        ? "SPRITES NAVIRE - LOW HP (5-8)"
        : "SPRITES NAVIRE - FULL HP (1-4)";
}

void EditorMapVfxScene::markShipVfxDirty(void)
{
    this->shipVfxDirty = true;
}

int EditorMapVfxScene::getActiveDirectionIndexForOverrides(void) const
{
    return std::clamp(this->previewDirectionIndex, 0, 3);
}

bool EditorMapVfxScene::hasSelectedVfxInstance(void) const
{
    return this->selectedVfxInstanceIndex >= 0 &&
        this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size());
}

EditorMapVfxScene::ShipVfxInstance* EditorMapVfxScene::getSelectedVfxInstance(void)
{
    if (!this->hasSelectedVfxInstance())
    {
        return nullptr;
    }
    return &this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
}

const EditorMapVfxScene::ShipVfxInstance* EditorMapVfxScene::getSelectedVfxInstance(void) const
{
    if (!this->hasSelectedVfxInstance())
    {
        return nullptr;
    }
    return &this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
}

EditorMapVfxScene::DirectionOverride* EditorMapVfxScene::getEditableDirectionOverride(EditorMapVfxScene::ShipVfxInstance* instance)
{
    if (instance == nullptr)
    {
        return nullptr;
    }
    if (instance->sharedForAllDirections)
    {
        return nullptr;
    }

    const int directionIndex = this->getActiveDirectionIndexForOverrides();
    DirectionOverride& override = instance->directionOverrides[static_cast<size_t>(directionIndex)];
    if (!override.enabled)
    {
        return nullptr;
    }
    return &override;
}

const EditorMapVfxScene::DirectionOverride* EditorMapVfxScene::getResolvedDirectionOverride(const EditorMapVfxScene::ShipVfxInstance* instance) const
{
    if (instance == nullptr)
    {
        return nullptr;
    }
    if (instance->sharedForAllDirections)
    {
        return nullptr;
    }
    const int directionIndex = this->getActiveDirectionIndexForOverrides();
    const DirectionOverride& override = instance->directionOverrides[static_cast<size_t>(directionIndex)];
    return override.enabled ? &override : nullptr;
}

void EditorMapVfxScene::resetSelectedVfxTransform(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }

    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->offsetX = 0.0f;
        override->offsetY = 0.0f;
        override->rotationDeg = 0.0f;
        override->flipHorizontal = false;
        override->flipVertical = false;
        override->drawOrder = instance->drawOrder;
        override->visible = true;
    }
    else
    {
        instance->offsetX = 0.0f;
        instance->offsetY = 0.0f;
        instance->rotationDeg = 0.0f;
        instance->flipHorizontal = false;
        instance->flipVertical = false;
        instance->visible = true;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxVisibility(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->visible = !override->visible;
    }
    else
    {
        instance->visible = !instance->visible;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxLock(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    instance->locked = !instance->locked;
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxBehindShip(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    instance->behindShip = !instance->behindShip;
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    int* drawOrder = (override != nullptr) ? &override->drawOrder : &instance->drawOrder;
    if (instance->behindShip && *drawOrder >= this->shipDrawOrder)
    {
        *drawOrder = this->shipDrawOrder - 1;
    }
    if (!instance->behindShip && *drawOrder <= this->shipDrawOrder)
    {
        *drawOrder = this->shipDrawOrder + 1;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxSharedForAllDirections(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }

    if (instance->sharedForAllDirections)
    {
        instance->sharedForAllDirections = false;
        for (DirectionOverride& override : instance->directionOverrides)
        {
            override.enabled = false;
            override.offsetX = instance->offsetX;
            override.offsetY = instance->offsetY;
            override.rotationDeg = instance->rotationDeg;
            override.flipHorizontal = instance->flipHorizontal;
            override.flipVertical = instance->flipVertical;
            override.drawOrder = instance->drawOrder;
            override.visible = instance->visible;
        }
    }
    else
    {
        const int directionIndex = this->getActiveDirectionIndexForOverrides();
        const DirectionOverride& override = instance->directionOverrides[static_cast<size_t>(directionIndex)];
        if (override.enabled)
        {
            instance->offsetX = override.offsetX;
            instance->offsetY = override.offsetY;
            instance->rotationDeg = override.rotationDeg;
            instance->flipHorizontal = override.flipHorizontal;
            instance->flipVertical = override.flipVertical;
            instance->drawOrder = override.drawOrder;
            instance->visible = override.visible;
        }
        instance->sharedForAllDirections = true;
        for (DirectionOverride& resetOverride : instance->directionOverrides)
        {
            resetOverride.enabled = false;
        }
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxDirectionOverride(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->sharedForAllDirections)
    {
        this->statusMessage = "Active d'abord le mode par direction.";
        return;
    }
    const int directionIndex = this->getActiveDirectionIndexForOverrides();
    DirectionOverride& override = instance->directionOverrides[static_cast<size_t>(directionIndex)];
    if (!override.enabled)
    {
        override.enabled = true;
        override.offsetX = instance->offsetX;
        override.offsetY = instance->offsetY;
        override.rotationDeg = instance->rotationDeg;
        override.flipHorizontal = instance->flipHorizontal;
        override.flipVertical = instance->flipVertical;
        override.drawOrder = instance->drawOrder;
        override.visible = instance->visible;
    }
    else
    {
        override.enabled = false;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::duplicateSelectedVfxInstance(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    ShipVfxInstance duplicate = *instance;
    duplicate.instanceId = this->nextVfxInstanceId++;
    int sourceInstanceNumber = 1;
    for (const ShipVfxInstance& existing : this->shipVfxInstances)
    {
        if (existing.importedSfxIndex == duplicate.importedSfxIndex)
        {
            sourceInstanceNumber += 1;
        }
    }
    const int sourceNumber = (std::max)(1, duplicate.importedSfxIndex + 1);
    duplicate.label = makeDefaultVfxLayerLabel(duplicate.sourceDisplayName, sourceNumber, sourceInstanceNumber);
    duplicate.offsetX += 12.0f;
    duplicate.offsetY += 12.0f;
    this->shipVfxInstances.push_back(std::move(duplicate));
    const int duplicatedIndex = static_cast<int>(this->shipVfxInstances.size()) - 1;
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    this->setSelectedVfxInstanceIndex(duplicatedIndex);
    this->markShipVfxDirty();
    this->statusMessage = "Instance VFX dupliquee.";
}

void EditorMapVfxScene::moveSelectedLayerOrder(int delta)
{
    if (this->shipLayerSelected)
    {
        this->statusMessage = "Utilise le drag dans Layers (SHIP reste a z=0).";
        return;
    }

    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    int* drawOrder = (override != nullptr) ? &override->drawOrder : &instance->drawOrder;
    *drawOrder += delta;
    if (*drawOrder == this->shipDrawOrder)
    {
        *drawOrder += (delta >= 0) ? 1 : -1;
    }
    instance->behindShip = (*drawOrder < this->shipDrawOrder);
    this->markShipVfxDirty();
}

bool EditorMapVfxScene::applyLayerNameInput(void)
{
    if (!this->hasSelectedVfxInstance())
    {
        this->layerNameInput.clear();
        return false;
    }

    std::string trimmed = trimAscii(this->layerNameInput);
    if (trimmed.empty())
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
        int sourceInstanceNumber = 0;
        for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
        {
            const ShipVfxInstance& candidate = this->shipVfxInstances[i];
            if (candidate.importedSfxIndex == instance.importedSfxIndex)
            {
                sourceInstanceNumber += 1;
            }
            if (static_cast<int>(i) == this->selectedVfxInstanceIndex)
            {
                break;
            }
        }
        if (sourceInstanceNumber <= 0)
        {
            sourceInstanceNumber = 1;
        }
        const int sourceNumber = (std::max)(1, instance.importedSfxIndex + 1);
        trimmed = makeDefaultVfxLayerLabel(instance.sourceDisplayName, sourceNumber, sourceInstanceNumber);
    }
    this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].label = trimmed;
    this->layerNameInput = trimmed;
    this->markShipVfxDirty();
    return true;
}

void EditorMapVfxScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();
    const float startX = 12.0f;
    const float topY = map.rect.y - 24.0f;
    const float row1Y = map.rect.y + map.rect.h + 4.0f;
    const float row2Y = row1Y + 30.0f;
    const float shipRow1Y = row1Y - 26.0f;
    const float shipRow2Y = shipRow1Y + 30.0f;
    const float shipRow3Y = shipRow2Y + 30.0f;
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
    setNextButton(&this->buttonReloadAssetsRect, &x, topY, 136.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, topY, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, topY, 92.0f);

    // Ligne 1 (gauche): controles navire.
    x = startX;
    setNextButton(&this->buttonDirectionPrevRect, &x, shipRow1Y, 240.0f);
    setNextButton(&this->buttonDirectionNextRect, &x, shipRow1Y, 240.0f);
    setNextButton(&this->buttonShipStateToggleRect, &x, shipRow1Y, 320.0f);

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
    if (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
    {
        setNextButton(&this->buttonLoosePreviewPlacementSnapRect, &looseRow2X, row2Y, 230.0f);
    }
    else
    {
        this->buttonLoosePreviewPlacementSnapRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    auto setPrevButtonFromRightWithGap = [h](SDL_FRect* rect, float* rightX, float y, float w, float customGap) {
        *rightX -= w;
        rect->x = *rightX;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        *rightX -= customGap;
    };

    auto computeAdaptiveWidths = [gap](const std::vector<float>& preferred, const std::vector<float>& minimum, float availableWidth, float* outGap) {
        std::vector<float> widths = preferred;
        const size_t count = preferred.size();
        if (count == 0U)
        {
            if (outGap != nullptr)
            {
                *outGap = 0.0f;
            }
            return widths;
        }

        const float minGapFloor = 4.0f;
        float preferredButtonsTotal = 0.0f;
        float minimumButtonsTotal = 0.0f;
        for (size_t i = 0; i < count; ++i)
        {
            preferredButtonsTotal += preferred[i];
            minimumButtonsTotal += minimum[i];
        }

        float workingGap = gap;
        if (count > 1U)
        {
            const float maxGapFromMinWidth = (std::max)(0.0f, availableWidth - minimumButtonsTotal) / static_cast<float>(count - 1U);
            if (maxGapFromMinWidth < minGapFloor)
            {
                workingGap = (std::max)(0.0f, maxGapFromMinWidth);
            }
            else
            {
                workingGap = std::clamp(gap, minGapFloor, maxGapFromMinWidth);
            }
        }

        float availableButtonsWidth = availableWidth - (workingGap * static_cast<float>((count > 1U) ? (count - 1U) : 0U));
        availableButtonsWidth = (std::max)(0.0f, availableButtonsWidth);

        if (availableButtonsWidth >= preferredButtonsTotal)
        {
            widths = preferred;
        }
        else if (availableButtonsWidth <= 0.0f)
        {
            std::fill(widths.begin(), widths.end(), 0.0f);
        }
        else if (availableButtonsWidth <= minimumButtonsTotal)
        {
            const float ratio = availableButtonsWidth / (std::max)(minimumButtonsTotal, 1.0f);
            for (size_t i = 0; i < count; ++i)
            {
                widths[i] = minimum[i] * ratio;
            }
        }
        else
        {
            const float ratio = (availableButtonsWidth - minimumButtonsTotal) /
                (std::max)(preferredButtonsTotal - minimumButtonsTotal, 0.001f);
            for (size_t i = 0; i < count; ++i)
            {
                widths[i] = minimum[i] + ((preferred[i] - minimum[i]) * ratio);
            }
        }

        if (outGap != nullptr)
        {
            *outGap = workingGap;
        }
        return widths;
    };

    const float availableBottomWidth = (std::max)(0.0f, map.rect.w - (startX * 2.0f));

    // Ligne 2 (droite): outils VFX principaux.
    float row2Gap = gap;
    const std::vector<float> row2Widths = computeAdaptiveWidths(
        std::vector<float>{220.0f, 220.0f, 320.0f},
        std::vector<float>{120.0f, 120.0f, 180.0f},
        availableBottomWidth,
        &row2Gap);
    float rightX = map.rect.x + map.rect.w - startX;
    setPrevButtonFromRightWithGap(&this->buttonRotatePlusRect, &rightX, shipRow2Y, row2Widths[0], row2Gap);
    setPrevButtonFromRightWithGap(&this->buttonRotateMinusRect, &rightX, shipRow2Y, row2Widths[1], row2Gap);
    setPrevButtonFromRightWithGap(&this->buttonFlipHorizontalRect, &rightX, shipRow2Y, row2Widths[2], row2Gap);

    // Ligne 3 (droite): outils VFX secondaires.
    float row3Gap = gap;
    const std::vector<float> row3Widths = computeAdaptiveWidths(
        std::vector<float>{200.0f, 180.0f, 250.0f, 360.0f},
        std::vector<float>{130.0f, 120.0f, 170.0f, 210.0f},
        availableBottomWidth,
        &row3Gap);
    rightX = map.rect.x + map.rect.w - startX;
    setPrevButtonFromRightWithGap(&this->buttonDirectionOverrideRect, &rightX, shipRow3Y, row3Widths[0], row3Gap);
    setPrevButtonFromRightWithGap(&this->buttonSharedDirectionsRect, &rightX, shipRow3Y, row3Widths[1], row3Gap);
    setPrevButtonFromRightWithGap(&this->buttonCenterVfxRect, &rightX, shipRow3Y, row3Widths[2], row3Gap);
    setPrevButtonFromRightWithGap(&this->buttonFlipVerticalRect, &rightX, shipRow3Y, row3Widths[3], row3Gap);
    this->buttonLayerNameInputRect = SDL_FRect{};

    this->shipListRect.w = 250.0f;
    this->shipListRect.h = 276.0f;
    this->shipListRect.x = map.rect.x + map.rect.w - this->shipListRect.w - 40.0f;
    this->shipListRect.y = map.rect.y + map.rect.h - this->shipListRect.h - 40.0f;

    this->sfxListRect = this->shipListRect;
    this->sfxListRect.x = this->shipListRect.x - this->sfxListRect.w - 16.0f;
    this->sfxListRect.x = (std::max)(this->sfxListRect.x, map.rect.x + 12.0f);

    this->layerListRect = this->shipListRect;
    this->layerListRect.w = 770.0f;
    this->layerListRect.h = 360.0f;
    this->layerListRect.x = map.rect.x + 12.0f;
    this->layerListRect.y = map.rect.y + map.rect.h - this->layerListRect.h - 30.0f;

    this->looseListRect = this->shipListRect;

    // Deux panneaux invalides empiles: VFX en haut, Ships en dessous.
    constexpr float invalidPanelHeight = 126.0f;
    constexpr float invalidPanelGap = 12.0f;
    constexpr float invalidPanelLiftUp = 18.0f;
    this->invalidShipListRect = SDL_FRect{
        this->layerListRect.x,
        this->layerListRect.y - invalidPanelHeight - invalidPanelGap - invalidPanelLiftUp,
        this->layerListRect.w,
        invalidPanelHeight};
    this->invalidVfxListRect = SDL_FRect{
        this->layerListRect.x,
        this->invalidShipListRect.y - invalidPanelHeight - invalidPanelGap,
        this->layerListRect.w,
        invalidPanelHeight};
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
            const int maxChars = std::clamp(static_cast<int>((rowsWidth - 20.0f) / 6.7f), 14, 84);
            char textBuffer[256] = {};
            SDL_snprintf(textBuffer, sizeof(textBuffer), "%d. %s", itemIndex + 1, makeAssetLabel(labels[static_cast<size_t>(itemIndex)], maxChars).c_str());
            RC2D_Text rowText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), textBuffer);
            rowText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&rowText);
            int textW = 0;
            int textH = 0;
            rc2d_graphics_getTextSize(&rowText, &textW, &textH);
            const float textY = rowRect.y + (std::max)((rowRect.h - static_cast<float>(textH)) * 0.5f, 1.0f);
            rc2d_graphics_drawText(&rowText, rowRect.x + 4.0f, textY);
            rc2d_graphics_destroyText(&rowText);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapVfxScene::drawLayerListPanel(const std::vector<int>& orderedLayerIndices) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kPanelFillColor);
    rc2d_graphics_rectangle("fill", &this->layerListRect);
    rc2d_graphics_setColor(kPanelBorderColor);
    rc2d_graphics_rectangle("line", &this->layerListRect);

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->layerListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->layerListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = this->layerListRect.x + panelPadding;
    const float rowsWidth = this->layerListRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text headerText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Layers (drag souris)");
        headerText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&headerText);
        rc2d_graphics_drawText(&headerText, this->layerListRect.x + panelPadding, this->layerListRect.y + 1.0f);
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

    const int itemCount = static_cast<int>(orderedLayerIndices.size());
    if (itemCount <= 0)
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int startIndex = this->computeListStartIndex(this->layerListScrollOffset, itemCount);
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

    const int selectedIndex = this->getSelectedLayerRowIndexForDisplay(orderedLayerIndices);
    const int visibleCount = (std::max)((std::min)(kVisibleListRows, itemCount - startIndex), 0);

    for (int i = 0; i < kVisibleListRows; ++i)
    {
        const int displayIndex = startIndex + i;
        if (displayIndex >= itemCount)
        {
            break;
        }

        SDL_FRect rowRect{};
        rowRect.x = rowsLeftX;
        rowRect.y = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
        rowRect.w = rowsWidth;
        rowRect.h = rowHeight;

        const int instanceIndex = orderedLayerIndices[static_cast<size_t>(displayIndex)];
        const bool isShipRow = (instanceIndex < 0);
        int drawOrder = this->shipDrawOrder;
        bool visible = this->shipLayerVisible;
        bool debugBoundsVisible = this->shipDebugBoundsVisible;
        bool locked = this->shipLayerLocked;
        std::string label = "SHIP";

        if (!isShipRow)
        {
            const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(instanceIndex)];
            const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
            drawOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
            visible = (override != nullptr) ? override->visible : instance.visible;
            debugBoundsVisible = instance.debugBoundsVisible;
            locked = instance.locked;
            label = instance.label;
        }

        const bool isDraggedRow = this->layerRowDragActive && (displayIndex == this->layerRowDragSourceDisplayIndex);
        const bool isSelected = (displayIndex == selectedIndex);
        RC2D_Color rowFill = kRowFillColor;
        if (isDraggedRow)
        {
            rowFill = RC2D_Color{62, 94, 126, 220};
        }
        else if (isSelected)
        {
            rowFill = kRowSelectedFillColor;
        }
        if (!visible)
        {
            rowFill = RC2D_Color{
                static_cast<uint8_t>((rowFill.r + 40U) / 2U),
                static_cast<uint8_t>((rowFill.g + 40U) / 2U),
                static_cast<uint8_t>((rowFill.b + 40U) / 2U),
                rowFill.a};
        }

        rc2d_graphics_setColor(rowFill);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kRowBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);

        SDL_FRect lockRect{};
        lockRect.w = 40.0f;
        lockRect.h = rowRect.h - 4.0f;
        lockRect.x = rowRect.x + rowRect.w - lockRect.w - 3.0f;
        lockRect.y = rowRect.y + 2.0f;

        SDL_FRect visRect{};
        visRect.w = 52.0f;
        visRect.h = lockRect.h;
        visRect.x = lockRect.x - visRect.w - 4.0f;
        visRect.y = lockRect.y;

        SDL_FRect dupRect{};
        dupRect.w = 46.0f;
        dupRect.h = lockRect.h;
        dupRect.x = visRect.x - dupRect.w - 4.0f;
        dupRect.y = lockRect.y;

        SDL_FRect delRect{};
        delRect.w = 46.0f;
        delRect.h = lockRect.h;
        delRect.x = dupRect.x - delRect.w - 4.0f;
        delRect.y = lockRect.y;

        SDL_FRect resetRect{};
        resetRect.w = 58.0f;
        resetRect.h = lockRect.h;
        resetRect.x = delRect.x - resetRect.w - 4.0f;
        resetRect.y = lockRect.y;

        SDL_FRect debugRect{};
        debugRect.w = 66.0f;
        debugRect.h = lockRect.h;
        debugRect.x = resetRect.x - debugRect.w - 4.0f;
        debugRect.y = lockRect.y;

        rc2d_graphics_setColor(visible ? RC2D_Color{74, 122, 92, 220} : RC2D_Color{92, 66, 66, 220});
        rc2d_graphics_rectangle("fill", &visRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &visRect);

        const bool lockVisualState = locked;
        rc2d_graphics_setColor(lockVisualState ? RC2D_Color{116, 90, 58, 220} : RC2D_Color{56, 66, 78, 220});
        rc2d_graphics_rectangle("fill", &lockRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &lockRect);

        const bool canDuplicateOrDelete = !isShipRow;
        rc2d_graphics_setColor(
            canDuplicateOrDelete
                ? RC2D_Color{64, 84, 110, 220}
                : RC2D_Color{50, 58, 68, 220});
        rc2d_graphics_rectangle("fill", &dupRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &dupRect);

        rc2d_graphics_setColor(
            canDuplicateOrDelete
                ? RC2D_Color{118, 64, 64, 220}
                : RC2D_Color{62, 52, 52, 220});
        rc2d_graphics_rectangle("fill", &delRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &delRect);

        const bool canReset = !isShipRow;
        rc2d_graphics_setColor(
            canReset
                ? RC2D_Color{76, 96, 66, 220}
                : RC2D_Color{56, 66, 58, 220});
        rc2d_graphics_rectangle("fill", &resetRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &resetRect);

        const bool debugActive = debugBoundsVisible;
        rc2d_graphics_setColor(
            debugActive ? RC2D_Color{86, 118, 68, 220} : RC2D_Color{64, 78, 60, 220});
        rc2d_graphics_rectangle("fill", &debugRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &debugRect);

        if (this->overlayFont.sdl_font != nullptr)
        {
            const char* visLabel = visible ? "SHOW" : "HIDE";
            RC2D_Text visText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), visLabel);
            visText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&visText);
            int visW = 0;
            int visH = 0;
            rc2d_graphics_getTextSize(&visText, &visW, &visH);
            rc2d_graphics_drawText(
                &visText,
                visRect.x + ((visRect.w - static_cast<float>(visW)) * 0.5f),
                visRect.y + ((visRect.h - static_cast<float>(visH)) * 0.5f));
            rc2d_graphics_destroyText(&visText);

            const char* lockLabel = lockVisualState ? "LOCK" : "FREE";
            RC2D_Text lockText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lockLabel);
            lockText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&lockText);
            int lockW = 0;
            int lockH = 0;
            rc2d_graphics_getTextSize(&lockText, &lockW, &lockH);
            rc2d_graphics_drawText(
                &lockText,
                lockRect.x + ((lockRect.w - static_cast<float>(lockW)) * 0.5f),
                lockRect.y + ((lockRect.h - static_cast<float>(lockH)) * 0.5f));
            rc2d_graphics_destroyText(&lockText);

            const char* dupLabel = canDuplicateOrDelete ? "DUP" : "-";
            RC2D_Text dupText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), dupLabel);
            dupText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&dupText);
            int dupW = 0;
            int dupH = 0;
            rc2d_graphics_getTextSize(&dupText, &dupW, &dupH);
            rc2d_graphics_drawText(
                &dupText,
                dupRect.x + ((dupRect.w - static_cast<float>(dupW)) * 0.5f),
                dupRect.y + ((dupRect.h - static_cast<float>(dupH)) * 0.5f));
            rc2d_graphics_destroyText(&dupText);

            const char* delLabel = canDuplicateOrDelete ? "DEL" : "-";
            RC2D_Text delText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), delLabel);
            delText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&delText);
            int delW = 0;
            int delH = 0;
            rc2d_graphics_getTextSize(&delText, &delW, &delH);
            rc2d_graphics_drawText(
                &delText,
                delRect.x + ((delRect.w - static_cast<float>(delW)) * 0.5f),
                delRect.y + ((delRect.h - static_cast<float>(delH)) * 0.5f));
            rc2d_graphics_destroyText(&delText);

            const char* resetLabel = canReset ? "RESET" : "-";
            RC2D_Text resetText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), resetLabel);
            resetText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&resetText);
            int resetW = 0;
            int resetH = 0;
            rc2d_graphics_getTextSize(&resetText, &resetW, &resetH);
            rc2d_graphics_drawText(
                &resetText,
                resetRect.x + ((resetRect.w - static_cast<float>(resetW)) * 0.5f),
                resetRect.y + ((resetRect.h - static_cast<float>(resetH)) * 0.5f));
            rc2d_graphics_destroyText(&resetText);

            const char* debugLabel = "DEBUG";
            RC2D_Text debugText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), debugLabel);
            debugText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&debugText);
            int debugW = 0;
            int debugH = 0;
            rc2d_graphics_getTextSize(&debugText, &debugW, &debugH);
            rc2d_graphics_drawText(
                &debugText,
                debugRect.x + ((debugRect.w - static_cast<float>(debugW)) * 0.5f),
                debugRect.y + ((debugRect.h - static_cast<float>(debugH)) * 0.5f));
            rc2d_graphics_destroyText(&debugText);

            const bool behind = (drawOrder < this->shipDrawOrder);
            const float indent = isShipRow ? 18.0f : (behind ? 8.0f : 34.0f);
            const float labelX = rowRect.x + indent;
            const float labelWidth = (debugRect.x - 6.0f) - labelX;
            const int maxChars = std::clamp(static_cast<int>(labelWidth / 6.6f), 12, 72);
            std::string rowLabel = std::string("z=") + std::to_string(drawOrder) + " | " + label;

            RC2D_Text rowText = rc2d_graphics_createText(
                const_cast<RC2D_Font*>(&this->overlayFont),
                makeAssetLabel(rowLabel, maxChars).c_str());
            rowText.color = visible ? kHudTextColor : RC2D_Color{170, 178, 188, 235};
            rc2d_graphics_setTextColor(&rowText);
            int rowTextW = 0;
            int rowTextH = 0;
            rc2d_graphics_getTextSize(&rowText, &rowTextW, &rowTextH);
            rc2d_graphics_drawText(
                &rowText,
                labelX,
                rowRect.y + (std::max)((rowRect.h - static_cast<float>(rowTextH)) * 0.5f, 1.0f));
            rc2d_graphics_destroyText(&rowText);
        }
    }

    if (this->layerRowDragActive && this->layerRowDragMoved && visibleCount > 0)
    {
        const int sourceInsertIndex = this->layerRowDragSourceDisplayIndex + 1;
        const int sourceAboveInsertIndex = this->layerRowDragSourceDisplayIndex;
        const int insertionIndex = std::clamp(this->layerRowDragTargetInsertIndex, 0, itemCount);
        if (insertionIndex != sourceInsertIndex &&
            insertionIndex != sourceAboveInsertIndex &&
            insertionIndex >= startIndex &&
            insertionIndex <= (startIndex + visibleCount))
        {
            float lineY = rowsTopY + (static_cast<float>(insertionIndex - startIndex) * (rowHeight + rowGap));
            const float rowsBottomY = rowsTopY + (static_cast<float>(visibleCount) * (rowHeight + rowGap)) - rowGap;
            lineY = std::clamp(lineY, rowsTopY, rowsBottomY);
            SDL_FRect insertLine{
                rowsLeftX + 2.0f,
                lineY - 1.0f,
                rowsWidth - 4.0f,
                2.0f};
            rc2d_graphics_setColor(RC2D_Color{228, 210, 86, 240});
            rc2d_graphics_rectangle("fill", &insertLine);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapVfxScene::updateLayerListRowDragFromMouse(void)
{
    if (!this->layerRowDragActive)
    {
        return;
    }

    const int itemCount = static_cast<int>(this->getOrderedVfxInstanceIndicesForLayerPanel().size());
    if (itemCount <= 1)
    {
        this->layerRowDragActive = false;
        this->layerRowDragMoved = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragTargetInsertIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        const int sourceDisplayIndex = this->layerRowDragSourceDisplayIndex;
        const int sourceInsertIndex = sourceDisplayIndex + 1;
        const int targetInsertIndex = std::clamp(this->layerRowDragTargetInsertIndex, 0, itemCount);

        this->layerRowDragActive = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;

        if (this->layerRowDragMoved && targetInsertIndex != sourceInsertIndex)
        {
            int targetDisplayIndex = targetInsertIndex;
            if (targetDisplayIndex > sourceDisplayIndex)
            {
                targetDisplayIndex -= 1;
            }
            targetDisplayIndex = std::clamp(targetDisplayIndex, 0, itemCount - 1);
            this->applyLayerPanelReorder(sourceDisplayIndex, targetDisplayIndex);
        }

        this->layerRowDragMoved = false;
        this->layerRowDragTargetInsertIndex = -1;
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
    const float rowsTopY = this->layerListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->layerListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const int startIndex = this->computeListStartIndex(this->layerListScrollOffset, itemCount);
    const int visibleCount = (std::max)((std::min)(kVisibleListRows, itemCount - startIndex), 0);

    int insertionIndex = startIndex;
    if (visibleCount > 0)
    {
        insertionIndex = startIndex + visibleCount;
        for (int i = 0; i < visibleCount; ++i)
        {
            const float rowTop = rowsTopY + (static_cast<float>(i) * (rowHeight + rowGap));
            const float rowMiddle = rowTop + (rowHeight * 0.5f);
            if (mouseY < rowMiddle)
            {
                insertionIndex = startIndex + i;
                break;
            }
        }
    }
    insertionIndex = std::clamp(insertionIndex, 0, itemCount);
    if (insertionIndex == this->layerRowDragSourceDisplayIndex)
    {
        insertionIndex = (std::max)(0, insertionIndex - 1);
    }

    if (std::fabs(mouseY - this->layerRowDragStartMouseY) > 2.5f)
    {
        this->layerRowDragMoved = true;
    }
    this->layerRowDragTargetInsertIndex = insertionIndex;

    const float autoScrollMargin = 12.0f;
    const int maxOffset = this->getListMaxScrollOffset(itemCount);
    if (mouseY < (rowsTopY + autoScrollMargin) && this->layerListScrollOffset > 0)
    {
        this->layerListScrollOffset -= 1;
    }
    else if (mouseY > (rowsTopY + rowsHeight - autoScrollMargin) && this->layerListScrollOffset < maxOffset)
    {
        this->layerListScrollOffset += 1;
    }
    this->clampListScrollOffset(&this->layerListScrollOffset, itemCount);
}

void EditorMapVfxScene::applyLayerPanelReorder(int sourceDisplayIndex, int targetDisplayIndex)
{
    const std::vector<int> orderedLayerIndices = this->getOrderedVfxInstanceIndicesForLayerPanel();
    const int itemCount = static_cast<int>(orderedLayerIndices.size());
    if (itemCount <= 1)
    {
        return;
    }
    if (sourceDisplayIndex < 0 || sourceDisplayIndex >= itemCount)
    {
        return;
    }
    if (targetDisplayIndex < 0 || targetDisplayIndex >= itemCount)
    {
        return;
    }
    if (sourceDisplayIndex == targetDisplayIndex)
    {
        return;
    }

    std::vector<int> rowToInstanceIndex = orderedLayerIndices;

    const int movedItem = rowToInstanceIndex[static_cast<size_t>(sourceDisplayIndex)];
    rowToInstanceIndex.erase(rowToInstanceIndex.begin() + sourceDisplayIndex);
    rowToInstanceIndex.insert(rowToInstanceIndex.begin() + targetDisplayIndex, movedItem);

    auto shipIt = std::find(rowToInstanceIndex.begin(), rowToInstanceIndex.end(), -1);
    if (shipIt == rowToInstanceIndex.end())
    {
        return;
    }
    const int shipDisplayIndex = static_cast<int>(std::distance(rowToInstanceIndex.begin(), shipIt));
    this->shipDrawOrder = 0;

    for (int i = 0; i < static_cast<int>(rowToInstanceIndex.size()); ++i)
    {
        const int instanceIndex = rowToInstanceIndex[static_cast<size_t>(i)];
        if (instanceIndex < 0)
        {
            continue;
        }

        int drawOrder = i - shipDisplayIndex;
        if (drawOrder == 0)
        {
            drawOrder = (i > shipDisplayIndex) ? 1 : -1;
        }

        ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(instanceIndex)];
        DirectionOverride* override = this->getEditableDirectionOverride(&instance);
        if (override != nullptr)
        {
            override->drawOrder = drawOrder;
        }
        else
        {
            instance.drawOrder = drawOrder;
        }
    }

    for (ShipVfxInstance& instance : this->shipVfxInstances)
    {
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const int effectiveOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
        instance.behindShip = (effectiveOrder < this->shipDrawOrder);
    }

    const int selectedRow = this->getSelectedLayerRowIndexForDisplay(this->getOrderedVfxInstanceIndicesForLayerPanel());
    this->ensureSelectionVisible(selectedRow, &this->layerListScrollOffset, itemCount);
    this->markShipVfxDirty();
    this->statusMessage = "Ordre des layers mis a jour (drag souris).";
}

void EditorMapVfxScene::drawInvalidAssetPanel(
    const SDL_FRect& panelRect,
    const char* title,
    const std::vector<InvalidAssetEntry>& entries,
    int scrollOffset) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{40, 30, 20, 205});
    rc2d_graphics_rectangle("fill", &panelRect);
    rc2d_graphics_setColor(RC2D_Color{185, 138, 102, 230});
    rc2d_graphics_rectangle("line", &panelRect);
    const float panelPadding = kInvalidPanelPadding;
    const float headerHeight = kInvalidPanelHeaderHeight;
    const float rowGap = kInvalidPanelRowGap;
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kInvalidVisibleRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kInvalidVisibleRows);
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;
    rc2d_graphics_setColor(RC2D_Color{90, 70, 52, 220});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{202, 162, 128, 225});
    rc2d_graphics_rectangle("line", &scrollTrackRect);

    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text titleText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title);
        titleText.color = RC2D_Color{248, 210, 168, 245};
        rc2d_graphics_setTextColor(&titleText);
        rc2d_graphics_drawText(&titleText, panelRect.x + 8.0f, panelRect.y + 4.0f);
        rc2d_graphics_destroyText(&titleText);
    }

    if (entries.empty())
    {
        if (this->overlayFont.sdl_font != nullptr)
        {
            RC2D_Text noEntryText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Aucune invalidation");
            noEntryText.color = RC2D_Color{210, 232, 210, 230};
            rc2d_graphics_setTextColor(&noEntryText);
            rc2d_graphics_drawText(&noEntryText, panelRect.x + 8.0f, rowsTopY + 3.0f);
            rc2d_graphics_destroyText(&noEntryText);
        }
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int itemCount = static_cast<int>(entries.size());
    const int startIndex = std::clamp(scrollOffset, 0, (std::max)(itemCount - kInvalidVisibleRows, 0));
    const int maxOffset = (std::max)(itemCount - kInvalidVisibleRows, 0);
    float thumbHeight = scrollTrackRect.h;
    float thumbY = scrollTrackRect.y;
    if (maxOffset > 0)
    {
        thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kInvalidVisibleRows)) / static_cast<float>(itemCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(maxOffset);
        thumbY += ratio * thumbTravel;
    }

    SDL_FRect scrollThumbRect{};
    scrollThumbRect.x = scrollTrackRect.x + 1.0f;
    scrollThumbRect.y = thumbY;
    scrollThumbRect.w = scrollTrackRect.w - 2.0f;
    scrollThumbRect.h = thumbHeight;
    rc2d_graphics_setColor(RC2D_Color{236, 198, 164, 235});
    rc2d_graphics_rectangle("fill", &scrollThumbRect);
    rc2d_graphics_setColor(RC2D_Color{252, 224, 196, 245});
    rc2d_graphics_rectangle("line", &scrollThumbRect);

    for (int i = 0; i < kInvalidVisibleRows; ++i)
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
        rc2d_graphics_setColor(RC2D_Color{66, 48, 32, 220});
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(RC2D_Color{190, 150, 114, 215});
        rc2d_graphics_rectangle("line", &rowRect);

        if (this->overlayFont.sdl_font != nullptr)
        {
            const InvalidAssetEntry& entry = entries[static_cast<size_t>(itemIndex)];
            std::string line = entry.folderName + ": " + entry.reason;
            const int maxChars = std::clamp(static_cast<int>((rowsWidth - 20.0f) / 6.5f), 24, 96);
            RC2D_Text rowText = rc2d_graphics_createText(
                const_cast<RC2D_Font*>(&this->overlayFont),
                makeAssetLabel(line, maxChars).c_str());
            rowText.color = RC2D_Color{245, 210, 158, 230};
            rc2d_graphics_setTextColor(&rowText);
            int textW = 0;
            int textH = 0;
            rc2d_graphics_getTextSize(&rowText, &textW, &textH);
            const float textY = rowRect.y + (std::max)((rowRect.h - static_cast<float>(textH)) * 0.5f, 1.0f);
            rc2d_graphics_drawText(&rowText, rowRect.x + 6.0f, textY);
            rc2d_graphics_destroyText(&rowText);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool EditorMapVfxScene::handleInvalidAssetPanelClick(
    float x,
    float y,
    const SDL_FRect& panelRect,
    int itemCount,
    int* scrollOffset,
    bool* dragActive,
    float* dragGrabOffsetY)
{
    if (!this->pointInRect(x, y, panelRect))
    {
        return false;
    }

    const float panelPadding = kInvalidPanelPadding;
    const float headerHeight = kInvalidPanelHeaderHeight;
    const float rowGap = kInvalidPanelRowGap;
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kInvalidVisibleRows - 1) * rowGap));
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        const int maxOffset = (std::max)(itemCount - kInvalidVisibleRows, 0);
        if (maxOffset <= 0)
        {
            if (dragActive != nullptr)
            {
                *dragActive = false;
            }
            return true;
        }

        const float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kInvalidVisibleRows)) / static_cast<float>(itemCount));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const int startIndex = std::clamp((scrollOffset != nullptr) ? *scrollOffset : 0, 0, maxOffset);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(maxOffset);
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
                *scrollOffset = std::clamp(*scrollOffset, 0, maxOffset);
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
    return true;
}

void EditorMapVfxScene::handleInvalidAssetPanelScrollDragFromMouse(
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

    const float panelPadding = kInvalidPanelPadding;
    const float headerHeight = kInvalidPanelHeaderHeight;
    const float rowGap = kInvalidPanelRowGap;
    const float rowsTopY = panelRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        panelRect.h - ((panelPadding * 2.0f) + headerHeight + ((kInvalidVisibleRows - 1) * rowGap));
    const float rowsLeftX = panelRect.x + panelPadding;
    const float rowsWidth = panelRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    const int maxOffset = (std::max)(itemCount - kInvalidVisibleRows, 0);
    if (maxOffset <= 0)
    {
        *dragActive = false;
        if (dragGrabOffsetY != nullptr)
        {
            *dragGrabOffsetY = 0.0f;
        }
        return;
    }

    const float thumbHeight = (std::max)(14.0f, (scrollTrackRect.h * static_cast<float>(kInvalidVisibleRows)) / static_cast<float>(itemCount));
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
        *scrollOffset = std::clamp(*scrollOffset, 0, maxOffset);
    }
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

    this->invalidShipFolders.clear();

    std::vector<std::filesystem::path> candidateFolders;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(rootPath, fsError))
    {
        if (fsError)
        {
            fsError.clear();
            continue;
        }
        if (entry.is_directory(fsError) && !fsError)
        {
            candidateFolders.push_back(entry.path());
        }
    }

    std::sort(candidateFolders.begin(), candidateFolders.end(), [](const auto& a, const auto& b) {
        return makePathKeyLower(a.string()) < makePathKeyLower(b.string());
    });

    std::vector<std::string> knownPathKeys;
    knownPathKeys.reserve(this->importedShips.size() + candidateFolders.size());
    for (const ImportedShip& ship : this->importedShips)
    {
        knownPathKeys.push_back(makePathKeyLower(ship.folderAbsolutePath));
    }

    int addedCount = 0;
    for (const std::filesystem::path& folderPath : candidateFolders)
    {
        std::error_code absError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(folderPath, absError);
        const std::string absolutePathNorm = normalizePathSlashes((absError ? folderPath : absolutePath).string());
        if (std::find(knownPathKeys.begin(), knownPathKeys.end(), makePathKeyLower(absolutePathNorm)) != knownPathKeys.end())
        {
            continue;
        }

        std::vector<std::string> fileNames;
        std::vector<std::string> pngFileNames;
        for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(folderPath, fsError))
        {
            if (fsError)
            {
                fsError.clear();
                continue;
            }
            if (!child.is_regular_file(fsError) || fsError)
            {
                fsError.clear();
                continue;
            }

            const std::string fileName = child.path().filename().string();
            fileNames.push_back(fileName);
            std::string extension = child.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (extension == ".png")
            {
                pngFileNames.push_back(fileName);
            }
        }

        std::vector<std::string> reasons;
        const bool hasAnchor = std::find(fileNames.begin(), fileNames.end(), "ship_anchor.json") != fileNames.end();
        if (!hasAnchor)
        {
            reasons.push_back("ship_anchor.json manquant");
        }

        std::array<bool, 8> hasExpected{};
        hasExpected.fill(false);
        std::vector<std::string> unexpectedPng;
        for (const std::string& pngName : pngFileNames)
        {
            bool matched = false;
            for (int i = 1; i <= kShipSpriteCount; ++i)
            {
                if (pngName == (std::to_string(i) + ".png"))
                {
                    hasExpected[static_cast<size_t>(i - 1)] = true;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                unexpectedPng.push_back(pngName);
            }
        }

        for (int i = 1; i <= kShipSpriteCount; ++i)
        {
            if (!hasExpected[static_cast<size_t>(i - 1)])
            {
                reasons.push_back("fichier obligatoire manquant: " + std::to_string(i) + ".png");
            }
        }
        if (static_cast<int>(pngFileNames.size()) != kShipSpriteCount)
        {
            reasons.push_back("exactement 8 PNG requis (trouve: " + std::to_string(pngFileNames.size()) + ")");
        }
        if (!unexpectedPng.empty())
        {
            std::sort(unexpectedPng.begin(), unexpectedPng.end());
            reasons.push_back("PNG mal nommes: " + unexpectedPng[0] + ((unexpectedPng.size() > 1) ? "..." : ""));
        }

        if (!reasons.empty())
        {
            InvalidAssetEntry invalid{};
            invalid.folderName = folderPath.filename().string();
            if (invalid.folderName.empty())
            {
                invalid.folderName = absolutePathNorm;
            }
            invalid.reason = reasons[0];
            this->invalidShipFolders.push_back(std::move(invalid));
            continue;
        }

        ImportedShip importedShip{};
        importedShip.folderAbsolutePath = absolutePathNorm;
        importedShip.displayName = folderPath.filename().string();
        if (importedShip.displayName.empty())
        {
            importedShip.displayName = importedShip.folderAbsolutePath;
        }
        importedShip.configJsonPath = this->buildShipConfigJsonPath(importedShip);
        this->importedShips.push_back(importedShip);
        knownPathKeys.push_back(makePathKeyLower(absolutePathNorm));
        addedCount += 1;
    }

    std::sort(this->importedShips.begin(), this->importedShips.end(), [](const ImportedShip& a, const ImportedShip& b) {
        std::string aKey = makePathKeyLower(stripListPrefix(a.displayName, "ship-"));
        std::string bKey = makePathKeyLower(stripListPrefix(b.displayName, "ship-"));
        if (aKey.empty())
        {
            aKey = makePathKeyLower(a.displayName);
        }
        if (bKey.empty())
        {
            bKey = makePathKeyLower(b.displayName);
        }
        if (aKey != bKey)
        {
            return aKey < bKey;
        }
        const bool aPrefixed = (makePathKeyLower(trimAscii(extractFileName(a.displayName))).rfind("ship-", 0U) == 0U);
        const bool bPrefixed = (makePathKeyLower(trimAscii(extractFileName(b.displayName))).rfind("ship-", 0U) == 0U);
        if (aPrefixed != bPrefixed)
        {
            return aPrefixed; // Priorite aux dossiers "ship-*".
        }
        return makePathKeyLower(a.folderAbsolutePath) < makePathKeyLower(b.folderAbsolutePath);
    });

    // Deduplication silencieuse pour la liste UI:
    // - on garde un seul element par chemin absolu
    // - puis un seul element par libelle affiche (prefixe "ship-" retire si present)
    // Aucun ajout dans "Ships ignores" pour ce cas.
    std::vector<ImportedShip> deduplicatedShips;
    deduplicatedShips.reserve(this->importedShips.size());
    std::vector<std::string> seenPathKeys;
    std::vector<std::string> seenUiKeys;
    for (const ImportedShip& ship : this->importedShips)
    {
        const std::string pathKey = makePathKeyLower(ship.folderAbsolutePath);
        if (std::find(seenPathKeys.begin(), seenPathKeys.end(), pathKey) != seenPathKeys.end())
        {
            continue;
        }

        std::string uiKey = makePathKeyLower(stripListPrefix(ship.displayName, "ship-"));
        if (uiKey.empty())
        {
            uiKey = makePathKeyLower(ship.displayName);
        }
        if (std::find(seenUiKeys.begin(), seenUiKeys.end(), uiKey) != seenUiKeys.end())
        {
            continue;
        }

        deduplicatedShips.push_back(ship);
        seenPathKeys.push_back(pathKey);
        seenUiKeys.push_back(uiKey);
    }
    this->importedShips.swap(deduplicatedShips);

    if (!this->importedShips.empty() && (this->selectedShipIndex < 0 || this->selectedShipIndex >= static_cast<int>(this->importedShips.size())))
    {
        this->selectImportedShipAtIndex(0);
    }

    if (addedCount <= 0 && this->importedShips.empty())
    {
        this->statusMessage = "Aucun navire valide detecte dans assets/images/ships.";
        return false;
    }

    this->statusMessage =
        std::to_string(static_cast<int>(this->importedShips.size())) +
        " navire(s) valides, " +
        std::to_string(static_cast<int>(this->invalidShipFolders.size())) +
        " ignore(s).";
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

    const std::filesystem::path anchorSourcePath = folderPath / "ship_anchor.json";
    if (!std::filesystem::exists(anchorSourcePath, fsError) || !std::filesystem::is_regular_file(anchorSourcePath, fsError))
    {
        this->statusMessage = "Dossier invalide: ship_anchor.json manquant.";
        return false;
    }

    std::vector<char> anchorBytes;
    {
        std::ifstream anchorInput(anchorSourcePath, std::ios::binary | std::ios::ate);
        if (!anchorInput.is_open())
        {
            this->statusMessage = "Lecture impossible: ship_anchor.json";
            return false;
        }
        const std::streamsize anchorSize = anchorInput.tellg();
        if (anchorSize <= 0)
        {
            this->statusMessage = "ship_anchor.json vide.";
            return false;
        }
        anchorInput.seekg(0, std::ios::beg);
        anchorBytes.resize(static_cast<size_t>(anchorSize));
        if (!anchorInput.read(anchorBytes.data(), anchorSize))
        {
            this->statusMessage = "Lecture ship_anchor.json echouee.";
            return false;
        }
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
    this->previewShip.setHealthVisual((this->previewShipStateIndex == 1) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
    this->applyPreviewDirectionToShip();
    this->previewShip.setDrawAlpha(255);
    this->previewShip.setPositionTile(this->previewShipTile.x, this->previewShipTile.y);
    this->previewShipLoaded = true;
    this->loadedShipFolderAbsolute = normalizePathSlashes(folderPath.string());
    this->shipLayerSelected = true;
    this->shipLayerVisible = true;
    this->shipLayerLocked = false;
    this->shipDebugBoundsVisible = true;
    this->statusMessage = "Navire preview charge (8 sprites + anchor).";
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

    this->shipVfxInstances.clear();
    this->setSelectedVfxInstanceIndex(-1);
    this->nextVfxInstanceId = 1U;
    this->shipDrawOrder = 0;
    this->shipDebugBoundsVisible = true;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath = selectedShip.configJsonPath;
    this->loadShipVfxConfigForSelectedShip();

    this->statusMessage = "Navire actif: " + selectedShip.displayName + " | config: " +
        (this->loadedShipVfxConfigPath.empty() ? "aucune" : this->loadedShipVfxConfigPath);
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

    std::vector<std::filesystem::path> jsonFiles;
    std::vector<std::filesystem::path> pngFiles;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sfxFolder, fsError))
    {
        if (fsError)
        {
            fsError.clear();
            continue;
        }
        if (!entry.is_regular_file(fsError) || fsError)
        {
            fsError.clear();
            continue;
        }
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext == ".json")
        {
            jsonFiles.push_back(entry.path());
        }
        else if (ext == ".png")
        {
            pngFiles.push_back(entry.path());
        }
    }

    if (jsonFiles.empty() || pngFiles.empty())
    {
        this->statusMessage = "VFX invalide: au moins un .json et un .png requis.";
        return false;
    }

    std::sort(jsonFiles.begin(), jsonFiles.end());
    std::sort(pngFiles.begin(), pngFiles.end());

    std::vector<std::string> pngNames;
    pngNames.reserve(pngFiles.size());
    for (const std::filesystem::path& pngPath : pngFiles)
    {
        pngNames.push_back(pngPath.filename().string());
    }

    std::vector<CustomSpritesheetFrame> selectedFrames;
    std::filesystem::path selectedJsonSourcePath;
    std::filesystem::path selectedImageSourcePath;
    float selectedDefaultFps = 12.0f;

    for (const std::filesystem::path& jsonSourcePath : jsonFiles)
    {
        std::ifstream jsonInput(jsonSourcePath, std::ios::binary | std::ios::ate);
        if (!jsonInput.is_open())
        {
            continue;
        }
        const std::streamsize jsonSize = jsonInput.tellg();
        if (jsonSize <= 0)
        {
            continue;
        }
        jsonInput.seekg(0, std::ios::beg);
        std::vector<char> jsonBytes(static_cast<size_t>(jsonSize) + 1U, '\0');
        if (!jsonInput.read(jsonBytes.data(), jsonSize))
        {
            continue;
        }

        CustomSpritesheetParseResult parsed{};
        std::string conversionError;
        if (tryParseCustomSpritesheetJson(
                jsonBytes.data(),
                pngNames,
                &parsed,
                &conversionError))
        {
            const std::filesystem::path imageSourcePath = sfxFolder / parsed.imageFileName;
            if (std::filesystem::exists(imageSourcePath, fsError) && std::filesystem::is_regular_file(imageSourcePath, fsError))
            {
                selectedFrames = std::move(parsed.frames);
                selectedJsonSourcePath = jsonSourcePath;
                selectedImageSourcePath = imageSourcePath;
                selectedDefaultFps = parsed.fps;
                break;
            }
        }
    }

    if (selectedFrames.empty() || selectedJsonSourcePath.empty() || selectedImageSourcePath.empty())
    {
        this->statusMessage = "VFX invalide: JSON attendu au format custom {fps, frames[]}.";
        return false;
    }

    std::ifstream imageInput(selectedImageSourcePath, std::ios::binary | std::ios::ate);
    if (!imageInput.is_open())
    {
        this->statusMessage = "Lecture image atlas impossible.";
        return false;
    }
    const std::streamsize imageSize = imageInput.tellg();
    if (imageSize <= 0)
    {
        this->statusMessage = "Image atlas vide.";
        return false;
    }
    imageInput.seekg(0, std::ios::beg);
    std::vector<char> imageBytes(static_cast<size_t>(imageSize));
    if (!imageInput.read(imageBytes.data(), imageSize))
    {
        this->statusMessage = "Lecture image atlas echouee.";
        return false;
    }

    this->ensureUserStorageFolders();
    ++this->importedSfxCounter;
    char storageFolderPath[256] = {};
    SDL_snprintf(storageFolderPath, sizeof(storageFolderPath), "editor-vfx-sfx/imported-%04u", this->importedSfxCounter);
    rc2d_storage_userMkdir(storageFolderPath);

    const std::string storageImagePath = std::string(storageFolderPath) + "/" + selectedImageSourcePath.filename().string();
    if (!rc2d_storage_userWriteFile(storageImagePath.c_str(), imageBytes.data(), static_cast<Uint64>(imageBytes.size())))
    {
        this->statusMessage = "Echec copie image atlas en storage user.";
        return false;
    }

    RC2D_Image image = rc2d_graphics_loadImageFromStorage(storageImagePath.c_str(), RC2D_STORAGE_USER);
    if (image.sdl_texture == nullptr)
    {
        this->statusMessage = "Echec chargement image VFX.";
        return false;
    }
    SDL_SetTextureScaleMode(image.sdl_texture, SDL_SCALEMODE_LINEAR);

    ImportedSfx imported{};
    imported.id = "sfx_" + std::to_string(this->importedSfxCounter);
    imported.displayName = sfxFolder.filename().string();
    if (imported.displayName.empty())
    {
        imported.displayName = selectedJsonSourcePath.stem().string();
    }
    imported.sourceJsonPath = normalizePathSlashes(selectedJsonSourcePath.string());
    imported.sourceImagePath = normalizePathSlashes(selectedImageSourcePath.string());
    imported.sourceFolderAbsolutePath = normalizePathSlashes(sfxFolder.string());
    imported.storageImagePath = storageImagePath;
    imported.image = image;
    imported.frames.reserve(selectedFrames.size());
    for (const CustomSpritesheetFrame& parsedFrame : selectedFrames)
    {
        ImportedSfxFrame frame{};
        frame.index = parsedFrame.index;
        frame.frameName = std::to_string(parsedFrame.index) + ".png";
        frame.x = parsedFrame.x;
        frame.y = parsedFrame.y;
        frame.w = parsedFrame.w;
        frame.h = parsedFrame.h;
        imported.frames.push_back(std::move(frame));
    }
    imported.defaultFps = std::clamp(selectedDefaultFps, kSfxFpsMin, kSfxFpsMax);

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

    this->invalidVfxFolders.clear();

    std::vector<std::filesystem::path> candidateFolders;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(rootPath, fsError))
    {
        if (fsError)
        {
            fsError.clear();
            continue;
        }
        if (entry.is_directory(fsError) && !fsError)
        {
            candidateFolders.push_back(entry.path());
        }
    }
    std::sort(candidateFolders.begin(), candidateFolders.end(), [](const auto& a, const auto& b) {
        return makePathKeyLower(a.string()) < makePathKeyLower(b.string());
    });

    std::vector<std::string> knownFolderKeys;
    knownFolderKeys.reserve(this->importedSfx.size() + candidateFolders.size());
    for (const ImportedSfx& sfx : this->importedSfx)
    {
        knownFolderKeys.push_back(makePathKeyLower(sfx.sourceFolderAbsolutePath));
    }

    int addedCount = 0;
    for (const std::filesystem::path& folderPath : candidateFolders)
    {
        std::error_code absError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(folderPath, absError);
        const std::string absoluteNorm = normalizePathSlashes((absError ? folderPath : absolutePath).string());
        if (std::find(knownFolderKeys.begin(), knownFolderKeys.end(), makePathKeyLower(absoluteNorm)) != knownFolderKeys.end())
        {
            continue;
        }

        int jsonCount = 0;
        int pngCount = 0;
        for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(folderPath, fsError))
        {
            if (fsError)
            {
                fsError.clear();
                continue;
            }
            if (!child.is_regular_file(fsError) || fsError)
            {
                fsError.clear();
                continue;
            }
            std::string ext = child.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (ext == ".json")
            {
                jsonCount += 1;
            }
            if (ext == ".png")
            {
                pngCount += 1;
            }
        }

        if (jsonCount <= 0 || pngCount <= 0)
        {
            InvalidAssetEntry invalid{};
            invalid.folderName = folderPath.filename().string();
            invalid.reason = (jsonCount <= 0 && pngCount <= 0)
                ? "Aucun .json et aucun .png"
                : (jsonCount <= 0 ? "Aucun .json" : "Aucun .png");
            this->invalidVfxFolders.push_back(std::move(invalid));
            continue;
        }

        if (this->importSfxFromAbsolutePath(absoluteNorm.c_str()))
        {
            knownFolderKeys.push_back(makePathKeyLower(absoluteNorm));
            addedCount += 1;
        }
        else
        {
            InvalidAssetEntry invalid{};
            invalid.folderName = folderPath.filename().string();
            invalid.reason = this->statusMessage;
            this->invalidVfxFolders.push_back(std::move(invalid));
        }
    }

    std::sort(this->importedSfx.begin(), this->importedSfx.end(), [](const ImportedSfx& a, const ImportedSfx& b) {
        const std::string aKey = makePathKeyLower(a.displayName);
        const std::string bKey = makePathKeyLower(b.displayName);
        if (aKey != bKey)
        {
            return aKey < bKey;
        }
        return makePathKeyLower(a.sourceFolderAbsolutePath) < makePathKeyLower(b.sourceFolderAbsolutePath);
    });

    if (!this->importedSfx.empty() && (this->selectedSfxIndex < 0 || this->selectedSfxIndex >= static_cast<int>(this->importedSfx.size())))
    {
        this->selectedSfxIndex = 0;
    }

    if (addedCount <= 0 && this->importedSfx.empty())
    {
        this->statusMessage = "Aucun VFX valide detecte dans assets/images/vfx.";
        return false;
    }

    this->statusMessage =
        std::to_string(static_cast<int>(this->importedSfx.size())) +
        " VFX valides, " +
        std::to_string(static_cast<int>(this->invalidVfxFolders.size())) +
        " ignores.";
    return true;
}

void EditorMapVfxScene::refreshLooseFolderUnionCrop(ImportedLooseFolder& folder)
{
    folder.looseUnionCropReady = false;
    if (folder.sprites.empty())
    {
        return;
    }

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

    std::vector<SDL_Surface*> sourceSurfaces;
    sourceSurfaces.reserve(orderedSpriteIndices.size());
    for (size_t o = 0; o < orderedSpriteIndices.size(); ++o)
    {
        const ImportedLooseSprite& sprite = folder.sprites[orderedSpriteIndices[o]];
        RC2D_ImageData src = rc2d_graphics_loadImageDataFromStorage(sprite.storagePath.c_str(), RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            return;
        }
        sourceSurfaces.push_back(src.sdl_surface);
        src.sdl_surface = nullptr;
        rc2d_graphics_freeImageData(&src);
    }

    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    computeOpaqueUnionCropRectFromSurfaces(sourceSurfaces, &cropX, &cropY, &cropW, &cropH);
    destroySurfaceVector(sourceSurfaces);

    if (cropW <= 0 || cropH <= 0)
    {
        return;
    }

    folder.looseUnionCropX = cropX;
    folder.looseUnionCropY = cropY;
    folder.looseUnionCropW = cropW;
    folder.looseUnionCropH = cropH;
    folder.looseUnionCropReady = true;
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

    this->refreshLooseFolderUnionCrop(folder);

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
    int sourceInstanceNumber = 1;
    for (const ShipVfxInstance& existing : this->shipVfxInstances)
    {
        if (existing.importedSfxIndex == this->selectedSfxIndex)
        {
            sourceInstanceNumber += 1;
        }
    }
    const int sourceNumber = (std::max)(1, this->selectedSfxIndex + 1);
    instance.label = makeDefaultVfxLayerLabel(imported.displayName, sourceNumber, sourceInstanceNumber);
    instance.sourceJsonPath = imported.sourceJsonPath;
    instance.sourceDisplayName = imported.displayName;
    instance.importedSfxIndex = this->selectedSfxIndex;
    instance.offsetX = 0.0f;
    instance.offsetY = 0.0f;
    instance.rotationDeg = 0.0f;
    instance.flipHorizontal = false;
    instance.flipVertical = false;
    instance.drawOrder = static_cast<int>(this->shipVfxInstances.size()) + 1;
    instance.visible = true;
    instance.debugBoundsVisible = true;
    instance.locked = false;
    instance.behindShip = (instance.drawOrder < this->shipDrawOrder);
    instance.followShip = true;
    instance.sharedForAllDirections = true;
    instance.sharedForAllStates = true;
    for (DirectionOverride& override : instance.directionOverrides)
    {
        override.enabled = false;
        override.offsetX = instance.offsetX;
        override.offsetY = instance.offsetY;
        override.rotationDeg = instance.rotationDeg;
        override.flipHorizontal = instance.flipHorizontal;
        override.flipVertical = instance.flipVertical;
        override.drawOrder = instance.drawOrder;
        override.visible = instance.visible;
    }
    this->shipVfxInstances.push_back(instance);
    const int spawnedIndex = static_cast<int>(this->shipVfxInstances.size()) - 1;
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    this->setSelectedVfxInstanceIndex(spawnedIndex);
    this->shipLayerSelected = false;
    this->markShipVfxDirty();
    this->statusMessage = "VFX ajoute au centre: " + imported.displayName;
}

void EditorMapVfxScene::setSelectedVfxInstanceIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->selectedVfxInstanceIndex = -1;
        this->layerNameInput.clear();
        this->shipLayerSelected = true;
        return;
    }

    this->selectedVfxInstanceIndex = index;
    this->shipLayerSelected = false;
    this->layerNameInput = this->shipVfxInstances[static_cast<size_t>(index)].label;
}

void EditorMapVfxScene::rebuildVfxLayerLabelsFromCurrentInstances(void)
{
    struct SourceCounter
    {
        std::string key;
        int sourceNumber;
        int instanceCount;
    };

    std::vector<SourceCounter> sourceCounters;
    sourceCounters.reserve(this->shipVfxInstances.size());

    auto buildSourceKey = [this](const ShipVfxInstance& instance) -> std::string {
        if (instance.importedSfxIndex >= 0 && instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            return std::string("idx:") + std::to_string(instance.importedSfxIndex);
        }
        if (!instance.sourceJsonPath.empty())
        {
            return std::string("json:") + makePathKeyLower(instance.sourceJsonPath);
        }
        if (!instance.sourceDisplayName.empty())
        {
            return std::string("name:") + makePathKeyLower(instance.sourceDisplayName);
        }
        return std::string("id:") + std::to_string(instance.instanceId);
    };

    auto resolveSourceDisplayName = [this](const ShipVfxInstance& instance) -> std::string {
        if (!trimAscii(instance.sourceDisplayName).empty())
        {
            return instance.sourceDisplayName;
        }
        if (instance.importedSfxIndex >= 0 && instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            return this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)].displayName;
        }
        if (!instance.sourceJsonPath.empty())
        {
            return extractFileName(instance.sourceJsonPath);
        }
        return "vfx";
    };

    for (ShipVfxInstance& instance : this->shipVfxInstances)
    {
        const std::string sourceKey = buildSourceKey(instance);
        auto counterIt = std::find_if(sourceCounters.begin(), sourceCounters.end(), [&sourceKey](const SourceCounter& value) {
            return value.key == sourceKey;
        });
        if (counterIt == sourceCounters.end())
        {
            sourceCounters.push_back(SourceCounter{
                sourceKey,
                static_cast<int>(sourceCounters.size()) + 1,
                0});
            counterIt = sourceCounters.end() - 1;
        }

        counterIt->instanceCount += 1;
        instance.label = makeDefaultVfxLayerLabel(
            resolveSourceDisplayName(instance),
            counterIt->sourceNumber,
            counterIt->instanceCount);
    }

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
    {
        this->layerNameInput = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].label;
    }
}

void EditorMapVfxScene::removeSelectedVfxInstance(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->shipVfxInstances.erase(this->shipVfxInstances.begin() + this->selectedVfxInstanceIndex);
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    if (this->shipVfxInstances.empty())
    {
        this->setSelectedVfxInstanceIndex(-1);
    }
    else
    {
        const int nextIndex = std::clamp(this->selectedVfxInstanceIndex, 0, static_cast<int>(this->shipVfxInstances.size()) - 1);
        this->setSelectedVfxInstanceIndex(nextIndex);
    }
    this->markShipVfxDirty();
    this->statusMessage = "VFX retire.";
}

void EditorMapVfxScene::centerSelectedVfxInstance(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->offsetX = 0.0f;
        override->offsetY = 0.0f;
    }
    else
    {
        instance->offsetX = 0.0f;
        instance->offsetY = 0.0f;
    }
    this->markShipVfxDirty();
    this->statusMessage = "VFX recentre.";
}

void EditorMapVfxScene::moveSelectedVfxInstance(float deltaX, float deltaY)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    // Les offsets sont stockes en "px base 1.00" pour rester stables entre les niveaux de zoom.
    if (override != nullptr)
    {
        override->offsetX += (deltaX / zoom);
        override->offsetY += (deltaY / zoom);
    }
    else
    {
        instance->offsetX += (deltaX / zoom);
        instance->offsetY += (deltaY / zoom);
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::adjustSelectedVfxRotation(float deltaDegrees)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }

    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    float* rotation = (override != nullptr) ? &override->rotationDeg : &instance->rotationDeg;
    *rotation += deltaDegrees;
    while (*rotation < 0.0f)
    {
        *rotation += 360.0f;
    }
    while (*rotation >= 360.0f)
    {
        *rotation -= 360.0f;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxFlipHorizontal(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->flipHorizontal = !override->flipHorizontal;
    }
    else
    {
        instance->flipHorizontal = !instance->flipHorizontal;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxFlipVertical(void)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }
    if (instance->locked)
    {
        this->statusMessage = "Instance VFX verrouillee.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->flipVertical = !override->flipVertical;
    }
    else
    {
        instance->flipVertical = !instance->flipVertical;
    }
    this->markShipVfxDirty();
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
    this->moveSelectedLayerOrder(delta);
}

void EditorMapVfxScene::adjustShipDrawOrder(int delta)
{
    (void)delta;
    this->statusMessage = "SHIP est fixe a z=0 (reordonne avec le panel Layers).";
}

void EditorMapVfxScene::normalizeShipVfxDrawOrders(void)
{
    this->shipDrawOrder = 0;

    std::vector<size_t> behind;
    std::vector<size_t> front;
    behind.reserve(this->shipVfxInstances.size());
    front.reserve(this->shipVfxInstances.size());

    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        const int effectiveOrder = instance.drawOrder;
        if (effectiveOrder < 0)
        {
            behind.push_back(i);
        }
        else
        {
            front.push_back(i);
        }
    }

    std::sort(behind.begin(), behind.end(), [this](size_t a, size_t b) {
        const ShipVfxInstance& lhs = this->shipVfxInstances[a];
        const ShipVfxInstance& rhs = this->shipVfxInstances[b];
        if (lhs.drawOrder != rhs.drawOrder)
        {
            return lhs.drawOrder < rhs.drawOrder;
        }
        return lhs.instanceId < rhs.instanceId;
    });
    std::sort(front.begin(), front.end(), [this](size_t a, size_t b) {
        const ShipVfxInstance& lhs = this->shipVfxInstances[a];
        const ShipVfxInstance& rhs = this->shipVfxInstances[b];
        if (lhs.drawOrder != rhs.drawOrder)
        {
            return lhs.drawOrder < rhs.drawOrder;
        }
        return lhs.instanceId < rhs.instanceId;
    });

    int negativeOrder = -static_cast<int>(behind.size());
    for (size_t idx : behind)
    {
        this->shipVfxInstances[idx].drawOrder = negativeOrder++;
        this->shipVfxInstances[idx].behindShip = true;
    }

    int positiveOrder = 1;
    for (size_t idx : front)
    {
        this->shipVfxInstances[idx].drawOrder = positiveOrder++;
        this->shipVfxInstances[idx].behindShip = false;
    }
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

bool EditorMapVfxScene::handleLayerNameInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->layerNameInputFocused)
    {
        return false;
    }

    (void)mod;
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->layerNameInputFocused = false;
        if (this->hasSelectedVfxInstance())
        {
            this->layerNameInput = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].label;
        }
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->applyLayerNameInput();
        this->layerNameInputFocused = false;
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->layerNameInput.empty() && !isrepeat)
    {
        this->layerNameInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->layerNameInput.empty() && !isrepeat)
    {
        this->layerNameInput.clear();
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

    if (this->layerNameInput.size() >= 64U)
    {
        return true;
    }

    const unsigned char c = static_cast<unsigned char>(key[0]);
    if (c >= 32U && c <= 126U)
    {
        this->layerNameInput.push_back(static_cast<char>(c));
    }
    return true;
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
    SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    float shipCenterOffsetX = 0.0f;
    float shipCenterOffsetY = 0.0f;
    if (this->previewShip.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
    {
        shipCenter.x += shipCenterOffsetX;
        shipCenter.y += shipCenterOffsetY;
    }
    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    std::vector<std::pair<int, RenderItem>> candidates;
    candidates.reserve(this->shipVfxInstances.size());
    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const bool visible = (override != nullptr) ? override->visible : instance.visible;
        if (!visible)
        {
            continue;
        }
        const int drawOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
        candidates.push_back({static_cast<int>(i), RenderItem{false, drawOrder, instance.instanceId, static_cast<int>(i)}});
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
        if (imported.frames.empty() || imported.image.sdl_texture == nullptr)
        {
            continue;
        }

        const int frameCount = static_cast<int>(imported.frames.size());
        const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(imported.defaultFps, 1.0f))) % frameCount;
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];

        const float sourceW = frame.w;
        const float sourceH = frame.h;
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const float offsetX = ((override != nullptr) ? override->offsetX : instance.offsetX) * scale;
        const float offsetY = ((override != nullptr) ? override->offsetY : instance.offsetY) * scale;
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

bool EditorMapVfxScene::importShipVfxConfigFromPath(const char* absoluteFilePath)
{
    if (absoluteFilePath == nullptr || absoluteFilePath[0] == '\0')
    {
        return false;
    }

    std::ifstream input(absoluteFilePath, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        return false;
    }
    const std::streamsize size = input.tellg();
    if (size <= 0)
    {
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(size) + 1U, '\0');
    if (!input.read(bytes.data(), size))
    {
        return false;
    }

    cJSON* root = cJSON_Parse(bytes.data());
    if (root == nullptr)
    {
        return false;
    }

    const cJSON* formatNode = cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON* versionNode = cJSON_GetObjectItemCaseSensitive(root, "version");
    const bool isNewFormat =
        cJSON_IsString(formatNode) &&
        formatNode->valuestring != nullptr &&
        SDL_strcasecmp(formatNode->valuestring, "ship_vfx_config") == 0;
    const int formatVersion = cJSON_IsNumber(versionNode)
        ? static_cast<int>(std::llround(versionNode->valuedouble))
        : 1;

    this->shipVfxInstances.clear();
    this->setSelectedVfxInstanceIndex(-1);
    this->nextVfxInstanceId = 1U;

    auto findImportedSfxIndex = [this](const std::string& sourceJsonPath, const std::string& displayName) -> int {
        const std::string sourceKey = makePathKeyLower(sourceJsonPath);
        if (!sourceKey.empty())
        {
            for (size_t i = 0; i < this->importedSfx.size(); ++i)
            {
                if (makePathKeyLower(this->importedSfx[i].sourceJsonPath) == sourceKey)
                {
                    return static_cast<int>(i);
                }
            }
        }

        const std::string displayKey = makePathKeyLower(displayName);
        if (!displayKey.empty())
        {
            for (size_t i = 0; i < this->importedSfx.size(); ++i)
            {
                if (makePathKeyLower(this->importedSfx[i].displayName) == displayKey)
                {
                    return static_cast<int>(i);
                }
            }
        }
        return -1;
    };

    auto parseDirectionOverride = [](const cJSON* node, DirectionOverride* out) {
        if (node == nullptr || out == nullptr || !cJSON_IsObject(node))
        {
            return;
        }

        const cJSON* enabledNode = cJSON_GetObjectItemCaseSensitive(node, "enabled");
        out->enabled = cJSON_IsBool(enabledNode) ? cJSON_IsTrue(enabledNode) : out->enabled;

        const cJSON* offsetXNode = cJSON_GetObjectItemCaseSensitive(node, "offsetX");
        const cJSON* offsetYNode = cJSON_GetObjectItemCaseSensitive(node, "offsetY");
        const cJSON* rotationNode = cJSON_GetObjectItemCaseSensitive(node, "rotationDeg");
        const cJSON* flipHNode = cJSON_GetObjectItemCaseSensitive(node, "flipHorizontal");
        const cJSON* flipVNode = cJSON_GetObjectItemCaseSensitive(node, "flipVertical");
        const cJSON* drawOrderNode = cJSON_GetObjectItemCaseSensitive(node, "drawOrder");
        const cJSON* visibleNode = cJSON_GetObjectItemCaseSensitive(node, "visible");

        if (cJSON_IsNumber(offsetXNode) && std::isfinite(offsetXNode->valuedouble))
        {
            out->offsetX = static_cast<float>(offsetXNode->valuedouble);
        }
        if (cJSON_IsNumber(offsetYNode) && std::isfinite(offsetYNode->valuedouble))
        {
            out->offsetY = static_cast<float>(offsetYNode->valuedouble);
        }
        if (cJSON_IsNumber(rotationNode) && std::isfinite(rotationNode->valuedouble))
        {
            out->rotationDeg = static_cast<float>(rotationNode->valuedouble);
        }
        if (cJSON_IsBool(flipHNode))
        {
            out->flipHorizontal = cJSON_IsTrue(flipHNode);
        }
        if (cJSON_IsBool(flipVNode))
        {
            out->flipVertical = cJSON_IsTrue(flipVNode);
        }
        if (cJSON_IsNumber(drawOrderNode) && std::isfinite(drawOrderNode->valuedouble))
        {
            out->drawOrder = static_cast<int>(std::llround(drawOrderNode->valuedouble));
        }
        if (cJSON_IsBool(visibleNode))
        {
            out->visible = cJSON_IsTrue(visibleNode);
        }
    };

    auto pushParsedInstance = [this, &findImportedSfxIndex, &parseDirectionOverride](const cJSON* node) {
        if (node == nullptr || !cJSON_IsObject(node))
        {
            return;
        }

        ShipVfxInstance instance{};
        const cJSON* idNode = cJSON_GetObjectItemCaseSensitive(node, "instanceId");
        if (cJSON_IsNumber(idNode) && std::isfinite(idNode->valuedouble))
        {
            instance.instanceId = static_cast<uint32_t>((std::max)(1LL, static_cast<long long>(std::llround(idNode->valuedouble))));
        }
        else
        {
            instance.instanceId = this->nextVfxInstanceId++;
        }

        const cJSON* labelNode = cJSON_GetObjectItemCaseSensitive(node, "label");
        const cJSON* displayNode = cJSON_GetObjectItemCaseSensitive(node, "displayName");
        if (cJSON_IsString(labelNode) && labelNode->valuestring != nullptr)
        {
            instance.label = labelNode->valuestring;
        }
        else if (cJSON_IsString(displayNode) && displayNode->valuestring != nullptr)
        {
            instance.label = displayNode->valuestring;
        }
        const cJSON* sourceJsonNode = cJSON_GetObjectItemCaseSensitive(node, "sourceJsonPath");
        if (cJSON_IsString(sourceJsonNode) && sourceJsonNode->valuestring != nullptr)
        {
            instance.sourceJsonPath = normalizePathSlashes(sourceJsonNode->valuestring);
        }
        instance.sourceDisplayName = instance.label;
        if (cJSON_IsString(displayNode) && displayNode->valuestring != nullptr)
        {
            instance.sourceDisplayName = displayNode->valuestring;
        }
        instance.importedSfxIndex = findImportedSfxIndex(instance.sourceJsonPath, instance.sourceDisplayName);
        if (instance.label.empty())
        {
            int sourceInstanceNumber = 1;
            for (const ShipVfxInstance& existing : this->shipVfxInstances)
            {
                if (existing.importedSfxIndex == instance.importedSfxIndex)
                {
                    sourceInstanceNumber += 1;
                }
            }
            const int sourceNumber = (std::max)(1, instance.importedSfxIndex + 1);
            instance.label = makeDefaultVfxLayerLabel(instance.sourceDisplayName, sourceNumber, sourceInstanceNumber);
        }

        const cJSON* sharedDirNode = cJSON_GetObjectItemCaseSensitive(node, "sharedForAllDirections");
        const cJSON* sharedStateNode = cJSON_GetObjectItemCaseSensitive(node, "sharedForAllStates");
        instance.sharedForAllDirections = cJSON_IsBool(sharedDirNode) ? cJSON_IsTrue(sharedDirNode) : true;
        instance.sharedForAllStates = cJSON_IsBool(sharedStateNode) ? cJSON_IsTrue(sharedStateNode) : true;

        const cJSON* offsetXNode = cJSON_GetObjectItemCaseSensitive(node, "offsetX");
        const cJSON* offsetYNode = cJSON_GetObjectItemCaseSensitive(node, "offsetY");
        const cJSON* rotationNode = cJSON_GetObjectItemCaseSensitive(node, "rotationDeg");
        const cJSON* flipHNode = cJSON_GetObjectItemCaseSensitive(node, "flipHorizontal");
        const cJSON* flipVNode = cJSON_GetObjectItemCaseSensitive(node, "flipVertical");
        const cJSON* drawOrderNode = cJSON_GetObjectItemCaseSensitive(node, "drawOrder");
        const cJSON* visibleNode = cJSON_GetObjectItemCaseSensitive(node, "visible");
        const cJSON* debugBoundsNode = cJSON_GetObjectItemCaseSensitive(node, "debugBoundsVisible");
        const cJSON* lockedNode = cJSON_GetObjectItemCaseSensitive(node, "locked");
        const cJSON* behindNode = cJSON_GetObjectItemCaseSensitive(node, "behindShip");

        instance.offsetX = cJSON_IsNumber(offsetXNode) ? static_cast<float>(offsetXNode->valuedouble) : 0.0f;
        instance.offsetY = cJSON_IsNumber(offsetYNode) ? static_cast<float>(offsetYNode->valuedouble) : 0.0f;
        instance.rotationDeg = cJSON_IsNumber(rotationNode) ? static_cast<float>(rotationNode->valuedouble) : 0.0f;
        instance.flipHorizontal = cJSON_IsBool(flipHNode) ? cJSON_IsTrue(flipHNode) : false;
        instance.flipVertical = cJSON_IsBool(flipVNode) ? cJSON_IsTrue(flipVNode) : false;
        instance.drawOrder = cJSON_IsNumber(drawOrderNode) ? static_cast<int>(std::llround(drawOrderNode->valuedouble)) : 0;
        instance.visible = cJSON_IsBool(visibleNode) ? cJSON_IsTrue(visibleNode) : true;
        instance.debugBoundsVisible = cJSON_IsBool(debugBoundsNode) ? cJSON_IsTrue(debugBoundsNode) : true;
        instance.locked = cJSON_IsBool(lockedNode) ? cJSON_IsTrue(lockedNode) : false;
        instance.behindShip = cJSON_IsBool(behindNode) ? cJSON_IsTrue(behindNode) : (instance.drawOrder < this->shipDrawOrder);
        instance.followShip = true;

        for (DirectionOverride& override : instance.directionOverrides)
        {
            override.enabled = false;
            override.offsetX = instance.offsetX;
            override.offsetY = instance.offsetY;
            override.rotationDeg = instance.rotationDeg;
            override.flipHorizontal = instance.flipHorizontal;
            override.flipVertical = instance.flipVertical;
            override.drawOrder = instance.drawOrder;
            override.visible = instance.visible;
        }

        const cJSON* directionOverridesNode = cJSON_GetObjectItemCaseSensitive(node, "directionOverrides");
        if (cJSON_IsObject(directionOverridesNode))
        {
            for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
            {
                const char* key = getDirectionIdByIndex(directionIndex);
                const cJSON* directionNode = cJSON_GetObjectItemCaseSensitive(directionOverridesNode, key);
                parseDirectionOverride(directionNode, &instance.directionOverrides[static_cast<size_t>(directionIndex)]);
            }
        }

        this->nextVfxInstanceId = (std::max)(this->nextVfxInstanceId, instance.instanceId + 1U);
        this->shipVfxInstances.push_back(std::move(instance));
    };

    if (isNewFormat && formatVersion >= 2)
    {
        const cJSON* previewNode = cJSON_GetObjectItemCaseSensitive(root, "preview");
        if (cJSON_IsObject(previewNode))
        {
            const cJSON* directionNode = cJSON_GetObjectItemCaseSensitive(previewNode, "direction");
            const cJSON* stateNode = cJSON_GetObjectItemCaseSensitive(previewNode, "state");
            if (cJSON_IsString(directionNode) && directionNode->valuestring != nullptr)
            {
                this->previewDirectionIndex = getDirectionIndexById(directionNode->valuestring);
                this->applyPreviewDirectionToShip();
            }
            if (cJSON_IsString(stateNode) && stateNode->valuestring != nullptr)
            {
                this->previewShipStateIndex = (SDL_strcasecmp(stateNode->valuestring, "damaged") == 0) ? 1 : 0;
                this->previewShip.setHealthVisual((this->previewShipStateIndex == 1) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
            }
        }

        const cJSON* shipLayerNode = cJSON_GetObjectItemCaseSensitive(root, "shipLayer");
        if (cJSON_IsObject(shipLayerNode))
        {
            const cJSON* shipOrderNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "drawOrder");
            const cJSON* shipVisibleNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "visible");
            const cJSON* shipLockedNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "locked");
            const cJSON* shipDebugNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "debugBoundsVisible");
            if (cJSON_IsNumber(shipOrderNode))
            {
                this->shipDrawOrder = static_cast<int>(std::llround(shipOrderNode->valuedouble));
            }
            if (cJSON_IsBool(shipVisibleNode))
            {
                this->shipLayerVisible = cJSON_IsTrue(shipVisibleNode);
            }
            if (cJSON_IsBool(shipLockedNode))
            {
                this->shipLayerLocked = cJSON_IsTrue(shipLockedNode);
            }
            if (cJSON_IsBool(shipDebugNode))
            {
                this->shipDebugBoundsVisible = cJSON_IsTrue(shipDebugNode);
            }
        }

        const cJSON* instancesNode = cJSON_GetObjectItemCaseSensitive(root, "vfxInstances");
        if (cJSON_IsArray(instancesNode))
        {
            cJSON* node = nullptr;
            cJSON_ArrayForEach(node, instancesNode)
            {
                pushParsedInstance(node);
            }
        }
    }
    else
    {
        const cJSON* shipOrderNode = cJSON_GetObjectItemCaseSensitive(root, "shipDrawOrder");
        if (cJSON_IsNumber(shipOrderNode))
        {
            this->shipDrawOrder = static_cast<int>(std::llround(shipOrderNode->valuedouble));
        }
        const cJSON* instancesNode = cJSON_GetObjectItemCaseSensitive(root, "vfxInstances");
        if (cJSON_IsArray(instancesNode))
        {
            cJSON* node = nullptr;
            cJSON_ArrayForEach(node, instancesNode)
            {
                pushParsedInstance(node);
            }
        }
    }

    cJSON_Delete(root);
    this->normalizeShipVfxDrawOrders();
    if (!this->shipVfxInstances.empty())
    {
        this->setSelectedVfxInstanceIndex(0);
    }
    this->shipVfxDirty = false;
    return true;
}

bool EditorMapVfxScene::loadShipVfxConfigForSelectedShip(void)
{
    if (this->selectedShipIndex < 0 || this->selectedShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        return false;
    }

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    std::string configPath = ship.configJsonPath;
    if (configPath.empty())
    {
        configPath = this->buildShipConfigJsonPath(ship);
    }

    std::error_code fsError;
    if (!std::filesystem::exists(configPath, fsError) || !std::filesystem::is_regular_file(configPath, fsError))
    {
        const std::filesystem::path legacyPath = std::filesystem::path(ship.folderAbsolutePath) / "ship_vfx.json";
        if (std::filesystem::exists(legacyPath, fsError) && std::filesystem::is_regular_file(legacyPath, fsError))
        {
            configPath = normalizePathSlashes(legacyPath.string());
        }
        else
        {
            this->shipVfxInstances.clear();
            this->setSelectedVfxInstanceIndex(-1);
            this->nextVfxInstanceId = 1U;
            this->shipDrawOrder = 0;
            this->shipDebugBoundsVisible = true;
            this->shipVfxDirty = false;
            this->loadedShipVfxConfigPath = normalizePathSlashes(configPath);
            return false;
        }
    }

    const bool loaded = this->importShipVfxConfigFromPath(configPath.c_str());
    if (loaded)
    {
        this->loadedShipVfxConfigPath = normalizePathSlashes(configPath);
        this->statusMessage = "Configuration VFX chargee: " + this->loadedShipVfxConfigPath;
        return true;
    }

    this->statusMessage = "JSON VFX present mais invalide: " + configPath;
    return false;
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

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    const std::string shipSlug = makeShipConfigSlug(ship.displayName);
    const std::filesystem::path jsonPath = folderPath / ("animations_vfx_" + shipSlug + ".json");

    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON.";
        return false;
    }

    cJSON_AddStringToObject(root, "format", "ship_vfx_config");
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "mode", "ship_vfx");

    cJSON* shipNode = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ship", shipNode);
    cJSON_AddStringToObject(shipNode, "id", shipSlug.c_str());
    cJSON_AddStringToObject(shipNode, "displayName", ship.displayName.c_str());
    cJSON_AddStringToObject(shipNode, "folderAbsolutePath", ship.folderAbsolutePath.c_str());

    cJSON* previewNode = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "preview", previewNode);
    cJSON_AddStringToObject(previewNode, "direction", getDirectionIdByIndex(this->previewDirectionIndex));
    cJSON_AddStringToObject(previewNode, "state", (this->previewShipStateIndex == 1) ? "damaged" : "healthy");

    cJSON* shipLayerNode = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "shipLayer", shipLayerNode);
    cJSON_AddNumberToObject(shipLayerNode, "drawOrder", this->shipDrawOrder);
    cJSON_AddBoolToObject(shipLayerNode, "visible", this->shipLayerVisible);
    cJSON_AddBoolToObject(shipLayerNode, "locked", this->shipLayerLocked);
    cJSON_AddBoolToObject(shipLayerNode, "debugBoundsVisible", this->shipDebugBoundsVisible);

    cJSON* instancesArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "vfxInstances", instancesArray);

    for (const ShipVfxInstance& instance : this->shipVfxInstances)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "instanceId", static_cast<double>(instance.instanceId));
        cJSON_AddStringToObject(item, "label", instance.label.c_str());
        cJSON_AddStringToObject(item, "displayName", instance.sourceDisplayName.c_str());
        cJSON_AddStringToObject(item, "sourceJsonPath", instance.sourceJsonPath.c_str());
        cJSON_AddNumberToObject(item, "offsetX", instance.offsetX);
        cJSON_AddNumberToObject(item, "offsetY", instance.offsetY);
        cJSON_AddNumberToObject(item, "rotationDeg", instance.rotationDeg);
        cJSON_AddBoolToObject(item, "flipHorizontal", instance.flipHorizontal);
        cJSON_AddBoolToObject(item, "flipVertical", instance.flipVertical);
        cJSON_AddNumberToObject(item, "drawOrder", instance.drawOrder);
        cJSON_AddBoolToObject(item, "visible", instance.visible);
        cJSON_AddBoolToObject(item, "debugBoundsVisible", instance.debugBoundsVisible);
        cJSON_AddBoolToObject(item, "locked", instance.locked);
        cJSON_AddBoolToObject(item, "behindShip", instance.behindShip);
        cJSON_AddBoolToObject(item, "followShip", instance.followShip);
        cJSON_AddBoolToObject(item, "sharedForAllDirections", instance.sharedForAllDirections);
        cJSON_AddBoolToObject(item, "sharedForAllStates", instance.sharedForAllStates);

        cJSON* directionOverridesNode = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "directionOverrides", directionOverridesNode);
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
        {
            const DirectionOverride& override = instance.directionOverrides[static_cast<size_t>(directionIndex)];
            cJSON* directionNode = cJSON_CreateObject();
            cJSON_AddBoolToObject(directionNode, "enabled", override.enabled);
            cJSON_AddNumberToObject(directionNode, "offsetX", override.offsetX);
            cJSON_AddNumberToObject(directionNode, "offsetY", override.offsetY);
            cJSON_AddNumberToObject(directionNode, "rotationDeg", override.rotationDeg);
            cJSON_AddBoolToObject(directionNode, "flipHorizontal", override.flipHorizontal);
            cJSON_AddBoolToObject(directionNode, "flipVertical", override.flipVertical);
            cJSON_AddNumberToObject(directionNode, "drawOrder", override.drawOrder);
            cJSON_AddBoolToObject(directionNode, "visible", override.visible);
            cJSON_AddItemToObject(directionOverridesNode, getDirectionIdByIndex(directionIndex), directionNode);
        }

        cJSON_AddItemToArray(instancesArray, item);
    }

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);
    if (jsonText == nullptr)
    {
        this->statusMessage = "Echec serialisation JSON.";
        return false;
    }

    std::ofstream output(jsonPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        cJSON_free(jsonText);
        this->statusMessage = "Impossible d'ouvrir le JSON export.";
        return false;
    }
    output.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
    const bool ok = output.good();
    output.close();
    cJSON_free(jsonText);

    if (!ok)
    {
        this->statusMessage = "Ecriture JSON export echouee.";
        return false;
    }

    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath = normalizePathSlashes(jsonPath.string());
    this->statusMessage = "Export OK: " + this->loadedShipVfxConfigPath;
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
    const std::string exportSpritesFolderName = exportPrefix + "-sprites";
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

    std::vector<SDL_Surface*> sourceSurfaces;
    sourceSurfaces.reserve(orderedSpriteIndices.size());
    for (size_t orderedIndex = 0; orderedIndex < orderedSpriteIndices.size(); ++orderedIndex)
    {
        const ImportedLooseSprite& sprite =
            folder.sprites[orderedSpriteIndices[orderedIndex]];
        RC2D_ImageData src = rc2d_graphics_loadImageDataFromStorage(sprite.storagePath.c_str(), RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            cleanupExportedFrames();
            this->statusMessage = "Sprite source manquant pour export.";
            return false;
        }
        sourceSurfaces.push_back(src.sdl_surface);
        src.sdl_surface = nullptr;
        rc2d_graphics_freeImageData(&src);
    }

    int cropX = 0;
    int cropY = 0;
    int cropW = 1;
    int cropH = 1;
    computeOpaqueUnionCropRectFromSurfaces(sourceSurfaces, &cropX, &cropY, &cropW, &cropH);

    for (size_t orderedIndex = 0; orderedIndex < orderedSpriteIndices.size(); ++orderedIndex)
    {
        const ImportedLooseSprite& sprite =
            folder.sprites[orderedSpriteIndices[orderedIndex]];
        SDL_Surface* rawSource = sourceSurfaces[orderedIndex];
        if (rawSource == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            cleanupExportedFrames();
            this->statusMessage = "Sprite source interne manquant pour export.";
            return false;
        }

        SDL_Surface* cropped = createCroppedSurfaceFromSource(rawSource, cropX, cropY, cropW, cropH);
        if (cropped == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            cleanupExportedFrames();
            this->statusMessage = "Rognage export KO.";
            return false;
        }

        const int srcW = cropW;
        const int srcH = cropH;
        const int dstW = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcW) * clampedPercent) / 100.0)));
        const int dstH = (std::max)(1, static_cast<int>(std::lround((static_cast<double>(srcH) * clampedPercent) / 100.0)));
        SDL_Surface* dst = SDL_CreateSurface(dstW, dstH, SDL_PIXELFORMAT_RGBA32);
        if (dst == nullptr)
        {
            SDL_DestroySurface(cropped);
            destroySurfaceVector(sourceSurfaces);
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
                SDL_ReadSurfacePixel(cropped, srcX, srcY, &r, &g, &b, &a);
                SDL_WriteSurfacePixel(dst, px, py, r, g, b, a);
            }
        }
        SDL_DestroySurface(cropped);

        const std::filesystem::path dstPath = outputSpritesFolder / sprite.fileName;
        if (!SDL_SavePNG(dst, dstPath.string().c_str()))
        {
            SDL_DestroySurface(dst);
            destroySurfaceVector(sourceSurfaces);
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

    destroySurfaceVector(sourceSurfaces);

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

    const std::string sheetFileName = exportPrefix + "-spritesheet.png";
    const std::filesystem::path sheetPath = outputFolder / sheetFileName;
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
    cJSON_AddStringToObject(jsonRoot, "image", sheetFileName.c_str());

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
    SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    float shipCenterOffsetX = 0.0f;
    float shipCenterOffsetY = 0.0f;
    if (this->previewShip.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
    {
        shipCenter.x += shipCenterOffsetX;
        shipCenter.y += shipCenterOffsetY;
    }
    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;

    std::vector<RenderItem> items;
    items.reserve(this->shipVfxInstances.size() + 1U);
    if (this->shipLayerVisible)
    {
        items.push_back(RenderItem{true, this->shipDrawOrder, 0U, -1});
    }
    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const bool visible = (override != nullptr) ? override->visible : instance.visible;
        if (!visible)
        {
            continue;
        }
        const int drawOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
        items.push_back(RenderItem{false, drawOrder, instance.instanceId, static_cast<int>(i)});
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
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const float resolvedOffsetX = (override != nullptr) ? override->offsetX : instance.offsetX;
        const float resolvedOffsetY = (override != nullptr) ? override->offsetY : instance.offsetY;
        const float resolvedRotation = (override != nullptr) ? override->rotationDeg : instance.rotationDeg;
        const bool resolvedFlipH = (override != nullptr) ? override->flipHorizontal : instance.flipHorizontal;
        const bool resolvedFlipV = (override != nullptr) ? override->flipVertical : instance.flipVertical;
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }

        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        if (imported.image.sdl_texture == nullptr || imported.frames.empty())
        {
            continue;
        }

        const int frameCount = static_cast<int>(imported.frames.size());
        const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(imported.defaultFps, 1.0f))) % frameCount;
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];

        const float sourceW = frame.w;
        const float sourceH = frame.h;
        const float offsetX = resolvedOffsetX * scale;
        const float offsetY = resolvedOffsetY * scale;
        const float drawScaleX = scale;
        const float drawScaleY = scale;
        const float sourceLeft = shipCenter.x + offsetX - ((sourceW * drawScaleX) * 0.5f);
        const float sourceTop = shipCenter.y + offsetY - ((sourceH * drawScaleY) * 0.5f);
        const float drawX = sourceLeft;
        const float drawY = sourceTop;
        const float pivotX = sourceW * 0.5f;
        const float pivotY = sourceH * 0.5f;

        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&imported.image),
            frame.x,
            frame.y,
            frame.w,
            frame.h);

        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&imported.image),
            &sourceQuad,
            drawX,
            drawY,
            resolvedRotation,
            drawScaleX,
            drawScaleY,
            pivotX,
            pivotY,
            resolvedFlipH,
            resolvedFlipV);
    }

    if (this->shipLayerVisible && this->shipDebugBoundsVisible && this->previewShipLoaded)
    {
        float shipSpriteW = 0.0f;
        float shipSpriteH = 0.0f;
        if (this->previewShip.getCurrentSpriteSizePixels(&shipSpriteW, &shipSpriteH))
        {
            const float shipScale = (std::max)(GetCamera().getZoomFactor(), 0.01f) * this->previewShip.getDrawScale();
            SDL_FRect shipBounds{};
            shipBounds.x = shipCenter.x - ((shipSpriteW * shipScale) * 0.5f);
            shipBounds.y = shipCenter.y - ((shipSpriteH * shipScale) * 0.5f);
            shipBounds.w = shipSpriteW * shipScale;
            shipBounds.h = shipSpriteH * shipScale;

            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            rc2d_graphics_setColor(RC2D_Color{128, 210, 255, 220});
            rc2d_graphics_rectangle("line", &shipBounds);
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        }
    }

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)];
        if (!instance.debugBoundsVisible)
        {
            return;
        }
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        if (instance.importedSfxIndex >= 0 && instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
            if (!imported.frames.empty())
            {
                const int frameCount = static_cast<int>(imported.frames.size());
                const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(imported.defaultFps, 1.0f))) % frameCount;
                const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
                const float sourceW = frame.w;
                const float sourceH = frame.h;
                const float offsetX = ((override != nullptr) ? override->offsetX : instance.offsetX) * scale;
                const float offsetY = ((override != nullptr) ? override->offsetY : instance.offsetY) * scale;
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
    const bool useUnionCropForSpritesheet =
        this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET && folder.looseUnionCropReady &&
        folder.looseUnionCropW > 0 && folder.looseUnionCropH > 0;

    float maxSpriteW = 1.0f;
    float maxSpriteH = 1.0f;
    if (useUnionCropForSpritesheet)
    {
        maxSpriteW = static_cast<float>(folder.looseUnionCropW);
        maxSpriteH = static_cast<float>(folder.looseUnionCropH);
    }
    else
    {
        for (const ImportedLooseSprite& sprite : folder.sprites)
        {
            maxSpriteW = (std::max)(maxSpriteW, sprite.widthPx);
            maxSpriteH = (std::max)(maxSpriteH, sprite.heightPx);
        }
    }

    const int spriteCount = static_cast<int>(folder.sprites.size());
    const int columns = (std::max)(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(spriteCount)))));
    const int rows = (std::max)(1, static_cast<int>(std::ceil(static_cast<float>(spriteCount) / static_cast<float>(columns))));
    const float requestedScale = static_cast<float>(this->looseScalePercent) / 100.0f;
    const float zoomMultiplier = std::clamp(GetCamera().getZoomFactor(), kLoosePreviewZoomMin, kLoosePreviewZoomMax);
    const float previewScale = requestedScale * zoomMultiplier;
    const float cellPadding = 18.0f * zoomMultiplier;
    const float contentW = maxSpriteW * previewScale;
    const float contentH = maxSpriteH * previewScale;
    const float cellStrideW = contentW + cellPadding;
    const float cellStrideH = contentH + cellPadding;
    const int gapCountW = (std::max)(0, columns - 1);
    const int gapCountH = (std::max)(0, rows - 1);
    const float gridW = static_cast<float>(columns) * contentW + static_cast<float>(gapCountW) * cellPadding;
    const float gridH = static_cast<float>(rows) * contentH + static_cast<float>(gapCountH) * cellPadding;
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
        const float cellX = startX + (static_cast<float>(col) * cellStrideW);
        const float cellY = startY + (static_cast<float>(row) * cellStrideH);
        RC2D_Image* spriteImage = const_cast<RC2D_Image*>(&sprite.image);
        if (useUnionCropForSpritesheet)
        {
            const int cropX = folder.looseUnionCropX;
            const int cropY = folder.looseUnionCropY;
            const int cropW = folder.looseUnionCropW;
            const int cropH = folder.looseUnionCropH;
            const int tw = static_cast<int>(std::lround(sprite.widthPx));
            const int th = static_cast<int>(std::lround(sprite.heightPx));
            const int interX1 = (std::max)(cropX, 0);
            const int interY1 = (std::max)(cropY, 0);
            const int interX2 = (std::min)(cropX + cropW, tw);
            const int interY2 = (std::min)(cropY + cropH, th);
            if (interX2 <= interX1 || interY2 <= interY1)
            {
                continue;
            }
            const float subX = static_cast<float>(interX1);
            const float subY = static_cast<float>(interY1);
            const float subW = static_cast<float>(interX2 - interX1);
            const float subH = static_cast<float>(interY2 - interY1);
            const float drawX = cellX + (static_cast<float>(interX1 - cropX) * previewScale);
            const float drawY = cellY + (static_cast<float>(interY1 - cropY) * previewScale);
            const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(spriteImage, subX, subY, subW, subH);
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
        else
        {
            const float cellCenterX = cellX + (contentW * 0.5f);
            const float cellCenterY = cellY + (contentH * 0.5f);
            const float drawW = sprite.widthPx * previewScale;
            const float drawH = sprite.heightPx * previewScale;
            const float drawX = cellCenterX - (drawW * 0.5f);
            const float drawY = cellCenterY - (drawH * 0.5f);
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

    if (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
    {
        const float linePx = (std::max)(1.0f, zoomMultiplier);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(kLooseSpritesheetGridColor);

        // Cadre exterieur en bandes pleines *autour* de la zone utile (evite le contour "line"
        // qui mange l'interieur + ne superpose pas les sprites sur les bords).
        const float outerPad = linePx;
        const SDL_FRect outerLeft = SDL_FRect{startX - outerPad, startY - outerPad, linePx, gridH + 2.0f * outerPad};
        const SDL_FRect outerRight = SDL_FRect{startX + gridW, startY - outerPad, linePx, gridH + 2.0f * outerPad};
        const SDL_FRect outerTop = SDL_FRect{startX - outerPad, startY - outerPad, gridW + 2.0f * outerPad, linePx};
        const SDL_FRect outerBottom = SDL_FRect{startX - outerPad, startY + gridH, gridW + 2.0f * outerPad, linePx};
        rc2d_graphics_rectangle("fill", &outerLeft);
        rc2d_graphics_rectangle("fill", &outerRight);
        rc2d_graphics_rectangle("fill", &outerTop);
        rc2d_graphics_rectangle("fill", &outerBottom);

        // Lignes interieures au milieu des gouttieres entre cases (pas sur le bord du contenu).
        for (int c = 1; c < columns; ++c)
        {
            const float vx =
                startX + static_cast<float>(c) * contentW + (static_cast<float>(c) - 0.5f) * cellPadding;
            const SDL_FRect vLine = SDL_FRect{vx - linePx * 0.5f, startY, linePx, gridH};
            rc2d_graphics_rectangle("fill", &vLine);
        }
        for (int r = 1; r < rows; ++r)
        {
            const float hy =
                startY + static_cast<float>(r) * contentH + (static_cast<float>(r) - 0.5f) * cellPadding;
            const SDL_FRect hLine = SDL_FRect{startX, hy - linePx * 0.5f, gridW, linePx};
            rc2d_graphics_rectangle("fill", &hLine);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
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

    const bool useUnionCrop =
        folder.looseUnionCropReady && folder.looseUnionCropW > 0 && folder.looseUnionCropH > 0;
    float layoutRefW = 1.0f;
    float layoutRefH = 1.0f;
    if (useUnionCrop)
    {
        layoutRefW = static_cast<float>(folder.looseUnionCropW);
        layoutRefH = static_cast<float>(folder.looseUnionCropH);
    }
    else
    {
        for (const ImportedLooseSprite& s : folder.sprites)
        {
            layoutRefW = (std::max)(layoutRefW, s.widthPx);
            layoutRefH = (std::max)(layoutRefH, s.heightPx);
        }
    }

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

        // Boite de reference (union rognee ou max des tailles), centree sur la tuile : meme logique que le
        // mode spritesheet, pour que le pourcentage ne deplace pas l'anim (centre stable) et que les frames
        // ne sautent pas quand les PNG ont des tailles differentes.
        const float boxLeft = screenPos.x - (layoutRefW * previewScale) * 0.5f;
        const float boxTop = screenPos.y - (layoutRefH * previewScale) * 0.5f;

        float subX = 0.0f;
        float subY = 0.0f;
        float subW = sprite.widthPx;
        float subH = sprite.heightPx;
        if (useUnionCrop)
        {
            const int cropX = folder.looseUnionCropX;
            const int cropY = folder.looseUnionCropY;
            const int cropW = folder.looseUnionCropW;
            const int cropH = folder.looseUnionCropH;
            const int tw = static_cast<int>(std::lround(sprite.widthPx));
            const int th = static_cast<int>(std::lround(sprite.heightPx));
            const int interX1 = (std::max)(cropX, 0);
            const int interY1 = (std::max)(cropY, 0);
            const int interX2 = (std::min)(cropX + cropW, tw);
            const int interY2 = (std::min)(cropY + cropH, th);
            if (interX2 <= interX1 || interY2 <= interY1)
            {
                return;
            }
            subX = static_cast<float>(interX1);
            subY = static_cast<float>(interY1);
            subW = static_cast<float>(interX2 - interX1);
            subH = static_cast<float>(interY2 - interY1);
        }

        float drawX = 0.0f;
        float drawY = 0.0f;
        if (useUnionCrop)
        {
            const int cropX = folder.looseUnionCropX;
            const int cropY = folder.looseUnionCropY;
            drawX = boxLeft + (subX - static_cast<float>(cropX)) * previewScale;
            drawY = boxTop + (subY - static_cast<float>(cropY)) * previewScale;
        }
        else
        {
            drawX = boxLeft + ((layoutRefW - sprite.widthPx) * previewScale) * 0.5f;
            drawY = boxTop + ((layoutRefH - sprite.heightPx) * previewScale) * 0.5f;
        }

        const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(spriteImage, subX, subY, subW, subH);

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
        if (this->loosePreviewPlacementSnapToTile)
        {
            const SDL_Point hoveredTile = map.screenToTileNearest(mouseX, mouseY);
            drawAnimatedAtTile(
                static_cast<float>(hoveredTile.x),
                static_cast<float>(hoveredTile.y),
                this->getLoosePreviewFpsOrDefault());
        }
        else
        {
            const SDL_FPoint hoveredTileF = map.screenToTile(mouseX, mouseY);
            drawAnimatedAtTile(hoveredTileF.x, hoveredTileF.y, this->getLoosePreviewFpsOrDefault());
        }
    }
}

std::vector<int> EditorMapVfxScene::getOrderedVfxInstanceIndicesForLayerPanel(void) const
{
    struct LayerEntry
    {
        int index; // -1 = ship, sinon index d'instance VFX
        int drawOrder;
        uint32_t stableId;
    };

    std::vector<LayerEntry> entries;
    entries.reserve(this->shipVfxInstances.size() + 1U);
    entries.push_back(LayerEntry{-1, this->shipDrawOrder, 0U});
    for (size_t i = 0; i < this->shipVfxInstances.size(); ++i)
    {
        const ShipVfxInstance& instance = this->shipVfxInstances[i];
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const int drawOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
        entries.push_back(LayerEntry{static_cast<int>(i), drawOrder, instance.instanceId});
    }

    std::sort(entries.begin(), entries.end(), [](const LayerEntry& a, const LayerEntry& b) {
        if (a.drawOrder != b.drawOrder)
        {
            return a.drawOrder < b.drawOrder;
        }
        // Ship avant VFX en cas d'egalite de z.
        if ((a.index < 0) != (b.index < 0))
        {
            return a.index < 0;
        }
        return a.stableId < b.stableId;
    });

    std::vector<int> ordered;
    ordered.reserve(entries.size());
    for (const LayerEntry& entry : entries)
    {
        ordered.push_back(entry.index);
    }
    return ordered;
}

int EditorMapVfxScene::getSelectedLayerRowIndexForDisplay(const std::vector<int>& orderedInstanceIndices) const
{
    for (size_t i = 0; i < orderedInstanceIndices.size(); ++i)
    {
        if (this->shipLayerSelected && orderedInstanceIndices[i] < 0)
        {
            return static_cast<int>(i);
        }
        if (!this->shipLayerSelected && orderedInstanceIndices[i] == this->selectedVfxInstanceIndex)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void EditorMapVfxScene::drawHud(void) const
{
    const bool shipMode = (this->editorMode == EditorMode::SHIP_VFX);

    this->drawToolbarButton(this->buttonModeShipVfxRect, "MODE : SHIP / VFX", shipMode);
    this->drawToolbarButton(this->buttonModeLooseSpritesRect, "MODE : ROGNAGE DOWNSCALE FOR SPRITESHEET", !shipMode);
    this->drawToolbarButton(this->buttonExportRect, "EXPORTER", false);
    if (shipMode)
    {
        this->drawToolbarButton(this->buttonReloadAssetsRect, "RELOAD ASSETS", false);
    }
    this->drawToolbarButton(this->buttonOceanPrevRect, "OCEAN -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "OCEAN +", false);

    if (shipMode)
    {
        const ShipVfxInstance* selectedInstance =
            (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->shipVfxInstances.size()))
            ? &this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)]
            : nullptr;
        const DirectionOverride* selectedOverride = this->getResolvedDirectionOverride(selectedInstance);
        const bool selectedSharedDirection = (selectedInstance != nullptr) ? selectedInstance->sharedForAllDirections : true;
        const bool selectedOverrideEnabled = (selectedInstance != nullptr) && (selectedOverride != nullptr) && selectedOverride->enabled;

        this->drawToolbarButton(this->buttonDirectionPrevRect, "SPRITE NAVIRE DIRECTION -", false);
        this->drawToolbarButton(this->buttonDirectionNextRect, "SPRITE NAVIRE DIRECTION +", false);
        this->drawToolbarButton(this->buttonShipStateToggleRect, this->getPreviewShipStateLabel(), this->previewShipStateIndex == 1);
        this->drawToolbarButton(this->buttonRotateMinusRect, "ROT -", false);
        this->drawToolbarButton(this->buttonRotatePlusRect, "ROT +", false);

        this->drawToolbarButton(this->buttonFlipHorizontalRect, "FLIP H", false);
        this->drawToolbarButton(this->buttonFlipVerticalRect, "FLIP V", false);
        this->drawToolbarButton(this->buttonSharedDirectionsRect, "SHARED DIR", selectedSharedDirection);
        this->drawToolbarButton(this->buttonDirectionOverrideRect, "OVERRIDE DIR", selectedOverrideEnabled);
        this->drawToolbarButton(this->buttonCenterVfxRect, "CENTER VFX ON SHIP", false);

        std::vector<std::string> shipLabels;
        shipLabels.reserve(this->importedShips.size());
        for (const ImportedShip& ship : this->importedShips)
        {
            shipLabels.push_back(stripListPrefix(ship.displayName, "ship-"));
        }
        this->drawListPanel(this->shipListRect, "Navires", shipLabels, this->selectedShipIndex, this->shipListScrollOffset);

        std::vector<std::string> sfxLabels;
        sfxLabels.reserve(this->importedSfx.size());
        for (const ImportedSfx& sfx : this->importedSfx)
        {
            sfxLabels.push_back(stripListPrefix(sfx.displayName, "vfx-"));
        }
        this->drawListPanel(this->sfxListRect, "VFX", sfxLabels, this->selectedSfxIndex, this->sfxListScrollOffset);

        const std::vector<int> orderedLayerIndices = this->getOrderedVfxInstanceIndicesForLayerPanel();
        this->drawLayerListPanel(orderedLayerIndices);

        this->drawInvalidAssetPanel(
            this->invalidVfxListRect,
            "VFX ignores",
            this->invalidVfxFolders,
            this->invalidVfxListScrollOffset);
        this->drawInvalidAssetPanel(
            this->invalidShipListRect,
            "Ships ignores",
            this->invalidShipFolders,
            this->invalidShipListScrollOffset);
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
            this->drawToolbarButton(
                this->buttonLoosePreviewPlacementSnapRect,
                this->loosePreviewPlacementSnapToTile ? "PLACEMENT: TUILE" : "PLACEMENT: SOUS-PIXEL",
                false);
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
            "Mode Ship / VFX | Ship: %s | Direction: %s | State: %s | Instances: %d | ShipOrder: %d | Dirty: %s | Ocean: %s",
            shipName,
            this->getPreviewDirectionLabel(),
            this->getPreviewShipStateLabel(),
            static_cast<int>(this->shipVfxInstances.size()),
            this->shipDrawOrder,
            this->shipVfxDirty ? "YES" : "NO",
            oceanLabel);
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

bool EditorMapVfxScene::handleLayerListClick(float x, float y)
{
    if (!this->pointInRect(x, y, this->layerListRect))
    {
        return false;
    }

    const std::vector<int> orderedLayerIndices = this->getOrderedVfxInstanceIndicesForLayerPanel();
    const int itemCount = static_cast<int>(orderedLayerIndices.size());

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = 3.0f;
    const float rowsTopY = this->layerListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->layerListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = this->layerListRect.x + panelPadding;
    const float rowsWidth = this->layerListRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        int clickedIndexIgnored = -1;
        this->handleListPanelClick(
            x,
            y,
            this->layerListRect,
            itemCount,
            &this->layerListScrollOffset,
            &this->layerListScrollDragActive,
            &this->layerListScrollDragGrabOffsetY,
            &clickedIndexIgnored);
        this->layerRowDragActive = false;
        this->layerRowDragMoved = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragTargetInsertIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;
        return true;
    }

    int clickedIndex = -1;
    const int startIndex = this->computeListStartIndex(this->layerListScrollOffset, itemCount);
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
            clickedIndex = rowIndex;
            break;
        }
    }
    if (clickedIndex < 0)
    {
        this->layerRowDragActive = false;
        this->layerRowDragMoved = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragTargetInsertIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;
        return true;
    }

    const int clickedVisibleSlot = clickedIndex - startIndex;
    if (clickedVisibleSlot < 0 || clickedVisibleSlot >= kVisibleListRows)
    {
        return true;
    }

    SDL_FRect clickedRowRect{};
    clickedRowRect.x = rowsLeftX;
    clickedRowRect.y = rowsTopY + (static_cast<float>(clickedVisibleSlot) * (rowHeight + rowGap));
    clickedRowRect.w = rowsWidth;
    clickedRowRect.h = rowHeight;

    SDL_FRect lockRect{};
    lockRect.w = 40.0f;
    lockRect.h = clickedRowRect.h - 4.0f;
    lockRect.x = clickedRowRect.x + clickedRowRect.w - lockRect.w - 3.0f;
    lockRect.y = clickedRowRect.y + 2.0f;

    SDL_FRect visRect{};
    visRect.w = 52.0f;
    visRect.h = lockRect.h;
    visRect.x = lockRect.x - visRect.w - 4.0f;
    visRect.y = lockRect.y;

    SDL_FRect dupRect{};
    dupRect.w = 46.0f;
    dupRect.h = lockRect.h;
    dupRect.x = visRect.x - dupRect.w - 4.0f;
    dupRect.y = lockRect.y;

    SDL_FRect delRect{};
    delRect.w = 46.0f;
    delRect.h = lockRect.h;
    delRect.x = dupRect.x - delRect.w - 4.0f;
    delRect.y = lockRect.y;

    SDL_FRect resetRect{};
    resetRect.w = 58.0f;
    resetRect.h = lockRect.h;
    resetRect.x = delRect.x - resetRect.w - 4.0f;
    resetRect.y = lockRect.y;

    SDL_FRect debugRect{};
    debugRect.w = 66.0f;
    debugRect.h = lockRect.h;
    debugRect.x = resetRect.x - debugRect.w - 4.0f;
    debugRect.y = lockRect.y;

    auto clearLayerDragState = [this]() {
        this->layerRowDragActive = false;
        this->layerRowDragMoved = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragTargetInsertIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;
    };

    auto selectClickedRow = [this, &orderedLayerIndices, clickedIndex]() {
        if (clickedIndex < 0 || clickedIndex >= static_cast<int>(orderedLayerIndices.size()))
        {
            return;
        }
        const int rowValue = orderedLayerIndices[static_cast<size_t>(clickedIndex)];
        if (rowValue < 0)
        {
            this->shipLayerSelected = true;
            this->setSelectedVfxInstanceIndex(-1);
            return;
        }

        this->shipLayerSelected = false;
        this->setSelectedVfxInstanceIndex(rowValue);
    };

    const bool clickedIsShipRow =
        clickedIndex >= 0 &&
        clickedIndex < static_cast<int>(orderedLayerIndices.size()) &&
        orderedLayerIndices[static_cast<size_t>(clickedIndex)] < 0;
    const int clickedInstanceIndex =
        (!clickedIsShipRow && clickedIndex >= 0 && clickedIndex < static_cast<int>(orderedLayerIndices.size()))
        ? orderedLayerIndices[static_cast<size_t>(clickedIndex)]
        : -1;

    if (this->pointInRect(x, y, debugRect))
    {
        selectClickedRow();
        if (clickedIsShipRow || clickedInstanceIndex < 0)
        {
            this->shipDebugBoundsVisible = !this->shipDebugBoundsVisible;
            this->markShipVfxDirty();
            clearLayerDragState();
            this->statusMessage = this->shipDebugBoundsVisible ? "DEBUG SHIP active." : "DEBUG SHIP desactivee.";
            return true;
        }

        ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
        instance.debugBoundsVisible = !instance.debugBoundsVisible;
        this->markShipVfxDirty();
        clearLayerDragState();
        this->statusMessage = instance.debugBoundsVisible ? "DEBUG active." : "DEBUG desactivee.";
        return true;
    }

    if (this->pointInRect(x, y, resetRect))
    {
        selectClickedRow();
        if (clickedIsShipRow || clickedInstanceIndex < 0)
        {
            clearLayerDragState();
            this->statusMessage = "Reset non disponible pour SHIP.";
            return true;
        }

        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
        if (instance.locked)
        {
            clearLayerDragState();
            this->statusMessage = "Layer verrouille: reset refuse.";
            return true;
        }

        this->resetSelectedVfxTransform();
        clearLayerDragState();
        return true;
    }

    if (this->pointInRect(x, y, dupRect))
    {
        selectClickedRow();
        if (clickedIsShipRow || clickedInstanceIndex < 0)
        {
            clearLayerDragState();
            this->statusMessage = "Duplication non disponible pour SHIP.";
            return true;
        }

        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
        if (instance.locked)
        {
            clearLayerDragState();
            this->statusMessage = "Layer verrouille: duplication refusee.";
            return true;
        }

        this->duplicateSelectedVfxInstance();
        clearLayerDragState();
        return true;
    }

    if (this->pointInRect(x, y, delRect))
    {
        selectClickedRow();
        if (clickedIsShipRow || clickedInstanceIndex < 0)
        {
            clearLayerDragState();
            this->statusMessage = "Suppression non disponible pour SHIP.";
            return true;
        }

        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
        if (instance.locked)
        {
            clearLayerDragState();
            this->statusMessage = "Layer verrouille: suppression refusee.";
            return true;
        }

        this->removeSelectedVfxInstance();
        clearLayerDragState();
        return true;
    }

    if (this->pointInRect(x, y, visRect))
    {
        selectClickedRow();
        if (clickedIsShipRow)
        {
            this->shipLayerVisible = !this->shipLayerVisible;
            this->markShipVfxDirty();
        }
        else
        {
            if (clickedInstanceIndex >= 0)
            {
                ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
                DirectionOverride* override = this->getEditableDirectionOverride(&instance);
                if (override != nullptr)
                {
                    override->visible = !override->visible;
                }
                else
                {
                    instance.visible = !instance.visible;
                }
                this->markShipVfxDirty();
            }
        }
        clearLayerDragState();
        return true;
    }

    if (this->pointInRect(x, y, lockRect))
    {
        selectClickedRow();
        if (clickedIsShipRow)
        {
            this->shipLayerLocked = !this->shipLayerLocked;
            this->markShipVfxDirty();
            clearLayerDragState();
            this->statusMessage = this->shipLayerLocked ? "SHIP lock active." : "SHIP lock desactive.";
            return true;
        }
        else
        {
            if (clickedInstanceIndex >= 0)
            {
                ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
                instance.locked = !instance.locked;
                this->markShipVfxDirty();
            }
        }
        clearLayerDragState();
        return true;
    }

    selectClickedRow();

    bool canDragRow = true;
    if (clickedIsShipRow)
    {
        canDragRow = !this->shipLayerLocked;
        this->statusMessage = "Layer ship selectionne.";
    }
    else
    {
        if (clickedInstanceIndex >= 0)
        {
            const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(clickedInstanceIndex)];
            canDragRow = !instance.locked;
        }
        this->statusMessage = "Layer VFX selectionne.";
    }

    this->ensureSelectionVisible(
        this->getSelectedLayerRowIndexForDisplay(orderedLayerIndices),
        &this->layerListScrollOffset,
        itemCount);

    if (!canDragRow)
    {
        clearLayerDragState();
        this->statusMessage = "Layer verrouille: deverrouille pour reordonner.";
    }
    else
    {
        this->layerListScrollDragActive = false;
        this->layerRowDragActive = true;
        this->layerRowDragMoved = false;
        this->layerRowDragSourceDisplayIndex = clickedIndex;
        this->layerRowDragTargetInsertIndex = clickedIndex + 1;
        this->layerRowDragStartMouseY = y;
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
        if (this->editorMode != EditorMode::SHIP_VFX)
        {
            this->editorMode = EditorMode::SHIP_VFX;
            this->clearEditorTransientInteractionState();
            this->applyShipVfxModeViewportReset();
            this->reloadPreviewShipIfUnloadedKeepVfxLayers();
        }
        this->statusMessage = "Mode Ship / VFX actif.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonModeLooseSpritesRect))
    {
        if (this->editorMode != EditorMode::LOOSE_SPRITES)
        {
            this->editorMode = EditorMode::LOOSE_SPRITES;
            this->clearEditorTransientInteractionState();
            this->applyLooseSpritesModeEntryReset();
        }
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

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        if (this->pointInRect(x, y, this->buttonReloadAssetsRect))
        {
            this->autoImportAssetsFromDefaultFolders();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonDirectionPrevRect))
        {
            this->cyclePreviewDirection(-1);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonDirectionNextRect))
        {
            this->cyclePreviewDirection(1);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonShipStateToggleRect))
        {
            this->cyclePreviewShipState(1);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonRotateMinusRect))
        {
            this->adjustSelectedVfxRotation(-15.0f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonRotatePlusRect))
        {
            this->adjustSelectedVfxRotation(15.0f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonFlipHorizontalRect))
        {
            this->toggleSelectedVfxFlipHorizontal();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonFlipVerticalRect))
        {
            this->toggleSelectedVfxFlipVertical();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonSharedDirectionsRect))
        {
            this->toggleSelectedVfxSharedForAllDirections();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonDirectionOverrideRect))
        {
            this->toggleSelectedVfxDirectionOverride();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonCenterVfxRect))
        {
            this->centerSelectedVfxInstance();
            return true;
        }
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
        if (this->pointInRect(x, y, this->buttonLoosePreviewPlacementSnapRect) &&
            this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewPlacementSnapToTile = !this->loosePreviewPlacementSnapToTile;
            this->statusMessage = this->loosePreviewPlacementSnapToTile
                ? "Preview VFX: ancrage centre tuile (sous le curseur)."
                : "Preview VFX: ancrage sous-pixel (position exacte du curseur).";
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

    this->layerNameInputFocused = false;
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

    auto isPointInsideInstanceBounds = [this, x, y](int instanceIndex) -> bool {
        if (!this->previewShipLoaded)
        {
            return false;
        }
        if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->shipVfxInstances.size()))
        {
            return false;
        }

        const ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(instanceIndex)];
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const bool visible = (override != nullptr) ? override->visible : instance.visible;
        if (!visible)
        {
            return false;
        }
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            return false;
        }

        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        if (imported.image.sdl_texture == nullptr || imported.frames.empty())
        {
            return false;
        }

        const Map& map = GetCurrentMap();
        SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
        float shipCenterOffsetX = 0.0f;
        float shipCenterOffsetY = 0.0f;
        if (this->previewShip.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
        {
            shipCenter.x += shipCenterOffsetX;
            shipCenter.y += shipCenterOffsetY;
        }

        const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
        const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
        const int frameCount = static_cast<int>(imported.frames.size());
        const int frameIndex = static_cast<int>(std::floor(timeSeconds * (std::max)(imported.defaultFps, 1.0f))) % frameCount;
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
        const float offsetX = ((override != nullptr) ? override->offsetX : instance.offsetX) * scale;
        const float offsetY = ((override != nullptr) ? override->offsetY : instance.offsetY) * scale;
        const SDL_FRect bounds = SDL_FRect{
            shipCenter.x + offsetX - ((frame.w * scale) * 0.5f),
            shipCenter.y + offsetY - ((frame.h * scale) * 0.5f),
            frame.w * scale,
            frame.h * scale};
        return this->pointInRect(x, y, bounds);
    };

    if (button == RC2D_MOUSE_BUTTON_RIGHT)
    {
        const int hitIndex = this->findTopmostVfxInstanceIndexAtPoint(x, y);
        if (hitIndex >= 0)
        {
            this->setSelectedVfxInstanceIndex(hitIndex);
            if (this->hasSelectedVfxInstance() &&
                this->shipVfxInstances[static_cast<size_t>(this->selectedVfxInstanceIndex)].locked)
            {
                this->statusMessage = "Instance verrouillee: suppression refusee.";
            }
            else
            {
                this->removeSelectedVfxInstance();
            }
        }
        return true;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        if (this->vfxDragActive && rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            return true;
        }

        int hitIndex = this->findTopmostVfxInstanceIndexAtPoint(x, y);
        if (this->hasSelectedVfxInstance() &&
            isPointInsideInstanceBounds(this->selectedVfxInstanceIndex))
        {
            hitIndex = this->selectedVfxInstanceIndex;
        }

        this->setSelectedVfxInstanceIndex(hitIndex);
        if (hitIndex >= 0)
        {
            this->shipLayerSelected = false;
            ShipVfxInstance& instance = this->shipVfxInstances[static_cast<size_t>(hitIndex)];
            const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
            this->vfxDragStartMouseX = x;
            this->vfxDragStartMouseY = y;
            this->vfxDragStartOffsetX = (override != nullptr) ? override->offsetX : instance.offsetX;
            this->vfxDragStartOffsetY = (override != nullptr) ? override->offsetY : instance.offsetY;
            this->vfxDragActive = !instance.locked;
            this->statusMessage = "VFX instance selectionnee.";
        }
        else
        {
            this->shipLayerSelected = true;
            this->vfxDragActive = false;
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

        LoosePreviewPlacement placement{};
        placement.instanceId = this->nextLoosePreviewPlacementId++;
        if (this->loosePreviewPlacementSnapToTile)
        {
            const SDL_Point tile = map.screenToTileNearest(x, y);
            placement.tileX = static_cast<float>(tile.x);
            placement.tileY = static_cast<float>(tile.y);
        }
        else
        {
            const SDL_FPoint tileF = map.screenToTile(x, y);
            placement.tileX = tileF.x;
            placement.tileY = tileF.y;
        }
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
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath.clear();
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();

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

    this->applyShipVfxModeViewportReset();

    this->loadLooseReferencePreviewAssets();
    this->applySelectedOceanColor();
    this->autoImportAssetsFromDefaultFolders();
    this->statusMessage = "Editor VFX charge. Scan assets/images/ships et assets/images/vfx termine.";

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
        this->handleListPanelScrollDragFromMouse(
            this->layerListRect,
            static_cast<int>(this->shipVfxInstances.size()) + 1,
            &this->layerListScrollOffset,
            &this->layerListScrollDragActive,
            &this->layerListScrollDragGrabOffsetY);
        this->updateLayerListRowDragFromMouse();
        this->handleInvalidAssetPanelScrollDragFromMouse(
            this->invalidVfxListRect,
            static_cast<int>(this->invalidVfxFolders.size()),
            &this->invalidVfxListScrollOffset,
            &this->invalidVfxListScrollDragActive,
            &this->invalidVfxListScrollDragGrabOffsetY);
        this->handleInvalidAssetPanelScrollDragFromMouse(
            this->invalidShipListRect,
            static_cast<int>(this->invalidShipFolders.size()),
            &this->invalidShipListScrollOffset,
            &this->invalidShipListScrollDragActive,
            &this->invalidShipListScrollDragGrabOffsetY);

        if (this->vfxDragActive)
        {
            if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
            {
                this->vfxDragActive = false;
            }
            else
            {
                float mouseX = 0.0f;
                float mouseY = 0.0f;
                if (this->getMouseRenderPosition(&mouseX, &mouseY) && this->hasSelectedVfxInstance())
                {
                    ShipVfxInstance* instance = this->getSelectedVfxInstance();
                    if (instance != nullptr && !instance->locked)
                    {
                        const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
                        const float deltaX = (mouseX - this->vfxDragStartMouseX) / zoom;
                        const float deltaY = (mouseY - this->vfxDragStartMouseY) / zoom;
                        DirectionOverride* override = this->getEditableDirectionOverride(instance);
                        if (override != nullptr)
                        {
                            override->offsetX = this->vfxDragStartOffsetX + deltaX;
                            override->offsetY = this->vfxDragStartOffsetY + deltaY;
                        }
                        else
                        {
                            instance->offsetX = this->vfxDragStartOffsetX + deltaX;
                            instance->offsetY = this->vfxDragStartOffsetY + deltaY;
                        }
                        this->shipVfxDirty = true;
                    }
                }
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
    if (this->handleLayerNameInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLoosePreviewFpsInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->layerNameInputFocused = false;
        this->loosePreviewFpsInputFocused = false;
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_TAB)
    {
        this->editorMode = (this->editorMode == EditorMode::SHIP_VFX)
            ? EditorMode::LOOSE_SPRITES
            : EditorMode::SHIP_VFX;
        this->clearEditorTransientInteractionState();
        if (this->editorMode == EditorMode::SHIP_VFX)
        {
            this->applyShipVfxModeViewportReset();
            this->reloadPreviewShipIfUnloadedKeepVfxLayers();
            this->statusMessage = "Mode Ship / VFX actif.";
        }
        else
        {
            this->applyLooseSpritesModeEntryReset();
            this->statusMessage = "Mode Downscale Sprites VFX actif.";
        }
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

    if (!isrepeat && scancode == SDL_SCANCODE_F8)
    {
        this->autoImportAssetsFromDefaultFolders();
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
        if (!isrepeat && scancode == SDL_SCANCODE_1)
        {
            this->setPreviewDirectionIndex(0);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_2)
        {
            this->setPreviewDirectionIndex(1);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_3)
        {
            this->setPreviewDirectionIndex(2);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_4)
        {
            this->setPreviewDirectionIndex(3);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_G)
        {
            this->cyclePreviewShipState(1);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_B)
        {
            this->toggleSelectedVfxBehindShip();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_I)
        {
            this->toggleSelectedVfxVisibility();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_L)
        {
            this->toggleSelectedVfxLock();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_U)
        {
            this->toggleSelectedVfxSharedForAllDirections();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_Y)
        {
            this->toggleSelectedVfxDirectionOverride();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_D)
        {
            this->duplicateSelectedVfxInstance();
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_R)
        {
            this->resetSelectedVfxTransform();
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
        if (scancode == SDL_SCANCODE_UP ||
            scancode == SDL_SCANCODE_DOWN ||
            scancode == SDL_SCANCODE_LEFT ||
            scancode == SDL_SCANCODE_RIGHT)
        {
            const bool upDown = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_UP) || scancode == SDL_SCANCODE_UP;
            const bool downDown = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_DOWN) || scancode == SDL_SCANCODE_DOWN;
            const bool leftDown = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LEFT) || scancode == SDL_SCANCODE_LEFT;
            const bool rightDown = rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RIGHT) || scancode == SDL_SCANCODE_RIGHT;

            float deltaX = 0.0f;
            float deltaY = 0.0f;
            if (upDown && !downDown)
            {
                deltaY -= kVfxMoveStepPx;
            }
            else if (downDown && !upDown)
            {
                deltaY += kVfxMoveStepPx;
            }
            if (leftDown && !rightDown)
            {
                deltaX -= kVfxMoveStepPx;
            }
            else if (rightDown && !leftDown)
            {
                deltaX += kVfxMoveStepPx;
            }

            if (deltaX != 0.0f || deltaY != 0.0f)
            {
                this->moveSelectedVfxInstance(deltaX, deltaY);
                return;
            }
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_7)
        {
            this->moveSelectedVfxInstance(-kVfxMoveStepPx, -kVfxMoveStepPx);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_8)
        {
            this->moveSelectedVfxInstance(0.0f, -kVfxMoveStepPx);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_9)
        {
            this->moveSelectedVfxInstance(kVfxMoveStepPx, -kVfxMoveStepPx);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_4)
        {
            this->moveSelectedVfxInstance(-kVfxMoveStepPx, 0.0f);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_6)
        {
            this->moveSelectedVfxInstance(kVfxMoveStepPx, 0.0f);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_1)
        {
            this->moveSelectedVfxInstance(-kVfxMoveStepPx, kVfxMoveStepPx);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_2)
        {
            this->moveSelectedVfxInstance(0.0f, kVfxMoveStepPx);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_KP_3)
        {
            this->moveSelectedVfxInstance(kVfxMoveStepPx, kVfxMoveStepPx);
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
        const bool wasLayerNameFocused = this->layerNameInputFocused;
        const bool wasLoosePreviewFpsInputFocused = this->loosePreviewFpsInputFocused;
        const bool clickInLoosePreviewFpsInput =
            (this->editorMode == EditorMode::LOOSE_SPRITES) &&
            (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW) &&
            this->pointInRect(x, y, this->buttonLoosePreviewFpsInputRect);

        if (wasLayerNameFocused)
        {
            this->layerNameInputFocused = false;
            this->applyLayerNameInput();
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
            if (this->handleInvalidAssetPanelClick(
                    x,
                    y,
                    this->invalidVfxListRect,
                    static_cast<int>(this->invalidVfxFolders.size()),
                    &this->invalidVfxListScrollOffset,
                    &this->invalidVfxListScrollDragActive,
                    &this->invalidVfxListScrollDragGrabOffsetY))
            {
                return;
            }
            if (this->handleInvalidAssetPanelClick(
                    x,
                    y,
                    this->invalidShipListRect,
                    static_cast<int>(this->invalidShipFolders.size()),
                    &this->invalidShipListScrollOffset,
                    &this->invalidShipListScrollDragActive,
                    &this->invalidShipListScrollDragGrabOffsetY))
            {
                return;
            }
            if (this->handleLayerListClick(x, y))
            {
                return;
            }
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

void EditorMapVfxScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)integer_x;
    (void)mouseID;

    int delta = static_cast<int>(integer_y);
    if (delta == 0)
    {
        if (y > 0.0f)
        {
            delta = 1;
        }
        else if (y < 0.0f)
        {
            delta = -1;
        }
    }
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

    float eventMouseX = mouse_x;
    float eventMouseY = mouse_y;
    float eventMouseRenderX = mouse_x;
    float eventMouseRenderY = mouse_y;
    this->convertWindowToRender(mouse_x, mouse_y, &eventMouseRenderX, &eventMouseRenderY);

    float currentMouseX = 0.0f;
    float currentMouseY = 0.0f;
    rc2d_mouse_getPosition(&currentMouseX, &currentMouseY);
    float currentMouseRenderX = currentMouseX;
    float currentMouseRenderY = currentMouseY;
    this->convertWindowToRender(currentMouseX, currentMouseY, &currentMouseRenderX, &currentMouseRenderY);

    auto isMouseInsidePanel = [this, eventMouseX, eventMouseY, eventMouseRenderX, eventMouseRenderY, currentMouseX, currentMouseY, currentMouseRenderX, currentMouseRenderY](const SDL_FRect& panelRect) -> bool {
        return
            this->pointInRect(eventMouseX, eventMouseY, panelRect) ||
            this->pointInRect(eventMouseRenderX, eventMouseRenderY, panelRect) ||
            this->pointInRect(currentMouseX, currentMouseY, panelRect) ||
            this->pointInRect(currentMouseRenderX, currentMouseRenderY, panelRect);
    };

    auto scrollListPanel = [&](const SDL_FRect& panelRect, int itemCount, int* scrollOffset) -> bool {
        if (scrollOffset == nullptr)
        {
            return false;
        }
        if (!isMouseInsidePanel(panelRect))
        {
            return false;
        }
        *scrollOffset -= delta;
        this->clampListScrollOffset(scrollOffset, itemCount);
        return true;
    };

    auto scrollInvalidPanel = [&](const SDL_FRect& panelRect, int itemCount, int* scrollOffset) -> bool {
        if (scrollOffset == nullptr)
        {
            return false;
        }
        if (!isMouseInsidePanel(panelRect))
        {
            return false;
        }
        const int maxOffset = (std::max)(itemCount - kInvalidVisibleRows, 0);
        *scrollOffset -= delta;
        *scrollOffset = std::clamp(*scrollOffset, 0, maxOffset);
        return true;
    };

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        const int layerItemCount = static_cast<int>(this->getOrderedVfxInstanceIndicesForLayerPanel().size());
        if (scrollInvalidPanel(this->invalidVfxListRect, static_cast<int>(this->invalidVfxFolders.size()), &this->invalidVfxListScrollOffset))
        {
            return;
        }
        if (scrollInvalidPanel(this->invalidShipListRect, static_cast<int>(this->invalidShipFolders.size()), &this->invalidShipListScrollOffset))
        {
            return;
        }
        if (scrollListPanel(this->shipListRect, static_cast<int>(this->importedShips.size()), &this->shipListScrollOffset))
        {
            return;
        }
        if (scrollListPanel(this->sfxListRect, static_cast<int>(this->importedSfx.size()), &this->sfxListScrollOffset))
        {
            return;
        }
        if (scrollListPanel(this->layerListRect, layerItemCount, &this->layerListScrollOffset))
        {
            return;
        }
        return;
    }

}

#endif // GAME_ENV_DEV
