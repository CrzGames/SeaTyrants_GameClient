#include "game/vfx/vfx.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <utility>

#include <SDL3/SDL.h>

#include <RC2D/RC2D_storage.h>
#include <cJSON.h>

#include "core/context.h"

// =============================================================================
// Helpers parse spritesheet (sans namespace anonyme)
// =============================================================================

/**
 * @brief Frame temporaire parsee depuis le JSON spritesheet.
 */
struct ParsedFrame {
    int index = 0;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

/**
 * @brief Resultat temporaire du parse spritesheet.
 */
struct ParsedSpritesheet {
    std::string imageFile;
    float fps = 12.0f;
    std::vector<ParsedFrame> frames;
};

/**
 * @brief Normalise un chemin en slash '/'.
 */
static std::string normalizePathSlashes(const std::string& path)
{
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

/**
 * @brief Trim ASCII en debut/fin de chaine.
 */
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

/**
 * @brief Convertit une chaine en lowercase ASCII.
 */
static std::string toLowerAscii(const std::string& value)
{
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/**
 * @brief Normalise un chemin dossier brut (trim + slash + suppression trailing '/').
 */
static std::string normalizeFolderPath(const char* rawPath)
{
    if (rawPath == nullptr)
    {
        return {};
    }

    std::string path = trimAscii(normalizePathSlashes(rawPath));
    while (path.size() > 1U && path.back() == '/')
    {
        path.pop_back();
    }
    return path;
}

/**
 * @brief Extrait le nom de dossier terminal d'un chemin.
 */
static std::string folderNameFromPath(const std::string& folderPath)
{
    if (folderPath.empty())
    {
        return {};
    }

    const std::filesystem::path path(folderPath);
    std::string folderName = path.filename().string();
    if (folderName.empty())
    {
        folderName = path.stem().string();
    }
    return trimAscii(folderName);
}

/**
 * @brief Lit un fichier texte depuis RC2D_STORAGE_TITLE.
 */
static bool readTextFileFromStorage(
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

/**
 * @brief Fallback de chemin JSON spritesheet depuis un dossier VFX.
 */
static std::string resolveSpritesheetJsonPathFallback(const std::string& vfxFolderPath)
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

    const std::filesystem::path candidate = folder / (folderName + "-spritesheet.json");
    return normalizePathSlashes(candidate.string());
}

/**
 * @brief Resout le chemin image de spritesheet.
 */
static std::string resolveSpritesheetImagePath(
    const std::string& spritesheetJsonPath,
    const std::string& imageFieldValue)
{
    if (imageFieldValue.empty())
    {
        return {};
    }

    const std::string normalizedImage = normalizePathSlashes(imageFieldValue);
    if (normalizedImage.rfind("assets/", 0U) == 0U)
    {
        return normalizedImage;
    }

    std::filesystem::path imagePath(normalizedImage);
    if (imagePath.is_absolute())
    {
        return normalizePathSlashes(imagePath.string());
    }

    const std::filesystem::path jsonDir = std::filesystem::path(spritesheetJsonPath).parent_path();
    return normalizePathSlashes((jsonDir / imagePath).string());
}

/**
 * @brief Parse un spritesheet JSON custom ({fps, image, frames[]}).
 */
static bool parseSpritesheetJson(
    const char* jsonText,
    ParsedSpritesheet* outSpritesheet)
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

    std::string imageFieldValue;
    const cJSON* imageNode = cJSON_GetObjectItemCaseSensitive(root, "image");
    if (cJSON_IsString(imageNode) && imageNode->valuestring != nullptr)
    {
        imageFieldValue = normalizePathSlashes(imageNode->valuestring);
    }
    if (imageFieldValue.empty())
    {
        const cJSON* metaNode = cJSON_GetObjectItemCaseSensitive(root, "meta");
        const cJSON* metaImageNode =
            cJSON_IsObject(metaNode) ? cJSON_GetObjectItemCaseSensitive(metaNode, "image") : nullptr;
        if (cJSON_IsString(metaImageNode) && metaImageNode->valuestring != nullptr)
        {
            imageFieldValue = normalizePathSlashes(metaImageNode->valuestring);
        }
    }

    std::vector<ParsedFrame> parsedFrames;
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

        parsedFrames.push_back(ParsedFrame{frameIndex, x, y, w, h});
    }

    if (parsedFrames.empty())
    {
        cJSON_Delete(root);
        return false;
    }

    std::sort(parsedFrames.begin(), parsedFrames.end(), [](const ParsedFrame& a, const ParsedFrame& b) {
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
    outSpritesheet->imageFile = imageFieldValue;
    outSpritesheet->frames = std::move(parsedFrames);
    return true;
}

namespace
{
constexpr float kMotionTrailRotationJitterMaxDeg = 45.0f;
constexpr float kTrailPieceDrawScaleLifeMin = 0.08f;
constexpr float kTrailPieceMotionNoiseAmp = 2.15f;
constexpr float kTrailPieceMotionNoiseRateX = 0.48f;
constexpr float kTrailPieceMotionNoiseRateY = 0.39f;
constexpr float kTrailPieceWakeImpulseUnits = 1.75f;
constexpr float kTrailPieceWakeDecayPerSec = 1.32f;
constexpr float kTrailPieceSpinDecayPerSec = 2.05f;
constexpr float kTrailPieceSpinOmegaMaxDegPerSec = 5.2f;
constexpr float kMotionTrailConeDepthBySpreadRatio = 1.0f;
constexpr float kTrailConePopupRearAxisRad = 1.57079632679f;
constexpr float kIdleRingPieceStaggerSec = 0.055f;

std::mt19937& vfxTrailRng(void)
{
    static std::mt19937 gen = []() {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd()};
        return std::mt19937(seed);
    }();
    return gen;
}

uint32_t vfxHashU32(uint32_t x)
{
    x ^= x >> 16U;
    x *= 0x85ebca6bu;
    x ^= x >> 13U;
    x *= 0xc2b2ae35u;
    x ^= x >> 16U;
    return x;
}

float vfxSmoothNoise1D(uint32_t seed, float t)
{
    const int k0 = static_cast<int>(std::floor(t));
    const int k1 = k0 + 1;
    const float f = t - static_cast<float>(k0);
    const float u = f * f * (3.0f - 2.0f * f);
    const uint32_t h0 =
        vfxHashU32(seed ^ (0x27d4eb2du + static_cast<uint32_t>(k0) * 0x9e3779b9u));
    const uint32_t h1 =
        vfxHashU32(seed ^ (0x27d4eb2du + static_cast<uint32_t>(k1) * 0x9e3779b9u));
    const float v0 = static_cast<float>(h0) * (2.0f / 4294967296.0f) - 1.0f;
    const float v1 = static_cast<float>(h1) * (2.0f / 4294967296.0f) - 1.0f;
    return v0 + (v1 - v0) * u;
}

void vfxSampleTrailConeDepthAndLateral(
    float lateralSpread,
    float coneHalfAngleRad,
    float* outDepth,
    float* outLateral)
{
    if (outDepth == nullptr || outLateral == nullptr)
    {
        return;
    }
    *outDepth = 0.0f;
    *outLateral = 0.0f;
    const float spreadAbs = std::fabs(lateralSpread);
    if (spreadAbs <= 0.0001f)
    {
        return;
    }
    const float tanHalf = std::tan(coneHalfAngleRad);
    const float depthFromSpread = spreadAbs * kMotionTrailConeDepthBySpreadRatio;
    float maxDepth = depthFromSpread;
    if (tanHalf > 1.0e-5f)
    {
        const float depthCapForSpread = spreadAbs / tanHalf;
        maxDepth = (std::min)(depthFromSpread, depthCapForSpread);
    }
    if (maxDepth <= 1.0e-6f)
    {
        return;
    }
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    const float u1 = (std::max)(u01(vfxTrailRng()), 1.0e-6f);
    const float u2 = u01(vfxTrailRng());
    const float depth = maxDepth * std::sqrt(u1);
    *outDepth = depth;
    *outLateral = (2.0f * u2 - 1.0f) * depth * tanHalf;
}
} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

VFX::VFX(void)
    : spritesheetImage{},
      frames{},
      directionStates{},
      defaultVfxFps(12.0f),
      playbackSeconds(0.0f),
      activeDirectionStateKey(0),
      loaded(false),
      shipFolderPath{},
      vfxFolderPath{},
      configJsonPath{},
      spritesheetJsonPath{},
      spritesheetImagePath{}
{
}

VFX::~VFX(void)
{
    this->unload();
}

int VFX::directionStateKey(ShipDirection direction, ShipState state)
{
    const int dir = std::clamp(static_cast<int>(direction), 0, 3);
    const int st = std::clamp(static_cast<int>(state), 0, 1);
    return dir + (st * 4);
}

VFX::ShipDirection VFX::shipDirectionFromString(const char* value)
{
    if (value == nullptr)
    {
        return ShipDirection::DOWN_LEFT;
    }

    const std::string lower = toLowerAscii(value);
    if (lower == "down_left")
    {
        return ShipDirection::DOWN_LEFT;
    }
    if (lower == "up_right")
    {
        return ShipDirection::UP_RIGHT;
    }
    if (lower == "up_left")
    {
        return ShipDirection::UP_LEFT;
    }
    if (lower == "down_right")
    {
        return ShipDirection::DOWN_RIGHT;
    }
    return ShipDirection::DOWN_LEFT;
}

VFX::ShipState VFX::shipStateFromString(const char* value)
{
    if (value == nullptr)
    {
        return ShipState::HEALTHY;
    }

    const std::string lower = toLowerAscii(value);
    if (lower == "damaged" || lower == "low")
    {
        return ShipState::DAMAGED;
    }
    return ShipState::HEALTHY;
}

VFX::ShipDirection VFX::shipDirectionFromPreviewDirection(Ship::PreviewDirection direction)
{
    switch (direction)
    {
    case Ship::PreviewDirection::DOWN_LEFT:
        return ShipDirection::DOWN_LEFT;
    case Ship::PreviewDirection::UP_RIGHT:
        return ShipDirection::UP_RIGHT;
    case Ship::PreviewDirection::UP_LEFT:
        return ShipDirection::UP_LEFT;
    case Ship::PreviewDirection::DOWN_RIGHT:
        return ShipDirection::DOWN_RIGHT;
    default:
        return ShipDirection::DOWN_LEFT;
    }
}

VFX::ShipState VFX::shipStateFromHealthVisual(Ship::HealthVisual healthVisual)
{
    return (healthVisual == Ship::HealthVisual::LOW) ? ShipState::DAMAGED : ShipState::HEALTHY;
}

void VFX::setDirectionStateFromShip(const Ship& ship)
{
    // Synchronise la page [direction,state] active en se basant sur le ship runtime.
    this->activeDirectionStateKey = directionStateKey(
        shipDirectionFromPreviewDirection(ship.getCurrentPreviewDirection()),
        shipStateFromHealthVisual(ship.getHealthVisual()));
}

const VFX::DirectionStateData& VFX::currentDirectionState(void) const
{
    const int key = std::clamp(this->activeDirectionStateKey, 0, 7);
    return this->directionStates[static_cast<size_t>(key)];
}

const VFX::Instance* VFX::findInstanceById(
    const DirectionStateData& directionState,
    uint32_t instanceId) const
{
    if (instanceId == 0U)
    {
        return nullptr;
    }

    for (const Instance& instance : directionState.instances)
    {
        if (instance.instanceId == instanceId)
        {
            return &instance;
        }
    }
    return nullptr;
}

float VFX::resolvedPlaybackPeriodSeconds(void) const
{
    if (this->frames.empty())
    {
        return 0.0f;
    }

    const float fps = (std::max)(this->defaultVfxFps, 1.0f);
    return static_cast<float>(this->frames.size()) / fps;
}

float VFX::computePhaseSecondsInCycle(
    const DirectionStateData& directionState,
    const Instance& instance,
    int chainDepth) const
{
    const float period = this->resolvedPlaybackPeriodSeconds();
    if (period <= 0.0001f)
    {
        return 0.0f;
    }

    auto normalizePhase = [period](float phase) -> float {
        float normalized = std::fmod(phase, period);
        if (normalized < 0.0f)
        {
            normalized += period;
        }
        return normalized;
    };

    // Cas de base: pas d'ancre, ou recursion trop profonde (anti boucle).
    if (instance.spawnAfterInstanceId == 0U || chainDepth > 32)
    {
        return normalizePhase(this->playbackSeconds);
    }

    const Instance* anchor = this->findInstanceById(directionState, instance.spawnAfterInstanceId);
    if (anchor == nullptr)
    {
        return normalizePhase(this->playbackSeconds);
    }

    const float anchorPhase = this->computePhaseSecondsInCycle(
        directionState,
        *anchor,
        chainDepth + 1);
    const float delaySeconds = static_cast<float>(instance.spawnAfterDelayMs) / 1000.0f;
    return normalizePhase(anchorPhase - delaySeconds);
}

int VFX::computeFrameIndex(const DirectionStateData& directionState, const Instance& instance) const
{
    const int frameCount = static_cast<int>(this->frames.size());
    if (frameCount <= 0)
    {
        return 0;
    }

    const float fps = (std::max)(this->defaultVfxFps, 1.0f);
    const float phase = this->computePhaseSecondsInCycle(directionState, instance, 0);
    int index = static_cast<int>(std::floor(phase * fps));
    index %= frameCount;
    if (index < 0)
    {
        index += frameCount;
    }
    return index;
}

int VFX::runtimeTrailFrameIndex(float defaultFps, int frameCount, float ageSec)
{
    if (frameCount <= 0)
    {
        return 0;
    }
    const float fps = (std::max)(defaultFps, 1.0f);
    const float t =
        (std::isfinite(ageSec) && ageSec > 0.0f) ? ageSec : 0.0f;
    const float frameFloat = t * fps;
    const float frameCountF = static_cast<float>(frameCount);
    float frameInCycle = std::fmod(frameFloat, frameCountF);
    if (frameInCycle < 0.0f)
    {
        frameInCycle += frameCountF;
    }
    const int frameIndex = static_cast<int>(std::floor(frameInCycle));
    return (std::clamp)(frameIndex, 0, frameCount - 1);
}

void VFX::runtimeInitTrailMotionExtras(TrailPiece* piece, float moveDxTiles, float moveDyTiles)
{
    if (piece == nullptr)
    {
        return;
    }
    std::uniform_real_distribution<float> ph(0.0f, 6.28318530718f);
    piece->trailAmbientDriftPhase0 = ph(vfxTrailRng());
    piece->trailAmbientDriftPhase1 = ph(vfxTrailRng());
    std::uniform_int_distribution<uint32_t> u32(0u, 0xFFFFFFFFu);
    piece->trailNoiseSeed = u32(vfxTrailRng());
    piece->trailWakeDirX = moveDxTiles;
    piece->trailWakeDirY = moveDyTiles;
    std::uniform_real_distribution<float> spinU(-kTrailPieceSpinOmegaMaxDegPerSec, kTrailPieceSpinOmegaMaxDegPerSec);
    piece->trailSpinOmega0 = spinU(vfxTrailRng());
}

void VFX::runtimeAppendTrailPieceFromStep(
    const Instance& inst,
    float anchorShipTileX,
    float anchorShipTileY,
    float moveDxTiles,
    float moveDyTiles,
    float timeSec,
    float effectiveZoom,
    const Ship& ship,
    float spawnSpeedTilesPerSec,
    std::vector<TrailPiece>& outPieces)
{
    TrailPiece piece{};
    piece.sourceVfxInstanceId = inst.instanceId;
    piece.anchorShipTileX = anchorShipTileX;
    piece.anchorShipTileY = anchorShipTileY;
    piece.bornTimeSeconds = timeSec;
    piece.fromIdleRingCrown = false;
    float speedTilesPerSec = spawnSpeedTilesPerSec;
    if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
    {
        speedTilesPerSec = ship.getSpeedTilesPerSecond();
    }
    if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
    {
        speedTilesPerSec = 6.0f;
    }
    const float lifetimeTiles = static_cast<float>((std::max)(inst.motionTrailLifetimeTiles, 1));
    piece.timeRemainingSec = (std::max)(lifetimeTiles / speedTilesPerSec, 0.05f);
    piece.trailLifetimeInitialSec = piece.timeRemainingSec;
    piece.trailDrawOffsetX = inst.offsetX;
    piece.trailDrawOffsetY = inst.offsetY;
    if (std::isfinite(effectiveZoom) && effectiveZoom > 0.0f)
    {
        piece.anchorShipSpriteCenterOffValid = ship.getCurrentSpriteCenterOffsetPixelsForEffectiveZoom(
            effectiveZoom,
            &piece.anchorShipSpriteCenterOffXPx,
            &piece.anchorShipSpriteCenterOffYPx);
    }
    else
    {
        piece.anchorShipSpriteCenterOffValid =
            ship.getCurrentSpriteCenterOffsetPixels(&piece.anchorShipSpriteCenterOffXPx, &piece.anchorShipSpriteCenterOffYPx);
    }
    piece.trailPerpendicularJitterX = 0.0f;
    piece.trailPerpendicularJitterY = 0.0f;
    if (!inst.motionTrailStrictTilePlacement)
    {
        piece.trailDrawOffsetX += inst.motionTrailConeOffsetX;
        piece.trailDrawOffsetY += inst.motionTrailConeOffsetY;
        const float spread = (std::max)(inst.motionTrailLateralJitterRadius, 0.0f);
        if (spread > 0.0001f)
        {
            const float dirOffRad = inst.motionTrailConeDirectionOffsetDeg * (3.14159265359f / 180.0f);
            const float coneAxisRad = kTrailConePopupRearAxisRad + dirOffRad;
            const float coneAxisX = std::cos(coneAxisRad);
            const float coneAxisY = std::sin(coneAxisRad);
            const float conePerpX = -coneAxisY;
            const float conePerpY = coneAxisX;
            const float coneHalfAngleDeg = std::clamp(inst.motionTrailConeHalfAngleDeg, 2.0f, 85.0f);
            const float coneHalfAngleRad = coneHalfAngleDeg * (3.14159265359f / 180.0f);
            float depth = 0.0f;
            float lateral = 0.0f;
            vfxSampleTrailConeDepthAndLateral(spread, coneHalfAngleRad, &depth, &lateral);
            piece.trailDrawOffsetX += coneAxisX * depth;
            piece.trailDrawOffsetY += coneAxisY * depth;
            piece.trailPerpendicularJitterX = conePerpX * lateral;
            piece.trailPerpendicularJitterY = conePerpY * lateral;
        }
    }
    piece.trailRotationJitterDeg = 0.0f;
    if (inst.motionTrailRotationRandomPercent > 0)
    {
        const float p =
            static_cast<float>(std::clamp(inst.motionTrailRotationRandomPercent, 0, 100)) / 100.0f;
        std::uniform_real_distribution<float> rotU(-kMotionTrailRotationJitterMaxDeg, kMotionTrailRotationJitterMaxDeg);
        piece.trailRotationJitterDeg = p * rotU(vfxTrailRng());
    }
    VFX::runtimeInitTrailMotionExtras(&piece, moveDxTiles, moveDyTiles);
    outPieces.push_back(std::move(piece));
}

bool VFX::loadFromFolders(const char* shipFolderPath, const char* vfxFolderPath)
{
    // Toujours repartir d'un etat clean pour eviter les residus d'un precedent chargement.
    this->unload();

    // 1) Normaliser les chemins dossiers ship/vfx.
    const std::string shipFolderPathNormalized = normalizeFolderPath(shipFolderPath);
    const std::string vfxFolderPathNormalized = normalizeFolderPath(vfxFolderPath);
    if (shipFolderPathNormalized.empty() || vfxFolderPathNormalized.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFX: chemins dossiers invalides (ship='%s', vfx='%s')",
            (shipFolderPath != nullptr) ? shipFolderPath : "(null)",
            (vfxFolderPath != nullptr) ? vfxFolderPath : "(null)");
        return false;
    }

    // 2) Extraire les noms de dossiers terminaux (ship-xxx / vfx-yyy).
    const std::string shipFolderName = folderNameFromPath(shipFolderPathNormalized);
    const std::string vfxFolderName = folderNameFromPath(vfxFolderPathNormalized);
    if (shipFolderName.empty() || vfxFolderName.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFX: impossible d'extraire le nom dossier (ship='%s', vfx='%s')",
            shipFolderPathNormalized.c_str(),
            vfxFolderPathNormalized.c_str());
        return false;
    }

