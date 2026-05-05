#include "game/combat/maritime-cannon-salvo.h"

#include "core/context.h"
#include "game/state.h"
#include "game/ui/ingame-hud-overlay.h"
#include "game/ui/hud/game-settings-widget.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cJSON.h>
#include <RC2D/RC2D_storage.h>
#include <RC2D/RC2D_system.h>

namespace {

void quadBezierMidControl(
    float p0x,
    float p0y,
    float p2x,
    float p2y,
    float bowChordFraction,
    float arcSide,
    float* p1x,
    float* p1y)
{
    const float mx = (p0x + p2x) * 0.5f;
    const float my = (p0y + p2y) * 0.5f;
    const float bx = p2x - p0x;
    const float by = p2y - p0y;
    const float blen = std::sqrt(bx * bx + by * by);
    if (blen < 1.0e-4f)
    {
        *p1x = p0x;
        *p1y = p0y;
        return;
    }
    const float nAx = by / blen;
    const float nAy = -bx / blen;
    const float nBx = -by / blen;
    const float nBy = bx / blen;
    float nx = nAx;
    float ny = nAy;
    if (nBy < ny)
    {
        nx = nBx;
        ny = nBy;
    }

    const float side = (std::clamp)((std::isfinite(arcSide) ? arcSide : 0.0f), -1.0f, 1.0f);
    if (std::fabs(side) > 0.001f)
    {
        const float targetNx = (side < 0.0f) ? nAx : nBx;
        const float targetNy = (side < 0.0f) ? nAy : nBy;
        const float blend = std::fabs(side);
        nx = nx + ((targetNx - nx) * blend);
        ny = ny + ((targetNy - ny) * blend);
        const float nLen = std::sqrt(nx * nx + ny * ny);
        if (nLen > 1.0e-5f)
        {
            nx /= nLen;
            ny /= nLen;
        }
    }

    const float bow = bowChordFraction * blen;
    *p1x = mx + nx * bow;
    *p1y = my + ny * bow;
}

void quadBezierEval(
    float p0x,
    float p0y,
    float p1x,
    float p1y,
    float p2x,
    float p2y,
    float u,
    float* ox,
    float* oy)
{
    const float omt = 1.0f - u;
    *ox = omt * omt * p0x + 2.0f * omt * u * p1x + u * u * p2x;
    *oy = omt * omt * p0y + 2.0f * omt * u * p1y + u * u * p2y;
}

void quadBezierTangent(
    float p0x,
    float p0y,
    float p1x,
    float p1y,
    float p2x,
    float p2y,
    float u,
    float* tx,
    float* ty)
{
    const float omt = 1.0f - u;
    *tx = 2.0f * omt * (p1x - p0x) + 2.0f * u * (p2x - p1x);
    *ty = 2.0f * omt * (p1y - p0y) + 2.0f * u * (p2y - p1y);
}

float remapFastSlowFastProgress(float u, float strength)
{
    const float clampedU = (std::clamp)((std::isfinite(u) ? u : 0.0f), 0.0f, 1.0f);
    const float s = (std::clamp)((std::isfinite(strength) ? strength : 0.0f), 0.0f, 1.0f);
    if (s <= 0.0001f)
    {
        return clampedU;
    }

    float fastSlowFast = clampedU;
    if (clampedU < 0.5f)
    {
        const float t = clampedU * 2.0f;
        fastSlowFast = 0.5f * (1.0f - ((1.0f - t) * (1.0f - t)));
    }
    else
    {
        const float t = (clampedU - 0.5f) * 2.0f;
        fastSlowFast = 0.5f + (0.5f * t * t);
    }

    return clampedU + ((fastSlowFast - clampedU) * s);
}

constexpr int kIlluminatedPaletteCount = 7;

/**
 * Rose, turquoise, bleu, rouge, blanc, jaune, orange — indices 0..kIlluminatedPaletteCount-1.
 * Couleur portee par le halo SDL (VFXClassic::drawIlluminatedProjectile).
 */
constexpr std::array<std::array<std::uint8_t, 3>, kIlluminatedPaletteCount> kIlluminatedGlowRgb = {{
    {{255, 110, 255}},
    {{114, 255, 255}},
    {{61, 63, 171}},
    {{247, 85, 85}},
    {{255, 255, 255}},
    {{255, 255, 92}},
    {{255, 95, 0}},
}};

/**
 * Tuiles relatives 3x3 autour du navire cible (reste sur la coque): index 0 = centre (0,0), 1..8 = les 8 voisines.
 * Ordre: centre, puis NW,N,NE,W,E,SW,S,SE en coordonnees tuiles (dx vers l'est, dy vers le sud selon convention carte).
 */
constexpr std::array<int, 9> kImpactTileDx3x3 = {{
    0, -1, -1, -1, 0, 0, 1, 1, 1,
}};
constexpr std::array<int, 9> kImpactTileDy3x3 = {{
    0, -1, 0, 1, -1, 1, -1, 0, 1,
}};
/** Indices des 4 coins (extremes) dans kImpactTile*3x3 — pour salve 5: separation max entre les 4 hors-centre. */
constexpr std::array<int, 4> kImpactCornerTileIdx3x3 = {{
    1,
    3,
    6,
    8,
}};

/**
 * Repartit les couleurs le long de la formation (ordre lateral croissant): on evite deux boulets
 * voisins de meme couleur quand le multiset le permet (glouton sur le sac de couleurs).
 */
std::vector<int> spreadIlluminatedPaletteAlongFormation(
    const std::vector<int>& paletteMultiset,
    const std::vector<float>& lateralSlots)
{
    const int n = static_cast<int>(paletteMultiset.size());
    if (n <= 0)
    {
        return {};
    }

    std::vector<std::size_t> order(static_cast<size_t>(n));
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (lateralSlots[a] != lateralSlots[b])
        {
            return lateralSlots[a] < lateralSlots[b];
        }
        return a < b;
    });

    std::vector<int> remaining = paletteMultiset;
    std::vector<int> perBall(static_cast<size_t>(n));
    int prevColor = -1;

    for (int step = 0; step < n; ++step)
    {
        const std::size_t ballIdx = order[static_cast<size_t>(step)];
        int chosen = -1;
        std::size_t chosenRemIdx = 0;

        for (std::size_t r = 0; r < remaining.size(); ++r)
        {
            if (remaining[r] != prevColor)
            {
                chosen = remaining[r];
                chosenRemIdx = r;
                break;
            }
        }

        if (chosen < 0)
        {
            chosenRemIdx = 0;
            chosen = remaining[0];
        }

        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(chosenRemIdx));
        perBall[ballIdx] = chosen;
        prevColor = chosen;
    }

    return perBall;
}

using ProjectileTrajectoryTuning = MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning;

constexpr float kPi = 3.14159265358979323846f;
constexpr const char* kProjectileTrajectoryConfigPath =
    "assets/data/cannon-salvo-trajectory.json";
bool gProjectileTrajectoryConfigLoadAttempted = false;
bool gProjectileTrajectoryConfigLoaded = false;

struct CachedIlluminatedProjectileGlowConfigFile {
    bool attempted = false;
    bool loaded = false;
    VFXClassic::IlluminatedProjectileGlowConfig config{};
};

std::unordered_map<std::string, CachedIlluminatedProjectileGlowConfigFile>
    gIlluminatedProjectileGlowConfigFiles;

std::string normalizePathSlashesSalvo(const std::string& path)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

bool tryResolveSalvoIlluminatedGlowConfig(
    const MaritimeCannonSalvoSystem::SalvoEntry::IlluminatedProjectileSettings& settings,
    VFXClassic::IlluminatedProjectileGlowConfig* outConfig)
{
    if (!settings.enabled ||
        settings.glowConfigJsonPath == nullptr ||
        settings.glowConfigJsonPath[0] == '\0')
    {
        return false;
    }

    const std::string path = normalizePathSlashesSalvo(settings.glowConfigJsonPath);
    if (path.empty())
    {
        return false;
    }

    CachedIlluminatedProjectileGlowConfigFile& cacheEntry =
        gIlluminatedProjectileGlowConfigFiles[path];
    if (!cacheEntry.attempted)
    {
        cacheEntry.attempted = true;
        cacheEntry.loaded =
            VFXClassic::readIlluminatedProjectileGlowConfigFromFile(path.c_str(), &cacheEntry.config);
    }

    if (!cacheEntry.loaded)
    {
        return false;
    }

    if (outConfig != nullptr)
    {
        *outConfig = cacheEntry.config;
    }
    return true;
}

int clampProjectileDistanceBandIndex(int distanceBandIndex)
{
    return (std::clamp)(
        distanceBandIndex,
        0,
        MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount - 1);
}

int clampProjectileAngleSectorIndex(int angleSectorIndex)
{
    return (std::clamp)(
        angleSectorIndex,
        0,
        MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount - 1);
}

