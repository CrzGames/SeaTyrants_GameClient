#include "game/vfx/vfx-classic.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <RC2D/RC2D_storage.h>
#include <RC2D/RC2D_system.h>
#include <cJSON.h>

#include <SDL3/SDL.h>

namespace
{
struct ParsedClassicFrame
{
    int index = 0;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct ParsedClassicSpritesheet
{
    std::string imageFile;
    float fps = 12.0f;
    int animationTotalDurationMs = 0;
    std::vector<ParsedClassicFrame> frames;
};

constexpr const char* kIlluminatedProjectileGlowConfigPath =
    "assets/data/ammo-illu-default.json";

VFXClassic::IlluminatedProjectileGlowConfig buildDefaultIlluminatedProjectileGlowConfig(void);
VFXClassic::IlluminatedProjectileGlowConfig clampIlluminatedProjectileGlowConfig(
    const VFXClassic::IlluminatedProjectileGlowConfig& config);

std::string normalizePathSlashesClassic(const std::string& path)
{
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

std::string trimAsciiClassic(const std::string& value)
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

std::string normalizeFolderPathClassic(const char* rawPath)
{
    if (rawPath == nullptr)
    {
        return {};
    }

    std::string path = trimAsciiClassic(normalizePathSlashesClassic(rawPath));
    while (path.size() > 1U && path.back() == '/')
    {
        path.pop_back();
    }
    return path;
}

std::string resolveSpritesheetJsonPathFallbackClassic(const std::string& vfxFolderPath)
{
    if (vfxFolderPath.empty())
    {
        return {};
    }

    const std::filesystem::path folder(vfxFolderPath);
    std::string folderName = folder.filename().string();
    if (folderName.empty())
    {
        folderName = folder.stem().string();
    }
    if (folderName.empty())
    {
        return {};
    }

    return normalizePathSlashesClassic((folder / (folderName + "-spritesheet.json")).string());
}

bool readTextFileFromStorageClassic(
    const char* path,
    std::string* outText)
{
    if (path == nullptr || path[0] == '\0' || outText == nullptr)
    {
        return false;
    }

    void* bytes = nullptr;
    Uint64 len = 0;
    const bool readOk = rc2d_storage_titleReadFile(path, &bytes, &len);
    if (!readOk || bytes == nullptr || len == 0)
    {
        if (bytes != nullptr)
        {
            RC2D_free(bytes);
        }
        return false;
    }

    outText->assign(static_cast<const char*>(bytes), static_cast<size_t>(len));
    RC2D_free(bytes);
    return true;
}

float readJsonFloatClassic(const cJSON* object, const char* key, float fallback)
{
    const cJSON* node = cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    if (!cJSON_IsNumber(node) || !std::isfinite(node->valuedouble))
    {
        return fallback;
    }
    return static_cast<float>(node->valuedouble);
}

bool readJsonBoolClassic(const cJSON* object, const char* key, bool fallback)
{
    const cJSON* node = cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    if (cJSON_IsBool(node))
    {
        return cJSON_IsTrue(node);
    }
    return fallback;
}

bool readIlluminatedProjectileGlowConfigFromJsonTextClassic(
    const char* jsonText,
    VFXClassic::IlluminatedProjectileGlowConfig* outConfig)
{
    if (jsonText == nullptr || outConfig == nullptr)
    {
        return false;
    }

    cJSON* root = cJSON_Parse(jsonText);
    if (root == nullptr)
    {
        return false;
    }

    VFXClassic::IlluminatedProjectileGlowConfig config = buildDefaultIlluminatedProjectileGlowConfig();
    config.glowIntensity = readJsonFloatClassic(root, "glowIntensity", config.glowIntensity);
    config.glowOpacity = readJsonFloatClassic(root, "glowOpacity", config.glowOpacity);
    config.glowRadius = readJsonFloatClassic(root, "glowRadius", config.glowRadius);
    config.glowDispersion = readJsonFloatClassic(root, "glowDispersion", config.glowDispersion);
    config.glowRoundness = readJsonFloatClassic(root, "glowRoundness", config.glowRoundness);
    config.coreSharpness = readJsonFloatClassic(root, "coreSharpness", config.coreSharpness);
    config.centerIntensity = readJsonFloatClassic(root, "centerIntensity", config.centerIntensity);
    config.centerFlashReduction = readJsonFloatClassic(root, "centerFlashReduction", config.centerFlashReduction);
    config.edgeSoftness = readJsonFloatClassic(root, "edgeSoftness", config.edgeSoftness);
    config.coreWhiteIntensity = readJsonFloatClassic(root, "coreWhiteIntensity", config.coreWhiteIntensity);
    config.coreWhiteRadius = readJsonFloatClassic(root, "coreWhiteRadius", config.coreWhiteRadius);
    config.colorShellIntensity = readJsonFloatClassic(root, "colorShellIntensity", config.colorShellIntensity);
    config.colorShellRadius = readJsonFloatClassic(root, "colorShellRadius", config.colorShellRadius);
    config.motionSmear = readJsonFloatClassic(root, "motionSmear", config.motionSmear);
    config.spriteOpacity = readJsonFloatClassic(root, "spriteOpacity", config.spriteOpacity);
    config.spriteBoost = readJsonFloatClassic(root, "spriteBoost", config.spriteBoost);
    config.spriteTintStrength = readJsonFloatClassic(root, "spriteTintStrength", config.spriteTintStrength);
    config.compactGlowEnabled = readJsonBoolClassic(root, "compactGlowEnabled", config.compactGlowEnabled);
    config.outerLayerEnabled = readJsonBoolClassic(root, "outerLayerEnabled", config.outerLayerEnabled);
    config.midLayerEnabled = readJsonBoolClassic(root, "midLayerEnabled", config.midLayerEnabled);
    config.innerLayerEnabled = readJsonBoolClassic(root, "innerLayerEnabled", config.innerLayerEnabled);
    cJSON_Delete(root);

    *outConfig = clampIlluminatedProjectileGlowConfig(config);
    return true;
}

bool exportIlluminatedProjectileGlowConfigToPathClassic(
    const char* path,
    const VFXClassic::IlluminatedProjectileGlowConfig& config)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }

    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "schema", "seatyrants.illuminatedProjectileGlow.v1");
    cJSON_AddNumberToObject(root, "glowIntensity", config.glowIntensity);
    cJSON_AddNumberToObject(root, "glowOpacity", config.glowOpacity);
    cJSON_AddNumberToObject(root, "glowRadius", config.glowRadius);
    cJSON_AddNumberToObject(root, "glowDispersion", config.glowDispersion);
    cJSON_AddNumberToObject(root, "glowRoundness", config.glowRoundness);
    cJSON_AddNumberToObject(root, "coreSharpness", config.coreSharpness);
    cJSON_AddNumberToObject(root, "centerIntensity", config.centerIntensity);
    cJSON_AddNumberToObject(root, "centerFlashReduction", config.centerFlashReduction);
    cJSON_AddNumberToObject(root, "edgeSoftness", config.edgeSoftness);
    cJSON_AddNumberToObject(root, "coreWhiteIntensity", config.coreWhiteIntensity);
    cJSON_AddNumberToObject(root, "coreWhiteRadius", config.coreWhiteRadius);
    cJSON_AddNumberToObject(root, "colorShellIntensity", config.colorShellIntensity);
    cJSON_AddNumberToObject(root, "colorShellRadius", config.colorShellRadius);
    cJSON_AddNumberToObject(root, "motionSmear", config.motionSmear);
    cJSON_AddNumberToObject(root, "spriteOpacity", config.spriteOpacity);
    cJSON_AddNumberToObject(root, "spriteBoost", config.spriteBoost);
    cJSON_AddNumberToObject(root, "spriteTintStrength", config.spriteTintStrength);
    cJSON_AddBoolToObject(root, "compactGlowEnabled", config.compactGlowEnabled);
    cJSON_AddBoolToObject(root, "outerLayerEnabled", config.outerLayerEnabled);
    cJSON_AddBoolToObject(root, "midLayerEnabled", config.midLayerEnabled);
    cJSON_AddBoolToObject(root, "innerLayerEnabled", config.innerLayerEnabled);

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);
    if (jsonText == nullptr)
    {
        return false;
    }

    const std::filesystem::path configPath(path);
    const std::filesystem::path parentPath = configPath.parent_path();
    std::error_code fsError;
    if (!parentPath.empty())
    {
        std::filesystem::create_directories(parentPath, fsError);
        if (fsError)
        {
            cJSON_free(jsonText);
            return false;
        }
    }

    std::ofstream output(configPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        cJSON_free(jsonText);
        return false;
    }

    output << jsonText;
    const bool ok = output.good();
    output.close();
    cJSON_free(jsonText);
    return ok;
}