    // 3) Deriver le slug vfx de nom de fichier export (fx-<slug>_ship-<ship>.json).
    std::string vfxSlug = vfxFolderName;
    if (toLowerAscii(vfxSlug).rfind("vfx-", 0U) == 0U)
    {
        vfxSlug.erase(0, 4);
    }
    if (vfxSlug.empty())
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "VFX: nom de dossier VFX invalide pour le slug config: %s",
            vfxFolderName.c_str());
        return false;
    }

    // 4) Construire le chemin config gameplay du ship.
    const std::string gameplayConfigJsonPath =
        shipFolderPathNormalized + "/fx-" + vfxSlug + "_" + shipFolderName + ".json";

    // 5) Lire le JSON gameplay (storage TITLE fixe par convention projet).
    std::string configText;
    if (!readTextFileFromStorage(gameplayConfigJsonPath.c_str(), &configText))
    {
        RC2D_log(RC2D_LOG_WARN, "VFX: JSON config introuvable: %s", gameplayConfigJsonPath.c_str());
        return false;
    }

    // 6) Parser la racine JSON.
    cJSON* root = cJSON_Parse(configText.c_str());
    if (root == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "VFX: JSON config invalide: %s", gameplayConfigJsonPath.c_str());
        return false;
    }

    // 7) Recuperer le noeud gameplay (fallback root pour compat legacy).
    const cJSON* gameplayNode = cJSON_GetObjectItemCaseSensitive(root, "gameplay");
    if (!cJSON_IsObject(gameplayNode))
    {
        gameplayNode = root;
    }
    if (!cJSON_IsObject(gameplayNode))
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: key gameplay absente: %s", gameplayConfigJsonPath.c_str());
        return false;
    }

    // 8) Lire les infos gameplay globales.
    std::string parsedShipFolderPath;
    std::string parsedVfxFolderPath;
    float parsedDefaultFps = 12.0f;

    const cJSON* shipFolderPathNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "shipFolderPath");
    if (cJSON_IsString(shipFolderPathNode) && shipFolderPathNode->valuestring != nullptr)
    {
        parsedShipFolderPath = normalizePathSlashes(shipFolderPathNode->valuestring);
    }

    const cJSON* vfxFolderPathNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "vfxFolderPath");
    if (cJSON_IsString(vfxFolderPathNode) && vfxFolderPathNode->valuestring != nullptr)
    {
        parsedVfxFolderPath = normalizePathSlashes(vfxFolderPathNode->valuestring);
    }

    const cJSON* defaultFpsNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "defaultVfxFps");
    if (cJSON_IsNumber(defaultFpsNode) && std::isfinite(defaultFpsNode->valuedouble))
    {
        parsedDefaultFps = static_cast<float>(defaultFpsNode->valuedouble);
    }

    // Fallbacks si absent dans le JSON.
    if (parsedShipFolderPath.empty())
    {
        parsedShipFolderPath = shipFolderPathNormalized;
    }
    if (parsedVfxFolderPath.empty())
    {
        parsedVfxFolderPath = vfxFolderPathNormalized;
    }

    // 9) Lire le bloc editor.animation pour sourceJsonPath/defaultVfxFps.
    const cJSON* editorNode = cJSON_GetObjectItemCaseSensitive(root, "editor");
    const cJSON* animationNode =
        cJSON_IsObject(editorNode) ? cJSON_GetObjectItemCaseSensitive(editorNode, "animation") : nullptr;

    if (cJSON_IsObject(animationNode))
    {
        const cJSON* editorFpsNode = cJSON_GetObjectItemCaseSensitive(animationNode, "defaultVfxFps");
        if (cJSON_IsNumber(editorFpsNode) && std::isfinite(editorFpsNode->valuedouble))
        {
            parsedDefaultFps = static_cast<float>(editorFpsNode->valuedouble);
        }
    }
    parsedDefaultFps = (std::max)(1.0f, parsedDefaultFps);

    // 10) Resoudre le chemin JSON de la spritesheet.
    std::string parsedSpritesheetJsonPath;
    if (cJSON_IsObject(animationNode))
    {
        const cJSON* sourceJsonPathNode = cJSON_GetObjectItemCaseSensitive(animationNode, "sourceJsonPath");
        if (cJSON_IsString(sourceJsonPathNode) && sourceJsonPathNode->valuestring != nullptr)
        {
            parsedSpritesheetJsonPath = normalizePathSlashes(sourceJsonPathNode->valuestring);
        }
    }
    if (parsedSpritesheetJsonPath.empty())
    {
        const cJSON* legacySourceNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "vfxSourceJsonPath");
        if (cJSON_IsString(legacySourceNode) && legacySourceNode->valuestring != nullptr)
        {
            parsedSpritesheetJsonPath = normalizePathSlashes(legacySourceNode->valuestring);
        }
    }
    if (parsedSpritesheetJsonPath.empty())
    {
        parsedSpritesheetJsonPath = resolveSpritesheetJsonPathFallback(parsedVfxFolderPath);
    }

    // 11) Parser les pages gameplay direction/state.
    std::array<DirectionStateData, 8> parsedDirectionStates{};
    int firstDirectionStateKey = 0;
    bool hasFirstDirectionStateKey = false;

    const cJSON* directionStatesNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "directionStates");
    if (!cJSON_IsArray(directionStatesNode))
    {
        directionStatesNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "pages");
    }
    if (!cJSON_IsArray(directionStatesNode))
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: gameplay.directionStates[] absent: %s", gameplayConfigJsonPath.c_str());
        return false;
    }

    int directionStateIndex = 0;
    cJSON* directionStateNode = nullptr;
    cJSON_ArrayForEach(directionStateNode, directionStatesNode)
    {
        if (!cJSON_IsObject(directionStateNode))
        {
            directionStateIndex += 1;
            continue;
        }

        ShipDirection direction = static_cast<ShipDirection>(directionStateIndex % 4);
        ShipState state = (directionStateIndex >= 4) ? ShipState::DAMAGED : ShipState::HEALTHY;

        const cJSON* shipNode = cJSON_GetObjectItemCaseSensitive(directionStateNode, "ship");
        if (cJSON_IsObject(shipNode))
        {
            const cJSON* directionNode = cJSON_GetObjectItemCaseSensitive(shipNode, "direction");
            if (cJSON_IsString(directionNode) && directionNode->valuestring != nullptr)
            {
                direction = shipDirectionFromString(directionNode->valuestring);
            }

            const cJSON* stateNode = cJSON_GetObjectItemCaseSensitive(shipNode, "state");
            if (cJSON_IsString(stateNode) && stateNode->valuestring != nullptr)
            {
                state = shipStateFromString(stateNode->valuestring);
            }
        }

        const int key = directionStateKey(direction, state);
        DirectionStateData stateData{};

        if (cJSON_IsObject(shipNode))
        {
            const cJSON* shipDrawOrderNode = cJSON_GetObjectItemCaseSensitive(shipNode, "drawOrder");
            if (cJSON_IsNumber(shipDrawOrderNode) && std::isfinite(shipDrawOrderNode->valuedouble))
            {
                stateData.shipDrawOrder = static_cast<int>(std::llround(shipDrawOrderNode->valuedouble));
            }
        }

        const cJSON* instancesNode = cJSON_GetObjectItemCaseSensitive(directionStateNode, "instances");
        if (cJSON_IsArray(instancesNode))
        {
            cJSON* instanceNode = nullptr;
            cJSON_ArrayForEach(instanceNode, instancesNode)
            {
                if (!cJSON_IsObject(instanceNode))
                {
                    continue;
                }

                Instance instance{};

                const cJSON* instanceIdNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "instanceId");
                if (cJSON_IsNumber(instanceIdNode) && std::isfinite(instanceIdNode->valuedouble))
                {
                    const double rawId = instanceIdNode->valuedouble;
                    instance.instanceId = (rawId <= 0.0) ? 0U : static_cast<uint32_t>(std::llround(rawId));
                }

                const cJSON* offsetXNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "offsetX");
                const cJSON* offsetYNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "offsetY");
                const cJSON* rotationNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "rotationDeg");
                const cJSON* drawOrderNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "drawOrder");
                if (cJSON_IsNumber(offsetXNode) && std::isfinite(offsetXNode->valuedouble))
                {
                    instance.offsetX = static_cast<float>(offsetXNode->valuedouble);
                }
                if (cJSON_IsNumber(offsetYNode) && std::isfinite(offsetYNode->valuedouble))
                {
                    instance.offsetY = static_cast<float>(offsetYNode->valuedouble);
                }
                if (cJSON_IsNumber(rotationNode) && std::isfinite(rotationNode->valuedouble))
                {
                    instance.rotationDeg = static_cast<float>(rotationNode->valuedouble);
                }
                if (cJSON_IsNumber(drawOrderNode) && std::isfinite(drawOrderNode->valuedouble))
                {
                    instance.drawOrder = static_cast<int>(std::llround(drawOrderNode->valuedouble));
                }

                const cJSON* visibleNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "visible");
                if (cJSON_IsBool(visibleNode))
                {
                    instance.visible = cJSON_IsTrue(visibleNode);
                }

                const cJSON* flipHNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "flipHorizontal");
                if (cJSON_IsBool(flipHNode))
                {
                    instance.flipHorizontal = cJSON_IsTrue(flipHNode);
                }
                const cJSON* flipVNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "flipVertical");
                if (cJSON_IsBool(flipVNode))
                {
                    instance.flipVertical = cJSON_IsTrue(flipVNode);
                }

                const cJSON* spawnAfterIdNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "spawnAfterInstanceId");
                if (cJSON_IsNumber(spawnAfterIdNode) && std::isfinite(spawnAfterIdNode->valuedouble))
                {
                    const double rawId = spawnAfterIdNode->valuedouble;
                    instance.spawnAfterInstanceId = (rawId <= 0.0) ? 0U : static_cast<uint32_t>(std::llround(rawId));
                }
                const cJSON* spawnAfterDelayNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "spawnAfterDelayMs");
                if (cJSON_IsNumber(spawnAfterDelayNode) && std::isfinite(spawnAfterDelayNode->valuedouble))
                {
                    instance.spawnAfterDelayMs = static_cast<int>(std::llround(spawnAfterDelayNode->valuedouble));
                }

                const cJSON* motionSpawnCapNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionSpawnCaptured");
                if (!cJSON_IsBool(motionSpawnCapNode))
                {
                    const cJSON* motionEnNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionOffsetPreviewEnabled");
                    instance.motionSpawnCaptured = cJSON_IsBool(motionEnNode) && cJSON_IsTrue(motionEnNode);
                }
                else
                {
                    instance.motionSpawnCaptured = cJSON_IsTrue(motionSpawnCapNode);
                }
                const cJSON* mSx = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionSpawnOffsetX");
                const cJSON* mSy = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionSpawnOffsetY");
                if (cJSON_IsNumber(mSx) && std::isfinite(mSx->valuedouble))
                {
                    instance.motionSpawnOffsetX = static_cast<float>(mSx->valuedouble);
                }
                if (cJSON_IsNumber(mSy) && std::isfinite(mSy->valuedouble))
                {
                    instance.motionSpawnOffsetY = static_cast<float>(mSy->valuedouble);
                }

                const cJSON* trailNNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailEveryNTiles");
                if (cJSON_IsNumber(trailNNode) && std::isfinite(trailNNode->valuedouble))
                {
                    instance.motionTrailEveryNTiles =
                        static_cast<int>(std::clamp(std::llround(trailNNode->valuedouble), 0LL, 64LL));
                }
                const cJSON* trailLifeNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailLifetimeTiles");
                if (cJSON_IsNumber(trailLifeNode) && std::isfinite(trailLifeNode->valuedouble) && trailLifeNode->valuedouble > 0.0)
                {
                    instance.motionTrailLifetimeTiles =
                        static_cast<int>(std::clamp(std::llround(trailLifeNode->valuedouble), 1LL, 4096LL));
                }
                instance.motionTrailDistanceAcc = 0.0f;
                const cJSON* trailJitNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailLateralJitterRadius");
                if (cJSON_IsNumber(trailJitNode) && std::isfinite(trailJitNode->valuedouble))
                {
                    instance.motionTrailLateralJitterRadius =
                        static_cast<float>(std::clamp(trailJitNode->valuedouble, 0.0, 2048.0));
                }
                const cJSON* coneOffXNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailConeOffsetX");
                if (cJSON_IsNumber(coneOffXNode) && std::isfinite(coneOffXNode->valuedouble))
                {
                    instance.motionTrailConeOffsetX =
                        static_cast<float>(std::clamp(coneOffXNode->valuedouble, -2048.0, 2048.0));
                }
                const cJSON* coneOffYNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailConeOffsetY");
                if (cJSON_IsNumber(coneOffYNode) && std::isfinite(coneOffYNode->valuedouble))
                {
                    instance.motionTrailConeOffsetY =
                        static_cast<float>(std::clamp(coneOffYNode->valuedouble, -2048.0, 2048.0));
                }
                const cJSON* coneDirNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailConeDirectionOffsetDeg");
                if (cJSON_IsNumber(coneDirNode) && std::isfinite(coneDirNode->valuedouble))
                {
                    instance.motionTrailConeDirectionOffsetDeg =
                        static_cast<float>(std::clamp(coneDirNode->valuedouble, -179.0, 179.0));
                }
                const cJSON* coneHalfNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailConeHalfAngleDeg");
                if (cJSON_IsNumber(coneHalfNode) && std::isfinite(coneHalfNode->valuedouble))
                {
                    instance.motionTrailConeHalfAngleDeg =
                        static_cast<float>(std::clamp(coneHalfNode->valuedouble, 2.0, 85.0));
                }
                const cJSON* coneCountNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailConeSpawnCount");
                if (cJSON_IsNumber(coneCountNode) && std::isfinite(coneCountNode->valuedouble))
                {
                    instance.motionTrailConeSpawnCount =
                        std::clamp(static_cast<int>(std::llround(coneCountNode->valuedouble)), 1, 32);
                }
                const cJSON* strictTileNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailStrictTilePlacement");
                if (cJSON_IsBool(strictTileNode))
                {
                    instance.motionTrailStrictTilePlacement = cJSON_IsTrue(strictTileNode);
                }
                else
                {
                    instance.motionTrailStrictTilePlacement = (instance.motionTrailLateralJitterRadius <= 0.0001f);
                }
                const cJSON* rotPctNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailRotationRandomPercent");
                if (cJSON_IsNumber(rotPctNode) && std::isfinite(rotPctNode->valuedouble))
                {
                    instance.motionTrailRotationRandomPercent =
                        std::clamp(static_cast<int>(std::llround(rotPctNode->valuedouble)), 0, 100);
                }
                const cJSON* idleRingNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleRingWhenStationary");
                instance.motionTrailIdleRingWhenStationary =
                    cJSON_IsBool(idleRingNode) && cJSON_IsTrue(idleRingNode);
                const cJSON* idleRadNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleRingRadius");
                if (cJSON_IsNumber(idleRadNode) && std::isfinite(idleRadNode->valuedouble))
                {
                    instance.motionTrailIdleRingRadius =
                        static_cast<float>(std::clamp(idleRadNode->valuedouble, 1.0, 2048.0));
                }
                const cJSON* idlePerNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleSpawnPeriodMs");
                if (cJSON_IsNumber(idlePerNode) && std::isfinite(idlePerNode->valuedouble))
                {
                    instance.motionTrailIdleSpawnPeriodMs =
                        static_cast<int>(std::clamp(std::llround(idlePerNode->valuedouble), 100LL, 60000LL));
                }
                const cJSON* idleRingPcNode = cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleRingPieceCount");
                if (cJSON_IsNumber(idleRingPcNode) && std::isfinite(idleRingPcNode->valuedouble))
                {
                    instance.motionTrailIdleRingPieceCount =
                        std::clamp(static_cast<int>(std::llround(idleRingPcNode->valuedouble)), 1, 32);
                }
                instance.motionTrailIdleRingRotationRandomPercent = instance.motionTrailRotationRandomPercent;
                const cJSON* idleRingRotPctNode =
                    cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleRingRotationRandomPercent");
                if (cJSON_IsNumber(idleRingRotPctNode) && std::isfinite(idleRingRotPctNode->valuedouble))
                {
                    instance.motionTrailIdleRingRotationRandomPercent =
                        std::clamp(static_cast<int>(std::llround(idleRingRotPctNode->valuedouble)), 0, 100);
                }
                const cJSON* idleRingPosJitNode =
                    cJSON_GetObjectItemCaseSensitive(instanceNode, "motionTrailIdleRingPositionJitterRadius");
                if (cJSON_IsNumber(idleRingPosJitNode) && std::isfinite(idleRingPosJitNode->valuedouble))
                {
                    instance.motionTrailIdleRingPositionJitterRadius = static_cast<float>(
                        std::clamp(idleRingPosJitNode->valuedouble, 0.0, 2048.0));
                }
                instance.motionTrailIdleSpawnAccSec = 0.0f;
                instance.motionTrailIdleRingSalvoPiecesRemaining = 0;
                instance.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;

                stateData.instances.push_back(instance);
            }
        }

        // Tri stable pour que le rendu soit deterministe.
        std::sort(stateData.instances.begin(), stateData.instances.end(), [](const Instance& a, const Instance& b) {
            if (a.drawOrder != b.drawOrder)
            {
                return a.drawOrder < b.drawOrder;
            }
            return a.instanceId < b.instanceId;
        });

        parsedDirectionStates[static_cast<size_t>(key)] = std::move(stateData);
        if (!hasFirstDirectionStateKey)
        {
            hasFirstDirectionStateKey = true;
            firstDirectionStateKey = key;
        }

        directionStateIndex += 1;
    }

    // 11) Charger la spritesheet (json + png).
    if (parsedSpritesheetJsonPath.empty())
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: source spritesheet JSON absent: %s", gameplayConfigJsonPath.c_str());
        return false;
    }

    std::string spritesheetText;
    if (!readTextFileFromStorage(parsedSpritesheetJsonPath.c_str(), &spritesheetText))
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: spritesheet JSON introuvable: %s", parsedSpritesheetJsonPath.c_str());
        return false;
    }

    ParsedSpritesheet parsedSpritesheet{};
    if (!parseSpritesheetJson(spritesheetText.c_str(), &parsedSpritesheet))
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: spritesheet JSON invalide: %s", parsedSpritesheetJsonPath.c_str());
        return false;
    }

    const std::string parsedSpritesheetImagePath = resolveSpritesheetImagePath(
        parsedSpritesheetJsonPath,
        parsedSpritesheet.imageFile);
    if (parsedSpritesheetImagePath.empty())
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: path image spritesheet invalide: %s", parsedSpritesheetJsonPath.c_str());
        return false;
    }

    RC2D_Image loadedImage = rc2d_graphics_loadImageFromStorage(parsedSpritesheetImagePath.c_str(), RC2D_STORAGE_TITLE);
    if (loadedImage.sdl_texture == nullptr)
    {
        cJSON_Delete(root);
        RC2D_log(RC2D_LOG_ERROR, "VFX: image spritesheet introuvable: %s", parsedSpritesheetImagePath.c_str());
        return false;
    }

    // Parse termine: liberer la racine JSON.
    cJSON_Delete(root);

    // 12) Publier tout l'etat runtime charge.
    this->spritesheetImage = loadedImage;
    this->frames.clear();
    this->frames.reserve(parsedSpritesheet.frames.size());
    for (const ParsedFrame& frame : parsedSpritesheet.frames)
    {
        this->frames.push_back(Frame{frame.index, frame.x, frame.y, frame.w, frame.h});
    }
    this->directionStates = std::move(parsedDirectionStates);
    this->defaultVfxFps = parsedDefaultFps;
    this->playbackSeconds = 0.0f;
    this->activeDirectionStateKey = hasFirstDirectionStateKey ? firstDirectionStateKey : 0;
    this->trailPieces.clear();
    this->trailPrevShipTileValid = false;
    this->trailPrevDirectionStateKey = -1;
    this->loaded = true;

    this->shipFolderPath = parsedShipFolderPath;
    this->vfxFolderPath = parsedVfxFolderPath;
    this->configJsonPath = normalizePathSlashes(gameplayConfigJsonPath);
    this->spritesheetJsonPath = parsedSpritesheetJsonPath;
    this->spritesheetImagePath = parsedSpritesheetImagePath;

    RC2D_log(
        RC2D_LOG_INFO,
        "VFX: charge depuis %s (frames=%d, fps=%.2f)",
        this->configJsonPath.c_str(),
        static_cast<int>(this->frames.size()),
        this->defaultVfxFps);
    return true;
}

