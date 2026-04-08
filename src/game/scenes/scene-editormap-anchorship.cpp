#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-anchorship.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_storage.h>
#include <cJSON.h>

#include "core/context.h"
#include "game/render/world-render-clip.h"

struct OceanColorEntry
{
    OceanShader::WaterColor value;
    const char* label;
};

constexpr int kShipSpriteCount = 8;

constexpr RC2D_Color kHudTextColor = RC2D_Color{235, 242, 250, 250};
constexpr RC2D_Color kButtonFillColor = RC2D_Color{36, 44, 52, 190};
constexpr RC2D_Color kButtonFillActiveColor = RC2D_Color{95, 145, 190, 210};
constexpr RC2D_Color kButtonBorderColor = RC2D_Color{140, 150, 165, 220};
constexpr RC2D_Color kButtonBorderActiveColor = RC2D_Color{160, 215, 255, 250};
constexpr int kPreviewClickDistanceTiles = 4;
constexpr double kPreviewPauseAfterArrivalSec = 0.55;
constexpr float kSpritePreviewZoomMin = 0.25f;
constexpr float kSpritePreviewZoomMax = 8.0f;
constexpr Uint8 kAnchorPlacementSpriteAlpha = 51; // 20% pour voir la tile dessous.

constexpr std::array<RC2D_FileDialogFilter, 1> kFolderFilters = {{
    {"Dossier navire", "*"},
}};

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

static std::string normalizePathSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static std::string shortenMiddle(const std::string& text, size_t maxLen)
{
    if (text.size() <= maxLen || maxLen < 8)
    {
        return text;
    }

    const size_t headLen = maxLen / 2;
    const size_t tailLen = maxLen - headLen - 3;
    return text.substr(0, headLen) + "..." + text.substr(text.size() - tailLen);
}

EditorMapAnchorShipScene* EditorMapAnchorShipScene::activeInstance = nullptr;

EditorMapAnchorShipScene::EditorMapAnchorShipScene(void)
    : backgroundUiImage{},
      overlayFont{},
      shipFrames{},
      spriteAnchors{},
      selectedSpriteIndex(0),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      cameraZoomBeforeCheck(0.60f),
      showAnchorGuides(true),
      spritePreviewZoom(1.0f),
      loadedShipFolderAbsolute{},
      statusMessage("Editor anchor navire pret."),
      pendingFolderDialogCompleted(false),
      pendingFolderDialogCanceled(false),
      pendingFolderAbsolute{},
      pendingFolderMutex{},
      movementPreviewActive(false),
      movementPreviewBaseTile{},
      movementPreviewCurrentTile{},
      movementPreviewTargets{},
      movementPreviewTargetIndex(0),
      movementPreviewVisualPass(0),
      movementPreviewPauseRemainingSec(0.0),
      movementPreviewShip{},
      buttonImportFolderRect{},
      buttonPrevSpriteRect{},
      buttonNextSpriteRect{},
      buttonCheckMovementRect{},
      buttonStopCheckRect{},
      buttonSaveAnchorRect{},
      buttonResetAnchorRect{},
      buttonToggleGuidesRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{}
{
    this->resetEditorState();
}

EditorMapAnchorShipScene::~EditorMapAnchorShipScene(void)
{
}

void EditorMapAnchorShipScene::resetEditorState(void)
{
    this->selectedSpriteIndex = 0;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->cameraZoomBeforeCheck = 0.60f;
    this->showAnchorGuides = true;
    this->spritePreviewZoom = 1.0f;
    this->loadedShipFolderAbsolute.clear();
    this->statusMessage = "Editor anchor navire pret.";

    this->pendingFolderDialogCompleted = false;
    this->pendingFolderDialogCanceled = false;
    this->pendingFolderAbsolute.clear();
    {
        std::lock_guard<std::mutex> lock(this->pendingFolderMutex);
        this->pendingFolderAbsolute.clear();
    }

    this->movementPreviewActive = false;
    this->movementPreviewBaseTile = SDL_FPoint{0.0f, 0.0f};
    this->movementPreviewCurrentTile = SDL_FPoint{0.0f, 0.0f};
    this->movementPreviewTargets.fill(SDL_FPoint{0.0f, 0.0f});
    this->movementPreviewTargetIndex = 0;
    this->movementPreviewVisualPass = 0;
    this->movementPreviewPauseRemainingSec = 0.0;
    this->movementPreviewShip.setPositionTile(0.0f, 0.0f);
    this->movementPreviewShip.setSpeedTilesPerSecond(4.0f);
    this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::FULL);
    this->movementPreviewShip.unloadSprites();
    // Dans cette scene, le marqueur doit rester visible jusqu'au prochain clic simule.
    this->clickMarker.setDurationSeconds(0.0);
    this->clickMarker.hide();

    for (ShipFrame& frame : this->shipFrames)
    {
        frame.widthPx = 0.0f;
        frame.heightPx = 0.0f;
        frame.loaded = false;
    }

    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        this->spriteAnchors[static_cast<size_t>(i)] = SDL_FPoint{0.5f, 0.5f};
    }
}

void EditorMapAnchorShipScene::unloadShipFrames(void)
{
    for (ShipFrame& frame : this->shipFrames)
    {
        if (frame.image.sdl_texture != nullptr)
        {
            rc2d_graphics_freeImage(&frame.image);
        }
        frame.widthPx = 0.0f;
        frame.heightPx = 0.0f;
        frame.loaded = false;
    }
}

void EditorMapAnchorShipScene::ensureUserStorageFolders(void)
{
    rc2d_storage_userMkdir("editor-ship-anchor");
    rc2d_storage_userMkdir("editor-ship-anchor/current");
}

void EditorMapAnchorShipScene::applySelectedOceanColor(void)
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
        RC2D_log(RC2D_LOG_WARN, "EditorMapAnchorShipScene: echec chargement ocean shader (%s)", entry.label);
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapAnchorShipScene::requestOceanColorStep(int delta)
{
    if (delta == 0)
    {
        return;
    }

    this->pendingOceanColorDelta += delta;
    // Evite une file trop longue si plusieurs events arrivent d'un coup.
    this->pendingOceanColorDelta = (std::max)(this->pendingOceanColorDelta, -8);
    this->pendingOceanColorDelta = (std::min)(this->pendingOceanColorDelta, 8);
}

void EditorMapAnchorShipScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    const int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapAnchorShipScene::cycleOceanColor(int delta)
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

void EditorMapAnchorShipScene::restoreCameraZoomAfterCheck(void)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    camera.setZoomFactor(this->cameraZoomBeforeCheck);
    camera.update(map, map.rect);
}

void EditorMapAnchorShipScene::openShipFolderDialog(void)
{
    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    options.filters = kFolderFilters.data();
    options.num_filters = static_cast<int>(kFolderFilters.size());
    options.default_location = nullptr;
    options.allow_many = false;
    options.title = "Selectionner le dossier navire (1.png..8.png)";
    options.accept_label = "Ouvrir";
    options.cancel_label = "Annuler";

    rc2d_filedialog_openFolder(&EditorMapAnchorShipScene::onOpenShipFolderDialogResult, this, &options);
}