std::string resolveSpritesheetImagePathClassic(
    const std::string& spritesheetJsonPath,
    const std::string& imageFieldValue)
{
    if (imageFieldValue.empty())
    {
        return {};
    }

    const std::string normalizedImage = normalizePathSlashesClassic(imageFieldValue);
    if (normalizedImage.rfind("assets/", 0U) == 0U)
    {
        return normalizedImage;
    }

    std::filesystem::path imagePath(normalizedImage);
    if (imagePath.is_absolute())
    {
        return normalizePathSlashesClassic(imagePath.string());
    }

    const std::filesystem::path jsonDir = std::filesystem::path(spritesheetJsonPath).parent_path();
    return normalizePathSlashesClassic((jsonDir / imagePath).string());
}

bool parseClassicSpritesheetJson(
    const char* jsonText,
    ParsedClassicSpritesheet* outSpritesheet)
{
    if (jsonText == nullptr || outSpritesheet == nullptr)
    {
        return false;
    }

    cJSON* root = cJSON_Parse(jsonText);
    if (root == nullptr)
    {
        return false;
    }

    const cJSON* framesNode = cJSON_GetObjectItemCaseSensitive(root, "frames");
    if (!cJSON_IsArray(framesNode))
    {
        cJSON_Delete(root);
        return false;
    }

    float parsedFps = 12.0f;
    const cJSON* fpsNode = cJSON_GetObjectItemCaseSensitive(root, "fps");
    if (cJSON_IsNumber(fpsNode) && std::isfinite(fpsNode->valuedouble))
    {
        parsedFps = static_cast<float>(fpsNode->valuedouble);
    }
    parsedFps = (std::max)(1.0f, parsedFps);

    int parsedAnimationTotalDurationMs = 0;
    const cJSON* animationDurationNode =
        cJSON_GetObjectItemCaseSensitive(root, "animationTotalDurationMs");
    if (cJSON_IsNumber(animationDurationNode) && std::isfinite(animationDurationNode->valuedouble))
    {
        const long long durationMs = std::llround(animationDurationNode->valuedouble);
        parsedAnimationTotalDurationMs =
            static_cast<int>((std::clamp)(durationMs, 0LL, 86400000LL));
    }

    std::string imageFieldValue;
    const cJSON* imageNode = cJSON_GetObjectItemCaseSensitive(root, "image");
    if (cJSON_IsString(imageNode) && imageNode->valuestring != nullptr)
    {
        imageFieldValue = normalizePathSlashesClassic(imageNode->valuestring);
    }
    if (imageFieldValue.empty())
    {
        const cJSON* metaNode = cJSON_GetObjectItemCaseSensitive(root, "meta");
        const cJSON* metaImageNode =
            cJSON_IsObject(metaNode) ? cJSON_GetObjectItemCaseSensitive(metaNode, "image") : nullptr;
        if (cJSON_IsString(metaImageNode) && metaImageNode->valuestring != nullptr)
        {
            imageFieldValue = normalizePathSlashesClassic(metaImageNode->valuestring);
        }
    }

    std::vector<ParsedClassicFrame> parsedFrames;
    parsedFrames.reserve(static_cast<size_t>(cJSON_GetArraySize(framesNode)));
    int generatedIndex = 0;

    cJSON* frameNode = nullptr;
    cJSON_ArrayForEach(frameNode, framesNode)
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
        if (!cJSON_IsNumber(xNode) || !cJSON_IsNumber(yNode) ||
            !cJSON_IsNumber(wNode) || !cJSON_IsNumber(hNode))
        {
            continue;
        }

        const float x = static_cast<float>(xNode->valuedouble);
        const float y = static_cast<float>(yNode->valuedouble);
        const float w = static_cast<float>(wNode->valuedouble);
        const float h = static_cast<float>(hNode->valuedouble);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) ||
            w <= 0.0f || h <= 0.0f)
        {
            continue;
        }

        int frameIndex = generatedIndex;
        if (cJSON_IsNumber(indexNode) && std::isfinite(indexNode->valuedouble))
        {
            frameIndex = static_cast<int>(std::llround(indexNode->valuedouble));
        }
        generatedIndex += 1;

        parsedFrames.push_back(ParsedClassicFrame{frameIndex, x, y, w, h});
    }

    if (parsedFrames.empty())
    {
        cJSON_Delete(root);
        return false;
    }

    std::sort(parsedFrames.begin(), parsedFrames.end(), [](const ParsedClassicFrame& a, const ParsedClassicFrame& b) {
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

    if (imageFieldValue.empty())
    {
        imageFieldValue = "spritesheet.png";
    }

    cJSON_Delete(root);
    outSpritesheet->fps = parsedFps;
    outSpritesheet->animationTotalDurationMs = parsedAnimationTotalDurationMs;
    outSpritesheet->imageFile = imageFieldValue;
    outSpritesheet->frames = std::move(parsedFrames);
    return true;
}

float clampClassicPlaybackSampleSeconds(
    float playbackSeconds,
    int animationTotalDurationMs)
{
    float sampleTime = (std::isfinite(playbackSeconds) && playbackSeconds > 0.0f)
        ? playbackSeconds
        : 0.0f;
    if (animationTotalDurationMs <= 0)
    {
        return sampleTime;
    }

    const float durationSec = static_cast<float>(animationTotalDurationMs) * 0.001f;
    if (durationSec <= 0.0f)
    {
        return 0.0f;
    }

    sampleTime = (std::min)(sampleTime, durationSec);
    if (sampleTime >= durationSec)
    {
        sampleTime = (std::max)(0.0f, durationSec - 0.000001f);
    }
    return sampleTime;
}

VFXClassic::IlluminatedProjectileGlowConfig buildDefaultIlluminatedProjectileGlowConfig(void)
{
    return VFXClassic::IlluminatedProjectileGlowConfig{};
}

VFXClassic::IlluminatedProjectileGlowConfig clampIlluminatedProjectileGlowConfig(
    const VFXClassic::IlluminatedProjectileGlowConfig& config)
{
    VFXClassic::IlluminatedProjectileGlowConfig clamped = config;
    clamped.glowIntensity =
        (std::clamp)((std::isfinite(clamped.glowIntensity) ? clamped.glowIntensity : 1.0f), 0.0f, 3.0f);
    clamped.glowOpacity =
        (std::clamp)((std::isfinite(clamped.glowOpacity) ? clamped.glowOpacity : 1.0f), 0.0f, 1.5f);
    clamped.glowRadius =
        (std::clamp)((std::isfinite(clamped.glowRadius) ? clamped.glowRadius : 1.0f), 0.25f, 2.5f);
    clamped.glowDispersion =
        (std::clamp)((std::isfinite(clamped.glowDispersion) ? clamped.glowDispersion : 1.0f), 0.0f, 2.0f);
    clamped.glowRoundness =
        (std::clamp)((std::isfinite(clamped.glowRoundness) ? clamped.glowRoundness : 1.0f), 0.35f, 2.5f);
    clamped.coreSharpness =
        (std::clamp)((std::isfinite(clamped.coreSharpness) ? clamped.coreSharpness : 1.0f), 0.35f, 4.0f);
    clamped.centerIntensity =
        (std::clamp)((std::isfinite(clamped.centerIntensity) ? clamped.centerIntensity : 1.0f), 0.0f, 2.5f);
    clamped.centerFlashReduction =
        (std::clamp)(
            (std::isfinite(clamped.centerFlashReduction) ? clamped.centerFlashReduction : 0.26f),
            0.0f,
            0.95f);
    clamped.edgeSoftness =
        (std::clamp)((std::isfinite(clamped.edgeSoftness) ? clamped.edgeSoftness : 1.0f), 0.35f, 2.5f);
    clamped.coreWhiteIntensity =
        (std::clamp)((std::isfinite(clamped.coreWhiteIntensity) ? clamped.coreWhiteIntensity : 0.56f), 0.0f, 1.5f);
    clamped.coreWhiteRadius =
        (std::clamp)((std::isfinite(clamped.coreWhiteRadius) ? clamped.coreWhiteRadius : 0.68f), 0.2f, 1.5f);
    clamped.colorShellIntensity =
        (std::clamp)((std::isfinite(clamped.colorShellIntensity) ? clamped.colorShellIntensity : 0.72f), 0.0f, 1.5f);
    clamped.colorShellRadius =
        (std::clamp)((std::isfinite(clamped.colorShellRadius) ? clamped.colorShellRadius : 0.92f), 0.3f, 2.0f);
    clamped.motionSmear =
        (std::clamp)((std::isfinite(clamped.motionSmear) ? clamped.motionSmear : 0.18f), 0.0f, 1.0f);
    clamped.spriteOpacity =
        (std::clamp)((std::isfinite(clamped.spriteOpacity) ? clamped.spriteOpacity : 1.0f), 0.0f, 1.0f);
    clamped.spriteBoost =
        (std::clamp)((std::isfinite(clamped.spriteBoost) ? clamped.spriteBoost : 0.28f), 0.0f, 1.5f);
    clamped.spriteTintStrength =
        (std::clamp)((std::isfinite(clamped.spriteTintStrength) ? clamped.spriteTintStrength : 0.18f), 0.0f, 1.0f);
    return clamped;
}

bool illuminatedProjectileGlowConfigsEqual(
    const VFXClassic::IlluminatedProjectileGlowConfig& a,
    const VFXClassic::IlluminatedProjectileGlowConfig& b)
{
    constexpr float kFloatEpsilon = 0.0005f;
    return
        std::fabs(a.glowIntensity - b.glowIntensity) <= kFloatEpsilon &&
        std::fabs(a.glowOpacity - b.glowOpacity) <= kFloatEpsilon &&
        std::fabs(a.glowRadius - b.glowRadius) <= kFloatEpsilon &&
        std::fabs(a.glowDispersion - b.glowDispersion) <= kFloatEpsilon &&
        std::fabs(a.glowRoundness - b.glowRoundness) <= kFloatEpsilon &&
        std::fabs(a.coreSharpness - b.coreSharpness) <= kFloatEpsilon &&
        std::fabs(a.centerIntensity - b.centerIntensity) <= kFloatEpsilon &&
        std::fabs(a.centerFlashReduction - b.centerFlashReduction) <= kFloatEpsilon &&
        std::fabs(a.edgeSoftness - b.edgeSoftness) <= kFloatEpsilon &&
        std::fabs(a.coreWhiteIntensity - b.coreWhiteIntensity) <= kFloatEpsilon &&
        std::fabs(a.coreWhiteRadius - b.coreWhiteRadius) <= kFloatEpsilon &&
        std::fabs(a.colorShellIntensity - b.colorShellIntensity) <= kFloatEpsilon &&
        std::fabs(a.colorShellRadius - b.colorShellRadius) <= kFloatEpsilon &&
        std::fabs(a.motionSmear - b.motionSmear) <= kFloatEpsilon &&
        std::fabs(a.spriteOpacity - b.spriteOpacity) <= kFloatEpsilon &&
        std::fabs(a.spriteBoost - b.spriteBoost) <= kFloatEpsilon &&
        std::fabs(a.spriteTintStrength - b.spriteTintStrength) <= kFloatEpsilon &&
        a.compactGlowEnabled == b.compactGlowEnabled &&
        a.outerLayerEnabled == b.outerLayerEnabled &&
        a.midLayerEnabled == b.midLayerEnabled &&
        a.innerLayerEnabled == b.innerLayerEnabled;
}

VFXClassic::IlluminatedProjectileGlowConfig gIlluminatedProjectileGlowConfig =
    buildDefaultIlluminatedProjectileGlowConfig();
int gIlluminatedProjectileGlowConfigGeneration = 0;
bool gIlluminatedProjectileGlowConfigLoadAttempted = false;
bool gIlluminatedProjectileGlowConfigLoaded = false;
} // namespace