void VFX::unload(void)
{
    // 1) Liberer la texture si elle existe.
    if (this->spritesheetImage.sdl_texture != nullptr)
    {
        rc2d_graphics_freeImage(&this->spritesheetImage);
    }

    // 2) Reset data runtime.
    this->spritesheetImage = RC2D_Image{};
    this->frames.clear();
    for (DirectionStateData& directionState : this->directionStates)
    {
        directionState.instances.clear();
        directionState.shipDrawOrder = 0;
    }
    this->trailPieces.clear();
    this->trailPrevShipTileValid = false;
    this->trailPrevDirectionStateKey = -1;

    // 3) Reset variables de lecture.
    this->defaultVfxFps = 12.0f;
    this->playbackSeconds = 0.0f;
    this->activeDirectionStateKey = 0;
    this->loaded = false;

    // 4) Nettoyer les chemins de debug.
    this->shipFolderPath.clear();
    this->vfxFolderPath.clear();
    this->configJsonPath.clear();
    this->spritesheetJsonPath.clear();
    this->spritesheetImagePath.clear();
}

bool VFX::isLoaded(void) const
{
    return this->loaded &&
        this->spritesheetImage.sdl_texture != nullptr &&
        !this->frames.empty();
}

void VFX::update(double dt, const Ship& ship)
{
    // 1) Pas de ressources chargees => rien a mettre a jour.
    if (!this->isLoaded())
    {
        return;
    }

    // 2) Synchroniser la page direction/state depuis le ship runtime.
    this->setDirectionStateFromShip(ship);

    // 3) Si dt invalide, on s'arrete ici (direction/state deja synchro).
    if (!std::isfinite(dt) || dt <= 0.0)
    {
        return;
    }

    const float dtf = static_cast<float>(dt);
    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;
    this->updateTrailsAndIdle(dtf, timeSec, ship);

    // 4) Faire avancer l'horloge d'animation.
    this->playbackSeconds += dtf;

    // 5) Normaliser periodiquement pour eviter des floats trop grands.
    const float period = this->resolvedPlaybackPeriodSeconds();
    if (period > 0.0001f && this->playbackSeconds > (period * 8192.0f))
    {
        this->playbackSeconds = std::fmod(this->playbackSeconds, period);
    }
}