void EditorMapAnchorShipScene::processPendingFolderRequest(void)
{
    bool hasResult = false;
    bool isCanceled = false;
    std::string selectedFolder;
    {
        std::lock_guard<std::mutex> lock(this->pendingFolderMutex);
        hasResult = this->pendingFolderDialogCompleted;
        if (hasResult)
        {
            isCanceled = this->pendingFolderDialogCanceled;
            selectedFolder.swap(this->pendingFolderAbsolute);
            this->pendingFolderDialogCompleted = false;
            this->pendingFolderDialogCanceled = false;
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

bool EditorMapAnchorShipScene::loadShipFolderFromAbsolutePath(const char* folderAbsolutePath)
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
    this->unloadShipFrames();

    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        const std::filesystem::path& sourcePath = sourcePngPaths[static_cast<size_t>(i)];
        std::ifstream input(sourcePath, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            this->statusMessage = "Lecture impossible: " + sourcePath.string();
            this->unloadShipFrames();
            return false;
        }

        const std::streamsize fileSize = input.tellg();
        if (fileSize <= 0)
        {
            this->statusMessage = "Fichier vide: " + sourcePath.string();
            this->unloadShipFrames();
            return false;
        }

        input.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(fileSize));
        if (!input.read(bytes.data(), fileSize))
        {
            this->statusMessage = "Lecture bytes echouee: " + sourcePath.string();
            this->unloadShipFrames();
            return false;
        }

        char userStoragePath[128] = {};
        SDL_snprintf(
            userStoragePath,
            sizeof(userStoragePath),
            "editor-ship-anchor/current/%d.png",
            i + 1);

        if (!rc2d_storage_userWriteFile(userStoragePath, bytes.data(), static_cast<Uint64>(bytes.size())))
        {
            this->statusMessage = "Echec copie user storage: " + std::string(userStoragePath);
            this->unloadShipFrames();
            return false;
        }

        RC2D_Image image = rc2d_graphics_loadImageFromStorage(userStoragePath, RC2D_STORAGE_USER);
        if (image.sdl_texture == nullptr)
        {
            this->statusMessage = "Echec chargement sprite user: " + std::string(userStoragePath);
            this->unloadShipFrames();
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

        ShipFrame frame{};
        frame.image = image;
        frame.widthPx = widthPx;
        frame.heightPx = heightPx;
        frame.loaded = true;
        this->shipFrames[static_cast<size_t>(i)] = frame;
    }

    this->loadedShipFolderAbsolute = normalizePathSlashes(folderPath.string());
    this->selectedSpriteIndex = 0;
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        this->spriteAnchors[static_cast<size_t>(i)] = SDL_FPoint{0.5f, 0.5f};
    }

    const bool loadedFromJson = this->loadAnchorsFromJsonIfPresent(this->loadedShipFolderAbsolute);
    if (loadedFromJson)
    {
        this->statusMessage = "Navire charge + anchors existants importes.";
    }
    else
    {
        this->statusMessage = "Navire charge. Clique sur le sprite pour regler chaque ancre.";
    }

    const bool wasPreviewActive = this->movementPreviewActive;
    this->movementPreviewActive = false;
    this->movementPreviewTargetIndex = 0;
    this->movementPreviewVisualPass = 0;
    this->movementPreviewPauseRemainingSec = 0.0;
    this->movementPreviewTargets.fill(SDL_FPoint{0.0f, 0.0f});

    // Charge aussi le vrai Ship de preview pour reutiliser le rendu/mouvement
    // identique au player (direction/alternance sprites).
    this->movementPreviewShip.unloadSprites();
    this->movementPreviewShip.setSpeedTilesPerSecond(4.0f);
    this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::FULL);
    this->movementPreviewShip.loadSpritesFromFolder("editor-ship-anchor/current", RC2D_STORAGE_USER);
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        const SDL_FPoint anchor = this->spriteAnchors[static_cast<size_t>(i)];
        this->movementPreviewShip.setDrawAnchorForSprite(i, anchor.x, anchor.y);
    }
    if (wasPreviewActive)
    {
        this->restoreCameraZoomAfterCheck();
    }
    return true;
}

bool EditorMapAnchorShipScene::loadAnchorsFromJsonIfPresent(const std::string& folderPath)
{
    if (folderPath.empty())
    {
        return false;
    }

    const std::filesystem::path jsonPath = std::filesystem::path(folderPath) / "ship_anchor.json";
    std::error_code fsError;
    if (!std::filesystem::exists(jsonPath, fsError) ||
        !std::filesystem::is_regular_file(jsonPath, fsError))
    {
        return false;
    }

    std::ifstream input(jsonPath, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        return false;
    }

    const std::streamsize fileSize = input.tellg();
    if (fileSize <= 0)
    {
        return false;
    }

    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(fileSize) + 1, '\0');
    if (!input.read(bytes.data(), fileSize))
    {
        return false;
    }

    cJSON* root = cJSON_Parse(bytes.data());
    if (root == nullptr)
    {
        return false;
    }

    bool loadedAnyAnchor = false;

    auto setAnchorNorm = [this, &loadedAnyAnchor](int spriteIndex, float anchorX, float anchorY) {
        if (spriteIndex < 0 || spriteIndex >= kShipSpriteCount)
        {
            return false;
        }

        if (!std::isfinite(anchorX) || !std::isfinite(anchorY))
        {
            return false;
        }

        this->spriteAnchors[static_cast<size_t>(spriteIndex)] = SDL_FPoint{
            std::clamp(anchorX, 0.0f, 1.0f),
            std::clamp(anchorY, 0.0f, 1.0f)};
        loadedAnyAnchor = true;
        return true;
    };

    auto setAnchorPixels = [this, &setAnchorNorm](int spriteIndex, float anchorPixelX, float anchorPixelY) {
        if (spriteIndex < 0 || spriteIndex >= kShipSpriteCount)
        {
            return false;
        }

        const ShipFrame& frame = this->shipFrames[static_cast<size_t>(spriteIndex)];
        const float widthPx = (std::max)(frame.widthPx, 1.0f);
        const float heightPx = (std::max)(frame.heightPx, 1.0f);
        return setAnchorNorm(spriteIndex, anchorPixelX / widthPx, anchorPixelY / heightPx);
    };

    bool hasDefaultAnchor = false;
    SDL_FPoint defaultAnchor = SDL_FPoint{0.5f, 0.5f};

    const cJSON* rootAnchorX = cJSON_GetObjectItemCaseSensitive(root, "anchorX");
    const cJSON* rootAnchorY = cJSON_GetObjectItemCaseSensitive(root, "anchorY");
    if (cJSON_IsNumber(rootAnchorX) && cJSON_IsNumber(rootAnchorY))
    {
        hasDefaultAnchor = true;
        defaultAnchor.x = std::clamp(static_cast<float>(rootAnchorX->valuedouble), 0.0f, 1.0f);
        defaultAnchor.y = std::clamp(static_cast<float>(rootAnchorY->valuedouble), 0.0f, 1.0f);
    }

    const cJSON* defaultObj = cJSON_GetObjectItemCaseSensitive(root, "default");
    if (cJSON_IsObject(defaultObj))
    {
        const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorX");
        const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorY");
        if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
        {
            hasDefaultAnchor = true;
            defaultAnchor.x = std::clamp(static_cast<float>(jAnchorX->valuedouble), 0.0f, 1.0f);
            defaultAnchor.y = std::clamp(static_cast<float>(jAnchorY->valuedouble), 0.0f, 1.0f);
        }
        else
        {
            const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelX");
            const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelY");
            if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
            {
                const ShipFrame& frame0 = this->shipFrames[0];
                const float widthPx = (std::max)(frame0.widthPx, 1.0f);
                const float heightPx = (std::max)(frame0.heightPx, 1.0f);
                hasDefaultAnchor = true;
                defaultAnchor.x = std::clamp(static_cast<float>(jAnchorPixelX->valuedouble) / widthPx, 0.0f, 1.0f);
                defaultAnchor.y = std::clamp(static_cast<float>(jAnchorPixelY->valuedouble) / heightPx, 0.0f, 1.0f);
            }
        }
    }

    if (hasDefaultAnchor)
    {
        for (int i = 0; i < kShipSpriteCount; ++i)
        {
            setAnchorNorm(i, defaultAnchor.x, defaultAnchor.y);
        }
    }

    const cJSON* framesObj = cJSON_GetObjectItemCaseSensitive(root, "frames");
    if (cJSON_IsObject(framesObj))
    {
        for (int i = 0; i < kShipSpriteCount; ++i)
        {
            char keyPng[16] = {};
            char keySimple[8] = {};
            SDL_snprintf(keyPng, sizeof(keyPng), "%d.png", i + 1);
            SDL_snprintf(keySimple, sizeof(keySimple), "%d", i + 1);

            const cJSON* frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, keyPng);
            if (!cJSON_IsObject(frameObj))
            {
                frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, keySimple);
            }

            if (!cJSON_IsObject(frameObj))
            {
                continue;
            }

            const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorX");
            const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorY");
            if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
            {
                setAnchorNorm(
                    i,
                    static_cast<float>(jAnchorX->valuedouble),
                    static_cast<float>(jAnchorY->valuedouble));
                continue;
            }

            const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelX");
            const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelY");
            if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
            {
                setAnchorPixels(
                    i,
                    static_cast<float>(jAnchorPixelX->valuedouble),
                    static_cast<float>(jAnchorPixelY->valuedouble));
            }
        }
    }

    cJSON_Delete(root);
    return loadedAnyAnchor;
}