int resolveProjectileDistanceBandIndex(float distanceTiles)
{
    const float d = (std::isfinite(distanceTiles) && distanceTiles > 0.0f) ? distanceTiles : 0.0f;
    if (d < 10.0f)
    {
        return 0;
    }
    if (d < 20.0f)
    {
        return 1;
    }
    if (d < 40.0f)
    {
        return 2;
    }
    return 3;
}

float normalizeDegrees0To360(float degrees)
{
    if (!std::isfinite(degrees))
    {
        return 0.0f;
    }

    float normalized = std::fmod(degrees, 360.0f);
    if (normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    return normalized;
}

int resolveProjectileAngleSectorIndex(float dxScreen, float dyScreen)
{
    if (std::fabs(dxScreen) < 1.0e-5f && std::fabs(dyScreen) < 1.0e-5f)
    {
        return 0;
    }

    // Convention outil en repere ecran: 0 deg = gauche, 90 deg = haut.
    const float angleDeg = normalizeDegrees0To360(std::atan2(-dyScreen, -dxScreen) * (180.0f / kPi));
    if (angleDeg <= 69.0f)
    {
        return 0;
    }
    if (angleDeg <= 110.0f)
    {
        return 1;
    }
    if (angleDeg <= 180.0f)
    {
        return 2;
    }
    if (angleDeg <= 250.0f)
    {
        return 3;
    }
    if (angleDeg <= 290.0f)
    {
        return 4;
    }
    return 5;
}

int resolveExplicitArcSideSign(float arcSide)
{
    if (!std::isfinite(arcSide) || std::fabs(arcSide) < 0.001f)
    {
        return 0;
    }
    return arcSide < 0.0f ? -1 : 1;
}

void forceOffsetToArcSide(
    float chordDx,
    float chordDy,
    int arcSideSign,
    float* offsetX,
    float* offsetY)
{
    if (offsetX == nullptr || offsetY == nullptr || arcSideSign == 0)
    {
        return;
    }

    const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
    if (chordLen < 1.0e-5f)
    {
        return;
    }

    const float dirX = chordDx / chordLen;
    const float dirY = chordDy / chordLen;
    const float sideNormalX = (arcSideSign < 0) ? (chordDy / chordLen) : (-chordDy / chordLen);
    const float sideNormalY = (arcSideSign < 0) ? (-chordDx / chordLen) : (chordDx / chordLen);
    const float along = (*offsetX * dirX) + (*offsetY * dirY);
    const float sideDistance = (std::max)(0.0f, std::fabs((*offsetX * sideNormalX) + (*offsetY * sideNormalY)));
    *offsetX = (dirX * along) + (sideNormalX * sideDistance);
    *offsetY = (dirY * along) + (sideNormalY * sideDistance);
}

SDL_FPoint resolveProjectileStartTileForAim(
    const Ship* attacker,
    float fallbackStartTileX,
    float fallbackStartTileY,
    float aimTileX,
    float aimTileY,
    float forwardOffsetTiles,
    float sideOffsetTiles)
{
    SDL_FPoint startTile = SDL_FPoint{fallbackStartTileX, fallbackStartTileY};
    if (attacker != nullptr)
    {
        startTile = attacker->getPositionTile();
    }

    const float chordDx = aimTileX - startTile.x;
    const float chordDy = aimTileY - startTile.y;
    const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
    if (chordLen > 1.0e-5f)
    {
        const float invLen = 1.0f / chordLen;
        const float dirX = chordDx * invLen;
        const float dirY = chordDy * invLen;
        const float perpX = -chordDy * invLen;
        const float perpY = chordDx * invLen;
        startTile.x += dirX * forwardOffsetTiles;
        startTile.y += dirY * forwardOffsetTiles;
        startTile.x += perpX * sideOffsetTiles;
        startTile.y += perpY * sideOffsetTiles;
    }

    return startTile;
}

ProjectileTrajectoryTuning clampProjectileTrajectoryTuning(const ProjectileTrajectoryTuning& tuning)
{
    ProjectileTrajectoryTuning clamped = tuning;
    clamped.bezierBowChordFraction =
        (std::clamp)((std::isfinite(clamped.bezierBowChordFraction) ? clamped.bezierBowChordFraction : 0.28f), 0.0f, 0.8f);
    clamped.arcSide =
        (std::isfinite(clamped.arcSide) && clamped.arcSide < 0.0f) ? -1.0f : 1.0f;
    clamped.screenLobPixels =
        (std::clamp)((std::isfinite(clamped.screenLobPixels) ? clamped.screenLobPixels : 72.0f), 0.0f, 220.0f);
    clamped.launchStaggerScale =
        (std::clamp)((std::isfinite(clamped.launchStaggerScale) ? clamped.launchStaggerScale : 1.0f), 0.05f, 2.5f);
    clamped.launchSideOffsetTiles =
        (std::clamp)((std::isfinite(clamped.launchSideOffsetTiles) ? clamped.launchSideOffsetTiles : 0.0f), -2.0f, 2.0f);
    clamped.launchForwardOffsetTiles =
        (std::clamp)((std::isfinite(clamped.launchForwardOffsetTiles) ? clamped.launchForwardOffsetTiles : 0.0f), -2.0f, 2.0f);
    clamped.launchFanHalfTiles =
        (std::clamp)((std::isfinite(clamped.launchFanHalfTiles) ? clamped.launchFanHalfTiles : 0.72f), 0.0f, 2.0f);
    clamped.launchNoiseTiles =
        (std::clamp)((std::isfinite(clamped.launchNoiseTiles) ? clamped.launchNoiseTiles : 0.16f), 0.0f, 0.8f);
    clamped.impactSpreadScale =
        (std::clamp)((std::isfinite(clamped.impactSpreadScale) ? clamped.impactSpreadScale : 0.75f), 0.0f, 2.0f);
    clamped.flightLaneJitterTiles =
        (std::clamp)((std::isfinite(clamped.flightLaneJitterTiles) ? clamped.flightLaneJitterTiles : 0.18f), 0.0f, 1.5f);
    clamped.flightDurationScale =
        (std::clamp)((std::isfinite(clamped.flightDurationScale) ? clamped.flightDurationScale : 1.0f), 0.35f, 2.5f);
    clamped.flightEaseStrength =
        (std::clamp)((std::isfinite(clamped.flightEaseStrength) ? clamped.flightEaseStrength : 0.45f), 0.0f, 1.0f);
    return clamped;
}

ProjectileTrajectoryTuning buildDefaultProjectileTrajectoryTuning(int distanceBandIndex, int angleSectorIndex)
{
    const int band = clampProjectileDistanceBandIndex(distanceBandIndex);
    const int sector = clampProjectileAngleSectorIndex(angleSectorIndex);

    ProjectileTrajectoryTuning tuning{};
    switch (band)
    {
        case 0:
            tuning.bezierBowChordFraction = 0.34f;
            tuning.arcSide = 1.0f;
            tuning.screenLobPixels = 116.0f;
            tuning.launchStaggerScale = 1.22f;
            tuning.launchSideOffsetTiles = 0.0f;
            tuning.launchForwardOffsetTiles = 0.12f;
            tuning.launchFanHalfTiles = 0.86f;
            tuning.launchNoiseTiles = 0.12f;
            tuning.impactSpreadScale = 0.42f;
            tuning.flightLaneJitterTiles = 0.28f;
            tuning.flightDurationScale = 1.10f;
            tuning.flightEaseStrength = 0.58f;
            break;
        case 1:
            tuning.bezierBowChordFraction = 0.30f;
            tuning.arcSide = 1.0f;
            tuning.screenLobPixels = 88.0f;
            tuning.launchStaggerScale = 1.05f;
            tuning.launchSideOffsetTiles = 0.0f;
            tuning.launchForwardOffsetTiles = 0.10f;
            tuning.launchFanHalfTiles = 0.74f;
            tuning.launchNoiseTiles = 0.14f;
            tuning.impactSpreadScale = 0.58f;
            tuning.flightLaneJitterTiles = 0.22f;
            tuning.flightDurationScale = 1.04f;
            tuning.flightEaseStrength = 0.48f;
            break;
        case 2:
            tuning.bezierBowChordFraction = 0.24f;
            tuning.arcSide = 1.0f;
            tuning.screenLobPixels = 58.0f;
            tuning.launchStaggerScale = 0.82f;
            tuning.launchSideOffsetTiles = 0.0f;
            tuning.launchForwardOffsetTiles = 0.08f;
            tuning.launchFanHalfTiles = 0.58f;
            tuning.launchNoiseTiles = 0.13f;
            tuning.impactSpreadScale = 0.65f;
            tuning.flightLaneJitterTiles = 0.16f;
            tuning.flightDurationScale = 0.98f;
            tuning.flightEaseStrength = 0.36f;
            break;
        default:
            tuning.bezierBowChordFraction = 0.18f;
            tuning.arcSide = 1.0f;
            tuning.screenLobPixels = 36.0f;
            tuning.launchStaggerScale = 0.62f;
            tuning.launchSideOffsetTiles = 0.0f;
            tuning.launchForwardOffsetTiles = 0.05f;
            tuning.launchFanHalfTiles = 0.42f;
            tuning.launchNoiseTiles = 0.11f;
            tuning.impactSpreadScale = 0.52f;
            tuning.flightLaneJitterTiles = 0.10f;
            tuning.flightDurationScale = 0.94f;
            tuning.flightEaseStrength = 0.24f;
            break;
    }

    // Les secteurs haut (60/120) privilegient le lobe ecran; les horizontaux gardent
    // un peu plus de bow Bezier. Tout reste reglable dans le panneau debug.
    if (sector == 1 || sector == 2)
    {
        tuning.bezierBowChordFraction *= 0.92f;
        tuning.screenLobPixels *= 1.08f;
    }
    else if (sector == 0 || sector == 3)
    {
        tuning.bezierBowChordFraction *= 1.05f;
        tuning.screenLobPixels *= 0.94f;
    }
    else
    {
        tuning.bezierBowChordFraction *= 0.98f;
        tuning.screenLobPixels *= 1.0f;
    }

    return clampProjectileTrajectoryTuning(tuning);
}

std::array<std::array<ProjectileTrajectoryTuning, MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount>,
           MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount>
buildDefaultProjectileTrajectoryTunings(void)
{
    std::array<std::array<ProjectileTrajectoryTuning, MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount>,
               MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount> tunings{};

    for (int band = 0; band < MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount; ++band)
    {
        for (int sector = 0; sector < MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount; ++sector)
        {
            tunings[static_cast<size_t>(band)][static_cast<size_t>(sector)] =
                buildDefaultProjectileTrajectoryTuning(band, sector);
        }
    }

    return tunings;
}

std::array<std::array<ProjectileTrajectoryTuning, MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount>,
           MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount>
    gProjectileTrajectoryTunings = buildDefaultProjectileTrajectoryTunings();

bool readTextFileFromTitleStorage(const char* path, std::string* outText)
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

float readJsonFloat(const cJSON* object, const char* key, float fallback)
{
    const cJSON* node = cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    if (!cJSON_IsNumber(node) || !std::isfinite(node->valuedouble))
    {
        return fallback;
    }
    return static_cast<float>(node->valuedouble);
}

int readJsonInt(const cJSON* object, const char* key, int fallback)
{
    const cJSON* node = cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    if (!cJSON_IsNumber(node) || !std::isfinite(node->valuedouble))
    {
        return fallback;
    }
    return static_cast<int>(std::lround(node->valuedouble));
}

ProjectileTrajectoryTuning readProjectileTrajectoryTuningJson(
    const cJSON* object,
    const ProjectileTrajectoryTuning& fallback)
{
    ProjectileTrajectoryTuning tuning = fallback;
    tuning.bezierBowChordFraction =
        readJsonFloat(object, "bezierBowChordFraction", tuning.bezierBowChordFraction);
    tuning.arcSide = readJsonFloat(object, "arcSide", tuning.arcSide);
    tuning.screenLobPixels = readJsonFloat(object, "screenLobPixels", tuning.screenLobPixels);
    tuning.launchStaggerScale = readJsonFloat(object, "launchStaggerScale", tuning.launchStaggerScale);
    tuning.launchSideOffsetTiles =
        readJsonFloat(object, "launchSideOffsetTiles", tuning.launchSideOffsetTiles);
    tuning.launchForwardOffsetTiles =
        readJsonFloat(object, "launchForwardOffsetTiles", tuning.launchForwardOffsetTiles);
    tuning.launchFanHalfTiles = readJsonFloat(object, "launchFanHalfTiles", tuning.launchFanHalfTiles);
    tuning.launchNoiseTiles = readJsonFloat(object, "launchNoiseTiles", tuning.launchNoiseTiles);
    tuning.impactSpreadScale = readJsonFloat(object, "impactSpreadScale", tuning.impactSpreadScale);
    tuning.flightLaneJitterTiles = readJsonFloat(object, "flightLaneJitterTiles", tuning.flightLaneJitterTiles);
    tuning.flightDurationScale = readJsonFloat(object, "flightDurationScale", tuning.flightDurationScale);
    tuning.flightEaseStrength = readJsonFloat(object, "flightEaseStrength", tuning.flightEaseStrength);
    return clampProjectileTrajectoryTuning(tuning);
}

void addProjectileTrajectoryTuningJson(
    cJSON* object,
    const ProjectileTrajectoryTuning& tuning)
{
    if (object == nullptr)
    {
        return;
    }

    const ProjectileTrajectoryTuning clamped = clampProjectileTrajectoryTuning(tuning);
    cJSON_AddNumberToObject(object, "bezierBowChordFraction", clamped.bezierBowChordFraction);
    cJSON_AddNumberToObject(object, "arcSide", clamped.arcSide);
    cJSON_AddNumberToObject(object, "screenLobPixels", clamped.screenLobPixels);
    cJSON_AddNumberToObject(object, "launchStaggerScale", clamped.launchStaggerScale);
    cJSON_AddNumberToObject(object, "launchSideOffsetTiles", clamped.launchSideOffsetTiles);
    cJSON_AddNumberToObject(object, "launchForwardOffsetTiles", clamped.launchForwardOffsetTiles);
    cJSON_AddNumberToObject(object, "launchFanHalfTiles", clamped.launchFanHalfTiles);
    cJSON_AddNumberToObject(object, "launchNoiseTiles", clamped.launchNoiseTiles);
    cJSON_AddNumberToObject(object, "impactSpreadScale", clamped.impactSpreadScale);
    cJSON_AddNumberToObject(object, "flightLaneJitterTiles", clamped.flightLaneJitterTiles);
    cJSON_AddNumberToObject(object, "flightDurationScale", clamped.flightDurationScale);
    cJSON_AddNumberToObject(object, "flightEaseStrength", clamped.flightEaseStrength);
}

} // namespace

