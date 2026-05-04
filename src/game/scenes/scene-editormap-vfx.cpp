#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-vfx.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <vector>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_storage.h>
#include <RC2D/RC2D.h>
#include <SDL3/SDL_surface.h>
#include <cJSON.h>

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/map/map.h"
#include "game/scenes/editormap-scene-layout.h"
#include "game/render/world-render-clip.h"

namespace
{
std::mt19937& editorMapVfxTrailJitterRng(void)
{
    static std::mt19937 gen = []() {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd()};
        return std::mt19937(seed);
    }();
    return gen;
}

/** Amplitude max de rotation aleatoire par rejet (deg) lorsque motionTrailRotationRandomPercent vaut 100. */
constexpr float kMotionTrailRotationJitterMaxDeg = 45.0f;
/** Echelle min (fraction taille au spawn) pour un rejet en fin de vie. */
constexpr float kTrailPieceDrawScaleLifeMin = 0.08f;
/** Amplitude max (unites offset) du bruit lisse des rejets (derive non figee). */
constexpr float kTrailPieceMotionNoiseAmp = 2.15f;
/** Echelles temporelles du bruit 1D (plus bas = variation plus lente). */
constexpr float kTrailPieceMotionNoiseRateX = 0.48f;
constexpr float kTrailPieceMotionNoiseRateY = 0.39f;
/** Envergure max de l'inertie visuelle le long du pas (t * exp(-kt), memes unites que offsets). */
constexpr float kTrailPieceWakeImpulseUnits = 1.75f;
constexpr float kTrailPieceWakeDecayPerSec = 1.32f;
constexpr float kTrailPieceSpinDecayPerSec = 2.05f;
constexpr float kTrailPieceSpinOmegaMaxDegPerSec = 5.2f;
/** Ecart temporel entre deux pieces d'une meme rafale couronne (secondes). */
constexpr float kIdleRingPieceStaggerSec = 0.055f;
/** Cadence fixe entre deux rafales couronne a l'arret (secondes). */
constexpr float kIdleRingSalvoPeriodSec = 0.60f;
constexpr int kMotionTrailEveryNTilesMin = 0;
constexpr int kMotionTrailEveryNTilesMax = 64;
/** Echelle max de profondeur du cone (meme unite que motionTrailLateralJitterRadius) avant plafond geometrique. */
constexpr float kMotionTrailConeDepthBySpreadRatio = 1.0f;
constexpr float kTrailConePopupLengthMin = 1.0f;
constexpr float kTrailConePopupLengthMax = 2048.0f;
constexpr float kTrailConePopupOffsetMin = -2048.0f;
constexpr float kTrailConePopupOffsetMax = 2048.0f;
constexpr float kTrailConePopupHalfAngleMin = 2.0f;
constexpr float kTrailConePopupHalfAngleMax = 85.0f;
/** Dans le preview/placement cone : axe "arriere" fixe (independant de l'orientation navire). */
constexpr float kTrailConePopupRearAxisRad = 1.57079632679f;

struct EditorMapVfxTrailConePreviewGeom
{
    float layerAnchorX = 0.0f;
    float layerAnchorY = 0.0f;
    float coneOriginX = 0.0f;
    float coneOriginY = 0.0f;
    float maxOffsetPx = 1.0f;
    float maxLengthPx = 1.0f;
    float lengthPx = 1.0f;
    float axisAngleRad = kTrailConePopupRearAxisRad;
    float tipX = 0.0f;
    float tipY = 0.0f;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float sideX = 0.0f;
    float sideY = 0.0f;
};

float editorMapVfxDistance2D(float ax, float ay, float bx, float by)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt((dx * dx) + (dy * dy));
}

void editorMapVfxClampRectInside(SDL_FRect* rect, const SDL_FRect& bounds, float marginPx)
{
    if (rect == nullptr)
    {
        return;
    }
    const float minX = bounds.x + marginPx;
    const float minY = bounds.y + marginPx;
    const float maxX = (bounds.x + bounds.w) - rect->w - marginPx;
    const float maxY = (bounds.y + bounds.h) - rect->h - marginPx;
    rect->x = (std::clamp)(rect->x, minX, maxX);
    rect->y = (std::clamp)(rect->y, minY, maxY);
}

void editorMapVfxPushHandleAwayFrom(
    SDL_FRect* movable,
    const SDL_FRect& anchor,
    float minCenterDistance,
    const SDL_FRect& bounds)
{
    if (movable == nullptr)
    {
        return;
    }
    float mx = movable->x + (movable->w * 0.5f);
    float my = movable->y + (movable->h * 0.5f);
    const float ax = anchor.x + (anchor.w * 0.5f);
    const float ay = anchor.y + (anchor.h * 0.5f);
    float dx = mx - ax;
    float dy = my - ay;
    float dist = std::sqrt((dx * dx) + (dy * dy));
    if (dist < 0.0001f)
    {
        dx = 1.0f;
        dy = 0.0f;
        dist = 1.0f;
    }
    if (dist < minCenterDistance)
    {
        const float need = minCenterDistance - dist;
        mx += (dx / dist) * need;
        my += (dy / dist) * need;
        movable->x = mx - (movable->w * 0.5f);
        movable->y = my - (movable->h * 0.5f);
        editorMapVfxClampRectInside(movable, bounds, 2.0f);
    }
}

void editorMapVfxDirectionIndexToTileStep(int directionIndex, float* outStepTileX, float* outStepTileY)
{
    if (outStepTileX == nullptr || outStepTileY == nullptr)
    {
        return;
    }
    const int dir = ((directionIndex % 4) + 4) % 4;
    switch (dir)
    {
    case 0:
        *outStepTileX = 0.0f;
        *outStepTileY = 1.0f;
        break;
    case 1:
        *outStepTileX = 0.0f;
        *outStepTileY = -1.0f;
        break;
    case 2:
        *outStepTileX = -1.0f;
        *outStepTileY = 0.0f;
        break;
    default:
        *outStepTileX = 1.0f;
        *outStepTileY = 0.0f;
        break;
    }
}

float editorMapVfxNormalizeSignedAngleDeg(float angleDeg)
{
    while (angleDeg > 180.0f)
    {
        angleDeg -= 360.0f;
    }
    while (angleDeg <= -180.0f)
    {
        angleDeg += 360.0f;
    }
    return angleDeg;
}

float editorMapVfxNormalizeDeg0To360(float deg)
{
    if (!std::isfinite(deg))
    {
        return 0.0f;
    }
    float x = std::fmod(deg, 360.0f);
    if (x < 0.0f)
    {
        x += 360.0f;
    }
    return x;
}

float editorMapVfxCircularDeltaDeg(float aDeg, float bDeg)
{
    const float da = editorMapVfxNormalizeDeg0To360(aDeg);
    const float db = editorMapVfxNormalizeDeg0To360(bDeg);
    const float d = std::fabs(da - db);
    return (std::min)(d, 360.0f - d);
}

void editorMapVfxBowForwardScreenUnit(Ship::PreviewDirection bow, float* outFx, float* outFy)
{
    if (outFx == nullptr || outFy == nullptr)
    {
        return;
    }
    float x = -1.0f;
    float y = 1.0f;
    switch (bow)
    {
    case Ship::PreviewDirection::DOWN_LEFT:
        x = -1.0f;
        y = 1.0f;
        break;
    case Ship::PreviewDirection::UP_RIGHT:
        x = 1.0f;
        y = -1.0f;
        break;
    case Ship::PreviewDirection::UP_LEFT:
        x = -1.0f;
        y = -1.0f;
        break;
    case Ship::PreviewDirection::DOWN_RIGHT:
        x = 1.0f;
        y = 1.0f;
        break;
    default:
        break;
    }
    const float inv = 1.0f / std::sqrt((x * x) + (y * y));
    *outFx = x * inv;
    *outFy = y * inv;
}

float editorMapVfxAdjustedTargetFireDegFromBowScreenDelta(Ship::PreviewDirection bow, float dScreenX, float dScreenY)
{
    constexpr float kRadToDeg = 57.29577951308232f;
    const float targetDeg = editorMapVfxNormalizeDeg0To360(std::atan2(dScreenY, dScreenX) * kRadToDeg);
    float fx = 0.0f;
    float fy = 0.0f;
    editorMapVfxBowForwardScreenUnit(bow, &fx, &fy);
    const float bowDeg = editorMapVfxNormalizeDeg0To360(std::atan2(fy, fx) * kRadToDeg);
    return editorMapVfxNormalizeDeg0To360(targetDeg - bowDeg + 135.0f);
}

int editorMapVfxTargetFireSectorFromAdjustedDeg(float adjDeg)
{
    const float a = editorMapVfxNormalizeDeg0To360(adjDeg);
    if (a >= 0.0f && a < 90.0f)
    {
        return 0;
    }
    if (a >= 180.0f && a < 270.0f)
    {
        return 1;
    }
    return (editorMapVfxCircularDeltaDeg(a, 45.0f) <= editorMapVfxCircularDeltaDeg(a, 225.0f)) ? 0 : 1;
}

int editorMapVfxTargetFireSectorFromScreenDelta(Ship::PreviewDirection bow, float dScreenX, float dScreenY)
{
    const float adj = editorMapVfxAdjustedTargetFireDegFromBowScreenDelta(bow, dScreenX, dScreenY);
    return editorMapVfxTargetFireSectorFromAdjustedDeg(adj);
}

void editorMapVfxTargetFireSectorQuarterBoundsScreenDeg(
    Ship::PreviewDirection bow,
    int sector,
    float* outDeg0,
    float* outDeg1)
{
    if (outDeg0 == nullptr || outDeg1 == nullptr)
    {
        return;
    }
    constexpr float kRadToDeg = 57.29577951308232f;
    float fx = 0.0f;
    float fy = 0.0f;
    editorMapVfxBowForwardScreenUnit(bow, &fx, &fy);
    const float bowDeg = editorMapVfxNormalizeDeg0To360(std::atan2(fy, fx) * kRadToDeg);
    const int s = std::clamp(sector, 0, 1);
    if (s == 0)
    {
        *outDeg0 = editorMapVfxNormalizeDeg0To360(bowDeg - 135.0f);
        *outDeg1 = editorMapVfxNormalizeDeg0To360(bowDeg - 45.0f);
    }
    else
    {
        *outDeg0 = editorMapVfxNormalizeDeg0To360(bowDeg + 45.0f);
        *outDeg1 = editorMapVfxNormalizeDeg0To360(bowDeg + 135.0f);
    }
}

float editorMapVfxTrailConeLengthToPreviewPx(float lengthValue, float maxLengthPx)
{
    const float maxPx = (std::max)(maxLengthPx, 1.0f);
    const float clampedLength = (std::clamp)(lengthValue, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
    // Mapping quasi 1:1 pour que la longueur vue dans la popup colle a la portee reelle de spawn.
    return (std::clamp)(clampedLength, 0.0f, maxPx);
}

float editorMapVfxTrailConePreviewPxToLength(float lengthPx, float maxLengthPx)
{
    const float maxPx = (std::max)(maxLengthPx, 1.0f);
    const float clampedPx = (std::clamp)(lengthPx, 0.0f, maxPx);
    return (std::clamp)(clampedPx, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
}

float editorMapVfxTrailConeOffsetUnitsToPreviewPx(float offsetUnits, float maxOffsetPx)
{
    const float maxPx = (std::max)(maxOffsetPx, 1.0f);
    // Mapping quasi 1:1 pour que l'offset vert vu en popup corresponde a la position de spawn.
    const float clampedUnits = (std::clamp)(offsetUnits, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    return (std::clamp)(clampedUnits, -maxPx, maxPx);
}

float editorMapVfxTrailConePreviewPxToOffsetUnits(float offsetPx, float maxOffsetPx)
{
    const float maxPx = (std::max)(maxOffsetPx, 1.0f);
    const float clampedPx = (std::clamp)(offsetPx, -maxPx, maxPx);
    return (std::clamp)(clampedPx, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
}

EditorMapVfxTrailConePreviewGeom editorMapVfxBuildTrailConePreviewGeom(
    const SDL_FRect& previewRect,
    float lengthValue,
    float offsetXUnits,
    float offsetYUnits,
    float directionOffsetDeg,
    float halfAngleDeg)
{
    EditorMapVfxTrailConePreviewGeom geom{};
    geom.layerAnchorX = previewRect.x + (previewRect.w * 0.5f);
    geom.layerAnchorY = previewRect.y + (previewRect.h * 0.5f);
    geom.maxOffsetPx = (std::max)(16.0f, (std::min)(previewRect.w, previewRect.h) * 0.44f);
    const float offsetXPx = editorMapVfxTrailConeOffsetUnitsToPreviewPx(offsetXUnits, geom.maxOffsetPx);
    const float offsetYPx = editorMapVfxTrailConeOffsetUnitsToPreviewPx(offsetYUnits, geom.maxOffsetPx);
    geom.coneOriginX = geom.layerAnchorX + offsetXPx;
    geom.coneOriginY = geom.layerAnchorY + offsetYPx;
    geom.maxLengthPx = (std::max)(24.0f, (std::min)(previewRect.w, previewRect.h) * 0.48f);
    geom.lengthPx = editorMapVfxTrailConeLengthToPreviewPx(lengthValue, geom.maxLengthPx);
    const float dirOffRad = directionOffsetDeg * (3.14159265359f / 180.0f);
    geom.axisAngleRad = kTrailConePopupRearAxisRad + dirOffRad;
    const float coneHalfAngleRad =
        (std::clamp)(halfAngleDeg, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax) *
        (3.14159265359f / 180.0f);

    geom.tipX = geom.coneOriginX + std::cos(geom.axisAngleRad) * geom.lengthPx;
    geom.tipY = geom.coneOriginY + std::sin(geom.axisAngleRad) * geom.lengthPx;
    geom.leftX = geom.coneOriginX + std::cos(geom.axisAngleRad - coneHalfAngleRad) * geom.lengthPx;
    geom.leftY = geom.coneOriginY + std::sin(geom.axisAngleRad - coneHalfAngleRad) * geom.lengthPx;
    geom.rightX = geom.coneOriginX + std::cos(geom.axisAngleRad + coneHalfAngleRad) * geom.lengthPx;
    geom.rightY = geom.coneOriginY + std::sin(geom.axisAngleRad + coneHalfAngleRad) * geom.lengthPx;
    // Poignee largeur: placee sur la branche droite du cone.
    geom.sideX = geom.rightX;
    geom.sideY = geom.rightY;
    return geom;
}

/**
 * Echantillon (profondeur le long de l'axe, ecart lateral) pour un rejet dans le cone :
 * repartition uniforme en aire du triangle (apex -> profondeur max), donc largeur equilibree
 * (plus de masse au centre comme avec une Gaussienne). La profondeur max est bornee pour que,
 * au fond du cone, la demi-largeur ne depasse pas |lateralSpread| (aligne demi-angle + "rayon").
 */
void editorMapVfxSampleTrailConeDepthAndLateral(
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
    const float u1 = (std::max)(u01(editorMapVfxTrailJitterRng()), 1.0e-6f);
    const float u2 = u01(editorMapVfxTrailJitterRng());
    const float depth = maxDepth * std::sqrt(u1);
    *outDepth = depth;
    *outLateral = (2.0f * u2 - 1.0f) * depth * tanHalf;
}

uint32_t editorMapVfxHashU32(uint32_t x)
{
    x ^= x >> 16U;
    x *= 0x85ebca6bu;
    x ^= x >> 13U;
    x *= 0xc2b2ae35u;
    x ^= x >> 16U;
    return x;
}

float editorMapVfxSmoothNoise1D(uint32_t seed, float t)
{
    const int k0 = static_cast<int>(std::floor(t));
    const int k1 = k0 + 1;
    const float f = t - static_cast<float>(k0);
    const float u = f * f * (3.0f - 2.0f * f);
    const uint32_t h0 =
        editorMapVfxHashU32(seed ^ (0x27d4eb2du + static_cast<uint32_t>(k0) * 0x9e3779b9u));
    const uint32_t h1 =
        editorMapVfxHashU32(seed ^ (0x27d4eb2du + static_cast<uint32_t>(k1) * 0x9e3779b9u));
    const float v0 = static_cast<float>(h0) * (2.0f / 4294967296.0f) - 1.0f;
    const float v1 = static_cast<float>(h1) * (2.0f / 4294967296.0f) - 1.0f;
    return v0 + (v1 - v0) * u;
}

int editorMapVfxParseIntClamped(const char* str, int lo, int hi, int fallback)
{
    if (str == nullptr || str[0] == '\0')
    {
        return fallback;
    }
    char* end = nullptr;
    const long v = std::strtol(str, &end, 10);
    if (end == str)
    {
        return fallback;
    }
    int iv = static_cast<int>(v);
    if (iv < lo)
    {
        iv = lo;
    }
    if (iv > hi)
    {
        iv = hi;
    }
    return iv;
}

float editorMapVfxParseFloat(const char* str, float fallback)
{
    if (str == nullptr || str[0] == '\0')
    {
        return fallback;
    }
    char* end = nullptr;
    const float v = std::strtof(str, &end);
    if (end == str)
    {
        return fallback;
    }
    return v;
}
} // namespace

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
/** Espacement vertical entre les lignes du panneau Layers (aligne dessin, clic, drag). */
constexpr float kLayerListPanelRowGap = 6.0f;
/**
 * Sentinel dans la liste d'affichage Layers : une ligne Ã‚Â« sous-instance Ã‚Â» par rejet de trainee actif.
 * code = kLayerPanelTrailPieceRowMarker - (int)layerPanelUiId (uiId > 0, raisonnable pour rester dans int32).
 */
constexpr int kLayerPanelTrailPieceRowMarker = -3000000;

inline bool layerPanelRowIsTrailPieceSubRow(int code)
{
    return code <= kLayerPanelTrailPieceRowMarker;
}

inline uint32_t layerPanelTrailPieceUiIdFromRow(int code)
{
    return static_cast<uint32_t>(kLayerPanelTrailPieceRowMarker - code);
}

inline int layerPanelTrailPieceRowCodeFromUiId(uint32_t uiId)
{
    return kLayerPanelTrailPieceRowMarker - static_cast<int>(uiId);
}

struct ShipVfxLayerPagePickerLayout
{
    SDL_FRect targetSectorsOverlayToggleRect{};
    SDL_FRect targetSectorsToggleRect{};
    SDL_FRect duplicateToPagesButtonRect{};
    SDL_FRect pageButtonRect{};
    SDL_FRect popupRect{};
    SDL_FRect pageRowRects[kShipVfxLayerPageCount]{};
};

static ShipVfxLayerPagePickerLayout buildShipVfxLayerPagePickerLayout(
    const SDL_FRect& layerListRect,
    float panelPadding,
    float headerHeight,
    bool pickerOpen,
    int pickerPageRowCount)
{
    ShipVfxLayerPagePickerLayout out{};
    const float rowsWidth = layerListRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);
    constexpr float kPageBtnW = 56.0f;
    constexpr float kDupPagesBtnW = 52.0f;
    constexpr float kAbToggleBtnW = 44.0f;
    constexpr float kOverlayToggleBtnW = 40.0f;
    constexpr float kHeaderPageBtnGap = 4.0f;
    out.pageButtonRect.x = layerListRect.x + panelPadding + rowsWidth - kPageBtnW;
    out.pageButtonRect.y = layerListRect.y + 1.0f;
    out.pageButtonRect.w = kPageBtnW;
    out.pageButtonRect.h = headerHeight + 2.0f;
    out.duplicateToPagesButtonRect.w = kDupPagesBtnW;
    out.duplicateToPagesButtonRect.h = headerHeight + 2.0f;
    out.duplicateToPagesButtonRect.x = out.pageButtonRect.x - kDupPagesBtnW - kHeaderPageBtnGap;
    out.duplicateToPagesButtonRect.y = layerListRect.y + 1.0f;
    out.targetSectorsToggleRect.w = kAbToggleBtnW;
    out.targetSectorsToggleRect.h = headerHeight + 2.0f;
    out.targetSectorsToggleRect.x = out.duplicateToPagesButtonRect.x - kAbToggleBtnW - kHeaderPageBtnGap;
    out.targetSectorsToggleRect.y = layerListRect.y + 1.0f;
    out.targetSectorsOverlayToggleRect.w = kOverlayToggleBtnW;
    out.targetSectorsOverlayToggleRect.h = headerHeight + 2.0f;
    out.targetSectorsOverlayToggleRect.x =
        out.targetSectorsToggleRect.x - kOverlayToggleBtnW - kHeaderPageBtnGap;
    out.targetSectorsOverlayToggleRect.y = layerListRect.y + 1.0f;

    if (!pickerOpen)
    {
        return out;
    }

    const int rowCount = std::clamp(pickerPageRowCount, 1, kShipVfxLayerPageCount);
    const float popPad = 4.0f;
    const float rowH = 16.0f;
    const float rowGap = 2.0f;
    const float popW = (std::min)(layerListRect.w - (panelPadding * 2.0f), 420.0f);
    const float popH = popPad * 2.0f + (rowH * static_cast<float>(rowCount)) +
        (rowGap * static_cast<float>(rowCount - 1));
    out.popupRect.x = layerListRect.x + panelPadding;
    out.popupRect.y = layerListRect.y + panelPadding + headerHeight + 2.0f;
    out.popupRect.w = popW;
    out.popupRect.h = popH;

    float y = out.popupRect.y + popPad;
    for (int i = 0; i < rowCount; ++i)
    {
        out.pageRowRects[i].x = out.popupRect.x + popPad;
        out.pageRowRects[i].y = y;
        out.pageRowRects[i].w = out.popupRect.w - (popPad * 2.0f);
        out.pageRowRects[i].h = rowH;
        y += rowH + rowGap;
    }
    return out;
}

static void shipVfxLayerPageLabelUtf8(int pageIndex, bool targetSectorsAB, char* buf, size_t bufSize)
{
    static const char* kDir[] = {"Bas-Gauche", "Haut-Droite", "Haut-Gauche", "Bas-Droite"};
    const int dir = pageIndex % 4;
    const int st = (pageIndex / 4) % 2;
    const char* hp = (st == 0) ? "HP PLEIN" : "HP BAS";
    if (!targetSectorsAB)
    {
        SDL_snprintf(
            buf,
            bufSize,
            "%d/%d  %s  |  %s",
            pageIndex + 1,
            kShipVfxLayerPageCountNoTargetSectors,
            kDir[dir],
            hp);
        return;
    }
    const int sec = pageIndex / 8;
    const char* zone = (sec == 0) ? "CIBLE A" : "CIBLE B";
    SDL_snprintf(
        buf,
        bufSize,
        "%d/%d  %s  |  %s  |  %s",
        pageIndex + 1,
        kShipVfxLayerPageCount,
        kDir[dir],
        hp,
        zone);
}

/** Cote de la grille iso debug (tuiles) centree sur le navire preview. */
constexpr int kShipVfxDebugIsoGridTiles = 30;
/** Largeur du bouton TILE/PIXEL (panneau layers), a gauche de DEBUG. */
constexpr float kLayerRowVfxPlaceModeButtonW = 58.0f;
/** Largeur du bouton ROT (cadran rotation), a gauche de TILE. */
constexpr float kLayerRowVfxRotateDialButtonW = 52.0f;
/** Largeur des boutons FLIP V / FLIP H (panneau layers), a gauche de ROT. */
constexpr float kLayerRowFlipButtonW = 56.0f;
/** Bouton RELATIF (decal spawn / phase), a gauche de FLIP V. */
constexpr float kLayerRowRelativeButtonW = 78.0f;
/** Bouton unique SPAWN|CIBLE (comme TILE/PIXEL), a gauche de RELATIF. */
constexpr float kLayerRowMotionSpawnCibleToggleW = 92.0f;
constexpr float kLayerRowMotionClusterGap = 4.0f;
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
constexpr int kLoosePreviewDurationMsMin = 1;
constexpr int kLoosePreviewDurationMsMax = 99999999;
/** Opacite du VFX sous le curseur (preview non pose) vs instances placees (255). */
constexpr Uint8 kLoosePlacementCursorPreviewAlpha = 168;

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

constexpr RC2D_FileDialogFilter kJsonFileFilters[] = {
    {"JSON", "json"},
    {"Tous les fichiers", "*"},
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

static std::string extractAssetsRelativePath(const std::string& sourcePath)
{
    const std::string normalized = normalizePathSlashes(trimAscii(sourcePath));
    if (normalized.empty())
    {
        return {};
    }

    const std::string lowered = makePathKeyLower(normalized);
    const size_t assetsPos = lowered.find("/assets/");
    if (assetsPos != std::string::npos)
    {
        return normalized.substr(assetsPos + 1U);
    }
    if (lowered.rfind("assets/", 0U) == 0U)
    {
        return normalized;
    }

    return {};
}

static std::string buildAssetsRelativePathForExport(const std::string& sourcePath, const std::string& fallbackName)
{
    const std::string relativePath = extractAssetsRelativePath(sourcePath);
    if (!relativePath.empty())
    {
        return relativePath;
    }

    const std::string fileName = extractFileName(fallbackName.empty() ? sourcePath : fallbackName);
    if (fileName.empty())
    {
        return "assets/unknown";
    }
    return "assets/" + fileName;
}

static std::string makeComparableSourcePathKey(const std::string& sourcePath)
{
    const std::string relativePath = extractAssetsRelativePath(sourcePath);
    if (!relativePath.empty())
    {
        return makePathKeyLower(relativePath);
    }
    return makePathKeyLower(sourcePath);
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

static bool readTextFileUtf8(const std::string& absolutePath, std::string* outText)
{
    if (outText == nullptr)
    {
        return false;
    }
    outText->clear();
    if (absolutePath.empty())
    {
        return false;
    }
    std::ifstream input(absolutePath, std::ios::binary | std::ios::ate);
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
    outText->assign(static_cast<size_t>(size), '\0');
    if (!input.read(outText->data(), size))
    {
        outText->clear();
        return false;
    }
    return true;
}

static void setOrReplaceJsonStringField(cJSON* object, const char* key, const std::string& value)
{
    if (object == nullptr || key == nullptr || key[0] == '\0')
    {
        return;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    cJSON_AddStringToObject(object, key, value.c_str());
}

static cJSON* ensureJsonObjectField(cJSON* parentObject, const char* key)
{
    if (parentObject == nullptr || key == nullptr || key[0] == '\0')
    {
        return nullptr;
    }
    cJSON* child = cJSON_GetObjectItemCaseSensitive(parentObject, key);
    if (cJSON_IsObject(child))
    {
        return child;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(parentObject, key);
    cJSON* created = cJSON_CreateObject();
    if (created == nullptr)
    {
        return nullptr;
    }
    cJSON_AddItemToObject(parentObject, key, created);
    return created;
}

static std::string buildShipVfxDuplicateTargetFileName(
    const std::string& sourceFileName,
    const std::string& sourceShipSlug,
    const std::string& targetShipSlug,
    const std::string& animationDisplayName)
{
    std::filesystem::path sourcePath(sourceFileName);
    std::string sourceStem = trimAscii(sourcePath.stem().string());
    if (sourceStem.empty())
    {
        sourceStem = "fx-vfx";
    }
    std::string sourceStemLower = makePathKeyLower(sourceStem);
    const std::string sourceShipLower = makePathKeyLower(sourceShipSlug);
    const std::string targetShipTrimmed = trimAscii(targetShipSlug);

    std::string targetStem = sourceStem;

    auto replaceSuffix = [&targetStem, &sourceStemLower](const std::string& suffixLower, const std::string& replacement) {
        if (suffixLower.empty() || sourceStemLower.size() < suffixLower.size())
        {
            return false;
        }
        if (sourceStemLower.compare(sourceStemLower.size() - suffixLower.size(), suffixLower.size(), suffixLower) != 0)
        {
            return false;
        }
        targetStem = targetStem.substr(0, targetStem.size() - suffixLower.size()) + replacement;
        return true;
    };

    const bool replacedShipPrefixedSuffix = replaceSuffix("_ship-" + sourceShipLower, "_ship-" + targetShipTrimmed);
    const bool replacedBasicSuffix = replacedShipPrefixedSuffix
        ? true
        : replaceSuffix("_" + sourceShipLower, "_" + targetShipTrimmed);
    if (!replacedBasicSuffix)
    {
        std::string animationSlug = makeExportAnimationSlug(animationDisplayName);
        if (animationSlug.empty())
        {
            animationSlug = makeExportAnimationSlug(sourceStem);
        }
        if (animationSlug.empty())
        {
            animationSlug = "vfx";
        }
        targetStem = "fx-" + animationSlug + "_" + targetShipTrimmed;
    }

    return targetStem + ".json";
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
    /** true si cle absente, JSON null, ou valeur invalide : duree de boucle d'anim infinie cote data. */
    bool animationTotalDurationInfinite = true;
    int animationTotalDurationMs = 0;
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

    bool animationTotalDurationInfinite = true;
    int animationTotalDurationMsValue = 0;
    const cJSON* animMsNode = cJSON_GetObjectItemCaseSensitive(root, "animationTotalDurationMs");
    if (cJSON_IsNull(animMsNode))
    {
        animationTotalDurationInfinite = true;
    }
    else if (cJSON_IsNumber(animMsNode) && std::isfinite(animMsNode->valuedouble))
    {
        const long long v = std::llround(animMsNode->valuedouble);
        if (v >= static_cast<long long>(kLoosePreviewDurationMsMin) &&
            v <= static_cast<long long>(kLoosePreviewDurationMsMax))
        {
            animationTotalDurationInfinite = false;
            animationTotalDurationMsValue = static_cast<int>(v);
        }
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
    outResult->animationTotalDurationInfinite = animationTotalDurationInfinite;
    outResult->animationTotalDurationMs = animationTotalDurationMsValue;
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

/** Fond des apercus popup trainee : shader ocean dans le rectangle (clip SDL), bordure legere. */
static void editorMapVfxDrawTrailPopupPreviewOceanBackground(const SDL_FRect& r, float worldPreviewZoom)
{
    if (r.w < 4.0f || r.h < 4.0f)
    {
        return;
    }
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        SDL_Rect clip{};
        clip.x = static_cast<int>(std::floor(r.x));
        clip.y = static_cast<int>(std::floor(r.y));
        clip.w = (std::max)(static_cast<int>(std::ceil(r.x + r.w)) - clip.x, 1);
        clip.h = (std::max)(static_cast<int>(std::ceil(r.y + r.h)) - clip.y, 1);
        SDL_SetRenderClipRect(renderer, &clip);
    }
    OceanShader& ocean = GetOceanShader();
    if (ocean.isReady())
    {
        ocean.drawUiScreenRect(r, worldPreviewZoom);
    }
    else
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{16, 22, 30, 245});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }
    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{100, 122, 148, 210});
    rc2d_graphics_rectangle("line", &r);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
} // namespace

EditorMapVfxScene* EditorMapVfxScene::activeInstance = nullptr;

EditorMapVfxScene::EditorMapVfxScene(void)
    : backgroundWidget{},
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
      looseReferenceGuildIslandImage{},
      looseReferenceTowerLevel1Image{},
      looseReferenceTowerLevel2Image{},
      looseReferenceTowerLevel3Image{},
      looseReferenceTowerLevel4Image{},
      looseReferenceShipLeftImage{},
      looseReferenceShipRightImage{},
      looseReferencePreviewVisible(false),
      looseReferencePreviewLoaded(false),
      selectedVfxInstanceIndex(-1),
      nextVfxInstanceId(1U),
      looseScalePercent(100),
      loosePreviewZoomFactor(kLoosePreviewZoomDefault),
      shipVfxPreviewZoomFactor(kLoosePreviewZoomDefault),
      loosePreviewMode(LoosePreviewMode::CENTER_SPRITESHEET),
      loosePreviewPlacementSnapToTile(true),
      loosePreviewPlacements{},
      nextLoosePreviewPlacementId(1U),
      loosePreviewFpsInput("12"),
      loosePreviewFpsInputFocused(false),
      loosePreviewTotalDurationMsInput{},
      loosePreviewTotalDurationMsInputFocused(false),
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
      looseImportBatchActive(false),
      looseImportBatchFolderPaths{},
      looseImportBatchNextIndex(0U),
      looseImportBatchAddedCount(0),
      looseImportBatchReloadedCount(0),
      looseImportBatchFailedCount(0),
      pendingExportFolderDialogCompleted(false),
      pendingExportFolderDialogCanceled(false),
      pendingExportFolderAbsolute{},
      pendingExportMode(EditorMode::SHIP_VFX),
      pendingExportFolderMutex{},
      pendingShipVfxConfigDialogCompleted(false),
      pendingShipVfxConfigDialogCanceled(false),
      pendingShipVfxConfigAbsolutePath{},
      pendingShipVfxConfigMutex{},
      buttonModeShipVfxRect{},
      buttonModeLooseSpritesRect{},
      buttonImportSfxRect{},
      buttonImportLooseRect{},
      buttonExportRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonShipOrderMinusRect{},
      buttonShipOrderPlusRect{},
      buttonVfxOrderMinusRect{},
      buttonVfxOrderPlusRect{},
      buttonShipOpacityMinusRect{},
      buttonShipOpacityPlusRect{},
      buttonPreviewIsoGridRect{},
      buttonShipVfxZoomMinusRect{},
      buttonShipVfxZoomPlusRect{},
      previewShipPilotActive(false),
      pilotVfxMaxDurationClockAnchorSeconds(0.0f),
      buttonShipPilotRect{},
      buttonFollowShipRect{},
      buttonRemoveVfxRect{},
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
      buttonLoosePreviewTotalDurationMsInputRect{},
      buttonLooseClearAllVfxRect{},
      buttonLoosePreviewPlacementSnapRect{},
      shipListRect{},
      sfxListRect{},
      sfxListActionButtonsVisible(false),
      sfxListActionButtonsSfxIndex(-1),
      sfxListActionAutoImportRect{},
      sfxListActionAddInstanceRect{},
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
    this->previewTargetFireSectorIndex = 0;
    this->shipVfxEditorTargetSectorsABEnabled = false;
    this->shipVfxEditorTargetingMode = targetingModeFromTargetSectorsAB(false);
    this->shipVfxEditorTargetSectorsOverlayVisible = false;
    this->previewShipOpacityPercent = 100;
    this->initDefaultShipLayerSettingsAllPages();
    this->shipLayerSelected = false;
    this->previewIsoGridVisible = false;
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->vfxRotationDialActive = false;
    this->vfxDragStartMouseX = 0.0f;
    this->vfxDragStartMouseY = 0.0f;
    this->vfxDragStartOffsetX = 0.0f;
    this->vfxDragStartOffsetY = 0.0f;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath.clear();
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();
    this->buttonReloadAssetsRect = SDL_FRect{};
    this->buttonShipVfxDuplicateShipsRect = SDL_FRect{};
    this->buttonDirectionPrevRect = SDL_FRect{};
    this->buttonDirectionNextRect = SDL_FRect{};
    this->buttonShipStateToggleRect = SDL_FRect{};
    this->buttonTargetFireSectorToggleRect = SDL_FRect{};
    this->buttonShipOpacityMinusRect = SDL_FRect{};
    this->buttonShipOpacityPlusRect = SDL_FRect{};
    this->buttonLayerOrderMinusRect = SDL_FRect{};
    this->buttonLayerOrderPlusRect = SDL_FRect{};
    this->buttonVisibleRect = SDL_FRect{};
    this->buttonLockedRect = SDL_FRect{};
    this->buttonBehindShipRect = SDL_FRect{};
    this->buttonDuplicateVfxRect = SDL_FRect{};
    this->buttonLayerNameInputRect = SDL_FRect{};
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

void EditorMapVfxScene::initDefaultShipLayerSettingsAllPages(void)
{
    for (size_t i = 0; i < this->shipDrawOrderByPage.size(); ++i)
    {
        this->shipDrawOrderByPage[i] = 0;
        this->shipLayerVisibleByPage[i] = true;
        this->shipLayerLockedByPage[i] = false;
        this->shipDebugBoundsVisibleByPage[i] = true;
        this->shipSpawnAfterVfxInstanceId[i] = 0U;
        this->shipSpawnAfterDelayMs[i] = 0;
    }
}

void EditorMapVfxScene::clearAllShipVfxLayerPages(void)
{
    for (std::vector<ShipVfxInstance>& page : this->shipVfxLayerPages)
    {
        page.clear();
    }
}

void EditorMapVfxScene::clearShipVfxLayerUiTransientStateForPageChange(void)
{
    this->closeVfxTrailPopup();
    this->selectedVfxInstanceIndex = -1;
    this->shipLayerSelected = false;
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->vfxRotationDialActive = false;
    this->layerRowDragActive = false;
    this->layerRowDragMoved = false;
    this->layerRowDragSourceDisplayIndex = -1;
    this->layerRowDragTargetInsertIndex = -1;
    this->layerRowDragStartMouseY = 0.0f;
}

ShipVfxTargetingMode EditorMapVfxScene::targetingModeFromTargetSectorsAB(bool enabled)
{
    return enabled ? ShipVfxTargetingMode::TARGET_RELATIVE_AB : ShipVfxTargetingMode::NONE;
}

const char* EditorMapVfxScene::targetingModeToJsonId(ShipVfxTargetingMode mode)
{
    return (mode == ShipVfxTargetingMode::TARGET_RELATIVE_AB) ? "target_relative_ab" : "none";
}

ShipVfxTargetingMode EditorMapVfxScene::targetingModeFromJsonId(const char* id, bool* outRecognized)
{
    if (outRecognized != nullptr)
    {
        *outRecognized = false;
    }
    if (id == nullptr)
    {
        return ShipVfxTargetingMode::NONE;
    }
    if (SDL_strcasecmp(id, "target_relative_ab") == 0 ||
        SDL_strcasecmp(id, "target-relative-ab") == 0 ||
        SDL_strcasecmp(id, "target_relative") == 0)
    {
        if (outRecognized != nullptr)
        {
            *outRecognized = true;
        }
        return ShipVfxTargetingMode::TARGET_RELATIVE_AB;
    }
    if (SDL_strcasecmp(id, "none") == 0 || SDL_strcasecmp(id, "legacy") == 0)
    {
        if (outRecognized != nullptr)
        {
            *outRecognized = true;
        }
        return ShipVfxTargetingMode::NONE;
    }
    return ShipVfxTargetingMode::NONE;
}

void EditorMapVfxScene::applyShipVfxLayerPageIndex(int pageIndex)
{
    const int p = std::clamp(pageIndex, 0, kShipVfxLayerPageCount - 1);
    this->previewDirectionIndex = p % 4;
    this->previewShipStateIndex = (p / 4) % 2;
    this->previewTargetFireSectorIndex = (p / 8) % 2;
    this->applyPreviewDirectionToShip();
    this->previewShip.setHealthVisual((this->previewShipStateIndex == 1) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
    this->clearShipVfxLayerUiTransientStateForPageChange();
    this->shipVfxLayerPagePickerOpen = false;
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleShipVfxEditorTargetSectorsAB(void)
{
    const bool next = !this->shipVfxEditorTargetSectorsABEnabled;
    if (!next)
    {
        const int key = this->getShipVfxLayerPageKey();
        if (key >= kShipVfxLayerPageCountNoTargetSectors)
        {
            this->applyShipVfxLayerPageIndex(key % kShipVfxLayerPageCountNoTargetSectors);
        }
        else
        {
            this->previewTargetFireSectorIndex = 0;
        }
        this->shipVfxEditorTargetSectorsOverlayVisible = false;
    }
    this->shipVfxEditorTargetSectorsABEnabled = next;
    this->shipVfxEditorTargetingMode = targetingModeFromTargetSectorsAB(next);
    this->shipVfxLayerPagePickerOpen = false;
    this->closeVfxDuplicateToPagesPopup();
    this->closeShipVfxShipDuplicatePopup();
    this->markShipVfxDirty();
    this->statusMessage = this->shipVfxEditorTargetSectorsABEnabled
        ? "Pages VFX : 16 (direction x HP x cibles A/B)."
        : "Pages VFX : 8 (direction x HP, sans A/B).";
}

void EditorMapVfxScene::toggleShipVfxEditorTargetSectorsOverlayVisible(void)
{
    if (!this->shipVfxEditorTargetSectorsABEnabled)
    {
        return;
    }
    this->shipVfxEditorTargetSectorsOverlayVisible = !this->shipVfxEditorTargetSectorsOverlayVisible;
    this->statusMessage = this->shipVfxEditorTargetSectorsOverlayVisible
        ? "Secteurs cibles A/B : affichage sur la preview active."
        : "Secteurs cibles A/B : affichage sur la preview masque.";
}

void EditorMapVfxScene::resetEditorState(void)
{
    this->clearShipVfxPairDraftStates();
    this->editorMode = EditorMode::SHIP_VFX;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->selectedShipIndex = -1;
    this->selectedSfxIndex = -1;
    this->sfxListActionButtonsVisible = false;
    this->sfxListActionButtonsSfxIndex = -1;
    this->sfxListActionAutoImportRect = SDL_FRect{};
    this->sfxListActionAddInstanceRect = SDL_FRect{};
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
    this->previewTargetFireSectorIndex = 0;
    this->shipVfxEditorTargetSectorsABEnabled = false;
    this->shipVfxEditorTargetingMode = targetingModeFromTargetSectorsAB(false);
    this->shipVfxEditorTargetSectorsOverlayVisible = false;
    this->previewShipOpacityPercent = 100;
    this->initDefaultShipLayerSettingsAllPages();
    this->shipLayerSelected = false;
    this->previewIsoGridVisible = false;
    this->previewShipPilotActive = false;
    this->pilotVfxMaxDurationClockAnchorSeconds = 0.0f;
    this->previewShipTile = SDL_FPoint{0.0f, 0.0f};
    this->clearShipVfxTrailPieces();
    this->clearAllShipVfxLayerPages();
    this->selectedVfxInstanceIndex = -1;
    this->nextVfxInstanceId = 1U;
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->sfxListActionButtonsVisible = false;
    this->sfxListActionButtonsSfxIndex = -1;
    this->vfxDragActive = false;
    this->vfxRotationDialActive = false;
    this->vfxDragStartMouseX = 0.0f;
    this->vfxDragStartMouseY = 0.0f;
    this->vfxDragStartOffsetX = 0.0f;
    this->vfxDragStartOffsetY = 0.0f;
    this->shipVfxLayerPagePickerOpen = false;
    this->closeVfxDuplicateToPagesPopup();
    this->closeShipVfxShipDuplicatePopup();
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
    this->shipVfxPreviewZoomFactor = kLoosePreviewZoomDefault;
    this->loosePreviewMode = LoosePreviewMode::CENTER_SPRITESHEET;
    this->loosePreviewPlacementSnapToTile = true;
    this->loosePreviewPlacements.clear();
    this->nextLoosePreviewPlacementId = 1U;
    this->loosePreviewFpsInput = "12";
    this->loosePreviewFpsInputFocused = false;
    this->loosePreviewTotalDurationMsInput.clear();
    this->loosePreviewTotalDurationMsInputFocused = false;
    this->exportConfirmPopupVisible = false;
    this->exportConfirmPopupAction = ExportConfirmAction::NONE;
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
    this->looseImportBatchActive = false;
    this->looseImportBatchFolderPaths.clear();
    this->looseImportBatchNextIndex = 0U;
    this->looseImportBatchAddedCount = 0;
    this->looseImportBatchReloadedCount = 0;
    this->looseImportBatchFailedCount = 0;
    this->pendingShipVfxConfigDialogCompleted = false;
    this->pendingShipVfxConfigDialogCanceled = false;
    this->pendingShipVfxConfigAbsolutePath.clear();
    this->statusMessage = "Editor VFX pret.";
}

void EditorMapVfxScene::clearEditorTransientInteractionState(void)
{
    this->layerNameInputFocused = false;
    this->loosePreviewFpsInputFocused = false;
    this->loosePreviewTotalDurationMsInputFocused = false;
    this->vfxDragActive = false;
    this->vfxRotationDialActive = false;
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
    this->shipVfxLayerPagePickerOpen = false;
    this->closeVfxRelativeTimingPopup();
    this->closeVfxTrailPopup();
    this->closeVfxDuplicateToPagesPopup();
    this->closeShipVfxShipDuplicatePopup();
    this->closeExportConfirmPopup();
    this->previewShipPilotActive = false;
    this->pilotVfxMaxDurationClockAnchorSeconds = 0.0f;
}

void EditorMapVfxScene::applyShipVfxModeViewportReset(void)
{
    this->previewShipPilotActive = false;
    this->pilotVfxMaxDurationClockAnchorSeconds = 0.0f;
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    map.update();
    this->shipVfxPreviewZoomFactor = kLoosePreviewZoomDefault;
    camera.setZoomFactor(kLoosePreviewZoomDefault);
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    const float shipScreenX = map.rect.x + (map.rect.w * 0.5f) + 450.0f;
    const float shipScreenY = map.rect.y + (map.rect.h * 0.5f) - 100.0f;
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
    this->closeExportConfirmPopup();
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
        ReleaseStorageImage(&sfx.image);
        sfx.frames.clear();
    }
    this->importedSfx.clear();
    this->selectedSfxIndex = -1;
    this->sfxListActionButtonsVisible = false;
    this->sfxListActionButtonsSfxIndex = -1;
}

void EditorMapVfxScene::unloadImportedLooseFolders(void)
{
    for (ImportedLooseFolder& folder : this->importedLooseFolders)
    {
        for (ImportedLooseSprite& sprite : folder.sprites)
        {
            ReleaseStorageImage(&sprite.image);
        }
        folder.sprites.clear();
    }
    this->importedLooseFolders.clear();
}

void EditorMapVfxScene::loadLooseReferencePreviewAssets(void)
{
    this->unloadLooseReferencePreviewAssets();

    this->looseReferenceGuildIslandImage = LoadStorageImage(
        "assets/images/scene-editormap-vfx/iles-guild/guild_island.png",
        RC2D_STORAGE_TITLE);

    this->looseReferenceTowerLevel1Image = LoadStorageImage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl1.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel2Image = LoadStorageImage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl2.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel3Image = LoadStorageImage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl3.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceTowerLevel4Image = LoadStorageImage(
        "assets/images/scene-editormap-vfx/towers/tower_lvl4.png",
        RC2D_STORAGE_TITLE);

    const bool islandLoaded = (this->looseReferenceGuildIslandImage.sdl_texture != nullptr);
    const bool tower1Loaded = (this->looseReferenceTowerLevel1Image.sdl_texture != nullptr);
    const bool tower2Loaded = (this->looseReferenceTowerLevel2Image.sdl_texture != nullptr);
    const bool tower3Loaded = (this->looseReferenceTowerLevel3Image.sdl_texture != nullptr);
    const bool tower4Loaded = (this->looseReferenceTowerLevel4Image.sdl_texture != nullptr);
    this->looseReferenceShipLeftImage = LoadStorageImage(
        "assets/images/scene-editormap-vfx/ships/test1/1.png",
        RC2D_STORAGE_TITLE);
    this->looseReferenceShipRightImage = LoadStorageImage(
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
    ResetStorageImageRef(&this->looseReferenceShipLeftImage);
    ResetStorageImageRef(&this->looseReferenceShipRightImage);
    ResetStorageImageRef(&this->looseReferenceGuildIslandImage);
    ResetStorageImageRef(&this->looseReferenceTowerLevel1Image);
    ResetStorageImageRef(&this->looseReferenceTowerLevel2Image);
    ResetStorageImageRef(&this->looseReferenceTowerLevel3Image);
    ResetStorageImageRef(&this->looseReferenceTowerLevel4Image);
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

void EditorMapVfxScene::adjustShipVfxPreviewZoom(float delta)
{
    if (this->editorMode != EditorMode::SHIP_VFX)
    {
        return;
    }
    const float currentZoom = this->shipVfxPreviewZoomFactor;
    const float nextZoom = std::clamp(currentZoom + delta, kLoosePreviewZoomMin, kLoosePreviewZoomMax);
    this->shipVfxPreviewZoomFactor = nextZoom;
    Camera& camera = GetCamera();
    Map& map = GetCurrentMap();
    camera.setZoomFactor(nextZoom);
    camera.update(map, map.rect);

    char status[128] = {};
    SDL_snprintf(status, sizeof(status), "Zoom Ship/VFX: %.2f", nextZoom);
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
    auto makeNormalizedPathKey = [](const std::string& path) -> std::string {
        return makePathKeyLower(normalizePathSlashes(path));
    };

    auto findImportedShipIndexByPathKey = [this, &makeNormalizedPathKey](const std::string& pathKey) -> int {
        if (pathKey.empty())
        {
            return -1;
        }
        for (int i = 0; i < static_cast<int>(this->importedShips.size()); ++i)
        {
            if (makeNormalizedPathKey(this->importedShips[static_cast<size_t>(i)].folderAbsolutePath) == pathKey)
            {
                return i;
            }
        }
        return -1;
    };

    auto findImportedSfxIndexByPathKey = [this, &makeNormalizedPathKey](const std::string& pathKey) -> int {
        if (pathKey.empty())
        {
            return -1;
        }
        for (int i = 0; i < static_cast<int>(this->importedSfx.size()); ++i)
        {
            if (makeNormalizedPathKey(this->importedSfx[static_cast<size_t>(i)].sourceFolderAbsolutePath) == pathKey)
            {
                return i;
            }
        }
        return -1;
    };

    const int shipCountBefore = static_cast<int>(this->importedShips.size());
    const int sfxCountBefore = static_cast<int>(this->importedSfx.size());

    // Sauvegarde du travail en cours avant scan disque.
    this->saveCurrentShipVfxPairDraft();

    std::string activeShipPathKey;
    if (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
    {
        activeShipPathKey = makeNormalizedPathKey(
            this->importedShips[static_cast<size_t>(this->selectedShipIndex)].folderAbsolutePath);
    }
    else if (!this->loadedShipFolderAbsolute.empty())
    {
        activeShipPathKey = makeNormalizedPathKey(this->loadedShipFolderAbsolute);
    }

    std::string activeSfxPathKey;
    if (this->selectedSfxIndex >= 0 && this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        activeSfxPathKey = makeNormalizedPathKey(
            this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].sourceFolderAbsolutePath);
    }

    this->importShipsFromRootFolderAbsolutePath("assets/images/ships");
    this->importSfxFromRootFolderAbsolutePath("assets/images/vfxship");

    // Remappe la selection apres tri/ajouts pour garder le meme ship/vfx actif.
    const int remappedShipIndex = findImportedShipIndexByPathKey(activeShipPathKey);
    if (remappedShipIndex >= 0)
    {
        this->selectedShipIndex = remappedShipIndex;
        this->ensureSelectionVisible(
            this->selectedShipIndex,
            &this->shipListScrollOffset,
            static_cast<int>(this->importedShips.size()));
    }
    else if (!this->importedShips.empty() &&
             (this->selectedShipIndex < 0 || this->selectedShipIndex >= static_cast<int>(this->importedShips.size())))
    {
        (void)this->selectImportedShipAtIndex(0);
    }
    else if (this->importedShips.empty())
    {
        this->selectedShipIndex = -1;
    }

    const int remappedSfxIndex = findImportedSfxIndexByPathKey(activeSfxPathKey);
    if (remappedSfxIndex >= 0)
    {
        this->selectedSfxIndex = remappedSfxIndex;
    }
    else if (!this->importedSfx.empty() &&
             (this->selectedSfxIndex < 0 || this->selectedSfxIndex >= static_cast<int>(this->importedSfx.size())))
    {
        this->selectedSfxIndex = 0;
    }
    else if (this->importedSfx.empty())
    {
        this->selectedSfxIndex = -1;
    }

    if (this->selectedSfxIndex >= 0 && this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        this->ensureSelectionVisible(
            this->selectedSfxIndex,
            &this->sfxListScrollOffset,
            static_cast<int>(this->importedSfx.size()));
        this->sfxListActionButtonsVisible = true;
        this->sfxListActionButtonsSfxIndex = this->selectedSfxIndex;
    }
    else
    {
        this->sfxListActionButtonsVisible = false;
        this->sfxListActionButtonsSfxIndex = -1;
    }

    // Si on est revenu sur le meme couple ship/vfx, restaure l'etat d'edition en cours.
    if (this->selectedShipIndex >= 0 &&
        this->selectedShipIndex < static_cast<int>(this->importedShips.size()) &&
        this->selectedSfxIndex >= 0 &&
        this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        const std::string currentShipKey = makeNormalizedPathKey(
            this->importedShips[static_cast<size_t>(this->selectedShipIndex)].folderAbsolutePath);
        const std::string currentSfxKey = makeNormalizedPathKey(
            this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)].sourceFolderAbsolutePath);
        if (!activeShipPathKey.empty() &&
            !activeSfxPathKey.empty() &&
            currentShipKey == activeShipPathKey &&
            currentSfxKey == activeSfxPathKey)
        {
            (void)this->restoreCurrentShipVfxPairDraft();
        }
    }

    const int shipCountAfter = static_cast<int>(this->importedShips.size());
    const int sfxCountAfter = static_cast<int>(this->importedSfx.size());
    const int addedShips = (std::max)(0, shipCountAfter - shipCountBefore);
    const int addedSfx = (std::max)(0, sfxCountAfter - sfxCountBefore);

    if (addedShips > 0 || addedSfx > 0)
    {
        this->statusMessage =
            "Reload assets: +" + std::to_string(addedShips) + " ship(s), +" +
            std::to_string(addedSfx) + " VFX. Etat courant conserve.";
    }
    else
    {
        this->statusMessage = "Reload assets: aucun nouvel asset, etat courant conserve.";
    }
}

std::string EditorMapVfxScene::buildShipConfigJsonPath(const ImportedShip& ship) const
{
    std::filesystem::path base(ship.folderAbsolutePath);
    const std::string slug = makeShipConfigSlug(ship.displayName);
    const std::string fileName = "animations_vfx_" + slug + ".json";
    return normalizePathSlashes((base / fileName).string());
}

std::string EditorMapVfxScene::buildShipVfxPairConfigJsonPath(
    const ImportedShip& ship,
    const ImportedSfx& sfx) const
{
    const std::string shipSlug = makeShipConfigSlug(ship.displayName);
    std::string normalizedVfxName = stripListPrefix(sfx.displayName, "vfx-");
    if (trimAscii(normalizedVfxName).empty())
    {
        normalizedVfxName = extractFileName(sfx.sourceFolderAbsolutePath);
    }
    std::string vfxSlug = makeExportAnimationSlug(normalizedVfxName);
    if (vfxSlug.empty())
    {
        return {};
    }

    const std::string fileName = "fx-" + vfxSlug + "_" + shipSlug + ".json";
    const std::filesystem::path pairPath = std::filesystem::path(ship.folderAbsolutePath) / fileName;
    return normalizePathSlashes(pairPath.string());
}

std::string EditorMapVfxScene::buildShipVfxPairDraftKey(int shipIndex, int sfxIndex) const
{
    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()) ||
        sfxIndex < 0 || sfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return {};
    }

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(shipIndex)];
    const ImportedSfx& sfx = this->importedSfx[static_cast<size_t>(sfxIndex)];
    const std::string pairPath = this->buildShipVfxPairConfigJsonPath(ship, sfx);
    if (!pairPath.empty())
    {
        return makePathKeyLower(normalizePathSlashes(pairPath));
    }

    return makePathKeyLower(normalizePathSlashes(ship.folderAbsolutePath)) + "|" +
        makePathKeyLower(normalizePathSlashes(sfx.sourceJsonPath)) + "|" +
        makePathKeyLower(sfx.displayName);
}

void EditorMapVfxScene::saveCurrentShipVfxPairDraft(void)
{
    const std::string pairKey = this->buildShipVfxPairDraftKey(this->selectedShipIndex, this->selectedSfxIndex);
    if (pairKey.empty())
    {
        return;
    }

    ShipVfxPairDraftState* draft = nullptr;
    for (ShipVfxPairDraftState& candidate : this->shipVfxPairDraftStates)
    {
        if (candidate.pairKey == pairKey)
        {
            draft = &candidate;
            break;
        }
    }
    if (draft == nullptr)
    {
        this->shipVfxPairDraftStates.push_back(ShipVfxPairDraftState{});
        draft = &this->shipVfxPairDraftStates.back();
    }

    draft->pairKey = pairKey;
    draft->shipIndex = this->selectedShipIndex;
    draft->sfxIndex = this->selectedSfxIndex;
    draft->layerPages = this->shipVfxLayerPages;
    draft->drawOrderByPage = this->shipDrawOrderByPage;
    draft->layerVisibleByPage = this->shipLayerVisibleByPage;
    draft->layerLockedByPage = this->shipLayerLockedByPage;
    draft->debugBoundsVisibleByPage = this->shipDebugBoundsVisibleByPage;
    draft->shipSpawnAfterInstanceIdByPage = this->shipSpawnAfterVfxInstanceId;
    draft->shipSpawnAfterDelayMsByPage = this->shipSpawnAfterDelayMs;
    draft->targetSectorsABEnabled = this->shipVfxEditorTargetSectorsABEnabled;
    draft->targetingMode = this->shipVfxEditorTargetingMode;
    draft->dirty = this->shipVfxDirty;
    draft->loadedConfigPath = this->loadedShipVfxConfigPath;
}

bool EditorMapVfxScene::restoreCurrentShipVfxPairDraft(void)
{
    const std::string pairKey = this->buildShipVfxPairDraftKey(this->selectedShipIndex, this->selectedSfxIndex);
    if (pairKey.empty())
    {
        return false;
    }

    const ShipVfxPairDraftState* draft = nullptr;
    for (const ShipVfxPairDraftState& candidate : this->shipVfxPairDraftStates)
    {
        if (candidate.pairKey == pairKey)
        {
            draft = &candidate;
            break;
        }
    }
    if (draft == nullptr)
    {
        return false;
    }

    this->shipVfxLayerPages = draft->layerPages;
    this->shipDrawOrderByPage = draft->drawOrderByPage;
    this->shipLayerVisibleByPage = draft->layerVisibleByPage;
    this->shipLayerLockedByPage = draft->layerLockedByPage;
    this->shipDebugBoundsVisibleByPage = draft->debugBoundsVisibleByPage;
    this->shipSpawnAfterVfxInstanceId = draft->shipSpawnAfterInstanceIdByPage;
    this->shipSpawnAfterDelayMs = draft->shipSpawnAfterDelayMsByPage;
    this->shipVfxEditorTargetSectorsABEnabled = draft->targetSectorsABEnabled;
    this->shipVfxEditorTargetingMode = draft->targetingMode;
    if ((this->shipVfxEditorTargetingMode == ShipVfxTargetingMode::TARGET_RELATIVE_AB) !=
        this->shipVfxEditorTargetSectorsABEnabled)
    {
        this->shipVfxEditorTargetingMode =
            EditorMapVfxScene::targetingModeFromTargetSectorsAB(this->shipVfxEditorTargetSectorsABEnabled);
    }
    if (!this->shipVfxEditorTargetSectorsABEnabled)
    {
        this->previewTargetFireSectorIndex = 0;
    }
    this->shipVfxEditorTargetSectorsOverlayVisible = false;
    this->clearShipVfxTrailPieces();
    this->setSelectedVfxInstanceIndex(-1);

    uint32_t maxInstanceId = 0U;
    for (std::vector<ShipVfxInstance>& page : this->shipVfxLayerPages)
    {
        for (ShipVfxInstance& instance : page)
        {
            instance.importedSfxIndex = this->selectedSfxIndex;
            maxInstanceId = (std::max)(maxInstanceId, instance.instanceId);
        }
    }
    this->nextVfxInstanceId = (maxInstanceId < (std::numeric_limits<uint32_t>::max)())
        ? (maxInstanceId + 1U)
        : maxInstanceId;
    this->normalizeShipVfxDrawOrders();
    this->shipVfxDirty = draft->dirty;
    this->loadedShipVfxConfigPath = draft->loadedConfigPath;
    if (this->loadedShipVfxConfigPath.empty() &&
        this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()) &&
        this->selectedSfxIndex >= 0 && this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        this->loadedShipVfxConfigPath = this->buildShipVfxPairConfigJsonPath(
            this->importedShips[static_cast<size_t>(this->selectedShipIndex)],
            this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)]);
    }
    return true;
}

void EditorMapVfxScene::clearShipVfxPairDraftStates(void)
{
    this->shipVfxPairDraftStates.clear();
}

bool EditorMapVfxScene::tryAutoImportShipVfxConfigForSelectedPair(bool* outPairFileFound)
{
    if (outPairFileFound != nullptr)
    {
        *outPairFileFound = false;
    }

    if (this->selectedShipIndex < 0 || this->selectedShipIndex >= static_cast<int>(this->importedShips.size()) ||
        this->selectedSfxIndex < 0 || this->selectedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return false;
    }

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(this->selectedShipIndex)];
    const ImportedSfx& sfx = this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)];
    const std::string pairConfigPath = this->buildShipVfxPairConfigJsonPath(ship, sfx);
    if (pairConfigPath.empty())
    {
        return false;
    }

    std::error_code fsError;
    if (!std::filesystem::exists(pairConfigPath, fsError) || !std::filesystem::is_regular_file(pairConfigPath, fsError))
    {
        return false;
    }

    if (outPairFileFound != nullptr)
    {
        *outPairFileFound = true;
    }

    const bool loaded = this->importShipVfxConfigFromPath(pairConfigPath.c_str());
    if (loaded)
    {
        this->loadedShipVfxConfigPath = normalizePathSlashes(pairConfigPath);
        this->statusMessage = "Configuration VFX chargee: " + this->loadedShipVfxConfigPath;
        return true;
    }

    this->statusMessage = "JSON VFX present mais invalide: " + pairConfigPath;
    return false;
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
    this->clearShipVfxLayerUiTransientStateForPageChange();
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
    this->clearShipVfxLayerUiTransientStateForPageChange();
}

void EditorMapVfxScene::cyclePreviewTargetFireSector(int delta)
{
    if (!this->shipVfxEditorTargetSectorsABEnabled)
    {
        return;
    }
    const int count = 2;
    int index = this->previewTargetFireSectorIndex + delta;
    while (index < 0)
    {
        index += count;
    }
    while (index >= count)
    {
        index -= count;
    }
    this->previewTargetFireSectorIndex = index;
    this->clearShipVfxLayerUiTransientStateForPageChange();
    this->markShipVfxDirty();
}

void EditorMapVfxScene::applyPreviewShipOpacityPercentToShip(void)
{
    const int p = std::clamp(this->previewShipOpacityPercent, 0, 100);
    const int alpha = (p * 255 + 50) / 100;
    this->previewShip.setDrawAlpha(static_cast<Uint8>(alpha));
}

void EditorMapVfxScene::adjustPreviewShipOpacityPercentStep(int deltaPercent)
{
    if (!this->previewShipLoaded)
    {
        this->statusMessage = "Chargez un navire pour regler l'opacite.";
        return;
    }
    this->previewShipOpacityPercent = std::clamp(this->previewShipOpacityPercent + deltaPercent, 0, 100);
    this->applyPreviewShipOpacityPercentToShip();
    char buf[72] = {};
    SDL_snprintf(buf, sizeof(buf), "Opacite navire: %d%%", this->previewShipOpacityPercent);
    this->statusMessage = buf;
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
    // Libelle court pour la barre d'outils (le bandeau info garde le detail via ce meme texte).
    return (this->previewShipStateIndex == 1)
        ? "SPRITES 5-8 (HP BAS)"
        : "SPRITES 1-4 (HP PLEIN)";
}

const char* EditorMapVfxScene::getPreviewTargetFireSectorLabel(void) const
{
    return (this->previewTargetFireSectorIndex == 1) ? "SECTEUR TIR B" : "SECTEUR TIR A";
}

void EditorMapVfxScene::markShipVfxDirty(void)
{
    this->shipVfxDirty = true;
}

void EditorMapVfxScene::closeVfxRelativeTimingPopup(void)
{
    this->vfxRelativeTimingPopupVisible = false;
    this->vfxRelativeTimingPopupStep = 0;
    this->vfxRelativeTimingPopupCandidateIndices.clear();
    this->vfxRelativeTimingPopupAnchorInstanceId = 0U;
    this->vfxRelativeTimingPopupDelayMsInput.clear();
    this->vfxRelativeTimingPopupDelayMsFocused = false;
}

void EditorMapVfxScene::closeVfxTrailPopup(void)
{
    this->closeVfxTrailConePopup();
    this->vfxTrailPopupVisible = false;
    this->vfxTrailPopupParentInstanceIndex = -1;
    this->vfxTrailPopupEveryNTilesInput.clear();
    this->vfxTrailPopupLifetimeTilesInput.clear();
    this->vfxTrailPopupLateralJitterInput.clear();
    this->vfxTrailPopupRotationPctInput.clear();
    this->vfxTrailPopupIdleRingPieceCountInput.clear();
    this->vfxTrailPopupIdlePeriodMsInput.clear();
    this->vfxTrailPopupIdleRadiusInput.clear();
    this->vfxTrailPopupIdleRingRotationPctInput.clear();
    this->vfxTrailPopupIdleRingPosJitterInput.clear();
    this->vfxTrailPopupStrictTilePlacement = true;
    this->vfxTrailPopupIdleRingWhenStationary = false;
    this->vfxTrailPopupEveryNTilesFocused = false;
    this->vfxTrailPopupLifetimeTilesFocused = false;
    this->vfxTrailPopupLateralJitterFocused = false;
    this->vfxTrailPopupRotationPctFocused = false;
    this->vfxTrailPopupIdleRingPieceCountFocused = false;
    this->vfxTrailPopupIdlePeriodMsFocused = false;
    this->vfxTrailPopupIdleRadiusFocused = false;
    this->vfxTrailPopupIdleRingRotationPctFocused = false;
    this->vfxTrailPopupIdleRingPosJitterFocused = false;
    this->vfxTrailPopupPreviewMarcheShipVisible = true;
    this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec = 0.0f;
    this->vfxTrailPopupPreviewCrownShipVisible = true;
    this->vfxTrailPopupPreviewZoom = 1.0f;
    this->vfxTrailPopupMarcheLastPilotMoveDirValid = false;
    this->vfxTrailPopupMarcheSimPieces.clear();
    this->vfxTrailPopupMarcheSimParamSignature.clear();
    this->vfxTrailPopupMarcheSimDistanceAcc = 0.0f;
    this->vfxTrailPopupMarcheDistAlongPathPx = 0.0f;
}

void EditorMapVfxScene::closeVfxTrailConePopup(void)
{
    this->vfxTrailConePopupVisible = false;
    this->vfxTrailConePopupParentInstanceIndex = -1;
    this->vfxTrailConePopupLength = 96.0f;
    this->vfxTrailConePopupOffsetX = 0.0f;
    this->vfxTrailConePopupOffsetY = 0.0f;
    this->vfxTrailConePopupDirectionOffsetDeg = 0.0f;
    this->vfxTrailConePopupHalfAngleDeg = 28.0f;
    this->vfxTrailConePopupSpawnCount = 1;
    this->vfxTrailConePopupCenterDragActive = false;
    this->vfxTrailConePopupTipDragActive = false;
    this->vfxTrailConePopupSideDragActive = false;
    this->vfxTrailConePopupLastLayout = VfxTrailConePopupLayout{};
}

void EditorMapVfxScene::openVfxTrailPopupForInstanceIndex(int vfxInstanceIndex)
{
    this->closeVfxRelativeTimingPopup();
    this->closeVfxDuplicateToPagesPopup();
    this->closeVfxTrailPopup();
    if (vfxInstanceIndex < 0 || vfxInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    const ShipVfxInstance& inst = this->currentShipVfxLayers()[static_cast<size_t>(vfxInstanceIndex)];
    this->vfxTrailPopupVisible = true;
    this->vfxTrailPopupPreviewZoom = (std::clamp)(GetCamera().getZoomFactor(), 0.4f, 1.0f);
    this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec = this->previewShip.getSpeedTilesPerSecond();
    if (!std::isfinite(this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec) ||
        this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec <= 0.0f)
    {
        this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec = 6.0f;
    }
    this->vfxTrailPopupParentInstanceIndex = vfxInstanceIndex;
    this->vfxTrailPopupEveryNTilesInput = std::to_string(inst.motionTrailEveryNTiles);
    this->vfxTrailPopupLifetimeTilesInput = std::to_string(inst.motionTrailLifetimeTiles);
    this->vfxTrailPopupLateralJitterInput =
        std::to_string(static_cast<int>(std::lround(inst.motionTrailLateralJitterRadius)));
    this->vfxTrailPopupRotationPctInput = std::to_string(inst.motionTrailRotationRandomPercent);
    this->vfxTrailPopupIdleRingPieceCountInput = std::to_string(inst.motionTrailIdleRingPieceCount);
    this->vfxTrailPopupIdlePeriodMsInput = std::to_string(inst.motionTrailIdleSpawnPeriodMs);
    this->vfxTrailPopupIdleRadiusInput =
        std::to_string(static_cast<int>(std::lround(inst.motionTrailIdleRingRadius)));
    this->vfxTrailPopupIdleRingRotationPctInput =
        std::to_string(inst.motionTrailIdleRingRotationRandomPercent);
    this->vfxTrailPopupIdleRingPosJitterInput =
        std::to_string(static_cast<int>(std::lround(inst.motionTrailIdleRingPositionJitterRadius)));
    this->vfxTrailPopupStrictTilePlacement = inst.motionTrailStrictTilePlacement;
    this->vfxTrailPopupIdleRingWhenStationary = inst.motionTrailIdleRingWhenStationary;
    this->vfxTrailPopupEveryNTilesFocused = true;
    this->vfxTrailPopupLifetimeTilesFocused = false;
    this->vfxTrailPopupLateralJitterFocused = false;
    this->vfxTrailPopupRotationPctFocused = false;
    this->shipVfxLayerPagePickerOpen = false;
    VfxTrailPopupLayout openLay{};
    if (this->computeVfxTrailPopupLayout(&openLay))
    {
        this->vfxTrailPopupLastLayout = openLay;
    }
    this->statusMessage =
        "SPAWN / trainee : SPAWN (EMPLACEMENT DU LAYER) ou SPAWN (OFFSET CONE), rotation %, puis VALIDER.";
}

void EditorMapVfxScene::openVfxTrailConePopupForInstanceIndex(int vfxInstanceIndex)
{
    if (vfxInstanceIndex < 0 || vfxInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    const ShipVfxInstance& inst = this->currentShipVfxLayers()[static_cast<size_t>(vfxInstanceIndex)];
    this->vfxTrailConePopupVisible = true;
    this->vfxTrailConePopupParentInstanceIndex = vfxInstanceIndex;
    this->vfxTrailConePopupLength = std::clamp(inst.motionTrailLateralJitterRadius, 1.0f, 2048.0f);
    this->vfxTrailConePopupOffsetX = std::clamp(inst.motionTrailConeOffsetX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    this->vfxTrailConePopupOffsetY = std::clamp(inst.motionTrailConeOffsetY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    this->vfxTrailConePopupDirectionOffsetDeg = std::clamp(inst.motionTrailConeDirectionOffsetDeg, -179.0f, 179.0f);
    this->vfxTrailConePopupHalfAngleDeg = std::clamp(inst.motionTrailConeHalfAngleDeg, 2.0f, 85.0f);
    this->vfxTrailConePopupSpawnCount = std::clamp(inst.motionTrailConeSpawnCount, 1, 32);
    this->vfxTrailConePopupCenterDragActive = false;
    this->vfxTrailConePopupTipDragActive = false;
    this->vfxTrailConePopupSideDragActive = false;
    VfxTrailConePopupLayout lay{};
    if (this->computeVfxTrailConePopupLayout(&lay))
    {
        this->vfxTrailConePopupLastLayout = lay;
    }
    this->statusMessage =
        "Cone arriere : glisse VERT (offset X/Y), ORANGE (direction + longueur), CYAN (largeur), puis APPLIQUER.";
}

bool EditorMapVfxScene::computeVfxTrailConePopupLayout(VfxTrailConePopupLayout* out) const
{
    if (out == nullptr || !this->vfxTrailConePopupVisible)
    {
        return false;
    }

    const SDL_FRect mapRect = GetCurrentMap().rect;
    out->dimFullMap = mapRect;

    constexpr float kPopupMarginMap = 16.0f;
    const float popupW = (std::clamp)(mapRect.w - (kPopupMarginMap * 2.0f), 680.0f, 980.0f);
    const float popupH = (std::clamp)(mapRect.h - (kPopupMarginMap * 2.0f), 430.0f, 760.0f);
    out->popup = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};

    constexpr float kPopupInnerPad = 14.0f;
    constexpr float kPreviewTop = 76.0f;
    constexpr float kPreviewBottomPad = 74.0f;
    out->previewRect = SDL_FRect{
        out->popup.x + kPopupInnerPad,
        out->popup.y + kPreviewTop,
        (std::max)(out->popup.w - (kPopupInnerPad * 2.0f), 120.0f),
        (std::max)(out->popup.h - kPreviewTop - kPreviewBottomPad, 140.0f)};

    constexpr float kBtnW = 120.0f;
    constexpr float kBtnH = 30.0f;
    constexpr float kBtnGap = 12.0f;
    const float btnY = out->popup.y + out->popup.h - kBtnH - 14.0f;
    out->cancelBtn = SDL_FRect{out->popup.x + kPopupInnerPad, btnY, kBtnW, kBtnH};
    out->resetBtn =
        SDL_FRect{out->cancelBtn.x + out->cancelBtn.w + kBtnGap, btnY, kBtnW, kBtnH};
    out->validateBtn =
        SDL_FRect{out->popup.x + out->popup.w - kPopupInnerPad - kBtnW, btnY, kBtnW, kBtnH};

    constexpr float kSpawnBtnW = 24.0f;
    constexpr float kSpawnBtnH = 22.0f;
    const float spawnY = out->popup.y + 31.0f;
    out->spawnCountPlusBtn =
        SDL_FRect{out->popup.x + out->popup.w - 14.0f - kSpawnBtnW, spawnY, kSpawnBtnW, kSpawnBtnH};
    out->spawnCountMinusBtn =
        SDL_FRect{out->spawnCountPlusBtn.x - 6.0f - kSpawnBtnW, spawnY, kSpawnBtnW, kSpawnBtnH};

    const EditorMapVfxTrailConePreviewGeom geom = editorMapVfxBuildTrailConePreviewGeom(
        out->previewRect,
        this->vfxTrailConePopupLength,
        this->vfxTrailConePopupOffsetX,
        this->vfxTrailConePopupOffsetY,
        this->vfxTrailConePopupDirectionOffsetDeg,
        this->vfxTrailConePopupHalfAngleDeg);
    constexpr float kCenterHandleHalf = 8.0f;
    constexpr float kTipHandleHalf = 10.0f;
    constexpr float kSideHandleHalf = 8.0f;
    out->centerHandleRect = SDL_FRect{
        geom.coneOriginX - kCenterHandleHalf,
        geom.coneOriginY - kCenterHandleHalf,
        kCenterHandleHalf * 2.0f,
        kCenterHandleHalf * 2.0f};
    out->tipHandleRect = SDL_FRect{
        geom.tipX - kTipHandleHalf,
        geom.tipY - kTipHandleHalf,
        kTipHandleHalf * 2.0f,
        kTipHandleHalf * 2.0f};
    out->sideHandleRect = SDL_FRect{
        geom.sideX - kSideHandleHalf,
        geom.sideY - kSideHandleHalf,
        kSideHandleHalf * 2.0f,
        kSideHandleHalf * 2.0f};

    // Evite les poignees collees l'une sur l'autre quand le cone est court/serre.
    editorMapVfxClampRectInside(&out->centerHandleRect, out->previewRect, 2.0f);
    editorMapVfxClampRectInside(&out->tipHandleRect, out->previewRect, 2.0f);
    editorMapVfxClampRectInside(&out->sideHandleRect, out->previewRect, 2.0f);
    constexpr float kCenterTipMinDist = 34.0f;
    constexpr float kCenterSideMinDist = 30.0f;
    constexpr float kTipSideMinDist = 28.0f;
    for (int i = 0; i < 3; ++i)
    {
        editorMapVfxPushHandleAwayFrom(
            &out->tipHandleRect, out->centerHandleRect, kCenterTipMinDist, out->previewRect);
        editorMapVfxPushHandleAwayFrom(
            &out->sideHandleRect, out->centerHandleRect, kCenterSideMinDist, out->previewRect);
        editorMapVfxPushHandleAwayFrom(
            &out->sideHandleRect, out->tipHandleRect, kTipSideMinDist, out->previewRect);
    }

    return true;
}

void EditorMapVfxScene::drawVfxTrailConePopup(void) const
{
    if (!this->vfxTrailConePopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }
    VfxTrailConePopupLayout lay{};
    if (!this->computeVfxTrailConePopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->vfxTrailConePopupLastLayout = lay;

    const EditorMapVfxTrailConePreviewGeom geom = editorMapVfxBuildTrailConePreviewGeom(
        lay.previewRect,
        this->vfxTrailConePopupLength,
        this->vfxTrailConePopupOffsetX,
        this->vfxTrailConePopupOffsetY,
        this->vfxTrailConePopupDirectionOffsetDeg,
        this->vfxTrailConePopupHalfAngleDeg);
    const float halfAngleDeg =
        (std::clamp)(this->vfxTrailConePopupHalfAngleDeg, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
    const float halfAngleRad = halfAngleDeg * (3.14159265359f / 180.0f);
    const float tipHalfWidthUnits = std::tan(halfAngleRad) *
                                    (std::clamp)(this->vfxTrailConePopupLength,
                                                 kTrailConePopupLengthMin,
                                                 kTrailConePopupLengthMax);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 150});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{22, 30, 40, 245});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{145, 168, 194, 245});
    rc2d_graphics_rectangle("line", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{16, 22, 30, 250});
    rc2d_graphics_rectangle("fill", &lay.previewRect);
    rc2d_graphics_setColor(RC2D_Color{100, 122, 148, 220});
    rc2d_graphics_rectangle("line", &lay.previewRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    RC2D_Text title = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "SPAWN (OFFSET CONE) : placement precis");
    title.color = kHudTextColor;
    rc2d_graphics_setTextColor(&title);
    rc2d_graphics_drawText(&title, lay.popup.x + 14.0f, lay.popup.y + 12.0f);
    rc2d_graphics_destroyText(&title);

    char line1[220] = {};
    SDL_snprintf(
        line1,
        sizeof(line1),
        "Longueur %.0f  |  Direction %.1f deg  |  Ouverture %.1f deg  |  Spawn x%d",
        static_cast<double>((std::clamp)(this->vfxTrailConePopupLength, kTrailConePopupLengthMin, kTrailConePopupLengthMax)),
        static_cast<double>((std::clamp)(this->vfxTrailConePopupDirectionOffsetDeg, -179.0f, 179.0f)),
        static_cast<double>(halfAngleDeg),
        std::clamp(this->vfxTrailConePopupSpawnCount, 1, 32));
    RC2D_Text info1 = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), line1);
    info1.color = RC2D_Color{200, 210, 224, 240};
    rc2d_graphics_setTextColor(&info1);
    rc2d_graphics_drawText(&info1, lay.popup.x + 14.0f, lay.popup.y + 34.0f);
    rc2d_graphics_destroyText(&info1);

    char line2[220] = {};
    SDL_snprintf(
        line2,
        sizeof(line2),
        "Offset cone X %.0f | Y %.0f  |  VERT=offset XY, ORANGE=dir+long, CYAN=largeur (demi-largeur: %.0f)",
        static_cast<double>((std::clamp)(this->vfxTrailConePopupOffsetX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax)),
        static_cast<double>((std::clamp)(this->vfxTrailConePopupOffsetY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax)),
        static_cast<double>(tipHalfWidthUnits));
    RC2D_Text info2 = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), line2);
    info2.color = RC2D_Color{184, 196, 212, 236};
    rc2d_graphics_setTextColor(&info2);
    rc2d_graphics_drawText(&info2, lay.popup.x + 14.0f, lay.popup.y + 52.0f);
    rc2d_graphics_destroyText(&info2);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    // Repere "arriere" de reference (0 deg).
    const float rearX = geom.coneOriginX + std::cos(kTrailConePopupRearAxisRad) * geom.maxLengthPx;
    const float rearY = geom.coneOriginY + std::sin(kTrailConePopupRearAxisRad) * geom.maxLengthPx;
    rc2d_graphics_setColor(RC2D_Color{110, 126, 148, 188});
    rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, rearX, rearY);

    // Leger remplissage du cone via des rayons anchor -> base.
    for (int i = 0; i < 10; ++i)
    {
        const float t = static_cast<float>(i) / 9.0f;
        const float bx = geom.leftX + ((geom.rightX - geom.leftX) * t);
        const float by = geom.leftY + ((geom.rightY - geom.leftY) * t);
        rc2d_graphics_setColor(RC2D_Color{255, 163, 74, 56});
        rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, bx, by);
    }

    rc2d_graphics_setColor(RC2D_Color{255, 170, 72, 230});
    rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, geom.leftX, geom.leftY);
    rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, geom.rightX, geom.rightY);
    rc2d_graphics_setColor(RC2D_Color{255, 144, 54, 245});
    rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, geom.tipX, geom.tipY);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    // Repere center du layer.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{220, 230, 242, 220});
    rc2d_graphics_line(geom.layerAnchorX - 6.0f, geom.layerAnchorY, geom.layerAnchorX + 6.0f, geom.layerAnchorY);
    rc2d_graphics_line(geom.layerAnchorX, geom.layerAnchorY - 6.0f, geom.layerAnchorX, geom.layerAnchorY + 6.0f);
    // Relie layer -> origine cone.
    rc2d_graphics_setColor(RC2D_Color{130, 220, 150, 210});
    rc2d_graphics_line(geom.layerAnchorX, geom.layerAnchorY, geom.coneOriginX, geom.coneOriginY);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        SDL_Rect clip{};
        clip.x = static_cast<int>(std::floor(lay.previewRect.x));
        clip.y = static_cast<int>(std::floor(lay.previewRect.y));
        clip.w = (std::max)(static_cast<int>(std::ceil(lay.previewRect.x + lay.previewRect.w)) - clip.x, 1);
        clip.h = (std::max)(static_cast<int>(std::ceil(lay.previewRect.y + lay.previewRect.h)) - clip.y, 1);
        SDL_SetRenderClipRect(renderer, &clip);
    }
    if (this->previewShipLoaded)
    {
        float shipW = 96.0f;
        float sw = 0.0f;
        float sh = 0.0f;
        if (this->previewShip.getCurrentSpriteSizePixels(&sw, &sh))
        {
            (void)sh;
            const float maxPreviewShipW = (std::max)((std::min)(lay.previewRect.w, lay.previewRect.h) * 0.36f, 36.0f);
            shipW = (std::clamp)(sw * this->previewShip.getDrawScale(), 36.0f, maxPreviewShipW);
        }
        this->previewShip.drawEditorPreviewAt(geom.layerAnchorX, geom.layerAnchorY, shipW);
    }
    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }

    // Si une poignee est decalee pour rester cliquable, relie son point reel au carre.
    const float centerHandleCx = lay.centerHandleRect.x + (lay.centerHandleRect.w * 0.5f);
    const float centerHandleCy = lay.centerHandleRect.y + (lay.centerHandleRect.h * 0.5f);
    const float tipHandleCx = lay.tipHandleRect.x + (lay.tipHandleRect.w * 0.5f);
    const float tipHandleCy = lay.tipHandleRect.y + (lay.tipHandleRect.h * 0.5f);
    const float sideHandleCx = lay.sideHandleRect.x + (lay.sideHandleRect.w * 0.5f);
    const float sideHandleCy = lay.sideHandleRect.y + (lay.sideHandleRect.h * 0.5f);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    if (editorMapVfxDistance2D(centerHandleCx, centerHandleCy, geom.coneOriginX, geom.coneOriginY) > 1.0f)
    {
        rc2d_graphics_setColor(RC2D_Color{130, 220, 150, 210});
        rc2d_graphics_line(geom.coneOriginX, geom.coneOriginY, centerHandleCx, centerHandleCy);
    }
    if (editorMapVfxDistance2D(tipHandleCx, tipHandleCy, geom.tipX, geom.tipY) > 1.0f)
    {
        rc2d_graphics_setColor(RC2D_Color{255, 180, 120, 210});
        rc2d_graphics_line(geom.tipX, geom.tipY, tipHandleCx, tipHandleCy);
    }
    if (editorMapVfxDistance2D(sideHandleCx, sideHandleCy, geom.sideX, geom.sideY) > 1.0f)
    {
        rc2d_graphics_setColor(RC2D_Color{160, 235, 245, 210});
        rc2d_graphics_line(geom.sideX, geom.sideY, sideHandleCx, sideHandleCy);
    }
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    auto drawHandle = [](const SDL_FRect& r, const RC2D_Color& fill, const RC2D_Color& border) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(fill);
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(border);
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    };
    drawHandle(
        lay.centerHandleRect,
        this->vfxTrailConePopupCenterDragActive ? RC2D_Color{92, 210, 120, 250} : RC2D_Color{72, 172, 96, 238},
        RC2D_Color{190, 250, 200, 250});
    drawHandle(
        lay.tipHandleRect,
        this->vfxTrailConePopupTipDragActive ? RC2D_Color{255, 146, 52, 250} : RC2D_Color{220, 118, 38, 238},
        RC2D_Color{255, 210, 170, 250});
    drawHandle(
        lay.sideHandleRect,
        this->vfxTrailConePopupSideDragActive ? RC2D_Color{64, 196, 215, 250} : RC2D_Color{48, 164, 186, 236},
        RC2D_Color{180, 245, 255, 250});

    auto drawBtn = [this](const SDL_FRect& r, const char* label) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{56, 72, 92, 230});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(RC2D_Color{150, 170, 190, 235});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), label);
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&t, &tw, &th);
        rc2d_graphics_drawText(
            &t,
            r.x + ((r.w - static_cast<float>(tw)) * 0.5f),
            r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&t);
    };
    drawBtn(lay.spawnCountMinusBtn, "-");
    drawBtn(lay.spawnCountPlusBtn, "+");
    drawBtn(lay.cancelBtn, "ANNULER");
    drawBtn(lay.resetBtn, "RESET");
    drawBtn(lay.validateBtn, "APPLIQUER");
}

bool EditorMapVfxScene::handleVfxTrailConePopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->vfxTrailConePopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    VfxTrailConePopupLayout lay{};
    if (!this->computeVfxTrailConePopupLayout(&lay))
    {
        return true;
    }
    this->vfxTrailConePopupLastLayout = lay;

    auto applyConeToInstance = [this]() {
        const float applyLen =
            (std::clamp)(this->vfxTrailConePopupLength, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
        const float applyOffX =
            (std::clamp)(this->vfxTrailConePopupOffsetX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        const float applyOffY =
            (std::clamp)(this->vfxTrailConePopupOffsetY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        const float applyDir =
            (std::clamp)(editorMapVfxNormalizeSignedAngleDeg(this->vfxTrailConePopupDirectionOffsetDeg), -179.0f, 179.0f);
        const float applyHalf =
            (std::clamp)(this->vfxTrailConePopupHalfAngleDeg, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
        const int applyCount = std::clamp(this->vfxTrailConePopupSpawnCount, 1, 32);
        this->vfxTrailConePopupLength = applyLen;
        this->vfxTrailConePopupOffsetX = applyOffX;
        this->vfxTrailConePopupOffsetY = applyOffY;
        this->vfxTrailConePopupDirectionOffsetDeg = applyDir;
        this->vfxTrailConePopupHalfAngleDeg = applyHalf;
        this->vfxTrailConePopupSpawnCount = applyCount;

        if (this->vfxTrailConePopupParentInstanceIndex >= 0 &&
            this->vfxTrailConePopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& target =
                this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailConePopupParentInstanceIndex)];
            target.motionTrailStrictTilePlacement = false;
            target.motionTrailLateralJitterRadius = applyLen;
            target.motionTrailConeOffsetX = applyOffX;
            target.motionTrailConeOffsetY = applyOffY;
            target.motionTrailConeDirectionOffsetDeg = applyDir;
            target.motionTrailConeHalfAngleDeg = applyHalf;
            target.motionTrailConeSpawnCount = applyCount;
            target.motionTrailDistanceAcc = 0.0f;
            this->vfxTrailPopupStrictTilePlacement = false;
            this->vfxTrailPopupLateralJitterInput =
                std::to_string(static_cast<int>(std::lround(target.motionTrailLateralJitterRadius)));
            this->markShipVfxDirty();
        }
    };

    if (this->pointInRect(x, y, lay.spawnCountMinusBtn))
    {
        this->vfxTrailConePopupSpawnCount = std::clamp(this->vfxTrailConePopupSpawnCount - 1, 1, 32);
        return true;
    }
    if (this->pointInRect(x, y, lay.spawnCountPlusBtn))
    {
        this->vfxTrailConePopupSpawnCount = std::clamp(this->vfxTrailConePopupSpawnCount + 1, 1, 32);
        return true;
    }
    if (this->pointInRect(x, y, lay.centerHandleRect))
    {
        this->vfxTrailConePopupCenterDragActive = true;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = false;
        this->updateVfxTrailConePopupDragFromMouse();
        return true;
    }
    if (this->pointInRect(x, y, lay.tipHandleRect))
    {
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = true;
        this->vfxTrailConePopupSideDragActive = false;
        this->updateVfxTrailConePopupDragFromMouse();
        return true;
    }
    if (this->pointInRect(x, y, lay.sideHandleRect))
    {
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = true;
        this->updateVfxTrailConePopupDragFromMouse();
        return true;
    }

    if (this->pointInRect(x, y, lay.previewRect))
    {
        const EditorMapVfxTrailConePreviewGeom geom = editorMapVfxBuildTrailConePreviewGeom(
            lay.previewRect,
            this->vfxTrailConePopupLength,
            this->vfxTrailConePopupOffsetX,
            this->vfxTrailConePopupOffsetY,
            this->vfxTrailConePopupDirectionOffsetDeg,
            this->vfxTrailConePopupHalfAngleDeg);
        const float centerDx = x - geom.coneOriginX;
        const float centerDy = y - geom.coneOriginY;
        const float tipDx = x - geom.tipX;
        const float tipDy = y - geom.tipY;
        const float sideDx = x - geom.sideX;
        const float sideDy = y - geom.sideY;
        const float centerDistSq = (centerDx * centerDx) + (centerDy * centerDy);
        const float tipDistSq = (tipDx * tipDx) + (tipDy * tipDy);
        const float sideDistSq = (sideDx * sideDx) + (sideDy * sideDy);
        if (centerDistSq <= tipDistSq && centerDistSq <= sideDistSq)
        {
            this->vfxTrailConePopupCenterDragActive = true;
            this->vfxTrailConePopupTipDragActive = false;
            this->vfxTrailConePopupSideDragActive = false;
        }
        else if (tipDistSq <= sideDistSq)
        {
            this->vfxTrailConePopupCenterDragActive = false;
            this->vfxTrailConePopupTipDragActive = true;
            this->vfxTrailConePopupSideDragActive = false;
        }
        else
        {
            this->vfxTrailConePopupCenterDragActive = false;
            this->vfxTrailConePopupTipDragActive = false;
            this->vfxTrailConePopupSideDragActive = true;
        }
        this->updateVfxTrailConePopupDragFromMouse();
        return true;
    }

    if (!this->pointInRect(x, y, lay.popup))
    {
        if (this->pointInRect(x, y, lay.dimFullMap))
        {
            this->closeVfxTrailConePopup();
            this->statusMessage = "Cone arriere : annule.";
        }
        return true;
    }

    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeVfxTrailConePopup();
        this->statusMessage = "Cone arriere : annule.";
        return true;
    }

    if (this->pointInRect(x, y, lay.resetBtn))
    {
        this->vfxTrailConePopupLength = 120.0f;
        this->vfxTrailConePopupOffsetX = 0.0f;
        this->vfxTrailConePopupOffsetY = 0.0f;
        this->vfxTrailConePopupDirectionOffsetDeg = 0.0f;
        this->vfxTrailConePopupHalfAngleDeg = 28.0f;
        this->vfxTrailConePopupSpawnCount = 1;
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = false;
        this->statusMessage = "Cone arriere : reset (L=120, OffX/Y=0, Dir=0, Ouv=28, Spawn x1).";
        return true;
    }

    if (this->pointInRect(x, y, lay.validateBtn))
    {
        applyConeToInstance();
        const float applyLen = this->vfxTrailConePopupLength;
        const float applyOffX = this->vfxTrailConePopupOffsetX;
        const float applyOffY = this->vfxTrailConePopupOffsetY;
        const float applyDir = this->vfxTrailConePopupDirectionOffsetDeg;
        const float applyHalf = this->vfxTrailConePopupHalfAngleDeg;
        const int applyCount = this->vfxTrailConePopupSpawnCount;
        this->closeVfxTrailConePopup();
        char msg[220] = {};
        SDL_snprintf(msg,
                     sizeof(msg),
                     "Cone applique : L %.0f, OffX %.0f, OffY %.0f, Dir %.1f deg, Ouv %.1f deg, Spawn x%d.",
                     static_cast<double>(applyLen),
                     static_cast<double>(applyOffX),
                     static_cast<double>(applyOffY),
                     static_cast<double>(applyDir),
                     static_cast<double>(applyHalf),
                     applyCount);
        this->statusMessage = msg;
        return true;
    }

    this->vfxTrailConePopupCenterDragActive = false;
    this->vfxTrailConePopupTipDragActive = false;
    this->vfxTrailConePopupSideDragActive = false;
    return true;
}

bool EditorMapVfxScene::handleVfxTrailConePopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;
    (void)keycode;
    (void)mod;
    if (!this->vfxTrailConePopupVisible)
    {
        return false;
    }

    auto applyConeToInstance = [this]() {
        const float applyLen =
            (std::clamp)(this->vfxTrailConePopupLength, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
        const float applyOffX =
            (std::clamp)(this->vfxTrailConePopupOffsetX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        const float applyOffY =
            (std::clamp)(this->vfxTrailConePopupOffsetY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        const float applyDir =
            (std::clamp)(editorMapVfxNormalizeSignedAngleDeg(this->vfxTrailConePopupDirectionOffsetDeg), -179.0f, 179.0f);
        const float applyHalf =
            (std::clamp)(this->vfxTrailConePopupHalfAngleDeg, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
        const int applyCount = std::clamp(this->vfxTrailConePopupSpawnCount, 1, 32);
        this->vfxTrailConePopupLength = applyLen;
        this->vfxTrailConePopupOffsetX = applyOffX;
        this->vfxTrailConePopupOffsetY = applyOffY;
        this->vfxTrailConePopupDirectionOffsetDeg = applyDir;
        this->vfxTrailConePopupHalfAngleDeg = applyHalf;
        this->vfxTrailConePopupSpawnCount = applyCount;

        if (this->vfxTrailConePopupParentInstanceIndex >= 0 &&
            this->vfxTrailConePopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& target =
                this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailConePopupParentInstanceIndex)];
            target.motionTrailStrictTilePlacement = false;
            target.motionTrailLateralJitterRadius = applyLen;
            target.motionTrailConeOffsetX = applyOffX;
            target.motionTrailConeOffsetY = applyOffY;
            target.motionTrailConeDirectionOffsetDeg = applyDir;
            target.motionTrailConeHalfAngleDeg = applyHalf;
            target.motionTrailConeSpawnCount = applyCount;
            target.motionTrailDistanceAcc = 0.0f;
            this->vfxTrailPopupStrictTilePlacement = false;
            this->vfxTrailPopupLateralJitterInput =
                std::to_string(static_cast<int>(std::lround(target.motionTrailLateralJitterRadius)));
            this->markShipVfxDirty();
        }
    };

    if (!isrepeat && scancode == SDL_SCANCODE_ESCAPE)
    {
        this->closeVfxTrailConePopup();
        this->statusMessage = "Cone arriere : annule.";
        return true;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        applyConeToInstance();
        const float applyLen = this->vfxTrailConePopupLength;
        const float applyOffX = this->vfxTrailConePopupOffsetX;
        const float applyOffY = this->vfxTrailConePopupOffsetY;
        const float applyDir = this->vfxTrailConePopupDirectionOffsetDeg;
        const float applyHalf = this->vfxTrailConePopupHalfAngleDeg;
        const int applyCount = this->vfxTrailConePopupSpawnCount;
        this->closeVfxTrailConePopup();
        char msg[220] = {};
        SDL_snprintf(msg,
                     sizeof(msg),
                     "Cone applique : L %.0f, OffX %.0f, OffY %.0f, Dir %.1f deg, Ouv %.1f deg, Spawn x%d.",
                     static_cast<double>(applyLen),
                     static_cast<double>(applyOffX),
                     static_cast<double>(applyOffY),
                     static_cast<double>(applyDir),
                     static_cast<double>(applyHalf),
                     applyCount);
        this->statusMessage = msg;
        return true;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_R)
    {
        this->vfxTrailConePopupLength = 120.0f;
        this->vfxTrailConePopupOffsetX = 0.0f;
        this->vfxTrailConePopupOffsetY = 0.0f;
        this->vfxTrailConePopupDirectionOffsetDeg = 0.0f;
        this->vfxTrailConePopupHalfAngleDeg = 28.0f;
        this->vfxTrailConePopupSpawnCount = 1;
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = false;
        this->statusMessage = "Cone arriere : reset (R).";
        return true;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_LEFT)
    {
        this->vfxTrailConePopupDirectionOffsetDeg = (std::clamp)(
            this->vfxTrailConePopupDirectionOffsetDeg - 1.0f, -179.0f, 179.0f);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_RIGHT)
    {
        this->vfxTrailConePopupDirectionOffsetDeg = (std::clamp)(
            this->vfxTrailConePopupDirectionOffsetDeg + 1.0f, -179.0f, 179.0f);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_UP)
    {
        this->vfxTrailConePopupHalfAngleDeg = (std::clamp)(
            this->vfxTrailConePopupHalfAngleDeg + 1.0f, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_DOWN)
    {
        this->vfxTrailConePopupHalfAngleDeg = (std::clamp)(
            this->vfxTrailConePopupHalfAngleDeg - 1.0f, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
        return true;
    }
    if (!isrepeat && (scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS))
    {
        this->vfxTrailConePopupLength = (std::clamp)(
            this->vfxTrailConePopupLength + 8.0f, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
        return true;
    }
    if (!isrepeat && (scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS))
    {
        this->vfxTrailConePopupLength = (std::clamp)(
            this->vfxTrailConePopupLength - 8.0f, kTrailConePopupLengthMin, kTrailConePopupLengthMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_A)
    {
        this->vfxTrailConePopupOffsetX = (std::clamp)(
            this->vfxTrailConePopupOffsetX - 8.0f, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_D)
    {
        this->vfxTrailConePopupOffsetX = (std::clamp)(
            this->vfxTrailConePopupOffsetX + 8.0f, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_W)
    {
        this->vfxTrailConePopupOffsetY = (std::clamp)(
            this->vfxTrailConePopupOffsetY - 8.0f, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_S)
    {
        this->vfxTrailConePopupOffsetY = (std::clamp)(
            this->vfxTrailConePopupOffsetY + 8.0f, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_KP_8)
    {
        this->vfxTrailConePopupSpawnCount = std::clamp(this->vfxTrailConePopupSpawnCount + 1, 1, 32);
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_KP_2)
    {
        this->vfxTrailConePopupSpawnCount = std::clamp(this->vfxTrailConePopupSpawnCount - 1, 1, 32);
        return true;
    }

    return true;
}

void EditorMapVfxScene::updateVfxTrailConePopupDragFromMouse(void)
{
    if (!this->vfxTrailConePopupVisible)
    {
        return;
    }
    if (!this->vfxTrailConePopupCenterDragActive &&
        !this->vfxTrailConePopupTipDragActive &&
        !this->vfxTrailConePopupSideDragActive)
    {
        return;
    }
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = false;
        return;
    }

    VfxTrailConePopupLayout lay{};
    if (!this->computeVfxTrailConePopupLayout(&lay))
    {
        this->vfxTrailConePopupCenterDragActive = false;
        this->vfxTrailConePopupTipDragActive = false;
        this->vfxTrailConePopupSideDragActive = false;
        return;
    }
    this->vfxTrailConePopupLastLayout = lay;

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    const EditorMapVfxTrailConePreviewGeom geom = editorMapVfxBuildTrailConePreviewGeom(
        lay.previewRect,
        this->vfxTrailConePopupLength,
        this->vfxTrailConePopupOffsetX,
        this->vfxTrailConePopupOffsetY,
        this->vfxTrailConePopupDirectionOffsetDeg,
        this->vfxTrailConePopupHalfAngleDeg);
    const float vx = mouseX - geom.coneOriginX;
    const float vy = mouseY - geom.coneOriginY;
    const float vLen = std::sqrt((vx * vx) + (vy * vy));

    if (this->vfxTrailConePopupCenterDragActive)
    {
        const float offXPx = mouseX - geom.layerAnchorX;
        const float offYPx = mouseY - geom.layerAnchorY;
        this->vfxTrailConePopupOffsetX =
            editorMapVfxTrailConePreviewPxToOffsetUnits(offXPx, geom.maxOffsetPx);
        this->vfxTrailConePopupOffsetY =
            editorMapVfxTrailConePreviewPxToOffsetUnits(offYPx, geom.maxOffsetPx);
    }
    else if (this->vfxTrailConePopupTipDragActive)
    {
        if (vLen > 0.0001f)
        {
            const float ang = std::atan2(vy, vx);
            float offDeg = (ang - kTrailConePopupRearAxisRad) * (180.0f / 3.14159265359f);
            offDeg = editorMapVfxNormalizeSignedAngleDeg(offDeg);
            this->vfxTrailConePopupDirectionOffsetDeg = (std::clamp)(offDeg, -179.0f, 179.0f);
        }
        this->vfxTrailConePopupLength = editorMapVfxTrailConePreviewPxToLength(vLen, geom.maxLengthPx);
    }
    else if (this->vfxTrailConePopupSideDragActive && vLen > 0.0001f)
    {
        const float sideAng = std::atan2(vy, vx);
        float deltaDeg = (sideAng - geom.axisAngleRad) * (180.0f / 3.14159265359f);
        deltaDeg = editorMapVfxNormalizeSignedAngleDeg(deltaDeg);
        this->vfxTrailConePopupHalfAngleDeg =
            (std::clamp)(std::fabs(deltaDeg), kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
    }
}

bool EditorMapVfxScene::computeVfxTrailPopupLayout(VfxTrailPopupLayout* out) const
{
    if (out == nullptr || !this->vfxTrailPopupVisible)
    {
        return false;
    }
    const SDL_FRect mapRect = GetCurrentMap().rect;
    out->dimFullMap = mapRect;
    constexpr float kTrailPopupMargin = 14.0f;
    constexpr float kTrailPopupColGutter = 12.0f;
    constexpr float kTrailPopupLeftColPreferred = 608.0f;
    const float popupW = (std::min)(1300.0f, (std::max)(mapRect.w - 16.0f, 720.0f));
    const float popupH = 960.0f;
    out->popup = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};
    float leftColW = kTrailPopupLeftColPreferred;
    const float innerAvail = popupW - (kTrailPopupMargin * 2.0f) - kTrailPopupColGutter;
    if (innerAvail < leftColW + 180.0f)
    {
        leftColW = (std::max)(280.0f, innerAvail * 0.5f);
    }
    const float px = out->popup.x + kTrailPopupMargin;
    const float pw = leftColW;
    const float previewX = px + pw + kTrailPopupColGutter;
    const float previewW = (std::max)(out->popup.x + out->popup.w - kTrailPopupMargin - previewX, 48.0f);
    constexpr float kTrailModeBtnGap = 10.0f;
    const float trailModeBtnW = (pw - kTrailModeBtnGap) * 0.5f;

    constexpr float kInputH = 32.0f;
    constexpr float kTrailModeBtnH = 30.0f;
    constexpr float kIdleModeBtnH = 28.0f;
    constexpr float kLabelH = 17.0f;
    constexpr float kLabelToInputGap = 8.0f;
    constexpr float kAfterFieldGap = 14.0f;
    constexpr float kAfterModeRowGap = 14.0f;
    /** Espace vide au-dessus / au-dessous de chaque trait horizontal (separation visuelle). */
    constexpr float kTrailRulePadV = 20.0f;
    /** Bas approximatif du bloc titre + ligne instance (titre py+12, instance py+36). */
    constexpr float kTrailPopupHeaderBottom = 54.0f;

    const float py = out->popup.y;
    out->trailRuleAfterInstanceY = py + kTrailPopupHeaderBottom + kTrailRulePadV;
    float y = out->trailRuleAfterInstanceY + kTrailRulePadV;

    out->everyNTilesInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
    y = out->everyNTilesInputRect.y + kInputH + kAfterFieldGap;

    out->lifetimeTilesInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
    y = out->lifetimeTilesInputRect.y + kInputH + kAfterFieldGap;

    out->trailStrictTileBtn =
        SDL_FRect{px, y + kLabelH + kLabelToInputGap, trailModeBtnW, kTrailModeBtnH};
    out->trailLateralSpreadBtn = SDL_FRect{px + trailModeBtnW + kTrailModeBtnGap,
                                           y + kLabelH + kLabelToInputGap,
                                           trailModeBtnW,
                                           kTrailModeBtnH};
    y = out->trailStrictTileBtn.y + kTrailModeBtnH + kAfterModeRowGap;

    out->lateralSectionVisible = !this->vfxTrailPopupStrictTilePlacement;
    if (out->lateralSectionVisible)
    {
        out->lateralJitterInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
        y = out->lateralJitterInputRect.y + kInputH + kAfterFieldGap;
    }
    else
    {
        out->lateralJitterInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    out->rotationPctInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
    y = out->rotationPctInputRect.y + kInputH + kAfterFieldGap;

    out->trailRuleBeforeArretY = y + kTrailRulePadV;
    y = out->trailRuleBeforeArretY + kTrailRulePadV;

    out->idleRingOffBtn = SDL_FRect{px, y + kLabelH + kLabelToInputGap, trailModeBtnW, kIdleModeBtnH};
    out->idleRingOnBtn = SDL_FRect{
        px + trailModeBtnW + kTrailModeBtnGap, y + kLabelH + kLabelToInputGap, trailModeBtnW, kIdleModeBtnH};
    y = out->idleRingOffBtn.y + kIdleModeBtnH + kAfterModeRowGap;

    out->idleRingInputsVisible = this->vfxTrailPopupIdleRingWhenStationary;
    if (out->idleRingInputsVisible)
    {
        out->idleRingPieceCountInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
        y = out->idleRingPieceCountInputRect.y + kInputH + kAfterFieldGap;
        out->idleRingPeriodMsInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        out->idleRingRadiusInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
        y = out->idleRingRadiusInputRect.y + kInputH + kAfterFieldGap;
        out->idleRingRotationPctInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
        y = out->idleRingRotationPctInputRect.y + kInputH + kAfterFieldGap;
        out->idleRingPosJitterInputRect = SDL_FRect{px, y + kLabelH + kLabelToInputGap, pw, kInputH};
    }
    else
    {
        out->idleRingPieceCountInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        out->idleRingPeriodMsInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        out->idleRingRadiusInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        out->idleRingRotationPctInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        out->idleRingPosJitterInputRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float pyTop = out->popup.y;
    const float previewTop = pyTop + 50.0f;
    const float previewBottom = pyTop + popupH - 58.0f;
    const float previewTotalH = (std::max)(previewBottom - previewTop, 80.0f);
    /** Bandeau zoom au-dessus des deux previews (hors rectangles marche / arret). */
    constexpr float kPreviewZoomStripH = 28.0f;
    constexpr float kPreviewZoomBelowStripGap = 4.0f;
    constexpr float kPreviewZoomBtnW = 30.0f;
    constexpr float kPreviewZoomBtnH = 22.0f;
    const float innerPreviewH =
        (std::max)(previewTotalH - kPreviewZoomStripH - kPreviewZoomBelowStripGap, 72.0f);
    const float marcheH = innerPreviewH * 0.5f - 6.0f;
    const float arretGap = 12.0f;
    const float arretH = innerPreviewH - marcheH - arretGap;
    const float marcheTop = previewTop + kPreviewZoomStripH + kPreviewZoomBelowStripGap;
    out->previewMarcheRect = SDL_FRect{previewX, marcheTop, previewW, (std::max)(marcheH, 48.0f)};
    out->previewArretRect =
        SDL_FRect{previewX, marcheTop + marcheH + arretGap, previewW, (std::max)(arretH, 48.0f)};
    const float zmY = previewTop + (kPreviewZoomStripH - kPreviewZoomBtnH) * 0.5f;
    out->previewZoomPlusBtn =
        SDL_FRect{previewX + previewW - kPreviewZoomBtnW - 6.0f, zmY, kPreviewZoomBtnW, kPreviewZoomBtnH};
    out->previewZoomMinusBtn = SDL_FRect{out->previewZoomPlusBtn.x - kPreviewZoomBtnW - 6.0f,
                                         zmY,
                                         kPreviewZoomBtnW,
                                         kPreviewZoomBtnH};
    constexpr float kPreviewSpeedGroupGap = 70.0f;
    out->previewSpeedPlusBtn = SDL_FRect{out->previewZoomMinusBtn.x - kPreviewZoomBtnW - kPreviewSpeedGroupGap,
                                         zmY,
                                         kPreviewZoomBtnW,
                                         kPreviewZoomBtnH};
    out->previewSpeedMinusBtn = SDL_FRect{out->previewSpeedPlusBtn.x - kPreviewZoomBtnW - 6.0f,
                                          zmY,
                                          kPreviewZoomBtnW,
                                          kPreviewZoomBtnH};
    /** Boutons ocean a gauche du libelle vitesse : [ < > ][ spd .. ][ - + vitesse ] */
    constexpr float kSpdLabelReserveX = 82.0f;
    constexpr float kGapSpdTextToSpeedBtns = 8.0f;
    constexpr float kGapOceanPair = 6.0f;
    constexpr float kGapOceanToSpdText = 8.0f;
    const float spdTextRight = out->previewSpeedMinusBtn.x - kGapSpdTextToSpeedBtns;
    const float spdTextLeft = spdTextRight - kSpdLabelReserveX;
    float oceanNextX = spdTextLeft - kGapOceanToSpdText - kPreviewZoomBtnW;
    float oceanPrevX = oceanNextX - kGapOceanPair - kPreviewZoomBtnW;
    const float oceanRowMinX = previewX + 2.0f;
    if (oceanPrevX < oceanRowMinX)
    {
        const float push = oceanRowMinX - oceanPrevX;
        oceanPrevX += push;
        oceanNextX += push;
    }
    out->previewOceanNextBtn =
        SDL_FRect{oceanNextX, zmY, kPreviewZoomBtnW, kPreviewZoomBtnH};
    out->previewOceanPrevBtn =
        SDL_FRect{oceanPrevX, zmY, kPreviewZoomBtnW, kPreviewZoomBtnH};
    out->previewMarcheShipToggleBtn = SDL_FRect{
        out->previewMarcheRect.x + out->previewMarcheRect.w - 120.0f,
        out->previewMarcheRect.y + 4.0f,
        112.0f,
        24.0f};
    out->previewCrownShipToggleBtn = SDL_FRect{
        out->previewArretRect.x + out->previewArretRect.w - 120.0f,
        out->previewArretRect.y + 4.0f,
        112.0f,
        24.0f};

    const float btnY = out->popup.y + popupH - 52.0f;
    out->cancelBtn = SDL_FRect{out->popup.x + kTrailPopupMargin, btnY, 110.0f, 28.0f};
    out->clearBtn = SDL_FRect{out->popup.x + kTrailPopupMargin + 120.0f, btnY, 110.0f, 28.0f};
    out->validateBtn = SDL_FRect{out->popup.x + popupW - kTrailPopupMargin - 110.0f, btnY, 110.0f, 28.0f};
    return true;
}

void EditorMapVfxScene::openVfxRelativeTimingPopup(bool forShipRow, int vfxInstanceIndex)
{
    this->closeVfxTrailPopup();
    this->closeVfxDuplicateToPagesPopup();
    this->vfxRelativeTimingPopupIsShipRow = forShipRow;
    this->vfxRelativeTimingPopupTargetVfxIndex = forShipRow ? -1 : vfxInstanceIndex;
    this->vfxRelativeTimingPopupStep = 0;
    this->vfxRelativeTimingPopupAnchorInstanceId = 0U;
    this->vfxRelativeTimingPopupDelayMsInput.clear();
    this->vfxRelativeTimingPopupDelayMsFocused = false;
    this->vfxRelativeTimingPopupCandidateIndices.clear();
    this->shipVfxLayerPagePickerOpen = false;

    const auto& layers = this->currentShipVfxLayers();
    if (forShipRow)
    {
        for (int j = 0; j < static_cast<int>(layers.size()); ++j)
        {
            this->vfxRelativeTimingPopupCandidateIndices.push_back(j);
        }
        if (this->vfxRelativeTimingPopupCandidateIndices.empty())
        {
            this->statusMessage = "RELATIF (SHIP) : ajoute au moins une instance VFX sur cette page.";
            return;
        }
    }
    else
    {
        if (vfxInstanceIndex < 0 || vfxInstanceIndex >= static_cast<int>(layers.size()))
        {
            return;
        }
        const int sfxIdx = layers[static_cast<size_t>(vfxInstanceIndex)].importedSfxIndex;
        for (int j = 0; j < static_cast<int>(layers.size()); ++j)
        {
            if (j == vfxInstanceIndex)
            {
                continue;
            }
            if (layers[static_cast<size_t>(j)].importedSfxIndex == sfxIdx)
            {
                this->vfxRelativeTimingPopupCandidateIndices.push_back(j);
            }
        }
        if (this->vfxRelativeTimingPopupCandidateIndices.empty())
        {
            this->statusMessage = "RELATIF : aucune autre instance de cette animation sur la page.";
            return;
        }
    }

    this->vfxRelativeTimingPopupVisible = true;
    this->statusMessage = forShipRow ? "RELATIF : choisis le VFX de reference pour le navire."
                                     : "RELATIF : choisis l'instance de reference (meme animation).";
}

bool EditorMapVfxScene::computeVfxRelativePopupLayout(VfxRelativePopupLayout* out) const
{
    if (out == nullptr || !this->vfxRelativeTimingPopupVisible)
    {
        return false;
    }
    const SDL_FRect mapRect = GetCurrentMap().rect;
    out->dimFullMap = mapRect;
    const int totalCandidates = static_cast<int>(this->vfxRelativeTimingPopupCandidateIndices.size());
    out->candidateCount =
        (this->vfxRelativeTimingPopupStep == 0) ? (std::min)(totalCandidates, 20) : 0;

    const float popupW = 520.0f;
    float popupH = 200.0f;
    if (this->vfxRelativeTimingPopupStep == 0)
    {
        popupH = 88.0f + static_cast<float>((std::max)(out->candidateCount, 1)) * 26.0f + 46.0f;
    }
    else
    {
        popupH = 210.0f;
    }
    out->popup = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};

    float cy = out->popup.y + 44.0f;
    for (int i = 0; i < out->candidateCount; ++i)
    {
        out->candidateRows[i] = SDL_FRect{out->popup.x + 14.0f, cy, out->popup.w - 28.0f, 24.0f};
        cy += 26.0f;
    }

    const float btnY = out->popup.y + popupH - 36.0f;
    out->cancelBtn = SDL_FRect{out->popup.x + 14.0f, btnY, 110.0f, 28.0f};
    out->clearBtn = SDL_FRect{out->popup.x + 134.0f, btnY, 110.0f, 28.0f};
    out->validateBtn = SDL_FRect{out->popup.x + popupW - 124.0f, btnY, 110.0f, 28.0f};
    out->delayInputRect = SDL_FRect{out->popup.x + 14.0f, out->popup.y + 102.0f, popupW - 28.0f, 32.0f};
    return true;
}

bool EditorMapVfxScene::shouldSkipDrawImportedSfxForPilotMaxLifetime(const ImportedSfx& imported, float timeSeconds) const
{
    if (!this->previewShipPilotActive)
    {
        return false;
    }
    /** L'horloge pilotage carte ne doit pas couper les previews du popup trainee (rejets / couronne). */
    if (this->vfxTrailPopupVisible)
    {
        return false;
    }
    if (imported.animationTotalDurationInfinite || imported.animationTotalDurationMs <= 0)
    {
        return false;
    }
    const float elapsedSec = timeSeconds - this->pilotVfxMaxDurationClockAnchorSeconds;
    return elapsedSec >= static_cast<float>(imported.animationTotalDurationMs) * 0.001f;
}

float EditorMapVfxScene::computeVfxPreviewPhaseSecondsInCycle(
    const ShipVfxInstance& instance,
    float timeSeconds,
    const std::vector<ShipVfxInstance>& layerVec,
    const ImportedSfx& imported,
    int chainDepth) const
{
    const int frameCount = static_cast<int>(imported.frames.size());
    if (frameCount <= 0)
    {
        return 0.0f;
    }
    const float fps = (std::max)(imported.defaultFps, 1.0f);
    const float period = static_cast<float>(frameCount) / fps;
    if (period <= 0.0001f)
    {
        return 0.0f;
    }

    auto normalizePhase = [period](float p) -> float {
        float x = std::fmod(p, period);
        if (x < 0.f)
        {
            x += period;
        }
        return x;
    };

    auto computeRootElapsedSeconds = [timeSeconds](const ShipVfxInstance& inst) -> float {
        if (std::isfinite(inst.previewSpawnTimeSeconds) && inst.previewSpawnTimeSeconds >= 0.0f)
        {
            return (std::max)(0.0f, timeSeconds - inst.previewSpawnTimeSeconds);
        }
        return (std::max)(0.0f, timeSeconds);
    };

    if (instance.spawnAfterInstanceId == 0U || chainDepth > 32)
    {
        return normalizePhase(computeRootElapsedSeconds(instance));
    }

    const ShipVfxInstance* anchor = nullptr;
    for (const ShipVfxInstance& inst : layerVec)
    {
        if (inst.instanceId == instance.spawnAfterInstanceId)
        {
            anchor = &inst;
            break;
        }
    }
    if (anchor == nullptr || anchor->importedSfxIndex != instance.importedSfxIndex)
    {
        return normalizePhase(computeRootElapsedSeconds(instance));
    }
    if (anchor->importedSfxIndex < 0 || anchor->importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return normalizePhase(computeRootElapsedSeconds(instance));
    }

    const float anchorPhase = this->computeVfxPreviewPhaseSecondsInCycle(
        *anchor,
        timeSeconds,
        layerVec,
        imported,
        chainDepth + 1);
    float childPhase = anchorPhase - (static_cast<float>(instance.spawnAfterDelayMs) / 1000.f);
    while (childPhase < 0.f)
    {
        childPhase += period;
    }
    while (childPhase >= period)
    {
        childPhase -= period;
    }
    return childPhase;
}

int EditorMapVfxScene::computeVfxPreviewFrameIndex(
    const ShipVfxInstance& instance,
    float timeSeconds,
    const std::vector<ShipVfxInstance>& layerVec,
    const ImportedSfx& imported) const
{
    const int frameCount = static_cast<int>(imported.frames.size());
    if (frameCount <= 0)
    {
        return 0;
    }
    const float fps = (std::max)(imported.defaultFps, 1.0f);
    const float period = static_cast<float>(frameCount) / fps;
    if (period <= 0.0001f)
    {
        return 0;
    }

    const float phaseSec = this->computeVfxPreviewPhaseSecondsInCycle(instance, timeSeconds, layerVec, imported, 0);
    return static_cast<int>(std::floor(phaseSec * fps)) % frameCount;
}

int EditorMapVfxScene::computeTrailPieceFrameIndex(const ImportedSfx& imported, float elapsedSinceSpawnSec) const
{
    const int frameCount = static_cast<int>(imported.frames.size());
    if (frameCount <= 0)
    {
        return 0;
    }

    const float fps = (std::max)(imported.defaultFps, 1.0f);
    const float ageSec =
        (std::isfinite(elapsedSinceSpawnSec) && elapsedSinceSpawnSec > 0.0f) ? elapsedSinceSpawnSec : 0.0f;
    const float frameFloat = ageSec * fps;
    /** Rejets/couronne: la lecture depend du FPS spritesheet, la vie depend uniquement de timeRemainingSec. */
    const float frameCountF = static_cast<float>(frameCount);
    float frameInCycle = std::fmod(frameFloat, frameCountF);
    if (frameInCycle < 0.0f)
    {
        frameInCycle += frameCountF;
    }
    const int frameIndex = static_cast<int>(std::floor(frameInCycle));
    return (std::clamp)(frameIndex, 0, frameCount - 1);
}

float EditorMapVfxScene::trailPieceLifetimeDrawScaleMul(const ShipVfxTrailPiece& piece)
{
    if (piece.trailLifetimeInitialSec <= 1.0e-4f)
    {
        return 1.0f;
    }
    const float u = piece.timeRemainingSec / piece.trailLifetimeInitialSec;
    const float linear = (std::clamp)(u, 0.0f, 1.0f);
    const float eased = std::sqrt(linear);
    return (std::max)(kTrailPieceDrawScaleLifeMin, eased);
}

void EditorMapVfxScene::trailPieceAmbientDriftOffsets(
    const ShipVfxTrailPiece& piece,
    float ageSec,
    float lifeScaleMul,
    float* outAddX,
    float* outAddY)
{
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
    const float tx =
        t * kTrailPieceMotionNoiseRateX + piece.trailAmbientDriftPhase0 * 0.18f;
    const float ty =
        t * kTrailPieceMotionNoiseRateY + piece.trailAmbientDriftPhase1 * 0.21f + 19.7f;
    *outAddX = editorMapVfxSmoothNoise1D(seed ^ 0x1a2b3c4du, tx) * kTrailPieceMotionNoiseAmp * lifeS;
    *outAddY = editorMapVfxSmoothNoise1D(seed ^ 0x5d6e7f8au, ty) * kTrailPieceMotionNoiseAmp * lifeS;
}

void EditorMapVfxScene::trailPieceWakeInertiaOffsets(
    const ShipVfxTrailPiece& piece,
    float ageSec,
    float lifeScaleMul,
    float* outAddX,
    float* outAddY)
{
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
}

float EditorMapVfxScene::trailPieceSpinExtraDeg(const ShipVfxTrailPiece& piece, float ageSec)
{
    const float omega0 = piece.trailSpinOmega0;
    constexpr float k = kTrailPieceSpinDecayPerSec;
    if (std::fabs(omega0) < 1.0e-5f || k < 1.0e-5f)
    {
        return 0.0f;
    }
    const float wt = (std::isfinite(ageSec) && ageSec > 0.0f) ? ageSec : 0.0f;
    return (omega0 / k) * (1.0f - std::exp(-k * wt));
}

void EditorMapVfxScene::initTrailPieceMotionExtras(ShipVfxTrailPiece* piece, float moveDxTiles, float moveDyTiles)
{
    if (piece == nullptr)
    {
        return;
    }
    std::uniform_real_distribution<float> ph(0.0f, 6.28318530718f);
    piece->trailAmbientDriftPhase0 = ph(editorMapVfxTrailJitterRng());
    piece->trailAmbientDriftPhase1 = ph(editorMapVfxTrailJitterRng());
    std::uniform_int_distribution<uint32_t> u32(0u, 0xFFFFFFFFu);
    piece->trailNoiseSeed = u32(editorMapVfxTrailJitterRng());
    piece->trailWakeDirX = moveDxTiles;
    piece->trailWakeDirY = moveDyTiles;
    std::uniform_real_distribution<float> spinU(-kTrailPieceSpinOmegaMaxDegPerSec, kTrailPieceSpinOmegaMaxDegPerSec);
    piece->trailSpinOmega0 = spinU(editorMapVfxTrailJitterRng());
}

bool EditorMapVfxScene::shouldPreviewHideShipForRelativeTiming(float timeSeconds) const
{
    const size_t page = static_cast<size_t>(this->getShipVfxLayerPageKey());
    const uint32_t anchorId = this->shipSpawnAfterVfxInstanceId[page];
    if (anchorId == 0U)
    {
        return false;
    }
    const auto& layers = this->currentShipVfxLayers();
    const ShipVfxInstance* anchor = nullptr;
    for (const ShipVfxInstance& inst : layers)
    {
        if (inst.instanceId == anchorId)
        {
            anchor = &inst;
            break;
        }
    }
    if (anchor == nullptr || anchor->importedSfxIndex < 0 ||
        anchor->importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return false;
    }
    const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(anchor->importedSfxIndex)];
    if (imported.frames.empty())
    {
        return false;
    }
    const int frameCount = static_cast<int>(imported.frames.size());
    const float fps = (std::max)(imported.defaultFps, 1.0f);
    const float period = static_cast<float>(frameCount) / fps;
    if (period <= 0.0001f)
    {
        return false;
    }
    const float anchorPhase =
        this->computeVfxPreviewPhaseSecondsInCycle(*anchor, timeSeconds, layers, imported, 0);
    const float delaySec = static_cast<float>(this->shipSpawnAfterDelayMs[page]) / 1000.f;
    return anchorPhase < delaySec;
}

int EditorMapVfxScene::getActiveDirectionIndexForOverrides(void) const
{
    return std::clamp(this->previewDirectionIndex, 0, 3);
}

bool EditorMapVfxScene::hasSelectedVfxInstance(void) const
{
    return this->selectedVfxInstanceIndex >= 0 &&
        this->selectedVfxInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size());
}

EditorMapVfxScene::ShipVfxInstance* EditorMapVfxScene::getSelectedVfxInstance(void)
{
    if (!this->hasSelectedVfxInstance())
    {
        return nullptr;
    }
    return &this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
}

const EditorMapVfxScene::ShipVfxInstance* EditorMapVfxScene::getSelectedVfxInstance(void) const
{
    if (!this->hasSelectedVfxInstance())
    {
        return nullptr;
    }
    return &this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
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
    if (instance->behindShip && *drawOrder >= this->activeShipDrawOrder())
    {
        *drawOrder = this->activeShipDrawOrder() - 1;
    }
    if (!instance->behindShip && *drawOrder <= this->activeShipDrawOrder())
    {
        *drawOrder = this->activeShipDrawOrder() + 1;
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

EditorMapVfxScene::ShipVfxInstance EditorMapVfxScene::duplicateShipVfxInstanceFreshId(
    const EditorMapVfxScene::ShipVfxInstance& src)
{
    ShipVfxInstance duplicate = src;
    duplicate.instanceId = this->nextVfxInstanceId++;
    duplicate.previewSpawnTimeSeconds =
        (std::isfinite(src.previewSpawnTimeSeconds) && src.previewSpawnTimeSeconds >= 0.0f)
            ? src.previewSpawnTimeSeconds
            : this->resolveCurrentPagePreviewSpawnTimeSeconds();
    // Runtime-only accumulators reset; toutes les proprietes editor/gameplay restent copiees.
    duplicate.motionTrailIdleSpawnAccSec = 0.0f;
    duplicate.motionTrailIdleRingSalvoPiecesRemaining = 0;
    duplicate.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
    duplicate.motionTrailDistanceAcc = 0.0f;
    return duplicate;
}

float EditorMapVfxScene::resolveCurrentPagePreviewSpawnTimeSeconds(void) const
{
    for (const ShipVfxInstance& instance : this->currentShipVfxLayers())
    {
        if (std::isfinite(instance.previewSpawnTimeSeconds) && instance.previewSpawnTimeSeconds >= 0.0f)
        {
            return instance.previewSpawnTimeSeconds;
        }
    }
    return static_cast<float>(SDL_GetTicks()) * 0.001f;
}

void EditorMapVfxScene::remapTrailConeForShipDirectionChange(
    ShipVfxInstance& inst, int srcDirectionIndex4, int tgtDirectionIndex4)
{
    if (inst.motionTrailStrictTilePlacement)
    {
        return;
    }
    if (!(inst.motionTrailLateralJitterRadius > 0.0001f))
    {
        return;
    }
    const int sd = ((srcDirectionIndex4 % 4) + 4) % 4;
    const int td = ((tgtDirectionIndex4 % 4) + 4) % 4;
    if (sd == td)
    {
        return;
    }
    float sx0 = 0.0f;
    float sy0 = 0.0f;
    editorMapVfxDirectionIndexToTileStep(sd, &sx0, &sy0);
    const float h0 = std::atan2(sy0, sx0) * (180.0f / 3.14159265359f);
    float sx1 = 0.0f;
    float sy1 = 0.0f;
    editorMapVfxDirectionIndexToTileStep(td, &sx1, &sy1);
    const float h1 = std::atan2(sy1, sx1) * (180.0f / 3.14159265359f);
    const float deltaDeg = editorMapVfxNormalizeSignedAngleDeg(h1 - h0);
    constexpr float kDegToRad = 3.14159265359f / 180.0f;
    const float rad = deltaDeg * kDegToRad;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float ox = inst.motionTrailConeOffsetX;
    const float oy = inst.motionTrailConeOffsetY;
    inst.motionTrailConeOffsetX = ox * c - oy * s;
    inst.motionTrailConeOffsetY = ox * s + oy * c;
    inst.motionTrailConeOffsetX = (std::clamp)(inst.motionTrailConeOffsetX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    inst.motionTrailConeOffsetY = (std::clamp)(inst.motionTrailConeOffsetY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    inst.motionTrailConeDirectionOffsetDeg = (std::clamp)(
        editorMapVfxNormalizeSignedAngleDeg(inst.motionTrailConeDirectionOffsetDeg + deltaDeg), -179.0f, 179.0f);
}

bool EditorMapVfxScene::duplicateVfxInstanceAtIndexInCurrentPage(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return false;
    }
    const ShipVfxInstance& src = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
    if (src.locked)
    {
        this->statusMessage = "Layer verrouille: duplication refusee.";
        return false;
    }

    ShipVfxInstance dup = this->duplicateShipVfxInstanceFreshId(src);
    // Petit decal visuel uniquement pour distinguer immediatement la copie.
    dup.offsetX += 12.0f;
    dup.offsetY += 12.0f;
    for (DirectionOverride& o : dup.directionOverrides)
    {
        if (o.enabled)
        {
            o.offsetX += 12.0f;
            o.offsetY += 12.0f;
        }
    }
    if (dup.motionSpawnCaptured)
    {
        dup.motionSpawnOffsetX += 12.0f;
        dup.motionSpawnOffsetY += 12.0f;
    }

    this->currentShipVfxLayers().push_back(std::move(dup));
    this->setSelectedVfxInstanceIndex(static_cast<int>(this->currentShipVfxLayers().size()) - 1);
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    this->markShipVfxDirty();
    this->statusMessage = "Layer duplique (copie complete).";
    return true;
}

void EditorMapVfxScene::closeVfxDuplicateToPagesPopup(void)
{
    this->vfxDuplicateToPagesPopupVisible = false;
    this->vfxDuplicateToPagesPopupSourcePageKey = -1;
}

void EditorMapVfxScene::openVfxDuplicateToPagesPopupFromSelectedVfx(void)
{
    this->closeVfxRelativeTimingPopup();
    this->closeVfxTrailPopup();
    this->closeVfxDuplicateToPagesPopup();
    this->shipVfxLayerPagePickerOpen = false;

    const int srcPage = this->getShipVfxLayerPageKey();
    if (srcPage < 0 || srcPage >= kShipVfxLayerPageCount)
    {
        return;
    }
    if (this->shipVfxLayerPages[static_cast<size_t>(srcPage)].empty())
    {
        this->statusMessage = "La page courante ne contient aucun layer VFX a dupliquer.";
        return;
    }

    this->vfxDuplicateToPagesPopupSourceSnapshot = ShipVfxInstance{};
    this->vfxDuplicateToPagesPopupSourcePageKey = srcPage;
    for (size_t i = 0; i < this->vfxDuplicateToPagesPageSelected.size(); ++i)
    {
        this->vfxDuplicateToPagesPageSelected[i] = (static_cast<int>(i) == srcPage);
    }
    const int dupEff = this->shipVfxEffectiveLayerPageCount();
    for (int i = dupEff; i < kShipVfxLayerPageCount; ++i)
    {
        this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = false;
    }
    this->vfxDuplicateToPagesPopupVisible = true;
    this->statusMessage =
        "DUP+ : dupliquer toute la page courante vers les pages cochees, puis VALIDER.";
}

void EditorMapVfxScene::applyVfxDuplicateToPagesPopupValidate(void)
{
    int targetCount = 0;
    for (bool sel : this->vfxDuplicateToPagesPageSelected)
    {
        if (sel)
        {
            targetCount += 1;
        }
    }
    if (targetCount <= 0)
    {
        this->statusMessage = "Coche au moins une page cible.";
        return;
    }

    const int srcPage = this->vfxDuplicateToPagesPopupSourcePageKey;
    if (srcPage < 0 || srcPage >= kShipVfxLayerPageCount)
    {
        this->statusMessage = "Page source invalide.";
        return;
    }
    const std::vector<ShipVfxInstance> sourceSnapshot =
        this->shipVfxLayerPages[static_cast<size_t>(srcPage)];
    if (sourceSnapshot.empty())
    {
        this->statusMessage = "La page source est vide.";
        return;
    }

    int copiedLayersTotal = 0;
    const int dupMaxPage = this->shipVfxEffectiveLayerPageCount();
    for (int p = 0; p < dupMaxPage; ++p)
    {
        if (!this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(p)])
        {
            continue;
        }

        std::vector<std::pair<uint32_t, uint32_t>> idMap{};
        idMap.reserve(sourceSnapshot.size());
        std::vector<ShipVfxInstance> clonedLayers{};
        clonedLayers.reserve(sourceSnapshot.size());
        for (const ShipVfxInstance& srcLayer : sourceSnapshot)
        {
            ShipVfxInstance dup = this->duplicateShipVfxInstanceFreshId(srcLayer);
            idMap.push_back({srcLayer.instanceId, dup.instanceId});
            clonedLayers.push_back(std::move(dup));
        }
        const auto remapInstanceId = [&idMap](uint32_t oldId) -> uint32_t {
            for (const auto& m : idMap)
            {
                if (m.first == oldId)
                {
                    return m.second;
                }
            }
            return oldId;
        };
        for (ShipVfxInstance& dup : clonedLayers)
        {
            if (dup.spawnAfterInstanceId != 0U)
            {
                dup.spawnAfterInstanceId = remapInstanceId(dup.spawnAfterInstanceId);
            }
            this->remapTrailConeForShipDirectionChange(dup, srcPage % 4, p % 4);
            if (p == srcPage)
            {
                dup.offsetX += 12.0f;
                dup.offsetY += 12.0f;
                for (DirectionOverride& o : dup.directionOverrides)
                {
                    if (o.enabled)
                    {
                        o.offsetX += 12.0f;
                        o.offsetY += 12.0f;
                    }
                }
                if (dup.motionSpawnCaptured)
                {
                    dup.motionSpawnOffsetX += 12.0f;
                    dup.motionSpawnOffsetY += 12.0f;
                }
            }
            this->shipVfxLayerPages[static_cast<size_t>(p)].push_back(std::move(dup));
            copiedLayersTotal += 1;
        }
    }

    const int curKey = this->getShipVfxLayerPageKey();
    const bool addedToCurrentPage = this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(curKey)];

    this->rebuildVfxLayerLabelsFromCurrentInstances();
    this->markShipVfxDirty();
    this->closeVfxDuplicateToPagesPopup();

    if (addedToCurrentPage)
    {
        this->setSelectedVfxInstanceIndex(static_cast<int>(this->currentShipVfxLayers().size()) - 1);
    }

    char buf[96] = {};
    SDL_snprintf(
        buf,
        sizeof(buf),
        "Page dupliquee: %d layer(s) copies vers %d page(s).",
        copiedLayersTotal,
        targetCount);
    this->statusMessage = buf;
}

bool EditorMapVfxScene::computeVfxDuplicateToPagesPopupLayout(VfxDuplicateToPagesPopupLayout* out) const
{
    if (out == nullptr || !this->vfxDuplicateToPagesPopupVisible)
    {
        return false;
    }
    const SDL_FRect mapRect = GetCurrentMap().rect;
    out->dimFullMap = mapRect;
    const float popupW = (std::min)(mapRect.w - 80.0f, 460.0f);
    const float rowH = 22.0f;
    const float rowGap = 3.0f;
    const float topPad = 38.0f;
    const float presetH = 26.0f;
    const float presetGap = 6.0f;
    const float bottomBtns = 36.0f;
    const int pageRows = this->shipVfxEffectiveLayerPageCount();
    const float popupH = topPad + (rowH + rowGap) * static_cast<float>(pageRows) + presetGap +
        presetH + presetGap + bottomBtns;
    out->popup = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};
    const float pad = 12.0f;
    float y = out->popup.y + topPad;
    for (int i = 0; i < pageRows; ++i)
    {
        out->pageRowRects[i] = SDL_FRect{out->popup.x + pad, y, popupW - pad * 2.0f, rowH};
        y += rowH + rowGap;
    }
    const float presetY = y + presetGap;
    const float quarter = (popupW - pad * 2.0f - presetGap * 3.0f) * 0.25f;
    out->btnAll = SDL_FRect{out->popup.x + pad, presetY, quarter, presetH};
    out->btnNone = SDL_FRect{out->btnAll.x + quarter + presetGap, presetY, quarter, presetH};
    out->btnOtherPages = SDL_FRect{out->btnNone.x + quarter + presetGap, presetY, quarter, presetH};
    out->btnSourceOnly = SDL_FRect{out->btnOtherPages.x + quarter + presetGap, presetY, quarter, presetH};
    const float btnY = out->popup.y + popupH - bottomBtns + 4.0f;
    out->cancelBtn = SDL_FRect{out->popup.x + pad, btnY, 120.0f, 28.0f};
    out->validateBtn = SDL_FRect{out->popup.x + popupW - pad - 130.0f, btnY, 130.0f, 28.0f};
    return true;
}

void EditorMapVfxScene::drawVfxDuplicateToPagesPopup(void) const
{
    if (!this->vfxDuplicateToPagesPopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }
    VfxDuplicateToPagesPopupLayout lay{};
    if (!this->computeVfxDuplicateToPagesPopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->vfxDuplicateToPagesPopupLastLayout = lay;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 150});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{34, 40, 50, 245});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{150, 168, 188, 245});
    rc2d_graphics_rectangle("line", &lay.popup);

    RC2D_Text titleText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "DUP+ : dupliquer toute la page vers les pages");
    titleText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&titleText);
    rc2d_graphics_drawText(&titleText, lay.popup.x + 12.0f, lay.popup.y + 8.0f);
    rc2d_graphics_destroyText(&titleText);

    const int dupPageRows = this->shipVfxEffectiveLayerPageCount();
    for (int i = 0; i < dupPageRows; ++i)
    {
        const bool on = this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)];
        rc2d_graphics_setColor(on ? RC2D_Color{64, 108, 86, 235} : RC2D_Color{44, 52, 64, 230});
        rc2d_graphics_rectangle("fill", &lay.pageRowRects[i]);
        rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
        rc2d_graphics_rectangle("line", &lay.pageRowRects[i]);
        char rowBuf[180] = {};
        shipVfxLayerPageLabelUtf8(i, this->shipVfxEditorTargetSectorsABEnabled, rowBuf, sizeof(rowBuf));
        char lineBuf[220] = {};
        SDL_snprintf(lineBuf, sizeof(lineBuf), "%s  [%s]", rowBuf, on ? "X" : " ");
        RC2D_Text rowT = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lineBuf);
        rowT.color = kHudTextColor;
        rc2d_graphics_setTextColor(&rowT);
        rc2d_graphics_drawText(&rowT, lay.pageRowRects[i].x + 8.0f, lay.pageRowRects[i].y + 3.0f);
        rc2d_graphics_destroyText(&rowT);
    }

    auto drawMiniBtn = [this](const SDL_FRect& r, const char* lab) {
        rc2d_graphics_setColor(RC2D_Color{56, 68, 84, 230});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
        rc2d_graphics_rectangle("line", &r);
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lab);
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&t, &tw, &th);
        rc2d_graphics_drawText(&t, r.x + ((r.w - static_cast<float>(tw)) * 0.5f), r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&t);
    };
    drawMiniBtn(lay.btnAll, "Toutes");
    drawMiniBtn(lay.btnNone, "Aucune");
    drawMiniBtn(lay.btnOtherPages, "Autres");
    drawMiniBtn(lay.btnSourceOnly, "Ici");

    drawMiniBtn(lay.cancelBtn, "Annuler");
    rc2d_graphics_setColor(RC2D_Color{72, 118, 92, 230});
    rc2d_graphics_rectangle("fill", &lay.validateBtn);
    rc2d_graphics_setColor(RC2D_Color{150, 188, 160, 240});
    rc2d_graphics_rectangle("line", &lay.validateBtn);
    RC2D_Text valT = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Valider");
    valT.color = kHudTextColor;
    rc2d_graphics_setTextColor(&valT);
    int vw = 0;
    int vh = 0;
    rc2d_graphics_getTextSize(&valT, &vw, &vh);
    rc2d_graphics_drawText(
        &valT,
        lay.validateBtn.x + ((lay.validateBtn.w - static_cast<float>(vw)) * 0.5f),
        lay.validateBtn.y + ((lay.validateBtn.h - static_cast<float>(vh)) * 0.5f));
    rc2d_graphics_destroyText(&valT);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool EditorMapVfxScene::handleVfxDuplicateToPagesPopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->vfxDuplicateToPagesPopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }
    VfxDuplicateToPagesPopupLayout lay{};
    if (!this->computeVfxDuplicateToPagesPopupLayout(&lay))
    {
        return true;
    }
    this->vfxDuplicateToPagesPopupLastLayout = lay;

    if (this->pointInRect(x, y, lay.validateBtn))
    {
        this->applyVfxDuplicateToPagesPopupValidate();
        return true;
    }
    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeVfxDuplicateToPagesPopup();
        this->statusMessage = "Duplication vers pages : annule.";
        return true;
    }
    if (this->pointInRect(x, y, lay.btnAll))
    {
        const int n = this->shipVfxEffectiveLayerPageCount();
        for (int i = 0; i < n; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = true;
        }
        for (int i = n; i < kShipVfxLayerPageCount; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = false;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.btnNone))
    {
        for (bool& s : this->vfxDuplicateToPagesPageSelected)
        {
            s = false;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.btnOtherPages))
    {
        const int src = this->vfxDuplicateToPagesPopupSourcePageKey;
        const int n = this->shipVfxEffectiveLayerPageCount();
        for (int i = 0; i < n; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = (i != src);
        }
        for (int i = n; i < kShipVfxLayerPageCount; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = false;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.btnSourceOnly))
    {
        const int n = this->shipVfxEffectiveLayerPageCount();
        const int srcKey = this->vfxDuplicateToPagesPopupSourcePageKey;
        for (int i = 0; i < n; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = (i == srcKey);
        }
        for (int i = n; i < kShipVfxLayerPageCount; ++i)
        {
            this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)] = false;
        }
        return true;
    }
    const int dupClickRows = this->shipVfxEffectiveLayerPageCount();
    for (int i = 0; i < dupClickRows; ++i)
    {
        if (this->pointInRect(x, y, lay.pageRowRects[i]))
        {
            bool& s = this->vfxDuplicateToPagesPageSelected[static_cast<size_t>(i)];
            s = !s;
            return true;
        }
    }
    if (!this->pointInRect(x, y, lay.popup))
    {
        this->closeVfxDuplicateToPagesPopup();
        this->statusMessage = "Duplication vers pages : annule.";
    }
    return true;
}

bool EditorMapVfxScene::handleVfxDuplicateToPagesPopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;
    (void)keycode;
    (void)mod;
    if (!this->vfxDuplicateToPagesPopupVisible)
    {
        return false;
    }
    if (scancode == SDL_SCANCODE_ESCAPE && !isrepeat)
    {
        this->closeVfxDuplicateToPagesPopup();
        this->statusMessage = "Duplication vers pages : annule.";
        return true;
    }
    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        this->applyVfxDuplicateToPagesPopupValidate();
        return true;
    }
    return true;
}

void EditorMapVfxScene::closeShipVfxShipDuplicatePopup(void)
{
    this->shipVfxShipDuplicatePopupVisible = false;
    this->shipVfxShipDuplicateSourceShipIndex = -1;
    this->shipVfxShipDuplicateSourceAnimations.clear();
    this->shipVfxShipDuplicateSourceAnimationSelected.clear();
    this->shipVfxShipDuplicateTargetShipSelected.clear();
    this->shipVfxShipDuplicateSourceShipScrollOffset = 0;
    this->shipVfxShipDuplicateAnimationScrollOffset = 0;
    this->shipVfxShipDuplicateTargetShipScrollOffset = 0;
    this->shipVfxShipDuplicateSourceShipScrollDragActive = false;
    this->shipVfxShipDuplicateAnimationScrollDragActive = false;
    this->shipVfxShipDuplicateTargetShipScrollDragActive = false;
    this->shipVfxShipDuplicateSourceShipScrollDragGrabOffsetY = 0.0f;
    this->shipVfxShipDuplicateAnimationScrollDragGrabOffsetY = 0.0f;
    this->shipVfxShipDuplicateTargetShipScrollDragGrabOffsetY = 0.0f;
}

void EditorMapVfxScene::openShipVfxShipDuplicatePopup(void)
{
    if (this->editorMode != EditorMode::SHIP_VFX)
    {
        this->statusMessage = "Disponible uniquement en mode Ship / VFX.";
        return;
    }
    if (this->importedShips.empty())
    {
        this->statusMessage = "Aucun navire importe pour la duplication.";
        return;
    }

    this->clearEditorTransientInteractionState();
    this->shipVfxShipDuplicatePopupVisible = true;
    this->shipVfxShipDuplicateSourceShipIndex =
        (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
        ? this->selectedShipIndex
        : 0;
    this->shipVfxShipDuplicateTargetShipSelected.assign(this->importedShips.size(), false);
    this->shipVfxShipDuplicateSourceShipScrollOffset = 0;
    this->shipVfxShipDuplicateAnimationScrollOffset = 0;
    this->shipVfxShipDuplicateTargetShipScrollOffset = 0;
    this->shipVfxShipDuplicateSourceShipScrollDragActive = false;
    this->shipVfxShipDuplicateAnimationScrollDragActive = false;
    this->shipVfxShipDuplicateTargetShipScrollDragActive = false;
    this->shipVfxShipDuplicateSourceShipScrollDragGrabOffsetY = 0.0f;
    this->shipVfxShipDuplicateAnimationScrollDragGrabOffsetY = 0.0f;
    this->shipVfxShipDuplicateTargetShipScrollDragGrabOffsetY = 0.0f;
    this->ensureSelectionVisible(
        this->shipVfxShipDuplicateSourceShipIndex,
        &this->shipVfxShipDuplicateSourceShipScrollOffset,
        static_cast<int>(this->importedShips.size()));
    this->statusMessage = "Duplication ships: choisis source, animations, puis cibles et VALIDER.";
    this->refreshShipVfxShipDuplicatePopupSourceAnimations();
}

bool EditorMapVfxScene::buildShipVfxShipDuplicateSourceAnimationsForShipIndex(
    int sourceShipIndex,
    std::vector<ShipVfxShipDuplicateSourceAnimation>* outAnimations) const
{
    if (outAnimations == nullptr)
    {
        return false;
    }
    outAnimations->clear();
    if (sourceShipIndex < 0 || sourceShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        return false;
    }

    const ImportedShip& sourceShip = this->importedShips[static_cast<size_t>(sourceShipIndex)];
    const std::filesystem::path shipFolderPath(sourceShip.folderAbsolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(shipFolderPath, fsError) || !std::filesystem::is_directory(shipFolderPath, fsError))
    {
        return false;
    }

    std::vector<std::filesystem::path> candidateJsonPaths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(shipFolderPath, fsError))
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
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension == ".json")
        {
            candidateJsonPaths.push_back(entry.path());
        }
    }

    std::sort(candidateJsonPaths.begin(), candidateJsonPaths.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
        return makePathKeyLower(a.filename().string()) < makePathKeyLower(b.filename().string());
    });

    const std::string sourceShipPathKey = makeComparableSourcePathKey(sourceShip.folderAbsolutePath);
    for (const std::filesystem::path& jsonPath : candidateJsonPaths)
    {
        std::string jsonText;
        if (!readTextFileUtf8(jsonPath.string(), &jsonText))
        {
            continue;
        }
        cJSON* root = cJSON_Parse(jsonText.c_str());
        if (root == nullptr)
        {
            continue;
        }

        bool includeFile = false;
        const cJSON* formatNode = cJSON_GetObjectItemCaseSensitive(root, "format");
        if (cJSON_IsString(formatNode) &&
            formatNode->valuestring != nullptr &&
            SDL_strcasecmp(formatNode->valuestring, "ship_vfx_config") == 0)
        {
            includeFile = true;
        }
        if (!includeFile)
        {
            cJSON_Delete(root);
            continue;
        }

        std::string shipPathFromJson;
        const cJSON* editorNode = cJSON_GetObjectItemCaseSensitive(root, "editor");
        if (cJSON_IsObject(editorNode))
        {
            const cJSON* shipNode = cJSON_GetObjectItemCaseSensitive(editorNode, "ship");
            if (cJSON_IsObject(shipNode))
            {
                const cJSON* folderPathNode = cJSON_GetObjectItemCaseSensitive(shipNode, "folderPath");
                const cJSON* folderAbsoluteNode = cJSON_GetObjectItemCaseSensitive(shipNode, "folderAbsolutePath");
                if (cJSON_IsString(folderPathNode) && folderPathNode->valuestring != nullptr)
                {
                    shipPathFromJson = folderPathNode->valuestring;
                }
                else if (cJSON_IsString(folderAbsoluteNode) && folderAbsoluteNode->valuestring != nullptr)
                {
                    shipPathFromJson = folderAbsoluteNode->valuestring;
                }
            }
        }
        if (shipPathFromJson.empty())
        {
            const cJSON* gameplayNode = cJSON_GetObjectItemCaseSensitive(root, "gameplay");
            if (cJSON_IsObject(gameplayNode))
            {
                const cJSON* shipFolderPathNode = cJSON_GetObjectItemCaseSensitive(gameplayNode, "shipFolderPath");
                if (cJSON_IsString(shipFolderPathNode) && shipFolderPathNode->valuestring != nullptr)
                {
                    shipPathFromJson = shipFolderPathNode->valuestring;
                }
            }
        }
        if (!shipPathFromJson.empty() &&
            makeComparableSourcePathKey(shipPathFromJson) != sourceShipPathKey)
        {
            cJSON_Delete(root);
            continue;
        }

        std::string displayName;
        if (cJSON_IsObject(editorNode))
        {
            const cJSON* animationNode = cJSON_GetObjectItemCaseSensitive(editorNode, "animation");
            if (cJSON_IsObject(animationNode))
            {
                const cJSON* animationNameNode = cJSON_GetObjectItemCaseSensitive(animationNode, "displayName");
                if (cJSON_IsString(animationNameNode) && animationNameNode->valuestring != nullptr)
                {
                    displayName = trimAscii(animationNameNode->valuestring);
                }
            }
        }
        if (displayName.empty())
        {
            displayName = stripListPrefix(jsonPath.stem().string(), "fx-");
        }
        if (displayName.empty())
        {
            displayName = stripListPrefix(jsonPath.stem().string(), "vfx-");
        }
        if (displayName.empty())
        {
            displayName = jsonPath.stem().string();
        }

        ShipVfxShipDuplicateSourceAnimation animationEntry{};
        animationEntry.fileName = jsonPath.filename().string();
        animationEntry.absolutePath = normalizePathSlashes(jsonPath.string());
        animationEntry.displayName = displayName;
        outAnimations->push_back(std::move(animationEntry));
        cJSON_Delete(root);
    }

    std::sort(outAnimations->begin(), outAnimations->end(), [](const ShipVfxShipDuplicateSourceAnimation& a, const ShipVfxShipDuplicateSourceAnimation& b) {
        const std::string aKey = makePathKeyLower(a.displayName);
        const std::string bKey = makePathKeyLower(b.displayName);
        if (aKey != bKey)
        {
            return aKey < bKey;
        }
        return makePathKeyLower(a.fileName) < makePathKeyLower(b.fileName);
    });
    return true;
}

void EditorMapVfxScene::refreshShipVfxShipDuplicatePopupSourceAnimations(void)
{
    this->shipVfxShipDuplicateSourceAnimations.clear();
    this->shipVfxShipDuplicateSourceAnimationSelected.clear();
    this->shipVfxShipDuplicateAnimationScrollOffset = 0;
    this->shipVfxShipDuplicateAnimationScrollDragActive = false;
    this->shipVfxShipDuplicateAnimationScrollDragGrabOffsetY = 0.0f;
    if (this->shipVfxShipDuplicateSourceShipIndex < 0 ||
        this->shipVfxShipDuplicateSourceShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        return;
    }

    std::vector<ShipVfxShipDuplicateSourceAnimation> sourceAnimations;
    if (!this->buildShipVfxShipDuplicateSourceAnimationsForShipIndex(
            this->shipVfxShipDuplicateSourceShipIndex,
            &sourceAnimations))
    {
        this->statusMessage = "Duplication ships: impossible de lire les animations source.";
        return;
    }
    this->shipVfxShipDuplicateSourceAnimations = std::move(sourceAnimations);
    this->shipVfxShipDuplicateSourceAnimationSelected.assign(
        this->shipVfxShipDuplicateSourceAnimations.size(),
        true);
    if (this->shipVfxShipDuplicateSourceAnimations.empty())
    {
        this->statusMessage = "Duplication ships: aucune animation VFX exportee trouvee pour la source.";
        return;
    }
    this->statusMessage =
        "Duplication ships: " +
        std::to_string(static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size())) +
        " animation(s) source detectee(s).";
}

bool EditorMapVfxScene::applyShipVfxShipDuplicatePopupValidate(void)
{
    if (this->shipVfxShipDuplicateSourceShipIndex < 0 ||
        this->shipVfxShipDuplicateSourceShipIndex >= static_cast<int>(this->importedShips.size()))
    {
        this->statusMessage = "Duplication ships: choisis un navire source valide.";
        return false;
    }
    if (this->shipVfxShipDuplicateSourceAnimations.empty() ||
        this->shipVfxShipDuplicateSourceAnimationSelected.size() != this->shipVfxShipDuplicateSourceAnimations.size())
    {
        this->statusMessage = "Duplication ships: aucune animation source disponible.";
        return false;
    }
    if (this->shipVfxShipDuplicateTargetShipSelected.size() != this->importedShips.size())
    {
        this->statusMessage = "Duplication ships: liste des cibles invalide.";
        return false;
    }

    std::vector<int> selectedAnimationIndices;
    selectedAnimationIndices.reserve(this->shipVfxShipDuplicateSourceAnimations.size());
    for (int i = 0; i < static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size()); ++i)
    {
        if (this->shipVfxShipDuplicateSourceAnimationSelected[static_cast<size_t>(i)])
        {
            selectedAnimationIndices.push_back(i);
        }
    }
    if (selectedAnimationIndices.empty())
    {
        this->statusMessage = "Duplication ships: coche au moins une animation.";
        return false;
    }

    std::vector<int> selectedTargetShipIndices;
    selectedTargetShipIndices.reserve(this->shipVfxShipDuplicateTargetShipSelected.size());
    for (int i = 0; i < static_cast<int>(this->shipVfxShipDuplicateTargetShipSelected.size()); ++i)
    {
        if (this->shipVfxShipDuplicateTargetShipSelected[static_cast<size_t>(i)])
        {
            selectedTargetShipIndices.push_back(i);
        }
    }
    if (selectedTargetShipIndices.empty())
    {
        this->statusMessage = "Duplication ships: coche au moins un navire cible.";
        return false;
    }

    const ImportedShip& sourceShip = this->importedShips[static_cast<size_t>(this->shipVfxShipDuplicateSourceShipIndex)];
    const std::string sourceShipSlug = makeShipConfigSlug(sourceShip.displayName);

    int writtenCount = 0;
    int failedCount = 0;
    int targetsWithWrite = 0;
    for (const int targetShipIndex : selectedTargetShipIndices)
    {
        if (targetShipIndex < 0 || targetShipIndex >= static_cast<int>(this->importedShips.size()))
        {
            continue;
        }
        const ImportedShip& targetShip = this->importedShips[static_cast<size_t>(targetShipIndex)];
        const std::string targetShipSlug = makeShipConfigSlug(targetShip.displayName);
        const std::string targetShipFolderRelativePath =
            buildAssetsRelativePathForExport(targetShip.folderAbsolutePath, targetShip.displayName);
        const std::string targetShipFolderAbsolutePath =
            normalizePathSlashes(targetShip.folderAbsolutePath);

        std::error_code fsError;
        std::filesystem::create_directories(targetShip.folderAbsolutePath, fsError);
        if (fsError)
        {
            failedCount += static_cast<int>(selectedAnimationIndices.size());
            continue;
        }

        std::vector<std::string> usedOutputNamesLower;
        bool wroteForTarget = false;
        for (const int animationIndex : selectedAnimationIndices)
        {
            const ShipVfxShipDuplicateSourceAnimation& sourceAnimation =
                this->shipVfxShipDuplicateSourceAnimations[static_cast<size_t>(animationIndex)];

            std::string jsonText;
            if (!readTextFileUtf8(sourceAnimation.absolutePath, &jsonText))
            {
                failedCount += 1;
                continue;
            }

            cJSON* root = cJSON_Parse(jsonText.c_str());
            if (root == nullptr)
            {
                failedCount += 1;
                continue;
            }
            const cJSON* formatNode = cJSON_GetObjectItemCaseSensitive(root, "format");
            const bool jsonLooksLikeShipVfxConfig =
                cJSON_IsString(formatNode) &&
                formatNode->valuestring != nullptr &&
                SDL_strcasecmp(formatNode->valuestring, "ship_vfx_config") == 0;
            if (!jsonLooksLikeShipVfxConfig)
            {
                cJSON_Delete(root);
                failedCount += 1;
                continue;
            }

            cJSON* editorNode = ensureJsonObjectField(root, "editor");
            cJSON* gameplayNode = ensureJsonObjectField(root, "gameplay");
            cJSON* shipNode = ensureJsonObjectField(editorNode, "ship");
            if (editorNode == nullptr || gameplayNode == nullptr || shipNode == nullptr)
            {
                cJSON_Delete(root);
                failedCount += 1;
                continue;
            }

            setOrReplaceJsonStringField(shipNode, "displayName", targetShip.displayName);
            setOrReplaceJsonStringField(shipNode, "folderPath", targetShipFolderRelativePath);
            setOrReplaceJsonStringField(shipNode, "folderAbsolutePath", targetShipFolderAbsolutePath);
            setOrReplaceJsonStringField(shipNode, "id", targetShipSlug);
            setOrReplaceJsonStringField(gameplayNode, "shipFolderPath", targetShipFolderRelativePath);

            std::string outputFileName = buildShipVfxDuplicateTargetFileName(
                sourceAnimation.fileName,
                sourceShipSlug,
                targetShipSlug,
                sourceAnimation.displayName);
            const std::string outputBaseStem = std::filesystem::path(outputFileName).stem().string();
            int suffix = 2;
            while (std::find(
                       usedOutputNamesLower.begin(),
                       usedOutputNamesLower.end(),
                       makePathKeyLower(outputFileName)) != usedOutputNamesLower.end())
            {
                outputFileName = outputBaseStem + "-" + std::to_string(suffix) + ".json";
                suffix += 1;
            }
            usedOutputNamesLower.push_back(makePathKeyLower(outputFileName));

            const std::filesystem::path outputJsonPath =
                std::filesystem::path(targetShip.folderAbsolutePath) / outputFileName;

            char* outputJsonText = cJSON_Print(root);
            cJSON_Delete(root);
            if (outputJsonText == nullptr)
            {
                failedCount += 1;
                continue;
            }

            std::ofstream output(outputJsonPath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                cJSON_free(outputJsonText);
                failedCount += 1;
                continue;
            }
            output.write(outputJsonText, static_cast<std::streamsize>(std::strlen(outputJsonText)));
            const bool writeOk = output.good();
            output.close();
            cJSON_free(outputJsonText);

            if (!writeOk)
            {
                failedCount += 1;
                continue;
            }

            wroteForTarget = true;
            writtenCount += 1;
        }
        if (wroteForTarget)
        {
            targetsWithWrite += 1;
        }
    }

    if (writtenCount <= 0)
    {
        this->statusMessage = "Duplication ships: echec (aucun JSON ecrit).";
        return false;
    }

    this->closeShipVfxShipDuplicatePopup();
    if (failedCount == 0)
    {
        this->statusMessage =
            "Duplication ships OK: " + std::to_string(writtenCount) +
            " JSON vers " + std::to_string(targetsWithWrite) + " navire(s).";
    }
    else
    {
        this->statusMessage =
            "Duplication ships partielle: " + std::to_string(writtenCount) +
            " JSON OK, " + std::to_string(failedCount) + " echec(s).";
    }
    return true;
}

bool EditorMapVfxScene::computeShipVfxShipDuplicatePopupLayout(ShipVfxShipDuplicatePopupLayout* out) const
{
    if (out == nullptr || !this->shipVfxShipDuplicatePopupVisible)
    {
        return false;
    }

    const SDL_FRect mapRect = GetCurrentMap().rect;
    out->dimFullMap = mapRect;

    const float popupW = (std::clamp)(mapRect.w - 90.0f, 860.0f, 1220.0f);
    const float popupH = (std::clamp)(mapRect.h - 100.0f, 460.0f, 700.0f);
    out->popup = SDL_FRect{
        mapRect.x + ((mapRect.w - popupW) * 0.5f),
        mapRect.y + ((mapRect.h - popupH) * 0.5f),
        popupW,
        popupH};

    const float pad = 14.0f;
    const float colGap = 10.0f;
    const float topHeaderH = 36.0f;
    const float footerH = 34.0f;
    const float listButtonsH = 58.0f;
    const float listTop = out->popup.y + topHeaderH + 20.0f;
    const float listH = (std::max)(out->popup.h - (topHeaderH + footerH + listButtonsH + 36.0f), 160.0f);
    const float listW = (out->popup.w - (pad * 2.0f) - (colGap * 2.0f)) / 3.0f;

    const float col1X = out->popup.x + pad;
    const float col2X = col1X + listW + colGap;
    const float col3X = col2X + listW + colGap;

    out->sourceShipListRect = SDL_FRect{col1X, listTop, listW, listH};
    out->sourceAnimationListRect = SDL_FRect{col2X, listTop, listW, listH};
    out->targetShipListRect = SDL_FRect{col3X, listTop, listW, listH};

    const float listActionsY = listTop + listH + 6.0f;
    const float halfBtnW = (listW - 6.0f) * 0.5f;
    out->animationSelectAllRect = SDL_FRect{col2X, listActionsY, halfBtnW, 24.0f};
    out->animationSelectNoneRect = SDL_FRect{col2X + halfBtnW + 6.0f, listActionsY, halfBtnW, 24.0f};
    out->targetSelectAllRect = SDL_FRect{col3X, listActionsY, halfBtnW, 24.0f};
    out->targetSelectNoneRect = SDL_FRect{col3X + halfBtnW + 6.0f, listActionsY, halfBtnW, 24.0f};
    out->targetSelectAllExceptSourceRect = SDL_FRect{col3X, listActionsY + 28.0f, listW, 24.0f};

    const float footerY = out->popup.y + out->popup.h - footerH + 2.0f;
    out->cancelBtn = SDL_FRect{out->popup.x + pad, footerY, 140.0f, 28.0f};
    out->validateBtn = SDL_FRect{out->popup.x + out->popup.w - pad - 210.0f, footerY, 210.0f, 28.0f};
    return true;
}

void EditorMapVfxScene::drawShipVfxShipDuplicatePopup(void) const
{
    if (!this->shipVfxShipDuplicatePopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    ShipVfxShipDuplicatePopupLayout lay{};
    if (!this->computeShipVfxShipDuplicatePopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->shipVfxShipDuplicatePopupLastLayout = lay;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 165});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{26, 34, 44, 246});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{148, 170, 194, 246});
    rc2d_graphics_rectangle("line", &lay.popup);

    RC2D_Text title = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Duplication VFX multi-navires");
    title.color = kHudTextColor;
    rc2d_graphics_setTextColor(&title);
    rc2d_graphics_drawText(&title, lay.popup.x + 14.0f, lay.popup.y + 8.0f);
    rc2d_graphics_destroyText(&title);

    int selectedAnimationCount = 0;
    for (bool selected : this->shipVfxShipDuplicateSourceAnimationSelected)
    {
        if (selected)
        {
            selectedAnimationCount += 1;
        }
    }
    int selectedTargetCount = 0;
    for (bool selected : this->shipVfxShipDuplicateTargetShipSelected)
    {
        if (selected)
        {
            selectedTargetCount += 1;
        }
    }

    const float headerY = lay.sourceShipListRect.y - 16.0f;
    RC2D_Text sourceText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Source ship");
    sourceText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&sourceText);
    rc2d_graphics_drawText(&sourceText, lay.sourceShipListRect.x + 2.0f, headerY);
    rc2d_graphics_destroyText(&sourceText);

    char animationHeader[128] = {};
    SDL_snprintf(
        animationHeader,
        sizeof(animationHeader),
        "Animations source (%d/%d)",
        selectedAnimationCount,
        static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size()));
    RC2D_Text animationText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), animationHeader);
    animationText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&animationText);
    rc2d_graphics_drawText(&animationText, lay.sourceAnimationListRect.x + 2.0f, headerY);
    rc2d_graphics_destroyText(&animationText);

    char targetHeader[128] = {};
    SDL_snprintf(
        targetHeader,
        sizeof(targetHeader),
        "Ships cibles (%d/%d)",
        selectedTargetCount,
        static_cast<int>(this->importedShips.size()));
    RC2D_Text targetText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), targetHeader);
    targetText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&targetText);
    rc2d_graphics_drawText(&targetText, lay.targetShipListRect.x + 2.0f, headerY);
    rc2d_graphics_destroyText(&targetText);

    std::vector<std::string> sourceShipLabels;
    sourceShipLabels.reserve(this->importedShips.size());
    for (const ImportedShip& ship : this->importedShips)
    {
        sourceShipLabels.push_back(stripListPrefix(ship.displayName, "ship-"));
    }
    this->drawListPanel(
        lay.sourceShipListRect,
        "Navire source",
        sourceShipLabels,
        this->shipVfxShipDuplicateSourceShipIndex,
        this->shipVfxShipDuplicateSourceShipScrollOffset);

    std::vector<std::string> animationLabels;
    animationLabels.reserve(this->shipVfxShipDuplicateSourceAnimations.size());
    for (size_t i = 0; i < this->shipVfxShipDuplicateSourceAnimations.size(); ++i)
    {
        const bool on =
            i < this->shipVfxShipDuplicateSourceAnimationSelected.size() &&
            this->shipVfxShipDuplicateSourceAnimationSelected[i];
        const ShipVfxShipDuplicateSourceAnimation& animation = this->shipVfxShipDuplicateSourceAnimations[i];
        animationLabels.push_back(std::string(on ? "[X] " : "[ ] ") + animation.displayName);
    }
    this->drawListPanel(
        lay.sourceAnimationListRect,
        "Animations",
        animationLabels,
        -1,
        this->shipVfxShipDuplicateAnimationScrollOffset);

    std::vector<std::string> targetShipLabels;
    targetShipLabels.reserve(this->importedShips.size());
    for (size_t i = 0; i < this->importedShips.size(); ++i)
    {
        const bool on =
            i < this->shipVfxShipDuplicateTargetShipSelected.size() &&
            this->shipVfxShipDuplicateTargetShipSelected[i];
        std::string label = std::string(on ? "[X] " : "[ ] ") +
            stripListPrefix(this->importedShips[i].displayName, "ship-");
        if (static_cast<int>(i) == this->shipVfxShipDuplicateSourceShipIndex)
        {
            label += " (source)";
        }
        targetShipLabels.push_back(std::move(label));
    }
    this->drawListPanel(
        lay.targetShipListRect,
        "Cibles",
        targetShipLabels,
        -1,
        this->shipVfxShipDuplicateTargetShipScrollOffset);

    this->drawToolbarButton(lay.animationSelectAllRect, "ANIMS TOUT", false);
    this->drawToolbarButton(lay.animationSelectNoneRect, "ANIMS AUCUNE", false);
    this->drawToolbarButton(lay.targetSelectAllRect, "SHIPS TOUT", false);
    this->drawToolbarButton(lay.targetSelectNoneRect, "SHIPS AUCUN", false);
    this->drawToolbarButton(lay.targetSelectAllExceptSourceRect, "TOUS SAUF SOURCE", false);
    this->drawToolbarButton(lay.cancelBtn, "ANNULER", false);

    const bool canValidate = (selectedAnimationCount > 0 && selectedTargetCount > 0);
    this->drawToolbarButton(lay.validateBtn, "VALIDER DUPLICATION", canValidate);

    RC2D_Text hintText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Clic gauche: cocher/decocher | Entree: valider | Echap: annuler");
    hintText.color = kHudStatusColor;
    rc2d_graphics_setTextColor(&hintText);
    rc2d_graphics_drawText(&hintText, lay.popup.x + 14.0f, lay.cancelBtn.y - 18.0f);
    rc2d_graphics_destroyText(&hintText);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool EditorMapVfxScene::handleShipVfxShipDuplicatePopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->shipVfxShipDuplicatePopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    ShipVfxShipDuplicatePopupLayout lay{};
    if (!this->computeShipVfxShipDuplicatePopupLayout(&lay))
    {
        return true;
    }
    this->shipVfxShipDuplicatePopupLastLayout = lay;

    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeShipVfxShipDuplicatePopup();
        this->statusMessage = "Duplication ships: annulee.";
        return true;
    }
    if (this->pointInRect(x, y, lay.validateBtn))
    {
        (void)this->applyShipVfxShipDuplicatePopupValidate();
        return true;
    }
    if (this->pointInRect(x, y, lay.animationSelectAllRect))
    {
        for (size_t i = 0; i < this->shipVfxShipDuplicateSourceAnimationSelected.size(); ++i)
        {
            this->shipVfxShipDuplicateSourceAnimationSelected[i] = true;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.animationSelectNoneRect))
    {
        for (size_t i = 0; i < this->shipVfxShipDuplicateSourceAnimationSelected.size(); ++i)
        {
            this->shipVfxShipDuplicateSourceAnimationSelected[i] = false;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.targetSelectAllRect))
    {
        for (size_t i = 0; i < this->shipVfxShipDuplicateTargetShipSelected.size(); ++i)
        {
            this->shipVfxShipDuplicateTargetShipSelected[i] = true;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.targetSelectNoneRect))
    {
        for (size_t i = 0; i < this->shipVfxShipDuplicateTargetShipSelected.size(); ++i)
        {
            this->shipVfxShipDuplicateTargetShipSelected[i] = false;
        }
        return true;
    }
    if (this->pointInRect(x, y, lay.targetSelectAllExceptSourceRect))
    {
        for (size_t i = 0; i < this->shipVfxShipDuplicateTargetShipSelected.size(); ++i)
        {
            this->shipVfxShipDuplicateTargetShipSelected[i] =
                (static_cast<int>(i) != this->shipVfxShipDuplicateSourceShipIndex);
        }
        return true;
    }

    int clickedIndex = -1;
    const bool sourceConsumed = this->handleListPanelClick(
        x,
        y,
        lay.sourceShipListRect,
        static_cast<int>(this->importedShips.size()),
        &this->shipVfxShipDuplicateSourceShipScrollOffset,
        &this->shipVfxShipDuplicateSourceShipScrollDragActive,
        &this->shipVfxShipDuplicateSourceShipScrollDragGrabOffsetY,
        &clickedIndex);
    if (sourceConsumed)
    {
        if (clickedIndex >= 0 &&
            clickedIndex < static_cast<int>(this->importedShips.size()) &&
            clickedIndex != this->shipVfxShipDuplicateSourceShipIndex)
        {
            this->shipVfxShipDuplicateSourceShipIndex = clickedIndex;
            this->ensureSelectionVisible(
                this->shipVfxShipDuplicateSourceShipIndex,
                &this->shipVfxShipDuplicateSourceShipScrollOffset,
                static_cast<int>(this->importedShips.size()));
            this->refreshShipVfxShipDuplicatePopupSourceAnimations();
        }
        return true;
    }

    clickedIndex = -1;
    const bool animationConsumed = this->handleListPanelClick(
        x,
        y,
        lay.sourceAnimationListRect,
        static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size()),
        &this->shipVfxShipDuplicateAnimationScrollOffset,
        &this->shipVfxShipDuplicateAnimationScrollDragActive,
        &this->shipVfxShipDuplicateAnimationScrollDragGrabOffsetY,
        &clickedIndex);
    if (animationConsumed)
    {
        if (clickedIndex >= 0 &&
            clickedIndex < static_cast<int>(this->shipVfxShipDuplicateSourceAnimationSelected.size()))
        {
            const size_t selectedIndex = static_cast<size_t>(clickedIndex);
            this->shipVfxShipDuplicateSourceAnimationSelected[selectedIndex] =
                !this->shipVfxShipDuplicateSourceAnimationSelected[selectedIndex];
        }
        return true;
    }

    clickedIndex = -1;
    const bool targetConsumed = this->handleListPanelClick(
        x,
        y,
        lay.targetShipListRect,
        static_cast<int>(this->importedShips.size()),
        &this->shipVfxShipDuplicateTargetShipScrollOffset,
        &this->shipVfxShipDuplicateTargetShipScrollDragActive,
        &this->shipVfxShipDuplicateTargetShipScrollDragGrabOffsetY,
        &clickedIndex);
    if (targetConsumed)
    {
        if (clickedIndex >= 0 &&
            clickedIndex < static_cast<int>(this->shipVfxShipDuplicateTargetShipSelected.size()))
        {
            const size_t selectedIndex = static_cast<size_t>(clickedIndex);
            this->shipVfxShipDuplicateTargetShipSelected[selectedIndex] =
                !this->shipVfxShipDuplicateTargetShipSelected[selectedIndex];
        }
        return true;
    }

    if (!this->pointInRect(x, y, lay.popup))
    {
        this->closeShipVfxShipDuplicatePopup();
        this->statusMessage = "Duplication ships: annulee.";
    }
    return true;
}

bool EditorMapVfxScene::handleShipVfxShipDuplicatePopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;
    (void)keycode;
    (void)mod;
    if (!this->shipVfxShipDuplicatePopupVisible)
    {
        return false;
    }
    if (scancode == SDL_SCANCODE_ESCAPE && !isrepeat)
    {
        this->closeShipVfxShipDuplicatePopup();
        this->statusMessage = "Duplication ships: annulee.";
        return true;
    }
    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        (void)this->applyShipVfxShipDuplicatePopupValidate();
        return true;
    }
    return true;
}

bool EditorMapVfxScene::handleShipVfxShipDuplicatePopupMouseWheel(int delta, float mouseX, float mouseY)
{
    if (!this->shipVfxShipDuplicatePopupVisible)
    {
        return false;
    }
    ShipVfxShipDuplicatePopupLayout lay{};
    if (!this->computeShipVfxShipDuplicatePopupLayout(&lay))
    {
        return true;
    }

    auto scrollIfInside = [this, delta, mouseX, mouseY](const SDL_FRect& rect, int itemCount, int* scrollOffset) -> bool {
        if (scrollOffset == nullptr || !this->pointInRect(mouseX, mouseY, rect))
        {
            return false;
        }
        *scrollOffset -= delta;
        this->clampListScrollOffset(scrollOffset, itemCount);
        return true;
    };

    if (scrollIfInside(
            lay.sourceShipListRect,
            static_cast<int>(this->importedShips.size()),
            &this->shipVfxShipDuplicateSourceShipScrollOffset))
    {
        return true;
    }
    if (scrollIfInside(
            lay.sourceAnimationListRect,
            static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size()),
            &this->shipVfxShipDuplicateAnimationScrollOffset))
    {
        return true;
    }
    if (scrollIfInside(
            lay.targetShipListRect,
            static_cast<int>(this->importedShips.size()),
            &this->shipVfxShipDuplicateTargetShipScrollOffset))
    {
        return true;
    }

    return this->pointInRect(mouseX, mouseY, lay.popup);
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
    if (*drawOrder == this->activeShipDrawOrder())
    {
        *drawOrder += (delta >= 0) ? 1 : -1;
    }
    instance->behindShip = (*drawOrder < this->activeShipDrawOrder());
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
        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
        int sourceInstanceNumber = 0;
        for (size_t i = 0; i < this->currentShipVfxLayers().size(); ++i)
        {
            const ShipVfxInstance& candidate = this->currentShipVfxLayers()[i];
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
    this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].label = trimmed;
    this->layerNameInput = trimmed;
    this->markShipVfxDirty();
    return true;
}

void EditorMapVfxScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();
    const SDL_FRect& gs = GetGameScreen().rect;
    const float startX = 12.0f;
    constexpr float h = 24.0f;
    const float gap = 8.0f;

    // Reference = game screen (zone logique). Marges map = bandes GUI hors ocean.
    const float topMargin = Map::MAP_TOP_UI_MARGIN_PX;
    const MapPlayfieldFrameMarginsPercent frame = MapGetPlayfieldFrameMarginsPercent();
    const float bottomMargin = gs.h * (static_cast<float>(frame.bottom) / 100.0f);
    const float topToolbarY = gs.y + std::floor((std::max)(0.0f, topMargin - h) * 0.5f);

    const float bottomStripTop = gs.y + gs.h - bottomMargin;
    constexpr float bottomPad = 5.0f;
    constexpr float bottomInterRowGap = 5.0f;
    const float guiBottomRow0Y = bottomStripTop + bottomPad;
    const float guiBottomRow1Y = guiBottomRow0Y + h + bottomInterRowGap;

    // Mode loose : deux lignes dans la bande basse 65px (alignees sur le game screen).
    const float row1Y = guiBottomRow0Y;
    const float row2Y = guiBottomRow1Y;

    // Mode ship : une seule ligne basse pour les controles navire (FLIP / SHARED / OVERRIDE / CENTER : panneau Layers).
    const float shipRow1Y = guiBottomRow0Y;

    auto setNextButton = [h, gap](SDL_FRect* rect, float* x, float y, float w) {
        rect->x = *x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        *x += w + gap;
    };
    float x = gs.x + startX;
    setNextButton(&this->buttonModeShipVfxRect, &x, topToolbarY, 210.0f);
    setNextButton(&this->buttonModeLooseSpritesRect, &x, topToolbarY, 460.0f);
    setNextButton(&this->buttonExportRect, &x, topToolbarY, 126.0f);
    setNextButton(&this->buttonReloadAssetsRect, &x, topToolbarY, 136.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, topToolbarY, 92.0f);
    setNextButton(&this->buttonOceanNextRect, &x, topToolbarY, 92.0f);

    // Ligne 1 (gauche): controles navire (largeurs alignees sur le texte + opacite).
    x = gs.x + startX;
    setNextButton(&this->buttonDirectionPrevRect, &x, shipRow1Y, 158.0f);
    setNextButton(&this->buttonDirectionNextRect, &x, shipRow1Y, 158.0f);
    setNextButton(&this->buttonShipStateToggleRect, &x, shipRow1Y, 220.0f);
    setNextButton(&this->buttonTargetFireSectorToggleRect, &x, shipRow1Y, 200.0f);
    setNextButton(&this->buttonShipOpacityMinusRect, &x, shipRow1Y, 88.0f);
    setNextButton(&this->buttonShipOpacityPlusRect, &x, shipRow1Y, 88.0f);
    setNextButton(&this->buttonPreviewIsoGridRect, &x, shipRow1Y, 152.0f);
    const float shipVfxZoomToolbarW = 110.0f;
    setNextButton(&this->buttonShipVfxZoomMinusRect, &x, shipRow1Y, shipVfxZoomToolbarW);
    setNextButton(&this->buttonShipVfxZoomPlusRect, &x, shipRow1Y, shipVfxZoomToolbarW);

    // Mode downscale: tout sur la meme ligne (gauche zoom+repere, centre import, droite scale).
    const float looseZoomW = 110.0f;
    const float looseReferenceW = 320.0f;
    const float looseScaleW = 200.0f;
    const float looseImportPreferredW = 500.0f;
    const float looseImportMinW = 180.0f;

    float looseLeftX = gs.x + startX;
    setNextButton(&this->buttonLooseZoomMinusRect, &looseLeftX, row1Y, looseZoomW);
    setNextButton(&this->buttonLooseZoomPlusRect, &looseLeftX, row1Y, looseZoomW);
    setNextButton(&this->buttonLooseReferencePreviewRect, &looseLeftX, row1Y, looseReferenceW);
    const float looseLeftEnd = looseLeftX - gap;

    const float looseRightGroupW = (looseScaleW * 2.0f) + gap;
    const float looseRightStart = gs.x + gs.w - startX - looseRightGroupW;
    this->buttonLooseScaleMinusRect = SDL_FRect{looseRightStart, row1Y, looseScaleW, h};
    this->buttonLooseScalePlusRect = SDL_FRect{looseRightStart + looseScaleW + gap, row1Y, looseScaleW, h};

    const float looseImportAvailableW = looseRightStart - gap - (looseLeftEnd + gap);
    float looseImportW = (std::min)(looseImportPreferredW, looseImportAvailableW);
    if (looseImportW < looseImportMinW)
    {
        looseImportW = (std::max)(0.0f, looseImportAvailableW);
    }

    float looseImportX = gs.x + ((gs.w - looseImportW) * 0.5f);
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

    float looseRow2X = gs.x + startX;
    setNextButton(&this->buttonLoosePreviewModeRect, &looseRow2X, row2Y, 250.0f);
    setNextButton(&this->buttonLoosePreviewFpsInputRect, &looseRow2X, row2Y, 210.0f);
    setNextButton(&this->buttonLoosePreviewTotalDurationMsInputRect, &looseRow2X, row2Y, 290.0f);
    setNextButton(&this->buttonLooseClearAllVfxRect, &looseRow2X, row2Y, 160.0f);
    if (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
    {
        setNextButton(&this->buttonLoosePreviewPlacementSnapRect, &looseRow2X, row2Y, 230.0f);
    }
    else
    {
        this->buttonLoosePreviewPlacementSnapRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    // FLIP V/H, SHARED DIR, OVERRIDE DIR, CENTER VFX : panneau Layers uniquement (plus de ligne 2/3 bas-droite).
    this->buttonLayerNameInputRect = SDL_FRect{};

    this->shipListRect.w = 250.0f;
    this->shipListRect.h = 276.0f;
    this->shipListRect.x = map.rect.x + map.rect.w - this->shipListRect.w - 40.0f;
    this->shipListRect.y = map.rect.y + map.rect.h - this->shipListRect.h - 40.0f;
    {
        float duplicateButtonY = this->shipListRect.y + this->shipListRect.h + 4.0f;
        if (duplicateButtonY + h > map.rect.y + map.rect.h - 2.0f)
        {
            duplicateButtonY = this->shipListRect.y - h - 4.0f;
        }
        this->buttonShipVfxDuplicateShipsRect = SDL_FRect{
            this->shipListRect.x,
            duplicateButtonY,
            this->shipListRect.w,
            h};
    }

    this->sfxListRect = this->shipListRect;
    this->sfxListRect.x = this->shipListRect.x - this->sfxListRect.w - 16.0f;
    this->sfxListRect.x = (std::max)(this->sfxListRect.x, map.rect.x + 12.0f);
    {
        const float actionGap = 8.0f;
        const float actionW = (this->sfxListRect.w - actionGap) * 0.5f;
        float actionY = this->sfxListRect.y + this->sfxListRect.h + 4.0f;
        if (actionY + h > map.rect.y + map.rect.h - 2.0f)
        {
            actionY = this->sfxListRect.y - h - 4.0f;
        }
        this->sfxListActionAutoImportRect = SDL_FRect{
            this->sfxListRect.x,
            actionY,
            actionW,
            h};
        this->sfxListActionAddInstanceRect = SDL_FRect{
            this->sfxListRect.x + actionW + actionGap,
            actionY,
            actionW,
            h};
    }

    this->layerListRect = this->shipListRect;
    this->layerListRect.w = 1040.0f;
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
        this->layerListRect.w * 0.5f,
        invalidPanelHeight};
    this->invalidVfxListRect = SDL_FRect{
        this->layerListRect.x,
        this->invalidShipListRect.y - invalidPanelHeight - invalidPanelGap,
        this->layerListRect.w * 0.5f,
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

    constexpr float kPadX = 5.0f;
    const float innerW = (std::max)(rect.w - (kPadX * 2.0f), 1.0f);
    float textX = rect.x + kPadX + ((innerW - static_cast<float>(textW)) * 0.5f);
    if (textX < rect.x + kPadX)
    {
        textX = rect.x + kPadX;
    }
    const float textY = rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        SDL_Rect clipRect{};
        clipRect.x = static_cast<int>(std::floor(rect.x));
        clipRect.y = static_cast<int>(std::floor(rect.y));
        const int clipR = static_cast<int>(std::ceil(rect.x + rect.w));
        const int clipB = static_cast<int>(std::ceil(rect.y + rect.h));
        clipRect.w = (std::max)(clipR - clipRect.x, 1);
        clipRect.h = (std::max)(clipB - clipRect.y, 1);
        SDL_SetRenderClipRect(renderer, &clipRect);
    }

    rc2d_graphics_drawText(&text, textX, textY);
    rc2d_graphics_destroyText(&text);

    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }
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
    const float rowGap = kLayerListPanelRowGap;
    const float rowsTopY = this->layerListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->layerListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = this->layerListRect.x + panelPadding;
    const float rowsWidth = this->layerListRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    const ShipVfxLayerPagePickerLayout pagePickHeader = buildShipVfxLayerPagePickerLayout(
        this->layerListRect,
        panelPadding,
        headerHeight,
        false,
        this->shipVfxEffectiveLayerPageCount());

    if (this->overlayFont.sdl_font != nullptr)
    {
        char pageLab[200] = {};
        shipVfxLayerPageLabelUtf8(
            this->getShipVfxLayerPageKey(),
            this->shipVfxEditorTargetSectorsABEnabled,
            pageLab,
            sizeof(pageLab));
        char headerLine[280] = {};
        const size_t plen = std::strlen(pageLab);
        if (plen > 52U)
        {
            SDL_snprintf(headerLine, sizeof(headerLine), "LAYERS (%.52s...)", pageLab);
        }
        else
        {
            SDL_snprintf(headerLine, sizeof(headerLine), "LAYERS (%s)", pageLab);
        }
        RC2D_Text headerText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), headerLine);
        headerText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&headerText);
        int hw = 0;
        int hh = 0;
        rc2d_graphics_getTextSize(&headerText, &hw, &hh);
        const float maxTextRight = pagePickHeader.targetSectorsOverlayToggleRect.x - 4.0f;
        float drawX = this->layerListRect.x + panelPadding;
        if (maxTextRight > drawX && static_cast<float>(hw) > (maxTextRight - drawX))
        {
            drawX = (std::max)(this->layerListRect.x + panelPadding, maxTextRight - static_cast<float>(hw));
        }
        rc2d_graphics_drawText(&headerText, drawX, this->layerListRect.y + 1.0f);
        rc2d_graphics_destroyText(&headerText);
    }

    rc2d_graphics_setColor(
        !this->shipVfxEditorTargetSectorsABEnabled
            ? RC2D_Color{42, 48, 56, 200}
            : (this->shipVfxEditorTargetSectorsOverlayVisible ? RC2D_Color{72, 108, 120, 230}
                                                              : RC2D_Color{52, 72, 96, 230}));
    rc2d_graphics_rectangle("fill", &pagePickHeader.targetSectorsOverlayToggleRect);
    rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
    rc2d_graphics_rectangle("line", &pagePickHeader.targetSectorsOverlayToggleRect);
    if (this->overlayFont.sdl_font != nullptr)
    {
        const char* ovlLab = !this->shipVfxEditorTargetSectorsABEnabled
            ? "off"
            : (this->shipVfxEditorTargetSectorsOverlayVisible ? "TRC" : "trc");
        RC2D_Text ovlText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), ovlLab);
        ovlText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&ovlText);
        int ow = 0;
        int oh = 0;
        rc2d_graphics_getTextSize(&ovlText, &ow, &oh);
        rc2d_graphics_drawText(
            &ovlText,
            pagePickHeader.targetSectorsOverlayToggleRect.x +
                ((pagePickHeader.targetSectorsOverlayToggleRect.w - static_cast<float>(ow)) * 0.5f),
            pagePickHeader.targetSectorsOverlayToggleRect.y + 1.0f);
        rc2d_graphics_destroyText(&ovlText);
    }
    rc2d_graphics_setColor(
        this->shipVfxEditorTargetSectorsABEnabled ? RC2D_Color{52, 92, 86, 230} : RC2D_Color{52, 72, 96, 230});
    rc2d_graphics_rectangle("fill", &pagePickHeader.targetSectorsToggleRect);
    rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
    rc2d_graphics_rectangle("line", &pagePickHeader.targetSectorsToggleRect);
    if (this->overlayFont.sdl_font != nullptr)
    {
        const char* abLab = this->shipVfxEditorTargetSectorsABEnabled ? "A/B" : "8pg";
        RC2D_Text abText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), abLab);
        abText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&abText);
        int abW = 0;
        int abH = 0;
        rc2d_graphics_getTextSize(&abText, &abW, &abH);
        rc2d_graphics_drawText(
            &abText,
            pagePickHeader.targetSectorsToggleRect.x +
                ((pagePickHeader.targetSectorsToggleRect.w - static_cast<float>(abW)) * 0.5f),
            pagePickHeader.targetSectorsToggleRect.y + 1.0f);
        rc2d_graphics_destroyText(&abText);
    }
    rc2d_graphics_setColor(RC2D_Color{52, 72, 96, 230});
    rc2d_graphics_rectangle("fill", &pagePickHeader.duplicateToPagesButtonRect);
    rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
    rc2d_graphics_rectangle("line", &pagePickHeader.duplicateToPagesButtonRect);
    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text dupPagesText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "DUP+");
        dupPagesText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&dupPagesText);
        int dpW = 0;
        int dpH = 0;
        rc2d_graphics_getTextSize(&dupPagesText, &dpW, &dpH);
        rc2d_graphics_drawText(
            &dupPagesText,
            pagePickHeader.duplicateToPagesButtonRect.x +
                ((pagePickHeader.duplicateToPagesButtonRect.w - static_cast<float>(dpW)) * 0.5f),
            pagePickHeader.duplicateToPagesButtonRect.y + 1.0f);
        rc2d_graphics_destroyText(&dupPagesText);
    }
    rc2d_graphics_setColor(RC2D_Color{48, 56, 68, 230});
    rc2d_graphics_rectangle("fill", &pagePickHeader.pageButtonRect);
    rc2d_graphics_setColor(RC2D_Color{130, 145, 162, 230});
    rc2d_graphics_rectangle("line", &pagePickHeader.pageButtonRect);
    if (this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text pagesBtnText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "PAGES");
        pagesBtnText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&pagesBtnText);
        rc2d_graphics_drawText(&pagesBtnText, pagePickHeader.pageButtonRect.x + 6.0f, pagePickHeader.pageButtonRect.y + 1.0f);
        rc2d_graphics_destroyText(&pagesBtnText);
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

    float layerRelTooltipMx = 0.0f;
    float layerRelTooltipMy = 0.0f;
    const bool layerRelTooltipMouseOk = this->getMouseRenderPosition(&layerRelTooltipMx, &layerRelTooltipMy);
    std::string layerRelTooltipMsText;
    SDL_FRect layerRelTooltipButtonRect{};
    bool layerRelTooltipHave = false;

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

        const int rowCode = orderedLayerIndices[static_cast<size_t>(displayIndex)];
        const bool isTrailPieceSubRow = layerPanelRowIsTrailPieceSubRow(rowCode);
        const bool isShipRow = (rowCode == -1);
        if (isTrailPieceSubRow)
        {
            const uint32_t pieceUi = layerPanelTrailPieceUiIdFromRow(rowCode);
            const int pieceIdx = this->findTrailPieceIndexByLayerPanelUiId(pieceUi);
            if (pieceIdx < 0)
            {
                continue;
            }
            const ShipVfxTrailPiece& tp = this->currentShipVfxTrailPieces()[static_cast<size_t>(pieceIdx)];
            const int parentIx = this->findVfxLayerIndexByInstanceId(tp.sourceVfxInstanceId);
            if (parentIx < 0)
            {
                continue;
            }
            const ShipVfxInstance& pinst = this->currentShipVfxLayers()[static_cast<size_t>(parentIx)];
            (void)pinst;
            const bool isDraggedSub = this->layerRowDragActive && (displayIndex == this->layerRowDragSourceDisplayIndex);
            const bool isSelectedSub = (displayIndex == selectedIndex);
            RC2D_Color subFill = RC2D_Color{40, 48, 58, 218};
            if (isDraggedSub)
            {
                subFill = RC2D_Color{62, 94, 126, 220};
            }
            else if (isSelectedSub)
            {
                subFill = kRowSelectedFillColor;
            }
            rc2d_graphics_setColor(subFill);
            rc2d_graphics_rectangle("fill", &rowRect);
            rc2d_graphics_setColor(kRowBorderColor);
            rc2d_graphics_rectangle("line", &rowRect);

            if (this->overlayFont.sdl_font != nullptr)
            {
                char subBuf[200] = {};
                SDL_snprintf(
                    subBuf,
                    sizeof(subBuf),
                    "      Sous-instance : rejet trainee (animation \"%s\", reste %.1f s)",
                    pinst.label.c_str(),
                    (std::max)(tp.timeRemainingSec, 0.0f));
                RC2D_Text rowText = rc2d_graphics_createText(
                    const_cast<RC2D_Font*>(&this->overlayFont),
                    makeAssetLabel(subBuf, 80).c_str());
                rowText.color = RC2D_Color{176, 188, 202, 240};
                rc2d_graphics_setTextColor(&rowText);
                int rowTextW = 0;
                int rowTextH = 0;
                rc2d_graphics_getTextSize(&rowText, &rowTextW, &rowTextH);
                rc2d_graphics_drawText(
                    &rowText,
                    rowRect.x + 28.0f,
                    rowRect.y + (std::max)((rowRect.h - static_cast<float>(rowTextH)) * 0.5f, 1.0f));
                rc2d_graphics_destroyText(&rowText);
            }
            continue;
        }

        const int instanceIndex = rowCode;
        int drawOrder = this->activeShipDrawOrder();
        bool visible = this->activeShipLayerVisible();
        bool debugBoundsVisible = this->activeShipDebugBoundsVisible();
        bool locked = this->activeShipLayerLocked();
        std::string label = "SHIP";
        bool flipHResolved = false;
        bool flipVResolved = false;

        if (!isShipRow)
        {
            const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
            const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
            drawOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
            visible = (override != nullptr) ? override->visible : instance.visible;
            debugBoundsVisible = instance.debugBoundsVisible;
            locked = instance.locked;
            label = instance.label;
            flipHResolved = (override != nullptr) ? override->flipHorizontal : instance.flipHorizontal;
            flipVResolved = (override != nullptr) ? override->flipVertical : instance.flipVertical;
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

        SDL_FRect vfxPlaceModeRect{};
        vfxPlaceModeRect.w = kLayerRowVfxPlaceModeButtonW;
        vfxPlaceModeRect.h = lockRect.h;
        vfxPlaceModeRect.x = debugRect.x - vfxPlaceModeRect.w - 4.0f;
        vfxPlaceModeRect.y = lockRect.y;

        SDL_FRect layerRotateDialRect{};
        layerRotateDialRect.w = kLayerRowVfxRotateDialButtonW;
        layerRotateDialRect.h = lockRect.h;
        layerRotateDialRect.x = vfxPlaceModeRect.x - layerRotateDialRect.w - 4.0f;
        layerRotateDialRect.y = lockRect.y;

        SDL_FRect layerFlipHRowRect{};
        layerFlipHRowRect.w = kLayerRowFlipButtonW;
        layerFlipHRowRect.h = lockRect.h;
        layerFlipHRowRect.x = layerRotateDialRect.x - layerFlipHRowRect.w - 4.0f;
        layerFlipHRowRect.y = lockRect.y;

        SDL_FRect layerFlipVRowRect{};
        layerFlipVRowRect.w = kLayerRowFlipButtonW;
        layerFlipVRowRect.h = lockRect.h;
        layerFlipVRowRect.x = layerFlipHRowRect.x - layerFlipVRowRect.w - 4.0f;
        layerFlipVRowRect.y = lockRect.y;

        SDL_FRect layerRelativeRect{};
        layerRelativeRect.w = kLayerRowRelativeButtonW;
        layerRelativeRect.h = lockRect.h;
        layerRelativeRect.x = layerFlipVRowRect.x - layerRelativeRect.w - 4.0f;
        layerRelativeRect.y = lockRect.y;

        SDL_FRect layerMotionSpawnCibleRect{};
        layerMotionSpawnCibleRect.w = kLayerRowMotionSpawnCibleToggleW;
        layerMotionSpawnCibleRect.h = lockRect.h;
        layerMotionSpawnCibleRect.x = layerRelativeRect.x - kLayerRowMotionClusterGap - layerMotionSpawnCibleRect.w;
        layerMotionSpawnCibleRect.y = lockRect.y;

        const ShipVfxInstance* motionRowInst =
            isShipRow ? nullptr : &this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];

        rc2d_graphics_setColor(
            isShipRow
                ? RC2D_Color{50, 58, 68, 220}
                : ((motionRowInst != nullptr && motionRowInst->motionSpawnCaptured)
                       ? RC2D_Color{72, 108, 84, 220}
                       : RC2D_Color{56, 62, 72, 220}));
        rc2d_graphics_rectangle("fill", &layerMotionSpawnCibleRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &layerMotionSpawnCibleRect);

        bool relLinkActive = false;
        if (!isShipRow)
        {
            relLinkActive =
                this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)].spawnAfterInstanceId != 0U;
        }

        rc2d_graphics_setColor(
            relLinkActive ? RC2D_Color{72, 96, 72, 220} : RC2D_Color{52, 62, 74, 220});
        rc2d_graphics_rectangle("fill", &layerRelativeRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &layerRelativeRect);

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

        const bool vfxPlaceTileMode =
            !isShipRow && this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)].placementSnapClickToTile;
        const bool rotDialRowActive =
            !isShipRow && this->vfxRotationDialActive &&
            (instanceIndex == this->selectedVfxInstanceIndex);
        rc2d_graphics_setColor(
            isShipRow
                ? RC2D_Color{50, 58, 68, 220}
                : (flipVResolved ? RC2D_Color{96, 112, 84, 220} : RC2D_Color{52, 62, 74, 220}));
        rc2d_graphics_rectangle("fill", &layerFlipVRowRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &layerFlipVRowRect);

        rc2d_graphics_setColor(
            isShipRow
                ? RC2D_Color{50, 58, 68, 220}
                : (flipHResolved ? RC2D_Color{96, 112, 84, 220} : RC2D_Color{52, 62, 74, 220}));
        rc2d_graphics_rectangle("fill", &layerFlipHRowRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &layerFlipHRowRect);

        rc2d_graphics_setColor(
            isShipRow
                ? RC2D_Color{50, 58, 68, 220}
                : (rotDialRowActive ? RC2D_Color{88, 108, 138, 220} : RC2D_Color{56, 70, 88, 220}));
        rc2d_graphics_rectangle("fill", &layerRotateDialRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &layerRotateDialRect);

        rc2d_graphics_setColor(
            isShipRow
                ? RC2D_Color{50, 58, 68, 220}
                : (vfxPlaceTileMode ? RC2D_Color{72, 108, 128, 220} : RC2D_Color{56, 62, 72, 220}));
        rc2d_graphics_rectangle("fill", &vfxPlaceModeRect);
        rc2d_graphics_setColor(RC2D_Color{162, 182, 200, 230});
        rc2d_graphics_rectangle("line", &vfxPlaceModeRect);

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

            const char* placeLabel = isShipRow ? "-" : (vfxPlaceTileMode ? "TILE" : "PIXEL");
            RC2D_Text placeText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), placeLabel);
            placeText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&placeText);
            int placeW = 0;
            int placeH = 0;
            rc2d_graphics_getTextSize(&placeText, &placeW, &placeH);
            rc2d_graphics_drawText(
                &placeText,
                vfxPlaceModeRect.x + ((vfxPlaceModeRect.w - static_cast<float>(placeW)) * 0.5f),
                vfxPlaceModeRect.y + ((vfxPlaceModeRect.h - static_cast<float>(placeH)) * 0.5f));
            rc2d_graphics_destroyText(&placeText);

            const char* rotLabel = isShipRow ? "-" : "ROT";
            RC2D_Text rotText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), rotLabel);
            rotText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&rotText);
            int rotW = 0;
            int rotH = 0;
            rc2d_graphics_getTextSize(&rotText, &rotW, &rotH);
            rc2d_graphics_drawText(
                &rotText,
                layerRotateDialRect.x + ((layerRotateDialRect.w - static_cast<float>(rotW)) * 0.5f),
                layerRotateDialRect.y + ((layerRotateDialRect.h - static_cast<float>(rotH)) * 0.5f));
            rc2d_graphics_destroyText(&rotText);

            const char* motionSpawnLabel = isShipRow ? "-" : "SPAWN";
            RC2D_Text motionScText =
                rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), motionSpawnLabel);
            motionScText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&motionScText);
            int mScW = 0;
            int mScH = 0;
            rc2d_graphics_getTextSize(&motionScText, &mScW, &mScH);
            rc2d_graphics_drawText(
                &motionScText,
                layerMotionSpawnCibleRect.x + ((layerMotionSpawnCibleRect.w - static_cast<float>(mScW)) * 0.5f),
                layerMotionSpawnCibleRect.y + ((layerMotionSpawnCibleRect.h - static_cast<float>(mScH)) * 0.5f));
            rc2d_graphics_destroyText(&motionScText);

            if (isShipRow)
            {
                RC2D_Text relShipDash = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "-");
                relShipDash.color = kHudTextColor;
                rc2d_graphics_setTextColor(&relShipDash);
                int sdW = 0;
                int sdH = 0;
                rc2d_graphics_getTextSize(&relShipDash, &sdW, &sdH);
                rc2d_graphics_drawText(
                    &relShipDash,
                    layerRelativeRect.x + ((layerRelativeRect.w - static_cast<float>(sdW)) * 0.5f),
                    layerRelativeRect.y + ((layerRelativeRect.h - static_cast<float>(sdH)) * 0.5f));
                rc2d_graphics_destroyText(&relShipDash);

                if (layerRelTooltipMouseOk)
                {
                    const size_t pk = static_cast<size_t>(this->getShipVfxLayerPageKey());
                    if (this->shipSpawnAfterVfxInstanceId[pk] != 0U &&
                        this->pointInRect(layerRelTooltipMx, layerRelTooltipMy, layerRelativeRect))
                    {
                        layerRelTooltipMsText = std::to_string(this->shipSpawnAfterDelayMs[pk]) + " ms";
                        layerRelTooltipButtonRect = layerRelativeRect;
                        layerRelTooltipHave = true;
                    }
                }
            }
            else
            {
                const ShipVfxInstance& relInst = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
                RC2D_Text relHdrOnly = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "RELATIF");
                relHdrOnly.color = kHudTextColor;
                rc2d_graphics_setTextColor(&relHdrOnly);
                int hoW = 0;
                int hoH = 0;
                rc2d_graphics_getTextSize(&relHdrOnly, &hoW, &hoH);
                rc2d_graphics_drawText(
                    &relHdrOnly,
                    layerRelativeRect.x + ((layerRelativeRect.w - static_cast<float>(hoW)) * 0.5f),
                    layerRelativeRect.y + ((layerRelativeRect.h - static_cast<float>(hoH)) * 0.5f));
                rc2d_graphics_destroyText(&relHdrOnly);

                if (layerRelTooltipMouseOk && relInst.spawnAfterInstanceId != 0U &&
                    this->pointInRect(layerRelTooltipMx, layerRelTooltipMy, layerRelativeRect))
                {
                    layerRelTooltipMsText = std::to_string(relInst.spawnAfterDelayMs) + " ms";
                    layerRelTooltipButtonRect = layerRelativeRect;
                    layerRelTooltipHave = true;
                }
            }

            const char* flipVLabel = isShipRow ? "-" : "FLIP V";
            RC2D_Text flipVText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), flipVLabel);
            flipVText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&flipVText);
            int flipVW = 0;
            int flipVH = 0;
            rc2d_graphics_getTextSize(&flipVText, &flipVW, &flipVH);
            rc2d_graphics_drawText(
                &flipVText,
                layerFlipVRowRect.x + ((layerFlipVRowRect.w - static_cast<float>(flipVW)) * 0.5f),
                layerFlipVRowRect.y + ((layerFlipVRowRect.h - static_cast<float>(flipVH)) * 0.5f));
            rc2d_graphics_destroyText(&flipVText);

            const char* flipHLabel = isShipRow ? "-" : "FLIP H";
            RC2D_Text flipHText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), flipHLabel);
            flipHText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&flipHText);
            int flipHW = 0;
            int flipHH = 0;
            rc2d_graphics_getTextSize(&flipHText, &flipHW, &flipHH);
            rc2d_graphics_drawText(
                &flipHText,
                layerFlipHRowRect.x + ((layerFlipHRowRect.w - static_cast<float>(flipHW)) * 0.5f),
                layerFlipHRowRect.y + ((layerFlipHRowRect.h - static_cast<float>(flipHH)) * 0.5f));
            rc2d_graphics_destroyText(&flipHText);

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

            const bool behind = (drawOrder < this->activeShipDrawOrder());
            const float indent = isShipRow ? 18.0f : (behind ? 8.0f : 34.0f);
            const float labelX = rowRect.x + indent;
            const float labelWidth = (layerMotionSpawnCibleRect.x - 6.0f) - labelX;
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

    if (layerRelTooltipHave && !layerRelTooltipMsText.empty() && this->overlayFont.sdl_font != nullptr)
    {
        RC2D_Text tipText =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), layerRelTooltipMsText.c_str());
        tipText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&tipText);
        int tipW = 0;
        int tipH = 0;
        rc2d_graphics_getTextSize(&tipText, &tipW, &tipH);
        constexpr float kRelTipPad = 5.0f;
        SDL_FRect tipBg{};
        tipBg.w = static_cast<float>(tipW) + kRelTipPad * 2.0f;
        tipBg.h = static_cast<float>(tipH) + kRelTipPad * 2.0f;
        tipBg.x = layerRelTooltipButtonRect.x + ((layerRelTooltipButtonRect.w - tipBg.w) * 0.5f);
        tipBg.y = layerRelTooltipButtonRect.y - tipBg.h - 3.0f;
        if (tipBg.y < this->layerListRect.y + 2.0f)
        {
            tipBg.y = layerRelTooltipButtonRect.y + layerRelTooltipButtonRect.h + 3.0f;
        }
        if (tipBg.x < this->layerListRect.x + 2.0f)
        {
            tipBg.x = this->layerListRect.x + 2.0f;
        }
        if (tipBg.x + tipBg.w > this->layerListRect.x + this->layerListRect.w - 2.0f)
        {
            tipBg.x = (this->layerListRect.x + this->layerListRect.w - 2.0f) - tipBg.w;
        }
        if (tipBg.y + tipBg.h > this->layerListRect.y + this->layerListRect.h - 2.0f)
        {
            tipBg.y = (this->layerListRect.y + this->layerListRect.h - 2.0f) - tipBg.h;
        }
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{32, 40, 52, 250});
        rc2d_graphics_rectangle("fill", &tipBg);
        rc2d_graphics_setColor(RC2D_Color{150, 175, 198, 245});
        rc2d_graphics_rectangle("line", &tipBg);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        rc2d_graphics_drawText(&tipText, tipBg.x + kRelTipPad, tipBg.y + kRelTipPad);
        rc2d_graphics_destroyText(&tipText);
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

    if (this->shipVfxLayerPagePickerOpen)
    {
        const int pickRows = this->shipVfxEffectiveLayerPageCount();
        const ShipVfxLayerPagePickerLayout pagePick = buildShipVfxLayerPagePickerLayout(
            this->layerListRect,
            panelPadding,
            headerHeight,
            true,
            pickRows);
        rc2d_graphics_setColor(RC2D_Color{10, 14, 18, 240});
        rc2d_graphics_rectangle("fill", &pagePick.popupRect);
        rc2d_graphics_setColor(RC2D_Color{150, 165, 182, 240});
        rc2d_graphics_rectangle("line", &pagePick.popupRect);
        const int curPage = this->getShipVfxLayerPageKey();
        for (int pi = 0; pi < pickRows; ++pi)
        {
            const bool isActive = (pi == curPage);
            rc2d_graphics_setColor(isActive ? RC2D_Color{72, 108, 152, 235} : RC2D_Color{38, 48, 60, 220});
            rc2d_graphics_rectangle("fill", &pagePick.pageRowRects[pi]);
            rc2d_graphics_setColor(RC2D_Color{120, 135, 152, 230});
            rc2d_graphics_rectangle("line", &pagePick.pageRowRects[pi]);
            if (this->overlayFont.sdl_font != nullptr)
            {
                char rowBuf[160] = {};
                shipVfxLayerPageLabelUtf8(pi, this->shipVfxEditorTargetSectorsABEnabled, rowBuf, sizeof(rowBuf));
                RC2D_Text rowPickText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), rowBuf);
                rowPickText.color = kHudTextColor;
                rc2d_graphics_setTextColor(&rowPickText);
                rc2d_graphics_drawText(&rowPickText, pagePick.pageRowRects[pi].x + 4.0f, pagePick.pageRowRects[pi].y + 1.0f);
                rc2d_graphics_destroyText(&rowPickText);
            }
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

    const std::vector<int> displayRowsForDrag =
        this->expandLayerPanelDisplayRows(this->getOrderedVfxInstanceIndicesForLayerPanel());
    const int itemCount = static_cast<int>(displayRowsForDrag.size());
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
        int blockEnd = sourceDisplayIndex;
        const int parentCode = displayRowsForDrag[static_cast<size_t>(sourceDisplayIndex)];
        if (parentCode >= 0)
        {
            const uint32_t parentInstId =
                this->currentShipVfxLayers()[static_cast<size_t>(parentCode)].instanceId;
            while (blockEnd + 1 < itemCount)
            {
                const int nxt = displayRowsForDrag[static_cast<size_t>(blockEnd + 1)];
                if (!layerPanelRowIsTrailPieceSubRow(nxt))
                {
                    break;
                }
                const int pi = this->findTrailPieceIndexByLayerPanelUiId(layerPanelTrailPieceUiIdFromRow(nxt));
                if (pi < 0 ||
                    this->currentShipVfxTrailPieces()[static_cast<size_t>(pi)].sourceVfxInstanceId != parentInstId)
                {
                    break;
                }
                blockEnd += 1;
            }
        }
        const int sourceInsertIndex = blockEnd + 1;
        const int targetInsertIndex = std::clamp(this->layerRowDragTargetInsertIndex, 0, itemCount);

        this->layerRowDragActive = false;
        this->layerRowDragSourceDisplayIndex = -1;
        this->layerRowDragStartMouseY = 0.0f;

        if (this->layerRowDragMoved && targetInsertIndex != sourceInsertIndex)
        {
            this->applyLayerPanelReorderFromDisplayDrag(sourceDisplayIndex, targetInsertIndex);
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
    const float rowGap = kLayerListPanelRowGap;
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

void EditorMapVfxScene::applyLayerPanelReorderFromDisplayDrag(int sourceDisplayIndex, int targetInsertIndex)
{
    const std::vector<int> core0 = this->getOrderedVfxInstanceIndicesForLayerPanel();
    std::vector<int> display = this->expandLayerPanelDisplayRows(core0);
    const int n = static_cast<int>(display.size());
    if (n <= 1)
    {
        return;
    }
    if (sourceDisplayIndex < 0 || sourceDisplayIndex >= n)
    {
        return;
    }
    if (layerPanelRowIsTrailPieceSubRow(display[static_cast<size_t>(sourceDisplayIndex)]))
    {
        return;
    }

    int blockStart = sourceDisplayIndex;
    int blockEnd = sourceDisplayIndex;
    const int parentInstIdx = display[static_cast<size_t>(blockStart)];
    if (parentInstIdx >= 0)
    {
        const uint32_t parentId =
            this->currentShipVfxLayers()[static_cast<size_t>(parentInstIdx)].instanceId;
        while (blockEnd + 1 < n)
        {
            const int nxt = display[static_cast<size_t>(blockEnd + 1)];
            if (!layerPanelRowIsTrailPieceSubRow(nxt))
            {
                break;
            }
            const int pi =
                this->findTrailPieceIndexByLayerPanelUiId(layerPanelTrailPieceUiIdFromRow(nxt));
            if (pi < 0 ||
                this->currentShipVfxTrailPieces()[static_cast<size_t>(pi)].sourceVfxInstanceId != parentId)
            {
                break;
            }
            blockEnd += 1;
        }
    }
    const int L = blockEnd - blockStart + 1;

    const int tIns = std::clamp(targetInsertIndex, 0, n);
    std::vector<int> block(display.begin() + blockStart, display.begin() + blockEnd + 1);
    display.erase(display.begin() + blockStart, display.begin() + blockEnd + 1);

    int insNew = tIns;
    if (tIns > blockEnd)
    {
        insNew = tIns - L;
    }
    else if (tIns > blockStart)
    {
        insNew = blockStart;
    }
    insNew = std::clamp(insNew, 0, static_cast<int>(display.size()));
    display.insert(display.begin() + insNew, block.begin(), block.end());

    std::vector<int> rowToInstanceIndex;
    rowToInstanceIndex.reserve(display.size());
    for (const int code : display)
    {
        if (!layerPanelRowIsTrailPieceSubRow(code))
        {
            rowToInstanceIndex.push_back(code);
        }
    }

    auto shipIt = std::find(rowToInstanceIndex.begin(), rowToInstanceIndex.end(), -1);
    if (shipIt == rowToInstanceIndex.end())
    {
        return;
    }
    const int shipDisplayIndex = static_cast<int>(std::distance(rowToInstanceIndex.begin(), shipIt));
    this->activeShipDrawOrder() = 0;

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

        ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
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

    for (ShipVfxInstance& instance : this->currentShipVfxLayers())
    {
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        const int effectiveOrder = (override != nullptr) ? override->drawOrder : instance.drawOrder;
        instance.behindShip = (effectiveOrder < this->activeShipDrawOrder());
    }

    const std::vector<int> expandedAfter =
        this->expandLayerPanelDisplayRows(rowToInstanceIndex);
    const int selectedRow = this->getSelectedLayerRowIndexForDisplay(expandedAfter);
    this->ensureSelectionVisible(selectedRow, &this->layerListScrollOffset, static_cast<int>(expandedAfter.size()));
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

void EditorMapVfxScene::openImportShipVfxConfigDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kJsonFileFilters;
    options.num_filters = static_cast<int>(std::size(kJsonFileFilters));
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Importer JSON VFX/Ship existant";
    options.accept_label = "Importer";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFile(&EditorMapVfxScene::onImportShipVfxConfigDialogResult, this, &options);
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
    if (this->pendingExportMode == EditorMode::LOOSE_SPRITES)
    {
        options.title =
            "Dossier parent d'export (ex: assets/images/vfxship ou assets/images/ships/nom-navire)";
    }
    else
    {
        options.title = "Choisir le dossier d'export JSON VFX";
    }
    options.accept_label = "Exporter";
    options.cancel_label = "Annuler";
    rc2d_filedialog_openFolder(&EditorMapVfxScene::onExportFolderDialogResult, this, &options);
}

void EditorMapVfxScene::openExportConfirmPopup(ExportConfirmAction action)
{
    this->exportConfirmPopupAction = action;
    this->exportConfirmPopupVisible = (action != ExportConfirmAction::NONE);
    if (!this->exportConfirmPopupVisible)
    {
        return;
    }
    this->statusMessage = "Export: confirmation requise.";
}

void EditorMapVfxScene::closeExportConfirmPopup(void)
{
    this->exportConfirmPopupVisible = false;
    this->exportConfirmPopupAction = ExportConfirmAction::NONE;
}

bool EditorMapVfxScene::computeExportConfirmPopupLayout(ExportConfirmPopupLayout* out) const
{
    if (out == nullptr)
    {
        return false;
    }

    out->dimFullMap = GetCurrentMap().rect;

    const float popupW = std::clamp(out->dimFullMap.w - 220.0f, 460.0f, 860.0f);
    const float popupH = 180.0f;
    out->popup = SDL_FRect{
        out->dimFullMap.x + ((out->dimFullMap.w - popupW) * 0.5f),
        out->dimFullMap.y + ((out->dimFullMap.h - popupH) * 0.5f),
        popupW,
        popupH};

    constexpr float btnW = 170.0f;
    constexpr float btnH = 34.0f;
    constexpr float gap = 14.0f;
    const float rowY = out->popup.y + out->popup.h - btnH - 16.0f;
    const float totalW = (btnW * 2.0f) + gap;
    const float startX = out->popup.x + ((out->popup.w - totalW) * 0.5f);
    out->validateBtn = SDL_FRect{startX, rowY, btnW, btnH};
    out->cancelBtn = SDL_FRect{startX + btnW + gap, rowY, btnW, btnH};
    return true;
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
    this->loosePreviewTotalDurationMsInputFocused = false;
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

void EditorMapVfxScene::processPendingShipVfxConfigRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    std::string jsonPath;
    {
        std::lock_guard<std::mutex> lock(this->pendingShipVfxConfigMutex);
        hasResult = this->pendingShipVfxConfigDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingShipVfxConfigDialogCanceled;
            jsonPath.swap(this->pendingShipVfxConfigAbsolutePath);
            this->pendingShipVfxConfigDialogCompleted = false;
            this->pendingShipVfxConfigDialogCanceled = false;
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled || jsonPath.empty())
    {
        this->statusMessage = "Import JSON VFX/Ship annule.";
        return;
    }

    if (!this->importShipVfxConfigFromPath(jsonPath.c_str()))
    {
        this->statusMessage = "Import JSON VFX/Ship invalide: " + jsonPath;
        return;
    }

    this->loadedShipVfxConfigPath = normalizePathSlashes(jsonPath);
    this->statusMessage = "Configuration VFX importee: " + this->loadedShipVfxConfigPath;
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

void EditorMapVfxScene::processLooseFolderImportBatch(void)
{
    if (!this->looseImportBatchActive)
    {
        return;
    }

    const size_t totalCount = this->looseImportBatchFolderPaths.size();
    if (this->looseImportBatchNextIndex >= totalCount)
    {
        this->looseImportBatchActive = false;
        this->looseImportBatchFolderPaths.clear();
        this->looseImportBatchNextIndex = 0U;
        this->looseImportBatchAddedCount = 0;
        this->looseImportBatchReloadedCount = 0;
        this->looseImportBatchFailedCount = 0;
        return;
    }

    constexpr size_t kMaxLooseFoldersPerFrame = 1;
    size_t processedThisFrame = 0;
    while (processedThisFrame < kMaxLooseFoldersPerFrame &&
           this->looseImportBatchNextIndex < totalCount)
    {
        const std::string& folderPath = this->looseImportBatchFolderPaths[this->looseImportBatchNextIndex];
        const std::string targetPathKey = makePathKeyLower(folderPath);

        bool alreadyImported = false;
        for (const ImportedLooseFolder& importedFolder : this->importedLooseFolders)
        {
            if (makePathKeyLower(importedFolder.folderAbsolutePath) == targetPathKey)
            {
                alreadyImported = true;
                break;
            }
        }

        bool importOk = false;
        if (alreadyImported)
        {
            importOk = this->reloadImportedLooseFolderFromAbsolutePath(folderPath.c_str());
            if (importOk)
            {
                this->looseImportBatchReloadedCount += 1;
            }
        }
        else
        {
            importOk = this->importLooseFolderFromAbsolutePath(folderPath.c_str());
            if (importOk)
            {
                this->looseImportBatchAddedCount += 1;
            }
        }

        if (!importOk)
        {
            this->looseImportBatchFailedCount += 1;
        }

        this->looseImportBatchNextIndex += 1;
        processedThisFrame += 1;
    }

    const size_t processedCount = this->looseImportBatchNextIndex;
    if (processedCount < totalCount)
    {
        this->statusMessage =
            "Import sprites: " +
            std::to_string(processedCount) + "/" +
            std::to_string(totalCount) + "...";
        return;
    }

    const int addedCount = this->looseImportBatchAddedCount;
    const int reloadedCount = this->looseImportBatchReloadedCount;
    const int failedCount = this->looseImportBatchFailedCount;
    if (addedCount > 0 || reloadedCount > 0)
    {
        std::string msg;
        if (addedCount > 0)
        {
            msg += std::to_string(addedCount) + " dossier(s) importe(s)";
        }
        if (reloadedCount > 0)
        {
            if (!msg.empty())
            {
                msg += ", ";
            }
            msg += std::to_string(reloadedCount) + " recharge(s) depuis le disque";
        }
        if (failedCount > 0)
        {
            msg += " (" + std::to_string(failedCount) + " en echec)";
        }
        msg += ".";
        this->statusMessage = msg;
    }
    else if (failedCount > 0)
    {
        this->statusMessage = "Import sprites: " + std::to_string(failedCount) + " dossier(s) en echec.";
    }
    else
    {
        this->statusMessage = "Aucun dossier sprites PNG valide trouve.";
    }

    this->looseImportBatchActive = false;
    this->looseImportBatchFolderPaths.clear();
    this->looseImportBatchNextIndex = 0U;
    this->looseImportBatchAddedCount = 0;
    this->looseImportBatchReloadedCount = 0;
    this->looseImportBatchFailedCount = 0;
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

    float previewSpeedTilesPerSec = GetGameState().player.getMoveSpeedTilesPerSecond();
    if (!std::isfinite(previewSpeedTilesPerSec) || previewSpeedTilesPerSec <= 0.0f)
    {
        previewSpeedTilesPerSec = 6.0f;
    }
    this->previewShip.setSpeedTilesPerSecond(previewSpeedTilesPerSec);
    this->previewShip.setHealthVisual((this->previewShipStateIndex == 1) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL);
    this->applyPreviewDirectionToShip();
    this->previewShipOpacityPercent = 100;
    this->applyPreviewShipOpacityPercentToShip();
    this->previewShip.setPositionTile(this->previewShipTile.x, this->previewShipTile.y);
    this->previewShipLoaded = true;
    this->loadedShipFolderAbsolute = normalizePathSlashes(folderPath.string());
    this->shipLayerSelected = true;
    this->initDefaultShipLayerSettingsAllPages();
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

    this->clearAllShipVfxLayerPages();
    this->clearShipVfxTrailPieces();
    this->setSelectedVfxInstanceIndex(-1);
    this->nextVfxInstanceId = 1U;
    this->initDefaultShipLayerSettingsAllPages();
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
    bool selectedAnimTotalDurationInfinite = true;
    int selectedAnimTotalDurationMs = 0;

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
                selectedAnimTotalDurationInfinite = parsed.animationTotalDurationInfinite;
                selectedAnimTotalDurationMs = parsed.animationTotalDurationMs;
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

    RC2D_Image image = LoadStorageImage(storageImagePath.c_str(), RC2D_STORAGE_USER);
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
    imported.animationTotalDurationInfinite = selectedAnimTotalDurationInfinite;
    imported.animationTotalDurationMs = selectedAnimTotalDurationMs;

    this->importedSfx.push_back(std::move(imported));
    this->selectedSfxIndex = static_cast<int>(this->importedSfx.size()) - 1;
    this->ensureSelectionVisible(this->selectedSfxIndex, &this->sfxListScrollOffset, static_cast<int>(this->importedSfx.size()));
    this->sfxListActionButtonsVisible = true;
    this->sfxListActionButtonsSfxIndex = this->selectedSfxIndex;
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

    if (this->selectedSfxIndex >= 0 && this->selectedSfxIndex < static_cast<int>(this->importedSfx.size()))
    {
        this->ensureSelectionVisible(this->selectedSfxIndex, &this->sfxListScrollOffset, static_cast<int>(this->importedSfx.size()));
        this->sfxListActionButtonsVisible = true;
        this->sfxListActionButtonsSfxIndex = this->selectedSfxIndex;
    }
    else
    {
        this->sfxListActionButtonsVisible = false;
        this->sfxListActionButtonsSfxIndex = -1;
    }

    if (addedCount <= 0 && this->importedSfx.empty())
    {
        this->statusMessage =
            std::string("Aucun VFX valide detecte dans ") + normalizePathSlashes(rootFolderAbsolutePath) + ".";
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
        RC2D_ImageData src = LoadStorageImageData(sprite.storagePath.c_str(), RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            return;
        }
        sourceSurfaces.push_back(src.sdl_surface);
        src.sdl_surface = nullptr;
        ReleaseStorageImageData(&src);
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

        RC2D_Image image = LoadStorageImage(storagePath.c_str(), RC2D_STORAGE_USER);
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

bool EditorMapVfxScene::reloadImportedLooseFolderFromAbsolutePath(const char* absolutePath)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        return false;
    }

    auto makePathKey = [](const std::string& path) {
        std::string key = normalizePathSlashes(path);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return key;
    };

    const std::string targetKey = makePathKey(std::string(absolutePath));
    int foundIndex = -1;
    for (int i = 0; i < static_cast<int>(this->importedLooseFolders.size()); ++i)
    {
        if (makePathKey(this->importedLooseFolders[static_cast<size_t>(i)].folderAbsolutePath) == targetKey)
        {
            foundIndex = i;
            break;
        }
    }
    if (foundIndex < 0)
    {
        return false;
    }

    std::filesystem::path folderPath(absolutePath);
    std::error_code fsError;
    if (!std::filesystem::exists(folderPath, fsError) || !std::filesystem::is_directory(folderPath, fsError))
    {
        this->statusMessage = "Reimport sprites: dossier introuvable.";
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
        this->statusMessage = "Reimport sprites: aucun PNG dans le dossier.";
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

    ImportedLooseFolder& folder = this->importedLooseFolders[static_cast<size_t>(foundIndex)];
    for (ImportedLooseSprite& sprite : folder.sprites)
    {
        ReleaseStorageImage(&sprite.image);
    }
    folder.sprites.clear();
    folder.looseUnionCropReady = false;
    folder.looseUnionCropX = 0;
    folder.looseUnionCropY = 0;
    folder.looseUnionCropW = 0;
    folder.looseUnionCropH = 0;

    folder.displayName = folderPath.filename().string();
    if (folder.displayName.empty())
    {
        folder.displayName = normalizePathSlashes(folderPath.string());
    }
    folder.folderAbsolutePath = normalizePathSlashes(folderPath.string());

    this->ensureUserStorageFolders();

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

        RC2D_Image image = LoadStorageImage(storagePath.c_str(), RC2D_STORAGE_USER);
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
        this->statusMessage = "Reimport sprites: aucune image chargeable.";
        return false;
    }

    this->refreshLooseFolderUnionCrop(folder);
    this->selectedLooseFolderIndex = foundIndex;
    this->ensureSelectionVisible(this->selectedLooseFolderIndex, &this->looseListScrollOffset, static_cast<int>(this->importedLooseFolders.size()));
    this->statusMessage = "Dossier sprites recharge: " + folder.displayName;
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
        return makePathKeyLower(a.string()) < makePathKeyLower(b.string());
    });

    if (discoveredFolders.empty())
    {
        this->statusMessage = "Aucun dossier sprites PNG valide trouve.";
        return false;
    }

    if (!this->looseImportBatchActive)
    {
        this->looseImportBatchActive = true;
        this->looseImportBatchFolderPaths.clear();
        this->looseImportBatchNextIndex = 0U;
        this->looseImportBatchAddedCount = 0;
        this->looseImportBatchReloadedCount = 0;
        this->looseImportBatchFailedCount = 0;
    }

    std::vector<std::string> queuedPathKeys;
    queuedPathKeys.reserve(this->looseImportBatchFolderPaths.size() + discoveredFolders.size());
    for (const std::string& queuedPath : this->looseImportBatchFolderPaths)
    {
        queuedPathKeys.push_back(makePathKeyLower(queuedPath));
    }

    int queuedCount = 0;
    for (const std::filesystem::path& folderPath : discoveredFolders)
    {
        std::error_code absError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(folderPath, absError);
        const std::string absoluteNorm = normalizePathSlashes((absError ? folderPath : absolutePath).string());
        const std::string absoluteKey = makePathKeyLower(absoluteNorm);
        if (std::find(queuedPathKeys.begin(), queuedPathKeys.end(), absoluteKey) != queuedPathKeys.end())
        {
            continue;
        }

        this->looseImportBatchFolderPaths.push_back(absoluteNorm);
        queuedPathKeys.push_back(absoluteKey);
        queuedCount += 1;
    }

    if (queuedCount <= 0)
    {
        this->statusMessage = "Import sprites: dossiers deja en file d'attente.";
        return true;
    }

    const size_t remainingCount = this->looseImportBatchFolderPaths.size() - this->looseImportBatchNextIndex;
    this->statusMessage =
        "Import sprites: " +
        std::to_string(queuedCount) + " dossier(s) ajoutes (" +
        std::to_string(remainingCount) + " en attente).";
    return true;
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
    const float previewSpawnTimeSeconds = this->resolveCurrentPagePreviewSpawnTimeSeconds();
    ShipVfxInstance instance{};
    instance.instanceId = this->nextVfxInstanceId++;
    int sourceInstanceNumber = 1;
    for (const ShipVfxInstance& existing : this->currentShipVfxLayers())
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
    instance.drawOrder = static_cast<int>(this->currentShipVfxLayers().size()) + 1;
    instance.visible = true;
    instance.debugBoundsVisible = true;
    instance.locked = false;
    instance.behindShip = (instance.drawOrder < this->activeShipDrawOrder());
    instance.followShip = true;
    instance.sharedForAllDirections = true;
    instance.sharedForAllStates = true;
    instance.placementSnapClickToTile = false;
    instance.previewSpawnTimeSeconds = previewSpawnTimeSeconds;
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
    this->currentShipVfxLayers().push_back(instance);
    const int spawnedIndex = static_cast<int>(this->currentShipVfxLayers().size()) - 1;
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    this->setSelectedVfxInstanceIndex(spawnedIndex);
    this->shipLayerSelected = false;
    this->markShipVfxDirty();
    this->statusMessage = "VFX ajoute au centre: " + imported.displayName;
}

void EditorMapVfxScene::setSelectedVfxInstanceIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        this->selectedVfxInstanceIndex = -1;
        this->layerNameInput.clear();
        this->shipLayerSelected = true;
        return;
    }

    this->selectedVfxInstanceIndex = index;
    this->shipLayerSelected = false;
    this->layerNameInput = this->currentShipVfxLayers()[static_cast<size_t>(index)].label;
}

void EditorMapVfxScene::rebuildVfxLayerLabelsFromCurrentInstances(void)
{
    struct SourceCounter
    {
        std::string key;
        int sourceNumber;
        int instanceCount;
    };

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

    for (std::vector<ShipVfxInstance>& page : this->shipVfxLayerPages)
    {
        std::vector<SourceCounter> sourceCounters;
        sourceCounters.reserve(page.size());

        for (ShipVfxInstance& instance : page)
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
    }

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
    {
        this->layerNameInput = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].label;
    }
}

void EditorMapVfxScene::removeSelectedVfxInstance(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    std::vector<ShipVfxInstance>& layers = this->currentShipVfxLayers();
    const uint32_t removedInstanceId =
        layers[static_cast<size_t>(this->selectedVfxInstanceIndex)].instanceId;
    this->closeVfxTrailPopup();
    this->removeTrailPiecesWithSourceInstanceId(removedInstanceId);
    layers.erase(layers.begin() + this->selectedVfxInstanceIndex);
    this->rebuildVfxLayerLabelsFromCurrentInstances();
    if (this->currentShipVfxLayers().empty())
    {
        this->setSelectedVfxInstanceIndex(-1);
    }
    else
    {
        const int nextIndex = std::clamp(this->selectedVfxInstanceIndex, 0, static_cast<int>(this->currentShipVfxLayers().size()) - 1);
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

void EditorMapVfxScene::snapSelectedVfxCenterToNearestTileAtScreen(float screenX, float screenY)
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
    if (!this->previewShipLoaded)
    {
        this->statusMessage = "Navire preview non charge.";
        return;
    }

    const Map& map = GetCurrentMap();
    const SDL_Point tile = map.screenToTileNearest(screenX, screenY);
    const SDL_FPoint tileCenter = map.tileToScreenCenterFloat(static_cast<float>(tile.x), static_cast<float>(tile.y));

    SDL_FPoint shipCenter = map.tileToScreenCenterFloat(this->previewShipTile.x, this->previewShipTile.y);
    float shipCenterOffsetX = 0.0f;
    float shipCenterOffsetY = 0.0f;
    if (this->previewShip.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
    {
        shipCenter.x += shipCenterOffsetX;
        shipCenter.y += shipCenterOffsetY;
    }

    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float newOffsetX = (tileCenter.x - shipCenter.x) / zoom;
    const float newOffsetY = (tileCenter.y - shipCenter.y) / zoom;

    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    if (override != nullptr)
    {
        override->offsetX = newOffsetX;
        override->offsetY = newOffsetY;
    }
    else
    {
        instance->offsetX = newOffsetX;
        instance->offsetY = newOffsetY;
    }
    this->markShipVfxDirty();

    char buf[128] = {};
    SDL_snprintf(
        buf,
        sizeof(buf),
        "VFX centre sur tuile (%d, %d) (offsets mis a jour).",
        tile.x,
        tile.y);
    this->statusMessage = buf;
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

void EditorMapVfxScene::applySelectedVfxRotationFromScreenPointer(float pointerX, float pointerY)
{
    ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr)
    {
        return;
    }
    if (instance->locked)
    {
        return;
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
    const DirectionOverride* resolved = this->getResolvedDirectionOverride(instance);
    float drawOffX = 0.0f;
    float drawOffY = 0.0f;
    this->getVfxPreviewDrawOffsets(*instance, resolved, &drawOffX, &drawOffY);
    const float pivotX = shipCenter.x + (drawOffX * scale);
    const float pivotY = shipCenter.y + (drawOffY * scale);

    const float dx = pointerX - pivotX;
    const float dy = pointerY - pivotY;
    if ((dx * dx) + (dy * dy) < 4.0f)
    {
        return;
    }

    // Degres horaire depuis le haut ecran (0 deg = vers le haut), aligne sur l'usage ecran Y vers le bas.
    float deg = static_cast<float>(std::atan2(dx, -dy)) * (180.0f / 3.14159265f);
    while (deg < 0.0f)
    {
        deg += 360.0f;
    }
    while (deg >= 360.0f)
    {
        deg -= 360.0f;
    }

    DirectionOverride* override = this->getEditableDirectionOverride(instance);
    float* rotation = (override != nullptr) ? &override->rotationDeg : &instance->rotationDeg;
    if (std::fabs(*rotation - deg) < 0.02f)
    {
        return;
    }
    *rotation = deg;
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

void EditorMapVfxScene::toggleVfxInstanceFlipHorizontalAtIndex(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
    if (instance.locked)
    {
        this->statusMessage = "Layer verrouille : FLIP H refuse.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(&instance);
    if (override != nullptr)
    {
        override->flipHorizontal = !override->flipHorizontal;
    }
    else
    {
        instance.flipHorizontal = !instance.flipHorizontal;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleVfxInstanceFlipVerticalAtIndex(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
    if (instance.locked)
    {
        this->statusMessage = "Layer verrouille : FLIP V refuse.";
        return;
    }
    DirectionOverride* override = this->getEditableDirectionOverride(&instance);
    if (override != nullptr)
    {
        override->flipVertical = !override->flipVertical;
    }
    else
    {
        instance.flipVertical = !instance.flipVertical;
    }
    this->markShipVfxDirty();
}

void EditorMapVfxScene::toggleSelectedVfxFollowShip(void)
{
    if (this->selectedVfxInstanceIndex < 0 || this->selectedVfxInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        this->statusMessage = "Aucun VFX selectionne.";
        return;
    }

    this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].followShip =
        !this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].followShip;
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
    for (size_t p = 0; p < this->shipVfxLayerPages.size(); ++p)
    {
        this->shipDrawOrderByPage[p] = 0;
        std::vector<ShipVfxInstance>& instances = this->shipVfxLayerPages[p];

        std::vector<size_t> behind;
        std::vector<size_t> front;
        behind.reserve(instances.size());
        front.reserve(instances.size());

        for (size_t i = 0; i < instances.size(); ++i)
        {
            const ShipVfxInstance& instance = instances[i];
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

        std::sort(behind.begin(), behind.end(), [&instances](size_t a, size_t b) {
            const ShipVfxInstance& lhs = instances[a];
            const ShipVfxInstance& rhs = instances[b];
            if (lhs.drawOrder != rhs.drawOrder)
            {
                return lhs.drawOrder < rhs.drawOrder;
            }
            return lhs.instanceId < rhs.instanceId;
        });
        std::sort(front.begin(), front.end(), [&instances](size_t a, size_t b) {
            const ShipVfxInstance& lhs = instances[a];
            const ShipVfxInstance& rhs = instances[b];
            if (lhs.drawOrder != rhs.drawOrder)
            {
                return lhs.drawOrder < rhs.drawOrder;
            }
            return lhs.instanceId < rhs.instanceId;
        });

        int negativeOrder = -static_cast<int>(behind.size());
        for (size_t idx : behind)
        {
            instances[idx].drawOrder = negativeOrder++;
            instances[idx].behindShip = true;
        }

        int positiveOrder = 1;
        for (size_t idx : front)
        {
            instances[idx].drawOrder = positiveOrder++;
            instances[idx].behindShip = false;
        }
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

int EditorMapVfxScene::getLoosePreviewTotalDurationMsActive(void) const
{
    const std::string trimmed = trimAscii(this->loosePreviewTotalDurationMsInput);
    if (trimmed.empty())
    {
        return 0;
    }

    char* endPtr = nullptr;
    const long long parsed = std::strtoll(trimmed.c_str(), &endPtr, 10);
    if (endPtr == trimmed.c_str() || *endPtr != '\0')
    {
        return 0;
    }
    if (parsed < static_cast<long long>(kLoosePreviewDurationMsMin) ||
        parsed > static_cast<long long>(kLoosePreviewDurationMsMax))
    {
        return 0;
    }
    return static_cast<int>(parsed);
}

bool EditorMapVfxScene::applyLoosePreviewTotalDurationMsInput(void)
{
    const std::string trimmed = trimAscii(this->loosePreviewTotalDurationMsInput);
    if (trimmed.empty())
    {
        this->loosePreviewTotalDurationMsInput.clear();
        return true;
    }

    char* endPtr = nullptr;
    const long long parsed = std::strtoll(trimmed.c_str(), &endPtr, 10);
    if (endPtr == trimmed.c_str() || *endPtr != '\0' || parsed < static_cast<long long>(kLoosePreviewDurationMsMin) ||
        parsed > static_cast<long long>(kLoosePreviewDurationMsMax))
    {
        this->statusMessage = "Duree totale ms invalide (entier 1.." + std::to_string(kLoosePreviewDurationMsMax) + ").";
        return false;
    }

    this->loosePreviewTotalDurationMsInput = std::to_string(static_cast<int>(parsed));
    this->statusMessage = "Duree totale animation: " + this->loosePreviewTotalDurationMsInput + " ms.";
    return true;
}

void EditorMapVfxScene::updateLoosePreviewPlacementExpirations(void)
{
    if (this->editorMode != EditorMode::LOOSE_SPRITES ||
        this->loosePreviewMode != LoosePreviewMode::PLACEMENT_PREVIEW)
    {
        return;
    }
    const int lifeMs = this->getLoosePreviewTotalDurationMsActive();
    if (lifeMs <= 0)
    {
        return;
    }
    const float nowSec = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const float lifeSec = static_cast<float>(lifeMs) * 0.001f;
    std::vector<LoosePreviewPlacement>& vec = this->loosePreviewPlacements;
    vec.erase(
        std::remove_if(
            vec.begin(),
            vec.end(),
            [nowSec, lifeSec](const LoosePreviewPlacement& p) {
                return (nowSec - p.spawnTimeSeconds) >= lifeSec;
            }),
        vec.end());
}

int EditorMapVfxScene::computeLooseAnimatedFrameIndex(float elapsedSeconds, int frameCount, float fpsFallback) const
{
    if (frameCount <= 0)
    {
        return 0;
    }

    const float clampedFps = std::clamp(fpsFallback, kSfxFpsMin, kSfxFpsMax);
    const float safeElapsedSeconds =
        (std::isfinite(elapsedSeconds) && elapsedSeconds > 0.0f) ? elapsedSeconds : 0.0f;
    return static_cast<int>(std::floor(safeElapsedSeconds * (std::max)(clampedFps, 1.0f))) % frameCount;
}

float EditorMapVfxScene::computeLooseExportSpritesheetFps(int frameCount) const
{
    (void)frameCount;
    return this->getLoosePreviewFpsOrDefault();
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
            this->layerNameInput = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].label;
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

bool EditorMapVfxScene::handleLoosePreviewTotalDurationMsInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->loosePreviewTotalDurationMsInputFocused)
    {
        return false;
    }

    (void)mod;
    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->loosePreviewTotalDurationMsInputFocused = false;
        const int active = this->getLoosePreviewTotalDurationMsActive();
        this->loosePreviewTotalDurationMsInput = active > 0 ? std::to_string(active) : std::string{};
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->applyLoosePreviewTotalDurationMsInput();
        this->loosePreviewTotalDurationMsInputFocused = false;
        return true;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE && !this->loosePreviewTotalDurationMsInput.empty() && !isrepeat)
    {
        this->loosePreviewTotalDurationMsInput.pop_back();
        return true;
    }
    if (scancode == SDL_SCANCODE_DELETE && !this->loosePreviewTotalDurationMsInput.empty() && !isrepeat)
    {
        this->loosePreviewTotalDurationMsInput.clear();
        return true;
    }

    auto appendDigit = [this](char c) -> bool {
        if (this->loosePreviewTotalDurationMsInput.size() >= 12U)
        {
            return true;
        }
        if (c >= '0' && c <= '9')
        {
            this->loosePreviewTotalDurationMsInput.push_back(c);
        }
        return true;
    };

    auto appendKeypadDigitIfAny = [&appendDigit, keycode, scancode]() -> bool {
        switch (keycode)
        {
        case SDLK_KP_0: return appendDigit('0');
        case SDLK_KP_1: return appendDigit('1');
        case SDLK_KP_2: return appendDigit('2');
        case SDLK_KP_3: return appendDigit('3');
        case SDLK_KP_4: return appendDigit('4');
        case SDLK_KP_5: return appendDigit('5');
        case SDLK_KP_6: return appendDigit('6');
        case SDLK_KP_7: return appendDigit('7');
        case SDLK_KP_8: return appendDigit('8');
        case SDLK_KP_9: return appendDigit('9');
        default: break;
        }
        switch (scancode)
        {
        case SDL_SCANCODE_KP_0: return appendDigit('0');
        case SDL_SCANCODE_KP_1: return appendDigit('1');
        case SDL_SCANCODE_KP_2: return appendDigit('2');
        case SDL_SCANCODE_KP_3: return appendDigit('3');
        case SDL_SCANCODE_KP_4: return appendDigit('4');
        case SDL_SCANCODE_KP_5: return appendDigit('5');
        case SDL_SCANCODE_KP_6: return appendDigit('6');
        case SDL_SCANCODE_KP_7: return appendDigit('7');
        case SDL_SCANCODE_KP_8: return appendDigit('8');
        case SDL_SCANCODE_KP_9: return appendDigit('9');
        default: break;
        }
        return false;
    };

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
    return appendDigit(key[0]);
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
        this->statusMessage = "Choisis le dossier parent d'export dans l'explorateur.";
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

bool EditorMapVfxScene::handleExportConfirmPopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->exportConfirmPopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    ExportConfirmPopupLayout lay{};
    if (!this->computeExportConfirmPopupLayout(&lay))
    {
        return true;
    }
    this->exportConfirmPopupLastLayout = lay;

    if (!this->pointInRect(x, y, lay.popup))
    {
        if (this->pointInRect(x, y, lay.dimFullMap))
        {
            this->closeExportConfirmPopup();
            this->statusMessage = "Export annule.";
        }
        return true;
    }

    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeExportConfirmPopup();
        this->statusMessage = "Export annule.";
        return true;
    }
    if (!this->pointInRect(x, y, lay.validateBtn))
    {
        return true;
    }

    const ExportConfirmAction action = this->exportConfirmPopupAction;
    this->closeExportConfirmPopup();
    if (action == ExportConfirmAction::SHIP_VFX_ALL_SHIPS)
    {
        this->exportAllShipsVfxJsonToShipFolders();
    }
    else if (action == ExportConfirmAction::LOOSE_OPEN_EXPORT_FLOW)
    {
        this->openLooseExportNamePopup();
    }
    else
    {
        this->statusMessage = "Export annule.";
    }
    return true;
}

bool EditorMapVfxScene::handleExportConfirmPopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;
    (void)keycode;
    (void)mod;
    if (!this->exportConfirmPopupVisible)
    {
        return false;
    }

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        this->closeExportConfirmPopup();
        this->statusMessage = "Export annule.";
        return true;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        const ExportConfirmAction action = this->exportConfirmPopupAction;
        this->closeExportConfirmPopup();
        if (action == ExportConfirmAction::SHIP_VFX_ALL_SHIPS)
        {
            this->exportAllShipsVfxJsonToShipFolders();
        }
        else if (action == ExportConfirmAction::LOOSE_OPEN_EXPORT_FLOW)
        {
            this->openLooseExportNamePopup();
        }
        else
        {
            this->statusMessage = "Export annule.";
        }
        return true;
    }

    return true;
}

void EditorMapVfxScene::getVfxPreviewDrawOffsets(
    const ShipVfxInstance& instance,
    const DirectionOverride* resolvedOverride,
    float* outOffsetX,
    float* outOffsetY) const
{
    if (outOffsetX == nullptr || outOffsetY == nullptr)
    {
        return;
    }
    const float bx = (resolvedOverride != nullptr) ? resolvedOverride->offsetX : instance.offsetX;
    const float by = (resolvedOverride != nullptr) ? resolvedOverride->offsetY : instance.offsetY;
    *outOffsetX = bx;
    *outOffsetY = by;
}

void EditorMapVfxScene::appendMotionTrailPieceFromStep(
    const ShipVfxInstance& inst,
    float anchorShipTileX,
    float anchorShipTileY,
    float moveDxTiles,
    float moveDyTiles,
    float timeSec,
    std::vector<ShipVfxTrailPiece>& outPieces,
    uint32_t& nextUiId,
    float marchePopupPreviewPathU,
    float anchorShipCenterOffsetEffectiveZoom,
    float spawnSpeedTilesPerSec) const
{
    ShipVfxTrailPiece piece{};
    piece.sourceVfxInstanceId = inst.instanceId;
    piece.anchorShipTileX = anchorShipTileX;
    piece.anchorShipTileY = anchorShipTileY;
    piece.bornTimeSeconds = timeSec;
    float speedTilesPerSec = spawnSpeedTilesPerSec;
    if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
    {
        speedTilesPerSec = this->previewShip.getSpeedTilesPerSecond();
    }
    if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
    {
        speedTilesPerSec = 6.0f;
    }
    const float lifetimeTiles = static_cast<float>((std::max)(inst.motionTrailLifetimeTiles, 1));
    piece.timeRemainingSec = (std::max)(lifetimeTiles / speedTilesPerSec, 0.05f);
    piece.trailLifetimeInitialSec = piece.timeRemainingSec;
    const DirectionOverride* movOvr = this->getResolvedDirectionOverride(&inst);
    // Base commune des rejets : ancrage sur le decal courant du layer.
    this->getVfxPreviewDrawOffsets(inst, movOvr, &piece.trailDrawOffsetX, &piece.trailDrawOffsetY);
    if (std::isfinite(anchorShipCenterOffsetEffectiveZoom) &&
        anchorShipCenterOffsetEffectiveZoom > 0.0f)
    {
        piece.anchorShipSpriteCenterOffValid = this->previewShip.getCurrentSpriteCenterOffsetPixelsForEffectiveZoom(
            anchorShipCenterOffsetEffectiveZoom,
            &piece.anchorShipSpriteCenterOffXPx,
            &piece.anchorShipSpriteCenterOffYPx);
    }
    else
    {
        piece.anchorShipSpriteCenterOffValid = this->previewShip.getCurrentSpriteCenterOffsetPixels(
            &piece.anchorShipSpriteCenterOffXPx,
            &piece.anchorShipSpriteCenterOffYPx);
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
            // Mode cone: repere fixe (independant de l'orientation navire), identique a la preview popup.
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
            editorMapVfxSampleTrailConeDepthAndLateral(spread, coneHalfAngleRad, &depth, &lateral);

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
        std::uniform_real_distribution<float> rotU(
            -kMotionTrailRotationJitterMaxDeg, kMotionTrailRotationJitterMaxDeg);
        piece.trailRotationJitterDeg = p * rotU(editorMapVfxTrailJitterRng());
    }
    EditorMapVfxScene::initTrailPieceMotionExtras(&piece, moveDxTiles, moveDyTiles);
    uint32_t nid = ++nextUiId;
    if (nid == 0U)
    {
        nid = ++nextUiId;
    }
    piece.layerPanelUiId = nid;
    piece.fromIdleRingCrown = false;
    piece.marchePopupPreviewPathU = marchePopupPreviewPathU;
    outPieces.push_back(std::move(piece));
}

struct EditorMapVfxMarchePopupGrid
{
    float halfTileW = 0.0f;
    float halfTileH = 0.0f;
    float startScreenX = 0.0f;
    float startScreenY = 0.0f;
    float startTileX = 0.0f;
    float startTileY = 0.0f;
    float stepTileX = 1.0f;
    float stepTileY = 0.0f;
    float pathLengthTiles = 0.0f;
};

static EditorMapVfxMarchePopupGrid editorMapVfxBuildMarchePopupGrid(
    const SDL_FRect& rm, int dirMarche, const Map& map, float worldZoom)
{
    EditorMapVfxMarchePopupGrid g{};
    const float camZ = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float baseTileW = map.getTileWidth() / camZ;
    const float baseTileH = map.getTileHeight() / camZ;
    const float tileW = (std::max)(baseTileW * worldZoom, 1.0f);
    const float tileH = (std::max)(baseTileH * worldZoom, 1.0f);
    g.halfTileW = tileW * 0.5f;
    g.halfTileH = tileH * 0.5f;

    editorMapVfxDirectionIndexToTileStep(dirMarche, &g.stepTileX, &g.stepTileY);

    const float stepScreenX = (g.stepTileX - g.stepTileY) * g.halfTileW;
    const float stepScreenY = (g.stepTileX + g.stepTileY) * g.halfTileH;

    constexpr float kMarchePathPad = 26.0f;
    const float availW = (std::max)(rm.w - (kMarchePathPad * 2.0f), 8.0f);
    const float availH = (std::max)(rm.h - (kMarchePathPad * 2.0f), 8.0f);
    const float maxByX = (std::fabs(stepScreenX) > 0.0001f) ? (availW / std::fabs(stepScreenX)) : 1000.0f;
    const float maxByY = (std::fabs(stepScreenY) > 0.0001f) ? (availH / std::fabs(stepScreenY)) : 1000.0f;
    g.pathLengthTiles = (std::clamp)(std::floor((std::min)(maxByX, maxByY)), 2.0f, 64.0f);

    const float spanX = stepScreenX * g.pathLengthTiles;
    const float spanY = stepScreenY * g.pathLengthTiles;
    const float minX = rm.x + ((rm.w - std::fabs(spanX)) * 0.5f);
    const float minY = rm.y + ((rm.h - std::fabs(spanY)) * 0.5f);
    g.startScreenX = minX + ((spanX < 0.0f) ? std::fabs(spanX) : 0.0f);
    g.startScreenY = minY + ((spanY < 0.0f) ? std::fabs(spanY) : 0.0f);

    g.startTileX = 0.0f;
    g.startTileY = 0.0f;
    return g;
}

void EditorMapVfxScene::resetVfxTrailPopupMarcheSimulationState(const std::string& signature)
{
    this->vfxTrailPopupMarcheSimParamSignature = signature;
    this->vfxTrailPopupMarcheSimPieces.clear();
    this->vfxTrailPopupMarcheDistAlongPathPx = 0.0f;
    this->vfxTrailPopupMarcheSimDistanceAcc = 0.0f;
}

void EditorMapVfxScene::updateVfxTrailPopupMarcheSimulation(double dt)
{
    const float dtf = static_cast<float>(dt);
    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;
    if (!this->vfxTrailPopupVisible || !this->previewShipLoaded ||
        this->vfxTrailPopupParentInstanceIndex < 0 ||
        this->vfxTrailPopupParentInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        this->vfxTrailPopupMarcheSimPieces.clear();
        this->vfxTrailPopupMarcheSimParamSignature.clear();
        this->vfxTrailPopupMarcheDistAlongPathPx = 0.0f;
        this->vfxTrailPopupMarcheSimDistanceAcc = 0.0f;
        return;
    }

    ShipVfxInstance& instRef =
        this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
    constexpr float kTrailPopupPreviewWorldZoomMin = 0.4f;
    constexpr float kTrailPopupPreviewWorldZoomMax = 1.0f;
    const float worldZ = (std::clamp)(this->vfxTrailPopupPreviewZoom,
                                      kTrailPopupPreviewWorldZoomMin,
                                      kTrailPopupPreviewWorldZoomMax);
    const float shipEffectiveZoom = worldZ * this->previewShip.getDrawScale();
    const int pageKey = this->getShipVfxLayerPageKey();
    const SDL_FRect& rm = this->vfxTrailPopupLastLayout.previewMarcheRect;
    float simConeLength = editorMapVfxParseFloat(
        this->vfxTrailPopupLateralJitterInput.c_str(), instRef.motionTrailLateralJitterRadius);
    float simConeOffX = instRef.motionTrailConeOffsetX;
    float simConeOffY = instRef.motionTrailConeOffsetY;
    float simConeDir = instRef.motionTrailConeDirectionOffsetDeg;
    float simConeHalf = instRef.motionTrailConeHalfAngleDeg;
    int simConeSpawnCount = std::clamp(instRef.motionTrailConeSpawnCount, 1, 32);
    if (this->vfxTrailConePopupVisible &&
        this->vfxTrailConePopupParentInstanceIndex == this->vfxTrailPopupParentInstanceIndex)
    {
        simConeLength = this->vfxTrailConePopupLength;
        simConeOffX = this->vfxTrailConePopupOffsetX;
        simConeOffY = this->vfxTrailConePopupOffsetY;
        simConeDir = this->vfxTrailConePopupDirectionOffsetDeg;
        simConeHalf = this->vfxTrailConePopupHalfAngleDeg;
        simConeSpawnCount = this->vfxTrailConePopupSpawnCount;
    }
    simConeLength = (std::clamp)(simConeLength, 0.0f, kTrailConePopupLengthMax);
    simConeOffX = (std::clamp)(simConeOffX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    simConeOffY = (std::clamp)(simConeOffY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
    simConeDir = (std::clamp)(simConeDir, -179.0f, 179.0f);
    simConeHalf = (std::clamp)(simConeHalf, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
    simConeSpawnCount = std::clamp(simConeSpawnCount, 1, 32);
    const std::string sig = std::to_string(pageKey) + "|" + this->vfxTrailPopupEveryNTilesInput + "|" +
                            this->vfxTrailPopupLifetimeTilesInput + "|" + std::to_string(simConeLength) + "|" +
                            std::to_string(simConeOffX) + "|" + std::to_string(simConeOffY) + "|" +
                            std::to_string(simConeDir) + "|" + std::to_string(simConeHalf) + "|" +
                            std::to_string(simConeSpawnCount) + "|" +
                            this->vfxTrailPopupRotationPctInput + "|" +
                            (this->vfxTrailPopupStrictTilePlacement ? "1" : "0");
    if (sig != this->vfxTrailPopupMarcheSimParamSignature)
    {
        this->resetVfxTrailPopupMarcheSimulationState(sig);
    }

    for (size_t i = 0; i < this->vfxTrailPopupMarcheSimPieces.size();)
    {
        this->vfxTrailPopupMarcheSimPieces[i].timeRemainingSec -= dtf;
        if (this->vfxTrailPopupMarcheSimPieces[i].timeRemainingSec <= 0.0f)
        {
            this->vfxTrailPopupMarcheSimPieces.erase(this->vfxTrailPopupMarcheSimPieces.begin() +
                                                    static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }

    const int parsedEveryNMarche = editorMapVfxParseIntClamped(
        this->vfxTrailPopupEveryNTilesInput.c_str(),
        kMotionTrailEveryNTilesMin,
        kMotionTrailEveryNTilesMax,
        instRef.motionTrailEveryNTiles);
    if (parsedEveryNMarche <= 0)
    {
        return;
    }

    if (rm.w <= 4.0f || rm.h <= 4.0f)
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const int dirMarche = pageKey % 4;
    /** Geometrie de rendu: suit le zoom popup pour que navire + rejets restent dans le meme repere visuel. */
    const EditorMapVfxMarchePopupGrid grid = editorMapVfxBuildMarchePopupGrid(rm, dirMarche, map, worldZ);
    if (grid.pathLengthTiles < 0.0001f)
    {
        return;
    }
    /** Vitesse preview rejets en marche ajustable via boutons SPEED - / +. */
    float speedTilesPerSec = this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec;
    if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
    {
        speedTilesPerSec = this->previewShip.getSpeedTilesPerSecond();
    }
    speedTilesPerSec = (std::clamp)(speedTilesPerSec, 0.05f, 50.0f);
    const float frameDistTiles = speedTilesPerSec * dtf;

    ShipVfxInstance simInst = instRef;
    simInst.motionTrailEveryNTiles = editorMapVfxParseIntClamped(
        this->vfxTrailPopupEveryNTilesInput.c_str(),
        kMotionTrailEveryNTilesMin,
        kMotionTrailEveryNTilesMax,
        instRef.motionTrailEveryNTiles);
    simInst.motionTrailLifetimeTiles = editorMapVfxParseIntClamped(
        this->vfxTrailPopupLifetimeTilesInput.c_str(), 1, 4096, instRef.motionTrailLifetimeTiles);
    simInst.motionTrailLateralJitterRadius = simConeLength;
    simInst.motionTrailConeOffsetX = simConeOffX;
    simInst.motionTrailConeOffsetY = simConeOffY;
    simInst.motionTrailConeDirectionOffsetDeg = simConeDir;
    simInst.motionTrailConeHalfAngleDeg = simConeHalf;
    simInst.motionTrailConeSpawnCount = simConeSpawnCount;
    simInst.motionTrailRotationRandomPercent = editorMapVfxParseIntClamped(
        this->vfxTrailPopupRotationPctInput.c_str(), 0, 100, instRef.motionTrailRotationRandomPercent);
    simInst.motionTrailStrictTilePlacement = this->vfxTrailPopupStrictTilePlacement;

    const float thresholdTiles = static_cast<float>(simInst.motionTrailEveryNTiles);
    const float movedDist = (std::max)(frameDistTiles, 0.0f);
    float accDist = this->vfxTrailPopupMarcheSimDistanceAcc + movedDist;
    float headU = std::fmod(this->vfxTrailPopupMarcheDistAlongPathPx, 1.0f);
    if (headU < 0.0f)
    {
        headU += 1.0f;
    }
    const float pathLenTiles = (std::max)(grid.pathLengthTiles, 0.0001f);
    const float headDistBefore = headU * pathLenTiles;
    const float headDistAfter = headDistBefore + movedDist;

    int spawnGuard = 0;
    while (thresholdTiles > 0.0f && accDist >= thresholdTiles && spawnGuard < 1024)
    {
        const float overshootTiles = accDist - thresholdTiles;
        float spawnDistTiles = headDistAfter - overshootTiles;
        spawnDistTiles = std::fmod(spawnDistTiles, pathLenTiles);
        if (spawnDistTiles < 0.0f)
        {
            spawnDistTiles += pathLenTiles;
        }
        const float spawnTileX = grid.startTileX + grid.stepTileX * spawnDistTiles;
        const float spawnTileY = grid.startTileY + grid.stepTileY * spawnDistTiles;
        const float spawnU = spawnDistTiles / pathLenTiles;
        accDist -= thresholdTiles;
        const int spawnCount = simInst.motionTrailStrictTilePlacement ? 1 : std::clamp(simInst.motionTrailConeSpawnCount, 1, 32);
        for (int burst = 0; burst < spawnCount && spawnGuard < 1024; ++burst)
        {
            this->appendMotionTrailPieceFromStep(simInst,
                                                 spawnTileX,
                                                 spawnTileY,
                                                 grid.stepTileX,
                                                 grid.stepTileY,
                                                 timeSec,
                                                 this->vfxTrailPopupMarcheSimPieces,
                                                 this->vfxTrailPopupMarcheSimNextUiId,
                                                 spawnU,
                                                 shipEffectiveZoom,
                                                 speedTilesPerSec);
            ++spawnGuard;
        }
    }
    this->vfxTrailPopupMarcheSimDistanceAcc =
        (thresholdTiles > 0.0f) ? (std::clamp)(accDist, 0.0f, thresholdTiles) : 0.0f;
    float headDistAfterWrapped = std::fmod(headDistAfter, pathLenTiles);
    if (headDistAfterWrapped < 0.0f)
    {
        headDistAfterWrapped += pathLenTiles;
    }
    const float headUAfter = headDistAfterWrapped / pathLenTiles;
    this->vfxTrailPopupMarcheDistAlongPathPx = headUAfter;
}

void EditorMapVfxScene::clearShipVfxTrailPieces(void)
{
    for (std::vector<ShipVfxTrailPiece>& pageTrails : this->shipVfxTrailPiecesByPage)
    {
        pageTrails.clear();
    }
    for (bool& valid : this->shipVfxTrailPrevShipTileValidByPage)
    {
        valid = false;
    }
}

void EditorMapVfxScene::updatePreviewShipPilotAndVfxMotion(double dt)
{
    Map& map = GetCurrentMap();
    const float dtf = static_cast<float>(dt);
    const float timeSec = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const size_t trailPageIx = static_cast<size_t>(this->getShipVfxLayerPageKey());

    for (size_t i = 0; i < this->currentShipVfxTrailPieces().size();)
    {
        this->currentShipVfxTrailPieces()[i].timeRemainingSec -= dtf;
        if (this->currentShipVfxTrailPieces()[i].timeRemainingSec <= 0.0f)
        {
            this->currentShipVfxTrailPieces().erase(this->currentShipVfxTrailPieces().begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }

    if (this->previewShipPilotActive && this->previewShipLoaded)
    {
        const float timeSecPilotCull = static_cast<float>(SDL_GetTicks()) * 0.001f;
        const auto& layersPilotCull = this->currentShipVfxLayers();
        for (size_t i = 0; i < this->currentShipVfxTrailPieces().size();)
        {
            const ShipVfxTrailPiece& trailPiece = this->currentShipVfxTrailPieces()[i];
            const ShipVfxInstance* srcInst = nullptr;
            for (const ShipVfxInstance& cand : layersPilotCull)
            {
                if (cand.instanceId == trailPiece.sourceVfxInstanceId)
                {
                    srcInst = &cand;
                    break;
                }
            }
            if (srcInst == nullptr || srcInst->importedSfxIndex < 0 ||
                srcInst->importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
            {
                ++i;
                continue;
            }
            const ImportedSfx& trailImported =
                this->importedSfx[static_cast<size_t>(srcInst->importedSfxIndex)];
            if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(trailImported, timeSecPilotCull))
            {
                this->currentShipVfxTrailPieces().erase(this->currentShipVfxTrailPieces().begin() + static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }

        this->previewShip.update(dt, map);
        const SDL_FPoint p = this->previewShip.getPositionTile();
        this->previewShipTile.x = p.x;
        this->previewShipTile.y = p.y;
    }

    if (!this->previewShipLoaded)
    {
        return;
    }

    if (!this->previewShipPilotActive)
    {
        this->vfxTrailPopupMarcheLastPilotMoveDirValid = false;
        return;
    }

    const bool moving = this->previewShip.isMoving();
    if (moving)
    {
        for (size_t i = 0; i < this->currentShipVfxTrailPieces().size();)
        {
            if (this->currentShipVfxTrailPieces()[i].fromIdleRingCrown)
            {
                this->currentShipVfxTrailPieces().erase(this->currentShipVfxTrailPieces().begin() +
                                                static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }
        for (ShipVfxInstance& inst : this->currentShipVfxLayers())
        {
            inst.motionTrailIdleSpawnAccSec = 0.0f;
            inst.motionTrailIdleRingSalvoPiecesRemaining = 0;
            inst.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
        }
        if (this->shipVfxTrailPrevShipTileValidByPage[trailPageIx])
        {
            const float dx =
                this->previewShipTile.x - this->shipVfxTrailPrevShipTileByPage[trailPageIx].x;
            const float dy =
                this->previewShipTile.y - this->shipVfxTrailPrevShipTileByPage[trailPageIx].y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.00001f)
            {
                this->vfxTrailPopupMarcheLastPilotMoveDirX = dx / dist;
                this->vfxTrailPopupMarcheLastPilotMoveDirY = dy / dist;
                this->vfxTrailPopupMarcheLastPilotMoveDirValid = true;
                auto& layers = this->currentShipVfxLayers();
                for (ShipVfxInstance& inst : layers)
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
                        if (inst.importedSfxIndex >= 0 &&
                            inst.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
                        {
                            const ImportedSfx& spawnImp =
                                this->importedSfx[static_cast<size_t>(inst.importedSfxIndex)];
                            if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(spawnImp, timeSec))
                            {
                                inst.motionTrailDistanceAcc -= thresholdTiles;
                                ++spawnGuard;
                                continue;
                            }
                        }
                        const float invDist = 1.0f / dist;
                        const float nx = dx * invDist;
                        const float ny = dy * invDist;
                        const float overshoot = inst.motionTrailDistanceAcc - thresholdTiles;
                        const float spawnTileX = this->previewShipTile.x - (nx * overshoot);
                        const float spawnTileY = this->previewShipTile.y - (ny * overshoot);
                        inst.motionTrailDistanceAcc -= thresholdTiles;
                        float speedTilesPerSec = (dtf > 0.000001f) ? (dist / dtf) : this->previewShip.getSpeedTilesPerSecond();
                        if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
                        {
                            speedTilesPerSec = 6.0f;
                        }
                        const int spawnCount =
                            inst.motionTrailStrictTilePlacement ? 1 : std::clamp(inst.motionTrailConeSpawnCount, 1, 32);
                        for (int burst = 0; burst < spawnCount && spawnGuard < 1024; ++burst)
                        {
                            this->appendMotionTrailPieceFromStep(inst,
                                                                spawnTileX,
                                                                spawnTileY,
                                                                dx,
                                                                dy,
                                                                timeSec,
                                                                this->currentShipVfxTrailPieces(),
                                                                this->nextShipVfxTrailLayerPanelUiId,
                                                                -1.0f,
                                                                -1.0f,
                                                                speedTilesPerSec);
                            ++spawnGuard;
                        }
                    }
                    if (thresholdTiles > 0.0f)
                    {
                        inst.motionTrailDistanceAcc = (std::clamp)(inst.motionTrailDistanceAcc, 0.0f, thresholdTiles);
                    }
                }
            }
            else
            {
                this->vfxTrailPopupMarcheLastPilotMoveDirValid = false;
            }
        }
        else
        {
            this->vfxTrailPopupMarcheLastPilotMoveDirValid = false;
        }
    }
    else
    {
        this->vfxTrailPopupMarcheLastPilotMoveDirValid = false;
        constexpr float kTwoPi = 6.28318530718f;
        const auto spawnIdleRingCrownPiece = [this, timeSec, kTwoPi](ShipVfxInstance& inst, int k) -> bool {
            if (inst.importedSfxIndex >= 0 &&
                inst.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
            {
                const ImportedSfx& ringImported =
                    this->importedSfx[static_cast<size_t>(inst.importedSfxIndex)];
                if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(ringImported, timeSec))
                {
                    return false;
                }
            }
            const int pieceCount = std::clamp(inst.motionTrailIdleRingPieceCount, 1, 32);
            const float R = (std::max)(inst.motionTrailIdleRingRadius, 2.0f);
            const DirectionOverride* ringOv = this->getResolvedDirectionOverride(&inst);
            const float baseOx = (ringOv != nullptr) ? ringOv->offsetX : inst.offsetX;
            const float baseOy = (ringOv != nullptr) ? ringOv->offsetY : inst.offsetY;
            const float ang =
                kTwoPi * (static_cast<float>(k) / static_cast<float>(pieceCount));
            ShipVfxTrailPiece piece{};
            piece.sourceVfxInstanceId = inst.instanceId;
            piece.anchorShipTileX = this->previewShipTile.x;
            piece.anchorShipTileY = this->previewShipTile.y;
            piece.bornTimeSeconds = timeSec;
            float speedTilesPerSec = this->previewShip.getSpeedTilesPerSecond();
            if (!std::isfinite(speedTilesPerSec) || speedTilesPerSec <= 0.0f)
            {
                speedTilesPerSec = 6.0f;
            }
            const float lifetimeTiles = static_cast<float>((std::max)(inst.motionTrailLifetimeTiles, 1));
            piece.timeRemainingSec = (std::max)(lifetimeTiles / speedTilesPerSec, 0.05f);
            piece.trailLifetimeInitialSec = piece.timeRemainingSec;
            float ox = baseOx + std::cos(ang) * R;
            float oy = baseOy + std::sin(ang) * R;
            if (inst.motionTrailIdleRingPositionJitterRadius > 0.0001f)
            {
                std::uniform_real_distribution<float> u01(0.0f, 1.0f);
                std::uniform_real_distribution<float> uAng(0.0f, kTwoPi);
                const float rr =
                    std::sqrt((std::max)(u01(editorMapVfxTrailJitterRng()), 1.0e-8f)) *
                    inst.motionTrailIdleRingPositionJitterRadius;
                const float ja = uAng(editorMapVfxTrailJitterRng());
                ox += std::cos(ja) * rr;
                oy += std::sin(ja) * rr;
            }
            piece.trailDrawOffsetX = ox;
            piece.trailDrawOffsetY = oy;
            piece.anchorShipSpriteCenterOffValid = this->previewShip.getCurrentSpriteCenterOffsetPixels(
                &piece.anchorShipSpriteCenterOffXPx,
                &piece.anchorShipSpriteCenterOffYPx);
            piece.trailPerpendicularJitterX = 0.0f;
            piece.trailPerpendicularJitterY = 0.0f;
            piece.trailRotationJitterDeg = 0.0f;
            if (inst.motionTrailIdleRingRotationRandomPercent > 0)
            {
                const float p =
                    static_cast<float>(std::clamp(inst.motionTrailIdleRingRotationRandomPercent, 0, 100)) /
                    100.0f;
                std::uniform_real_distribution<float> rotU(
                    -kMotionTrailRotationJitterMaxDeg, kMotionTrailRotationJitterMaxDeg);
                piece.trailRotationJitterDeg = p * rotU(editorMapVfxTrailJitterRng());
            }
            EditorMapVfxScene::initTrailPieceMotionExtras(&piece, 0.0f, 0.0f);
            uint32_t nid = ++this->nextShipVfxTrailLayerPanelUiId;
            if (nid == 0U)
            {
                nid = ++this->nextShipVfxTrailLayerPanelUiId;
            }
            piece.layerPanelUiId = nid;
            piece.fromIdleRingCrown = true;
            this->currentShipVfxTrailPieces().push_back(piece);
            return true;
        };

        auto& layers = this->currentShipVfxLayers();
        for (ShipVfxInstance& inst : layers)
        {
            if (!inst.motionTrailIdleRingWhenStationary)
            {
                inst.motionTrailIdleRingSalvoPiecesRemaining = 0;
                inst.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
                continue;
            }
            const int pieceCount = std::clamp(inst.motionTrailIdleRingPieceCount, 1, 32);
            const float periodSec = kIdleRingSalvoPeriodSec;
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

    if (this->previewShipPilotActive)
    {
        this->shipVfxTrailPrevShipTileByPage[trailPageIx].x = this->previewShipTile.x;
        this->shipVfxTrailPrevShipTileByPage[trailPageIx].y = this->previewShipTile.y;
        this->shipVfxTrailPrevShipTileValidByPage[trailPageIx] = true;
    }
}

void EditorMapVfxScene::togglePreviewShipPilotControl(void)
{
    this->previewShipPilotActive = false;
    this->clearShipVfxTrailPieces();
    this->statusMessage = "Mode PILOTER retire de scene-editormap-vfx.";
}

void EditorMapVfxScene::captureVfxMotionSpawnAtIndex(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    ShipVfxInstance& inst = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
    const DirectionOverride* o = this->getResolvedDirectionOverride(&inst);
    inst.motionSpawnOffsetX = (o != nullptr) ? o->offsetX : inst.offsetX;
    inst.motionSpawnOffsetY = (o != nullptr) ? o->offsetY : inst.offsetY;
    inst.motionSpawnCaptured = true;
    inst.motionTrailDistanceAcc = 0.0f;
    this->markShipVfxDirty();
    this->statusMessage =
        "Reference SPAWN enregistree (etat technique). La trainee suit le layer ; en mode cone, "
        "la dispersion se fait autour du layer.";
}

int EditorMapVfxScene::findTopmostVfxInstanceIndexAtPointExcluding(float x, float y, int excludeInstanceIndex) const
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
    candidates.reserve(this->currentShipVfxLayers().size());
    for (size_t i = 0; i < this->currentShipVfxLayers().size(); ++i)
    {
        const ShipVfxInstance& instance = this->currentShipVfxLayers()[i];
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
    const auto& layerVec = this->currentShipVfxLayers();
    for (const auto& candidate : candidates)
    {
        if (excludeInstanceIndex >= 0 && candidate.first == excludeInstanceIndex)
        {
            continue;
        }

        const ShipVfxInstance& instance = layerVec[static_cast<size_t>(candidate.first)];
        if (instance.importedSfxIndex < 0 || instance.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }

        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
        if (imported.frames.empty() || imported.image.sdl_texture == nullptr)
        {
            continue;
        }
        if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(imported, timeSeconds))
        {
            continue;
        }

        const int frameIndex = this->computeVfxPreviewFrameIndex(instance, timeSeconds, layerVec, imported);
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];

        const float sourceW = frame.w;
        const float sourceH = frame.h;
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        float drawOffX = 0.0f;
        float drawOffY = 0.0f;
        this->getVfxPreviewDrawOffsets(instance, override, &drawOffX, &drawOffY);
        const float offsetX = drawOffX * scale;
        const float offsetY = drawOffY * scale;
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

int EditorMapVfxScene::findTopmostVfxInstanceIndexAtPoint(float x, float y) const
{
    return this->findTopmostVfxInstanceIndexAtPointExcluding(x, y, -1);
}

bool EditorMapVfxScene::tryGetScreenTileNearestForSelectedVfxCenter(SDL_Point* outTile) const
{
    if (outTile == nullptr || !this->previewShipLoaded || !this->hasSelectedVfxInstance())
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
    const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
    const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
    float drawOffX = 0.0f;
    float drawOffY = 0.0f;
    this->getVfxPreviewDrawOffsets(instance, override, &drawOffX, &drawOffY);
    const float offsetX = drawOffX * scale;
    const float offsetY = drawOffY * scale;
    *outTile = map.screenToTileNearest(shipCenter.x + offsetX, shipCenter.y + offsetY);
    return true;
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

    const cJSON* editorNode = cJSON_GetObjectItemCaseSensitive(root, "editor");
    const cJSON* parseRoot = cJSON_IsObject(editorNode) ? editorNode : root;

    auto trySelectShipFromJson = [this, parseRoot]() {
        const cJSON* shipNode = cJSON_GetObjectItemCaseSensitive(parseRoot, "ship");
        if (!cJSON_IsObject(shipNode))
        {
            return;
        }

        std::string shipPathFromJson;
        const cJSON* shipFolderPathNode = cJSON_GetObjectItemCaseSensitive(shipNode, "folderPath");
        const cJSON* shipFolderAbsoluteNode = cJSON_GetObjectItemCaseSensitive(shipNode, "folderAbsolutePath");
        if (cJSON_IsString(shipFolderPathNode) && shipFolderPathNode->valuestring != nullptr)
        {
            shipPathFromJson = normalizePathSlashes(shipFolderPathNode->valuestring);
        }
        else if (cJSON_IsString(shipFolderAbsoluteNode) && shipFolderAbsoluteNode->valuestring != nullptr)
        {
            shipPathFromJson = normalizePathSlashes(shipFolderAbsoluteNode->valuestring);
        }

        int matchedShipIndex = -1;
        const std::string shipPathKey = makeComparableSourcePathKey(shipPathFromJson);
        if (!shipPathKey.empty())
        {
            for (size_t i = 0; i < this->importedShips.size(); ++i)
            {
                if (makeComparableSourcePathKey(this->importedShips[i].folderAbsolutePath) == shipPathKey)
                {
                    matchedShipIndex = static_cast<int>(i);
                    break;
                }
            }
        }

        if (matchedShipIndex < 0)
        {
            const cJSON* shipIdNode = cJSON_GetObjectItemCaseSensitive(shipNode, "id");
            if (cJSON_IsString(shipIdNode) && shipIdNode->valuestring != nullptr)
            {
                const std::string shipIdKey = makePathKeyLower(trimAscii(shipIdNode->valuestring));
                for (size_t i = 0; i < this->importedShips.size(); ++i)
                {
                    const std::string importedShipId =
                        makePathKeyLower(makeShipConfigSlug(this->importedShips[i].displayName));
                    if (!shipIdKey.empty() && shipIdKey == importedShipId)
                    {
                        matchedShipIndex = static_cast<int>(i);
                        break;
                    }
                }
            }
        }

        if (matchedShipIndex < 0 ||
            matchedShipIndex >= static_cast<int>(this->importedShips.size()))
        {
            return;
        }

        this->selectedShipIndex = matchedShipIndex;
        this->ensureSelectionVisible(
            this->selectedShipIndex,
            &this->shipListScrollOffset,
            static_cast<int>(this->importedShips.size()));

        const ImportedShip& selectedShip = this->importedShips[static_cast<size_t>(matchedShipIndex)];
        if (!this->previewShipLoaded ||
            makeComparableSourcePathKey(this->loadedShipFolderAbsolute) !=
                makeComparableSourcePathKey(selectedShip.folderAbsolutePath))
        {
            this->loadShipFolderFromAbsolutePath(selectedShip.folderAbsolutePath.c_str());
        }
    };
    trySelectShipFromJson();

    const cJSON* formatNode = cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON* versionNode = cJSON_GetObjectItemCaseSensitive(root, "version");
    const bool isNewFormat =
        cJSON_IsString(formatNode) &&
        formatNode->valuestring != nullptr &&
        SDL_strcasecmp(formatNode->valuestring, "ship_vfx_config") == 0;
    const int formatVersion = cJSON_IsNumber(versionNode)
        ? static_cast<int>(std::llround(versionNode->valuedouble))
        : 0;

    if (!cJSON_IsObject(editorNode))
    {
        this->statusMessage = "Import JSON refuse: cle \"editor\" obligatoire.";
        cJSON_Delete(root);
        return false;
    }
    cJSON* pagesNode = cJSON_GetObjectItemCaseSensitive(editorNode, "layerPages");
    const int layerPageCount = cJSON_IsArray(pagesNode) ? cJSON_GetArraySize(pagesNode) : 0;
    if (!isNewFormat || formatVersion < 3 ||
        (layerPageCount != kShipVfxLayerPageCount && layerPageCount != kShipVfxLayerPageCountNoTargetSectors))
    {
        this->statusMessage =
            "Import JSON refuse: attendu racine { format:\"ship_vfx_config\", version>=3 } et "
            "editor.layerPages[] de exactement " +
            std::to_string(kShipVfxLayerPageCountNoTargetSectors) + " ou " +
            std::to_string(kShipVfxLayerPageCount) + " entrees.";
        cJSON_Delete(root);
        return false;
    }

    this->shipVfxEditorTargetSectorsABEnabled = (layerPageCount == kShipVfxLayerPageCount);
    this->shipVfxEditorTargetingMode =
        targetingModeFromTargetSectorsAB(this->shipVfxEditorTargetSectorsABEnabled);
    this->shipVfxEditorTargetSectorsOverlayVisible = false;
    const cJSON* targetingModeNode = cJSON_GetObjectItemCaseSensitive(editorNode, "targetingMode");
    if (cJSON_IsString(targetingModeNode) && targetingModeNode->valuestring != nullptr)
    {
        bool recognizedMode = false;
        const ShipVfxTargetingMode parsedMode =
            targetingModeFromJsonId(targetingModeNode->valuestring, &recognizedMode);
        if (recognizedMode)
        {
            const bool modeExpectsAB = (parsedMode == ShipVfxTargetingMode::TARGET_RELATIVE_AB);
            if (modeExpectsAB != this->shipVfxEditorTargetSectorsABEnabled)
            {
                this->statusMessage =
                    "Import: editor.targetingMode ne correspond pas au nombre de pages ; on suit layerPages[].";
            }
        }
    }
    const cJSON* targetAbNode = cJSON_GetObjectItemCaseSensitive(editorNode, "targetFireSectorsAB");
    if (cJSON_IsBool(targetAbNode) &&
        ((cJSON_IsTrue(targetAbNode) != 0) != this->shipVfxEditorTargetSectorsABEnabled))
    {
        this->statusMessage =
            "Import: editor.targetFireSectorsAB ne correspond pas au nombre de pages ; on suit layerPages[].";
    }

    this->clearAllShipVfxLayerPages();
    this->clearShipVfxTrailPieces();
    this->initDefaultShipLayerSettingsAllPages();
    this->setSelectedVfxInstanceIndex(-1);
    this->nextVfxInstanceId = 1U;
    this->previewTargetFireSectorIndex = 0;
    const float importedPreviewSpawnTimeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;

    auto findImportedSfxIndex = [this](const std::string& sourceJsonPath, const std::string& displayName) -> int {
        const std::string sourceKey = makeComparableSourcePathKey(sourceJsonPath);
        if (!sourceKey.empty())
        {
            for (size_t i = 0; i < this->importedSfx.size(); ++i)
            {
                if (makeComparableSourcePathKey(this->importedSfx[i].sourceJsonPath) == sourceKey)
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

    auto pushParsedInstance = [this, &findImportedSfxIndex, &parseDirectionOverride, importedPreviewSpawnTimeSeconds](
                                  const cJSON* node,
                                  std::vector<ShipVfxInstance>& targetVec,
                                  int shipDrawOrderForPage) {
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
        if (!cJSON_IsString(sourceJsonNode))
        {
            sourceJsonNode = cJSON_GetObjectItemCaseSensitive(node, "vfxSourceJsonPath");
        }
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
            for (const ShipVfxInstance& existing : targetVec)
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
        instance.behindShip = cJSON_IsBool(behindNode) ? cJSON_IsTrue(behindNode) : (instance.drawOrder < shipDrawOrderForPage);
        instance.followShip = true;
        const cJSON* placementSnapNode = cJSON_GetObjectItemCaseSensitive(node, "placementSnapClickToTile");
        instance.placementSnapClickToTile =
            cJSON_IsBool(placementSnapNode) ? cJSON_IsTrue(placementSnapNode) : false;

        const cJSON* spawnAfterIdNode = cJSON_GetObjectItemCaseSensitive(node, "spawnAfterInstanceId");
        const cJSON* spawnAfterMsNode = cJSON_GetObjectItemCaseSensitive(node, "spawnAfterDelayMs");
        if (cJSON_IsNumber(spawnAfterIdNode) && std::isfinite(spawnAfterIdNode->valuedouble))
        {
            const double v = spawnAfterIdNode->valuedouble;
            instance.spawnAfterInstanceId = (v <= 0.0) ? 0U : static_cast<uint32_t>(std::llround(v));
        }
        if (cJSON_IsNumber(spawnAfterMsNode) && std::isfinite(spawnAfterMsNode->valuedouble))
        {
            instance.spawnAfterDelayMs = static_cast<int>(std::llround(spawnAfterMsNode->valuedouble));
        }
        instance.previewSpawnTimeSeconds = importedPreviewSpawnTimeSeconds;

        const cJSON* motionSpawnCapNode = cJSON_GetObjectItemCaseSensitive(node, "motionSpawnCaptured");
        instance.motionSpawnCaptured = cJSON_IsBool(motionSpawnCapNode) ? cJSON_IsTrue(motionSpawnCapNode) : false;
        if (!cJSON_IsBool(motionSpawnCapNode))
        {
            const cJSON* motionEnNode = cJSON_GetObjectItemCaseSensitive(node, "motionOffsetPreviewEnabled");
            if (cJSON_IsBool(motionEnNode) && cJSON_IsTrue(motionEnNode))
            {
                instance.motionSpawnCaptured = true;
            }
        }
        const cJSON* mSx = cJSON_GetObjectItemCaseSensitive(node, "motionSpawnOffsetX");
        const cJSON* mSy = cJSON_GetObjectItemCaseSensitive(node, "motionSpawnOffsetY");
        if (cJSON_IsNumber(mSx) && std::isfinite(mSx->valuedouble))
        {
            instance.motionSpawnOffsetX = static_cast<float>(mSx->valuedouble);
        }
        if (cJSON_IsNumber(mSy) && std::isfinite(mSy->valuedouble))
        {
            instance.motionSpawnOffsetY = static_cast<float>(mSy->valuedouble);
        }
        const cJSON* trailNNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailEveryNTiles");
        if (cJSON_IsNumber(trailNNode) && std::isfinite(trailNNode->valuedouble))
        {
            instance.motionTrailEveryNTiles = (std::clamp)(
                static_cast<int>(std::llround(trailNNode->valuedouble)),
                kMotionTrailEveryNTilesMin,
                kMotionTrailEveryNTilesMax);
        }
        const cJSON* trailLifeNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailLifetimeTiles");
        if (cJSON_IsNumber(trailLifeNode) && std::isfinite(trailLifeNode->valuedouble) && trailLifeNode->valuedouble > 0.0)
        {
            instance.motionTrailLifetimeTiles =
                static_cast<int>(std::clamp(std::llround(trailLifeNode->valuedouble), 1LL, 4096LL));
        }
        instance.motionTrailDistanceAcc = 0.0f;
        const cJSON* trailJitNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailLateralJitterRadius");
        if (cJSON_IsNumber(trailJitNode) && std::isfinite(trailJitNode->valuedouble))
        {
            instance.motionTrailLateralJitterRadius = static_cast<float>(
                std::clamp(trailJitNode->valuedouble, 0.0, 2048.0));
        }
        else
        {
            instance.motionTrailLateralJitterRadius = 0.0f;
        }
        const cJSON* coneOffXNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailConeOffsetX");
        if (cJSON_IsNumber(coneOffXNode) && std::isfinite(coneOffXNode->valuedouble))
        {
            instance.motionTrailConeOffsetX = static_cast<float>(
                std::clamp(coneOffXNode->valuedouble, -2048.0, 2048.0));
        }
        else
        {
            instance.motionTrailConeOffsetX = 0.0f;
        }
        const cJSON* coneOffYNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailConeOffsetY");
        if (cJSON_IsNumber(coneOffYNode) && std::isfinite(coneOffYNode->valuedouble))
        {
            instance.motionTrailConeOffsetY = static_cast<float>(
                std::clamp(coneOffYNode->valuedouble, -2048.0, 2048.0));
        }
        else
        {
            instance.motionTrailConeOffsetY = 0.0f;
        }
        const cJSON* coneDirNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailConeDirectionOffsetDeg");
        if (cJSON_IsNumber(coneDirNode) && std::isfinite(coneDirNode->valuedouble))
        {
            instance.motionTrailConeDirectionOffsetDeg = static_cast<float>(
                std::clamp(coneDirNode->valuedouble, -179.0, 179.0));
        }
        else
        {
            instance.motionTrailConeDirectionOffsetDeg = 0.0f;
        }
        const cJSON* coneHalfNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailConeHalfAngleDeg");
        if (cJSON_IsNumber(coneHalfNode) && std::isfinite(coneHalfNode->valuedouble))
        {
            instance.motionTrailConeHalfAngleDeg = static_cast<float>(
                std::clamp(coneHalfNode->valuedouble, 2.0, 85.0));
        }
        else
        {
            instance.motionTrailConeHalfAngleDeg = 28.0f;
        }
        const cJSON* coneCountNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailConeSpawnCount");
        if (cJSON_IsNumber(coneCountNode) && std::isfinite(coneCountNode->valuedouble))
        {
            instance.motionTrailConeSpawnCount =
                std::clamp(static_cast<int>(std::llround(coneCountNode->valuedouble)), 1, 32);
        }
        else
        {
            instance.motionTrailConeSpawnCount = 1;
        }
        const cJSON* strictTileNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailStrictTilePlacement");
        if (cJSON_IsBool(strictTileNode))
        {
            instance.motionTrailStrictTilePlacement = cJSON_IsTrue(strictTileNode);
        }
        else
        {
            instance.motionTrailStrictTilePlacement = (instance.motionTrailLateralJitterRadius <= 0.0001f);
        }
        const cJSON* rotPctNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailRotationRandomPercent");
        if (cJSON_IsNumber(rotPctNode) && std::isfinite(rotPctNode->valuedouble))
        {
            instance.motionTrailRotationRandomPercent =
                std::clamp(static_cast<int>(std::llround(rotPctNode->valuedouble)), 0, 100);
        }
        else
        {
            instance.motionTrailRotationRandomPercent = 0;
        }
        const cJSON* idleRingNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleRingWhenStationary");
        instance.motionTrailIdleRingWhenStationary =
            cJSON_IsBool(idleRingNode) ? cJSON_IsTrue(idleRingNode) : false;
        const cJSON* idleRadNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleRingRadius");
        if (cJSON_IsNumber(idleRadNode) && std::isfinite(idleRadNode->valuedouble))
        {
            instance.motionTrailIdleRingRadius =
                static_cast<float>(std::clamp(idleRadNode->valuedouble, 1.0, 2048.0));
        }
        else
        {
            instance.motionTrailIdleRingRadius = 48.0f;
        }
        const cJSON* idlePerNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleSpawnPeriodMs");
        if (cJSON_IsNumber(idlePerNode) && std::isfinite(idlePerNode->valuedouble))
        {
            instance.motionTrailIdleSpawnPeriodMs =
                static_cast<int>(std::clamp(std::llround(idlePerNode->valuedouble), 100LL, 60000LL));
        }
        else
        {
            instance.motionTrailIdleSpawnPeriodMs = 600;
        }
        const cJSON* idleRingPcNode = cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleRingPieceCount");
        if (cJSON_IsNumber(idleRingPcNode) && std::isfinite(idleRingPcNode->valuedouble))
        {
            instance.motionTrailIdleRingPieceCount =
                std::clamp(static_cast<int>(std::llround(idleRingPcNode->valuedouble)), 1, 32);
        }
        else
        {
            instance.motionTrailIdleRingPieceCount = 8;
        }
        instance.motionTrailIdleRingRotationRandomPercent = instance.motionTrailRotationRandomPercent;
        const cJSON* idleRingRotPctNode =
            cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleRingRotationRandomPercent");
        if (cJSON_IsNumber(idleRingRotPctNode) && std::isfinite(idleRingRotPctNode->valuedouble))
        {
            instance.motionTrailIdleRingRotationRandomPercent =
                std::clamp(static_cast<int>(std::llround(idleRingRotPctNode->valuedouble)), 0, 100);
        }
        const cJSON* idleRingPosJitNode =
            cJSON_GetObjectItemCaseSensitive(node, "motionTrailIdleRingPositionJitterRadius");
        if (cJSON_IsNumber(idleRingPosJitNode) && std::isfinite(idleRingPosJitNode->valuedouble))
        {
            instance.motionTrailIdleRingPositionJitterRadius = static_cast<float>(
                std::clamp(idleRingPosJitNode->valuedouble, 0.0, 2048.0));
        }
        else
        {
            instance.motionTrailIdleRingPositionJitterRadius = 0.0f;
        }
        instance.motionTrailIdleSpawnAccSec = 0.0f;

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
        targetVec.push_back(std::move(instance));
    };

    auto applyPreviewFromJsonRoot = [this](const cJSON* fileRoot) {
        const cJSON* previewNode = cJSON_GetObjectItemCaseSensitive(fileRoot, "preview");
        if (!cJSON_IsObject(previewNode))
        {
            return;
        }
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
        const cJSON* sectorNode = cJSON_GetObjectItemCaseSensitive(previewNode, "targetFireSector");
        if (cJSON_IsNumber(sectorNode) && std::isfinite(sectorNode->valuedouble))
        {
            this->previewTargetFireSectorIndex =
                std::clamp(static_cast<int>(std::llround(sectorNode->valuedouble)), 0, 1);
        }
    };

    auto applyShipLayerToPage = [this](const cJSON* shipLayerNode, int pageIndex) {
        if (shipLayerNode == nullptr || !cJSON_IsObject(shipLayerNode))
        {
            return;
        }
        const size_t pi = static_cast<size_t>(std::clamp(pageIndex, 0, kShipVfxLayerPageCount - 1));
        const cJSON* shipOrderNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "drawOrder");
        const cJSON* shipVisibleNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "visible");
        const cJSON* shipLockedNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "locked");
        const cJSON* shipDebugNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "debugBoundsVisible");
        const cJSON* shipSpawnAfterIdNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "spawnAfterVfxInstanceId");
        const cJSON* shipSpawnAfterMsNode = cJSON_GetObjectItemCaseSensitive(shipLayerNode, "spawnAfterDelayMs");
        if (cJSON_IsNumber(shipOrderNode))
        {
            this->shipDrawOrderByPage[pi] = static_cast<int>(std::llround(shipOrderNode->valuedouble));
        }
        if (cJSON_IsBool(shipVisibleNode))
        {
            this->shipLayerVisibleByPage[pi] = cJSON_IsTrue(shipVisibleNode);
        }
        if (cJSON_IsBool(shipLockedNode))
        {
            this->shipLayerLockedByPage[pi] = cJSON_IsTrue(shipLockedNode);
        }
        if (cJSON_IsBool(shipDebugNode))
        {
            this->shipDebugBoundsVisibleByPage[pi] = cJSON_IsTrue(shipDebugNode);
        }
        if (cJSON_IsNumber(shipSpawnAfterIdNode) && std::isfinite(shipSpawnAfterIdNode->valuedouble))
        {
            const double v = shipSpawnAfterIdNode->valuedouble;
            this->shipSpawnAfterVfxInstanceId[pi] =
                (v <= 0.0) ? 0U : static_cast<uint32_t>(std::llround(v));
        }
        if (cJSON_IsNumber(shipSpawnAfterMsNode) && std::isfinite(shipSpawnAfterMsNode->valuedouble))
        {
            this->shipSpawnAfterDelayMs[pi] = static_cast<int>(std::llround(shipSpawnAfterMsNode->valuedouble));
        }
    };

    applyPreviewFromJsonRoot(root);

    int pageIdx = 0;
    cJSON* pageEntry = nullptr;
    cJSON_ArrayForEach(pageEntry, pagesNode)
    {
        if (!cJSON_IsObject(pageEntry))
        {
            this->statusMessage =
                "Import JSON refuse: editor.layerPages[" + std::to_string(pageIdx) + "] doit etre un objet.";
            cJSON_Delete(root);
            return false;
        }
        const cJSON* pageShipLayer = cJSON_GetObjectItemCaseSensitive(pageEntry, "shipLayer");
        applyShipLayerToPage(pageShipLayer, pageIdx);

        const int shipZ = this->shipDrawOrderByPage[static_cast<size_t>(pageIdx)];
        std::vector<ShipVfxInstance>& pageVec = this->shipVfxLayerPages[static_cast<size_t>(pageIdx)];
        const cJSON* instancesNode = cJSON_GetObjectItemCaseSensitive(pageEntry, "vfxInstances");
        if (cJSON_IsArray(instancesNode))
        {
            cJSON* instNode = nullptr;
            cJSON_ArrayForEach(instNode, instancesNode)
            {
                pushParsedInstance(instNode, pageVec, shipZ);
            }
        }
        pageIdx += 1;
    }

    if (!this->shipVfxEditorTargetSectorsABEnabled)
    {
        this->previewTargetFireSectorIndex = 0;
    }

    cJSON_Delete(root);
    this->normalizeShipVfxDrawOrders();
    if (!this->currentShipVfxLayers().empty())
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
            this->clearAllShipVfxLayerPages();
            this->clearShipVfxTrailPieces();
            this->initDefaultShipLayerSettingsAllPages();
            this->setSelectedVfxInstanceIndex(-1);
            this->nextVfxInstanceId = 1U;
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
    return this->exportShipVfxJsonToFolderForShipIndex(
        this->selectedShipIndex,
        absoluteFolderPath,
        true,
        nullptr,
        nullptr);
}

bool EditorMapVfxScene::exportShipVfxJsonToFolderForShipIndex(
    int shipIndex,
    const char* absoluteFolderPath,
    bool updateStatusMessage,
    int* outExportedFileCount,
    int* outFailedFileCount)
{
    if (outExportedFileCount != nullptr)
    {
        *outExportedFileCount = 0;
    }
    if (outFailedFileCount != nullptr)
    {
        *outFailedFileCount = 0;
    }

    if (shipIndex < 0 || shipIndex >= static_cast<int>(this->importedShips.size()))
    {
        if (updateStatusMessage)
        {
            this->statusMessage = "Charge d'abord un navire avant export JSON.";
        }
        return false;
    }
    if (absoluteFolderPath == nullptr || absoluteFolderPath[0] == '\0')
    {
        if (updateStatusMessage)
        {
            this->statusMessage = "Dossier export invalide.";
        }
        return false;
    }

    std::filesystem::path folderPath(absoluteFolderPath);
    std::error_code fsError;
    std::filesystem::create_directories(folderPath, fsError);
    if (fsError)
    {
        if (updateStatusMessage)
        {
            this->statusMessage = "Impossible de creer le dossier export.";
        }
        return false;
    }

    const ImportedShip& ship = this->importedShips[static_cast<size_t>(shipIndex)];
    const std::string shipSlug = makeShipConfigSlug(ship.displayName);

    struct ExportAnimationGroup
    {
        std::string key;
        int importedSfxIndex = -1;
        std::string displayName;
        std::string sourceJsonPath;
        float defaultFps = 12.0f;
        bool animationTotalDurationInfinite = true;
        int animationTotalDurationMs = 0;
    };

    auto buildInstanceGroupKey = [this](const ShipVfxInstance& instance) -> std::string {
        if (instance.importedSfxIndex >= 0 &&
            instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            return "idx:" + std::to_string(instance.importedSfxIndex);
        }
        const std::string sourcePathKey = makeComparableSourcePathKey(instance.sourceJsonPath);
        if (!sourcePathKey.empty())
        {
            return "path:" + sourcePathKey;
        }
        const std::string displayKey = makePathKeyLower(trimAscii(instance.sourceDisplayName));
        if (!displayKey.empty())
        {
            return "name:" + displayKey;
        }
        return "id:" + std::to_string(instance.instanceId);
    };

    struct ExportSourceState
    {
        std::array<std::vector<ShipVfxInstance>, kShipVfxLayerPageCount> layerPages{};
        std::array<int, kShipVfxLayerPageCount> drawOrderByPage{};
        std::array<bool, kShipVfxLayerPageCount> layerVisibleByPage{};
        std::array<bool, kShipVfxLayerPageCount> layerLockedByPage{};
        std::array<bool, kShipVfxLayerPageCount> debugBoundsVisibleByPage{};
        std::array<uint32_t, kShipVfxLayerPageCount> shipSpawnAfterVfxInstanceIdByPage{};
        std::array<int, kShipVfxLayerPageCount> shipSpawnAfterDelayMsByPage{};
        bool targetSectorsABEnabled = false;
        ShipVfxTargetingMode targetingMode = ShipVfxTargetingMode::NONE;
    };

    this->saveCurrentShipVfxPairDraft();

    std::vector<ExportSourceState> exportSources;
    exportSources.reserve(this->shipVfxPairDraftStates.size() + 1U);
    for (const ShipVfxPairDraftState& draft : this->shipVfxPairDraftStates)
    {
        if (draft.shipIndex != shipIndex)
        {
            continue;
        }

        ExportSourceState source{};
        source.layerPages = draft.layerPages;
        source.drawOrderByPage = draft.drawOrderByPage;
        source.layerVisibleByPage = draft.layerVisibleByPage;
        source.layerLockedByPage = draft.layerLockedByPage;
        source.debugBoundsVisibleByPage = draft.debugBoundsVisibleByPage;
        source.shipSpawnAfterVfxInstanceIdByPage = draft.shipSpawnAfterInstanceIdByPage;
        source.shipSpawnAfterDelayMsByPage = draft.shipSpawnAfterDelayMsByPage;
        source.targetSectorsABEnabled = draft.targetSectorsABEnabled;
        source.targetingMode = draft.targetingMode;
        exportSources.push_back(std::move(source));
    }

    if (exportSources.empty() && shipIndex == this->selectedShipIndex)
    {
        ExportSourceState currentSource{};
        currentSource.layerPages = this->shipVfxLayerPages;
        currentSource.drawOrderByPage = this->shipDrawOrderByPage;
        currentSource.layerVisibleByPage = this->shipLayerVisibleByPage;
        currentSource.layerLockedByPage = this->shipLayerLockedByPage;
        currentSource.debugBoundsVisibleByPage = this->shipDebugBoundsVisibleByPage;
        currentSource.shipSpawnAfterVfxInstanceIdByPage = this->shipSpawnAfterVfxInstanceId;
        currentSource.shipSpawnAfterDelayMsByPage = this->shipSpawnAfterDelayMs;
        currentSource.targetSectorsABEnabled = this->shipVfxEditorTargetSectorsABEnabled;
        currentSource.targetingMode = this->shipVfxEditorTargetingMode;
        exportSources.push_back(std::move(currentSource));
    }

    std::array<std::vector<ShipVfxInstance>, kShipVfxLayerPageCount> mergedLayerPages{};
    std::array<int, kShipVfxLayerPageCount> shipDrawOrderForExport{};
    std::array<bool, kShipVfxLayerPageCount> shipLayerVisibleForExport{};
    std::array<bool, kShipVfxLayerPageCount> shipLayerLockedForExport{};
    std::array<bool, kShipVfxLayerPageCount> shipDebugBoundsVisibleForExport{};
    std::array<uint32_t, kShipVfxLayerPageCount> shipSpawnAfterVfxInstanceIdForExport{};
    std::array<int, kShipVfxLayerPageCount> shipSpawnAfterDelayMsForExport{};
    shipDrawOrderForExport.fill(0);
    shipLayerVisibleForExport.fill(true);
    shipLayerLockedForExport.fill(false);
    shipDebugBoundsVisibleForExport.fill(false);
    shipSpawnAfterVfxInstanceIdForExport.fill(0U);
    shipSpawnAfterDelayMsForExport.fill(0);

    bool targetSectorsABForExport = false;
    bool haveShipLayerSettingsForExport = false;
    for (const ExportSourceState& source : exportSources)
    {
        if (!haveShipLayerSettingsForExport)
        {
            shipDrawOrderForExport = source.drawOrderByPage;
            shipLayerVisibleForExport = source.layerVisibleByPage;
            shipLayerLockedForExport = source.layerLockedByPage;
            shipDebugBoundsVisibleForExport = source.debugBoundsVisibleByPage;
            shipSpawnAfterVfxInstanceIdForExport = source.shipSpawnAfterVfxInstanceIdByPage;
            shipSpawnAfterDelayMsForExport = source.shipSpawnAfterDelayMsByPage;
            haveShipLayerSettingsForExport = true;
        }

        const int sourcePageCount = source.targetSectorsABEnabled ? kShipVfxLayerPageCount : kShipVfxLayerPageCountNoTargetSectors;
        for (int p = 0; p < sourcePageCount; ++p)
        {
            std::vector<ShipVfxInstance>& mergedPage = mergedLayerPages[static_cast<size_t>(p)];
            const std::vector<ShipVfxInstance>& sourcePage = source.layerPages[static_cast<size_t>(p)];
            mergedPage.insert(mergedPage.end(), sourcePage.begin(), sourcePage.end());
        }
        targetSectorsABForExport = targetSectorsABForExport || source.targetSectorsABEnabled;
    }

    const int exportPageCount =
        targetSectorsABForExport ? kShipVfxLayerPageCount : kShipVfxLayerPageCountNoTargetSectors;
    ShipVfxTargetingMode targetingModeForExport = targetingModeFromTargetSectorsAB(targetSectorsABForExport);
    const auto* layerPagesForExport = &mergedLayerPages;

    std::vector<ExportAnimationGroup> groups;
    for (int p = 0; p < exportPageCount; ++p)
    {
        for (const ShipVfxInstance& instance : (*layerPagesForExport)[static_cast<size_t>(p)])
        {
            const std::string key = buildInstanceGroupKey(instance);
            auto it = std::find_if(groups.begin(), groups.end(), [&key](const ExportAnimationGroup& group) {
                return group.key == key;
            });
            if (it != groups.end())
            {
                continue;
            }

            ExportAnimationGroup group{};
            group.key = key;
            group.importedSfxIndex = instance.importedSfxIndex;
            if (instance.importedSfxIndex >= 0 &&
                instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
            {
                const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
                group.displayName = imported.displayName;
                group.sourceJsonPath = imported.sourceJsonPath;
                group.defaultFps = imported.defaultFps;
                group.animationTotalDurationInfinite = imported.animationTotalDurationInfinite;
                group.animationTotalDurationMs = imported.animationTotalDurationMs;
            }
            else
            {
                group.displayName = trimAscii(instance.sourceDisplayName);
                group.sourceJsonPath = instance.sourceJsonPath;
            }

            if (group.displayName.empty())
            {
                group.displayName = trimAscii(instance.label);
            }
            if (group.displayName.empty())
            {
                group.displayName = "vfx";
            }
            groups.push_back(std::move(group));
        }
    }

    const std::string shipFolderRelativePath =
        buildAssetsRelativePathForExport(ship.folderAbsolutePath, ship.displayName);

    auto addEditorInstanceJson = [this](cJSON* instancesArray, const ShipVfxInstance& instance) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "instanceId", static_cast<double>(instance.instanceId));
        cJSON_AddStringToObject(item, "label", instance.label.c_str());
        cJSON_AddStringToObject(item, "displayName", instance.sourceDisplayName.c_str());

        std::string sourceJsonPath = instance.sourceJsonPath;
        if (instance.importedSfxIndex >= 0 &&
            instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            sourceJsonPath =
                this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)].sourceJsonPath;
        }
        sourceJsonPath = buildAssetsRelativePathForExport(sourceJsonPath, sourceJsonPath);
        cJSON_AddStringToObject(item, "sourceJsonPath", sourceJsonPath.c_str());

        cJSON_AddNumberToObject(item, "offsetX", instance.offsetX);
        cJSON_AddNumberToObject(item, "offsetY", instance.offsetY);
        cJSON_AddNumberToObject(item, "rotationDeg", instance.rotationDeg);
        cJSON_AddBoolToObject(item, "flipHorizontal", instance.flipHorizontal);
        cJSON_AddBoolToObject(item, "flipVertical", instance.flipVertical);
        cJSON_AddNumberToObject(item, "drawOrder", instance.drawOrder);
        cJSON_AddBoolToObject(item, "visible", instance.visible);
        cJSON_AddBoolToObject(item, "debugBoundsVisible", instance.debugBoundsVisible);
        cJSON_AddBoolToObject(item, "placementSnapClickToTile", instance.placementSnapClickToTile);
        cJSON_AddBoolToObject(item, "locked", instance.locked);
        cJSON_AddBoolToObject(item, "followShip", instance.followShip);
        cJSON_AddBoolToObject(item, "sharedForAllDirections", instance.sharedForAllDirections);
        cJSON_AddBoolToObject(item, "sharedForAllStates", instance.sharedForAllStates);
        cJSON_AddNumberToObject(item, "spawnAfterInstanceId", static_cast<double>(instance.spawnAfterInstanceId));
        cJSON_AddNumberToObject(item, "spawnAfterDelayMs", static_cast<double>(instance.spawnAfterDelayMs));
        cJSON_AddBoolToObject(item, "motionOffsetPreviewEnabled", instance.motionSpawnCaptured);
        cJSON_AddBoolToObject(item, "motionSpawnCaptured", instance.motionSpawnCaptured);
        cJSON_AddNumberToObject(item, "motionSpawnOffsetX", static_cast<double>(instance.motionSpawnOffsetX));
        cJSON_AddNumberToObject(item, "motionSpawnOffsetY", static_cast<double>(instance.motionSpawnOffsetY));
        cJSON_AddNumberToObject(
            item, "motionTrailEveryNTiles", static_cast<double>(instance.motionTrailEveryNTiles));
        cJSON_AddNumberToObject(item, "motionTrailLifetimeTiles", static_cast<double>(instance.motionTrailLifetimeTiles));
        cJSON_AddNumberToObject(
            item, "motionTrailLateralJitterRadius", static_cast<double>(instance.motionTrailLateralJitterRadius));
        cJSON_AddNumberToObject(
            item, "motionTrailConeOffsetX", static_cast<double>(instance.motionTrailConeOffsetX));
        cJSON_AddNumberToObject(
            item, "motionTrailConeOffsetY", static_cast<double>(instance.motionTrailConeOffsetY));
        cJSON_AddNumberToObject(
            item, "motionTrailConeDirectionOffsetDeg", static_cast<double>(instance.motionTrailConeDirectionOffsetDeg));
        cJSON_AddNumberToObject(
            item, "motionTrailConeHalfAngleDeg", static_cast<double>(instance.motionTrailConeHalfAngleDeg));
        cJSON_AddNumberToObject(
            item, "motionTrailConeSpawnCount", static_cast<double>(instance.motionTrailConeSpawnCount));
        cJSON_AddBoolToObject(item, "motionTrailStrictTilePlacement", instance.motionTrailStrictTilePlacement);
        cJSON_AddNumberToObject(
            item, "motionTrailRotationRandomPercent", static_cast<double>(instance.motionTrailRotationRandomPercent));
        cJSON_AddBoolToObject(item, "motionTrailIdleRingWhenStationary", instance.motionTrailIdleRingWhenStationary);
        cJSON_AddNumberToObject(item, "motionTrailIdleRingRadius", static_cast<double>(instance.motionTrailIdleRingRadius));
        cJSON_AddNumberToObject(
            item, "motionTrailIdleSpawnPeriodMs", static_cast<double>(instance.motionTrailIdleSpawnPeriodMs));
        cJSON_AddNumberToObject(
            item, "motionTrailIdleRingPieceCount", static_cast<double>(instance.motionTrailIdleRingPieceCount));
        cJSON_AddNumberToObject(
            item,
            "motionTrailIdleRingRotationRandomPercent",
            static_cast<double>(instance.motionTrailIdleRingRotationRandomPercent));
        cJSON_AddNumberToObject(
            item,
            "motionTrailIdleRingPositionJitterRadius",
            static_cast<double>(instance.motionTrailIdleRingPositionJitterRadius));

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
    };

    std::vector<std::string> usedFileNames;
    std::vector<std::string> exportedPaths;
    std::vector<std::string> exportedFileNameKeys;
    int failedCount = 0;
    for (const ExportAnimationGroup& group : groups)
    {
        std::string normalizedVfxName = stripListPrefix(group.displayName, "vfx-");
        if (trimAscii(normalizedVfxName).empty())
        {
            normalizedVfxName = extractFileName(group.sourceJsonPath);
        }
        std::string vfxSlug = makeExportAnimationSlug(normalizedVfxName);
        if (vfxSlug.empty())
        {
            vfxSlug = "vfx";
        }
        const std::string vfxId = "fx-" + vfxSlug;
        std::string fileStem = vfxId + "_" + shipSlug;
        std::string fileName = fileStem + ".json";
        int suffix = 2;
        while (std::find(usedFileNames.begin(), usedFileNames.end(), makePathKeyLower(fileName)) != usedFileNames.end())
        {
            fileName = fileStem + "-" + std::to_string(suffix) + ".json";
            suffix += 1;
        }
        usedFileNames.push_back(makePathKeyLower(fileName));

        const std::filesystem::path jsonPath = folderPath / fileName;
        cJSON* root = cJSON_CreateObject();
        if (root == nullptr)
        {
            failedCount += 1;
            continue;
        }

        cJSON_AddStringToObject(root, "format", "ship_vfx_config");
        cJSON_AddNumberToObject(root, "version", 3);
        cJSON* previewRoot = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "preview", previewRoot);
        cJSON_AddStringToObject(previewRoot, "direction", getDirectionIdByIndex(this->previewDirectionIndex));
        cJSON_AddStringToObject(
            previewRoot,
            "state",
            (this->previewShipStateIndex == 1) ? "damaged" : "healthy");
        cJSON_AddNumberToObject(
            previewRoot,
            "targetFireSector",
            static_cast<double>(
                targetSectorsABForExport ? this->previewTargetFireSectorIndex : 0));

        cJSON* editorJson = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "editor", editorJson);
        cJSON_AddStringToObject(editorJson, "schema", "scene-editormap-vfx");
        cJSON_AddNumberToObject(editorJson, "schemaVersion", 2);
        cJSON_AddBoolToObject(editorJson, "targetFireSectorsAB", targetSectorsABForExport);
        cJSON_AddStringToObject(
            editorJson,
            "targetingMode",
            targetingModeToJsonId(targetingModeForExport));

        cJSON* shipNode = cJSON_CreateObject();
        cJSON_AddItemToObject(editorJson, "ship", shipNode);
        cJSON_AddStringToObject(shipNode, "displayName", ship.displayName.c_str());
        cJSON_AddStringToObject(shipNode, "folderPath", shipFolderRelativePath.c_str());

        std::string sourceJsonPathForGroup =
            buildAssetsRelativePathForExport(group.sourceJsonPath, group.displayName + ".json");
        std::string vfxFolderPathForGroup = "assets";
        {
            const std::filesystem::path sourceJsonPath(sourceJsonPathForGroup);
            const std::string sourceFolderPath = normalizePathSlashes(sourceJsonPath.parent_path().string());
            if (!sourceFolderPath.empty())
            {
                vfxFolderPathForGroup = sourceFolderPath;
            }
        }
        cJSON* animationNode = cJSON_CreateObject();
        cJSON_AddItemToObject(editorJson, "animation", animationNode);
        cJSON_AddStringToObject(animationNode, "displayName", group.displayName.c_str());
        cJSON_AddStringToObject(animationNode, "sourceJsonPath", sourceJsonPathForGroup.c_str());
        cJSON_AddNumberToObject(animationNode, "defaultVfxFps", group.defaultFps);

        cJSON* layerPagesArray = cJSON_CreateArray();
        cJSON_AddItemToObject(editorJson, "layerPages", layerPagesArray);

        std::vector<uint32_t> groupInstanceIds;
        for (int p = 0; p < exportPageCount; ++p)
        {
            for (const ShipVfxInstance& instance : (*layerPagesForExport)[static_cast<size_t>(p)])
            {
                if (buildInstanceGroupKey(instance) == group.key)
                {
                    groupInstanceIds.push_back(instance.instanceId);
                }
            }
        }

        for (int p = 0; p < exportPageCount; ++p)
        {
            cJSON* pageObj = cJSON_CreateObject();
            cJSON_AddItemToArray(layerPagesArray, pageObj);

            cJSON* shipLayerPage = cJSON_CreateObject();
            cJSON_AddItemToObject(pageObj, "shipLayer", shipLayerPage);
            cJSON_AddNumberToObject(shipLayerPage, "drawOrder", shipDrawOrderForExport[static_cast<size_t>(p)]);
            cJSON_AddBoolToObject(shipLayerPage, "visible", shipLayerVisibleForExport[static_cast<size_t>(p)]);
            cJSON_AddBoolToObject(shipLayerPage, "locked", shipLayerLockedForExport[static_cast<size_t>(p)]);
            cJSON_AddBoolToObject(
                shipLayerPage,
                "debugBoundsVisible",
                shipDebugBoundsVisibleForExport[static_cast<size_t>(p)]);
            cJSON_AddNumberToObject(
                shipLayerPage,
                "spawnAfterVfxInstanceId",
                static_cast<double>(shipSpawnAfterVfxInstanceIdForExport[static_cast<size_t>(p)]));
            cJSON_AddNumberToObject(
                shipLayerPage,
                "spawnAfterDelayMs",
                static_cast<double>(shipSpawnAfterDelayMsForExport[static_cast<size_t>(p)]));

            cJSON* instancesArray = cJSON_CreateArray();
            cJSON_AddItemToObject(pageObj, "vfxInstances", instancesArray);
            for (const ShipVfxInstance& instance : (*layerPagesForExport)[static_cast<size_t>(p)])
            {
                if (buildInstanceGroupKey(instance) == group.key)
                {
                    addEditorInstanceJson(instancesArray, instance);
                }
            }
        }

        cJSON* gameplayJson = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "gameplay", gameplayJson);
        cJSON_AddStringToObject(gameplayJson, "shipFolderPath", shipFolderRelativePath.c_str());
        cJSON_AddStringToObject(gameplayJson, "vfxFolderPath", vfxFolderPathForGroup.c_str());
        cJSON_AddNumberToObject(gameplayJson, "defaultVfxFps", group.defaultFps);
        if (!group.animationTotalDurationInfinite && group.animationTotalDurationMs > 0)
        {
            cJSON_AddNumberToObject(
                gameplayJson,
                "animationTotalDurationMs",
                static_cast<double>(group.animationTotalDurationMs));
        }
        else
        {
            cJSON_AddNullToObject(gameplayJson, "animationTotalDurationMs");
        }
        cJSON_AddStringToObject(
            gameplayJson,
            "targetingMode",
            targetingModeToJsonId(targetingModeForExport));

        cJSON* gameplayDirectionStatesArray = cJSON_CreateArray();
        cJSON_AddItemToObject(gameplayJson, "directionStates", gameplayDirectionStatesArray);
        for (int p = 0; p < exportPageCount; ++p)
        {
            cJSON* gameplayPageObj = cJSON_CreateObject();
            cJSON_AddItemToArray(gameplayDirectionStatesArray, gameplayPageObj);

            cJSON* gameplayShipNode = cJSON_CreateObject();
            cJSON_AddItemToObject(gameplayPageObj, "ship", gameplayShipNode);
            const int stateBand = (p / 4) % 2;
            const int fireSector = targetSectorsABForExport ? (p / 8) : 0;
            cJSON_AddStringToObject(gameplayShipNode, "direction", getDirectionIdByIndex(p % 4));
            cJSON_AddStringToObject(gameplayShipNode, "state", (stateBand == 1) ? "damaged" : "healthy");
            cJSON_AddNumberToObject(gameplayShipNode, "targetFireSector", static_cast<double>(fireSector));
            cJSON_AddNumberToObject(gameplayShipNode, "drawOrder", shipDrawOrderForExport[static_cast<size_t>(p)]);

            cJSON* gameplayInstancesArray = cJSON_CreateArray();
            cJSON_AddItemToObject(gameplayPageObj, "instances", gameplayInstancesArray);
            const int directionIndex = p % 4;
            for (const ShipVfxInstance& instance : (*layerPagesForExport)[static_cast<size_t>(p)])
            {
                if (buildInstanceGroupKey(instance) != group.key)
                {
                    continue;
                }

                const DirectionOverride* resolvedDirectionOverride = nullptr;
                if (!instance.sharedForAllDirections)
                {
                    const DirectionOverride& directionOverride =
                        instance.directionOverrides[static_cast<size_t>(directionIndex)];
                    if (directionOverride.enabled)
                    {
                        resolvedDirectionOverride = &directionOverride;
                    }
                }

                cJSON* gameplayInstance = cJSON_CreateObject();
                cJSON_AddItemToArray(gameplayInstancesArray, gameplayInstance);
                cJSON_AddNumberToObject(gameplayInstance, "instanceId", static_cast<double>(instance.instanceId));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "offsetX",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->offsetX : instance.offsetX);
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "offsetY",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->offsetY : instance.offsetY);
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "rotationDeg",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->rotationDeg : instance.rotationDeg);
                cJSON_AddBoolToObject(
                    gameplayInstance,
                    "flipHorizontal",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->flipHorizontal : instance.flipHorizontal);
                cJSON_AddBoolToObject(
                    gameplayInstance,
                    "flipVertical",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->flipVertical : instance.flipVertical);
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "drawOrder",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->drawOrder : instance.drawOrder);
                cJSON_AddBoolToObject(
                    gameplayInstance,
                    "visible",
                    (resolvedDirectionOverride != nullptr) ? resolvedDirectionOverride->visible : instance.visible);

                uint32_t spawnAfterInstanceId = instance.spawnAfterInstanceId;
                if (spawnAfterInstanceId != 0U &&
                    std::find(groupInstanceIds.begin(), groupInstanceIds.end(), spawnAfterInstanceId) == groupInstanceIds.end())
                {
                    spawnAfterInstanceId = 0U;
                }
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "spawnAfterInstanceId",
                    static_cast<double>(spawnAfterInstanceId));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "spawnAfterDelayMs",
                    static_cast<double>(instance.spawnAfterDelayMs));
                cJSON_AddBoolToObject(gameplayInstance, "motionOffsetPreviewEnabled", instance.motionSpawnCaptured);
                cJSON_AddBoolToObject(gameplayInstance, "motionSpawnCaptured", instance.motionSpawnCaptured);
                cJSON_AddNumberToObject(gameplayInstance, "motionSpawnOffsetX", static_cast<double>(instance.motionSpawnOffsetX));
                cJSON_AddNumberToObject(gameplayInstance, "motionSpawnOffsetY", static_cast<double>(instance.motionSpawnOffsetY));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailEveryNTiles",
                    static_cast<double>(instance.motionTrailEveryNTiles));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailLifetimeTiles",
                    static_cast<double>(instance.motionTrailLifetimeTiles));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailLateralJitterRadius",
                    static_cast<double>(instance.motionTrailLateralJitterRadius));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailConeOffsetX",
                    static_cast<double>(instance.motionTrailConeOffsetX));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailConeOffsetY",
                    static_cast<double>(instance.motionTrailConeOffsetY));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailConeDirectionOffsetDeg",
                    static_cast<double>(instance.motionTrailConeDirectionOffsetDeg));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailConeHalfAngleDeg",
                    static_cast<double>(instance.motionTrailConeHalfAngleDeg));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailConeSpawnCount",
                    static_cast<double>(instance.motionTrailConeSpawnCount));
                cJSON_AddBoolToObject(
                    gameplayInstance, "motionTrailStrictTilePlacement", instance.motionTrailStrictTilePlacement);
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailRotationRandomPercent",
                    static_cast<double>(instance.motionTrailRotationRandomPercent));
                cJSON_AddBoolToObject(
                    gameplayInstance,
                    "motionTrailIdleRingWhenStationary",
                    instance.motionTrailIdleRingWhenStationary);
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailIdleRingRadius",
                    static_cast<double>(instance.motionTrailIdleRingRadius));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailIdleSpawnPeriodMs",
                    static_cast<double>(instance.motionTrailIdleSpawnPeriodMs));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailIdleRingPieceCount",
                    static_cast<double>(instance.motionTrailIdleRingPieceCount));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailIdleRingRotationRandomPercent",
                    static_cast<double>(instance.motionTrailIdleRingRotationRandomPercent));
                cJSON_AddNumberToObject(
                    gameplayInstance,
                    "motionTrailIdleRingPositionJitterRadius",
                    static_cast<double>(instance.motionTrailIdleRingPositionJitterRadius));
            }
        }

        char* jsonText = cJSON_Print(root);
        cJSON_Delete(root);
        if (jsonText == nullptr)
        {
            failedCount += 1;
            continue;
        }

        std::ofstream output(jsonPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            cJSON_free(jsonText);
            failedCount += 1;
            continue;
        }
        output.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
        const bool ok = output.good();
        output.close();
        cJSON_free(jsonText);

        if (!ok)
        {
            failedCount += 1;
            continue;
        }

        exportedPaths.push_back(normalizePathSlashes(jsonPath.string()));
        exportedFileNameKeys.push_back(makePathKeyLower(fileName));
    }

    // Complete l'export avec les JSON "fx-*.json" deja presents dans le dossier du navire
    // (animations non chargees dans l'editeur, donc non modifiees).
    const std::filesystem::path shipFolderPath(ship.folderAbsolutePath);
    if (std::filesystem::exists(shipFolderPath, fsError) && std::filesystem::is_directory(shipFolderPath, fsError))
    {
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(shipFolderPath, fsError))
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
            if (ext != ".json")
            {
                continue;
            }

            const std::string sourceFileName = entry.path().filename().string();
            const std::string sourceFileNameKey = makePathKeyLower(sourceFileName);
            if (sourceFileNameKey.rfind("fx-", 0U) != 0U)
            {
                continue;
            }
            if (std::find(exportedFileNameKeys.begin(), exportedFileNameKeys.end(), sourceFileNameKey) != exportedFileNameKeys.end())
            {
                continue;
            }

            std::string sourceText;
            const std::string sourcePathNorm = normalizePathSlashes(entry.path().string());
            if (!readTextFileUtf8(sourcePathNorm, &sourceText))
            {
                continue;
            }
            cJSON* sourceRoot = cJSON_Parse(sourceText.c_str());
            if (sourceRoot == nullptr)
            {
                continue;
            }
            const cJSON* formatNode = cJSON_GetObjectItemCaseSensitive(sourceRoot, "format");
            const bool isShipVfxConfig =
                cJSON_IsString(formatNode) &&
                formatNode->valuestring != nullptr &&
                std::strcmp(formatNode->valuestring, "ship_vfx_config") == 0;
            bool hasAnyInstance = false;
            if (isShipVfxConfig)
            {
                const cJSON* editorNode = cJSON_GetObjectItemCaseSensitive(sourceRoot, "editor");
                const cJSON* layerPagesNode = cJSON_GetObjectItemCaseSensitive(editorNode, "layerPages");
                if (cJSON_IsArray(layerPagesNode))
                {
                    cJSON* layerPageNode = nullptr;
                    cJSON_ArrayForEach(layerPageNode, layerPagesNode)
                    {
                        const cJSON* instancesNode = cJSON_GetObjectItemCaseSensitive(layerPageNode, "vfxInstances");
                        if (cJSON_IsArray(instancesNode) && cJSON_GetArraySize(instancesNode) > 0)
                        {
                            hasAnyInstance = true;
                            break;
                        }
                    }
                }
            }
            cJSON_Delete(sourceRoot);
            if (!isShipVfxConfig || !hasAnyInstance)
            {
                continue;
            }

            const std::filesystem::path destinationPath = folderPath / sourceFileName;
            const std::string destinationPathNorm = normalizePathSlashes(destinationPath.string());
            const bool sameSourceAndDestination =
                makePathKeyLower(sourcePathNorm) == makePathKeyLower(destinationPathNorm);

            bool copyOk = sameSourceAndDestination;
            if (!sameSourceAndDestination)
            {
                std::error_code copyError;
                std::filesystem::copy_file(
                    entry.path(),
                    destinationPath,
                    std::filesystem::copy_options::overwrite_existing,
                    copyError);
                copyOk = !copyError;
            }

            if (!copyOk)
            {
                failedCount += 1;
                continue;
            }

            exportedPaths.push_back(destinationPathNorm);
            exportedFileNameKeys.push_back(sourceFileNameKey);
        }
    }

    if (exportedPaths.empty())
    {
        if (updateStatusMessage)
        {
            this->statusMessage = "Echec export JSON VFX (aucun fichier ecrit).";
        }
        return false;
    }

    if (outExportedFileCount != nullptr)
    {
        *outExportedFileCount = static_cast<int>(exportedPaths.size());
    }
    if (outFailedFileCount != nullptr)
    {
        *outFailedFileCount = failedCount;
    }

    if (failedCount == 0 && shipIndex == this->selectedShipIndex)
    {
        this->shipVfxDirty = false;
    }
    if (shipIndex == this->selectedShipIndex)
    {
        this->loadedShipVfxConfigPath = exportedPaths.front();
    }
    if (updateStatusMessage)
    {
        if (failedCount == 0)
        {
            this->statusMessage =
                "Export OK: " + std::to_string(static_cast<int>(exportedPaths.size())) +
                " fichier(s) JSON.";
        }
        else
        {
            this->statusMessage =
                "Export partiel: " + std::to_string(static_cast<int>(exportedPaths.size())) +
                " OK, " + std::to_string(failedCount) + " en echec.";
        }
    }
    return true;
}

bool EditorMapVfxScene::exportAllShipsVfxJsonToShipFolders(void)
{
    if (this->importedShips.empty())
    {
        this->statusMessage = "Aucun navire importe pour export JSON.";
        return false;
    }

    this->saveCurrentShipVfxPairDraft();

    int shipsWithExport = 0;
    int shipsWithExportErrors = 0;
    int totalExportedFiles = 0;
    int totalFailedFiles = 0;

    for (int shipIndex = 0; shipIndex < static_cast<int>(this->importedShips.size()); ++shipIndex)
    {
        const ImportedShip& ship = this->importedShips[static_cast<size_t>(shipIndex)];
        int exportedForShip = 0;
        int failedForShip = 0;
        const bool shipExportOk = this->exportShipVfxJsonToFolderForShipIndex(
            shipIndex,
            ship.folderAbsolutePath.c_str(),
            false,
            &exportedForShip,
            &failedForShip);

        if (exportedForShip > 0)
        {
            shipsWithExport += 1;
            totalExportedFiles += exportedForShip;
        }
        if (failedForShip > 0)
        {
            shipsWithExportErrors += 1;
            totalFailedFiles += failedForShip;
        }

        // shipExportOk=false peut simplement signifier "aucun json a produire" pour ce navire.
        (void)shipExportOk;
    }

    if (totalExportedFiles <= 0)
    {
        if (totalFailedFiles > 0)
        {
            this->statusMessage =
                "Export global VFX: 0 fichier JSON, " + std::to_string(totalFailedFiles) + " en echec.";
        }
        else
        {
            this->statusMessage = "Export global VFX: aucun JSON a exporter.";
        }
        return false;
    }

    if (totalFailedFiles == 0)
    {
        this->statusMessage =
            "Export global OK: " + std::to_string(totalExportedFiles) + " fichier(s) JSON sur " +
            std::to_string(shipsWithExport) + " navire(s).";
        return true;
    }

    this->statusMessage =
        "Export global partiel: " + std::to_string(totalExportedFiles) + " JSON OK sur " +
        std::to_string(shipsWithExport) + " navire(s), " +
        std::to_string(totalFailedFiles) + " echec(s) sur " +
        std::to_string(shipsWithExportErrors) + " navire(s).";
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
        RC2D_ImageData src = LoadStorageImageData(sprite.storagePath.c_str(), RC2D_STORAGE_USER);
        if (src.sdl_surface == nullptr)
        {
            destroySurfaceVector(sourceSurfaces);
            cleanupExportedFrames();
            this->statusMessage = "Sprite source manquant pour export.";
            return false;
        }
        sourceSurfaces.push_back(src.sdl_surface);
        src.sdl_surface = nullptr;
        ReleaseStorageImageData(&src);
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

    cJSON_AddNumberToObject(
        jsonRoot,
        "fps",
        this->computeLooseExportSpritesheetFps(static_cast<int>(exportedFrames.size())));
    const int exportAnimMs = this->getLoosePreviewTotalDurationMsActive();
    if (exportAnimMs > 0)
    {
        cJSON_AddNumberToObject(jsonRoot, "animationTotalDurationMs", static_cast<double>(exportAnimMs));
    }
    else
    {
        cJSON_AddNullToObject(jsonRoot, "animationTotalDurationMs");
    }
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

void EditorMapVfxScene::drawShipVfxDebugIsoGrid(void) const
{
    if (!this->previewIsoGridVisible || !this->previewShipLoaded)
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const int cx = static_cast<int>(std::lround(static_cast<double>(this->previewShipTile.x)));
    const int cy = static_cast<int>(std::lround(static_cast<double>(this->previewShipTile.y)));
    static_assert(kShipVfxDebugIsoGridTiles % 2 == 0, "grille paire pour centrage entier");
    const int half = kShipVfxDebugIsoGridTiles / 2;
    const int baseX = cx - half;
    const int baseY = cy - half;

    const float hw = map.getTileWidth() * 0.5f;
    const float hh = map.getTileHeight() * 0.5f;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{120, 200, 255, 185});
    for (int iy = 0; iy < kShipVfxDebugIsoGridTiles; ++iy)
    {
        for (int ix = 0; ix < kShipVfxDebugIsoGridTiles; ++ix)
        {
            const int tx = baseX + ix;
            const int ty = baseY + iy;
            if (!map.isInside(tx, ty))
            {
                continue;
            }
            const SDL_FPoint c = map.tileToScreenCenter(tx, ty);
            const float xTop = c.x;
            const float yTop = c.y - hh;
            const float xRight = c.x + hw;
            const float yRight = c.y;
            const float xBot = c.x;
            const float yBot = c.y + hh;
            const float xLeft = c.x - hw;
            const float yLeft = c.y;
            rc2d_graphics_line(xTop, yTop, xRight, yRight);
            rc2d_graphics_line(xRight, yRight, xBot, yBot);
            rc2d_graphics_line(xBot, yBot, xLeft, yLeft);
            rc2d_graphics_line(xLeft, yLeft, xTop, yTop);
        }
    }
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
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
    const auto& previewLayers = this->currentShipVfxLayers();

    std::vector<RenderItem> items;
    items.reserve(this->currentShipVfxLayers().size() + 1U);
    if (this->activeShipLayerVisible())
    {
        items.push_back(RenderItem{true, this->activeShipDrawOrder(), 0U, -1});
    }
    for (size_t i = 0; i < this->currentShipVfxLayers().size(); ++i)
    {
        const ShipVfxInstance& instance = this->currentShipVfxLayers()[i];
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
            if (this->previewShipLoaded && !this->shouldPreviewHideShipForRelativeTiming(timeSeconds))
            {
                this->previewShip.draw(map);
            }
            continue;
        }

        const ShipVfxInstance& instance = previewLayers[static_cast<size_t>(item.instanceIndex)];
        const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
        float resolvedOffsetX = 0.0f;
        float resolvedOffsetY = 0.0f;
        this->getVfxPreviewDrawOffsets(instance, override, &resolvedOffsetX, &resolvedOffsetY);
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
        if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(imported, timeSeconds))
        {
            continue;
        }

        const int frameIndex = this->computeVfxPreviewFrameIndex(instance, timeSeconds, previewLayers, imported);
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

    if (this->activeShipLayerVisible() && this->activeShipDebugBoundsVisible() && this->previewShipLoaded)
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

    if (this->selectedVfxInstanceIndex >= 0 && this->selectedVfxInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
    {
        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
        if (instance.debugBoundsVisible)
        {
            const DirectionOverride* override = this->getResolvedDirectionOverride(&instance);
            if (instance.importedSfxIndex >= 0 && instance.importedSfxIndex < static_cast<int>(this->importedSfx.size()))
            {
                const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(instance.importedSfxIndex)];
                if (!imported.frames.empty() && !this->shouldSkipDrawImportedSfxForPilotMaxLifetime(imported, timeSeconds))
                {
                    const int frameIndex =
                        this->computeVfxPreviewFrameIndex(instance, timeSeconds, previewLayers, imported);
                    const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
                    const float sourceW = frame.w;
                    const float sourceH = frame.h;
                    float dbgOffX = 0.0f;
                    float dbgOffY = 0.0f;
                    this->getVfxPreviewDrawOffsets(instance, override, &dbgOffX, &dbgOffY);
                    const float offsetX = dbgOffX * scale;
                    const float offsetY = dbgOffY * scale;
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

void EditorMapVfxScene::drawShipVfxTrailPieces(void) const
{
    if (!this->previewShipLoaded || this->currentShipVfxTrailPieces().empty())
    {
        return;
    }
    const Map& map = GetCurrentMap();
    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const auto& previewLayers = this->currentShipVfxLayers();

    auto findInst = [&previewLayers](uint32_t id) -> const ShipVfxInstance* {
        for (const ShipVfxInstance& inst : previewLayers)
        {
            if (inst.instanceId == id)
            {
                return &inst;
            }
        }
        return nullptr;
    };

    std::vector<const ShipVfxTrailPiece*> sorted;
    sorted.reserve(this->currentShipVfxTrailPieces().size());
    for (const ShipVfxTrailPiece& p : this->currentShipVfxTrailPieces())
    {
        sorted.push_back(&p);
    }
    std::sort(sorted.begin(), sorted.end(), [this, &findInst](const ShipVfxTrailPiece* a, const ShipVfxTrailPiece* b) {
        const ShipVfxInstance* ia = findInst(a->sourceVfxInstanceId);
        const ShipVfxInstance* ib = findInst(b->sourceVfxInstanceId);
        int oa = 0;
        int ob = 0;
        if (ia != nullptr)
        {
            const DirectionOverride* ova = this->getResolvedDirectionOverride(ia);
            oa = (ova != nullptr) ? ova->drawOrder : ia->drawOrder;
        }
        if (ib != nullptr)
        {
            const DirectionOverride* ovb = this->getResolvedDirectionOverride(ib);
            ob = (ovb != nullptr) ? ovb->drawOrder : ib->drawOrder;
        }
        if (oa != ob)
        {
            return oa < ob;
        }
        return a->bornTimeSeconds < b->bornTimeSeconds;
    });

    for (const ShipVfxTrailPiece* piecePtr : sorted)
    {
        const ShipVfxTrailPiece& piece = *piecePtr;
        const ShipVfxInstance* inst = findInst(piece.sourceVfxInstanceId);
        if (inst == nullptr)
        {
            continue;
        }
        const DirectionOverride* override = this->getResolvedDirectionOverride(inst);
        const bool visible = (override != nullptr) ? override->visible : inst->visible;
        if (!visible)
        {
            continue;
        }
        if (inst->importedSfxIndex < 0 || inst->importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
        {
            continue;
        }
        const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(inst->importedSfxIndex)];
        if (imported.image.sdl_texture == nullptr || imported.frames.empty())
        {
            continue;
        }
        if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(imported, timeSeconds))
        {
            continue;
        }

        SDL_FPoint shipCenter =
            map.tileToScreenCenterFloat(piece.anchorShipTileX, piece.anchorShipTileY);
        if (piece.anchorShipSpriteCenterOffValid)
        {
            shipCenter.x += piece.anchorShipSpriteCenterOffXPx;
            shipCenter.y += piece.anchorShipSpriteCenterOffYPx;
        }
        else
        {
            float shipCenterOffsetX = 0.0f;
            float shipCenterOffsetY = 0.0f;
            if (this->previewShip.getCurrentSpriteCenterOffsetPixels(&shipCenterOffsetX, &shipCenterOffsetY))
            {
                shipCenter.x += shipCenterOffsetX;
                shipCenter.y += shipCenterOffsetY;
            }
        }

        const float phaseTime = (std::max)(0.0f, timeSeconds - piece.bornTimeSeconds);
        const float trailPlaybackSec = piece.trailInitialPhaseSec + phaseTime;
        const int frameIndex = this->computeTrailPieceFrameIndex(imported, trailPlaybackSec);
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];

        const float lifeScale = EditorMapVfxScene::trailPieceLifetimeDrawScaleMul(piece);
        float driftX = 0.0f;
        float driftY = 0.0f;
        EditorMapVfxScene::trailPieceAmbientDriftOffsets(piece, phaseTime, lifeScale, &driftX, &driftY);
        float wakeX = 0.0f;
        float wakeY = 0.0f;
        EditorMapVfxScene::trailPieceWakeInertiaOffsets(piece, phaseTime, lifeScale, &wakeX, &wakeY);
        const float ox =
            (piece.trailDrawOffsetX + piece.trailPerpendicularJitterX + driftX + wakeX) * scale;
        const float oy =
            (piece.trailDrawOffsetY + piece.trailPerpendicularJitterY + driftY + wakeY) * scale;
        const float resolvedRotation = (override != nullptr) ? override->rotationDeg : inst->rotationDeg;
        const float trailDrawRotationDeg =
            resolvedRotation + piece.trailRotationJitterDeg +
            EditorMapVfxScene::trailPieceSpinExtraDeg(piece, phaseTime);
        const bool resolvedFlipH = (override != nullptr) ? override->flipHorizontal : inst->flipHorizontal;
        const bool resolvedFlipV = (override != nullptr) ? override->flipVertical : inst->flipVertical;

        const float sourceW = frame.w;
        const float sourceH = frame.h;
        const float drawScaleX = scale * lifeScale;
        const float drawScaleY = scale * lifeScale;
        const float drawX = shipCenter.x + ox - ((sourceW * drawScaleX) * 0.5f);
        const float drawY = shipCenter.y + oy - ((sourceH * drawScaleY) * 0.5f);
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
            trailDrawRotationDeg,
            drawScaleX,
            drawScaleY,
            pivotX,
            pivotY,
            resolvedFlipH,
            resolvedFlipV);
    }
}

void EditorMapVfxScene::drawShipVfxTargetFireSectorsOverlay(void) const
{
    if (!this->previewShipLoaded || !this->shipVfxEditorTargetSectorsABEnabled ||
        !this->shipVfxEditorTargetSectorsOverlayVisible)
    {
        return;
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

    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    float radius = ((map.getTileWidth() + map.getTileHeight()) * 0.5f) * zoom * 0.85f;
    float shipSpriteW = 0.0f;
    float shipSpriteH = 0.0f;
    if (this->previewShip.getCurrentSpriteSizePixels(&shipSpriteW, &shipSpriteH))
    {
        const float shipScale = zoom * this->previewShip.getDrawScale();
        const float rSprite = 0.52f * (std::max)(shipSpriteW, shipSpriteH) * shipScale;
        radius = (std::max)(radius, rSprite);
    }

    const Ship::PreviewDirection bow = this->previewShip.getCurrentPreviewDirection();
    float fx = 0.0f;
    float fy = 0.0f;
    editorMapVfxBowForwardScreenUnit(bow, &fx, &fy);

    constexpr float kPi = 3.14159265f;
    constexpr float kDegToRad = kPi / 180.0f;
    const int activeSec = std::clamp(this->previewTargetFireSectorIndex, 0, 1);

    int hoverSec = -1;
    float mx = 0.0f;
    float my = 0.0f;
    if (this->getMouseRenderPosition(&mx, &my) && this->pointInRect(mx, my, map.rect))
    {
        const SDL_Point hoverTile = map.screenToTileNearest(mx, my);
        const SDL_FPoint hoverCenter = map.tileToScreenCenterFloat(hoverTile.x, hoverTile.y);
        const float dx = hoverCenter.x - shipCenter.x;
        const float dy = hoverCenter.y - shipCenter.y;
        const float len2 = (dx * dx) + (dy * dy);
        if (std::isfinite(len2) && len2 >= 9.0f)
        {
            hoverSec = editorMapVfxTargetFireSectorFromScreenDelta(bow, dx, dy);
        }
    }

    auto drawQuarterWedgeOutline = [&](float deg0, float deg1, RC2D_Color color, int seg) {
        rc2d_graphics_setColor(color);
        float r0 = deg0 * kDegToRad;
        float r1 = deg1 * kDegToRad;
        if (r1 < r0)
        {
            r1 += 2.0f * kPi;
        }
        const float x0 = shipCenter.x + radius * std::cos(r0);
        const float y0 = shipCenter.y + radius * std::sin(r0);
        const float x1 = shipCenter.x + radius * std::cos(r1);
        const float y1 = shipCenter.y + radius * std::sin(r1);
        rc2d_graphics_line(shipCenter.x, shipCenter.y, x0, y0);
        rc2d_graphics_line(shipCenter.x, shipCenter.y, x1, y1);
        float prevX = x0;
        float prevY = y0;
        for (int i = 1; i <= seg; ++i)
        {
            const float u = static_cast<float>(i) / static_cast<float>(seg);
            const float t = r0 + ((r1 - r0) * u);
            const float x = shipCenter.x + radius * std::cos(t);
            const float y = shipCenter.y + radius * std::sin(t);
            rc2d_graphics_line(prevX, prevY, x, y);
            prevX = x;
            prevY = y;
        }
    };

    auto wedgeLabelPos = [&](float deg0, float deg1, float dist) -> SDL_FPoint {
        float r0 = deg0 * kDegToRad;
        float r1 = deg1 * kDegToRad;
        if (r1 < r0)
        {
            r1 += 2.0f * kPi;
        }
        const float tm = r0 + ((r1 - r0) * 0.5f);
        return SDL_FPoint{shipCenter.x + dist * std::cos(tm), shipCenter.y + dist * std::sin(tm)};
    };

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Fleche proue (avant du navire, repere ecran).
    const float arrowLen = radius * 0.38f;
    rc2d_graphics_setColor(RC2D_Color{255, 255, 255, 210});
    rc2d_graphics_line(shipCenter.x, shipCenter.y, shipCenter.x + arrowLen * fx, shipCenter.y + arrowLen * fy);

    const int arcSeg = 24;
    float degA0 = 0.0f;
    float degA1 = 0.0f;
    float degB0 = 0.0f;
    float degB1 = 0.0f;
    editorMapVfxTargetFireSectorQuarterBoundsScreenDeg(bow, 0, &degA0, &degA1);
    editorMapVfxTargetFireSectorQuarterBoundsScreenDeg(bow, 1, &degB0, &degB1);

    // Secteur A : quart de disque (90 deg) en coordonnees ecran.
    {
        const bool hi = (hoverSec == 0);
        const bool act = (activeSec == 0);
        const Uint8 baseA = static_cast<Uint8>(hi ? 215 : (act ? 185 : 115));
        drawQuarterWedgeOutline(degA0, degA1, RC2D_Color{70, 200, 255, baseA}, arcSeg);
    }
    // Secteur B : quart oppose (90 deg).
    {
        const bool hi = (hoverSec == 1);
        const bool act = (activeSec == 1);
        const Uint8 baseA = static_cast<Uint8>(hi ? 215 : (act ? 185 : 115));
        drawQuarterWedgeOutline(degB0, degB1, RC2D_Color{255, 170, 90, baseA}, arcSeg);
    }

    if (this->overlayFont.sdl_font != nullptr)
    {
        const float labelDist = radius * 0.78f;
        const SDL_FPoint pa = wedgeLabelPos(degA0, degA1, labelDist);
        const SDL_FPoint pb = wedgeLabelPos(degB0, degB1, labelDist);

        auto drawLabel = [this](const char* ch, float x, float y, RC2D_Color col) {
            RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), ch);
            t.color = col;
            rc2d_graphics_setTextColor(&t);
            int tw = 0;
            int th = 0;
            rc2d_graphics_getTextSize(&t, &tw, &th);
            rc2d_graphics_drawText(&t, x - (static_cast<float>(tw) * 0.5f), y - (static_cast<float>(th) * 0.5f));
            rc2d_graphics_destroyText(&t);
        };

        drawLabel(
            "A",
            pa.x,
            pa.y,
            RC2D_Color{210, 250, 255, static_cast<Uint8>((activeSec == 0) ? 255 : 220)});
        drawLabel(
            "B",
            pb.x,
            pb.y,
            RC2D_Color{255, 230, 200, static_cast<Uint8>((activeSec == 1) ? 255 : 220)});
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapVfxScene::drawShipVfxRotationDialOverlay(void) const
{
    if (!this->vfxRotationDialActive || !this->previewShipLoaded || !this->hasSelectedVfxInstance())
    {
        return;
    }
    const ShipVfxInstance* instance = this->getSelectedVfxInstance();
    if (instance == nullptr || instance->locked)
    {
        return;
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

    const float zoom = (std::max)(GetCamera().getZoomFactor(), 0.01f);

    const DirectionOverride* resolved = this->getResolvedDirectionOverride(instance);
    float drawOffX = 0.0f;
    float drawOffY = 0.0f;
    this->getVfxPreviewDrawOffsets(*instance, resolved, &drawOffX, &drawOffY);
    const float pivotX = shipCenter.x + (drawOffX * zoom);
    const float pivotY = shipCenter.y + (drawOffY * zoom);

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{255, 230, 150, 235});
    rc2d_graphics_line(pivotX, pivotY, mouseX, mouseY);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapVfxScene::drawShipVfxTilePlacementGhost(void) const
{
    if (this->vfxRotationDialActive || this->vfxDragActive || !this->previewShipLoaded ||
        this->selectedVfxInstanceIndex < 0 ||
        this->selectedVfxInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }

    const ShipVfxInstance& ghostInst = this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)];
    if (!ghostInst.placementSnapClickToTile || ghostInst.locked)
    {
        return;
    }

    const Map& map = GetCurrentMap();
    float mx = 0.0f;
    float my = 0.0f;
    if (!this->getMouseRenderPosition(&mx, &my) || !this->pointInRect(mx, my, map.rect))
    {
        return;
    }
    SDL_Point placedTile{};
    if (!this->tryGetScreenTileNearestForSelectedVfxCenter(&placedTile))
    {
        return;
    }

    const SDL_Point hoverTile = map.screenToTileNearest(mx, my);
    if (hoverTile.x == placedTile.x && hoverTile.y == placedTile.y)
    {
        return;
    }

    if (ghostInst.importedSfxIndex < 0 || ghostInst.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return;
    }

    const ImportedSfx& gImported = this->importedSfx[static_cast<size_t>(ghostInst.importedSfxIndex)];
    if (gImported.image.sdl_texture == nullptr || gImported.frames.empty())
    {
        return;
    }

    const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(gImported, timeSeconds))
    {
        return;
    }
    const DirectionOverride* gOverride = this->getResolvedDirectionOverride(&ghostInst);
    const float gRot = (gOverride != nullptr) ? gOverride->rotationDeg : ghostInst.rotationDeg;
    const bool gFh = (gOverride != nullptr) ? gOverride->flipHorizontal : ghostInst.flipHorizontal;
    const bool gFv = (gOverride != nullptr) ? gOverride->flipVertical : ghostInst.flipVertical;
    const SDL_FPoint gTileCenter = map.tileToScreenCenterFloat(
        static_cast<float>(hoverTile.x),
        static_cast<float>(hoverTile.y));
    const int gFrameIndex = this->computeVfxPreviewFrameIndex(
        ghostInst,
        timeSeconds,
        this->currentShipVfxLayers(),
        gImported);
    const ImportedSfxFrame& gFrame = gImported.frames[static_cast<size_t>(gFrameIndex)];
    const float gSourceW = gFrame.w;
    const float gSourceH = gFrame.h;
    const float gDrawX = gTileCenter.x - ((gSourceW * scale) * 0.5f);
    const float gDrawY = gTileCenter.y - ((gSourceH * scale) * 0.5f);
    const RC2D_Quad gQuad = rc2d_graphics_newQuad(
        const_cast<RC2D_Image*>(&gImported.image),
        gFrame.x,
        gFrame.y,
        gFrame.w,
        gFrame.h);
    Uint8 prevAlpha = 255;
    SDL_GetTextureAlphaMod(gImported.image.sdl_texture, &prevAlpha);
    SDL_SetTextureAlphaMod(gImported.image.sdl_texture, 150);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_drawQuad(
        const_cast<RC2D_Image*>(&gImported.image),
        &gQuad,
        gDrawX,
        gDrawY,
        gRot,
        scale,
        scale,
        gSourceW * 0.5f,
        gSourceH * 0.5f,
        gFh,
        gFv);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(gImported.image.sdl_texture, prevAlpha);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{120, 200, 255, 200});
    constexpr float crossHalf = 6.0f;
    SDL_FRect hLine{gTileCenter.x - crossHalf, gTileCenter.y - 1.0f, crossHalf * 2.0f, 2.0f};
    SDL_FRect vLine{gTileCenter.x - 1.0f, gTileCenter.y - crossHalf, 2.0f, crossHalf * 2.0f};
    rc2d_graphics_rectangle("fill", &hLine);
    rc2d_graphics_rectangle("fill", &vLine);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
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
    const float nowSecondsPl = static_cast<float>(SDL_GetTicks()) * 0.001f;
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

    auto drawAnimatedAtTile = [&](float tileX, float tileY, float elapsedSeconds, float fps, Uint8 textureAlpha255) {
        if (frameCount <= 0)
        {
            return;
        }

        const float clampedFps = std::clamp(fps, kSfxFpsMin, kSfxFpsMax);
        const int frameIndex = this->computeLooseAnimatedFrameIndex(elapsedSeconds, frameCount, clampedFps);
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

        Uint8 prevTexAlpha = 255;
        SDL_Texture* const tex = spriteImage->sdl_texture;
        const bool canAlphaMod = tex != nullptr;
        if (canAlphaMod)
        {
            SDL_GetTextureAlphaMod(tex, &prevTexAlpha);
            SDL_SetTextureAlphaMod(tex, textureAlpha255);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
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
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        if (canAlphaMod)
        {
            SDL_SetTextureAlphaMod(tex, prevTexAlpha);
        }
    };

    for (const LoosePreviewPlacement& placement : this->loosePreviewPlacements)
    {
        drawAnimatedAtTile(
            placement.tileX,
            placement.tileY,
            nowSecondsPl - placement.spawnTimeSeconds,
            placement.fps,
            255);
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (this->getMouseRenderPosition(&mouseX, &mouseY) && this->pointInRect(mouseX, mouseY, map.rect))
    {
        auto loosePlacementPreviewTileOccupied = [this](float tix, float tiy) -> bool {
            constexpr float kSameTileEps = 1.0e-4f;
            for (const LoosePreviewPlacement& placement : this->loosePreviewPlacements)
            {
                if (this->loosePreviewPlacementSnapToTile)
                {
                    if (std::lround(placement.tileX) == std::lround(tix) && std::lround(placement.tileY) == std::lround(tiy))
                    {
                        return true;
                    }
                }
                else
                {
                    const float dx = placement.tileX - tix;
                    const float dy = placement.tileY - tiy;
                    if ((dx * dx) + (dy * dy) <= kSameTileEps * kSameTileEps)
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        if (this->loosePreviewPlacementSnapToTile)
        {
            const SDL_Point hoveredTile = map.screenToTileNearest(mouseX, mouseY);
            const float htx = static_cast<float>(hoveredTile.x);
            const float hty = static_cast<float>(hoveredTile.y);
            if (!loosePlacementPreviewTileOccupied(htx, hty))
            {
                drawAnimatedAtTile(
                    htx,
                    hty,
                    0.0f,
                    this->getLoosePreviewFpsOrDefault(),
                    kLoosePlacementCursorPreviewAlpha);
            }
        }
        else
        {
            const SDL_FPoint hoveredTileF = map.screenToTile(mouseX, mouseY);
            if (!loosePlacementPreviewTileOccupied(hoveredTileF.x, hoveredTileF.y))
            {
                drawAnimatedAtTile(
                    hoveredTileF.x,
                    hoveredTileF.y,
                    0.0f,
                    this->getLoosePreviewFpsOrDefault(),
                    kLoosePlacementCursorPreviewAlpha);
            }
        }
    }
}

std::vector<int> EditorMapVfxScene::expandLayerPanelDisplayRows(const std::vector<int>& coreOrdered) const
{
    std::vector<int> out;
    out.reserve(coreOrdered.size() + this->currentShipVfxTrailPieces().size());
    for (const int code : coreOrdered)
    {
        out.push_back(code);
        if (code < 0)
        {
            continue;
        }
        const ShipVfxInstance& inst = this->currentShipVfxLayers()[static_cast<size_t>(code)];
        for (const ShipVfxTrailPiece& p : this->currentShipVfxTrailPieces())
        {
            if (p.sourceVfxInstanceId == inst.instanceId && p.layerPanelUiId != 0U)
            {
                out.push_back(layerPanelTrailPieceRowCodeFromUiId(p.layerPanelUiId));
            }
        }
    }
    return out;
}

int EditorMapVfxScene::findVfxLayerIndexByInstanceId(uint32_t instanceId) const
{
    const auto& layers = this->currentShipVfxLayers();
    for (int i = 0; i < static_cast<int>(layers.size()); ++i)
    {
        if (layers[static_cast<size_t>(i)].instanceId == instanceId)
        {
            return i;
        }
    }
    return -1;
}

int EditorMapVfxScene::findTrailPieceIndexByLayerPanelUiId(uint32_t uiId) const
{
    if (uiId == 0U)
    {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(this->currentShipVfxTrailPieces().size()); ++i)
    {
        if (this->currentShipVfxTrailPieces()[static_cast<size_t>(i)].layerPanelUiId == uiId)
        {
            return i;
        }
    }
    return -1;
}

void EditorMapVfxScene::removeTrailPiecesWithSourceInstanceId(uint32_t sourceInstanceId)
{
    for (size_t i = 0; i < this->currentShipVfxTrailPieces().size();)
    {
        if (this->currentShipVfxTrailPieces()[i].sourceVfxInstanceId == sourceInstanceId)
        {
            this->currentShipVfxTrailPieces().erase(this->currentShipVfxTrailPieces().begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
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
    entries.reserve(this->currentShipVfxLayers().size() + 1U);
    entries.push_back(LayerEntry{-1, this->activeShipDrawOrder(), 0U});
    for (size_t i = 0; i < this->currentShipVfxLayers().size(); ++i)
    {
        const ShipVfxInstance& instance = this->currentShipVfxLayers()[i];
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
        const int v = orderedInstanceIndices[i];
        if (this->shipLayerSelected && v == -1)
        {
            return static_cast<int>(i);
        }
        if (!this->shipLayerSelected && !layerPanelRowIsTrailPieceSubRow(v) && v == this->selectedVfxInstanceIndex)
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
        this->drawToolbarButton(this->buttonDirectionPrevRect, "NAV DIRECTION -", false);
        this->drawToolbarButton(this->buttonDirectionNextRect, "NAV DIRECTION +", false);
        this->drawToolbarButton(this->buttonShipStateToggleRect, this->getPreviewShipStateLabel(), this->previewShipStateIndex == 1);
        this->drawToolbarButton(
            this->buttonTargetFireSectorToggleRect,
            this->shipVfxEditorTargetSectorsABEnabled ? this->getPreviewTargetFireSectorLabel()
                                                     : "CIBLE (8 pg)",
            this->shipVfxEditorTargetSectorsABEnabled && (this->previewTargetFireSectorIndex == 1));
        this->drawToolbarButton(this->buttonShipOpacityMinusRect, "OPA -10%", false);
        this->drawToolbarButton(this->buttonShipOpacityPlusRect, "OPA +10%", false);
        this->drawToolbarButton(
            this->buttonPreviewIsoGridRect,
            "GRILLE ISO 30x30",
            this->previewIsoGridVisible);
        this->drawToolbarButton(this->buttonShipVfxZoomMinusRect, "ZOOM -", false);
        this->drawToolbarButton(this->buttonShipVfxZoomPlusRect, "ZOOM +", false);

        std::vector<std::string> shipLabels;
        shipLabels.reserve(this->importedShips.size());
        for (const ImportedShip& ship : this->importedShips)
        {
            shipLabels.push_back(stripListPrefix(ship.displayName, "ship-"));
        }
        this->drawListPanel(this->shipListRect, "Navires", shipLabels, this->selectedShipIndex, this->shipListScrollOffset);
        this->drawToolbarButton(this->buttonShipVfxDuplicateShipsRect, "DUPLIQUER VFX SHIPS", false);

        std::vector<std::string> sfxLabels;
        sfxLabels.reserve(this->importedSfx.size());
        for (const ImportedSfx& sfx : this->importedSfx)
        {
            sfxLabels.push_back(stripListPrefix(sfx.displayName, "vfx-"));
        }
        this->drawListPanel(this->sfxListRect, "VFX", sfxLabels, this->selectedSfxIndex, this->sfxListScrollOffset);
        if (this->sfxListActionButtonsVisible &&
            this->sfxListActionButtonsSfxIndex >= 0 &&
            this->sfxListActionButtonsSfxIndex < static_cast<int>(this->importedSfx.size()))
        {
            this->drawToolbarButton(this->sfxListActionAutoImportRect, "AUTO IMPORT", false);
            this->drawToolbarButton(this->sfxListActionAddInstanceRect, "AJOUTER INSTANCE", false);
        }

        const std::vector<int> orderedLayerIndices = this->expandLayerPanelDisplayRows(
            this->getOrderedVfxInstanceIndicesForLayerPanel());
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
        }

        {
            const RC2D_Color durFillColor = this->loosePreviewTotalDurationMsInputFocused
                ? RC2D_Color{58, 88, 122, 210}
                : RC2D_Color{28, 38, 50, 205};
            const RC2D_Color durBorderColor = this->loosePreviewTotalDurationMsInputFocused
                ? RC2D_Color{124, 186, 236, 245}
                : RC2D_Color{108, 126, 148, 220};
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            rc2d_graphics_setColor(durFillColor);
            rc2d_graphics_rectangle("fill", &this->buttonLoosePreviewTotalDurationMsInputRect);
            rc2d_graphics_setColor(durBorderColor);
            rc2d_graphics_rectangle("line", &this->buttonLoosePreviewTotalDurationMsInputRect);
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

            const int durActive = this->getLoosePreviewTotalDurationMsActive();
            std::string durValue = this->loosePreviewTotalDurationMsInputFocused
                ? this->loosePreviewTotalDurationMsInput
                : (durActive > 0 ? std::to_string(durActive) : std::string("-"));
            std::string durLabel = "Duree totale (ms): " + durValue;
            if (this->loosePreviewTotalDurationMsInputFocused)
            {
                durLabel += "_";
            }
            RC2D_Text durInputText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), durLabel.c_str());
            durInputText.color = kHudTextColor;
            rc2d_graphics_setTextColor(&durInputText);
            rc2d_graphics_drawText(
                &durInputText,
                this->buttonLoosePreviewTotalDurationMsInputRect.x + 6.0f,
                this->buttonLoosePreviewTotalDurationMsInputRect.y + 2.0f);
            rc2d_graphics_destroyText(&durInputText);
        }

        if (this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
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

    char infoBuffer[768] = {};
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
            "Mode Ship / VFX | Ship: %s | Direction: %s | State: %s | Zoom: %.2f | Instances: %d | ShipOrder: %d | Dirty: %s | Ocean: %s",
            shipName,
            this->getPreviewDirectionLabel(),
            this->getPreviewShipStateLabel(),
            this->shipVfxPreviewZoomFactor,
            static_cast<int>(this->currentShipVfxLayers().size()),
            this->activeShipDrawOrder(),
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
        char durInfoBuf[32] = {};
        const int durAct = this->getLoosePreviewTotalDurationMsActive();
        if (durAct > 0)
        {
            SDL_snprintf(durInfoBuf, sizeof(durInfoBuf), "%d ms", durAct);
        }
        else
        {
            SDL_snprintf(durInfoBuf, sizeof(durInfoBuf), "FPS");
        }
        SDL_snprintf(
            infoBuffer,
            sizeof(infoBuffer),
            "Mode Downscale Sprites VFX | Sous-mode: %s | Dossier: %s | Scale export: %d%% | Zoom: %.2f | Placements: %d | Frame/S: %s | Anim: %s | Ocean: %s",
            looseSubMode,
            folderName,
            this->looseScalePercent,
            this->loosePreviewZoomFactor,
            static_cast<int>(this->loosePreviewPlacements.size()),
            this->loosePreviewFpsInput.c_str(),
            durInfoBuf,
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
    this->drawVfxRelativeTimingPopup();
    this->drawVfxTrailPopup();
    this->drawVfxTrailConePopup();
    this->drawVfxDuplicateToPagesPopup();
    this->drawShipVfxShipDuplicatePopup();
    this->drawExportConfirmPopup();
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

void EditorMapVfxScene::drawExportConfirmPopup(void) const
{
    if (!this->exportConfirmPopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    ExportConfirmPopupLayout lay{};
    if (!this->computeExportConfirmPopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->exportConfirmPopupLastLayout = lay;

    const char* title = "EXPORTER - CONFIRMATION";
    const char* details = "";
    switch (this->exportConfirmPopupAction)
    {
    case ExportConfirmAction::SHIP_VFX_ALL_SHIPS:
        details = "Etes-vous sur ? Cette action exporte les JSON VFX pour tous les navires.";
        break;
    case ExportConfirmAction::LOOSE_OPEN_EXPORT_FLOW:
        details = "Etes-vous sur ? Vous allez lancer le flux d'export sprites VFX.";
        break;
    default:
        details = "Etes-vous sur de lancer l'export ?";
        break;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 155});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{22, 30, 40, 238});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{145, 168, 194, 245});
    rc2d_graphics_rectangle("line", &lay.popup);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    RC2D_Text titleText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title);
    titleText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&titleText);
    rc2d_graphics_drawText(&titleText, lay.popup.x + 18.0f, lay.popup.y + 18.0f);
    rc2d_graphics_destroyText(&titleText);

    RC2D_Text detailsText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), details);
    detailsText.color = RC2D_Color{208, 220, 236, 235};
    rc2d_graphics_setTextColor(&detailsText);
    rc2d_graphics_drawText(&detailsText, lay.popup.x + 18.0f, lay.popup.y + 68.0f);
    rc2d_graphics_destroyText(&detailsText);

    auto drawBtn = [this](const SDL_FRect& r, const char* label, bool danger) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(danger ? RC2D_Color{96, 58, 46, 236} : RC2D_Color{56, 72, 92, 232});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(danger ? RC2D_Color{228, 182, 156, 245} : RC2D_Color{150, 170, 190, 235});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        RC2D_Text txt = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), label);
        txt.color = kHudTextColor;
        rc2d_graphics_setTextColor(&txt);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&txt, &tw, &th);
        rc2d_graphics_drawText(
            &txt,
            r.x + ((r.w - static_cast<float>(tw)) * 0.5f),
            r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&txt);
    };
    drawBtn(lay.validateBtn, "OUI, EXPORTER", true);
    drawBtn(lay.cancelBtn, "ANNULER", false);
}

void EditorMapVfxScene::drawVfxRelativeTimingPopup(void) const
{
    if (!this->vfxRelativeTimingPopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    VfxRelativePopupLayout lay{};
    if (!this->computeVfxRelativePopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->vfxRelativePopupLastLayout = lay;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 150});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{22, 30, 40, 235});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{145, 168, 194, 245});
    rc2d_graphics_rectangle("line", &lay.popup);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    const char* title = (this->vfxRelativeTimingPopupStep == 0)
        ? "RELATIF Ã¢â‚¬â€ choisir l'instance de reference"
        : "RELATIF Ã¢â‚¬â€ delai apres l'instance choisie (ms)";
    RC2D_Text titleText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), title);
    titleText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&titleText);
    rc2d_graphics_drawText(&titleText, lay.popup.x + 14.0f, lay.popup.y + 12.0f);
    rc2d_graphics_destroyText(&titleText);

    if (this->vfxRelativeTimingPopupStep == 0)
    {
        const auto& layers = this->currentShipVfxLayers();
        for (int i = 0; i < lay.candidateCount; ++i)
        {
            const int idx = this->vfxRelativeTimingPopupCandidateIndices[static_cast<size_t>(i)];
            if (idx < 0 || idx >= static_cast<int>(layers.size()))
            {
                continue;
            }
            const ShipVfxInstance& cand = layers[static_cast<size_t>(idx)];
            char line[192] = {};
            SDL_snprintf(
                line,
                sizeof(line),
                "id %u  |  %s",
                static_cast<unsigned int>(cand.instanceId),
                cand.label.c_str());
            rc2d_graphics_setColor(RC2D_Color{48, 58, 72, 220});
            rc2d_graphics_rectangle("fill", &lay.candidateRows[i]);
            rc2d_graphics_setColor(RC2D_Color{130, 148, 168, 230});
            rc2d_graphics_rectangle("line", &lay.candidateRows[i]);
            RC2D_Text rowT = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), line);
            rowT.color = kHudTextColor;
            rc2d_graphics_setTextColor(&rowT);
            rc2d_graphics_drawText(&rowT, lay.candidateRows[i].x + 6.0f, lay.candidateRows[i].y + 4.0f);
            rc2d_graphics_destroyText(&rowT);
        }
    }
    else
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{40, 54, 70, 245});
        rc2d_graphics_rectangle("fill", &lay.delayInputRect);
        rc2d_graphics_setColor(
            this->vfxRelativeTimingPopupDelayMsFocused ? RC2D_Color{160, 200, 240, 245} : RC2D_Color{124, 186, 236, 245});
        rc2d_graphics_rectangle("line", &lay.delayInputRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        std::string prompt = "SPAWN APRES INSTANCE id ";
        prompt += std::to_string(static_cast<unsigned long long>(this->vfxRelativeTimingPopupAnchorInstanceId));
        prompt += "  :  delai (ms)";
        RC2D_Text pText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), prompt.c_str());
        pText.color = RC2D_Color{210, 218, 228, 240};
        rc2d_graphics_setTextColor(&pText);
        rc2d_graphics_drawText(&pText, lay.popup.x + 14.0f, lay.popup.y + 56.0f);
        rc2d_graphics_destroyText(&pText);

        const std::string inputShow = this->vfxRelativeTimingPopupDelayMsInput + (this->vfxRelativeTimingPopupDelayMsFocused ? "_" : "");
        RC2D_Text inText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), inputShow.c_str());
        inText.color = kHudTextColor;
        rc2d_graphics_setTextColor(&inText);
        rc2d_graphics_drawText(&inText, lay.delayInputRect.x + 8.0f, lay.delayInputRect.y + 7.0f);
        rc2d_graphics_destroyText(&inText);
    }

    auto drawBtn = [this](const SDL_FRect& r, const char* lab) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{56, 72, 92, 230});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(RC2D_Color{150, 170, 190, 235});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lab);
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&t, &tw, &th);
        rc2d_graphics_drawText(&t, r.x + ((r.w - static_cast<float>(tw)) * 0.5f), r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&t);
    };

    drawBtn(lay.cancelBtn, "ANNULER");
    drawBtn(lay.clearBtn, "EFFACER");
    if (this->vfxRelativeTimingPopupStep == 1)
    {
        drawBtn(lay.validateBtn, "VALIDER");
    }
}

bool EditorMapVfxScene::handleVfxRelativeTimingPopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->vfxRelativeTimingPopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    VfxRelativePopupLayout lay{};
    if (!this->computeVfxRelativePopupLayout(&lay))
    {
        return true;
    }
    this->vfxRelativePopupLastLayout = lay;

    if (!this->pointInRect(x, y, lay.popup))
    {
        if (this->pointInRect(x, y, lay.dimFullMap))
        {
            this->closeVfxRelativeTimingPopup();
            this->statusMessage = "RELATIF : annule.";
        }
        return true;
    }

    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeVfxRelativeTimingPopup();
        this->statusMessage = "RELATIF : annule.";
        return true;
    }

    if (this->pointInRect(x, y, lay.clearBtn))
    {
        const size_t page = static_cast<size_t>(this->getShipVfxLayerPageKey());
        if (this->vfxRelativeTimingPopupIsShipRow)
        {
            this->shipSpawnAfterVfxInstanceId[page] = 0U;
            this->shipSpawnAfterDelayMs[page] = 0;
        }
        else if (this->vfxRelativeTimingPopupTargetVfxIndex >= 0 &&
                 this->vfxRelativeTimingPopupTargetVfxIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& t = this->currentShipVfxLayers()[static_cast<size_t>(this->vfxRelativeTimingPopupTargetVfxIndex)];
            t.spawnAfterInstanceId = 0U;
            t.spawnAfterDelayMs = 0;
        }
        this->markShipVfxDirty();
        this->closeVfxRelativeTimingPopup();
        this->statusMessage = "RELATIF : lien supprime.";
        return true;
    }

    if (this->vfxRelativeTimingPopupStep == 1)
    {
        if (this->pointInRect(x, y, lay.delayInputRect))
        {
            this->vfxRelativeTimingPopupDelayMsFocused = true;
            return true;
        }
        this->vfxRelativeTimingPopupDelayMsFocused = false;

        if (this->pointInRect(x, y, lay.validateBtn))
        {
            char* endPtr = nullptr;
            const long parsed =
                std::strtol(this->vfxRelativeTimingPopupDelayMsInput.c_str(), &endPtr, 10);
            int ms = 0;
            if (endPtr != this->vfxRelativeTimingPopupDelayMsInput.c_str())
            {
                ms = static_cast<int>(std::clamp(parsed, 0L, 999999L));
            }
            if (this->vfxRelativeTimingPopupIsShipRow)
            {
                const size_t pageKey = static_cast<size_t>(this->getShipVfxLayerPageKey());
                this->shipSpawnAfterVfxInstanceId[pageKey] = this->vfxRelativeTimingPopupAnchorInstanceId;
                this->shipSpawnAfterDelayMs[pageKey] = ms;
            }
            else if (this->vfxRelativeTimingPopupTargetVfxIndex >= 0 &&
                     this->vfxRelativeTimingPopupTargetVfxIndex < static_cast<int>(this->currentShipVfxLayers().size()))
            {
                ShipVfxInstance& tgt =
                    this->currentShipVfxLayers()[static_cast<size_t>(this->vfxRelativeTimingPopupTargetVfxIndex)];
                tgt.spawnAfterInstanceId = this->vfxRelativeTimingPopupAnchorInstanceId;
                tgt.spawnAfterDelayMs = ms;
            }
            this->markShipVfxDirty();
            this->closeVfxRelativeTimingPopup();
            this->statusMessage = "RELATIF : delai " + std::to_string(ms) + " ms enregistre.";
            return true;
        }
        return true;
    }

    if (this->vfxRelativeTimingPopupStep == 0)
    {
        const auto& layers = this->currentShipVfxLayers();
        for (int i = 0; i < lay.candidateCount; ++i)
        {
            if (this->pointInRect(x, y, lay.candidateRows[i]))
            {
                const int idx = this->vfxRelativeTimingPopupCandidateIndices[static_cast<size_t>(i)];
                if (idx >= 0 && idx < static_cast<int>(layers.size()))
                {
                    this->vfxRelativeTimingPopupAnchorInstanceId = layers[static_cast<size_t>(idx)].instanceId;
                    this->vfxRelativeTimingPopupStep = 1;
                    this->vfxRelativeTimingPopupDelayMsInput.clear();
                    this->vfxRelativeTimingPopupDelayMsFocused = true;
                    if (this->vfxRelativeTimingPopupIsShipRow)
                    {
                        const size_t pk = static_cast<size_t>(this->getShipVfxLayerPageKey());
                        if (this->shipSpawnAfterVfxInstanceId[pk] == this->vfxRelativeTimingPopupAnchorInstanceId)
                        {
                            this->vfxRelativeTimingPopupDelayMsInput = std::to_string(this->shipSpawnAfterDelayMs[pk]);
                        }
                    }
                    else if (this->vfxRelativeTimingPopupTargetVfxIndex >= 0 &&
                             this->vfxRelativeTimingPopupTargetVfxIndex < static_cast<int>(layers.size()))
                    {
                        const ShipVfxInstance& t = layers[static_cast<size_t>(this->vfxRelativeTimingPopupTargetVfxIndex)];
                        if (t.spawnAfterInstanceId == this->vfxRelativeTimingPopupAnchorInstanceId)
                        {
                            this->vfxRelativeTimingPopupDelayMsInput = std::to_string(t.spawnAfterDelayMs);
                        }
                    }
                    this->statusMessage = "RELATIF : saisis le delai (ms), puis VALIDER.";
                }
                return true;
            }
        }
        this->vfxRelativeTimingPopupDelayMsFocused = false;
    }

    return true;
}

bool EditorMapVfxScene::handleVfxRelativeTimingPopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)keycode;
    (void)mod;
    if (!this->vfxRelativeTimingPopupVisible)
    {
        return false;
    }

    if (scancode == SDL_SCANCODE_ESCAPE && !isrepeat)
    {
        this->closeVfxRelativeTimingPopup();
        this->statusMessage = "RELATIF : annule.";
        return true;
    }

    if (this->vfxRelativeTimingPopupStep == 1)
    {
        if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
        {
            char* endPtr = nullptr;
            const long parsed =
                std::strtol(this->vfxRelativeTimingPopupDelayMsInput.c_str(), &endPtr, 10);
            int ms = 0;
            if (endPtr != this->vfxRelativeTimingPopupDelayMsInput.c_str())
            {
                ms = static_cast<int>(std::clamp(parsed, 0L, 999999L));
            }
            if (this->vfxRelativeTimingPopupIsShipRow)
            {
                const size_t pageKey = static_cast<size_t>(this->getShipVfxLayerPageKey());
                this->shipSpawnAfterVfxInstanceId[pageKey] = this->vfxRelativeTimingPopupAnchorInstanceId;
                this->shipSpawnAfterDelayMs[pageKey] = ms;
            }
            else if (this->vfxRelativeTimingPopupTargetVfxIndex >= 0 &&
                     this->vfxRelativeTimingPopupTargetVfxIndex < static_cast<int>(this->currentShipVfxLayers().size()))
            {
                ShipVfxInstance& tgt =
                    this->currentShipVfxLayers()[static_cast<size_t>(this->vfxRelativeTimingPopupTargetVfxIndex)];
                tgt.spawnAfterInstanceId = this->vfxRelativeTimingPopupAnchorInstanceId;
                tgt.spawnAfterDelayMs = ms;
            }
            this->markShipVfxDirty();
            this->closeVfxRelativeTimingPopup();
            this->statusMessage = "RELATIF : delai " + std::to_string(ms) + " ms enregistre.";
            return true;
        }
        if (scancode == SDL_SCANCODE_BACKSPACE && !isrepeat)
        {
            if (this->vfxRelativeTimingPopupDelayMsFocused && !this->vfxRelativeTimingPopupDelayMsInput.empty())
            {
                this->vfxRelativeTimingPopupDelayMsInput.pop_back();
            }
            return true;
        }
        if (this->vfxRelativeTimingPopupDelayMsFocused && !isrepeat)
        {
            auto appendMsDigit = [this](char digit) -> bool {
                if (this->vfxRelativeTimingPopupDelayMsInput.size() >= 8U)
                {
                    return true;
                }
                this->vfxRelativeTimingPopupDelayMsInput.push_back(digit);
                return true;
            };
            switch (scancode)
            {
            case SDL_SCANCODE_KP_0:
                return appendMsDigit('0');
            case SDL_SCANCODE_KP_1:
                return appendMsDigit('1');
            case SDL_SCANCODE_KP_2:
                return appendMsDigit('2');
            case SDL_SCANCODE_KP_3:
                return appendMsDigit('3');
            case SDL_SCANCODE_KP_4:
                return appendMsDigit('4');
            case SDL_SCANCODE_KP_5:
                return appendMsDigit('5');
            case SDL_SCANCODE_KP_6:
                return appendMsDigit('6');
            case SDL_SCANCODE_KP_7:
                return appendMsDigit('7');
            case SDL_SCANCODE_KP_8:
                return appendMsDigit('8');
            case SDL_SCANCODE_KP_9:
                return appendMsDigit('9');
            default:
                break;
            }
        }
        if (this->vfxRelativeTimingPopupDelayMsFocused && key != nullptr && std::strlen(key) == 1U)
        {
            const char c = key[0];
            if (c >= '0' && c <= '9' && this->vfxRelativeTimingPopupDelayMsInput.size() < 8U)
            {
                this->vfxRelativeTimingPopupDelayMsInput.push_back(c);
                return true;
            }
        }
        return true;
    }

    return true;
}

void EditorMapVfxScene::drawVfxTrailPopupPreviews(const VfxTrailPopupLayout& lay) const
{
    if (!this->previewShipLoaded)
    {
        return;
    }
    if (this->vfxTrailPopupParentInstanceIndex < 0 ||
        this->vfxTrailPopupParentInstanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
    {
        return;
    }
    const ShipVfxInstance& inst =
        this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
    if (inst.importedSfxIndex < 0 || inst.importedSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        return;
    }
    const ImportedSfx& imported = this->importedSfx[static_cast<size_t>(inst.importedSfxIndex)];
    if (imported.image.sdl_texture == nullptr || imported.frames.empty())
    {
        return;
    }

    const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const auto& previewLayers = this->currentShipVfxLayers();
    const DirectionOverride* ovr = this->getResolvedDirectionOverride(&inst);
    /** Meme plage que Camera::min/maxZoomFactor : apercu identique au zoom carte pour navire + VFX. */
    constexpr float kTrailPopupPreviewWorldZoomMin = 0.4f;
    constexpr float kTrailPopupPreviewWorldZoomMax = 1.0f;
    const float worldZ = (std::clamp)(this->vfxTrailPopupPreviewZoom,
                                      kTrailPopupPreviewWorldZoomMin,
                                      kTrailPopupPreviewWorldZoomMax);
    const float shipEffectiveZoom = worldZ * this->previewShip.getDrawScale();
    const int marcheEveryN = editorMapVfxParseIntClamped(
        this->vfxTrailPopupEveryNTilesInput.c_str(),
        kMotionTrailEveryNTilesMin,
        kMotionTrailEveryNTilesMax,
        inst.motionTrailEveryNTiles);
    const bool showMarcheTrailPieces = marcheEveryN > 0;
    const bool showMarcheShipOnPath = marcheEveryN > 0 && this->vfxTrailPopupPreviewMarcheShipVisible;

    auto drawOneQuadAt = [&](float centerX,
                             float centerY,
                             float offsetXUnscaled,
                             float offsetYUnscaled,
                             float extraRotDeg) {
        float scx = 0.0f;
        float scy = 0.0f;
        if (this->previewShip.getCurrentSpriteCenterOffsetPixelsForEffectiveZoom(shipEffectiveZoom, &scx, &scy))
        {
            centerX += scx;
            centerY += scy;
        }
        const float ox = offsetXUnscaled * worldZ;
        const float oy = offsetYUnscaled * worldZ;
        const float resolvedRot = (ovr != nullptr) ? ovr->rotationDeg : inst.rotationDeg;
        const float trailRot = resolvedRot + extraRotDeg;
        const bool fh = (ovr != nullptr) ? ovr->flipHorizontal : inst.flipHorizontal;
        const bool fv = (ovr != nullptr) ? ovr->flipVertical : inst.flipVertical;

        const int frameIndex =
            this->computeVfxPreviewFrameIndex(inst, timeSeconds, previewLayers, imported);
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
        const float sourceW = frame.w;
        const float sourceH = frame.h;
        const float drawX = centerX + ox - ((sourceW * worldZ) * 0.5f);
        const float drawY = centerY + oy - ((sourceH * worldZ) * 0.5f);
        const float pivotX = sourceW * 0.5f;
        const float pivotY = sourceH * 0.5f;
        const RC2D_Quad sourceQuad =
            rc2d_graphics_newQuad(const_cast<RC2D_Image*>(&imported.image), frame.x, frame.y, frame.w, frame.h);
        rc2d_graphics_drawQuad(const_cast<RC2D_Image*>(&imported.image),
                               &sourceQuad,
                               drawX,
                               drawY,
                               trailRot,
                               worldZ,
                               worldZ,
                               pivotX,
                               pivotY,
                               fh,
                               fv);
    };

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());

    const SDL_FRect& rm = lay.previewMarcheRect;
    if (rm.w > 4.0f && rm.h > 4.0f)
    {
        if (renderer != nullptr)
        {
            SDL_Rect clip{};
            clip.x = static_cast<int>(std::floor(rm.x));
            clip.y = static_cast<int>(std::floor(rm.y));
            clip.w = (std::max)(static_cast<int>(std::ceil(rm.x + rm.w)) - clip.x, 1);
            clip.h = (std::max)(static_cast<int>(std::ceil(rm.y + rm.h)) - clip.y, 1);
            SDL_SetRenderClipRect(renderer, &clip);
        }

        const int pageKeyMarche = this->getShipVfxLayerPageKey();
        const int dirMarche = pageKeyMarche % 4;
        const Map& map = GetCurrentMap();
        const EditorMapVfxMarchePopupGrid grid = editorMapVfxBuildMarchePopupGrid(rm, dirMarche, map, worldZ);
        if (grid.pathLengthTiles > 0.0001f)
        {
            float previewShipCenterOffX = 0.0f;
            float previewShipCenterOffY = 0.0f;
            const bool previewShipCenterOffValid = this->previewShip.getCurrentSpriteCenterOffsetPixelsForEffectiveZoom(
                shipEffectiveZoom,
                &previewShipCenterOffX,
                &previewShipCenterOffY);

            auto marcheTileToScreen = [&](float tileX, float tileY) -> SDL_FPoint {
                const float relX = tileX - grid.startTileX;
                const float relY = tileY - grid.startTileY;
                return SDL_FPoint{
                    grid.startScreenX + (relX - relY) * grid.halfTileW,
                    grid.startScreenY + (relX + relY) * grid.halfTileH};
            };

            auto marcheVfxCenterAtTile = [&](const ShipVfxTrailPiece& piece) -> SDL_FPoint {
                float ptx = piece.anchorShipTileX;
                float pty = piece.anchorShipTileY;
                if (piece.marchePopupPreviewPathU >= 0.0f)
                {
                    const float pu = (std::clamp)(piece.marchePopupPreviewPathU, 0.0f, 1.0f);
                    const float pd = pu * grid.pathLengthTiles;
                    ptx = grid.startTileX + grid.stepTileX * pd;
                    pty = grid.startTileY + grid.stepTileY * pd;
                }
                SDL_FPoint out = marcheTileToScreen(ptx, pty);
                if (previewShipCenterOffValid)
                {
                    out.x += previewShipCenterOffX;
                    out.y += previewShipCenterOffY;
                }
                out.x += (piece.trailDrawOffsetX + piece.trailPerpendicularJitterX) * worldZ;
                out.y += (piece.trailDrawOffsetY + piece.trailPerpendicularJitterY) * worldZ;
                return out;
            };

            auto drawMarcheSimQuadAt = [&](float centerX, float centerY, const ShipVfxTrailPiece& piece) {
                const float resolvedRot = (ovr != nullptr) ? ovr->rotationDeg : inst.rotationDeg;
                const bool fh = (ovr != nullptr) ? ovr->flipHorizontal : inst.flipHorizontal;
                const bool fv = (ovr != nullptr) ? ovr->flipVertical : inst.flipVertical;
                const float phaseT = (std::max)(0.0f, timeSeconds - piece.bornTimeSeconds);
                const float trailRot = resolvedRot + piece.trailRotationJitterDeg +
                                       EditorMapVfxScene::trailPieceSpinExtraDeg(piece, phaseT);
                const float trailPlaybackSec = piece.trailInitialPhaseSec + phaseT;
                const int frameIndex = this->computeTrailPieceFrameIndex(imported, trailPlaybackSec);
                const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
                const float sourceW = frame.w;
                const float sourceH = frame.h;
                const float lifeScaleMarche = EditorMapVfxScene::trailPieceLifetimeDrawScaleMul(piece);
                float driftMx = 0.0f;
                float driftMy = 0.0f;
                EditorMapVfxScene::trailPieceAmbientDriftOffsets(piece, phaseT, lifeScaleMarche, &driftMx, &driftMy);
                float wakeMx = 0.0f;
                float wakeMy = 0.0f;
                EditorMapVfxScene::trailPieceWakeInertiaOffsets(piece, phaseT, lifeScaleMarche, &wakeMx, &wakeMy);
                const float pieceZ = worldZ * lifeScaleMarche;
                const float drawX =
                    centerX + (driftMx + wakeMx) * worldZ - ((sourceW * pieceZ) * 0.5f);
                const float drawY =
                    centerY + (driftMy + wakeMy) * worldZ - ((sourceH * pieceZ) * 0.5f);
                const float pivotX = sourceW * 0.5f;
                const float pivotY = sourceH * 0.5f;
                const RC2D_Quad sourceQuad =
                    rc2d_graphics_newQuad(const_cast<RC2D_Image*>(&imported.image), frame.x, frame.y, frame.w, frame.h);
                rc2d_graphics_drawQuad(const_cast<RC2D_Image*>(&imported.image),
                                       &sourceQuad,
                                       drawX,
                                       drawY,
                                       trailRot,
                                       pieceZ,
                                       pieceZ,
                                       pivotX,
                                       pivotY,
                                       fh,
                                       fv);
            };

            float headU = std::fmod(this->vfxTrailPopupMarcheDistAlongPathPx, 1.0f);
            if (headU < 0.0f)
            {
                headU += 1.0f;
            }
            const float headDistTiles = headU * grid.pathLengthTiles;
            const float headTileX = grid.startTileX + grid.stepTileX * headDistTiles;
            const float headTileY = grid.startTileY + grid.stepTileY * headDistTiles;
            const SDL_FPoint headAnchor = marcheTileToScreen(headTileX, headTileY);

            std::vector<const ShipVfxTrailPiece*> marcheSorted;
            marcheSorted.reserve(this->vfxTrailPopupMarcheSimPieces.size());
            for (const ShipVfxTrailPiece& mp : this->vfxTrailPopupMarcheSimPieces)
            {
                if (mp.sourceVfxInstanceId == inst.instanceId && mp.marchePopupPreviewPathU >= 0.0f)
                {
                    marcheSorted.push_back(&mp);
                }
            }
            std::sort(marcheSorted.begin(),
                      marcheSorted.end(),
                      [](const ShipVfxTrailPiece* a, const ShipVfxTrailPiece* b) {
                          return a->bornTimeSeconds < b->bornTimeSeconds;
                      });

            const int shipOrdMarche = this->activeShipDrawOrder();
            const int vfxOrdMarche = (ovr != nullptr) ? ovr->drawOrder : inst.drawOrder;
            const bool marcheVfxDrawnBeforeShip = vfxOrdMarche < shipOrdMarche;

            auto drawMarcheTrailLayer = [&]() {
                if (!showMarcheTrailPieces)
                {
                    return;
                }
                for (const ShipVfxTrailPiece* mpPtr : marcheSorted)
                {
                    const ShipVfxTrailPiece& mp = *mpPtr;
                    const SDL_FPoint pc = marcheVfxCenterAtTile(mp);
                    drawMarcheSimQuadAt(pc.x, pc.y, mp);
                }
            };

            if (marcheVfxDrawnBeforeShip)
            {
                drawMarcheTrailLayer();
            }
            if (showMarcheShipOnPath)
            {
                float sw = 0.0f;
                float sh = 0.0f;
                if (this->previewShip.getCurrentSpriteSizePixels(&sw, &sh))
                {
                    const float targetShipW = sw * shipEffectiveZoom;
                    this->previewShip.drawEditorPreviewAt(headAnchor.x, headAnchor.y, targetShipW);
                }
            }
            if (!marcheVfxDrawnBeforeShip)
            {
                drawMarcheTrailLayer();
            }
        }

        if (renderer != nullptr)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
        }
    }

    const SDL_FRect& ra = lay.previewArretRect;
    if (ra.w > 4.0f && ra.h > 4.0f && this->vfxTrailPopupIdleRingWhenStationary)
    {
        if (renderer != nullptr)
        {
            SDL_Rect clip{};
            clip.x = static_cast<int>(std::floor(ra.x));
            clip.y = static_cast<int>(std::floor(ra.y));
            clip.w = (std::max)(static_cast<int>(std::ceil(ra.x + ra.w)) - clip.x, 1);
            clip.h = (std::max)(static_cast<int>(std::ceil(ra.y + ra.h)) - clip.y, 1);
            SDL_SetRenderClipRect(renderer, &clip);
        }

        const int pc = editorMapVfxParseIntClamped(
            this->vfxTrailPopupIdleRingPieceCountInput.c_str(), 1, 32, inst.motionTrailIdleRingPieceCount);
        const float radiusU =
            editorMapVfxParseFloat(this->vfxTrailPopupIdleRadiusInput.c_str(), inst.motionTrailIdleRingRadius);
        const int idleRotPct = editorMapVfxParseIntClamped(this->vfxTrailPopupIdleRingRotationPctInput.c_str(),
                                                           0,
                                                           100,
                                                           inst.motionTrailIdleRingRotationRandomPercent);
        const float posJitU = editorMapVfxParseFloat(this->vfxTrailPopupIdleRingPosJitterInput.c_str(),
                                                     inst.motionTrailIdleRingPositionJitterRadius);
        const float baseOx = (ovr != nullptr) ? ovr->offsetX : inst.offsetX;
        const float baseOy = (ovr != nullptr) ? ovr->offsetY : inst.offsetY;
        constexpr float kTwoPi = 6.28318530718f;
        const float crx = ra.x + ra.w * 0.5f;
        const float cry = ra.y + ra.h * 0.52f;
        const float idleRotF = static_cast<float>(idleRotPct) / 100.0f;

        const int shipOrd = this->activeShipDrawOrder();
        const int vfxOrd = (ovr != nullptr) ? ovr->drawOrder : inst.drawOrder;
        const bool vfxDrawnBeforeShip = vfxOrd < shipOrd;

        float crownSw = 0.0f;
        float crownSh = 0.0f;
        const bool haveCrownShipSize = this->previewShip.getCurrentSpriteSizePixels(&crownSw, &crownSh);
        const float wantShipWCrown = haveCrownShipSize ? (crownSw * shipEffectiveZoom) : 64.0f;

        auto drawCrownPieces = [&]() {
            for (int k = 0; k < pc; ++k)
            {
                const float ang = kTwoPi * (static_cast<float>(k) / static_cast<float>(pc));
                float ox = baseOx + std::cos(ang) * radiusU;
                float oy = baseOy + std::sin(ang) * radiusU;
                ox += posJitU * std::cos(ang * 2.7f + timeSeconds * 1.9f) * 0.65f;
                oy += posJitU * std::sin(ang * 2.3f + timeSeconds * 1.7f) * 0.65f;
                const float er =
                    idleRotF * kMotionTrailRotationJitterMaxDeg * std::sin(ang * 4.0f + timeSeconds * 2.0f);
                drawOneQuadAt(crx, cry, ox, oy, er);
            }
        };

        if (vfxDrawnBeforeShip)
        {
            drawCrownPieces();
        }
        if (this->vfxTrailPopupPreviewCrownShipVisible && this->previewShipLoaded)
        {
            this->previewShip.drawEditorPreviewAt(crx, cry, wantShipWCrown);
        }
        if (!vfxDrawnBeforeShip)
        {
            drawCrownPieces();
        }

        if (renderer != nullptr)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
        }
    }
}

void EditorMapVfxScene::drawVfxTrailPopup(void) const
{
    if (!this->vfxTrailPopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }
    VfxTrailPopupLayout lay{};
    if (!this->computeVfxTrailPopupLayout(&lay))
    {
        return;
    }
    const_cast<EditorMapVfxScene*>(this)->vfxTrailPopupLastLayout = lay;

    constexpr float kTrailLblH = 17.0f;
    constexpr float kTrailLblToInputGap = 8.0f;
    const float px = lay.everyNTilesInputRect.x;
    const float ruleLineX0 = lay.everyNTilesInputRect.x;
    const float ruleLineX1 = lay.everyNTilesInputRect.x + lay.everyNTilesInputRect.w;
    const auto trailLabelYAboveInput = [kTrailLblH, kTrailLblToInputGap](const SDL_FRect& inputRect) {
        return inputRect.y - kTrailLblToInputGap - kTrailLblH;
    };

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 150});
    rc2d_graphics_rectangle("fill", &lay.dimFullMap);
    rc2d_graphics_setColor(RC2D_Color{22, 30, 40, 235});
    rc2d_graphics_rectangle("fill", &lay.popup);
    rc2d_graphics_setColor(RC2D_Color{145, 168, 194, 245});
    rc2d_graphics_rectangle("line", &lay.popup);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    RC2D_Text titleText = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Trainee Ã¢â‚¬â€ marche (trace) et arret (couronne)");
    titleText.color = kHudTextColor;
    rc2d_graphics_setTextColor(&titleText);
    rc2d_graphics_drawText(&titleText, lay.popup.x + 14.0f, lay.popup.y + 12.0f);
    rc2d_graphics_destroyText(&titleText);

    const ShipVfxInstance* pInst = nullptr;
    if (this->vfxTrailPopupParentInstanceIndex >= 0 &&
        this->vfxTrailPopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
    {
        pInst = &this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
    }
    if (pInst != nullptr)
    {
        char sub[220] = {};
        SDL_snprintf(
            sub,
            sizeof(sub),
            "Instance id %u  |  %s",
            static_cast<unsigned int>(pInst->instanceId),
            pInst->label.c_str());
        RC2D_Text subText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), sub);
        subText.color = RC2D_Color{200, 208, 218, 235};
        rc2d_graphics_setTextColor(&subText);
        rc2d_graphics_drawText(&subText, lay.popup.x + 14.0f, lay.popup.y + 36.0f);
        rc2d_graphics_destroyText(&subText);
    }

    auto drawTrailHRule = [ruleLineX0, ruleLineX1](float yy) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{88, 108, 132, 215});
        rc2d_graphics_line(ruleLineX0, yy, ruleLineX1, yy);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    };
    drawTrailHRule(lay.trailRuleAfterInstanceY);

    auto drawInput = [this](const SDL_FRect& r, bool focused, const std::string& value) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{40, 54, 70, 245});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(focused ? RC2D_Color{160, 200, 240, 245} : RC2D_Color{124, 186, 236, 245});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        const std::string show = value + (focused ? "_" : "");
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), show.c_str());
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        rc2d_graphics_drawText(&t, r.x + 8.0f, r.y + 7.0f);
        rc2d_graphics_destroyText(&t);
    };

    RC2D_Text secMarche = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont), "Marche (trace et rejets)");
    secMarche.color = RC2D_Color{170, 188, 208, 235};
    rc2d_graphics_setTextColor(&secMarche);
    rc2d_graphics_drawText(&secMarche, px, lay.trailRuleAfterInstanceY - 20.0f + 4.0f);
    rc2d_graphics_destroyText(&secMarche);

    RC2D_Text nLbl = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "En marche : tuiles entre rejets (0 = off) :");
    nLbl.color = RC2D_Color{210, 218, 228, 240};
    rc2d_graphics_setTextColor(&nLbl);
    rc2d_graphics_drawText(&nLbl, px, trailLabelYAboveInput(lay.everyNTilesInputRect));
    rc2d_graphics_destroyText(&nLbl);
    drawInput(lay.everyNTilesInputRect, this->vfxTrailPopupEveryNTilesFocused, this->vfxTrailPopupEveryNTilesInput);

    RC2D_Text msLbl = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Duree de vie d'un rejet (en tuiles) :");
    msLbl.color = RC2D_Color{210, 218, 228, 240};
    rc2d_graphics_setTextColor(&msLbl);
    rc2d_graphics_drawText(&msLbl, px, trailLabelYAboveInput(lay.lifetimeTilesInputRect));
    rc2d_graphics_destroyText(&msLbl);
    drawInput(lay.lifetimeTilesInputRect, this->vfxTrailPopupLifetimeTilesFocused, this->vfxTrailPopupLifetimeTilesInput);

    RC2D_Text placeLbl = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Trace en marche :");
    placeLbl.color = RC2D_Color{210, 218, 228, 240};
    rc2d_graphics_setTextColor(&placeLbl);
    rc2d_graphics_drawText(&placeLbl, px, trailLabelYAboveInput(lay.trailStrictTileBtn));
    rc2d_graphics_destroyText(&placeLbl);

    auto drawModeBtn = [this](const SDL_FRect& r, const char* lab, bool selected) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(selected ? RC2D_Color{72, 96, 124, 245} : RC2D_Color{56, 72, 92, 230});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(selected ? RC2D_Color{190, 214, 240, 250} : RC2D_Color{150, 170, 190, 235});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lab);
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&t, &tw, &th);
        rc2d_graphics_drawText(&t, r.x + ((r.w - static_cast<float>(tw)) * 0.5f), r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&t);
    };
    drawModeBtn(lay.trailStrictTileBtn, "SPAWN (EMPLACEMENT DU LAYER)", this->vfxTrailPopupStrictTilePlacement);
    drawModeBtn(
        lay.trailLateralSpreadBtn,
        "SPAWN (OFFSET CONE)",
        !this->vfxTrailPopupStrictTilePlacement);

    if (lay.lateralSectionVisible)
    {
        float coneLen = editorMapVfxParseFloat(
            this->vfxTrailPopupLateralJitterInput.c_str(),
            (pInst != nullptr) ? pInst->motionTrailLateralJitterRadius : 0.0f);
        float coneOffX = (pInst != nullptr) ? pInst->motionTrailConeOffsetX : 0.0f;
        float coneOffY = (pInst != nullptr) ? pInst->motionTrailConeOffsetY : 0.0f;
        float coneDir = (pInst != nullptr) ? pInst->motionTrailConeDirectionOffsetDeg : 0.0f;
        float coneHalf = (pInst != nullptr) ? pInst->motionTrailConeHalfAngleDeg : 28.0f;
        int coneCount = (pInst != nullptr) ? pInst->motionTrailConeSpawnCount : 1;
        if (this->vfxTrailConePopupVisible &&
            this->vfxTrailConePopupParentInstanceIndex == this->vfxTrailPopupParentInstanceIndex)
        {
            coneLen = this->vfxTrailConePopupLength;
            coneOffX = this->vfxTrailConePopupOffsetX;
            coneOffY = this->vfxTrailConePopupOffsetY;
            coneDir = this->vfxTrailConePopupDirectionOffsetDeg;
            coneHalf = this->vfxTrailConePopupHalfAngleDeg;
            coneCount = this->vfxTrailConePopupSpawnCount;
        }
        coneLen = (std::clamp)(coneLen, 0.0f, kTrailConePopupLengthMax);
        coneOffX = (std::clamp)(coneOffX, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        coneOffY = (std::clamp)(coneOffY, kTrailConePopupOffsetMin, kTrailConePopupOffsetMax);
        coneDir = (std::clamp)(coneDir, -179.0f, 179.0f);
        coneHalf = (std::clamp)(coneHalf, kTrailConePopupHalfAngleMin, kTrailConePopupHalfAngleMax);
        coneCount = std::clamp(coneCount, 1, 32);

        RC2D_Text latH1 = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Cone arriere : clique ici pour configurer offset + direction + ouverture :");
        latH1.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&latH1);
        rc2d_graphics_drawText(&latH1, px, trailLabelYAboveInput(lay.lateralJitterInputRect));
        rc2d_graphics_destroyText(&latH1);

        char coneSummary[180] = {};
        SDL_snprintf(coneSummary,
                     sizeof(coneSummary),
                     "Configurer cone (L %.0f | OffX %.0f | OffY %.0f | Dir %.1f deg | Ouv %.1f deg | x%d)",
                     static_cast<double>(coneLen),
                     static_cast<double>(coneOffX),
                     static_cast<double>(coneOffY),
                     static_cast<double>(coneDir),
                     static_cast<double>(coneHalf),
                     coneCount);
        drawInput(lay.lateralJitterInputRect, false, coneSummary);
    }

    RC2D_Text rotLbl = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Rotation en marche (0-100 %, max +-45 deg) :");
    rotLbl.color = RC2D_Color{210, 218, 228, 240};
    rc2d_graphics_setTextColor(&rotLbl);
    rc2d_graphics_drawText(&rotLbl, px, trailLabelYAboveInput(lay.rotationPctInputRect));
    rc2d_graphics_destroyText(&rotLbl);
    drawInput(
        lay.rotationPctInputRect,
        this->vfxTrailPopupRotationPctFocused,
        this->vfxTrailPopupRotationPctInput);

    drawTrailHRule(lay.trailRuleBeforeArretY);

    RC2D_Text secArret = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont), "Arret (couronne)");
    secArret.color = RC2D_Color{170, 188, 208, 235};
    rc2d_graphics_setTextColor(&secArret);
    rc2d_graphics_drawText(&secArret, px, lay.trailRuleBeforeArretY - 20.0f + 4.0f);
    rc2d_graphics_destroyText(&secArret);

    RC2D_Text idleHdr = rc2d_graphics_createText(
        const_cast<RC2D_Font*>(&this->overlayFont),
        "Couronne autour du layer :");
    idleHdr.color = RC2D_Color{210, 218, 228, 240};
    rc2d_graphics_setTextColor(&idleHdr);
    rc2d_graphics_drawText(&idleHdr, px, trailLabelYAboveInput(lay.idleRingOffBtn));
    rc2d_graphics_destroyText(&idleHdr);
    drawModeBtn(lay.idleRingOffBtn, "Couronne OFF", !this->vfxTrailPopupIdleRingWhenStationary);
    drawModeBtn(lay.idleRingOnBtn, "Couronne ON", this->vfxTrailPopupIdleRingWhenStationary);
    if (lay.idleRingInputsVisible)
    {
        RC2D_Text pcLbl = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Couronne : nombre de pieces (1-32) :");
        pcLbl.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&pcLbl);
        rc2d_graphics_drawText(&pcLbl, px, trailLabelYAboveInput(lay.idleRingPieceCountInputRect));
        rc2d_graphics_destroyText(&pcLbl);
        drawInput(
            lay.idleRingPieceCountInputRect,
            this->vfxTrailPopupIdleRingPieceCountFocused,
            this->vfxTrailPopupIdleRingPieceCountInput);
        RC2D_Text radLbl = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Couronne : rayon (comme offset) :");
        radLbl.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&radLbl);
        rc2d_graphics_drawText(&radLbl, px, trailLabelYAboveInput(lay.idleRingRadiusInputRect));
        rc2d_graphics_destroyText(&radLbl);
        drawInput(
            lay.idleRingRadiusInputRect,
            this->vfxTrailPopupIdleRadiusFocused,
            this->vfxTrailPopupIdleRadiusInput);
        RC2D_Text idleRotLbl = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Couronne : rotation (0-100 %) :");
        idleRotLbl.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&idleRotLbl);
        rc2d_graphics_drawText(&idleRotLbl, px, trailLabelYAboveInput(lay.idleRingRotationPctInputRect));
        rc2d_graphics_destroyText(&idleRotLbl);
        drawInput(
            lay.idleRingRotationPctInputRect,
            this->vfxTrailPopupIdleRingRotationPctFocused,
            this->vfxTrailPopupIdleRingRotationPctInput);
        RC2D_Text idleJitLbl = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Couronne : decal aleatoire max (0 = off) :");
        idleJitLbl.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&idleJitLbl);
        rc2d_graphics_drawText(&idleJitLbl, px, trailLabelYAboveInput(lay.idleRingPosJitterInputRect));
        rc2d_graphics_destroyText(&idleJitLbl);
        drawInput(
            lay.idleRingPosJitterInputRect,
            this->vfxTrailPopupIdleRingPosJitterFocused,
            this->vfxTrailPopupIdleRingPosJitterInput);
    }

    {
        const float dividerX = lay.previewMarcheRect.x - 6.0f;
        const float divY0 = lay.popup.y + 46.0f;
        const float divY1 = lay.popup.y + lay.popup.h - 56.0f;
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{88, 108, 132, 200});
        rc2d_graphics_line(dividerX, divY0, dividerX, divY1);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        constexpr float kTrailPopupPreviewWorldZoomMinDraw = 0.4f;
        constexpr float kTrailPopupPreviewWorldZoomMaxDraw = 1.0f;
        const float trailPopupWorldZ = (std::clamp)(this->vfxTrailPopupPreviewZoom,
                                                    kTrailPopupPreviewWorldZoomMinDraw,
                                                    kTrailPopupPreviewWorldZoomMaxDraw);
        const int popupMarcheEveryNForUi = editorMapVfxParseIntClamped(
            this->vfxTrailPopupEveryNTilesInput.c_str(),
            kMotionTrailEveryNTilesMin,
            kMotionTrailEveryNTilesMax,
            (pInst != nullptr) ? pInst->motionTrailEveryNTiles : 0);
        editorMapVfxDrawTrailPopupPreviewOceanBackground(lay.previewMarcheRect, trailPopupWorldZ);
        editorMapVfxDrawTrailPopupPreviewOceanBackground(lay.previewArretRect, trailPopupWorldZ);

        RC2D_Text pm = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            "Preview : rejets en marche (live)");
        pm.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&pm);
        rc2d_graphics_drawText(&pm, lay.previewMarcheRect.x + 8.0f, lay.previewMarcheRect.y + 6.0f);
        rc2d_graphics_destroyText(&pm);

        RC2D_Text pa = rc2d_graphics_createText(
            const_cast<RC2D_Font*>(&this->overlayFont),
            this->vfxTrailPopupIdleRingWhenStationary ? "Preview : couronne (live)"
                                                      : "Preview : couronne OFF (ocean seul)");
        pa.color = RC2D_Color{190, 200, 212, 238};
        rc2d_graphics_setTextColor(&pa);
        rc2d_graphics_drawText(&pa, lay.previewArretRect.x + 8.0f, lay.previewArretRect.y + 30.0f);
        rc2d_graphics_destroyText(&pa);

        this->drawVfxTrailPopupPreviews(lay);

        {
            auto drawTinyBtn = [this](const SDL_FRect& r, const char* lab) {
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
                rc2d_graphics_setColor(RC2D_Color{56, 72, 92, 240});
                rc2d_graphics_rectangle("fill", &r);
                rc2d_graphics_setColor(RC2D_Color{150, 170, 190, 235});
                rc2d_graphics_rectangle("line", &r);
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
                RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lab);
                t.color = kHudTextColor;
                rc2d_graphics_setTextColor(&t);
                int tw = 0;
                int th = 0;
                rc2d_graphics_getTextSize(&t, &tw, &th);
                rc2d_graphics_drawText(
                    &t, r.x + ((r.w - static_cast<float>(tw)) * 0.5f), r.y + ((r.h - static_cast<float>(th)) * 0.5f));
                rc2d_graphics_destroyText(&t);
            };
            drawTinyBtn(lay.previewOceanPrevBtn, "<");
            drawTinyBtn(lay.previewOceanNextBtn, ">");
            drawTinyBtn(lay.previewSpeedMinusBtn, "-");
            drawTinyBtn(lay.previewSpeedPlusBtn, "+");
            drawTinyBtn(lay.previewZoomMinusBtn, "-");
            drawTinyBtn(lay.previewZoomPlusBtn, "+");
            RC2D_Text ocLbl = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), "Ocean");
            ocLbl.color = RC2D_Color{190, 200, 212, 238};
            rc2d_graphics_setTextColor(&ocLbl);
            rc2d_graphics_drawText(
                &ocLbl,
                lay.previewOceanPrevBtn.x - 48.0f,
                lay.previewOceanPrevBtn.y + ((lay.previewOceanPrevBtn.h - 16.0f) * 0.5f));
            rc2d_graphics_destroyText(&ocLbl);
            char sl[32] = {};
            SDL_snprintf(sl,
                         sizeof(sl),
                         "spd %.2f",
                         static_cast<double>(
                             (std::clamp)(this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec, 0.05f, 50.0f)));
            RC2D_Text st = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), sl);
            st.color = RC2D_Color{190, 200, 212, 238};
            rc2d_graphics_setTextColor(&st);
            rc2d_graphics_drawText(
                &st,
                lay.previewOceanNextBtn.x + lay.previewOceanNextBtn.w + 8.0f,
                lay.previewSpeedMinusBtn.y + ((lay.previewSpeedMinusBtn.h - 16.0f) * 0.5f));
            rc2d_graphics_destroyText(&st);
            char zl[28] = {};
            SDL_snprintf(zl,
                         sizeof(zl),
                         "x%.2f",
                         static_cast<double>(
                             (std::clamp)(this->vfxTrailPopupPreviewZoom, 0.4f, 1.0f)));
            RC2D_Text zt = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), zl);
            zt.color = RC2D_Color{190, 200, 212, 238};
            rc2d_graphics_setTextColor(&zt);
            rc2d_graphics_drawText(
                &zt,
                lay.previewZoomMinusBtn.x - 44.0f,
                lay.previewZoomMinusBtn.y + ((lay.previewZoomMinusBtn.h - 16.0f) * 0.5f));
            rc2d_graphics_destroyText(&zt);
        }

        {
            auto drawShipToggleBtn = [this](const SDL_FRect& tb, bool visible) {
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
                rc2d_graphics_setColor(RC2D_Color{56, 72, 92, 230});
                rc2d_graphics_rectangle("fill", &tb);
                rc2d_graphics_setColor(RC2D_Color{150, 170, 190, 235});
                rc2d_graphics_rectangle("line", &tb);
                rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
                const char* togLab = visible ? "Cacher navire" : "Voir navire";
                RC2D_Text tt = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), togLab);
                tt.color = kHudTextColor;
                rc2d_graphics_setTextColor(&tt);
                int ttw = 0;
                int tth = 0;
                rc2d_graphics_getTextSize(&tt, &ttw, &tth);
                rc2d_graphics_drawText(
                    &tt,
                    tb.x + ((tb.w - static_cast<float>(ttw)) * 0.5f),
                    tb.y + ((tb.h - static_cast<float>(tth)) * 0.5f));
                rc2d_graphics_destroyText(&tt);
            };
            if (popupMarcheEveryNForUi > 0)
            {
                drawShipToggleBtn(lay.previewMarcheShipToggleBtn, this->vfxTrailPopupPreviewMarcheShipVisible);
            }
            if (this->vfxTrailPopupIdleRingWhenStationary)
            {
                drawShipToggleBtn(lay.previewCrownShipToggleBtn, this->vfxTrailPopupPreviewCrownShipVisible);
            }
        }
    }

    auto drawBtn = [this](const SDL_FRect& r, const char* lab) {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{56, 72, 92, 230});
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(RC2D_Color{150, 170, 190, 235});
        rc2d_graphics_rectangle("line", &r);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        RC2D_Text t = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), lab);
        t.color = kHudTextColor;
        rc2d_graphics_setTextColor(&t);
        int tw = 0;
        int th = 0;
        rc2d_graphics_getTextSize(&t, &tw, &th);
        rc2d_graphics_drawText(&t, r.x + ((r.w - static_cast<float>(tw)) * 0.5f), r.y + ((r.h - static_cast<float>(th)) * 0.5f));
        rc2d_graphics_destroyText(&t);
    };

    drawBtn(lay.cancelBtn, "ANNULER");
    drawBtn(lay.clearBtn, "EFFACER");
    drawBtn(lay.validateBtn, "VALIDER");
}

bool EditorMapVfxScene::handleVfxTrailPopupMouseClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->vfxTrailPopupVisible)
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    VfxTrailPopupLayout lay{};
    if (!this->computeVfxTrailPopupLayout(&lay))
    {
        return true;
    }
    this->vfxTrailPopupLastLayout = lay;

    const ShipVfxInstance* pInstClick = nullptr;
    if (this->vfxTrailPopupParentInstanceIndex >= 0 &&
        this->vfxTrailPopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
    {
        pInstClick = &this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
    }
    const int popupMarcheEveryNClick = editorMapVfxParseIntClamped(
        this->vfxTrailPopupEveryNTilesInput.c_str(),
        kMotionTrailEveryNTilesMin,
        kMotionTrailEveryNTilesMax,
        (pInstClick != nullptr) ? pInstClick->motionTrailEveryNTiles : 0);

    if (this->pointInRect(x, y, lay.previewOceanPrevBtn))
    {
        this->cycleOceanColor(-1);
        return true;
    }
    if (this->pointInRect(x, y, lay.previewOceanNextBtn))
    {
        this->cycleOceanColor(1);
        return true;
    }
    if (this->pointInRect(x, y, lay.previewSpeedMinusBtn))
    {
        this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec =
            (std::max)(0.05f, this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec - 0.25f);
        char sb[72] = {};
        SDL_snprintf(sb,
                     sizeof(sb),
                     "Apercu rejets en marche : vitesse %.2f tuiles/s",
                     static_cast<double>(this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec));
        this->statusMessage = sb;
        return true;
    }
    if (this->pointInRect(x, y, lay.previewSpeedPlusBtn))
    {
        this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec =
            (std::min)(50.0f, this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec + 0.25f);
        char sb[72] = {};
        SDL_snprintf(sb,
                     sizeof(sb),
                     "Apercu rejets en marche : vitesse %.2f tuiles/s",
                     static_cast<double>(this->vfxTrailPopupPreviewMarcheSpeedTilesPerSec));
        this->statusMessage = sb;
        return true;
    }
    if (this->pointInRect(x, y, lay.previewZoomMinusBtn))
    {
        this->vfxTrailPopupPreviewZoom = (std::max)(0.4f, this->vfxTrailPopupPreviewZoom - 0.05f);
        char zb[48] = {};
        SDL_snprintf(zb, sizeof(zb), "Apercu trainee : zoom %.2f", static_cast<double>(this->vfxTrailPopupPreviewZoom));
        this->statusMessage = zb;
        return true;
    }
    if (this->pointInRect(x, y, lay.previewZoomPlusBtn))
    {
        this->vfxTrailPopupPreviewZoom = (std::min)(1.0f, this->vfxTrailPopupPreviewZoom + 0.05f);
        char zb[48] = {};
        SDL_snprintf(zb, sizeof(zb), "Apercu trainee : zoom %.2f", static_cast<double>(this->vfxTrailPopupPreviewZoom));
        this->statusMessage = zb;
        return true;
    }

    if (this->vfxTrailPopupIdleRingWhenStationary && this->pointInRect(x, y, lay.previewCrownShipToggleBtn))
    {
        this->vfxTrailPopupPreviewCrownShipVisible = !this->vfxTrailPopupPreviewCrownShipVisible;
        this->statusMessage = this->vfxTrailPopupPreviewCrownShipVisible
                                  ? "Apercu couronne : navire visible."
                                  : "Apercu couronne : navire masque.";
        return true;
    }
    if (popupMarcheEveryNClick > 0 && this->pointInRect(x, y, lay.previewMarcheShipToggleBtn))
    {
        this->vfxTrailPopupPreviewMarcheShipVisible = !this->vfxTrailPopupPreviewMarcheShipVisible;
        this->statusMessage = this->vfxTrailPopupPreviewMarcheShipVisible
                                  ? "Apercu rejets en marche : navire visible."
                                  : "Apercu rejets en marche : navire masque.";
        return true;
    }

    if (this->pointInRect(x, y, lay.previewMarcheRect) || this->pointInRect(x, y, lay.previewArretRect))
    {
        return true;
    }

    if (!this->pointInRect(x, y, lay.popup))
    {
        if (this->pointInRect(x, y, lay.dimFullMap))
        {
            this->closeVfxTrailPopup();
            this->statusMessage = "TraÃƒÂ®nÃƒÂ©e : annule.";
        }
        return true;
    }

    if (this->pointInRect(x, y, lay.cancelBtn))
    {
        this->closeVfxTrailPopup();
        this->statusMessage = "TraÃƒÂ®nÃƒÂ©e : annule.";
        return true;
    }

    if (this->pointInRect(x, y, lay.clearBtn))
    {
        if (this->vfxTrailPopupParentInstanceIndex >= 0 &&
            this->vfxTrailPopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& t = this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
            this->removeTrailPiecesWithSourceInstanceId(t.instanceId);
            t.motionTrailEveryNTiles = 0;
            t.motionTrailLifetimeTiles = 12;
            t.motionTrailStrictTilePlacement = true;
            t.motionTrailLateralJitterRadius = 0.0f;
            t.motionTrailConeOffsetX = 0.0f;
            t.motionTrailConeOffsetY = 0.0f;
            t.motionTrailConeDirectionOffsetDeg = 0.0f;
            t.motionTrailConeHalfAngleDeg = 28.0f;
            t.motionTrailConeSpawnCount = 1;
            t.motionTrailRotationRandomPercent = 0;
            t.motionTrailIdleRingWhenStationary = false;
            t.motionTrailIdleRingRadius = 48.0f;
            t.motionTrailIdleSpawnPeriodMs = 600;
            t.motionTrailIdleRingPieceCount = 8;
            t.motionTrailIdleRingRotationRandomPercent = 0;
            t.motionTrailIdleRingPositionJitterRadius = 0.0f;
            t.motionTrailIdleSpawnAccSec = 0.0f;
            t.motionTrailIdleRingSalvoPiecesRemaining = 0;
            t.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
            t.motionTrailDistanceAcc = 0.0f;
            t.motionSpawnCaptured = false;
            this->markShipVfxDirty();
        }
        this->closeVfxTrailPopup();
        this->statusMessage = "SPAWN / trainee : efface (rejets supprimes, spawn non memorise).";
        return true;
    }

    if (this->pointInRect(x, y, lay.trailStrictTileBtn))
    {
        this->vfxTrailPopupStrictTilePlacement = true;
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        this->statusMessage =
            "Trainee : un rejet tous les N tuiles parcourues, avec le meme decal X/Y que le layer (comme la preview).";
        return true;
    }
    if (this->pointInRect(x, y, lay.trailLateralSpreadBtn))
    {
        this->vfxTrailPopupStrictTilePlacement = false;
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        this->openVfxTrailConePopupForInstanceIndex(this->vfxTrailPopupParentInstanceIndex);
        return true;
    }

    if (this->pointInRect(x, y, lay.everyNTilesInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = true;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (this->pointInRect(x, y, lay.lifetimeTilesInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = true;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (lay.lateralSectionVisible && this->pointInRect(x, y, lay.lateralJitterInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        this->openVfxTrailConePopupForInstanceIndex(this->vfxTrailPopupParentInstanceIndex);
        return true;
    }
    if (this->pointInRect(x, y, lay.rotationPctInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = true;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (this->pointInRect(x, y, lay.idleRingOffBtn))
    {
        this->vfxTrailPopupIdleRingWhenStationary = false;
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        this->statusMessage = "Couronne a l'arret : desactivee.";
        return true;
    }
    if (this->pointInRect(x, y, lay.idleRingOnBtn))
    {
        this->vfxTrailPopupIdleRingWhenStationary = true;
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        this->statusMessage =
            "Couronne a l'arret : activee (preview popup).";
        return true;
    }
    if (lay.idleRingInputsVisible && this->pointInRect(x, y, lay.idleRingPieceCountInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = true;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (lay.idleRingInputsVisible && this->pointInRect(x, y, lay.idleRingRadiusInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = true;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (lay.idleRingInputsVisible && this->pointInRect(x, y, lay.idleRingRotationPctInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = true;
        this->vfxTrailPopupIdleRingPosJitterFocused = false;
        return true;
    }
    if (lay.idleRingInputsVisible && this->pointInRect(x, y, lay.idleRingPosJitterInputRect))
    {
        this->vfxTrailPopupEveryNTilesFocused = false;
        this->vfxTrailPopupLifetimeTilesFocused = false;
        this->vfxTrailPopupLateralJitterFocused = false;
        this->vfxTrailPopupRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPieceCountFocused = false;
        this->vfxTrailPopupIdlePeriodMsFocused = false;
        this->vfxTrailPopupIdleRadiusFocused = false;
        this->vfxTrailPopupIdleRingRotationPctFocused = false;
        this->vfxTrailPopupIdleRingPosJitterFocused = true;
        return true;
    }
    this->vfxTrailPopupEveryNTilesFocused = false;
    this->vfxTrailPopupLifetimeTilesFocused = false;
    this->vfxTrailPopupLateralJitterFocused = false;
    this->vfxTrailPopupRotationPctFocused = false;
    this->vfxTrailPopupIdleRingPieceCountFocused = false;
    this->vfxTrailPopupIdlePeriodMsFocused = false;
    this->vfxTrailPopupIdleRadiusFocused = false;
    this->vfxTrailPopupIdleRingRotationPctFocused = false;
    this->vfxTrailPopupIdleRingPosJitterFocused = false;

    if (this->pointInRect(x, y, lay.validateBtn))
    {
        char* endN = nullptr;
        const long parsedN = std::strtol(this->vfxTrailPopupEveryNTilesInput.c_str(), &endN, 10);
        int nTiles = 0;
        if (endN != this->vfxTrailPopupEveryNTilesInput.c_str())
        {
            nTiles = static_cast<int>(std::clamp(
                parsedN,
                static_cast<long>(kMotionTrailEveryNTilesMin),
                static_cast<long>(kMotionTrailEveryNTilesMax)));
        }
        char* endLife = nullptr;
        const long parsedLife = std::strtol(this->vfxTrailPopupLifetimeTilesInput.c_str(), &endLife, 10);
        int lifeTiles = 12;
        if (endLife != this->vfxTrailPopupLifetimeTilesInput.c_str())
        {
            lifeTiles = static_cast<int>(std::clamp(parsedLife, 1L, 4096L));
        }
        char* endJit = nullptr;
        const long parsedJit = std::strtol(this->vfxTrailPopupLateralJitterInput.c_str(), &endJit, 10);
        float jitterR = 0.0f;
        if (endJit != this->vfxTrailPopupLateralJitterInput.c_str())
        {
            jitterR = static_cast<float>(std::clamp(parsedJit, 0L, 2048L));
        }
        char* endRot = nullptr;
        const long parsedRot = std::strtol(this->vfxTrailPopupRotationPctInput.c_str(), &endRot, 10);
        int rotPct = 0;
        if (endRot != this->vfxTrailPopupRotationPctInput.c_str())
        {
            rotPct = static_cast<int>(std::clamp(parsedRot, 0L, 100L));
        }
        char* endIdlePc = nullptr;
        const long parsedIdlePc =
            std::strtol(this->vfxTrailPopupIdleRingPieceCountInput.c_str(), &endIdlePc, 10);
        int idleRingPc = 8;
        if (endIdlePc != this->vfxTrailPopupIdleRingPieceCountInput.c_str())
        {
            idleRingPc = static_cast<int>(std::clamp(parsedIdlePc, 1L, 32L));
        }
        constexpr int idlePerMs = 600;
        char* endIdleRad = nullptr;
        const long parsedIdleRad = std::strtol(this->vfxTrailPopupIdleRadiusInput.c_str(), &endIdleRad, 10);
        float idleRad = 48.0f;
        if (endIdleRad != this->vfxTrailPopupIdleRadiusInput.c_str())
        {
            idleRad = static_cast<float>(std::clamp(parsedIdleRad, 1L, 2048L));
        }
        char* endIdleRingRot = nullptr;
        const long parsedIdleRingRot =
            std::strtol(this->vfxTrailPopupIdleRingRotationPctInput.c_str(), &endIdleRingRot, 10);
        int idleRingRotPct = 0;
        if (endIdleRingRot != this->vfxTrailPopupIdleRingRotationPctInput.c_str())
        {
            idleRingRotPct = static_cast<int>(std::clamp(parsedIdleRingRot, 0L, 100L));
        }
        char* endIdleRingJit = nullptr;
        const long parsedIdleRingJit =
            std::strtol(this->vfxTrailPopupIdleRingPosJitterInput.c_str(), &endIdleRingJit, 10);
        float idleRingPosJit = 0.0f;
        if (endIdleRingJit != this->vfxTrailPopupIdleRingPosJitterInput.c_str())
        {
            idleRingPosJit = static_cast<float>(std::clamp(parsedIdleRingJit, 0L, 2048L));
        }
        bool didMemorizeSpawnDecal = false;
        if (this->vfxTrailPopupParentInstanceIndex >= 0 &&
            this->vfxTrailPopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& t = this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
            const bool needInitialSpawnCapture = !t.motionSpawnCaptured;
            t.motionTrailEveryNTiles = nTiles;
            t.motionTrailLifetimeTiles = lifeTiles;
            t.motionTrailStrictTilePlacement = this->vfxTrailPopupStrictTilePlacement;
            t.motionTrailLateralJitterRadius =
                this->vfxTrailPopupStrictTilePlacement ? 0.0f : jitterR;
            t.motionTrailRotationRandomPercent = rotPct;
            t.motionTrailIdleRingWhenStationary = this->vfxTrailPopupIdleRingWhenStationary;
            t.motionTrailIdleRingPieceCount = idleRingPc;
            t.motionTrailIdleSpawnPeriodMs = idlePerMs;
            t.motionTrailIdleRingRadius = idleRad;
            t.motionTrailIdleRingRotationRandomPercent = idleRingRotPct;
            t.motionTrailIdleRingPositionJitterRadius = idleRingPosJit;
            t.motionTrailIdleSpawnAccSec = 0.0f;
            t.motionTrailIdleRingSalvoPiecesRemaining = 0;
            t.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
            t.motionTrailDistanceAcc = 0.0f;
            this->markShipVfxDirty();
            if (needInitialSpawnCapture)
            {
                this->captureVfxMotionSpawnAtIndex(this->vfxTrailPopupParentInstanceIndex);
                didMemorizeSpawnDecal = true;
            }
        }
        const bool trailStrictForStatus = this->vfxTrailPopupStrictTilePlacement;
        const bool trailIdleForStatus = this->vfxTrailPopupIdleRingWhenStationary;
        this->closeVfxTrailPopup();
        const std::string spawnTail = didMemorizeSpawnDecal
            ? std::string("Reference SPAWN technique enregistree.")
            : std::string("Reference SPAWN technique conservee.");
        this->statusMessage =
            "SPAWN / trainee : N=" + std::to_string(nTiles) + ", vie=" + std::to_string(lifeTiles) + " tuiles, " +
            (trailStrictForStatus ? std::string("SPAWN (EMPLACEMENT DU LAYER)")
                                  : (std::string("SPAWN (OFFSET CONE) (rayon ") +
                                     std::to_string(static_cast<int>(std::lround(jitterR))) + ")")) +
            ", rotation " + std::to_string(rotPct) + "%" +
            (trailIdleForStatus ? (std::string(", couronne arret ON (") + std::to_string(idleRingPc) +
                                    " pcs, rayon " + std::to_string(static_cast<int>(std::lround(idleRad))) + ", rot. " +
                                    std::to_string(idleRingRotPct) +
                                    " %, pos. alea. max " + std::to_string(static_cast<int>(std::lround(idleRingPosJit))) +
                                    ")")
                                 : std::string(", couronne arret OFF")) +
            ". Base trainee: layer courant. " + spawnTail;
        return true;
    }

    return true;
}

bool EditorMapVfxScene::handleVfxTrailPopupKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)keycode;
    (void)mod;
    if (!this->vfxTrailPopupVisible)
    {
        return false;
    }

    if (scancode == SDL_SCANCODE_ESCAPE && !isrepeat)
    {
        this->closeVfxTrailPopup();
        this->statusMessage = "TraÃƒÂ®nÃƒÂ©e : annule.";
        return true;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_TAB)
    {
        const bool idleIn = this->vfxTrailPopupIdleRingWhenStationary;
        auto clearIdle = [this]() {
            this->vfxTrailPopupIdleRingPieceCountFocused = false;
            this->vfxTrailPopupIdlePeriodMsFocused = false;
            this->vfxTrailPopupIdleRadiusFocused = false;
            this->vfxTrailPopupIdleRingRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPosJitterFocused = false;
        };
        if (this->vfxTrailPopupEveryNTilesFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = true;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            clearIdle();
        }
        else if (this->vfxTrailPopupLifetimeTilesFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = true;
            clearIdle();
        }
        else if (this->vfxTrailPopupLateralJitterFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = true;
            clearIdle();
        }
        else if (this->vfxTrailPopupRotationPctFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            if (idleIn)
            {
                clearIdle();
                this->vfxTrailPopupIdleRingPieceCountFocused = true;
            }
            else
            {
                this->vfxTrailPopupEveryNTilesFocused = true;
                clearIdle();
            }
        }
        else if (this->vfxTrailPopupIdleRingPieceCountFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPieceCountFocused = false;
            this->vfxTrailPopupIdlePeriodMsFocused = false;
            this->vfxTrailPopupIdleRadiusFocused = true;
            this->vfxTrailPopupIdleRingRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPosJitterFocused = false;
        }
        else if (this->vfxTrailPopupIdleRadiusFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPieceCountFocused = false;
            this->vfxTrailPopupIdlePeriodMsFocused = false;
            this->vfxTrailPopupIdleRadiusFocused = false;
            this->vfxTrailPopupIdleRingRotationPctFocused = true;
            this->vfxTrailPopupIdleRingPosJitterFocused = false;
        }
        else if (this->vfxTrailPopupIdleRingRotationPctFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = false;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPieceCountFocused = false;
            this->vfxTrailPopupIdlePeriodMsFocused = false;
            this->vfxTrailPopupIdleRadiusFocused = false;
            this->vfxTrailPopupIdleRingRotationPctFocused = false;
            this->vfxTrailPopupIdleRingPosJitterFocused = true;
        }
        else if (this->vfxTrailPopupIdleRingPosJitterFocused)
        {
            this->vfxTrailPopupEveryNTilesFocused = true;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            clearIdle();
        }
        else
        {
            this->vfxTrailPopupEveryNTilesFocused = true;
            this->vfxTrailPopupLifetimeTilesFocused = false;
            this->vfxTrailPopupLateralJitterFocused = false;
            this->vfxTrailPopupRotationPctFocused = false;
            clearIdle();
        }
        return true;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        char* endN = nullptr;
        const long parsedN = std::strtol(this->vfxTrailPopupEveryNTilesInput.c_str(), &endN, 10);
        int nTiles = 0;
        if (endN != this->vfxTrailPopupEveryNTilesInput.c_str())
        {
            nTiles = static_cast<int>(std::clamp(
                parsedN,
                static_cast<long>(kMotionTrailEveryNTilesMin),
                static_cast<long>(kMotionTrailEveryNTilesMax)));
        }
        char* endLife = nullptr;
        const long parsedLife = std::strtol(this->vfxTrailPopupLifetimeTilesInput.c_str(), &endLife, 10);
        int lifeTiles = 12;
        if (endLife != this->vfxTrailPopupLifetimeTilesInput.c_str())
        {
            lifeTiles = static_cast<int>(std::clamp(parsedLife, 1L, 4096L));
        }
        char* endJit = nullptr;
        const long parsedJit = std::strtol(this->vfxTrailPopupLateralJitterInput.c_str(), &endJit, 10);
        float jitterR = 0.0f;
        if (endJit != this->vfxTrailPopupLateralJitterInput.c_str())
        {
            jitterR = static_cast<float>(std::clamp(parsedJit, 0L, 2048L));
        }
        char* endRot = nullptr;
        const long parsedRot = std::strtol(this->vfxTrailPopupRotationPctInput.c_str(), &endRot, 10);
        int rotPct = 0;
        if (endRot != this->vfxTrailPopupRotationPctInput.c_str())
        {
            rotPct = static_cast<int>(std::clamp(parsedRot, 0L, 100L));
        }
        char* endIdlePc = nullptr;
        const long parsedIdlePc =
            std::strtol(this->vfxTrailPopupIdleRingPieceCountInput.c_str(), &endIdlePc, 10);
        int idleRingPc = 8;
        if (endIdlePc != this->vfxTrailPopupIdleRingPieceCountInput.c_str())
        {
            idleRingPc = static_cast<int>(std::clamp(parsedIdlePc, 1L, 32L));
        }
        constexpr int idlePerMs = 600;
        char* endIdleRad = nullptr;
        const long parsedIdleRad = std::strtol(this->vfxTrailPopupIdleRadiusInput.c_str(), &endIdleRad, 10);
        float idleRad = 48.0f;
        if (endIdleRad != this->vfxTrailPopupIdleRadiusInput.c_str())
        {
            idleRad = static_cast<float>(std::clamp(parsedIdleRad, 1L, 2048L));
        }
        char* endIdleRingRot = nullptr;
        const long parsedIdleRingRot =
            std::strtol(this->vfxTrailPopupIdleRingRotationPctInput.c_str(), &endIdleRingRot, 10);
        int idleRingRotPct = 0;
        if (endIdleRingRot != this->vfxTrailPopupIdleRingRotationPctInput.c_str())
        {
            idleRingRotPct = static_cast<int>(std::clamp(parsedIdleRingRot, 0L, 100L));
        }
        char* endIdleRingJit = nullptr;
        const long parsedIdleRingJit =
            std::strtol(this->vfxTrailPopupIdleRingPosJitterInput.c_str(), &endIdleRingJit, 10);
        float idleRingPosJit = 0.0f;
        if (endIdleRingJit != this->vfxTrailPopupIdleRingPosJitterInput.c_str())
        {
            idleRingPosJit = static_cast<float>(std::clamp(parsedIdleRingJit, 0L, 2048L));
        }
        bool didMemorizeSpawnDecal = false;
        if (this->vfxTrailPopupParentInstanceIndex >= 0 &&
            this->vfxTrailPopupParentInstanceIndex < static_cast<int>(this->currentShipVfxLayers().size()))
        {
            ShipVfxInstance& t = this->currentShipVfxLayers()[static_cast<size_t>(this->vfxTrailPopupParentInstanceIndex)];
            const bool needInitialSpawnCapture = !t.motionSpawnCaptured;
            t.motionTrailEveryNTiles = nTiles;
            t.motionTrailLifetimeTiles = lifeTiles;
            t.motionTrailStrictTilePlacement = this->vfxTrailPopupStrictTilePlacement;
            t.motionTrailLateralJitterRadius =
                this->vfxTrailPopupStrictTilePlacement ? 0.0f : jitterR;
            t.motionTrailRotationRandomPercent = rotPct;
            t.motionTrailIdleRingWhenStationary = this->vfxTrailPopupIdleRingWhenStationary;
            t.motionTrailIdleRingPieceCount = idleRingPc;
            t.motionTrailIdleSpawnPeriodMs = idlePerMs;
            t.motionTrailIdleRingRadius = idleRad;
            t.motionTrailIdleRingRotationRandomPercent = idleRingRotPct;
            t.motionTrailIdleRingPositionJitterRadius = idleRingPosJit;
            t.motionTrailIdleSpawnAccSec = 0.0f;
            t.motionTrailIdleRingSalvoPiecesRemaining = 0;
            t.motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
            t.motionTrailDistanceAcc = 0.0f;
            this->markShipVfxDirty();
            if (needInitialSpawnCapture)
            {
                this->captureVfxMotionSpawnAtIndex(this->vfxTrailPopupParentInstanceIndex);
                didMemorizeSpawnDecal = true;
            }
        }
        const bool trailStrictForStatus = this->vfxTrailPopupStrictTilePlacement;
        const bool trailIdleForStatus = this->vfxTrailPopupIdleRingWhenStationary;
        this->closeVfxTrailPopup();
        const std::string spawnTail = didMemorizeSpawnDecal
            ? std::string("Reference SPAWN technique enregistree.")
            : std::string("Reference SPAWN technique conservee.");
        this->statusMessage =
            "SPAWN / trainee : N=" + std::to_string(nTiles) + ", vie=" + std::to_string(lifeTiles) + " tuiles, " +
            (trailStrictForStatus ? std::string("SPAWN (EMPLACEMENT DU LAYER)")
                                  : (std::string("SPAWN (OFFSET CONE) (rayon ") +
                                     std::to_string(static_cast<int>(std::lround(jitterR))) + ")")) +
            ", rotation " + std::to_string(rotPct) + "%" +
            (trailIdleForStatus ? (std::string(", couronne arret ON (") + std::to_string(idleRingPc) +
                                    " pcs, rayon " + std::to_string(static_cast<int>(std::lround(idleRad))) + ", rot. " +
                                    std::to_string(idleRingRotPct) +
                                    " %, pos. alea. max " + std::to_string(static_cast<int>(std::lround(idleRingPosJit))) +
                                    ")")
                                 : std::string(", couronne arret OFF")) +
            ". Base trainee: layer courant. " + spawnTail;
        return true;
    }

    if (scancode == SDL_SCANCODE_BACKSPACE && !isrepeat)
    {
        if (this->vfxTrailPopupEveryNTilesFocused && !this->vfxTrailPopupEveryNTilesInput.empty())
        {
            this->vfxTrailPopupEveryNTilesInput.pop_back();
        }
        else if (this->vfxTrailPopupLifetimeTilesFocused && !this->vfxTrailPopupLifetimeTilesInput.empty())
        {
            this->vfxTrailPopupLifetimeTilesInput.pop_back();
        }
        else if (this->vfxTrailPopupLateralJitterFocused && !this->vfxTrailPopupLateralJitterInput.empty())
        {
            this->vfxTrailPopupLateralJitterInput.pop_back();
        }
        else if (this->vfxTrailPopupRotationPctFocused && !this->vfxTrailPopupRotationPctInput.empty())
        {
            this->vfxTrailPopupRotationPctInput.pop_back();
        }
        else if (this->vfxTrailPopupIdleRingPieceCountFocused &&
                 !this->vfxTrailPopupIdleRingPieceCountInput.empty())
        {
            this->vfxTrailPopupIdleRingPieceCountInput.pop_back();
        }
        else if (this->vfxTrailPopupIdlePeriodMsFocused && !this->vfxTrailPopupIdlePeriodMsInput.empty())
        {
            this->vfxTrailPopupIdlePeriodMsInput.pop_back();
        }
        else if (this->vfxTrailPopupIdleRadiusFocused && !this->vfxTrailPopupIdleRadiusInput.empty())
        {
            this->vfxTrailPopupIdleRadiusInput.pop_back();
        }
        else if (this->vfxTrailPopupIdleRingRotationPctFocused &&
                 !this->vfxTrailPopupIdleRingRotationPctInput.empty())
        {
            this->vfxTrailPopupIdleRingRotationPctInput.pop_back();
        }
        else if (this->vfxTrailPopupIdleRingPosJitterFocused &&
                 !this->vfxTrailPopupIdleRingPosJitterInput.empty())
        {
            this->vfxTrailPopupIdleRingPosJitterInput.pop_back();
        }
        return true;
    }

    auto appendDigit = [this](char digit) -> bool {
        if (this->vfxTrailPopupEveryNTilesFocused)
        {
            if (this->vfxTrailPopupEveryNTilesInput.size() >= 3U)
            {
                return true;
            }
            this->vfxTrailPopupEveryNTilesInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupLifetimeTilesFocused)
        {
            if (this->vfxTrailPopupLifetimeTilesInput.size() >= 4U)
            {
                return true;
            }
            this->vfxTrailPopupLifetimeTilesInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupLateralJitterFocused)
        {
            if (this->vfxTrailPopupLateralJitterInput.size() >= 4U)
            {
                return true;
            }
            this->vfxTrailPopupLateralJitterInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupRotationPctFocused)
        {
            if (this->vfxTrailPopupRotationPctInput.size() >= 3U)
            {
                return true;
            }
            this->vfxTrailPopupRotationPctInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupIdleRingPieceCountFocused)
        {
            if (this->vfxTrailPopupIdleRingPieceCountInput.size() >= 2U)
            {
                return true;
            }
            this->vfxTrailPopupIdleRingPieceCountInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupIdlePeriodMsFocused)
        {
            if (this->vfxTrailPopupIdlePeriodMsInput.size() >= 5U)
            {
                return true;
            }
            this->vfxTrailPopupIdlePeriodMsInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupIdleRadiusFocused)
        {
            if (this->vfxTrailPopupIdleRadiusInput.size() >= 4U)
            {
                return true;
            }
            this->vfxTrailPopupIdleRadiusInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupIdleRingRotationPctFocused)
        {
            if (this->vfxTrailPopupIdleRingRotationPctInput.size() >= 3U)
            {
                return true;
            }
            this->vfxTrailPopupIdleRingRotationPctInput.push_back(digit);
            return true;
        }
        if (this->vfxTrailPopupIdleRingPosJitterFocused)
        {
            if (this->vfxTrailPopupIdleRingPosJitterInput.size() >= 4U)
            {
                return true;
            }
            this->vfxTrailPopupIdleRingPosJitterInput.push_back(digit);
        }
        return true;
    };

    const bool nFocus = this->vfxTrailPopupEveryNTilesFocused;
    const bool lifeTilesFocus = this->vfxTrailPopupLifetimeTilesFocused;
    const bool jitFocus = this->vfxTrailPopupLateralJitterFocused;
    const bool rotFocus = this->vfxTrailPopupRotationPctFocused;
    const bool idlePcFocus = this->vfxTrailPopupIdleRingPieceCountFocused;
    const bool idlePerFocus = this->vfxTrailPopupIdlePeriodMsFocused;
    const bool idleRadFocus = this->vfxTrailPopupIdleRadiusFocused;
    const bool idleRingRotFocus = this->vfxTrailPopupIdleRingRotationPctFocused;
    const bool idleRingJitFocus = this->vfxTrailPopupIdleRingPosJitterFocused;
    if ((nFocus || lifeTilesFocus || jitFocus || rotFocus || idlePcFocus || idlePerFocus || idleRadFocus ||
         idleRingRotFocus || idleRingJitFocus) &&
        !isrepeat)
    {
        switch (scancode)
        {
        case SDL_SCANCODE_KP_0:
            return appendDigit('0');
        case SDL_SCANCODE_KP_1:
            return appendDigit('1');
        case SDL_SCANCODE_KP_2:
            return appendDigit('2');
        case SDL_SCANCODE_KP_3:
            return appendDigit('3');
        case SDL_SCANCODE_KP_4:
            return appendDigit('4');
        case SDL_SCANCODE_KP_5:
            return appendDigit('5');
        case SDL_SCANCODE_KP_6:
            return appendDigit('6');
        case SDL_SCANCODE_KP_7:
            return appendDigit('7');
        case SDL_SCANCODE_KP_8:
            return appendDigit('8');
        case SDL_SCANCODE_KP_9:
            return appendDigit('9');
        default:
            break;
        }
    }

    if ((nFocus || lifeTilesFocus || jitFocus || rotFocus || idlePcFocus || idlePerFocus || idleRadFocus ||
         idleRingRotFocus || idleRingJitFocus) &&
        key != nullptr && std::strlen(key) == 1U)
    {
        const char c = key[0];
        if (c >= '0' && c <= '9')
        {
            if (nFocus && this->vfxTrailPopupEveryNTilesInput.size() < 3U)
            {
                this->vfxTrailPopupEveryNTilesInput.push_back(c);
                return true;
            }
            if (lifeTilesFocus && this->vfxTrailPopupLifetimeTilesInput.size() < 4U)
            {
                this->vfxTrailPopupLifetimeTilesInput.push_back(c);
                return true;
            }
            if (jitFocus && this->vfxTrailPopupLateralJitterInput.size() < 4U)
            {
                this->vfxTrailPopupLateralJitterInput.push_back(c);
                return true;
            }
            if (rotFocus && this->vfxTrailPopupRotationPctInput.size() < 3U)
            {
                this->vfxTrailPopupRotationPctInput.push_back(c);
                return true;
            }
            if (idlePcFocus && this->vfxTrailPopupIdleRingPieceCountInput.size() < 2U)
            {
                this->vfxTrailPopupIdleRingPieceCountInput.push_back(c);
                return true;
            }
            if (idlePerFocus && this->vfxTrailPopupIdlePeriodMsInput.size() < 5U)
            {
                this->vfxTrailPopupIdlePeriodMsInput.push_back(c);
                return true;
            }
            if (idleRadFocus && this->vfxTrailPopupIdleRadiusInput.size() < 4U)
            {
                this->vfxTrailPopupIdleRadiusInput.push_back(c);
                return true;
            }
            if (idleRingRotFocus && this->vfxTrailPopupIdleRingRotationPctInput.size() < 3U)
            {
                this->vfxTrailPopupIdleRingRotationPctInput.push_back(c);
                return true;
            }
            if (idleRingJitFocus && this->vfxTrailPopupIdleRingPosJitterInput.size() < 4U)
            {
                this->vfxTrailPopupIdleRingPosJitterInput.push_back(c);
                return true;
            }
        }
    }

    return true;
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
        if (clickedIndex == this->selectedShipIndex)
        {
            return true;
        }

        this->saveCurrentShipVfxPairDraft();
        if (!this->selectImportedShipAtIndex(clickedIndex))
        {
            return true;
        }
        if (this->restoreCurrentShipVfxPairDraft())
        {
            this->statusMessage += " | Brouillon ship+vfx restaure.";
        }
    }
    return true;
}


bool EditorMapVfxScene::handleSfxListClick(float x, float y, RC2D_MouseButton button)
{
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

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
        if (clickedIndex == this->selectedSfxIndex)
        {
            this->sfxListActionButtonsVisible = true;
            this->sfxListActionButtonsSfxIndex = this->selectedSfxIndex;
            this->statusMessage = "VFX selectionne. Choisis: AUTO IMPORT ou AJOUTER INSTANCE.";
            return true;
        }

        this->saveCurrentShipVfxPairDraft();
        this->selectedSfxIndex = clickedIndex;
        this->ensureSelectionVisible(this->selectedSfxIndex, &this->sfxListScrollOffset, static_cast<int>(this->importedSfx.size()));
        this->sfxListActionButtonsVisible = true;
        this->sfxListActionButtonsSfxIndex = this->selectedSfxIndex;

        if (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()) &&
            this->restoreCurrentShipVfxPairDraft())
        {
            this->statusMessage = "VFX selectionne. Brouillon ship+vfx restaure.";
        }
        else
        {
            this->clearAllShipVfxLayerPages();
            this->clearShipVfxTrailPieces();
            this->initDefaultShipLayerSettingsAllPages();
            this->setSelectedVfxInstanceIndex(-1);
            this->nextVfxInstanceId = 1U;
            this->shipVfxDirty = false;

            if (this->selectedShipIndex >= 0 && this->selectedShipIndex < static_cast<int>(this->importedShips.size()))
            {
                this->loadedShipVfxConfigPath = this->buildShipVfxPairConfigJsonPath(
                    this->importedShips[static_cast<size_t>(this->selectedShipIndex)],
                    this->importedSfx[static_cast<size_t>(this->selectedSfxIndex)]);
                this->statusMessage = "VFX selectionne. Brouillon vide pour ce couple: AUTO IMPORT ou AJOUTER INSTANCE.";
            }
            else
            {
                this->loadedShipVfxConfigPath.clear();
                this->statusMessage = "VFX selectionne. Choisis: AUTO IMPORT ou AJOUTER INSTANCE.";
            }
        }
    }
    return true;
}

bool EditorMapVfxScene::handleSfxListActionButtonsClick(float x, float y, RC2D_MouseButton button)
{
    if (button != RC2D_MOUSE_BUTTON_LEFT ||
        !this->sfxListActionButtonsVisible ||
        this->editorMode != EditorMode::SHIP_VFX)
    {
        return false;
    }

    if (!this->pointInRect(x, y, this->sfxListActionAutoImportRect) &&
        !this->pointInRect(x, y, this->sfxListActionAddInstanceRect))
    {
        return false;
    }

    if (this->sfxListActionButtonsSfxIndex < 0 ||
        this->sfxListActionButtonsSfxIndex >= static_cast<int>(this->importedSfx.size()))
    {
        this->statusMessage = "Selection VFX invalide.";
        return true;
    }

    this->selectedSfxIndex = this->sfxListActionButtonsSfxIndex;
    this->ensureSelectionVisible(
        this->selectedSfxIndex,
        &this->sfxListScrollOffset,
        static_cast<int>(this->importedSfx.size()));

    if (this->pointInRect(x, y, this->sfxListActionAutoImportRect))
    {
        bool pairFileFound = false;
        if (this->tryAutoImportShipVfxConfigForSelectedPair(&pairFileFound))
        {
            this->statusMessage += " | Tu peux ensuite cliquer AJOUTER INSTANCE.";
            return true;
        }
        if (!pairFileFound)
        {
            this->statusMessage = "Aucun JSON ship+vfx trouve pour ce couple.";
            return true;
        }
        return true;
    }

    this->spawnSelectedSfxAtShipCenter();
    return true;
}

bool EditorMapVfxScene::handleLayerListClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->pointInRect(x, y, this->layerListRect))
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT && button != RC2D_MOUSE_BUTTON_RIGHT)
    {
        return false;
    }

    const std::vector<int> orderedLayerIndices = this->expandLayerPanelDisplayRows(
        this->getOrderedVfxInstanceIndicesForLayerPanel());
    const int itemCount = static_cast<int>(orderedLayerIndices.size());

    const float panelPadding = 4.0f;
    const float headerHeight = 14.0f;
    const float rowGap = kLayerListPanelRowGap;
    const float rowsTopY = this->layerListRect.y + panelPadding + headerHeight + 1.0f;
    const float rowsHeight =
        this->layerListRect.h - ((panelPadding * 2.0f) + headerHeight + ((kVisibleListRows - 1) * rowGap));
    const float rowHeight = rowsHeight / static_cast<float>(kVisibleListRows);
    const float rowsLeftX = this->layerListRect.x + panelPadding;
    const float rowsWidth = this->layerListRect.w - ((panelPadding * 2.0f) + kListScrollBarWidth + 4.0f);

    const int pickRowCount = this->shipVfxEffectiveLayerPageCount();
    const ShipVfxLayerPagePickerLayout pagePickLayout = buildShipVfxLayerPagePickerLayout(
        this->layerListRect,
        panelPadding,
        headerHeight,
        this->shipVfxLayerPagePickerOpen,
        pickRowCount);

    if (button == RC2D_MOUSE_BUTTON_LEFT && this->pointInRect(x, y, pagePickLayout.targetSectorsOverlayToggleRect))
    {
        if (!this->shipVfxEditorTargetSectorsABEnabled)
        {
            this->statusMessage = "Active d'abord A/B (16 pages) pour afficher les secteurs sur la preview.";
            return true;
        }
        this->toggleShipVfxEditorTargetSectorsOverlayVisible();
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT && this->pointInRect(x, y, pagePickLayout.targetSectorsToggleRect))
    {
        this->toggleShipVfxEditorTargetSectorsAB();
        return true;
    }

    if (this->shipVfxLayerPagePickerOpen)
    {
        if (button == RC2D_MOUSE_BUTTON_RIGHT)
        {
            this->shipVfxLayerPagePickerOpen = false;
            return true;
        }
        for (int pi = 0; pi < pickRowCount; ++pi)
        {
            if (this->pointInRect(x, y, pagePickLayout.pageRowRects[pi]))
            {
                this->applyShipVfxLayerPageIndex(pi);
                char buf[180] = {};
                shipVfxLayerPageLabelUtf8(pi, this->shipVfxEditorTargetSectorsABEnabled, buf, sizeof(buf));
                this->statusMessage = std::string("Page calques: ") + buf;
                return true;
            }
        }
        if (this->pointInRect(x, y, pagePickLayout.pageButtonRect))
        {
            this->shipVfxLayerPagePickerOpen = false;
            return true;
        }
        if (!this->pointInRect(x, y, pagePickLayout.popupRect))
        {
            this->shipVfxLayerPagePickerOpen = false;
            return true;
        }
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT && this->pointInRect(x, y, pagePickLayout.duplicateToPagesButtonRect))
    {
        this->openVfxDuplicateToPagesPopupFromSelectedVfx();
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT && this->pointInRect(x, y, pagePickLayout.pageButtonRect))
    {
        this->shipVfxLayerPagePickerOpen = true;
        return true;
    }

    SDL_FRect scrollTrackRect{};
    scrollTrackRect.x = rowsLeftX + rowsWidth + 4.0f;
    scrollTrackRect.y = rowsTopY;
    scrollTrackRect.w = kListScrollBarWidth;
    scrollTrackRect.h = rowsHeight;

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        if (button == RC2D_MOUSE_BUTTON_LEFT)
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
        }
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

    SDL_FRect vfxPlaceModeRect{};
    vfxPlaceModeRect.w = kLayerRowVfxPlaceModeButtonW;
    vfxPlaceModeRect.h = lockRect.h;
    vfxPlaceModeRect.x = debugRect.x - vfxPlaceModeRect.w - 4.0f;
    vfxPlaceModeRect.y = lockRect.y;

    SDL_FRect layerRotateDialRect{};
    layerRotateDialRect.w = kLayerRowVfxRotateDialButtonW;
    layerRotateDialRect.h = lockRect.h;
    layerRotateDialRect.x = vfxPlaceModeRect.x - layerRotateDialRect.w - 4.0f;
    layerRotateDialRect.y = lockRect.y;

    SDL_FRect layerFlipHRowRect{};
    layerFlipHRowRect.w = kLayerRowFlipButtonW;
    layerFlipHRowRect.h = lockRect.h;
    layerFlipHRowRect.x = layerRotateDialRect.x - layerFlipHRowRect.w - 4.0f;
    layerFlipHRowRect.y = lockRect.y;

    SDL_FRect layerFlipVRowRect{};
    layerFlipVRowRect.w = kLayerRowFlipButtonW;
    layerFlipVRowRect.h = lockRect.h;
    layerFlipVRowRect.x = layerFlipHRowRect.x - layerFlipVRowRect.w - 4.0f;
    layerFlipVRowRect.y = lockRect.y;

    SDL_FRect layerRelativeRect{};
    layerRelativeRect.w = kLayerRowRelativeButtonW;
    layerRelativeRect.h = lockRect.h;
    layerRelativeRect.x = layerFlipVRowRect.x - layerRelativeRect.w - 4.0f;
    layerRelativeRect.y = lockRect.y;

    SDL_FRect layerMotionSpawnCibleRect{};
    layerMotionSpawnCibleRect.w = kLayerRowMotionSpawnCibleToggleW;
    layerMotionSpawnCibleRect.h = lockRect.h;
    layerMotionSpawnCibleRect.x = layerRelativeRect.x - kLayerRowMotionClusterGap - layerMotionSpawnCibleRect.w;
    layerMotionSpawnCibleRect.y = lockRect.y;

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
        if (rowValue == -1)
        {
            this->shipLayerSelected = true;
            this->setSelectedVfxInstanceIndex(-1);
            return;
        }
        if (layerPanelRowIsTrailPieceSubRow(rowValue))
        {
            const uint32_t uid = layerPanelTrailPieceUiIdFromRow(rowValue);
            const int pi = this->findTrailPieceIndexByLayerPanelUiId(uid);
            if (pi >= 0)
            {
                const uint32_t sid = this->currentShipVfxTrailPieces()[static_cast<size_t>(pi)].sourceVfxInstanceId;
                const int pIx = this->findVfxLayerIndexByInstanceId(sid);
                if (pIx >= 0)
                {
                    this->shipLayerSelected = false;
                    this->setSelectedVfxInstanceIndex(pIx);
                }
            }
            return;
        }

        this->shipLayerSelected = false;
        this->setSelectedVfxInstanceIndex(rowValue);
    };

    const int clickedRowValue = orderedLayerIndices[static_cast<size_t>(clickedIndex)];
    const bool clickedTrailPieceSubRow = layerPanelRowIsTrailPieceSubRow(clickedRowValue);
    if (clickedTrailPieceSubRow)
    {
        clearLayerDragState();
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            selectClickedRow();
            this->ensureSelectionVisible(
                this->getSelectedLayerRowIndexForDisplay(orderedLayerIndices),
                &this->layerListScrollOffset,
                itemCount);
        }
        return true;
    }

    const bool clickedIsShipRow =
        clickedIndex >= 0 &&
        clickedIndex < static_cast<int>(orderedLayerIndices.size()) &&
        clickedRowValue == -1;
    const int clickedInstanceIndex =
        (!clickedIsShipRow && !clickedTrailPieceSubRow && clickedIndex >= 0 &&
         clickedIndex < static_cast<int>(orderedLayerIndices.size()))
        ? clickedRowValue
        : -1;

    if (this->pointInRect(x, y, layerMotionSpawnCibleRect))
    {
        selectClickedRow();
        clearLayerDragState();
        if (clickedIsShipRow)
        {
            this->statusMessage = "SPAWN / trainee : layer VFX uniquement (SHIP : -).";
            return true;
        }
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            this->openVfxTrailPopupForInstanceIndex(clickedInstanceIndex);
            return true;
        }
        return true;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        clearLayerDragState();
        return true;
    }

    if (this->pointInRect(x, y, layerRelativeRect))
    {
        selectClickedRow();
        clearLayerDragState();
        this->openVfxRelativeTimingPopup(clickedIsShipRow, clickedInstanceIndex);
        return true;
    }

    if (this->pointInRect(x, y, layerFlipVRowRect))
    {
        selectClickedRow();
        clearLayerDragState();
        if (clickedIsShipRow)
        {
            this->statusMessage = "FLIP V : option des layers VFX uniquement (SHIP affiche -).";
            return true;
        }
        this->toggleVfxInstanceFlipVerticalAtIndex(clickedInstanceIndex);
        return true;
    }

    if (this->pointInRect(x, y, layerFlipHRowRect))
    {
        selectClickedRow();
        clearLayerDragState();
        if (clickedIsShipRow)
        {
            this->statusMessage = "FLIP H : option des layers VFX uniquement (SHIP affiche -).";
            return true;
        }
        this->toggleVfxInstanceFlipHorizontalAtIndex(clickedInstanceIndex);
        return true;
    }

    if (this->pointInRect(x, y, layerRotateDialRect))
    {
        const int prevSelectedVfx = this->selectedVfxInstanceIndex;
        selectClickedRow();
        clearLayerDragState();
        if (clickedIsShipRow)
        {
            this->vfxRotationDialActive = false;
            this->statusMessage = "ROT : disponible pour les layers VFX (pas SHIP).";
            return true;
        }
        const ShipVfxInstance& rotTarget = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
        if (rotTarget.locked)
        {
            this->vfxRotationDialActive = false;
            this->statusMessage = "Layer verrouille : ROT refuse.";
            return true;
        }
        if (this->vfxRotationDialActive && prevSelectedVfx == clickedInstanceIndex)
        {
            this->vfxRotationDialActive = false;
            this->statusMessage = "Mode ROT desactive.";
        }
        else
        {
            this->vfxRotationDialActive = true;
            this->statusMessage =
                "Mode ROT : orientez avec la souris, puis clic gauche sur la carte pour valider. "
                "ECHAP ou reclic ROT sur ce layer : quitter le mode.";
        }
        return true;
    }

    if (this->pointInRect(x, y, vfxPlaceModeRect))
    {
        selectClickedRow();
        clearLayerDragState();
        if (clickedIsShipRow)
        {
            this->statusMessage = "Placement TILE/PIXEL : option des layers VFX uniquement (SHIP affiche -).";
            return true;
        }
        ShipVfxInstance& placeInstance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
        placeInstance.placementSnapClickToTile = !placeInstance.placementSnapClickToTile;
        this->statusMessage = placeInstance.placementSnapClickToTile
            ? "Layer: TILE Ã¢â‚¬â€ fantome sur la tuile sous le curseur, clic pour poser (y compris pres d'autres VFX)."
            : "Layer: PIXEL Ã¢â‚¬â€ pas de snap tuile au clic.";
        return true;
    }

    if (this->pointInRect(x, y, debugRect))
    {
        selectClickedRow();
        if (clickedIsShipRow || clickedInstanceIndex < 0)
        {
            this->activeShipDebugBoundsVisible() = !this->activeShipDebugBoundsVisible();
            this->markShipVfxDirty();
            clearLayerDragState();
            this->statusMessage = this->activeShipDebugBoundsVisible() ? "DEBUG SHIP active." : "DEBUG SHIP desactivee.";
            return true;
        }

        ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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

        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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

        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
        if (instance.locked)
        {
            clearLayerDragState();
            this->statusMessage = "Layer verrouille: duplication refusee.";
            return true;
        }

        (void)this->duplicateVfxInstanceAtIndexInCurrentPage(clickedInstanceIndex);
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

        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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
            this->activeShipLayerVisible() = !this->activeShipLayerVisible();
            this->markShipVfxDirty();
        }
        else
        {
            if (clickedInstanceIndex >= 0)
            {
                ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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
            this->activeShipLayerLocked() = !this->activeShipLayerLocked();
            this->markShipVfxDirty();
            clearLayerDragState();
            this->statusMessage = this->activeShipLayerLocked() ? "SHIP lock active." : "SHIP lock desactive.";
            return true;
        }
        else
        {
            if (clickedInstanceIndex >= 0)
            {
                ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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
        canDragRow = !this->activeShipLayerLocked();
        this->statusMessage = "Layer ship selectionne.";
    }
    else
    {
        if (clickedInstanceIndex >= 0)
        {
            const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(clickedInstanceIndex)];
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
        this->loosePreviewFpsInputFocused = false;
        this->loosePreviewTotalDurationMsInputFocused = false;
        if (this->editorMode == EditorMode::LOOSE_SPRITES)
        {
            this->openExportConfirmPopup(ExportConfirmAction::LOOSE_OPEN_EXPORT_FLOW);
        }
        else
        {
            this->openExportConfirmPopup(ExportConfirmAction::SHIP_VFX_ALL_SHIPS);
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
        if (this->pointInRect(x, y, this->buttonTargetFireSectorToggleRect))
        {
            if (!this->shipVfxEditorTargetSectorsABEnabled)
            {
                this->statusMessage = "Cibles A/B desactivees : active le bouton A/B/8pg a cote de DUP+.";
                return true;
            }
            this->cyclePreviewTargetFireSector(1);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonShipOpacityMinusRect))
        {
            this->adjustPreviewShipOpacityPercentStep(-10);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonShipOpacityPlusRect))
        {
            this->adjustPreviewShipOpacityPercentStep(10);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonPreviewIsoGridRect))
        {
            this->previewIsoGridVisible = !this->previewIsoGridVisible;
            this->statusMessage = this->previewIsoGridVisible
                ? "Grille isometrique 30x30 (debug) affichee sous le navire."
                : "Grille isometrique debug masquee.";
            return true;
        }
        if (this->pointInRect(x, y, this->buttonShipVfxZoomMinusRect))
        {
            this->adjustShipVfxPreviewZoom(-0.05f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonShipVfxZoomPlusRect))
        {
            this->adjustShipVfxPreviewZoom(0.05f);
            return true;
        }
    }

    if (this->editorMode == EditorMode::LOOSE_SPRITES)
    {
        if (this->pointInRect(x, y, this->buttonImportLooseRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->openImportLooseFolderDialog();
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLoosePreviewModeRect))
        {
            this->loosePreviewMode = (this->loosePreviewMode == LoosePreviewMode::CENTER_SPRITESHEET)
                ? LoosePreviewMode::PLACEMENT_PREVIEW
                : LoosePreviewMode::CENTER_SPRITESHEET;
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
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
        if (this->pointInRect(x, y, this->buttonLoosePreviewTotalDurationMsInputRect))
        {
            this->loosePreviewFpsInputFocused = false;
            const int durActive = this->getLoosePreviewTotalDurationMsActive();
            this->loosePreviewTotalDurationMsInput = durActive > 0 ? std::to_string(durActive) : std::string{};
            this->loosePreviewTotalDurationMsInputFocused = true;
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLoosePreviewFpsInputRect))
        {
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->loosePreviewFpsInput = formatSfxFpsValue(this->getLoosePreviewFpsOrDefault());
            this->loosePreviewFpsInputFocused = true;
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseClearAllVfxRect) &&
            this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->loosePreviewPlacements.clear();
            this->nextLoosePreviewPlacementId = 1U;
            this->statusMessage = "Toutes les instances preview VFX ont ete supprimees.";
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLoosePreviewPlacementSnapRect) &&
            this->loosePreviewMode == LoosePreviewMode::PLACEMENT_PREVIEW)
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->loosePreviewPlacementSnapToTile = !this->loosePreviewPlacementSnapToTile;
            this->statusMessage = this->loosePreviewPlacementSnapToTile
                ? "Preview VFX: ancrage centre tuile (sous le curseur)."
                : "Preview VFX: ancrage sous-pixel (position exacte du curseur).";
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseScaleMinusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->looseScalePercent = std::clamp(this->looseScalePercent - kLooseScaleStepPercent, kLooseScaleMinPercent, kLooseScaleMaxPercent);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseScalePlusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->looseScalePercent = std::clamp(this->looseScalePercent + kLooseScaleStepPercent, kLooseScaleMinPercent, kLooseScaleMaxPercent);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseZoomMinusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->adjustLoosePreviewZoom(-0.05f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseZoomPlusRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->adjustLoosePreviewZoom(0.05f);
            return true;
        }
        if (this->pointInRect(x, y, this->buttonLooseReferencePreviewRect))
        {
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
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
    this->loosePreviewTotalDurationMsInputFocused = false;
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
        if (instanceIndex < 0 || instanceIndex >= static_cast<int>(this->currentShipVfxLayers().size()))
        {
            return false;
        }

        const ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(instanceIndex)];
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

        const float scale = (std::max)(GetCamera().getZoomFactor(), 0.01f);
        const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
        if (this->shouldSkipDrawImportedSfxForPilotMaxLifetime(imported, timeSeconds))
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

        const auto& hitLayers = this->currentShipVfxLayers();
        const int frameIndex = this->computeVfxPreviewFrameIndex(instance, timeSeconds, hitLayers, imported);
        const ImportedSfxFrame& frame = imported.frames[static_cast<size_t>(frameIndex)];
        float hitOffX = 0.0f;
        float hitOffY = 0.0f;
        this->getVfxPreviewDrawOffsets(instance, override, &hitOffX, &hitOffY);
        const float offsetX = hitOffX * scale;
        const float offsetY = hitOffY * scale;
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
                this->currentShipVfxLayers()[static_cast<size_t>(this->selectedVfxInstanceIndex)].locked)
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
        if (this->vfxRotationDialActive)
        {
            this->vfxDragActive = false;
            this->applySelectedVfxRotationFromScreenPointer(x, y);
            this->vfxRotationDialActive = false;
            this->statusMessage = "Rotation validee (clic gauche sur la carte).";
            return true;
        }

        int hitIndex = this->findTopmostVfxInstanceIndexAtPoint(x, y);
        const ShipVfxInstance* snapCandidate = this->getSelectedVfxInstance();
        const Map& clickMap = GetCurrentMap();
        SDL_Point placedCenterTile{};
        const bool havePlacedTile = this->tryGetScreenTileNearestForSelectedVfxCenter(&placedCenterTile);
        const SDL_Point clickTile = clickMap.screenToTileNearest(x, y);
        if (snapCandidate != nullptr && snapCandidate->placementSnapClickToTile && !snapCandidate->locked &&
            havePlacedTile &&
            (clickTile.x != placedCenterTile.x || clickTile.y != placedCenterTile.y))
        {
            this->snapSelectedVfxCenterToNearestTileAtScreen(x, y);
            return true;
        }
        if (this->hasSelectedVfxInstance() &&
            isPointInsideInstanceBounds(this->selectedVfxInstanceIndex))
        {
            hitIndex = this->selectedVfxInstanceIndex;
        }

        this->setSelectedVfxInstanceIndex(hitIndex);
        if (hitIndex >= 0)
        {
            this->shipLayerSelected = false;
            ShipVfxInstance& instance = this->currentShipVfxLayers()[static_cast<size_t>(hitIndex)];
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
        if (!this->applyLoosePreviewTotalDurationMsInput())
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
        placement.spawnTimeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
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

void EditorMapVfxScene::onImportShipVfxConfigDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapVfxScene* scene = static_cast<EditorMapVfxScene*>(userdata);
    if (scene == nullptr || scene != EditorMapVfxScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingShipVfxConfigMutex);
    scene->pendingShipVfxConfigDialogCompleted = true;
    scene->pendingShipVfxConfigAbsolutePath.clear();
    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingShipVfxConfigDialogCanceled = true;
        return;
    }
    scene->pendingShipVfxConfigDialogCanceled = false;
    scene->pendingShipVfxConfigAbsolutePath = filelist[0];
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
    EditorMapSceneLayout::popBottomToolbarPlayfieldMargins();

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
    this->looseImportBatchActive = false;
    this->looseImportBatchFolderPaths.clear();
    this->looseImportBatchNextIndex = 0U;
    this->looseImportBatchAddedCount = 0;
    this->looseImportBatchReloadedCount = 0;
    this->looseImportBatchFailedCount = 0;
    {
        std::lock_guard<std::mutex> lock(this->pendingExportFolderMutex);
        this->pendingExportFolderDialogCompleted = false;
        this->pendingExportFolderDialogCanceled = false;
        this->pendingExportFolderAbsolute.clear();
        this->pendingExportMode = EditorMode::SHIP_VFX;
    }
    {
        std::lock_guard<std::mutex> lock(this->pendingShipVfxConfigMutex);
        this->pendingShipVfxConfigDialogCompleted = false;
        this->pendingShipVfxConfigDialogCanceled = false;
        this->pendingShipVfxConfigAbsolutePath.clear();
    }
    this->closeExportConfirmPopup();
    this->looseExportNamePopupVisible = false;
    this->looseExportNameInput.clear();
    this->pendingLooseExportAnimationName.clear();
    this->layerNameInput.clear();
    this->layerNameInputFocused = false;
    this->vfxDragActive = false;
    this->vfxRotationDialActive = false;
    this->shipVfxDirty = false;
    this->loadedShipVfxConfigPath.clear();
    this->invalidShipFolders.clear();
    this->invalidVfxFolders.clear();

    this->scrollBarOverlay.unload();
    ResetStorageFontRef(&this->overlayFont);
    this->backgroundWidget.unload();

    RC2D_log(RC2D_LOG_INFO, "EditorMapVfxScene: unloaded");
}

void EditorMapVfxScene::load(void)
{
    EditorMapVfxScene::activeInstance = this;
    EditorMapSceneLayout::pushBottomToolbarPlayfieldMargins();
    this->resetEditorState();
    this->ensureUserStorageFolders();

    this->backgroundWidget.load();

    this->overlayFont = OpenStorageFont(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);
    this->scrollBarOverlay.load();

    Map& map = GetCurrentMap();
    map.update();
    this->updateToolbarLayout();

    this->applyShipVfxModeViewportReset();

    this->loadLooseReferencePreviewAssets();
    this->applySelectedOceanColor();
    this->autoImportAssetsFromDefaultFolders();
    this->statusMessage = "Editor VFX charge. Scan assets/images/ships et assets/images/vfxship termine.";

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
    this->processPendingShipVfxConfigRequest();
    this->processPendingLooseFolderRequest();
    this->processLooseFolderImportBatch();
    this->processPendingExportFolderRequest();
    this->applyPendingOceanColorStep();

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        if (this->shipVfxShipDuplicatePopupVisible)
        {
            ShipVfxShipDuplicatePopupLayout popupLayout{};
            if (this->computeShipVfxShipDuplicatePopupLayout(&popupLayout))
            {
                this->handleListPanelScrollDragFromMouse(
                    popupLayout.sourceShipListRect,
                    static_cast<int>(this->importedShips.size()),
                    &this->shipVfxShipDuplicateSourceShipScrollOffset,
                    &this->shipVfxShipDuplicateSourceShipScrollDragActive,
                    &this->shipVfxShipDuplicateSourceShipScrollDragGrabOffsetY);
                this->handleListPanelScrollDragFromMouse(
                    popupLayout.sourceAnimationListRect,
                    static_cast<int>(this->shipVfxShipDuplicateSourceAnimations.size()),
                    &this->shipVfxShipDuplicateAnimationScrollOffset,
                    &this->shipVfxShipDuplicateAnimationScrollDragActive,
                    &this->shipVfxShipDuplicateAnimationScrollDragGrabOffsetY);
                this->handleListPanelScrollDragFromMouse(
                    popupLayout.targetShipListRect,
                    static_cast<int>(this->importedShips.size()),
                    &this->shipVfxShipDuplicateTargetShipScrollOffset,
                    &this->shipVfxShipDuplicateTargetShipScrollDragActive,
                    &this->shipVfxShipDuplicateTargetShipScrollDragGrabOffsetY);
            }
        }
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
            static_cast<int>(this->currentShipVfxLayers().size()) + 1,
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

        if (this->vfxRotationDialActive)
        {
            if (!this->previewShipLoaded || !this->hasSelectedVfxInstance())
            {
                this->vfxRotationDialActive = false;
            }
            else
            {
                ShipVfxInstance* dialInstance = this->getSelectedVfxInstance();
                if (dialInstance == nullptr || dialInstance->locked)
                {
                    this->vfxRotationDialActive = false;
                }
                else
                {
                    float dialMx = 0.0f;
                    float dialMy = 0.0f;
                    if (this->getMouseRenderPosition(&dialMx, &dialMy))
                    {
                        this->applySelectedVfxRotationFromScreenPointer(dialMx, dialMy);
                    }
                }
            }
        }

        this->updatePreviewShipPilotAndVfxMotion(dt);
        this->updateVfxTrailPopupMarcheSimulation(dt);
        this->updateVfxTrailConePopupDragFromMouse();

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
        this->updateLoosePreviewPlacementExpirations();

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
    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        if (this->previewShipPilotActive)
        {
            this->previewShipPilotActive = false;
            this->clearShipVfxTrailPieces();
        }
        this->shipVfxPreviewZoomFactor = std::clamp(
            camera.getZoomFactor(),
            kLoosePreviewZoomMin,
            kLoosePreviewZoomMax);
    }
    camera.update(map, map.rect);
}

void EditorMapVfxScene::draw(void)
{
    Map& map = GetCurrentMap();

    this->backgroundWidget.draw();

    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);
    if (GetOceanShader().isReady())
    {
        GetOceanShader().draw(map.rect);
    }

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        if (this->previewIsoGridVisible && this->previewShipLoaded)
        {
            this->drawShipVfxDebugIsoGrid();
        }
        this->drawShipVfxTrailPieces();
        this->drawShipVfxTargetFireSectorsOverlay();
        this->drawShipVfxPreview();
        this->drawShipVfxRotationDialOverlay();
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

    if (this->editorMode == EditorMode::SHIP_VFX)
    {
        // Fantome placement tuile : hors clip map.rect pour les gros sprites pres des bords.
        this->drawShipVfxTilePlacementGhost();
    }

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

    if (this->handleExportConfirmPopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleShipVfxShipDuplicatePopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleVfxDuplicateToPagesPopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleVfxTrailConePopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleVfxTrailPopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleVfxRelativeTimingPopupKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLooseExportNameInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLayerNameInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLoosePreviewTotalDurationMsInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    if (this->handleLoosePreviewFpsInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        if (this->vfxRotationDialActive)
        {
            this->vfxRotationDialActive = false;
            this->statusMessage = "Mode ROT desactive.";
            return;
        }
        if (this->shipVfxLayerPagePickerOpen)
        {
            this->shipVfxLayerPagePickerOpen = false;
            return;
        }
        this->layerNameInputFocused = false;
        this->loosePreviewFpsInputFocused = false;
        this->loosePreviewTotalDurationMsInputFocused = false;
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
            this->openVfxDuplicateToPagesPopupFromSelectedVfx();
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
        if (!isrepeat && scancode == SDL_SCANCODE_PAGEUP)
        {
            this->adjustShipVfxPreviewZoom(0.05f);
            return;
        }
        if (!isrepeat && scancode == SDL_SCANCODE_PAGEDOWN)
        {
            this->adjustShipVfxPreviewZoom(-0.05f);
            return;
        }
        if (!isrepeat &&
            (scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS))
        {
            this->adjustShipVfxPreviewZoom(-0.05f);
            return;
        }
        if (!isrepeat &&
            (scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS))
        {
            this->adjustShipVfxPreviewZoom(0.05f);
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
            this->loosePreviewFpsInputFocused = false;
            this->loosePreviewTotalDurationMsInputFocused = false;
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

    if (this->exportConfirmPopupVisible)
    {
        (void)this->handleExportConfirmPopupMouseClick(x, y, button);
        return;
    }

    if (this->shipVfxShipDuplicatePopupVisible)
    {
        (void)this->handleShipVfxShipDuplicatePopupMouseClick(x, y, button);
        return;
    }

    if (this->vfxDuplicateToPagesPopupVisible)
    {
        (void)this->handleVfxDuplicateToPagesPopupMouseClick(x, y, button);
        return;
    }

    if (this->vfxTrailConePopupVisible)
    {
        (void)this->handleVfxTrailConePopupMouseClick(x, y, button);
        return;
    }

    if (this->vfxTrailPopupVisible)
    {
        (void)this->handleVfxTrailPopupMouseClick(x, y, button);
        return;
    }

    if (this->vfxRelativeTimingPopupVisible)
    {
        (void)this->handleVfxRelativeTimingPopupMouseClick(x, y, button);
        return;
    }

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
        const bool wasLoosePreviewDurationMsInputFocused = this->loosePreviewTotalDurationMsInputFocused;
        const bool clickInLoosePreviewFpsInput =
            (this->editorMode == EditorMode::LOOSE_SPRITES) &&
            this->pointInRect(x, y, this->buttonLoosePreviewFpsInputRect);
        const bool clickInLoosePreviewDurationMsInput =
            (this->editorMode == EditorMode::LOOSE_SPRITES) &&
            this->pointInRect(x, y, this->buttonLoosePreviewTotalDurationMsInputRect);

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
        if (wasLoosePreviewDurationMsInputFocused && !clickInLoosePreviewDurationMsInput)
        {
            this->loosePreviewTotalDurationMsInputFocused = false;
            this->applyLoosePreviewTotalDurationMsInput();
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
            if (this->handleSfxListActionButtonsClick(x, y, button))
            {
                return;
            }
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
            if (this->handleLayerListClick(x, y, button))
            {
                return;
            }
            if (this->pointInRect(x, y, this->buttonShipVfxDuplicateShipsRect))
            {
                this->openShipVfxShipDuplicatePopup();
                return;
            }
            if (this->handleShipListClick(x, y))
            {
                return;
            }
            if (this->handleSfxListClick(x, y, button))
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

    if (button == RC2D_MOUSE_BUTTON_RIGHT && this->editorMode == EditorMode::SHIP_VFX)
    {
        if (this->handleLayerListClick(x, y, button))
        {
            return;
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
        if (button == RC2D_MOUSE_BUTTON_LEFT && this->shipVfxLayerPagePickerOpen &&
            !this->pointInRect(x, y, this->layerListRect))
        {
            this->shipVfxLayerPagePickerOpen = false;
        }
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
    (void)mouse_x;
    (void)mouse_y;
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
    if (this->exportConfirmPopupVisible)
    {
        return;
    }

    // Meme espace que mousepressed / handleListPanelClick : coordonnees rendu SDL.
    float wheelMx = 0.0f;
    float wheelMy = 0.0f;
    this->getMouseRenderPosition(&wheelMx, &wheelMy);
    if (this->handleShipVfxShipDuplicatePopupMouseWheel(delta, wheelMx, wheelMy))
    {
        return;
    }

    auto isMouseInsidePanel = [this, wheelMx, wheelMy](const SDL_FRect& panelRect) -> bool {
        return this->pointInRect(wheelMx, wheelMy, panelRect);
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
        const Map& wheelMap = GetCurrentMap();
        const int layerItemCount = static_cast<int>(this->expandLayerPanelDisplayRows(
            this->getOrderedVfxInstanceIndicesForLayerPanel()).size());
        // Ordre aligne sur mousepressed : ignores, calques, puis listes navire / VFX.
        if (scrollInvalidPanel(this->invalidVfxListRect, static_cast<int>(this->invalidVfxFolders.size()), &this->invalidVfxListScrollOffset))
        {
            return;
        }
        if (scrollInvalidPanel(this->invalidShipListRect, static_cast<int>(this->invalidShipFolders.size()), &this->invalidShipListScrollOffset))
        {
            return;
        }
        if (scrollListPanel(this->layerListRect, layerItemCount, &this->layerListScrollOffset))
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
        if (this->pointInRect(wheelMx, wheelMy, wheelMap.rect))
        {
            constexpr float kShipVfxWheelZoomStep = 0.05f;
            this->adjustShipVfxPreviewZoom(delta > 0 ? kShipVfxWheelZoomStep : -kShipVfxWheelZoomStep);
        }
        return;
    }

    if (scrollListPanel(this->looseListRect, static_cast<int>(this->importedLooseFolders.size()), &this->looseListScrollOffset))
    {
        return;
    }

}

#endif // GAME_ENV_DEV





