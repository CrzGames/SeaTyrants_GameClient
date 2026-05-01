#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>

#include "core/context.h"

static std::vector<std::string> titleAssetCache_scanAssetFiles(
    const char* rootPath,
    const std::vector<const char*>& allowedExtensions)
{
    std::vector<std::string> paths{};

    std::error_code scanError{};
    const std::filesystem::path root(rootPath);
    if (!std::filesystem::exists(root, scanError))
    {
        return paths;
    }

    for (std::filesystem::recursive_directory_iterator it(root, scanError), end; it != end; it.increment(scanError))
    {
        if (scanError)
        {
            break;
        }

        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(scanError))
        {
            continue;
        }

        std::string normalized = entry.path().generic_string();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return static_cast<char>((c == '\\') ? '/' : c);
        });

        bool allowed = allowedExtensions.empty();
        for (const char* extension : allowedExtensions)
        {
            if (normalized.size() >= std::char_traits<char>::length(extension) &&
                normalized.compare(normalized.size() - std::char_traits<char>::length(extension),
                                   std::char_traits<char>::length(extension),
                                   extension) == 0)
            {
                allowed = true;
                break;
            }
        }

        if (!allowed)
        {
            continue;
        }

        if (normalized.rfind("./", 0) == 0)
        {
            normalized.erase(0, 2);
        }

        paths.push_back(normalized);
    }

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

static const std::vector<float>& titleAssetCache_getKnownFontSizes(const std::string& normalizedPath)
{
    static const std::vector<float> kTradeWindsSizes = {15.0f, 18.0f};
    static const std::vector<float> kSegoeRegularSizes = {13.0f, 14.0f, 17.0f, 18.0f, 20.0f, 24.0f};
    static const std::vector<float> kSegoeSemiboldSizes = {14.0f, 20.0f, 24.0f};
    static const std::vector<float> kDefaultSizes = {16.0f};

    if (normalizedPath == "assets/fonts/TradeWinds-Regular.ttf")
    {
        return kTradeWindsSizes;
    }

    if (normalizedPath == "assets/fonts/SegoeUI-Regular.ttf")
    {
        return kSegoeRegularSizes;
    }

    if (normalizedPath == "assets/fonts/SegoeUI-Semibold.ttf")
    {
        return kSegoeSemiboldSizes;
    }

    return kDefaultSizes;
}

static const char* titleAssetCache_assetKindLabel(TitleAssetKind kind)
{
    switch (kind)
    {
        case TitleAssetKind::IMAGE: return "image";
        case TitleAssetKind::FONT: return "font";
        case TitleAssetKind::AUDIO: return "audio";
        case TitleAssetKind::VIDEO: return "video";
        default: return "asset";
    }
}

TitleAssetCache::TitleAssetCache()
    : cachedImages{},
      cachedImageData{},
      cachedFonts{},
      cachedAudio{},
      warmedVideos{},
      countedPreloadedFontPaths{},
      preloadEntries{},
      preloadProgress{},
      preloadIndex(0)
{
}

TitleAssetCache::~TitleAssetCache()
{
    this->clear();
}

void TitleAssetCache::clear()
{
    for (auto& it : this->cachedImageData)
    {
        rc2d_graphics_freeImageData(&it.second);
    }
    this->cachedImageData.clear();

    for (auto& it : this->cachedImages)
    {
        rc2d_graphics_freeImage(&it.second);
    }
    this->cachedImages.clear();

    for (auto& it : this->cachedFonts)
    {
        rc2d_graphics_closeFont(&it.second);
    }
    this->cachedFonts.clear();

    for (auto& it : this->cachedAudio)
    {
        rc2d_audio_destroy(it.second);
    }
    this->cachedAudio.clear();

    this->warmedVideos.clear();
    this->countedPreloadedFontPaths.clear();
    this->preloadEntries.clear();
    this->preloadProgress = TitleAssetPreloadProgress{};
    this->preloadIndex = 0;
}

void TitleAssetCache::beginFullPreload()
{
    this->rebuildPreloadEntries();
}