MaritimeCannonSalvoSystem::MaritimeCannonSalvoSystem(void)
    : cannonballs{},
      muzzleBursts{},
      targetImpactBursts{},
      pendingTargetImpactBursts{},
      salvoEntries{},
      salvoProjectileVfx{},
      rng(std::random_device{}())
{
}

MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning
MaritimeCannonSalvoSystem::getDefaultProjectileTrajectoryTuning(
    int distanceBandIndex,
    int angleSectorIndex)
{
    return buildDefaultProjectileTrajectoryTuning(distanceBandIndex, angleSectorIndex);
}

MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning
MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
    int distanceBandIndex,
    int angleSectorIndex)
{
    const int band = clampProjectileDistanceBandIndex(distanceBandIndex);
    const int sector = clampProjectileAngleSectorIndex(angleSectorIndex);
    return gProjectileTrajectoryTunings[static_cast<size_t>(band)][static_cast<size_t>(sector)];
}

void MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
    int distanceBandIndex,
    int angleSectorIndex,
    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning& tuning)
{
    const int band = clampProjectileDistanceBandIndex(distanceBandIndex);
    const int sector = clampProjectileAngleSectorIndex(angleSectorIndex);
    gProjectileTrajectoryTunings[static_cast<size_t>(band)][static_cast<size_t>(sector)] =
        clampProjectileTrajectoryTuning(tuning);
}

bool MaritimeCannonSalvoSystem::loadProjectileTrajectoryTuningsFromFile(void)
{
    if (gProjectileTrajectoryConfigLoadAttempted)
    {
        return gProjectileTrajectoryConfigLoaded;
    }
    gProjectileTrajectoryConfigLoadAttempted = true;
    gProjectileTrajectoryConfigLoaded = false;

    std::string jsonText;
    if (!readTextFileFromTitleStorage(kProjectileTrajectoryConfigPath, &jsonText))
    {
        return false;
    }

    cJSON* root = cJSON_Parse(jsonText.c_str());
    if (root == nullptr)
    {
        return false;
    }

    const cJSON* bandsNode = cJSON_GetObjectItemCaseSensitive(root, "distanceBands");
    if (!cJSON_IsArray(bandsNode))
    {
        cJSON_Delete(root);
        return false;
    }

    auto parsedTunings = buildDefaultProjectileTrajectoryTunings();
    int bandArrayIndex = 0;
    const cJSON* bandNode = nullptr;
    cJSON_ArrayForEach(bandNode, bandsNode)
    {
        if (!cJSON_IsObject(bandNode))
        {
            ++bandArrayIndex;
            continue;
        }

        const int bandIndex =
            clampProjectileDistanceBandIndex(readJsonInt(bandNode, "index", bandArrayIndex));
        const cJSON* sectorsNode = cJSON_GetObjectItemCaseSensitive(bandNode, "sectors");
        if (!cJSON_IsArray(sectorsNode))
        {
            ++bandArrayIndex;
            continue;
        }

        int sectorArrayIndex = 0;
        const cJSON* sectorNode = nullptr;
        cJSON_ArrayForEach(sectorNode, sectorsNode)
        {
            if (!cJSON_IsObject(sectorNode))
            {
                ++sectorArrayIndex;
                continue;
            }

            const int sectorIndex =
                clampProjectileAngleSectorIndex(readJsonInt(sectorNode, "index", sectorArrayIndex));
            const ProjectileTrajectoryTuning fallback =
                parsedTunings[static_cast<size_t>(bandIndex)][static_cast<size_t>(sectorIndex)];
            parsedTunings[static_cast<size_t>(bandIndex)][static_cast<size_t>(sectorIndex)] =
                readProjectileTrajectoryTuningJson(sectorNode, fallback);
            ++sectorArrayIndex;
        }

        ++bandArrayIndex;
    }

    cJSON_Delete(root);
    gProjectileTrajectoryTunings = parsedTunings;
    gProjectileTrajectoryConfigLoaded = true;
    return true;
}