VFXClassic::VFXClassic(void)
    : spritesheetImage{},
      frames{},
      defaultFps(12.0f),
      animationTotalDurationMs(0),
      playbackSeconds(0.0f),
      framePhaseOffsetSec(0.0f),
      loaded(false),
      vfxFolderPath{},
      spritesheetJsonPath{},
      spritesheetImagePath{}
{
}

VFXClassic::~VFXClassic(void)
{
    this->unload();
}

float VFXClassic::resolvedPlaybackPeriodSeconds(void) const
{
    if (this->frames.empty())
    {
        return 0.0f;
    }

    const float fps = (std::max)(this->defaultFps, 1.0f);
    return static_cast<float>(this->frames.size()) / fps;
}

bool VFXClassic::loadFromFolder(const char* rawVfxFolderPath)
{
    this->unload();

    const std::string vfxFolderPathNormalized = normalizeFolderPathClassic(rawVfxFolderPath);
    if (vfxFolderPathNormalized.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: chemin dossier invalide (vfx='%s')",
            (rawVfxFolderPath != nullptr) ? rawVfxFolderPath : "(null)");
        return false;
    }

    const std::string parsedSpritesheetJsonPath =
        resolveSpritesheetJsonPathFallbackClassic(vfxFolderPathNormalized);
    if (parsedSpritesheetJsonPath.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: impossible de resoudre le JSON spritesheet depuis '%s'",
            vfxFolderPathNormalized.c_str());
        return false;
    }

    std::string spritesheetText;
    if (!readTextFileFromStorageClassic(parsedSpritesheetJsonPath.c_str(), &spritesheetText))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: spritesheet JSON introuvable: %s",
            parsedSpritesheetJsonPath.c_str());
        return false;
    }

    ParsedClassicSpritesheet parsedSpritesheet{};
    if (!parseClassicSpritesheetJson(spritesheetText.c_str(), &parsedSpritesheet))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: spritesheet JSON invalide: %s",
            parsedSpritesheetJsonPath.c_str());
        return false;
    }

    const std::string parsedSpritesheetImagePath =
        resolveSpritesheetImagePathClassic(parsedSpritesheetJsonPath, parsedSpritesheet.imageFile);
    if (parsedSpritesheetImagePath.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: path image spritesheet invalide: %s",
            parsedSpritesheetJsonPath.c_str());
        return false;
    }

    RC2D_Image loadedImage = LoadStorageImage(parsedSpritesheetImagePath.c_str(), RC2D_STORAGE_TITLE);
    if (loadedImage.sdl_texture == nullptr)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFXClassic: image spritesheet introuvable: %s",
            parsedSpritesheetImagePath.c_str());
        return false;
    }

    this->spritesheetImage = loadedImage;
    this->frames.clear();
    this->frames.reserve(parsedSpritesheet.frames.size());
    for (const ParsedClassicFrame& frame : parsedSpritesheet.frames)
    {
        this->frames.push_back(Frame{frame.index, frame.x, frame.y, frame.w, frame.h});
    }
    this->defaultFps = parsedSpritesheet.fps;
    this->animationTotalDurationMs = parsedSpritesheet.animationTotalDurationMs;
    this->playbackSeconds = 0.0f;
    this->framePhaseOffsetSec = 0.0f;
    this->loaded = true;
    this->vfxFolderPath = vfxFolderPathNormalized;
    this->spritesheetJsonPath = parsedSpritesheetJsonPath;
    this->spritesheetImagePath = parsedSpritesheetImagePath;

    RC2D_log(
        RC2D_LOG_INFO,
        "VFXClassic: charge depuis %s (frames=%d, fps=%.2f, durationMs=%d)",
        this->spritesheetJsonPath.c_str(),
        static_cast<int>(this->frames.size()),
        this->defaultFps,
        this->animationTotalDurationMs);
    return true;
}