bool TitleAssetCache::preloadNextBatch(double maxMilliseconds, std::size_t maxEntries)
{
    if (this->preloadEntries.empty() || this->preloadProgress.finished)
    {
        return false;
    }

    const Uint64 perfFreq = SDL_GetPerformanceFrequency();
    const Uint64 startTicks = SDL_GetPerformanceCounter();
    const double maxSeconds = (maxMilliseconds > 0.0) ? (maxMilliseconds / 1000.0) : 0.0;

    std::size_t processedThisBatch = 0;
    while (this->preloadIndex < this->preloadEntries.size())
    {
        const PreloadEntry& entry = this->preloadEntries[this->preloadIndex];
        this->preloadProgress.currentKind = entry.kind;
        this->preloadProgress.currentPath = entry.path;
        const bool loadedOk = this->processPreloadEntry(entry);

        ++this->preloadIndex;
        ++this->preloadProgress.processedEntries;
        ++processedThisBatch;

        if (!loadedOk)
        {
            ++this->preloadProgress.failedEntries;
        }

        if (this->preloadIndex >= this->preloadEntries.size())
        {
            this->preloadProgress.finished = true;
            break;
        }

        if (maxEntries > 0 && processedThisBatch >= maxEntries)
        {
            break;
        }

        if (maxSeconds > 0.0 && perfFreq > 0)
        {
            const Uint64 nowTicks = SDL_GetPerformanceCounter();
            const double elapsedSeconds = static_cast<double>(nowTicks - startTicks) / static_cast<double>(perfFreq);
            if (elapsedSeconds >= maxSeconds)
            {
                break;
            }
        }
    }

    return processedThisBatch > 0;
}

const TitleAssetPreloadProgress& TitleAssetCache::getPreloadProgress() const
{
    return this->preloadProgress;
}

bool TitleAssetCache::isPreloadFinished() const
{
    return this->preloadProgress.finished;
}

RC2D_Image TitleAssetCache::loadImage(const char* storagePath, RC2D_StorageKind storageKind)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldCacheImage(normalizedPath, storageKind))
    {
        return rc2d_graphics_loadImageFromStorage(storagePath, storageKind);
    }

    return this->ensureCachedImage(normalizedPath);
}

RC2D_ImageData TitleAssetCache::loadImageData(const char* storagePath, RC2D_StorageKind storageKind)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldCacheImageData(normalizedPath, storageKind))
    {
        return rc2d_graphics_loadImageDataFromStorage(storagePath, storageKind);
    }

    return this->ensureCachedImageData(normalizedPath);
}

RC2D_Font TitleAssetCache::openFont(const char* storagePath, RC2D_StorageKind storageKind, float fontSize)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldCacheFont(normalizedPath, storageKind))
    {
        return rc2d_graphics_openFontFromStorage(storagePath, storageKind, fontSize);
    }

    return this->ensureCachedFont(normalizedPath, fontSize);
}

MIX_Audio* TitleAssetCache::loadAudio(const char* storagePath, RC2D_StorageKind storageKind, bool predecode)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldCacheAudio(normalizedPath, storageKind))
    {
        return rc2d_audio_loadAudioFromStorage(storagePath, storageKind, predecode);
    }

    return this->ensureCachedAudio(normalizedPath, predecode);
}

int TitleAssetCache::openVideo(RC2D_Video* video, const char* storagePath, RC2D_StorageKind storageKind)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (this->shouldManageVideo(normalizedPath, storageKind) && !this->isVideoWarmed(normalizedPath))
    {
        this->ensureVideoWarmed(normalizedPath);
    }

    return rc2d_video_openFromStorage(video, storagePath, storageKind);
}

void TitleAssetCache::evictImage(const char* storagePath, RC2D_StorageKind storageKind)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldCacheImage(normalizedPath, storageKind))
    {
        return;
    }

    auto it = this->cachedImages.find(normalizedPath);
    if (it == this->cachedImages.end())
    {
        return;
    }

    rc2d_graphics_freeImage(&it->second);
    this->cachedImages.erase(it);
}

void TitleAssetCache::evictVideo(const char* storagePath, RC2D_StorageKind storageKind)
{
    const std::string normalizedPath = normalizeStoragePath(storagePath);
    if (!this->shouldManageVideo(normalizedPath, storageKind))
    {
        return;
    }

    this->warmedVideos.erase(
        std::remove(this->warmedVideos.begin(), this->warmedVideos.end(), normalizedPath),
        this->warmedVideos.end());
}

void TitleAssetCache::releaseImage(RC2D_Image* image)
{
    if (image == nullptr || image->sdl_texture == nullptr)
    {
        return;
    }

    for (const auto& it : this->cachedImages)
    {
        if (it.second.sdl_texture == image->sdl_texture)
        {
            image->sdl_texture = nullptr;
            return;
        }
    }

    rc2d_graphics_freeImage(image);
}