bool MaritimeCannonSalvoSystem::exportProjectileTrajectoryTuningsToFile(void)
{
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "schema", "seatyrants.maritimeProjectileTrajectory.v1");
    cJSON_AddStringToObject(root, "angleConvention", "0deg=left,90deg=up,sectors=0-69,70-110,111-180,181-250,251-290,291-0");
    cJSON_AddStringToObject(root, "distanceConvention", "0:0-10,1:10-20,2:20-40,3:40+ tiles");

    cJSON* bandsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "distanceBands", bandsArray);
    constexpr std::array<const char*, kProjectileTrajectoryDistanceBandCount> bandLabels = {{
        "0-10",
        "10-20",
        "20-40",
        "40+",
    }};
    constexpr std::array<float, kProjectileTrajectoryDistanceBandCount> bandMins = {{
        0.0f,
        10.0f,
        20.0f,
        40.0f,
    }};
    constexpr std::array<float, kProjectileTrajectoryDistanceBandCount> bandMaxs = {{
        10.0f,
        20.0f,
        40.0f,
        -1.0f,
    }};

    for (int band = 0; band < kProjectileTrajectoryDistanceBandCount; ++band)
    {
        cJSON* bandObject = cJSON_CreateObject();
        cJSON_AddNumberToObject(bandObject, "index", band);
        cJSON_AddStringToObject(bandObject, "label", bandLabels[static_cast<size_t>(band)]);
        cJSON_AddNumberToObject(bandObject, "minTiles", bandMins[static_cast<size_t>(band)]);
        if (bandMaxs[static_cast<size_t>(band)] > 0.0f)
        {
            cJSON_AddNumberToObject(bandObject, "maxTiles", bandMaxs[static_cast<size_t>(band)]);
        }
        else
        {
            cJSON_AddNullToObject(bandObject, "maxTiles");
        }

        cJSON* sectorsArray = cJSON_CreateArray();
        cJSON_AddItemToObject(bandObject, "sectors", sectorsArray);
        constexpr std::array<int, kProjectileTrajectoryAngleSectorCount> sectorStartDeg = {{
            0,
            70,
            111,
            181,
            251,
            291,
        }};
        constexpr std::array<int, kProjectileTrajectoryAngleSectorCount> sectorEndDeg = {{
            69,
            110,
            180,
            250,
            290,
            0,
        }};
        for (int sector = 0; sector < kProjectileTrajectoryAngleSectorCount; ++sector)
        {
            cJSON* sectorObject = cJSON_CreateObject();
            cJSON_AddNumberToObject(sectorObject, "index", sector);
            cJSON_AddNumberToObject(sectorObject, "angleStartDeg", sectorStartDeg[static_cast<size_t>(sector)]);
            cJSON_AddNumberToObject(sectorObject, "angleEndDeg", sectorEndDeg[static_cast<size_t>(sector)]);
            addProjectileTrajectoryTuningJson(
                sectorObject,
                gProjectileTrajectoryTunings[static_cast<size_t>(band)][static_cast<size_t>(sector)]);
            cJSON_AddItemToArray(sectorsArray, sectorObject);
        }

        cJSON_AddItemToArray(bandsArray, bandObject);
    }

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);
    if (jsonText == nullptr)
    {
        return false;
    }

    const std::filesystem::path configPath(kProjectileTrajectoryConfigPath);
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

const char* MaritimeCannonSalvoSystem::getProjectileTrajectoryConfigPath(void)
{
    return kProjectileTrajectoryConfigPath;
}

void MaritimeCannonSalvoSystem::resetProjectileTrajectoryTunings(void)
{
    gProjectileTrajectoryTunings = buildDefaultProjectileTrajectoryTunings();
}

void MaritimeCannonSalvoSystem::invalidateProjectileTrajectoryConfigFileLoadState(void)
{
    gProjectileTrajectoryConfigLoadAttempted = false;
    gProjectileTrajectoryConfigLoaded = false;
}

void MaritimeCannonSalvoSystem::invalidateIlluminatedProjectileGlowConfigFileCache(const char* glowConfigJsonPath)
{
    if (glowConfigJsonPath == nullptr || glowConfigJsonPath[0] == '\0')
    {
        gIlluminatedProjectileGlowConfigFiles.clear();
        return;
    }

    gIlluminatedProjectileGlowConfigFiles.erase(
        normalizePathSlashesSalvo(glowConfigJsonPath));
}

void MaritimeCannonSalvoSystem::clearSalvoEntries(void)
{
    this->salvoEntries.clear();
}

void MaritimeCannonSalvoSystem::addSalvoEntry(const MaritimeCannonSalvoSystem::SalvoEntry& entry)
{
    if (entry.entryId == 0U)
    {
        return;
    }

    for (SalvoEntry& existing : this->salvoEntries)
    {
        if (existing.entryId == entry.entryId)
        {
            existing = entry;
            return;
        }
    }
    this->salvoEntries.push_back(entry);
}

const MaritimeCannonSalvoSystem::SalvoEntry* MaritimeCannonSalvoSystem::findSalvoEntryById(
    std::uint32_t entryId) const
{
    for (const SalvoEntry& entry : this->salvoEntries)
    {
        if (entry.entryId == entryId)
        {
            return &entry;
        }
    }
    return nullptr;
}

void MaritimeCannonSalvoSystem::fireSalvo(
    Ship& shipAttack,
    Ship& shipTarget,
    const SalvoEntry& entry)
{
    (void)MaritimeCannonSalvoSystem::loadProjectileTrajectoryTuningsFromFile();
    (void)VFXClassic::loadIlluminatedProjectileGlowConfigFromFile();
    GameSettingsWidget& settings = GetIngameHudOverlay().getGameSettingsWidget();
    const int ballCount = settings.getSalvoBulletCount();
    this->fireSalvoInternal(shipAttack, shipTarget, entry, ballCount, nullptr);
}

void MaritimeCannonSalvoSystem::fireSalvo(
    Ship& shipAttack,
    Ship& shipTarget,
    const SalvoEntry& entry,
    int ballCount,
    std::vector<GameplayVfxShipSlot>* localVfxShipSlots)
{
    (void)MaritimeCannonSalvoSystem::loadProjectileTrajectoryTuningsFromFile();
    (void)VFXClassic::loadIlluminatedProjectileGlowConfigFromFile();
    this->fireSalvoInternal(shipAttack, shipTarget, entry, ballCount, localVfxShipSlots);
}