bool EditorMapAnchorShipScene::saveAnchorsToJson(void)
{
    if (this->loadedShipFolderAbsolute.empty())
    {
        this->statusMessage = "Aucun dossier navire charge.";
        return false;
    }

    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        this->statusMessage = "Echec allocation JSON anchors.";
        return false;
    }

    cJSON* defaultObj = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "default", defaultObj);
    cJSON_AddNumberToObject(defaultObj, "anchorX", this->spriteAnchors[0].x);
    cJSON_AddNumberToObject(defaultObj, "anchorY", this->spriteAnchors[0].y);

    cJSON* framesObj = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "frames", framesObj);
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        char frameKey[16] = {};
        SDL_snprintf(frameKey, sizeof(frameKey), "%d.png", i + 1);

        cJSON* frameObj = cJSON_CreateObject();
        cJSON_AddNumberToObject(frameObj, "anchorX", this->spriteAnchors[static_cast<size_t>(i)].x);
        cJSON_AddNumberToObject(frameObj, "anchorY", this->spriteAnchors[static_cast<size_t>(i)].y);
        cJSON_AddItemToObject(framesObj, frameKey, frameObj);
    }

    char* jsonText = cJSON_Print(root);
    cJSON_Delete(root);
    if (jsonText == nullptr)
    {
        this->statusMessage = "Echec serialisation JSON anchors.";
        return false;
    }

    const std::filesystem::path outputPath =
        std::filesystem::path(this->loadedShipFolderAbsolute) / "ship_anchor.json";

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        cJSON_free(jsonText);
        this->statusMessage = "Impossible d'ecrire ship_anchor.json.";
        return false;
    }

    output.write(jsonText, static_cast<std::streamsize>(std::strlen(jsonText)));
    const bool writeOk = output.good();
    output.close();
    cJSON_free(jsonText);

    if (!writeOk)
    {
        this->statusMessage = "Ecriture ship_anchor.json echouee.";
        return false;
    }

    this->statusMessage = "ship_anchor.json exporte avec succes.";
    return true;
}