void TitleAssetCache::releaseImageData(RC2D_ImageData* imageData)
{
    if (imageData == nullptr || imageData->sdl_surface == nullptr)
    {
        return;
    }

    for (const auto& it : this->cachedImageData)
    {
        if (it.second.sdl_surface == imageData->sdl_surface)
        {
            imageData->sdl_surface = nullptr;
            return;
        }
    }

    rc2d_graphics_freeImageData(imageData);
}

void TitleAssetCache::releaseFont(RC2D_Font* font)
{
    if (font == nullptr || font->sdl_font == nullptr)
    {
        return;
    }

    for (const auto& it : this->cachedFonts)
    {
        if (it.second.sdl_font == font->sdl_font)
        {
            font->sdl_font = nullptr;
            font->fontSize = 0.0f;
            font->style = TTF_STYLE_NORMAL;
            font->alignment = TTF_HORIZONTAL_ALIGN_LEFT;
            font->_file_data = nullptr;
            return;
        }
    }

    rc2d_graphics_closeFont(font);
}

void TitleAssetCache::releaseAudio(MIX_Audio* audio)
{
    if (audio == nullptr)
    {
        return;
    }

    for (const auto& it : this->cachedAudio)
    {
        if (it.second == audio)
        {
            return;
        }
    }

    rc2d_audio_destroy(audio);
}

bool TitleAssetCache::FontKey::operator<(const FontKey& other) const
{
    if (this->path != other.path)
    {
        return this->path < other.path;
    }

    return this->fontSizeMilli < other.fontSizeMilli;
}

bool TitleAssetCache::AudioKey::operator<(const AudioKey& other) const
{
    if (this->path != other.path)
    {
        return this->path < other.path;
    }

    return static_cast<int>(this->predecode) < static_cast<int>(other.predecode);
}

std::string TitleAssetCache::normalizeStoragePath(const char* storagePath)
{
    if (storagePath == nullptr)
    {
        return {};
    }

    std::string normalized(storagePath);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    while (normalized.rfind("./", 0) == 0)
    {
        normalized.erase(0, 2);
    }

    return normalized;
}

bool TitleAssetCache::hasExtension(const std::string& path, const char* extension)
{
    const std::size_t extensionSize = std::char_traits<char>::length(extension);
    return path.size() >= extensionSize &&
           path.compare(path.size() - extensionSize, extensionSize, extension) == 0;
}

bool TitleAssetCache::startsWith(const std::string& value, const char* prefix)
{
    const std::size_t prefixSize = std::char_traits<char>::length(prefix);
    return value.size() >= prefixSize && value.compare(0, prefixSize, prefix) == 0;
}

int TitleAssetCache::toFontSizeMilli(float fontSize)
{
    return static_cast<int>(std::lround(static_cast<double>(fontSize) * 1000.0));
}

bool TitleAssetCache::shouldCacheImage(const std::string& normalizedPath, RC2D_StorageKind storageKind) const
{
    return storageKind == RC2D_STORAGE_TITLE &&
           startsWith(normalizedPath, "assets/images/") &&
           hasExtension(normalizedPath, ".png");
}

bool TitleAssetCache::shouldCacheImageData(const std::string& normalizedPath, RC2D_StorageKind storageKind) const
{
    return this->shouldCacheImage(normalizedPath, storageKind);
}

bool TitleAssetCache::shouldCacheFont(const std::string& normalizedPath, RC2D_StorageKind storageKind) const
{
    return storageKind == RC2D_STORAGE_TITLE &&
           startsWith(normalizedPath, "assets/fonts/") &&
           hasExtension(normalizedPath, ".ttf");
}

bool TitleAssetCache::shouldCacheAudio(const std::string& normalizedPath, RC2D_StorageKind storageKind) const
{
    return storageKind == RC2D_STORAGE_TITLE &&
           startsWith(normalizedPath, "assets/sounds/");
}

bool TitleAssetCache::shouldManageVideo(const std::string& normalizedPath, RC2D_StorageKind storageKind) const
{
    return storageKind == RC2D_STORAGE_TITLE &&
           startsWith(normalizedPath, "assets/videos/");
}

RC2D_Image& TitleAssetCache::ensureCachedImage(const std::string& normalizedPath)
{
    auto it = this->cachedImages.find(normalizedPath);
    if (it != this->cachedImages.end())
    {
        return it->second;
    }

    RC2D_Image image = rc2d_graphics_loadImageFromStorage(normalizedPath.c_str(), RC2D_STORAGE_TITLE);
    auto inserted = this->cachedImages.emplace(normalizedPath, image);
    return inserted.first->second;
}