void MaritimeCannonSalvoSystem::fireSalvoInternal(
    Ship& shipAttack,
    Ship& shipTarget,
    const SalvoEntry& entry,
    int ballCount,
    std::vector<GameplayVfxShipSlot>* localVfxShipSlots)
{
    const char* projectileVfxClassicFolder = entry.projectileVfxClassicFolder;
    const char* startActionVfxShipFolder = entry.startActionVfxShipFolderForAttacker;
    const MaritimeCannonSalvoSystem::SalvoEntry::IlluminatedProjectileSettings&
        illuminatedProjectile = entry.illuminatedProjectile;
    const bool useIlluminatedProjectileTint = illuminatedProjectile.enabled;
    VFXClassic::IlluminatedProjectileGlowConfig resolvedIlluminatedGlowConfig{};
    const bool hasResolvedIlluminatedGlowConfig =
        tryResolveSalvoIlluminatedGlowConfig(
            illuminatedProjectile,
            &resolvedIlluminatedGlowConfig);

    if (projectileVfxClassicFolder == nullptr || projectileVfxClassicFolder[0] == '\0')
    {
        return;
    }

    GameState& gameState = GetGameState();
    std::vector<GameplayVfxShipSlot>* vfxShipSlots =
        (localVfxShipSlots != nullptr) ? localVfxShipSlots : &gameState.vfxShips;

    if (ballCount < 1)
    {
        ballCount = 1;
    }
    if (ballCount > 32)
    {
        ballCount = 32;
    }

    const SDL_FPoint atk = shipAttack.getPositionTile();
    const SDL_FPoint tgt0 = shipTarget.getPositionTile();
    const float chordDx0 = tgt0.x - atk.x;
    const float chordDy0 = tgt0.y - atk.y;
    const float chordLen0 = std::sqrt(chordDx0 * chordDx0 + chordDy0 * chordDy0);
    const Map& map = GetCurrentMap();
    const SDL_FPoint atkScreen = map.tileToScreenCenterFloat(atk.x, atk.y);
    const SDL_FPoint tgtScreen = map.tileToScreenCenterFloat(tgt0.x, tgt0.y);
    const float screenDx0 = tgtScreen.x - atkScreen.x;
    const float screenDy0 = tgtScreen.y - atkScreen.y;

    // Distance -> [0..1] (proche -> 0, loin -> 1). Sert a piloter courbure + "file indienne".
    constexpr float kNearDistanceTiles = 8.0f;
    constexpr float kFarDistanceTiles = 48.0f;
    const float distanceAlpha = (kFarDistanceTiles > kNearDistanceTiles)
        ? (std::clamp)((chordLen0 - kNearDistanceTiles) / (kFarDistanceTiles - kNearDistanceTiles), 0.0f, 1.0f)
        : 1.0f;

    const int salvoDistanceBandIndex = resolveProjectileDistanceBandIndex(chordLen0);
    const int salvoAngleSectorIndex = resolveProjectileAngleSectorIndex(screenDx0, screenDy0);
    const ProjectileTrajectoryTuning salvoTuning =
        MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
            salvoDistanceBandIndex,
            salvoAngleSectorIndex);

    // Courbe reglable par distance + secteur d'angle; 0 veut dire trajectoire droite.

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_int_distribution<int> impactTileIdxDist(0, 8);
    std::uniform_real_distribution<float> impactTileJitterDist(-0.45f, 0.45f);
    std::uniform_int_distribution<int> pickPalette7(0, kIlluminatedPaletteCount - 1);

    // Palette 7 couleurs: salve 5 => 5 couleurs distinctes;
    // salve 10 => les 7 couleurs une fois chacune + 3 tirages supplementaires parmi les 7.
    std::vector<int> paletteMultiset;
    if (useIlluminatedProjectileTint)
    {
        if (ballCount == 1)
        {
            paletteMultiset.resize(1);
            int pick = pickPalette7(rng);
            if (kIlluminatedPaletteCount > 1 && this->lastIlluminatedSingleSalvoPaletteIdx >= 0 &&
                pick == this->lastIlluminatedSingleSalvoPaletteIdx)
            {
                pick = (this->lastIlluminatedSingleSalvoPaletteIdx + 1) % kIlluminatedPaletteCount;
            }
            paletteMultiset[0] = pick;
            this->lastIlluminatedSingleSalvoPaletteIdx = pick;
        }
        else if (ballCount <= 5)
        {
            paletteMultiset.resize(static_cast<size_t>(ballCount));
            std::vector<int> pool = {0, 1, 2, 3, 4, 5, 6};
            std::shuffle(pool.begin(), pool.end(), rng);
            for (int i = 0; i < ballCount; ++i)
            {
                paletteMultiset[static_cast<size_t>(i)] = pool[static_cast<size_t>(i)];
            }
        }
        else if (ballCount == 10)
        {
            paletteMultiset.reserve(10);
            for (int c = 0; c < kIlluminatedPaletteCount; ++c)
            {
                paletteMultiset.push_back(c);
            }
            for (int k = 0; k < 3; ++k)
            {
                paletteMultiset.push_back(pickPalette7(rng));
            }
            std::shuffle(paletteMultiset.begin(), paletteMultiset.end(), rng);
        }
        else
        {
            paletteMultiset.reserve(static_cast<size_t>(ballCount));
            std::vector<int> pool = {0, 1, 2, 3, 4, 5, 6};
            std::shuffle(pool.begin(), pool.end(), rng);
            const int take = (std::min)(ballCount, kIlluminatedPaletteCount);
            for (int i = 0; i < take; ++i)
            {
                paletteMultiset.push_back(pool[static_cast<size_t>(i)]);
            }
            while (static_cast<int>(paletteMultiset.size()) < ballCount)
            {
                paletteMultiset.push_back(pickPalette7(rng));
            }
        }
    }

    // SeaFight: pres = delais suffisants pour distinguer chaque boulet / lobe.
    // Loin = salve plus compacte sur la ligne (pas de grands trous dans la file), pas une queue etiree.
    std::vector<float> launchDelays;
    launchDelays.reserve(static_cast<size_t>(ballCount));
    std::vector<float> lateralSlots;
    lateralSlots.reserve(static_cast<size_t>(ballCount));
    if (ballCount > 1)
    {
        float staggerMax = 0.0f;
        if (ballCount >= 10)
        {
            staggerMax = MaritimeCannonSalvoSystem::kSalvoLaunchStaggerMax10;
        }
        else if (ballCount >= 5)
        {
            staggerMax = MaritimeCannonSalvoSystem::kSalvoLaunchStaggerMax5;
        }
        else
        {
            staggerMax = MaritimeCannonSalvoSystem::kSalvoLaunchStaggerMaxSmall;
        }

        staggerMax *= salvoTuning.launchStaggerScale;

        // Delais "SeaFight-like": sequence ordonnee mais avec des paquets (petits gaps)
        // et parfois un trou (gros gap), puis normalisation sur la fenetre staggerMax.
        std::vector<float> gaps;
        gaps.reserve(static_cast<size_t>(ballCount));
        gaps.push_back(0.0f);
        float sum = 0.0f;
        for (int i = 1; i < ballCount; ++i)
        {
            const float nearMul = 1.0f - distanceAlpha;
            const float farMul = distanceAlpha;

            const float smallMin = 0.012f * (0.90f * nearMul + 0.92f * farMul);
            const float smallMax = 0.028f * (0.90f * nearMul + 0.95f * farMul);
            const float bigMin = 0.038f * (0.90f * nearMul + 1.05f * farMul);
            const float bigMax = 0.078f * (0.90f * nearMul + 1.18f * farMul);

            const float pBig = (1.0f - distanceAlpha) * (0.12f + 0.16f * distanceAlpha);
            const bool big = (dist01(rng) < pBig);

            float g = 0.0f;
            if (big)
            {
                const float t = dist01(rng);
                g = bigMin + (bigMax - bigMin) * t;
            }
            else
            {
                const float t = dist01(rng);
                const float bias =
                    (distanceAlpha > 0.55f) ? (0.55f * t + 0.45f * t * t) : (0.38f * t + 0.62f * t * t);
                g = smallMin + (smallMax - smallMin) * bias;
            }

            const float jitter = (dist01(rng) - 0.5f) * 2.0f * MaritimeCannonSalvoSystem::kSalvoLaunchStaggerJitter;
            g = (std::max)(0.0f, g + jitter);

            gaps.push_back(g);
            sum += g;
        }

        // Normalise sur staggerMax (si sum ~ 0, fallback lineaire).
        float t = 0.0f;
        launchDelays.push_back(0.0f);
        if (sum > 1.0e-6f)
        {
            const float scale = staggerMax / sum;
            for (int i = 1; i < ballCount; ++i)
            {
                t += gaps[static_cast<size_t>(i)] * scale;
                launchDelays.push_back(t);
            }
        }
        else
        {
            for (int i = 1; i < ballCount; ++i)
            {
                const float slot = static_cast<float>(i) / static_cast<float>(ballCount - 1);
                launchDelays.push_back(slot * staggerMax);
            }
        }

        // Ordonne le long de la formation (teintes illuminees).
        for (int i = 0; i < ballCount; ++i)
        {
            lateralSlots.push_back(static_cast<float>(i));
        }
    }
    else
    {
        launchDelays.push_back(0.0f);
        lateralSlots.push_back(0.0f);
    }

    std::vector<int> illuminatedColorPlan;
    if (useIlluminatedProjectileTint && ballCount > 0 &&
        static_cast<int>(paletteMultiset.size()) == ballCount)
    {
        illuminatedColorPlan = spreadIlluminatedPaletteAlongFormation(paletteMultiset, lateralSlots);
    }

    /** Pour chaque boulet: indice tuile 0..8 dans la grille 3x3 (voir kImpactTileDx3x3). */
    std::vector<int> impactTilePick(static_cast<size_t>(ballCount));
    if (ballCount == 1)
    {
        impactTilePick[0] = 0;
    }
    else if (ballCount == 5)
    {
        impactTilePick[0] = 0;
        std::array<int, 4> corners = kImpactCornerTileIdx3x3;
        std::shuffle(corners.begin(), corners.end(), rng);
        impactTilePick[1] = corners[0];
        impactTilePick[2] = corners[1];
        impactTilePick[3] = corners[2];
        impactTilePick[4] = corners[3];
    }
    else if (ballCount == 10)
    {
        std::array<int, 9> cover{};
        for (int t = 0; t < 9; ++t)
        {
            cover[static_cast<size_t>(t)] = t;
        }
        std::shuffle(cover.begin(), cover.end(), rng);
        for (int k = 0; k < 9; ++k)
        {
            impactTilePick[static_cast<size_t>(k)] = cover[static_cast<size_t>(k)];
        }
        impactTilePick[9] = impactTileIdxDist(rng);
    }
    else
    {
        for (int i = 0; i < ballCount; ++i)
        {
            impactTilePick[static_cast<size_t>(i)] = impactTileIdxDist(rng);
        }
    }

    for (int i = 0; i < ballCount; ++i)
    {
        Cannonball ball{};
        ball.target = &shipTarget;
        ball.attacker = &shipAttack;
        ball.vfxShipSlots = vfxShipSlots;
        ball.endImpactVfxRequests.reserve(entry.endActionVfxShipFoldersForTarget.size());
        for (const SalvoEntry::EndActionVfxShipFolderForTarget& endAction :
             entry.endActionVfxShipFoldersForTarget)
        {
            if (endAction.vfxShipFolder != nullptr && endAction.vfxShipFolder[0] != '\0')
            {
                Cannonball::ImpactVfxRequest request{};
                request.folderPath = endAction.vfxShipFolder;
                request.delayAfterImpactMs = endAction.delayAfterImpactMs;
                ball.endImpactVfxRequests.push_back(std::move(request));
            }
        }
        ball.useIlluminatedProjectileTint = useIlluminatedProjectileTint;
        ball.hasIlluminatedGlowConfigOverride = hasResolvedIlluminatedGlowConfig;
        if (hasResolvedIlluminatedGlowConfig)
        {
            ball.illuminatedGlowConfigOverride = resolvedIlluminatedGlowConfig;
        }
        ball.illuminatedColorIndex = 0;
        ball.ageSec = 0.0f;
        const float spreadIndex = (ballCount > 1) ? static_cast<float>(i) / static_cast<float>(ballCount - 1) : 0.0f;
        ball.launchDelaySec = launchDelays[static_cast<size_t>(i)];
        const int tileSlot = impactTilePick[static_cast<size_t>(i)];
        const size_t ti = static_cast<size_t>((std::clamp)(tileSlot, 0, 8));
        const ProjectileTrajectoryTuning ballTuning = salvoTuning;
        const int ballArcSideSign = resolveExplicitArcSideSign(ballTuning.arcSide);
        float jx = 0.0f;
        float jy = 0.0f;
        if (ballCount > 1)
        {
            jx = impactTileJitterDist(rng);
            jy = impactTileJitterDist(rng);
        }
        ball.impactTileOffX =
            (static_cast<float>(kImpactTileDx3x3[ti]) + jx) * salvoTuning.impactSpreadScale;
        ball.impactTileOffY =
            (static_cast<float>(kImpactTileDy3x3[ti]) + jy) * salvoTuning.impactSpreadScale;
        forceOffsetToArcSide(
            chordDx0,
            chordDy0,
            ballArcSideSign,
            &ball.impactTileOffX,
            &ball.impactTileOffY);

        const float aimX = tgt0.x + ball.impactTileOffX;
        const float aimY = tgt0.y + ball.impactTileOffY;
        const float chordDx = aimX - atk.x;
        const float chordDy = aimY - atk.y;
        const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);

        ball.flightDurationSec =
            static_cast<float>(MaritimeCannonSalvoSystem::kProjectileFlightDurationMs) / 1000.0f;

        ball.bezierBowChordFraction = ballTuning.bezierBowChordFraction;
        ball.arcSide = ballTuning.arcSide;
        ball.screenLobPixels = ballTuning.screenLobPixels;
        ball.flightEaseStrength = ballTuning.flightEaseStrength;
        ball.launchForwardOffsetTiles = ballTuning.launchForwardOffsetTiles;
        ball.launchSideOffsetTiles = 0.0f;
        if (ballCount > 1 && ballTuning.flightLaneJitterTiles > 0.001f)
        {
            const float sideSign = static_cast<float>((ballArcSideSign != 0) ? ballArcSideSign : 1);
            const float amount =
                ballTuning.flightLaneJitterTiles * (0.18f + (0.82f * dist01(rng)));
            ball.flightLaneOffsetTiles = sideSign * amount;
        }

        float startX = atk.x;
        float startY = atk.y;
        if (chordLen > 1.0e-5f)
        {
            const float invLen = 1.0f / chordLen;
            const float dirX = chordDx * invLen;
            const float dirY = chordDy * invLen;
            const float perpX = -chordDy * invLen;
            const float perpY = chordDx * invLen;
            /** Eventail perpendiculaire: pres = bien separer; loin = rester sur la ligne (moins de dispersion laterale). */
            float launchFanHalf = ballTuning.launchFanHalfTiles;
            float launchNoise = ballTuning.launchNoiseTiles;
            const float fanNorm =
                (ballCount > 1)
                    ? ((2.0f * static_cast<float>(i) / static_cast<float>(ballCount - 1)) - 1.0f)
                    : 0.0f;
            const float fan = fanNorm * launchFanHalf;
            const float noise = (dist01(rng) - 0.5f) * 2.0f * launchNoise;
            float side = ballTuning.launchSideOffsetTiles + fan + noise;
            if (ballArcSideSign != 0)
            {
                const float sameSideFan =
                    (ballCount > 1)
                        ? spreadIndex * launchFanHalf
                        : 0.0f;
                side = static_cast<float>(ballArcSideSign) *
                    (std::fabs(ballTuning.launchSideOffsetTiles) + sameSideFan + std::fabs(noise));
            }
            startX += dirX * ballTuning.launchForwardOffsetTiles;
            startY += dirY * ballTuning.launchForwardOffsetTiles;
            startX += perpX * side;
            startY += perpY * side;
            ball.launchSideOffsetTiles = side;
        }
        ball.startTileX = startX;
        ball.startTileY = startY;
        ball.launchStartResolved = false;
        ball.lateralSlot = lateralSlots[static_cast<size_t>(i)];
        ball.salvoBallCount = static_cast<std::uint8_t>(ballCount);

        if (useIlluminatedProjectileTint &&
            static_cast<size_t>(i) < illuminatedColorPlan.size())
        {
            const int pick = illuminatedColorPlan[static_cast<size_t>(i)];
            const int idx = (std::clamp)(pick, 0, kIlluminatedPaletteCount - 1);
            ball.illuminatedColorIndex = static_cast<std::uint8_t>(idx);
            const auto& rgb = kIlluminatedGlowRgb[static_cast<size_t>(idx)];
            ball.illuminatedGlowR = rgb[0];
            ball.illuminatedGlowG = rgb[1];
            ball.illuminatedGlowB = rgb[2];
        }

        VFXClassic projectile{};
        if (!projectile.loadFromFolder(projectileVfxClassicFolder))
        {
            continue;
        }
        projectile.resetPlayback();

        // Depart sur la courbe (Bezier quadratique); depart legerement decale perpendiculairement a la corde vers impact.
        ball.tileX = ball.startTileX;
        ball.tileY = ball.startTileY;

        const int frameCount = projectile.getFrameCount();
        const float fps = (std::max)(projectile.getDefaultFps(), 1.0f);
        const float periodSec =
            (frameCount > 0) ? (static_cast<float>(frameCount) / fps) : (1.0f / fps);
        float phaseOff = 0.0f;
        if (frameCount > 0)
        {
            // Chaque boulet commence sur une phase differente de la spritesheet.
            // (melange index + aleatoire) -> evite les frames synchronisees.
            phaseOff = (0.15f + 0.80f * dist01(rng)) * periodSec;
            if (ballCount > 1)
            {
                phaseOff += spreadIndex * 0.35f * periodSec;
            }
        }
        projectile.setFramePhaseOffsetSeconds(phaseOff);

        this->salvoProjectileVfx.push_back(std::move(projectile));
        ball.projectileVfxIndex = this->salvoProjectileVfx.size() - 1U;
        this->cannonballs.push_back(ball);
    }

    if (startActionVfxShipFolder != nullptr && startActionVfxShipFolder[0] != '\0' &&
        shipAttack.areSpritesLoaded())
    {
        const std::string& shipFolder = shipAttack.getSpritesFolderPath();
        if (!shipFolder.empty())
        {
            MuzzleBurst burst{};
            burst.vfxShipSlots = vfxShipSlots;
            burst.attacker = &shipAttack;
            burst.target = &shipTarget;
            burst.elapsedSec = 0.0f;
            VFXShip muzzle{};
            if (muzzle.loadFromFolders(shipFolder.c_str(), startActionVfxShipFolder))
            {
                SDL_FPoint tgtTile = tgt0;
                muzzle.update(0.0, shipAttack, &tgtTile);

                GameplayVfxShipSlot shipSlot{};
                shipSlot.attackerShip = &shipAttack;
                shipSlot.targetShip = &shipTarget;
                shipSlot.anchorRole = GameplayVfxShipAnchorRole::ATTACKER;
                shipSlot.vfx = std::move(muzzle);
                vfxShipSlots->push_back(std::move(shipSlot));
                burst.vfxShipIndex = vfxShipSlots->size() - 1U;
                this->muzzleBursts.push_back(burst);
            }
        }
    }
}