void EditorMapAnchorShipScene::startMovementPreviewCheck(void)
{
    // Sync immediate: le check doit utiliser les anchors en cours d'edition,
    // meme si l'utilisateur n'a pas encore exporte le JSON.
    for (int i = 0; i < kShipSpriteCount; ++i)
    {
        const SDL_FPoint anchor = this->spriteAnchors[static_cast<size_t>(i)];
        this->movementPreviewShip.setDrawAnchorForSprite(i, anchor.x, anchor.y);
    }

    if (!this->shipFrames[0].loaded)
    {
        this->statusMessage = "Charge d'abord un dossier navire.";
        return;
    }

    if (!this->movementPreviewShip.areSpritesLoaded())
    {
        this->statusMessage = "Check impossible: navire preview non charge.";
        return;
    }

    // Pass 1: sprites 1..4 (etat FULL).
    this->movementPreviewVisualPass = 0;
    this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::FULL);

    Map& map = GetCurrentMap();
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    // Sequence de clics simules autour du navire courant (en coordonnees ecran):
    // on couvre les 8 directions pour voir tous les cotes du navire.
    this->movementPreviewTargets[0] = SDL_FPoint{0.0f, -1.0f};   // haut
    this->movementPreviewTargets[1] = SDL_FPoint{-1.0f, -1.0f};  // haut-gauche
    this->movementPreviewTargets[2] = SDL_FPoint{-1.0f, 0.0f};   // gauche
    this->movementPreviewTargets[3] = SDL_FPoint{-1.0f, 1.0f};   // bas-gauche
    this->movementPreviewTargets[4] = SDL_FPoint{0.0f, 1.0f};    // bas
    this->movementPreviewTargets[5] = SDL_FPoint{1.0f, 1.0f};    // bas-droite
    this->movementPreviewTargets[6] = SDL_FPoint{1.0f, 0.0f};    // droite
    this->movementPreviewTargets[7] = SDL_FPoint{1.0f, -1.0f};   // haut-droite

    auto computeTargetFromSimulatedClick = [&map](const SDL_FPoint& shipTilePos, const SDL_FPoint& clickDir, SDL_Point* outTargetTile) -> bool {
        if (outTargetTile == nullptr)
        {
            return false;
        }

        const SDL_Point shipTileRounded = map.roundTile(shipTilePos.x, shipTilePos.y);
        SDL_Point targetTileFromDir = shipTileRounded;
        if (clickDir.x > 0.0f && clickDir.y < 0.0f)
        {
            // haut-droite ecran => nord en iso (x, y-d)
            targetTileFromDir = SDL_Point{
                shipTileRounded.x,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.x > 0.0f && clickDir.y > 0.0f)
        {
            // bas-droite ecran => est en iso (x+d, y)
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y};
        }
        else if (clickDir.x < 0.0f && clickDir.y > 0.0f)
        {
            // bas-gauche ecran => sud en iso (x, y+d)
            targetTileFromDir = SDL_Point{
                shipTileRounded.x,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }
        else if (clickDir.x < 0.0f && clickDir.y < 0.0f)
        {
            // haut-gauche ecran => ouest en iso (x-d, y)
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y};
        }
        else if (clickDir.x > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.x < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }
        else if (clickDir.y < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.y > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }

        // Simulation du clic: on calcule l'ecran de la tuile visee puis on repasse
        // par la conversion ecran->tuile, comme un vrai clic map.
        const SDL_FPoint clickScreen = map.tileToScreenCenter(targetTileFromDir.x, targetTileFromDir.y);
        SDL_Point targetTile = map.screenToTileNearest(clickScreen.x, clickScreen.y);
        if (targetTile.x == shipTileRounded.x && targetTile.y == shipTileRounded.y)
        {
            targetTile = targetTileFromDir;
        }

        *outTargetTile = targetTile;
        return true;
    };

    // Validation prealable: on simule exactement la suite de clics avec le vrai A*.
    this->movementPreviewShip.setPositionTileInt(centerTile.x, centerTile.y);
    for (const SDL_FPoint& clickDir : this->movementPreviewTargets)
    {
        SDL_Point targetTile{};
        if (!computeTargetFromSimulatedClick(this->movementPreviewShip.getPositionTile(), clickDir, &targetTile))
        {
            this->statusMessage = "Check impossible: calcul cible clic echoue.";
            return;
        }

        if (!map.isInside(targetTile.x, targetTile.y))
        {
            this->statusMessage = "Check impossible: une cible sort de la map.";
            return;
        }

        this->movementPreviewShip.moveToTile(map, targetTile.x, targetTile.y);
        if (!this->movementPreviewShip.isMoving())
        {
            this->statusMessage = "Check impossible: A* ne trouve pas de chemin vers une direction.";
            return;
        }

        int safetyTick = 0;
        while (this->movementPreviewShip.isMoving() && safetyTick < 2400)
        {
            this->movementPreviewShip.update(1.0 / 60.0, map);
            safetyTick += 1;
        }
        if (this->movementPreviewShip.isMoving())
        {
            this->statusMessage = "Check impossible: simulation A* incomplete.";
            return;
        }
    }

    this->movementPreviewBaseTile = SDL_FPoint{
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y)};
    this->movementPreviewCurrentTile = this->movementPreviewBaseTile;
    this->movementPreviewTargetIndex = 0;
    this->movementPreviewPauseRemainingSec = 0.0;
    this->movementPreviewShip.setPositionTileInt(centerTile.x, centerTile.y);

    SDL_Point firstTargetTile{};
    if (!computeTargetFromSimulatedClick(this->movementPreviewShip.getPositionTile(), this->movementPreviewTargets[0], &firstTargetTile))
    {
        this->statusMessage = "Check impossible: calcul du premier clic echoue.";
        return;
    }

    // Pendant le check, on force un zoom camera de reference (100%),
    // puis on restaurera le zoom precedent a la fin/stop du check.
    Camera& camera = GetCamera();
    this->cameraZoomBeforeCheck = camera.getZoomFactor();
    camera.setZoomFactor(1.00f);
    camera.update(map, map.rect);

    this->movementPreviewShip.moveToTile(map, firstTargetTile.x, firstTargetTile.y);
    this->clickMarker.show(firstTargetTile.x, firstTargetTile.y);

    if (!this->movementPreviewShip.isMoving())
    {
        this->statusMessage = "Check impossible: A* ne demarre pas la preview.";
        return;
    }

    this->movementPreviewActive = true;
    this->movementPreviewPauseRemainingSec = kPreviewPauseAfterArrivalSec;
    this->statusMessage =
        "Check deplacement A* [pass 1/2 sprites 1..4] en cours (clic simule: haut -> tuile " +
        std::to_string(firstTargetTile.x) + "," + std::to_string(firstTargetTile.y) + ").";
}

void EditorMapAnchorShipScene::updateMovementPreview(double dt)
{
    if (!this->movementPreviewActive)
    {
        return;
    }

    Map& map = GetCurrentMap();
    this->movementPreviewShip.update(dt, map);
    this->movementPreviewCurrentTile = this->movementPreviewShip.getPositionTile();

    if (this->movementPreviewShip.isMoving())
    {
        return;
    }

    if (this->movementPreviewPauseRemainingSec > 0.0)
    {
        this->movementPreviewPauseRemainingSec =
            (std::max)(0.0, this->movementPreviewPauseRemainingSec - dt);
        return;
    }

    auto computeTargetFromSimulatedClick = [&map](const SDL_FPoint& shipTilePos, const SDL_FPoint& clickDir, SDL_Point* outTargetTile) -> bool {
        if (outTargetTile == nullptr)
        {
            return false;
        }

        const SDL_Point shipTileRounded = map.roundTile(shipTilePos.x, shipTilePos.y);
        SDL_Point targetTileFromDir = shipTileRounded;
        if (clickDir.x > 0.0f && clickDir.y < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.x > 0.0f && clickDir.y > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y};
        }
        else if (clickDir.x < 0.0f && clickDir.y > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }
        else if (clickDir.x < 0.0f && clickDir.y < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y};
        }
        else if (clickDir.x > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.x < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }
        else if (clickDir.y < 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x - kPreviewClickDistanceTiles,
                shipTileRounded.y - kPreviewClickDistanceTiles};
        }
        else if (clickDir.y > 0.0f)
        {
            targetTileFromDir = SDL_Point{
                shipTileRounded.x + kPreviewClickDistanceTiles,
                shipTileRounded.y + kPreviewClickDistanceTiles};
        }

        const SDL_FPoint clickScreen = map.tileToScreenCenter(targetTileFromDir.x, targetTileFromDir.y);
        SDL_Point targetTile = map.screenToTileNearest(clickScreen.x, clickScreen.y);
        if (targetTile.x == shipTileRounded.x && targetTile.y == shipTileRounded.y)
        {
            targetTile = targetTileFromDir;
        }

        *outTargetTile = targetTile;
        return true;
    };

    this->movementPreviewTargetIndex += 1;
    if (this->movementPreviewTargetIndex >= static_cast<int>(this->movementPreviewTargets.size()))
    {
        // Fin pass 1 -> on enchaine directement pass 2 (sprites 5..8).
        if (this->movementPreviewVisualPass == 0)
        {
            this->movementPreviewVisualPass = 1;
            this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::LOW);
            this->movementPreviewTargetIndex = 0;
            this->movementPreviewCurrentTile = this->movementPreviewBaseTile;
            this->movementPreviewShip.setPositionTile(
                this->movementPreviewBaseTile.x,
                this->movementPreviewBaseTile.y);

            SDL_Point firstTargetTile{};
            if (!computeTargetFromSimulatedClick(
                    this->movementPreviewShip.getPositionTile(),
                    this->movementPreviewTargets[0],
                    &firstTargetTile))
            {
                this->movementPreviewActive = false;
                this->restoreCameraZoomAfterCheck();
                this->statusMessage = "Check interrompu: calcul premier clic pass 2 echoue.";
                return;
            }

            if (!map.isInside(firstTargetTile.x, firstTargetTile.y))
            {
                this->movementPreviewActive = false;
                this->restoreCameraZoomAfterCheck();
                this->statusMessage = "Check interrompu: pass 2 cible hors map.";
                return;
            }

            this->movementPreviewShip.moveToTile(map, firstTargetTile.x, firstTargetTile.y);
            if (!this->movementPreviewShip.isMoving())
            {
                this->movementPreviewActive = false;
                this->restoreCameraZoomAfterCheck();
                this->statusMessage = "Check interrompu: A* pass 2 impossible.";
                return;
            }

            this->movementPreviewPauseRemainingSec = kPreviewPauseAfterArrivalSec;
            this->clickMarker.show(firstTargetTile.x, firstTargetTile.y);
            this->statusMessage =
                "Check deplacement A* [pass 2/2 sprites 5..8] en cours (clic simule: haut -> tuile " +
                std::to_string(firstTargetTile.x) + "," + std::to_string(firstTargetTile.y) + ").";
            return;
        }

        this->movementPreviewActive = false;
        this->restoreCameraZoomAfterCheck();
        this->movementPreviewCurrentTile = this->movementPreviewBaseTile;
        this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::FULL);
        this->statusMessage = "Check termine: deplacement A* valide sur sprites 1..8.";
        return;
    }

    const SDL_FPoint clickDir =
        this->movementPreviewTargets[static_cast<size_t>(this->movementPreviewTargetIndex)];
    SDL_Point targetTile{};
    if (!computeTargetFromSimulatedClick(this->movementPreviewShip.getPositionTile(), clickDir, &targetTile))
    {
        this->movementPreviewActive = false;
        this->restoreCameraZoomAfterCheck();
        this->statusMessage = "Check interrompu: calcul cible clic echoue.";
        return;
    }

    if (!map.isInside(targetTile.x, targetTile.y))
    {
        this->movementPreviewActive = false;
        this->restoreCameraZoomAfterCheck();
        this->statusMessage = "Check interrompu: cible hors map.";
        return;
    }

    this->movementPreviewShip.moveToTile(map, targetTile.x, targetTile.y);
    if (!this->movementPreviewShip.isMoving())
    {
        this->movementPreviewActive = false;
        this->restoreCameraZoomAfterCheck();
        this->statusMessage = "Check interrompu: A* impossible sur une etape.";
        return;
    }
    this->movementPreviewPauseRemainingSec = kPreviewPauseAfterArrivalSec;
    this->clickMarker.show(targetTile.x, targetTile.y);

    const char* clickLabel = "clic simule";
    if (clickDir.x > 0.0f && clickDir.y < 0.0f)
    {
        clickLabel = "clic simule: haut-droite";
    }
    else if (clickDir.x > 0.0f && clickDir.y > 0.0f)
    {
        clickLabel = "clic simule: bas-droite";
    }
    else if (clickDir.x < 0.0f && clickDir.y > 0.0f)
    {
        clickLabel = "clic simule: bas-gauche";
    }
    else if (clickDir.x < 0.0f && clickDir.y < 0.0f)
    {
        clickLabel = "clic simule: haut-gauche";
    }
    else if (clickDir.x > 0.0f)
    {
        clickLabel = "clic simule: droite";
    }
    else if (clickDir.x < 0.0f)
    {
        clickLabel = "clic simule: gauche";
    }
    else if (clickDir.y < 0.0f)
    {
        clickLabel = "clic simule: haut";
    }
    else if (clickDir.y > 0.0f)
    {
        clickLabel = "clic simule: bas";
    }
    const char* passLabel =
        (this->movementPreviewVisualPass == 0)
        ? "pass 1/2 sprites 1..4"
        : "pass 2/2 sprites 5..8";
    this->statusMessage =
        std::string("Check deplacement A* [") + passLabel + "] en cours (" + clickLabel +
        " -> tuile " + std::to_string(targetTile.x) + "," + std::to_string(targetTile.y) + ").";
}

void EditorMapAnchorShipScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();

    const float startX = 12.0f;
    const float row1Y = map.rect.y + map.rect.h + 4.0f;
    const float row2Y = row1Y + 30.0f;
    const float h = 24.0f;
    const float gap = 8.0f;

    float x = startX;

    this->buttonImportFolderRect = SDL_FRect{x, row1Y, 260.0f, h};
    x += this->buttonImportFolderRect.w + gap;

    this->buttonPrevSpriteRect = SDL_FRect{x, row1Y, 110.0f, h};
    x += this->buttonPrevSpriteRect.w + gap;

    this->buttonNextSpriteRect = SDL_FRect{x, row1Y, 110.0f, h};
    x += this->buttonNextSpriteRect.w + gap;

    this->buttonToggleGuidesRect = SDL_FRect{x, row1Y, 170.0f, h};

    // Stop check: haut-droite de la zone map.
    this->buttonStopCheckRect = SDL_FRect{
        map.rect.x + map.rect.w - 320.0f - 14.0f,
        map.rect.y + 10.0f,
        320.0f,
        24.0f};

    x = startX;
    this->buttonCheckMovementRect = SDL_FRect{x, row2Y, 190.0f, h};
    x += this->buttonCheckMovementRect.w + gap;

    this->buttonSaveAnchorRect = SDL_FRect{x, row2Y, 240.0f, h};
    x += this->buttonSaveAnchorRect.w + gap;

    this->buttonResetAnchorRect = SDL_FRect{x, row2Y, 170.0f, h};
    x += this->buttonResetAnchorRect.w + gap;

    this->buttonOceanPrevRect = SDL_FRect{x, row2Y, 92.0f, h};
    x += this->buttonOceanPrevRect.w + gap;
    this->buttonOceanNextRect = SDL_FRect{x, row2Y, 92.0f, h};
}

void EditorMapAnchorShipScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
{
    const RC2D_Color fillColor = active ? kButtonFillActiveColor : kButtonFillColor;
    const RC2D_Color borderColor = active ? kButtonBorderActiveColor : kButtonBorderColor;

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

    const float drawX = rect.x + ((rect.w - static_cast<float>(textW)) * 0.5f);
    const float drawY = rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f);
    rc2d_graphics_drawText(&text, drawX, drawY);
    rc2d_graphics_destroyText(&text);
}

bool EditorMapAnchorShipScene::tryBuildCurrentSpriteDrawRect(
    SDL_FRect* outRect,
    float* outAnchorScreenX,
    float* outAnchorScreenY,
    float* outScale) const
{
    if (this->selectedSpriteIndex < 0 || this->selectedSpriteIndex >= kShipSpriteCount)
    {
        return false;
    }

    const ShipFrame& frame = this->shipFrames[static_cast<size_t>(this->selectedSpriteIndex)];
    if (!frame.loaded || frame.image.sdl_texture == nullptr || frame.widthPx <= 0.0f || frame.heightPx <= 0.0f)
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    const float scale = std::clamp(this->spritePreviewZoom, kSpritePreviewZoomMin, kSpritePreviewZoomMax);

    SDL_FPoint spriteCenterScreen{};
    if (this->movementPreviewActive)
    {
        spriteCenterScreen = map.tileToScreenCenterFloat(
            this->movementPreviewCurrentTile.x,
            this->movementPreviewCurrentTile.y);
    }
    else
    {
        const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        spriteCenterScreen = map.tileToScreenCenterFloat(
            static_cast<float>(centerTile.x),
            static_cast<float>(centerTile.y));
    }

    SDL_FPoint anchor = this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)];
    anchor.x = std::clamp(anchor.x, 0.0f, 1.0f);
    anchor.y = std::clamp(anchor.y, 0.0f, 1.0f);

    // Important: utiliser la meme formule que Ship::drawSpriteCentered().
    // Ainsi, le repere de tuile et l'anchor preview correspondent exactement
    // au rendu runtime du navire.
    SDL_FRect drawRect{};
    drawRect.x = spriteCenterScreen.x - ((frame.widthPx * anchor.x) * scale);
    drawRect.y = spriteCenterScreen.y - ((frame.heightPx * anchor.y) * scale);
    drawRect.w = frame.widthPx * scale;
    drawRect.h = frame.heightPx * scale;

    const float anchorScreenX = drawRect.x + (drawRect.w * anchor.x);
    const float anchorScreenY = drawRect.y + (drawRect.h * anchor.y);

    if (outRect != nullptr)
    {
        *outRect = drawRect;
    }
    if (outAnchorScreenX != nullptr)
    {
        *outAnchorScreenX = anchorScreenX;
    }
    if (outAnchorScreenY != nullptr)
    {
        *outAnchorScreenY = anchorScreenY;
    }
    if (outScale != nullptr)
    {
        *outScale = scale;
    }

    return true;
}