void VFXClassic::unload(void)
{
    if (this->spritesheetImage.sdl_texture != nullptr)
    {
        ResetStorageImageRef(&this->spritesheetImage);
    }

    this->spritesheetImage = RC2D_Image{};
    this->frames.clear();
    this->defaultFps = 12.0f;
    this->animationTotalDurationMs = 0;
    this->playbackSeconds = 0.0f;
    this->framePhaseOffsetSec = 0.0f;
    this->loaded = false;
    this->vfxFolderPath.clear();
    this->spritesheetJsonPath.clear();
    this->spritesheetImagePath.clear();
}

bool VFXClassic::isLoaded(void) const
{
    return this->loaded &&
        this->spritesheetImage.sdl_texture != nullptr &&
        !this->frames.empty();
}

void VFXClassic::resetPlayback(void)
{
    this->playbackSeconds = 0.0f;
}

void VFXClassic::setFramePhaseOffsetSeconds(float seconds)
{
    this->framePhaseOffsetSec = (std::isfinite(seconds)) ? seconds : 0.0f;
}

void VFXClassic::update(double dt)
{
    if (!this->isLoaded())
    {
        return;
    }

    const float deltaSeconds = (std::isfinite(dt) && dt > 0.0) ? static_cast<float>(dt) : 0.0f;
    this->playbackSeconds += deltaSeconds;

    if (this->animationTotalDurationMs > 0)
    {
        const float maxDurationSec = static_cast<float>(this->animationTotalDurationMs) * 0.001f;
        this->playbackSeconds = (std::min)(this->playbackSeconds, maxDurationSec);
    }
}

bool VFXClassic::isInfinite(void) const
{
    return this->animationTotalDurationMs <= 0;
}

bool VFXClassic::isFinished(void) const
{
    if (!this->isLoaded() || this->isInfinite())
    {
        return false;
    }

    const float maxDurationSec = static_cast<float>(this->animationTotalDurationMs) * 0.001f;
    return this->playbackSeconds >= maxDurationSec;
}

int VFXClassic::getFrameCount(void) const
{
    return static_cast<int>(this->frames.size());
}

float VFXClassic::getDefaultFps(void) const
{
    return this->defaultFps;
}