RC2D_ImageData& TitleAssetCache::ensureCachedImageData(const std::string& normalizedPath)
{
    auto it = this->cachedImageData.find(normalizedPath);
    if (it != this->cachedImageData.end())
    {
        return it->second;
    }

    RC2D_ImageData imageData = rc2d_graphics_loadImageDataFromStorage(normalizedPath.c_str(), RC2D_STORAGE_TITLE);
    auto inserted = this->cachedImageData.emplace(normalizedPath, imageData);
    return inserted.first->second;
}

RC2D_Font& TitleAssetCache::ensureCachedFont(const std::string& normalizedPath, float fontSize)
{
    const FontKey key{normalizedPath, toFontSizeMilli(fontSize)};
    auto it = this->cachedFonts.find(key);
    if (it != this->cachedFonts.end())
    {
        return it->second;
    }

    RC2D_Font font = rc2d_graphics_openFontFromStorage(normalizedPath.c_str(), RC2D_STORAGE_TITLE, fontSize);
    auto inserted = this->cachedFonts.emplace(key, font);
    return inserted.first->second;
}

MIX_Audio* TitleAssetCache::ensureCachedAudio(const std::string& normalizedPath, bool predecode)
{
    AudioKey key{normalizedPath, predecode};
    auto it = this->cachedAudio.find(key);
    if (it != this->cachedAudio.end())
    {
        return it->second;
    }

    MIX_Audio* audio = rc2d_audio_loadAudioFromStorage(normalizedPath.c_str(), RC2D_STORAGE_TITLE, predecode);
    this->cachedAudio.emplace(key, audio);
    return audio;
}

void TitleAssetCache::ensureVideoWarmed(const std::string& normalizedPath)
{
    if (this->isVideoWarmed(normalizedPath))
    {
        return;
    }

    RC2D_Video video{};
    if (rc2d_video_openFromStorage(&video, normalizedPath.c_str(), RC2D_STORAGE_TITLE) == 0)
    {
        rc2d_video_close(&video);
        this->warmedVideos.push_back(normalizedPath);
    }
}

bool TitleAssetCache::isVideoWarmed(const std::string& normalizedPath) const
{
    return std::find(this->warmedVideos.begin(), this->warmedVideos.end(), normalizedPath) != this->warmedVideos.end();
}

void TitleAssetCache::rebuildPreloadEntries()
{
    this->preloadEntries.clear();
    this->countedPreloadedFontPaths.clear();
    this->preloadProgress = TitleAssetPreloadProgress{};
    this->preloadProgress.started = true;
    this->preloadIndex = 0;

    std::vector<std::string> imagePaths = titleAssetCache_scanAssetFiles("assets/images", {".png"});
    const std::string loadingBackgroundPath = "assets/images/ui-scene-loading/background.png";
    auto loadingBackgroundIt = std::find(imagePaths.begin(), imagePaths.end(), loadingBackgroundPath);
    if (loadingBackgroundIt != imagePaths.end() && loadingBackgroundIt != imagePaths.begin())
    {
        const std::string prioritizedPath = *loadingBackgroundIt;
        imagePaths.erase(loadingBackgroundIt);
        imagePaths.insert(imagePaths.begin(), prioritizedPath);
    }
    for (const std::string& path : imagePaths)
    {
        this->preloadEntries.push_back(PreloadEntry{TitleAssetKind::IMAGE, path, 0.0f, true});
    }

    const std::vector<std::string> fontPaths = titleAssetCache_scanAssetFiles("assets/fonts", {".ttf"});
    for (const std::string& path : fontPaths)
    {
        const std::vector<float>& sizes = titleAssetCache_getKnownFontSizes(path);
        for (float size : sizes)
        {
            this->preloadEntries.push_back(PreloadEntry{TitleAssetKind::FONT, path, size, true});
        }
    }

    const std::vector<std::string> audioPaths =
        titleAssetCache_scanAssetFiles("assets/sounds", {".opus", ".ogg", ".wav", ".mp3", ".flac"});
    for (const std::string& path : audioPaths)
    {
        this->preloadEntries.push_back(PreloadEntry{TitleAssetKind::AUDIO, path, 0.0f, true});
    }

    const std::vector<std::string> videoPaths = titleAssetCache_scanAssetFiles("assets/videos", {".mp4", ".mov", ".webm", ".mkv"});
    for (const std::string& path : videoPaths)
    {
        this->preloadEntries.push_back(PreloadEntry{TitleAssetKind::VIDEO, path, 0.0f, true});
    }

    this->preloadProgress.totalEntries = this->preloadEntries.size();
    if (this->preloadEntries.empty())
    {
        this->preloadProgress.finished = true;
    }
}