void EditorMapAnchorShipScene::drawShipAnchorPreview(void) const
{
    const Map& map = GetCurrentMap();
    SDL_FPoint tileAnchorCenterScreen{};
    if (this->movementPreviewActive)
    {
        tileAnchorCenterScreen = map.tileToScreenCenterFloat(
            this->movementPreviewCurrentTile.x,
            this->movementPreviewCurrentTile.y);
    }
    else
    {
        const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        tileAnchorCenterScreen = map.tileToScreenCenterFloat(
            static_cast<float>(centerTile.x),
            static_cast<float>(centerTile.y));
    }

    auto drawTileAnchorMarker = [&map](const SDL_FPoint& center, bool drawCrosshair, float zoomScale) {
        const float timeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
        const float pulse01 = 0.5f + (0.5f * std::sin(timeSeconds * 6.0f));

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        const Uint8 fillAlpha = static_cast<Uint8>(std::clamp(35.0f + (pulse01 * 80.0f), 0.0f, 255.0f));
        rc2d_graphics_setColor(RC2D_Color{90, 230, 255, fillAlpha});

        const float zoom = (std::max)(zoomScale, 0.01f);
        const float arm = (std::max)(map.getTileHeight() * 0.30f * zoom, 2.0f);
        const float markerW = (std::max)(map.getTileWidth() * 0.28f * zoom, 2.0f);
        const float markerH = (std::max)(map.getTileHeight() * 0.28f * zoom, 2.0f);
        rc2d_graphics_drawTileIsometric("fill", center.x, center.y, markerW, markerH);

        rc2d_graphics_setColor(RC2D_Color{90, 230, 255, 245});
        if (drawCrosshair)
        {
            rc2d_graphics_line(center.x - arm, center.y, center.x + arm, center.y);
            rc2d_graphics_line(center.x, center.y - arm, center.x, center.y + arm);
        }
        rc2d_graphics_drawTileIsometric("line", center.x, center.y, markerW, markerH);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    };

    if (this->movementPreviewActive && this->movementPreviewShip.areSpritesLoaded())
    {
        // Pendant le check, on dessine le vrai Ship pour avoir le meme rendu
        // de deplacement que le player (directions/sprites alternes).
        this->movementPreviewShip.draw(GetCurrentMap());
        drawTileAnchorMarker(tileAnchorCenterScreen, this->showAnchorGuides, 1.0f);
        return;
    }

    SDL_FRect drawRect{};
    float anchorScreenX = 0.0f;
    float anchorScreenY = 0.0f;
    float scale = 1.0f;
    if (!this->tryBuildCurrentSpriteDrawRect(&drawRect, &anchorScreenX, &anchorScreenY, &scale))
    {
        return;
    }

    const ShipFrame& frame = this->shipFrames[static_cast<size_t>(this->selectedSpriteIndex)];
    const RC2D_Quad sourceQuad = rc2d_graphics_newQuad(
        const_cast<RC2D_Image*>(&frame.image),
        0.0f,
        0.0f,
        frame.widthPx,
        frame.heightPx);

    Uint8 previousAlpha = 255;
    SDL_GetTextureAlphaMod(frame.image.sdl_texture, &previousAlpha);
    SDL_SetTextureAlphaMod(frame.image.sdl_texture, kAnchorPlacementSpriteAlpha);
    rc2d_graphics_drawQuad(
        const_cast<RC2D_Image*>(&frame.image),
        &sourceQuad,
        drawRect.x,
        drawRect.y,
        0.0,
        scale,
        scale,
        -1.0f,
        -1.0f,
        false,
        false);
    SDL_SetTextureAlphaMod(frame.image.sdl_texture, previousAlpha);

    // La tuile d'ancrage clignotante reste toujours visible pour faciliter
    // le placement precis, meme quand les autres guides sont masques.
    drawTileAnchorMarker(tileAnchorCenterScreen, this->showAnchorGuides, scale);

    if (this->showAnchorGuides)
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(RC2D_Color{220, 230, 240, 220});
        // Le contour suit exactement la texture affichee.
        const SDL_FRect guideRect = drawRect;
        rc2d_graphics_rectangle("line", &guideRect);

        rc2d_graphics_setColor(RC2D_Color{255, 90, 90, 245});
        const float anchorArm = (std::max)(9.0f * scale, 2.0f);
        const float anchorDotSize = (std::max)(4.0f * scale, 2.0f);
        rc2d_graphics_line(anchorScreenX - anchorArm, anchorScreenY, anchorScreenX + anchorArm, anchorScreenY);
        rc2d_graphics_line(anchorScreenX, anchorScreenY - anchorArm, anchorScreenX, anchorScreenY + anchorArm);
        SDL_FRect anchorDot = SDL_FRect{
            anchorScreenX - (anchorDotSize * 0.5f),
            anchorScreenY - (anchorDotSize * 0.5f),
            anchorDotSize,
            anchorDotSize};
        rc2d_graphics_rectangle("fill", &anchorDot);

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }
}