int VFXClassic::getAnimationTotalDurationMs(void) const
{
    return this->animationTotalDurationMs;
}

VFXClassic::IlluminatedProjectileGlowConfig VFXClassic::getDefaultIlluminatedProjectileGlowConfig(void)
{
    return buildDefaultIlluminatedProjectileGlowConfig();
}

const VFXClassic::IlluminatedProjectileGlowConfig& VFXClassic::getIlluminatedProjectileGlowConfig(void)
{
    return gIlluminatedProjectileGlowConfig;
}

void VFXClassic::setIlluminatedProjectileGlowConfig(const IlluminatedProjectileGlowConfig& config)
{
    const VFXClassic::IlluminatedProjectileGlowConfig clamped =
        clampIlluminatedProjectileGlowConfig(config);
    if (illuminatedProjectileGlowConfigsEqual(clamped, gIlluminatedProjectileGlowConfig))
    {
        return;
    }

    gIlluminatedProjectileGlowConfig = clamped;
    gIlluminatedProjectileGlowConfigGeneration += 1;
}

void VFXClassic::resetIlluminatedProjectileGlowConfig(void)
{
    VFXClassic::setIlluminatedProjectileGlowConfig(buildDefaultIlluminatedProjectileGlowConfig());
}

bool VFXClassic::loadIlluminatedProjectileGlowConfigFromFile(void)
{
    if (gIlluminatedProjectileGlowConfigLoadAttempted)
    {
        return gIlluminatedProjectileGlowConfigLoaded;
    }
    gIlluminatedProjectileGlowConfigLoadAttempted = true;
    gIlluminatedProjectileGlowConfigLoaded = false;

    std::string jsonText;
    if (!readTextFileFromStorageClassic(kIlluminatedProjectileGlowConfigPath, &jsonText))
    {
        return false;
    }

    IlluminatedProjectileGlowConfig config{};
    if (!readIlluminatedProjectileGlowConfigFromJsonTextClassic(jsonText.c_str(), &config))
    {
        return false;
    }

    VFXClassic::setIlluminatedProjectileGlowConfig(config);
    gIlluminatedProjectileGlowConfigLoaded = true;
    return true;
}

bool VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
    const char* path,
    IlluminatedProjectileGlowConfig* outConfig)
{
    if (path == nullptr || path[0] == '\0' || outConfig == nullptr)
    {
        return false;
    }

    std::string jsonText;
    if (!readTextFileFromStorageClassic(path, &jsonText))
    {
        return false;
    }

    return readIlluminatedProjectileGlowConfigFromJsonTextClassic(jsonText.c_str(), outConfig);
}

bool VFXClassic::exportIlluminatedProjectileGlowConfigToFile(void)
{
    const IlluminatedProjectileGlowConfig config = gIlluminatedProjectileGlowConfig;
    const bool exported =
        exportIlluminatedProjectileGlowConfigToPathClassic(kIlluminatedProjectileGlowConfigPath, config);
    if (exported)
    {
        VFXClassic::invalidateIlluminatedProjectileGlowConfigFileLoadState();
    }
    return exported;
}

bool VFXClassic::exportIlluminatedProjectileGlowConfigToFile(
    const char* path,
    const IlluminatedProjectileGlowConfig* configOverride)
{
    const IlluminatedProjectileGlowConfig config =
        (configOverride != nullptr) ? *configOverride : gIlluminatedProjectileGlowConfig;
    const bool exported =
        exportIlluminatedProjectileGlowConfigToPathClassic(path, clampIlluminatedProjectileGlowConfig(config));
    if (exported &&
        path != nullptr &&
        normalizePathSlashesClassic(path) == normalizePathSlashesClassic(kIlluminatedProjectileGlowConfigPath))
    {
        VFXClassic::invalidateIlluminatedProjectileGlowConfigFileLoadState();
    }
    return exported;
}

void VFXClassic::invalidateIlluminatedProjectileGlowConfigFileLoadState(void)
{
    gIlluminatedProjectileGlowConfigLoadAttempted = false;
    gIlluminatedProjectileGlowConfigLoaded = false;
}

const char* VFXClassic::getIlluminatedProjectileGlowConfigPath(void)
{
    return kIlluminatedProjectileGlowConfigPath;
}

int VFXClassic::getCurrentFrameIndex(void) const
{
    return this->getCurrentFrameIndexWithPhaseOffset(0.0f);
}

int VFXClassic::getCurrentFrameIndexWithPhaseOffset(float additionalPhaseOffsetSec) const
{
    const int frameCount = static_cast<int>(this->frames.size());
    if (frameCount <= 0)
    {
        return 0;
    }

    const float periodSec = this->resolvedPlaybackPeriodSeconds();
    if (periodSec <= 0.0001f)
    {
        return 0;
    }

    const float fps = (std::max)(this->defaultFps, 1.0f);
    const float sampleTime =
        clampClassicPlaybackSampleSeconds(this->playbackSeconds, this->animationTotalDurationMs);
    const float totalPhaseOffset =
        this->framePhaseOffsetSec + ((std::isfinite(additionalPhaseOffsetSec)) ? additionalPhaseOffsetSec : 0.0f);
    float phase = std::fmod(sampleTime + totalPhaseOffset, periodSec);
    if (phase < 0.0f)
    {
        phase += periodSec;
    }

    int index = static_cast<int>(std::floor(phase * fps));
    index %= frameCount;
    if (index < 0)
    {
        index += frameCount;
    }
    return index;
}

void VFXClassic::draw(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical) const
{
    this->drawInternal(centerX, centerY, scale, rotationDeg, flipHorizontal, flipVertical, nullptr);
}

void VFXClassic::drawWithRgbTint(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical,
    std::uint8_t tintR,
    std::uint8_t tintG,
    std::uint8_t tintB) const
{
    const std::uint8_t rgb[3] = {tintR, tintG, tintB};
    this->drawInternal(centerX, centerY, scale, rotationDeg, flipHorizontal, flipVertical, rgb);
}

void VFXClassic::drawWithTintAlphaBlend(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical,
    const std::uint8_t* tintRgbOrNull,
    std::uint8_t alpha,
    int blendMode) const
{
    this->drawInternal(
        centerX,
        centerY,
        scale,
        rotationDeg,
        flipHorizontal,
        flipVertical,
        tintRgbOrNull,
        alpha,
        blendMode);
}

void VFXClassic::drawWithTintAlphaBlendPhaseOffset(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical,
    const std::uint8_t* tintRgbOrNull,
    std::uint8_t alpha,
    int blendMode,
    float phaseOffsetSec) const
{
    this->drawInternal(
        centerX,
        centerY,
        scale,
        rotationDeg,
        flipHorizontal,
        flipVertical,
        tintRgbOrNull,
        alpha,
        blendMode,
        phaseOffsetSec);
}