void VFX::updateTrailsAndIdle(float dtf, float timeSec, const Ship& ship)
{
    const int activeKey = (std::clamp)(this->activeDirectionStateKey, 0, 7);
    if (this->trailPrevDirectionStateKey != activeKey)
    {
        this->trailPrevDirectionStateKey = activeKey;
        this->trailPrevShipTileValid = false;
        for (DirectionStateData& ds : this->directionStates)
        {
            for (Instance& inst : ds.instances)
            {
                inst.motionTrailDistanceAcc = 0.0f;
            }
        }
    }

    for (size_t i = 0; i < this->trailPieces.size();)
    {
        this->trailPieces[i].timeRemainingSec -= dtf;
        if (this->trailPieces[i].timeRemainingSec <= 0.0f)
        {
            this->trailPieces.erase(this->trailPieces.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }

    DirectionStateData& activeState = this->directionStates[static_cast<size_t>(activeKey)];
    const SDL_FPoint shipTile = ship.getPositionTile();
    const bool moving = ship.isMoving();

    if (moving)
    {
        for (size_t i = 0; i < this->trailPieces.size();)
        {
            if (this->trailPieces[i].fromIdleRingCrown)
            {
                this->trailPieces.erase(this->trailPieces.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }
        for (Instance& inst : activeState.instances)
        {
            inst.motionTrailIdleSpawnAccSec = 0.0f;
            inst.motionTrailIdleRingSalvoPiecesRemaining = 0;
            inst.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
        }

        if (this->trailPrevShipTileValid)
        {
            const float dx = shipTile.x - this->trailPrevShipTileX;
            const float dy = shipTile.y - this->trailPrevShipTileY;
            const float dist = std::sqrt((dx * dx) + (dy * dy));
            if (dist > 0.00001f)
            {
                const float effZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f) * ship.getDrawScale();
                for (Instance& inst : activeState.instances)
                {
                    if (inst.motionTrailEveryNTiles <= 0)
                    {
                        continue;
                    }
                    const float thresholdTiles = static_cast<float>(inst.motionTrailEveryNTiles);
                    inst.motionTrailDistanceAcc += dist;
                    int spawnGuard = 0;
                    while (thresholdTiles > 0.0f && inst.motionTrailDistanceAcc >= thresholdTiles && spawnGuard < 1024)
                    {
                        const float invDist = 1.0f / dist;
                        const float nx = dx * invDist;
                        const float ny = dy * invDist;
                        const float overshoot = inst.motionTrailDistanceAcc - thresholdTiles;
                        const float spawnTileX = shipTile.x - (nx * overshoot);
                        const float spawnTileY = shipTile.y - (ny * overshoot);
                        inst.motionTrailDistanceAcc -= thresholdTiles;
                        float speedTilesPerSec =
                            (dtf > 0.000001f) ? (dist / dtf) : ship.getSpeedTilesPerSecond();
                        if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
                        {
                            speedTilesPerSec = 6.0f;
                        }
                        const int spawnCount = inst.motionTrailStrictTilePlacement
                            ? 1
                            : (std::clamp)(inst.motionTrailConeSpawnCount, 1, 32);
                        for (int burst = 0; burst < spawnCount && spawnGuard < 1024; ++burst)
                        {
                            VFX::runtimeAppendTrailPieceFromStep(inst,
                                                                   spawnTileX,
                                                                   spawnTileY,
                                                                   dx,
                                                                   dy,
                                                                   timeSec,
                                                                   effZoom,
                                                                   ship,
                                                                   speedTilesPerSec,
                                                                   this->trailPieces);
                            ++spawnGuard;
                        }
                    }
                    if (thresholdTiles > 0.0f)
                    {
                        inst.motionTrailDistanceAcc =
                            (std::clamp)(inst.motionTrailDistanceAcc, 0.0f, thresholdTiles);
                    }
                }
            }
        }
    }
    else
    {
        constexpr float kTwoPi = 6.28318530718f;
        const float effZoom = (std::max)(GetCamera().getZoomFactor(), 0.01f) * ship.getDrawScale();

        auto spawnIdleRingCrownPiece = [&](Instance& inst, int k) -> bool {
            const int pieceCount = (std::clamp)(inst.motionTrailIdleRingPieceCount, 1, 32);
            const float R = (std::max)(inst.motionTrailIdleRingRadius, 2.0f);
            const float ang = kTwoPi * (static_cast<float>(k) / static_cast<float>(pieceCount));
            float ox = inst.offsetX + std::cos(ang) * R;
            float oy = inst.offsetY + std::sin(ang) * R;
            if (inst.motionTrailIdleRingPositionJitterRadius > 0.0001f)
            {
                std::uniform_real_distribution<float> u01(0.0f, 1.0f);
                std::uniform_real_distribution<float> uAng(0.0f, kTwoPi);
                const float rr =
                    std::sqrt((std::max)(u01(vfxTrailRng()), 1.0e-8f)) *
                    inst.motionTrailIdleRingPositionJitterRadius;
                const float ja = uAng(vfxTrailRng());
                ox += std::cos(ja) * rr;
                oy += std::sin(ja) * rr;
            }
            TrailPiece piece{};
            piece.sourceVfxInstanceId = inst.instanceId;
            piece.anchorShipTileX = shipTile.x;
            piece.anchorShipTileY = shipTile.y;
            piece.bornTimeSeconds = timeSec;
            float speedTilesPerSec = ship.getSpeedTilesPerSecond();
            if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
            {
                speedTilesPerSec = 6.0f;
            }
            const float lifetimeTiles = static_cast<float>((std::max)(inst.motionTrailLifetimeTiles, 1));
            piece.timeRemainingSec = (std::max)(lifetimeTiles / speedTilesPerSec, 0.05f);
            piece.trailLifetimeInitialSec = piece.timeRemainingSec;
            piece.trailDrawOffsetX = ox;
            piece.trailDrawOffsetY = oy;
            piece.anchorShipSpriteCenterOffValid = ship.getCurrentSpriteCenterOffsetPixelsForEffectiveZoom(
                effZoom,
                &piece.anchorShipSpriteCenterOffXPx,
                &piece.anchorShipSpriteCenterOffYPx);
            if (!piece.anchorShipSpriteCenterOffValid)
            {
                piece.anchorShipSpriteCenterOffValid =
                    ship.getCurrentSpriteCenterOffsetPixels(&piece.anchorShipSpriteCenterOffXPx, &piece.anchorShipSpriteCenterOffYPx);
            }
            piece.trailPerpendicularJitterX = 0.0f;
            piece.trailPerpendicularJitterY = 0.0f;
            piece.trailRotationJitterDeg = 0.0f;
            if (inst.motionTrailIdleRingRotationRandomPercent > 0)
            {
                const float p =
                    static_cast<float>((std::clamp)(inst.motionTrailIdleRingRotationRandomPercent, 0, 100)) /
                    100.0f;
                std::uniform_real_distribution<float> rotU(-kMotionTrailRotationJitterMaxDeg, kMotionTrailRotationJitterMaxDeg);
                piece.trailRotationJitterDeg = p * rotU(vfxTrailRng());
            }
            VFX::runtimeInitTrailMotionExtras(&piece, 0.0f, 0.0f);
            piece.fromIdleRingCrown = true;
            this->trailPieces.push_back(std::move(piece));
            return true;
        };

        for (Instance& inst : activeState.instances)
        {
            if (!inst.motionTrailIdleRingWhenStationary)
            {
                inst.motionTrailIdleRingSalvoPiecesRemaining = 0;
                inst.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
                continue;
            }
            const int pieceCount = (std::clamp)(inst.motionTrailIdleRingPieceCount, 1, 32);
            const float periodSec =
                (std::max)(static_cast<float>(inst.motionTrailIdleSpawnPeriodMs) * 0.001f, 0.05f);
            const float staggerSec = kIdleRingPieceStaggerSec;

            if (inst.motionTrailIdleRingSalvoPiecesRemaining > 0)
            {
                inst.motionTrailIdleRingSalvoStaggerAccSec += dtf;
                while (inst.motionTrailIdleRingSalvoPiecesRemaining > 0 &&
                       inst.motionTrailIdleRingSalvoStaggerAccSec >= staggerSec)
                {
                    inst.motionTrailIdleRingSalvoStaggerAccSec -= staggerSec;
                    const int k = pieceCount - inst.motionTrailIdleRingSalvoPiecesRemaining;
                    inst.motionTrailIdleRingSalvoPiecesRemaining -= 1;
                    if (!spawnIdleRingCrownPiece(inst, k))
                    {
                        inst.motionTrailIdleRingSalvoPiecesRemaining = 0;
                        break;
                    }
                }
            }
            else
            {
                inst.motionTrailIdleSpawnAccSec += dtf;
                while (inst.motionTrailIdleSpawnAccSec >= periodSec)
                {
                    inst.motionTrailIdleSpawnAccSec -= periodSec;
                    if (!spawnIdleRingCrownPiece(inst, 0))
                    {
                        break;
                    }
                    inst.motionTrailIdleRingSalvoPiecesRemaining = pieceCount - 1;
                    inst.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
                }
            }
        }
    }

    this->trailPrevShipTileX = shipTile.x;
    this->trailPrevShipTileY = shipTile.y;
    this->trailPrevShipTileValid = true;
}

void VFX::drawTrailPieces(const Map& map, const Ship& ship, bool drawBehindShip, float timeSec) const
{
    if (this->trailPieces.empty())
    {
        return;
    }

    const DirectionStateData& directionState = this->currentDirectionState();
    const int shipDrawOrder = directionState.shipDrawOrder;

    auto findInst = [&directionState](uint32_t id) -> const Instance* {
        for (const Instance& inst : directionState.instances)
        {
            if (inst.instanceId == id)
            {
                return &inst;
            }
        }
        return nullptr;
    };

    std::vector<const TrailPiece*> sorted;
    sorted.reserve(this->trailPieces.size());
    for (const TrailPiece& p : this->trailPieces)
    {
        sorted.push_back(&p);
    }
    std::sort(sorted.begin(), sorted.end(), [&findInst, shipDrawOrder](const TrailPiece* a, const TrailPiece* b) {
        const Instance* ia = findInst(a->sourceVfxInstanceId);
        const Instance* ib = findInst(b->sourceVfxInstanceId);
        int oa = shipDrawOrder;
        int ob = shipDrawOrder;
        if (ia != nullptr)
        {
            oa = ia->drawOrder;
        }
        if (ib != nullptr)
        {
            ob = ib->drawOrder;
        }
        if (oa != ob)
        {
            return oa < ob;
        }
        return a->bornTimeSeconds < b->bornTimeSeconds;
    });

    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    auto lifetimeScaleMul = [](const TrailPiece& piece) -> float {
        if (piece.trailLifetimeInitialSec <= 1.0e-4f)
        {
            return 1.0f;
        }
        const float u = piece.timeRemainingSec / piece.trailLifetimeInitialSec;
        const float linear = (std::clamp)(u, 0.0f, 1.0f);
        const float eased = std::sqrt(linear);
        return (std::max)(kTrailPieceDrawScaleLifeMin, eased);
    };

    auto trailDrift = [](const TrailPiece& piece, float ageSec, float lifeScaleMul, float* outAddX, float* outAddY) {
        if (outAddX == nullptr || outAddY == nullptr)
        {
            return;
        }
        *outAddX = 0.0f;
        *outAddY = 0.0f;
        const float lifeS = (std::clamp)(lifeScaleMul, 0.0f, 1.0f);
        if (lifeS <= 1.0e-4f)
        {
            return;
        }
        const float t = (std::isfinite(ageSec) && ageSec > 0.0f) ? ageSec : 0.0f;
        const uint32_t seed = piece.trailNoiseSeed;
        const float tx = t * kTrailPieceMotionNoiseRateX + piece.trailAmbientDriftPhase0 * 0.18f;
        const float ty = t * kTrailPieceMotionNoiseRateY + piece.trailAmbientDriftPhase1 * 0.21f + 19.7f;
        *outAddX = vfxSmoothNoise1D(seed ^ 0x1a2b3c4du, tx) * kTrailPieceMotionNoiseAmp * lifeS;
        *outAddY = vfxSmoothNoise1D(seed ^ 0x5d6e7f8au, ty) * kTrailPieceMotionNoiseAmp * lifeS;
    };

    auto trailWake = [](const TrailPiece& piece, float ageSec, float lifeScaleMul, float* outAddX, float* outAddY) {
        if (outAddX == nullptr || outAddY == nullptr)
        {
            return;
        }
        *outAddX = 0.0f;
        *outAddY = 0.0f;
        const float lifeS = (std::clamp)(lifeScaleMul, 0.0f, 1.0f);
        if (lifeS <= 1.0e-4f)
        {
            return;
        }
        float dx = piece.trailWakeDirX;
        float dy = piece.trailWakeDirY;
        const float len = std::sqrt((dx * dx) + (dy * dy));
        if (len < 1.0e-4f)
        {
            return;
        }
        dx /= len;
        dy /= len;
        const float wt = (std::isfinite(ageSec) && ageSec > 0.0f) ? ageSec : 0.0f;
        const float envelope = wt * std::exp(-kTrailPieceWakeDecayPerSec * wt);
        const float mag = kTrailPieceWakeImpulseUnits * envelope * lifeS;
        *outAddX = dx * mag;
        *outAddY = dy * mag;
    };

    auto trailSpin = [](const TrailPiece& piece, float ageSec) -> float {
        const float omega0 = piece.trailSpinOmega0;
        constexpr float k = kTrailPieceSpinDecayPerSec;
        if (std::fabs(omega0) < 1.0e-5f || k < 1.0e-5f)
        {
            return 0.0f;
        }
        const float wt = (std::isfinite(ageSec) && ageSec > 0.0f) ? ageSec : 0.0f;
        return (omega0 / k) * (1.0f - std::exp(-k * wt));
    };

    for (const TrailPiece* piecePtr : sorted)
    {
        const TrailPiece& piece = *piecePtr;
        const Instance* inst = findInst(piece.sourceVfxInstanceId);
        if (inst == nullptr || !inst->visible)
        {
            continue;
        }
        const bool isBehindShip = inst->drawOrder < shipDrawOrder;
        if (isBehindShip != drawBehindShip)
        {
            continue;
        }

        SDL_FPoint shipCenter = map.tileToScreenCenterFloat(piece.anchorShipTileX, piece.anchorShipTileY);
        if (piece.anchorShipSpriteCenterOffValid)
        {
            shipCenter.x += piece.anchorShipSpriteCenterOffXPx;
            shipCenter.y += piece.anchorShipSpriteCenterOffYPx;
        }
        else
        {
            float shipCenterOffsetX = 0.0f;
            float shipCenterOffsetY = 0.0f;
            if (ship.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
            {
                shipCenter.x += shipCenterOffsetX;
                shipCenter.y += shipCenterOffsetY;
            }
        }

        const float phaseTime = (std::max)(0.0f, timeSec - piece.bornTimeSeconds);
        const float trailPlaybackSec = piece.trailInitialPhaseSec + phaseTime;
        const int frameCount = static_cast<int>(this->frames.size());
        const int frameIndex =
            VFX::runtimeTrailFrameIndex(this->defaultVfxFps, frameCount, trailPlaybackSec);
        if (frameIndex < 0 || frameIndex >= frameCount)
        {
            continue;
        }

        const Frame& frame = this->frames[static_cast<size_t>(frameIndex)];
        const float lifeScale = lifetimeScaleMul(piece);
        float driftX = 0.0f;
        float driftY = 0.0f;
        trailDrift(piece, phaseTime, lifeScale, &driftX, &driftY);
        float wakeX = 0.0f;
        float wakeY = 0.0f;
        trailWake(piece, phaseTime, lifeScale, &wakeX, &wakeY);
        const float ox =
            (piece.trailDrawOffsetX + piece.trailPerpendicularJitterX + driftX + wakeX) * scale;
        const float oy =
            (piece.trailDrawOffsetY + piece.trailPerpendicularJitterY + driftY + wakeY) * scale;
        const float trailDrawRotationDeg =
            inst->rotationDeg + piece.trailRotationJitterDeg + trailSpin(piece, phaseTime);
        const float drawScaleX = scale * lifeScale;
        const float drawScaleY = scale * lifeScale;
        const float drawX = shipCenter.x + ox - ((frame.w * drawScaleX) * 0.5f);
        const float drawY = shipCenter.y + oy - ((frame.h * drawScaleY) * 0.5f);
        const float pivotX = frame.w * 0.5f;
        const float pivotY = frame.h * 0.5f;

        RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
            const_cast<RC2D_Image*>(&this->spritesheetImage),
            frame.x,
            frame.y,
            frame.w,
            frame.h);
        rc2d_graphics_drawQuad(
            const_cast<RC2D_Image*>(&this->spritesheetImage),
            &sourceQuad,
            drawX,
            drawY,
            trailDrawRotationDeg,
            drawScaleX,
            drawScaleY,
            pivotX,
            pivotY,
            inst->flipHorizontal,
            inst->flipVertical);
    }
}

void VFX::draw(const Map& map, const Ship& ship, bool drawBehindShip) const
{
    // 1) Guard global.
    if (!this->isLoaded())
    {
        return;
    }

    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;
    this->drawTrailPieces(map, ship, drawBehindShip, timeSec);

    // 2) Recuperer la page active et sortir si aucune instance.
    const DirectionStateData& directionState = this->currentDirectionState();
    if (directionState.instances.empty())
    {
        return;
    }

    // 3) Calculer le centre visuel du ship (tuile + correction anchor).
    const SDL_FPoint shipTile = ship.getPositionTile();
    SDL_FPoint shipCenter = map.tileToScreenCenterFloat(shipTile.x, shipTile.y);
    float shipCenterOffsetX = 0.0f;
    float shipCenterOffsetY = 0.0f;
    if (ship.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
    {
        shipCenter.x += shipCenterOffsetX;
        shipCenter.y += shipCenterOffsetY;
    }

    // 4) Zoom courant applique au rendu.
    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    // 5) Dessiner chaque instance eligible.
    for (const Instance& instance : directionState.instances)
    {
        if (!instance.visible)
        {
            continue;
        }

        // Filtre de passe: derriere ou devant le ship.
        const bool isBehindShip = instance.drawOrder < directionState.shipDrawOrder;
        if (isBehindShip != drawBehindShip)
        {
            continue;
        }

        const int frameIndex = this->computeFrameIndex(directionState, instance);
        if (frameIndex < 0 || frameIndex >= static_cast<int>(this->frames.size()))
        {
            continue;
        }

        const Frame& frame = this->frames[static_cast<size_t>(frameIndex)];
        const float drawScaleX = zoom;
        const float drawScaleY = zoom;
        const float offsetX = instance.offsetX * zoom;
        const float offsetY = instance.offsetY * zoom;

        const float drawX = shipCenter.x + offsetX - ((frame.w * drawScaleX) * 0.5f);
        const float drawY = shipCenter.y + offsetY - ((frame.h * drawScaleY) * 0.5f);
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
            instance.rotationDeg,
            drawScaleX,
            drawScaleY,
            pivotX,
            pivotY,
            instance.flipHorizontal,
            instance.flipVertical);
    }
}