bool TitleAssetCache::processPreloadEntry(const PreloadEntry& entry)
{
    switch (entry.kind)
    {
        case TitleAssetKind::IMAGE:
        {
            RC2D_Image image = this->ensureCachedImage(entry.path);
            if (image.sdl_texture == nullptr)
            {
                RC2D_log(RC2D_LOG_ERROR, "TitleAssetCache: failed to preload image '%s'", entry.path.c_str());
                return false;
            }

            ++this->preloadProgress.loadedImageCount;
            return true;
        }

        case TitleAssetKind::FONT:
        {
            RC2D_Font font = this->ensureCachedFont(entry.path, entry.fontSize);
            if (font.sdl_font == nullptr)
            {
                RC2D_log(
                    RC2D_LOG_ERROR,
                    "TitleAssetCache: failed to preload font '%s' size %.2f",
                    entry.path.c_str(),
                    static_cast<double>(entry.fontSize));
                return false;
            }

            if (std::find(
                    this->countedPreloadedFontPaths.begin(),
                    this->countedPreloadedFontPaths.end(),
                    entry.path) == this->countedPreloadedFontPaths.end())
            {
                this->countedPreloadedFontPaths.push_back(entry.path);
                ++this->preloadProgress.loadedFontCount;
            }

            return true;
        }

        case TitleAssetKind::AUDIO:
        {
            MIX_Audio* audio = this->ensureCachedAudio(entry.path, entry.audioPredecode);
            if (audio == nullptr)
            {
                RC2D_log(RC2D_LOG_ERROR, "TitleAssetCache: failed to preload audio '%s'", entry.path.c_str());
                return false;
            }

            ++this->preloadProgress.loadedAudioCount;
            return true;
        }

        case TitleAssetKind::VIDEO:
        {
            this->ensureVideoWarmed(entry.path);
            if (!this->isVideoWarmed(entry.path))
            {
                RC2D_log(RC2D_LOG_ERROR, "TitleAssetCache: failed to warm video '%s'", entry.path.c_str());
                return false;
            }

            ++this->preloadProgress.warmedVideoCount;
            return true;
        }

        default:
            RC2D_log(RC2D_LOG_ERROR, "TitleAssetCache: unsupported preload kind '%s'", titleAssetCache_assetKindLabel(entry.kind));
            return false;
    }
}

RC2D_Image LoadStorageImage(const char* storagePath, RC2D_StorageKind storageKind)
{
    return GetTitleAssetCache().loadImage(storagePath, storageKind);
}

RC2D_ImageData LoadStorageImageData(const char* storagePath, RC2D_StorageKind storageKind)
{
    return GetTitleAssetCache().loadImageData(storagePath, storageKind);
}

RC2D_Font OpenStorageFont(const char* storagePath, RC2D_StorageKind storageKind, float fontSize)
{
    return GetTitleAssetCache().openFont(storagePath, storageKind, fontSize);
}

MIX_Audio* LoadStorageAudio(const char* storagePath, RC2D_StorageKind storageKind, bool predecode)
{
    return GetTitleAssetCache().loadAudio(storagePath, storageKind, predecode);
}

int OpenStorageVideo(RC2D_Video* video, const char* storagePath, RC2D_StorageKind storageKind)
{
    return GetTitleAssetCache().openVideo(video, storagePath, storageKind);
}

void ResetStorageImageRef(RC2D_Image* image)
{
    if (image == nullptr)
    {
        return;
    }

    *image = RC2D_Image{};
}

void ResetStorageImageDataRef(RC2D_ImageData* imageData)
{
    if (imageData == nullptr)
    {
        return;
    }

    *imageData = RC2D_ImageData{};
}

void ResetStorageFontRef(RC2D_Font* font)
{
    if (font == nullptr)
    {
        return;
    }

    *font = RC2D_Font{};
}

void ResetStorageAudioRef(MIX_Audio** audio)
{
    if (audio == nullptr)
    {
        return;
    }

    *audio = nullptr;
}

void ReleaseStorageImage(RC2D_Image* image)
{
    GetTitleAssetCache().releaseImage(image);
}

void ReleaseStorageImageData(RC2D_ImageData* imageData)
{
    GetTitleAssetCache().releaseImageData(imageData);
}

void ReleaseStorageFont(RC2D_Font* font)
{
    GetTitleAssetCache().releaseFont(font);
}

void ReleaseStorageAudio(MIX_Audio* audio)
{
    GetTitleAssetCache().releaseAudio(audio);
}