void EditorMapAnchorShipScene::drawHud(void) const
{
    if (this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    this->drawToolbarButton(this->buttonImportFolderRect, "IMPORTER DOSSIER NAVIRE", false);
    this->drawToolbarButton(this->buttonPrevSpriteRect, "SPRITE -", false);
    this->drawToolbarButton(this->buttonNextSpriteRect, "SPRITE +", false);
    this->drawToolbarButton(
        this->buttonToggleGuidesRect,
        this->showAnchorGuides ? "MASQUER GUIDES" : "AFFICHER GUIDES",
        this->showAnchorGuides);
    this->drawToolbarButton(this->buttonCheckMovementRect, "CHECK DEPLACEMENT", this->movementPreviewActive);
    this->drawToolbarButton(this->buttonStopCheckRect, "STOP CHECK DEPLACEMENT", this->movementPreviewActive);
    this->drawToolbarButton(this->buttonSaveAnchorRect, "EXPORT SHIP_ANCHOR", false);
    this->drawToolbarButton(this->buttonResetAnchorRect, "RESET ANCRE", false);
    this->drawToolbarButton(this->buttonOceanPrevRect, "OCEAN -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "OCEAN +", false);

    auto drawLine = [this](const char* text, float x, float y, RC2D_Color color) {
        RC2D_Text renderedText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);
        rc2d_graphics_drawText(&renderedText, x, y);
        rc2d_graphics_destroyText(&renderedText);
    };
    auto drawLineRight = [this](const char* text, float rightX, float y, RC2D_Color color) {
        RC2D_Text renderedText = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);
        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);
        const float drawX = rightX - static_cast<float>(textWidth);
        rc2d_graphics_drawText(&renderedText, drawX, y);
        rc2d_graphics_destroyText(&renderedText);
    };

    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const SDL_FRect mapRect = GetCurrentMap().rect;
    const SDL_FPoint anchor = this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)];
    const char* oceanLabel = kOceanColors[static_cast<size_t>(this->selectedOceanColorIndex)].label;
    const char* guidesLabel = this->showAnchorGuides ? "ON" : "OFF";

    char line0[1024] = {};
    SDL_snprintf(
        line0,
        sizeof(line0),
        "EDITOR ANCHOR SHIP");

    const std::string folderInfo = this->loadedShipFolderAbsolute.empty()
        ? std::string("Dossier: aucun")
        : std::string("Dossier: ") + shortenMiddle(this->loadedShipFolderAbsolute, 110);
    char line1[1024] = {};
    SDL_snprintf(line1, sizeof(line1), "%s", folderInfo.c_str());

    char line2[1024] = {};
    SDL_snprintf(
        line2,
        sizeof(line2),
        "Sprite %d/8 | Anchor=(%.4f, %.4f) | Ocean=%s | Guides=%s | Zoom=%.2fx | Fleches=decalage texture | Clique=point d'ancre",
        this->selectedSpriteIndex + 1,
        anchor.x,
        anchor.y,
        oceanLabel,
        guidesLabel,
        this->spritePreviewZoom);

    // Titre en haut a gauche.
    drawLine(line0, gameScreenRect.x + 14.0f, gameScreenRect.y + 5.0f, kHudTextColor);

    // Ligne dossier en haut a droite.
    const float rightPadding = 14.0f;
    const float rightX = gameScreenRect.x + gameScreenRect.w - rightPadding;
    drawLineRight(line1, rightX, gameScreenRect.y + 5.0f, kHudTextColor);

    // Infos principales en bas a droite.
    const float infoBaseY = mapRect.y + mapRect.h + 6.0f;
    drawLineRight(line2, rightX, infoBaseY, kHudTextColor);
}

bool EditorMapAnchorShipScene::handleToolbarClick(float x, float y)
{
    if (this->pointInRect(x, y, this->buttonStopCheckRect))
    {
        if (this->movementPreviewActive)
        {
            this->movementPreviewActive = false;
            this->movementPreviewTargetIndex = 0;
            this->movementPreviewVisualPass = 0;
            this->movementPreviewPauseRemainingSec = 0.0;
            this->movementPreviewShip.setHealthVisual(Ship::HealthVisual::FULL);
            this->movementPreviewCurrentTile = this->movementPreviewShip.getPositionTile();
            this->movementPreviewShip.setPositionTile(
                this->movementPreviewCurrentTile.x,
                this->movementPreviewCurrentTile.y);
            this->clickMarker.hide();
            this->restoreCameraZoomAfterCheck();
            this->statusMessage = "Check deplacement arrete manuellement.";
        }
        else
        {
            this->statusMessage = "Aucun check deplacement en cours.";
        }
        return true;
    }

    if (this->pointInRect(x, y, this->buttonImportFolderRect))
    {
        this->openShipFolderDialog();
        return true;
    }

    if (this->pointInRect(x, y, this->buttonPrevSpriteRect))
    {
        this->selectedSpriteIndex -= 1;
        if (this->selectedSpriteIndex < 0)
        {
            this->selectedSpriteIndex = kShipSpriteCount - 1;
        }
        return true;
    }

    if (this->pointInRect(x, y, this->buttonNextSpriteRect))
    {
        this->selectedSpriteIndex += 1;
        if (this->selectedSpriteIndex >= kShipSpriteCount)
        {
            this->selectedSpriteIndex = 0;
        }
        return true;
    }

    if (this->pointInRect(x, y, this->buttonToggleGuidesRect))
    {
        this->showAnchorGuides = !this->showAnchorGuides;
        this->statusMessage = this->showAnchorGuides
            ? "Guides anchor affiches."
            : "Guides anchor masques.";
        return true;
    }

    if (this->pointInRect(x, y, this->buttonCheckMovementRect))
    {
        this->startMovementPreviewCheck();
        return true;
    }

    if (this->pointInRect(x, y, this->buttonSaveAnchorRect))
    {
        this->saveAnchorsToJson();
        return true;
    }

    if (this->pointInRect(x, y, this->buttonResetAnchorRect))
    {
        this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)] = SDL_FPoint{0.5f, 0.5f};
        this->movementPreviewShip.setDrawAnchorForSprite(this->selectedSpriteIndex, 0.5f, 0.5f);
        this->statusMessage =
            "Ancre reset sprite " + std::to_string(this->selectedSpriteIndex + 1) + ".";
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

    return false;
}

bool EditorMapAnchorShipScene::setCurrentAnchorFromClick(float x, float y)
{
    SDL_FRect drawRect{};
    if (!this->tryBuildCurrentSpriteDrawRect(&drawRect, nullptr, nullptr, nullptr))
    {
        return false;
    }

    if (!this->pointInRect(x, y, drawRect))
    {
        return false;
    }

    const float nx = std::clamp((x - drawRect.x) / (std::max)(drawRect.w, 1.0f), 0.0f, 1.0f);
    const float ny = std::clamp((y - drawRect.y) / (std::max)(drawRect.h, 1.0f), 0.0f, 1.0f);
    this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)] = SDL_FPoint{nx, ny};
    this->movementPreviewShip.setDrawAnchorForSprite(this->selectedSpriteIndex, nx, ny);

    char status[256] = {};
    SDL_snprintf(
        status,
        sizeof(status),
        "Sprite %d ancre mise a jour: (%.4f, %.4f).",
        this->selectedSpriteIndex + 1,
        nx,
        ny);
    this->statusMessage = status;
    return true;
}

bool EditorMapAnchorShipScene::pointInRect(float x, float y, const SDL_FRect& rect) const
{
    return (
        x >= rect.x &&
        y >= rect.y &&
        x <= (rect.x + rect.w) &&
        y <= (rect.y + rect.h));
}

void EditorMapAnchorShipScene::onOpenShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;

    EditorMapAnchorShipScene* scene = static_cast<EditorMapAnchorShipScene*>(userdata);
    if (scene == nullptr || scene != EditorMapAnchorShipScene::activeInstance)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingFolderMutex);
    scene->pendingFolderDialogCompleted = true;
    scene->pendingFolderAbsolute.clear();

    if (filelist == nullptr || filelist[0] == nullptr)
    {
        scene->pendingFolderDialogCanceled = true;
        return;
    }

    scene->pendingFolderDialogCanceled = false;
    scene->pendingFolderAbsolute = filelist[0];
}

void EditorMapAnchorShipScene::unload(void)
{
    if (EditorMapAnchorShipScene::activeInstance == this)
    {
        EditorMapAnchorShipScene::activeInstance = nullptr;
    }

    GetOceanShader().unload();
    this->clickMarker.hide();
    this->movementPreviewShip.unloadSprites();
    this->unloadShipFrames();
    rc2d_graphics_closeFont(&this->overlayFont);
    rc2d_graphics_freeImage(&this->backgroundUiImage);

    RC2D_log(RC2D_LOG_INFO, "EditorMapAnchorShipScene: unloaded");
}

