#include "game/vfx/vfx-classic.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <RC2D/RC2D_storage.h>
#include <cJSON.h>

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
} // namespace

VFXClassic::VFXClassic(void)
    : spritesheetImage{},
      frames{},
      defaultFps(12.0f),
      animationTotalDurationMs(0),
      playbackSeconds(0.0f),
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

int VFXClassic::getCurrentFrameIndex(void) const
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
    float phase = std::fmod(sampleTime, periodSec);
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
    if (!this->isLoaded() || this->isFinished())
    {
        return;
    }

    const int frameIndex = this->getCurrentFrameIndex();
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
}