void VFXClassic::drawInternal(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical,
    const std::uint8_t* tintRgb,
    std::uint8_t alpha,
    int blendMode,
    float additionalPhaseOffsetSec) const
{
    if (!this->isLoaded() || this->isFinished())
    {
        return;
    }

    const int frameIndex = this->getCurrentFrameIndexWithPhaseOffset(additionalPhaseOffsetSec);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(this->frames.size()))
    {
        return;
    }

    const Frame& frame = this->frames[static_cast<size_t>(frameIndex)];
    const float drawScale = (std::max)(scale, 0.0001f);
    const float drawWidth = frame.w * drawScale;
    const float drawHeight = frame.h * drawScale;
    const float drawX = centerX - (drawWidth * 0.5f);
    const float drawY = centerY - (drawHeight * 0.5f);
    const float pivotX = frame.w * 0.5f;
    const float pivotY = frame.h * 0.5f;

    RC2D_Quad quad = rc2d_graphics_newQuad(
        const_cast<RC2D_Image*>(&this->spritesheetImage),
        frame.x,
        frame.y,
        frame.w,
        frame.h);

    SDL_Texture* tex = this->spritesheetImage.sdl_texture;
    Uint8 prevR = 255;
    Uint8 prevG = 255;
    Uint8 prevB = 255;
    Uint8 prevA = 255;
    SDL_BlendMode prevBlendMode = SDL_BLENDMODE_BLEND;
    bool restoreTint = false;
    bool restoreAlpha = false;
    bool restoreBlend = false;
    if (tintRgb != nullptr && tex != nullptr)
    {
        SDL_GetTextureColorMod(tex, &prevR, &prevG, &prevB);
        SDL_SetTextureColorMod(tex, tintRgb[0], tintRgb[1], tintRgb[2]);
        restoreTint = true;
    }
    if (tex != nullptr && alpha != 255)
    {
        SDL_GetTextureAlphaMod(tex, &prevA);
        SDL_SetTextureAlphaMod(tex, alpha);
        restoreAlpha = true;
    }
    if (tex != nullptr && blendMode != 0)
    {
        SDL_GetTextureBlendMode(tex, &prevBlendMode);
        const SDL_BlendMode nextBlendMode = static_cast<SDL_BlendMode>(blendMode);
        if (prevBlendMode != nextBlendMode)
        {
            SDL_SetTextureBlendMode(tex, nextBlendMode);
            restoreBlend = true;
        }
    }

    rc2d_graphics_drawQuad(
        const_cast<RC2D_Image*>(&this->spritesheetImage),
        &quad,
        drawX,
        drawY,
        rotationDeg,
        drawScale,
        drawScale,
        pivotX,
        pivotY,
        flipHorizontal,
        flipVertical);

    if (restoreTint && tex != nullptr)
    {
        SDL_SetTextureColorMod(tex, prevR, prevG, prevB);
    }
    if (restoreAlpha && tex != nullptr)
    {
        SDL_SetTextureAlphaMod(tex, prevA);
    }
    if (restoreBlend && tex != nullptr)
    {
        SDL_SetTextureBlendMode(tex, prevBlendMode);
    }
}