void EditorMapAnchorShipScene::load(void)
{
    EditorMapAnchorShipScene::activeInstance = this;
    this->unloadShipFrames();
    this->resetEditorState();
    this->ensureUserStorageFolders();

    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/background.png",
        RC2D_STORAGE_TITLE);

    this->overlayFont = rc2d_graphics_openFontFromStorage(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    map.update();
    this->updateToolbarLayout();

    camera.setZoomFactor(0.60f);
    this->cameraZoomBeforeCheck = camera.getZoomFactor();
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    this->movementPreviewBaseTile = SDL_FPoint{
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y)};
    this->movementPreviewCurrentTile = this->movementPreviewBaseTile;
    this->movementPreviewShip.setPositionTileInt(centerTile.x, centerTile.y);
    this->movementPreviewShip.setSpeedTilesPerSecond(4.0f);

    this->applySelectedOceanColor();

    if (GetOceanShader().isReady())
    {
        this->statusMessage = "Editor anchor navire charge. Importe un dossier navire.";
    }

    RC2D_log(RC2D_LOG_INFO, "EditorMapAnchorShipScene: loaded");
}

void EditorMapAnchorShipScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    map.update();
    this->updateToolbarLayout();
    this->processPendingFolderRequest();
    this->updateMovementPreview(dt);
    this->clickMarker.update(dt);
    this->applyPendingOceanColorStep();
    GetOceanShader().update(dt);
    camera.update(map, map.rect);
}

void EditorMapAnchorShipScene::draw(void)
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

    this->clickMarker.draw(map);
    this->drawShipAnchorPreview();
    WorldRenderClip::end(renderer);

    this->drawHud();
}

void EditorMapAnchorShipScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)keycode;
    (void)keyboardID;

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        return;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_F5 || scancode == SDL_SCANCODE_I))
    {
        this->openShipFolderDialog();
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_O)
    {
        const bool reverse = ((mod & SDL_KMOD_SHIFT) != 0);
        this->requestOceanColorStep(reverse ? -1 : 1);
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_LEFTBRACKET)
    {
        this->selectedSpriteIndex -= 1;
        if (this->selectedSpriteIndex < 0)
        {
            this->selectedSpriteIndex = kShipSpriteCount - 1;
        }
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_RIGHTBRACKET)
    {
        this->selectedSpriteIndex += 1;
        if (this->selectedSpriteIndex >= kShipSpriteCount)
        {
            this->selectedSpriteIndex = 0;
        }
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_C)
    {
        this->startMovementPreviewCheck();
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_S)
    {
        this->saveAnchorsToJson();
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_R)
    {
        this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)] = SDL_FPoint{0.5f, 0.5f};
        this->movementPreviewShip.setDrawAnchorForSprite(this->selectedSpriteIndex, 0.5f, 0.5f);
        this->statusMessage =
            "Ancre reset sprite " + std::to_string(this->selectedSpriteIndex + 1) + ".";
        return;
    }

    // Pendant le check deplacement, le pave numerique pilote le zoom camera
    // (meme comportement que la camera gameplay: pas 0.05, clamp [0.40..1.00]).
    if (!isrepeat &&
        this->movementPreviewActive &&
        (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_KP_MINUS))
    {
        Camera& camera = GetCamera();
        Map& map = GetCurrentMap();
        const float deltaZoom = (scancode == SDL_SCANCODE_KP_PLUS) ? 0.05f : -0.05f;
        camera.setZoomFactor(camera.getZoomFactor() + deltaZoom);
        camera.update(map, map.rect);

        char status[128] = {};
        SDL_snprintf(status, sizeof(status), "Zoom check deplacement: %.2f", camera.getZoomFactor());
        this->statusMessage = status;
        return;
    }

    // Zoom preview du sprite pour reglage fin.
    if (!isrepeat && (scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS))
    {
        const float zoomStep = ((mod & SDL_KMOD_CTRL) != 0) ? 0.02f : 0.10f;
        this->spritePreviewZoom =
            std::clamp(this->spritePreviewZoom + zoomStep, kSpritePreviewZoomMin, kSpritePreviewZoomMax);

        char status[128] = {};
        SDL_snprintf(status, sizeof(status), "Zoom sprite: %.2fx", this->spritePreviewZoom);
        this->statusMessage = status;
        return;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS))
    {
        const float zoomStep = ((mod & SDL_KMOD_CTRL) != 0) ? 0.02f : 0.10f;
        this->spritePreviewZoom =
            std::clamp(this->spritePreviewZoom - zoomStep, kSpritePreviewZoomMin, kSpritePreviewZoomMax);

        char status[128] = {};
        SDL_snprintf(status, sizeof(status), "Zoom sprite: %.2fx", this->spritePreviewZoom);
        this->statusMessage = status;
        return;
    }

    if (!isrepeat && (scancode == SDL_SCANCODE_0 || scancode == SDL_SCANCODE_KP_0))
    {
        this->spritePreviewZoom = 1.0f;
        this->statusMessage = "Zoom sprite reset a 1.00x";
        return;
    }

    // Reglage fin au clavier:
    // - fleches: deplacement de la texture autour du point d'ancre (centre tuile)
    // - SHIFT: pas plus large
    // - CTRL: pas ultra-fin
    float anchorStep = 0.001f;
    if ((mod & SDL_KMOD_CTRL) != 0)
    {
        anchorStep = 0.00025f;
    }
    else if ((mod & SDL_KMOD_SHIFT) != 0)
    {
        anchorStep = 0.005f;
    }

    float nx = this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)].x;
    float ny = this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)].y;
    bool movedAnchor = false;

    if (scancode == SDL_SCANCODE_LEFT)
    {
        // Texture vers la gauche -> anchor vers la droite ecran (x normalise augmente).
        nx += anchorStep;
        movedAnchor = true;
    }
    else if (scancode == SDL_SCANCODE_RIGHT)
    {
        // Texture vers la droite -> anchor vers la gauche ecran (x normalise diminue).
        nx -= anchorStep;
        movedAnchor = true;
    }
    else if (scancode == SDL_SCANCODE_UP)
    {
        // Texture vers le haut -> anchor vers le bas ecran (y normalise augmente).
        ny += anchorStep;
        movedAnchor = true;
    }
    else if (scancode == SDL_SCANCODE_DOWN)
    {
        // Texture vers le bas -> anchor vers le haut ecran (y normalise diminue).
        ny -= anchorStep;
        movedAnchor = true;
    }

    if (movedAnchor)
    {
        nx = std::clamp(nx, 0.0f, 1.0f);
        ny = std::clamp(ny, 0.0f, 1.0f);
        this->spriteAnchors[static_cast<size_t>(this->selectedSpriteIndex)] = SDL_FPoint{nx, ny};
        this->movementPreviewShip.setDrawAnchorForSprite(this->selectedSpriteIndex, nx, ny);

        char status[256] = {};
        SDL_snprintf(
            status,
            sizeof(status),
            "Decalage texture sprite %d (anchor runtime: %.5f, %.5f) | pas=%.5f",
            this->selectedSpriteIndex + 1,
            nx,
            ny,
            anchorStep);
        this->statusMessage = status;
        return;
    }
}

void EditorMapAnchorShipScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    if (this->handleToolbarClick(x, y))
    {
        return;
    }

    const Map& map = GetCurrentMap();
    if (!this->pointInRect(x, y, map.rect))
    {
        return;
    }

    this->setCurrentAnchorFromClick(x, y);
}

#endif // GAME_ENV_DEV
