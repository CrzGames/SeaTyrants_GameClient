#include "game/vfx/vfx.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>

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

    // 4) Faire avancer l'horloge d'animation.
    this->playbackSeconds += static_cast<float>(dt);

    // 5) Normaliser periodiquement pour eviter des floats trop grands.
    const float period = this->resolvedPlaybackPeriodSeconds();
    if (period > 0.0001f && this->playbackSeconds > (period * 8192.0f))
    {
        this->playbackSeconds = std::fmod(this->playbackSeconds, period);
    }
}

void VFX::draw(const Map& map, const Ship& ship, bool drawBehindShip) const
{
    // 1) Guard global.
    if (!this->isLoaded())
    {
        return;
    }

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