void MaritimeCannonSalvoSystem::trimFinishedMuzzles(void)
{
    auto remapShipIndexAfterSwap = [&](std::vector<GameplayVfxShipSlot>* slots, std::size_t oldIndex, std::size_t newIndex) {
        for (MuzzleBurst& m : this->muzzleBursts)
        {
            if (m.vfxShipSlots == slots && m.vfxShipIndex == oldIndex)
            {
                m.vfxShipIndex = newIndex;
            }
        }
        for (TargetImpactBurst& t : this->targetImpactBursts)
        {
            if (t.vfxShipSlots == slots && t.vfxShipIndex == oldIndex)
            {
                t.vfxShipIndex = newIndex;
            }
        }
    };

    for (size_t i = 0; i < this->muzzleBursts.size();)
    {
        MuzzleBurst& m = this->muzzleBursts[i];
        std::vector<GameplayVfxShipSlot>* slots =
            (m.vfxShipSlots != nullptr) ? m.vfxShipSlots : &GetGameState().vfxShips;
        if (m.vfxShipIndex >= slots->size())
        {
            this->muzzleBursts[i] = std::move(this->muzzleBursts.back());
            this->muzzleBursts.pop_back();
            continue;
        }

        VFXShip& vfx = (*slots)[m.vfxShipIndex].vfx;
        if (m.elapsedSec >= MaritimeCannonSalvoSystem::kMuzzleMaxDurationSec || !vfx.isLoaded())
        {
            vfx.unload();

            const std::size_t removedIndex = m.vfxShipIndex;
            const std::size_t lastIndex = slots->size() - 1U;
            if (removedIndex != lastIndex)
            {
                (*slots)[removedIndex] = std::move((*slots)[lastIndex]);
                remapShipIndexAfterSwap(slots, lastIndex, removedIndex);
            }
            slots->pop_back();

            this->muzzleBursts[i] = std::move(this->muzzleBursts.back());
            this->muzzleBursts.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

void MaritimeCannonSalvoSystem::scheduleTargetImpactVfxFromCannonball(MaritimeCannonSalvoSystem::Cannonball& b)
{
    if (b.target == nullptr || b.attacker == nullptr)
    {
        return;
    }

    for (const Cannonball::ImpactVfxRequest& request : b.endImpactVfxRequests)
    {
        if (request.folderPath.empty())
        {
            continue;
        }

        if (request.delayAfterImpactMs == 0U)
        {
            this->spawnTargetImpactVfx(
                *b.target,
                *b.attacker,
                request.folderPath.c_str(),
                b.vfxShipSlots);
            continue;
        }

        PendingTargetImpactBurst pending{};
        pending.vfxShipFolder = request.folderPath;
        pending.vfxShipSlots = b.vfxShipSlots;
        pending.targetShip = b.target;
        pending.attackerShip = b.attacker;
        pending.remainingDelaySec = static_cast<float>(request.delayAfterImpactMs) * 0.001f;
        this->pendingTargetImpactBursts.push_back(std::move(pending));
    }
}

void MaritimeCannonSalvoSystem::spawnTargetImpactVfx(
    Ship& targetShip,
    Ship& attackerShip,
    const char* endFolder,
    std::vector<GameplayVfxShipSlot>* vfxShipSlots)
{
    if (endFolder == nullptr || endFolder[0] == '\0')
    {
        return;
    }
    if (!targetShip.areSpritesLoaded())
    {
        return;
    }
    const std::string& targetFolder = targetShip.getSpritesFolderPath();
    if (targetFolder.empty())
    {
        return;
    }

    if (vfxShipSlots == nullptr)
    {
        vfxShipSlots = &GetGameState().vfxShips;
    }

    VFXShip impact{};
    if (!impact.loadFromFolders(targetFolder.c_str(), endFolder))
    {
        return;
    }

    SDL_FPoint aimTile = attackerShip.getPositionTile();
    impact.update(0.0, targetShip, &aimTile);

    GameplayVfxShipSlot shipSlot{};
    shipSlot.attackerShip = &attackerShip;
    shipSlot.targetShip = &targetShip;
    shipSlot.anchorRole = GameplayVfxShipAnchorRole::TARGET;
    shipSlot.vfx = std::move(impact);
    vfxShipSlots->push_back(std::move(shipSlot));

    TargetImpactBurst burst{};
    burst.vfxShipIndex = vfxShipSlots->size() - 1U;
    burst.vfxShipSlots = vfxShipSlots;
    burst.targetShip = &targetShip;
    burst.attackerShip = &attackerShip;
    burst.elapsedSec = 0.0f;
    this->targetImpactBursts.push_back(burst);
}

void MaritimeCannonSalvoSystem::processPendingTargetImpacts(double dt)
{
    const float dtf =
        (std::isfinite(dt) && dt > 0.0) ? static_cast<float>(dt) : 0.0f;

    for (size_t i = 0; i < this->pendingTargetImpactBursts.size();)
    {
        PendingTargetImpactBurst& pending = this->pendingTargetImpactBursts[i];
        pending.remainingDelaySec -= dtf;
        if (pending.remainingDelaySec > 0.0f)
        {
            ++i;
            continue;
        }

        if (pending.targetShip != nullptr && pending.attackerShip != nullptr)
        {
            this->spawnTargetImpactVfx(
                *pending.targetShip,
                *pending.attackerShip,
                pending.vfxShipFolder.c_str(),
                pending.vfxShipSlots);
        }

        this->pendingTargetImpactBursts[i] = std::move(this->pendingTargetImpactBursts.back());
        this->pendingTargetImpactBursts.pop_back();
    }
}

void MaritimeCannonSalvoSystem::trimFinishedTargetImpacts(void)
{
    auto remapIndexAfterSwap = [&](std::vector<GameplayVfxShipSlot>* slots, std::size_t oldIndex, std::size_t newIndex) {
        for (MuzzleBurst& m : this->muzzleBursts)
        {
            if (m.vfxShipSlots == slots && m.vfxShipIndex == oldIndex)
            {
                m.vfxShipIndex = newIndex;
            }
        }
        for (TargetImpactBurst& t : this->targetImpactBursts)
        {
            if (t.vfxShipSlots == slots && t.vfxShipIndex == oldIndex)
            {
                t.vfxShipIndex = newIndex;
            }
        }
    };

    for (size_t i = 0; i < this->targetImpactBursts.size();)
    {
        TargetImpactBurst& t = this->targetImpactBursts[i];
        std::vector<GameplayVfxShipSlot>* slots =
            (t.vfxShipSlots != nullptr) ? t.vfxShipSlots : &GetGameState().vfxShips;
        if (t.vfxShipIndex >= slots->size())
        {
            this->targetImpactBursts[i] = std::move(this->targetImpactBursts.back());
            this->targetImpactBursts.pop_back();
            continue;
        }

        VFXShip& vfx = (*slots)[t.vfxShipIndex].vfx;
        if (t.elapsedSec >= MaritimeCannonSalvoSystem::kMuzzleMaxDurationSec || !vfx.isLoaded())
        {
            vfx.unload();

            const std::size_t removedIndex = t.vfxShipIndex;
            const std::size_t lastIndex = slots->size() - 1U;
            if (removedIndex != lastIndex)
            {
                (*slots)[removedIndex] = std::move((*slots)[lastIndex]);
                remapIndexAfterSwap(slots, lastIndex, removedIndex);
            }
            slots->pop_back();

            this->targetImpactBursts[i] = std::move(this->targetImpactBursts.back());
            this->targetImpactBursts.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

void MaritimeCannonSalvoSystem::update(double dt)
{
    this->updateInternal(dt);
}

void MaritimeCannonSalvoSystem::updateInternal(double dt)
{
    const Map& map = GetCurrentMap();
    auto remapProjectileVfxIndexAfterSwap = [&](std::size_t oldIndex, std::size_t newIndex) {
        for (Cannonball& b : this->cannonballs)
        {
            if (b.projectileVfxIndex == oldIndex)
            {
                b.projectileVfxIndex = newIndex;
            }
        }
    };

    const float dtf =
        (std::isfinite(dt) && dt > 0.0) ? static_cast<float>(dt) : 0.0f;

    for (MuzzleBurst& m : this->muzzleBursts)
    {
        std::vector<GameplayVfxShipSlot>* slots =
            (m.vfxShipSlots != nullptr) ? m.vfxShipSlots : &GetGameState().vfxShips;
        if (m.vfxShipIndex < slots->size() && m.attacker != nullptr && m.target != nullptr)
        {
            VFXShip& vfx = (*slots)[m.vfxShipIndex].vfx;
            SDL_FPoint tgtTile = m.target->getPositionTile();
            vfx.update(static_cast<double>(dtf), *m.attacker, &tgtTile);
        }
        m.elapsedSec += dtf;
    }
    this->trimFinishedMuzzles();

    for (TargetImpactBurst& t : this->targetImpactBursts)
    {
        std::vector<GameplayVfxShipSlot>* slots =
            (t.vfxShipSlots != nullptr) ? t.vfxShipSlots : &GetGameState().vfxShips;
        if (t.vfxShipIndex < slots->size() && t.targetShip != nullptr && t.attackerShip != nullptr)
        {
            VFXShip& vfx = (*slots)[t.vfxShipIndex].vfx;
            SDL_FPoint aimTile = t.attackerShip->getPositionTile();
            vfx.update(static_cast<double>(dtf), *t.targetShip, &aimTile);
        }
        t.elapsedSec += dtf;
    }
    this->trimFinishedTargetImpacts();
    this->processPendingTargetImpacts(dt);

    for (size_t i = 0; i < this->cannonballs.size();)
    {
        Cannonball& b = this->cannonballs[i];
        if (b.projectileVfxIndex >= this->salvoProjectileVfx.size() || b.target == nullptr)
        {
            this->cannonballs[i] = std::move(this->cannonballs.back());
            this->cannonballs.pop_back();
            continue;
        }

        VFXClassic& vfx = this->salvoProjectileVfx[b.projectileVfxIndex];
        if (!vfx.isLoaded())
        {
            const std::size_t removedIndex = b.projectileVfxIndex;
            const std::size_t lastIndex = this->salvoProjectileVfx.size() - 1U;
            if (removedIndex != lastIndex)
            {
                this->salvoProjectileVfx[removedIndex] = std::move(this->salvoProjectileVfx[lastIndex]);
                remapProjectileVfxIndexAfterSwap(lastIndex, removedIndex);
            }
            this->salvoProjectileVfx.pop_back();

            this->cannonballs[i] = std::move(this->cannonballs.back());
            this->cannonballs.pop_back();
            continue;
        }

        // Etale la sortie de salve: tant que le delai n'est pas ecoule,
        // on ne deplace pas et on ne dessine pas le boulet.
        b.ageSec += dtf;

        if (b.ageSec < b.launchDelaySec)
        {
            if (b.attacker != nullptr)
            {
                const SDL_FPoint attackerTile = b.attacker->getPositionTile();
                b.tileX = attackerTile.x;
                b.tileY = attackerTile.y;
            }
            else
            {
                b.tileX = b.startTileX;
                b.tileY = b.startTileY;
            }
            ++i;
            continue;
        }

        if (!b.launchStartResolved)
        {
            const SDL_FPoint targetTile = b.target->getPositionTile();
            const float aimX = targetTile.x + b.impactTileOffX;
            const float aimY = targetTile.y + b.impactTileOffY;
            const SDL_FPoint launchTile = resolveProjectileStartTileForAim(
                b.attacker,
                b.startTileX,
                b.startTileY,
                aimX,
                aimY,
                b.launchForwardOffsetTiles,
                b.launchSideOffsetTiles);

            b.startTileX = launchTile.x;
            b.startTileY = launchTile.y;
            b.tileX = launchTile.x;
            b.tileY = launchTile.y;
            b.launchStartResolved = true;
        }

        if (b.ageSec >= b.launchDelaySec + b.flightDurationSec)
        {
            this->scheduleTargetImpactVfxFromCannonball(b);

            vfx.unload();

            const std::size_t removedIndex = b.projectileVfxIndex;
            const std::size_t lastIndex = this->salvoProjectileVfx.size() - 1U;
            if (removedIndex != lastIndex)
            {
                this->salvoProjectileVfx[removedIndex] = std::move(this->salvoProjectileVfx[lastIndex]);
                remapProjectileVfxIndexAfterSwap(lastIndex, removedIndex);
            }
            this->salvoProjectileVfx.pop_back();

            this->cannonballs[i] = std::move(this->cannonballs.back());
            this->cannonballs.pop_back();
            continue;
        }

        const SDL_FPoint tgt = b.target->getPositionTile();
        const float p2x = tgt.x + b.impactTileOffX;
        const float p2y = tgt.y + b.impactTileOffY;
        const SDL_FPoint liveStartTile = resolveProjectileStartTileForAim(
            b.attacker,
            b.startTileX,
            b.startTileY,
            p2x,
            p2y,
            b.launchForwardOffsetTiles,
            b.launchSideOffsetTiles);
        b.startTileX = liveStartTile.x;
        b.startTileY = liveStartTile.y;
        const float p0x = liveStartTile.x;
        const float p0y = liveStartTile.y;
        float p1x = 0.0f;
        float p1y = 0.0f;
        quadBezierMidControl(
            p0x,
            p0y,
            p2x,
            p2y,
            b.bezierBowChordFraction,
            b.arcSide,
            &p1x,
            &p1y);

        const float denom = b.flightDurationSec;
        float u = 1.0f;
        if (denom > 1.0e-5f)
        {
            u = (b.ageSec - b.launchDelaySec) / denom;
            u = (std::clamp)(u, 0.0f, 1.0f);
        }

        const float motionU = remapFastSlowFastProgress(u, b.flightEaseStrength);
        float baseX = 0.0f;
        float baseY = 0.0f;
        quadBezierEval(p0x, p0y, p1x, p1y, p2x, p2y, motionU, &baseX, &baseY);
        if (std::fabs(b.flightLaneOffsetTiles) > 0.001f)
        {
            const float chordDx = p2x - p0x;
            const float chordDy = p2y - p0y;
            const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
            if (chordLen > 1.0e-5f)
            {
                const float laneCurve = std::sin(motionU * kPi);
                baseX += (-chordDy / chordLen) * b.flightLaneOffsetTiles * laneCurve;
                baseY += (chordDx / chordLen) * b.flightLaneOffsetTiles * laneCurve;
            }
        }
        b.tileX = baseX;
        b.tileY = baseY;

        vfx.update(static_cast<double>(dtf));

        ++i;
    }

    (void)map;
}

void MaritimeCannonSalvoSystem::drawSalvoProjectilesInternal(void) const
{
    const Map& map = GetCurrentMap();
    const float zoomFactor = GetCamera().getZoomFactor();
    const float drawScale = MaritimeCannonSalvoSystem::kDrawScale * zoomFactor;
    for (const Cannonball& b : this->cannonballs)
    {
        if (b.projectileVfxIndex >= this->salvoProjectileVfx.size() || b.target == nullptr)
        {
            continue;
        }
        if (b.ageSec < b.launchDelaySec)
        {
            continue;
        }
        const VFXClassic& vfx = this->salvoProjectileVfx[b.projectileVfxIndex];
        if (!vfx.isLoaded() || vfx.isFinished())
        {
            continue;
        }
        SDL_FPoint screen = map.tileToScreenCenterFloat(b.tileX, b.tileY);
        const float p0x = b.startTileX;
        const float p0y = b.startTileY;
        const SDL_FPoint tgt = b.target->getPositionTile();
        const float p2x = tgt.x + b.impactTileOffX;
        const float p2y = tgt.y + b.impactTileOffY;
        float p1x = 0.0f;
        float p1y = 0.0f;
        quadBezierMidControl(
            p0x,
            p0y,
            p2x,
            p2y,
            b.bezierBowChordFraction,
            b.arcSide,
            &p1x,
            &p1y);
        const float denom = b.flightDurationSec;
        float u = 1.0f;
        if (denom > 1.0e-5f)
        {
            u = (b.ageSec - b.launchDelaySec) / denom;
            u = (std::clamp)(u, 0.0f, 1.0f);
        }
        const float motionU = remapFastSlowFastProgress(u, b.flightEaseStrength);
        if (b.screenLobPixels > 0.001f)
        {
            const float lobCurve = std::sin(motionU * kPi);
            screen.y -= lobCurve * b.screenLobPixels * zoomFactor;
        }
        float tx = 0.0f;
        float ty = 0.0f;
        quadBezierTangent(p0x, p0y, p1x, p1y, p2x, p2y, motionU, &tx, &ty);
        const float rotDeg = std::atan2(ty, tx) * (180.0f / kPi);
        if (b.useIlluminatedProjectileTint)
        {
            vfx.drawIlluminatedProjectile(
                screen.x,
                screen.y,
                drawScale,
                rotDeg,
                false,
                false,
                b.illuminatedGlowR,
                b.illuminatedGlowG,
                b.illuminatedGlowB,
                b.hasIlluminatedGlowConfigOverride ? &b.illuminatedGlowConfigOverride : nullptr,
                b.salvoBallCount == 1);
        }
        else
        {
            vfx.draw(screen.x, screen.y, drawScale, rotDeg, false, false);
        }
    }
}

void MaritimeCannonSalvoSystem::drawSalvoProjectiles(void) const
{
    this->drawSalvoProjectilesInternal();
}

void MaritimeCannonSalvoSystem::clear(void)
{
    this->clearInternal();
}

void MaritimeCannonSalvoSystem::clearInternal(void)
{
    for (VFXClassic& v : this->salvoProjectileVfx)
    {
        v.unload();
    }
    this->salvoProjectileVfx.clear();
    this->cannonballs.clear();
    this->muzzleBursts.clear();
    this->targetImpactBursts.clear();
    this->pendingTargetImpactBursts.clear();
    this->lastIlluminatedSingleSalvoPaletteIdx = -1;
}