namespace
{

struct IlluminatedGlowTextureCacheEntry {
    VFXClassic::IlluminatedProjectileGlowConfig config{};
    SDL_Texture* radialTex = nullptr;
    SDL_Texture* shellTex = nullptr;
};

std::vector<IlluminatedGlowTextureCacheEntry> gIllumGlowTextureCacheEntries;

/**
 * Retourne / genere les textures procedurales pour une config glow donnee.
 */
bool ensureIlluminatedGlowRadialTexture(
    SDL_Renderer* renderer,
    const VFXClassic::IlluminatedProjectileGlowConfig& config,
    SDL_Texture** outRadialTex,
    SDL_Texture** outShellTex)
{
    if (outRadialTex != nullptr)
    {
        *outRadialTex = nullptr;
    }
    if (outShellTex != nullptr)
    {
        *outShellTex = nullptr;
    }

    for (IlluminatedGlowTextureCacheEntry& entry : gIllumGlowTextureCacheEntries)
    {
        if (!illuminatedProjectileGlowConfigsEqual(entry.config, config))
        {
            continue;
        }

        if (outRadialTex != nullptr)
        {
            *outRadialTex = entry.radialTex;
        }
        if (outShellTex != nullptr)
        {
            *outShellTex = entry.shellTex;
        }
        return entry.radialTex != nullptr;
    }

    if (renderer == nullptr)
    {
        return false;
    }

    constexpr int sz = 256;
    SDL_Surface* surf = SDL_CreateSurface(sz, sz, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* shellSurf = SDL_CreateSurface(sz, sz, SDL_PIXELFORMAT_RGBA32);
    if (surf == nullptr || shellSurf == nullptr)
    {
        if (surf != nullptr)
        {
            SDL_DestroySurface(surf);
        }
        if (shellSurf != nullptr)
        {
            SDL_DestroySurface(shellSurf);
        }
        return false;
    }

    const float cx = static_cast<float>(sz - 1) * 0.5f;
    const float cy = static_cast<float>(sz - 1) * 0.5f;
    const float maxR = std::hypot(cx, cy);
    for (int y = 0; y < sz; ++y)
    {
        for (int x = 0; x < sz; ++x)
        {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float d = std::sqrt(dx * dx + dy * dy) / maxR;
            const float t = (std::clamp)(d, 0.0f, 1.0f);
            const bool compactMode = config.compactGlowEnabled;
            const float roundedDistance =
                std::pow(t, config.glowRoundness * (compactMode ? 1.12f : 1.0f));
            const float radialPower =
                compactMode
                    ? (5.2f + (config.coreSharpness * 2.6f))
                    : (3.35f + (config.coreSharpness * 1.45f));
            float a = std::pow(1.0f - roundedDistance, radialPower);

            if (compactMode)
            {
                const float tailT = (std::clamp)((t - 0.28f) / 0.72f, 0.0f, 1.0f);
                const float compactTail =
                    1.0f - std::pow(tailT, 1.15f + (config.coreSharpness * 0.32f));
                a *= compactTail;
            }

            const float centerMask = 1.0f - t;
            const float centerWeight = compactMode ? std::pow(centerMask, 2.35f) : (centerMask * centerMask);
            const float centerBoost =
                1.0f + ((config.centerIntensity - 1.0f) * centerWeight);
            const float centerAtten =
                1.0f - (config.centerFlashReduction * (compactMode ? std::pow(centerMask, 0.82f) : centerMask));
            a *= centerBoost;
            a *= centerAtten;
            a = (std::clamp)(a, 0.0f, 1.0f);
            a = std::pow(a, 1.0f / config.edgeSoftness);
            a *= config.glowOpacity;
            const auto ai = static_cast<Uint8>((std::clamp)(a * 255.0f, 0.0f, 255.0f));
            SDL_WriteSurfacePixel(surf, x, y, 255, 255, 255, ai);

            const float shellCenterFalloff = 1.0f - std::pow(centerMask, compactMode ? 2.6f : 2.0f);
            const float shellTail = std::pow(1.0f - t, compactMode ? 3.8f : 3.1f);
            float shellAlpha = shellCenterFalloff * shellTail;
            shellAlpha = std::pow(shellAlpha, compactMode ? 0.82f : 0.94f);
            shellAlpha *= config.glowOpacity;
            const auto shellAi = static_cast<Uint8>((std::clamp)(shellAlpha * 255.0f, 0.0f, 255.0f));
            SDL_WriteSurfacePixel(shellSurf, x, y, 255, 255, 255, shellAi);
        }
    }

    SDL_Texture* radialTex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    SDL_Texture* shellTex = SDL_CreateTextureFromSurface(renderer, shellSurf);
    SDL_DestroySurface(shellSurf);
    if (radialTex == nullptr || shellTex == nullptr)
    {
        if (radialTex != nullptr)
        {
            SDL_DestroyTexture(radialTex);
        }
        if (shellTex != nullptr)
        {
            SDL_DestroyTexture(shellTex);
        }
        return false;
    }

    SDL_SetTextureScaleMode(radialTex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(radialTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(shellTex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(shellTex, SDL_BLENDMODE_BLEND);

    constexpr std::size_t kMaxCachedGlowConfigs = 12U;
    if (gIllumGlowTextureCacheEntries.size() >= kMaxCachedGlowConfigs)
    {
        IlluminatedGlowTextureCacheEntry& evicted = gIllumGlowTextureCacheEntries.front();
        if (evicted.radialTex != nullptr)
        {
            SDL_DestroyTexture(evicted.radialTex);
        }
        if (evicted.shellTex != nullptr)
        {
            SDL_DestroyTexture(evicted.shellTex);
        }
        gIllumGlowTextureCacheEntries.erase(gIllumGlowTextureCacheEntries.begin());
    }

    IlluminatedGlowTextureCacheEntry cacheEntry{};
    cacheEntry.config = config;
    cacheEntry.radialTex = radialTex;
    cacheEntry.shellTex = shellTex;
    gIllumGlowTextureCacheEntries.push_back(cacheEntry);
    if (outRadialTex != nullptr)
    {
        *outRadialTex = radialTex;
    }
    if (outShellTex != nullptr)
    {
        *outShellTex = shellTex;
    }
    return true;
}

} // namespace

void VFXClassic::drawIlluminatedProjectile(
    float centerX,
    float centerY,
    float scale,
    float rotationDeg,
    bool flipHorizontal,
    bool flipVertical,
    std::uint8_t glowR,
    std::uint8_t glowG,
    std::uint8_t glowB,
    const IlluminatedProjectileGlowConfig* glowConfigOverride,
    bool lockGlowToMaxSpriteFrame) const
{
    if (!this->isLoaded() || this->isFinished())
    {
        return;
    }

    SDL_Window* window = rc2d_window_getWindow();
    SDL_Renderer* renderer = (window != nullptr) ? SDL_GetRenderer(window) : nullptr;
    const VFXClassic::IlluminatedProjectileGlowConfig& runtimeConfig =
        VFXClassic::getIlluminatedProjectileGlowConfig();
    const VFXClassic::IlluminatedProjectileGlowConfig& config =
        (glowConfigOverride != nullptr) ? *glowConfigOverride : runtimeConfig;
    SDL_Texture* glowTex = nullptr;
    SDL_Texture* shellTex = nullptr;
    if (renderer == nullptr || !ensureIlluminatedGlowRadialTexture(renderer, config, &glowTex, &shellTex))
    {
        this->drawInternal(centerX, centerY, scale, rotationDeg, flipHorizontal, flipVertical, nullptr);
        if (renderer != nullptr)
        {
            SDL_FlushRenderer(renderer);
        }
        return;
    }

    const int frameIndex = this->getCurrentFrameIndex();
    if (frameIndex < 0 || frameIndex >= static_cast<int>(this->frames.size()))
    {
        return;
    }

    const Frame& frame = this->frames[static_cast<size_t>(frameIndex)];
    const float drawScale = (std::max)(scale, 0.0001f);
    float baseMax = 0.0f;
    if (lockGlowToMaxSpriteFrame && !this->frames.empty())
    {
        float maxExtent = 0.0f;
        for (const Frame& f : this->frames)
        {
            maxExtent = (std::max)(maxExtent, (std::max)(f.w, f.h));
        }
        baseMax = maxExtent * drawScale;
    }
    else
    {
        const float drawWidth = frame.w * drawScale;
        const float drawHeight = frame.h * drawScale;
        baseMax = (std::max)(drawWidth, drawHeight);
    }

    /** Base rayon halos: plus serre que l ancien x2 x(4/3) pour eviter l auréole trop large / dispersée. */
    constexpr float kIlluminatedHaloDiameterScale = 1.16f;
    constexpr float kIlluminatedHaloDiameterBoost = 1.0f;
    const float haloVisualMul = config.compactGlowEnabled ? 1.78f : 2.18f;
    const float glowBaseMax =
        baseMax *
        kIlluminatedHaloDiameterScale *
        kIlluminatedHaloDiameterBoost *
        haloVisualMul *
        config.glowRadius;
    Uint8 prevR = 255;
    Uint8 prevG = 255;
    Uint8 prevB = 255;
    Uint8 prevA = 255;
    SDL_GetTextureColorMod(glowTex, &prevR, &prevG, &prevB);
    SDL_GetTextureAlphaMod(glowTex, &prevA);
    Uint8 prevShellR = 255;
    Uint8 prevShellG = 255;
    Uint8 prevShellB = 255;
    Uint8 prevShellA = 255;
    if (shellTex != nullptr)
    {
        SDL_GetTextureColorMod(shellTex, &prevShellR, &prevShellG, &prevShellB);
        SDL_GetTextureAlphaMod(shellTex, &prevShellA);
    }

    SDL_BlendMode prevBlend = SDL_BLENDMODE_BLEND;
    SDL_GetTextureBlendMode(glowTex, &prevBlend);
    // ADD sur mer: empilement des 3 disques saturait le centre; alphas + texture radiale adoucies.
    SDL_SetTextureBlendMode(glowTex, SDL_BLENDMODE_ADD);

    const auto drawGlowDisc = [&](SDL_Texture* texture, float diameterPixels, float alphaMul) {
        if (diameterPixels <= 0.0f || alphaMul <= 0.0f)
        {
            return;
        }

        SDL_FRect dst{};
        dst.x = centerX - diameterPixels * 0.5f;
        dst.y = centerY - diameterPixels * 0.5f;
        dst.w = diameterPixels;
        dst.h = diameterPixels;
        SDL_SetTextureColorMod(texture, glowR, glowG, glowB);
        const float clampedAlpha = (std::clamp)(alphaMul, 0.0f, 255.0f);
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(std::lround(clampedAlpha)));
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
    };

    const auto drawRotatedGlowDisc = [&](SDL_Texture* texture, float widthPixels, float heightPixels, float alphaMul) {
        if (widthPixels <= 0.0f || heightPixels <= 0.0f || alphaMul <= 0.0f)
        {
            return;
        }

        SDL_FRect dst{};
        dst.x = centerX - widthPixels * 0.5f;
        dst.y = centerY - heightPixels * 0.5f;
        dst.w = widthPixels;
        dst.h = heightPixels;
        SDL_SetTextureColorMod(texture, glowR, glowG, glowB);
        const float clampedAlpha = (std::clamp)(alphaMul, 0.0f, 255.0f);
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(std::lround(clampedAlpha)));
        SDL_RenderTextureRotated(renderer, texture, nullptr, &dst, rotationDeg, nullptr, SDL_FLIP_NONE);
    };

    // Trois couches rapprochées (diametres relatifs plus "sphère" que large plateau diffus).
    const float kHaloOuterDiamMul = config.compactGlowEnabled ? 1.32f : 1.72f;
    const float kHaloMidDiamMul = config.compactGlowEnabled ? 1.17f : 1.44f;
    const float kHaloInnerDiamMul = config.compactGlowEnabled ? 1.05f : 1.20f;
    const float haloOuterDiamMul =
        1.0f + ((kHaloOuterDiamMul - 1.0f) * config.glowDispersion);
    const float haloMidDiamMul =
        1.0f + ((kHaloMidDiamMul - 1.0f) * config.glowDispersion);
    const float haloInnerDiamMul =
        1.0f + ((kHaloInnerDiamMul - 1.0f) * config.glowDispersion);
    const float glowAlphaMul = config.glowIntensity;
    const float outerAlphaBase = config.compactGlowEnabled ? 7.0f : 16.0f;
    const float midAlphaBase = config.compactGlowEnabled ? 24.0f : 38.0f;
    const float innerAlphaBase = config.compactGlowEnabled ? 72.0f : 64.0f;

    if (config.outerLayerEnabled)
    {
        drawGlowDisc(glowTex, glowBaseMax * haloOuterDiamMul, outerAlphaBase * glowAlphaMul);
    }
    if (config.midLayerEnabled)
    {
        drawGlowDisc(glowTex, glowBaseMax * haloMidDiamMul, midAlphaBase * glowAlphaMul);
    }
    if (config.innerLayerEnabled)
    {
        drawGlowDisc(glowTex, glowBaseMax * haloInnerDiamMul, innerAlphaBase * glowAlphaMul);
    }

    if (shellTex != nullptr && config.colorShellIntensity > 0.001f)
    {
        const float shellDiameter = glowBaseMax * config.colorShellRadius;
        const float smearMul = 1.0f + (config.motionSmear * 0.82f);
        const float smearHeightMul = 1.0f - (config.motionSmear * 0.16f);
        drawRotatedGlowDisc(
            shellTex,
            shellDiameter * smearMul,
            shellDiameter * smearHeightMul,
            118.0f * config.colorShellIntensity);
    }

    if (config.coreWhiteIntensity > 0.001f)
    {
        const float coreDiameter = baseMax * config.coreWhiteRadius;
        SDL_SetTextureColorMod(glowTex, 255, 255, 255);
        const float clampedAlpha = (std::clamp)(config.coreWhiteIntensity * 255.0f, 0.0f, 255.0f);
        SDL_SetTextureAlphaMod(glowTex, static_cast<Uint8>(std::lround(clampedAlpha)));
        SDL_FRect coreDst{};
        coreDst.x = centerX - coreDiameter * 0.5f;
        coreDst.y = centerY - coreDiameter * 0.5f;
        coreDst.w = coreDiameter;
        coreDst.h = coreDiameter;
        SDL_RenderTexture(renderer, glowTex, nullptr, &coreDst);
    }

    SDL_SetTextureBlendMode(glowTex, prevBlend);
    SDL_SetTextureColorMod(glowTex, prevR, prevG, prevB);
    SDL_SetTextureAlphaMod(glowTex, prevA);
    if (shellTex != nullptr)
    {
        SDL_SetTextureColorMod(shellTex, prevShellR, prevShellG, prevShellB);
        SDL_SetTextureAlphaMod(shellTex, prevShellA);
    }

    const float spriteOpacityFloat = (std::clamp)(config.spriteOpacity * 255.0f, 0.0f, 255.0f);
    this->drawInternal(
        centerX,
        centerY,
        scale,
        rotationDeg,
        flipHorizontal,
        flipVertical,
        nullptr,
        static_cast<std::uint8_t>(std::lround(spriteOpacityFloat)));

    if (config.spriteBoost > 0.001f)
    {
        const float tintStrength = (std::clamp)(config.spriteTintStrength, 0.0f, 1.0f);
        const auto mixTowardGlow = [&](std::uint8_t channel) -> std::uint8_t {
            const float mixed =
                255.0f + ((static_cast<float>(channel) - 255.0f) * tintStrength);
            return static_cast<std::uint8_t>(std::lround((std::clamp)(mixed, 0.0f, 255.0f)));
        };
        const std::uint8_t spriteBoostTint[3] = {
            mixTowardGlow(glowR),
            mixTowardGlow(glowG),
            mixTowardGlow(glowB),
        };
        const float spriteBoostAlphaFloat =
            (std::clamp)(config.spriteBoost * 255.0f, 0.0f, 255.0f);
        this->drawInternal(
            centerX,
            centerY,
            scale,
            rotationDeg,
            flipHorizontal,
            flipVertical,
            spriteBoostTint,
            static_cast<std::uint8_t>(std::lround(spriteBoostAlphaFloat)),
            static_cast<int>(SDL_BLENDMODE_ADD));
    }

    // Texture ColorMod + batch SDL: sans flush, plusieurs boulets partageant la meme texture spritesheet
    // peuvent melanger les etats entre soumissions quads (couleur qui "glisse" en vol).
    SDL_FlushRenderer(renderer);
}
