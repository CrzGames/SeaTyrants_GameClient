#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-crashtest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <SDL3/SDL_render.h>

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"

struct OceanColorEntry
{
    OceanShader::WaterColor value;
    const char* label;
};

constexpr int kCrashTestShipCount = 500;
constexpr int kRebuildShipStep = 500;
constexpr int kNetworkCommandDistanceTiles = 14;
constexpr int kNetworkCommandFallbackMinDistanceTiles = 8;
constexpr double kPauseAfterArrivalSec = 0.22;
constexpr double kRetargetTickPeriodSec = 0.08;
constexpr float kDesiredMovingRatio = 0.90f;
constexpr int kRetargetBudgetPerTickMin = 10;
constexpr int kRetargetBudgetPerTickMax = 64;
constexpr int kMaxMovingShipsHardCap = 3200;

constexpr RC2D_Color kHudTextColor = RC2D_Color{235, 242, 250, 250};
constexpr RC2D_Color kHudStatusColor = RC2D_Color{230, 200, 90, 250};

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

constexpr std::array<SDL_FPoint, 8> kSimulatedClickDirections = {{
    SDL_FPoint{0.0f, -1.0f},  // haut
    SDL_FPoint{-1.0f, -1.0f}, // haut-gauche
    SDL_FPoint{-1.0f, 0.0f},  // gauche
    SDL_FPoint{-1.0f, 1.0f},  // bas-gauche
    SDL_FPoint{0.0f, 1.0f},   // bas
    SDL_FPoint{1.0f, 1.0f},   // bas-droite
    SDL_FPoint{1.0f, 0.0f},   // droite
    SDL_FPoint{1.0f, -1.0f},  // haut-droite
}};

static std::string normalizePathSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static float miniMapNormalizedToSectorCenter(float normalizedValue, int sectorCount)
{
    const float span = static_cast<float>((std::max)(sectorCount, 1));
    const float maxSectorCenter = static_cast<float>((std::max)(sectorCount - 1, 0));
    return std::clamp((normalizedValue * span) - 0.5f, 0.0f, maxSectorCenter);
}

static std::string shortenMiddle(const std::string& text, std::size_t maxLen)
{
    if (text.size() <= maxLen || maxLen < 8)
    {
        return text;
    }

    const std::size_t headLen = maxLen / 2;
    const std::size_t tailLen = maxLen - headLen - 3;
    return text.substr(0, headLen) + "..." + text.substr(text.size() - tailLen);
}

static uint32_t randomNextU32(uint32_t* state)
{
    if (state == nullptr)
    {
        return 0U;
    }

    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}

static float randomNext01(uint32_t* state)
{
    const uint32_t value = randomNextU32(state);
    return static_cast<float>((value >> 8) & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

static int randomIntInclusive(uint32_t* state, int minValue, int maxValue)
{
    if (minValue >= maxValue)
    {
        return minValue;
    }

    const float t = randomNext01(state);
    const float span = static_cast<float>((maxValue - minValue) + 1);
    const int value = minValue + static_cast<int>(std::floor(t * span));
    return std::clamp(value, minValue, maxValue);
}

static SDL_Point simulatedNetworkDirectionToTileOffset(
    const SDL_FPoint& clickDirection,
    int clickDistanceTiles)
{
    if (clickDirection.x > 0.0f && clickDirection.y < 0.0f)
    {
        return SDL_Point{0, -clickDistanceTiles};
    }
    if (clickDirection.x > 0.0f && clickDirection.y > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, 0};
    }
    if (clickDirection.x < 0.0f && clickDirection.y > 0.0f)
    {
        return SDL_Point{0, clickDistanceTiles};
    }
    if (clickDirection.x < 0.0f && clickDirection.y < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, 0};
    }
    if (clickDirection.x > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, -clickDistanceTiles};
    }
    if (clickDirection.x < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, clickDistanceTiles};
    }
    if (clickDirection.y < 0.0f)
    {
        return SDL_Point{-clickDistanceTiles, -clickDistanceTiles};
    }
    if (clickDirection.y > 0.0f)
    {
        return SDL_Point{clickDistanceTiles, clickDistanceTiles};
    }

    return SDL_Point{0, 0};
}

EditorMapCrashTestScene::EditorMapCrashTestScene(void)
    : backgroundUiImage{},
      overlayFont{},
      scrollBarOverlay{},
      renderShipPrototype{},
      renderShipLoaded(false),
      loadedShipFolderPath{},
      simPlayers{},
      retargetCursor(0U),
      retargetTickAccumulatorSec(0.0),
      simulationPaused(false),
      simulationShipCount(kCrashTestShipCount),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      buttonRebuildRect{},
      buttonPauseRect{},
      buttonCenterRect{},
      buttonZoomOutRect{},
      buttonZoomInRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      miniMapRect{},
      miniMapDragActive(false),
      miniMapDragOffsetX(0.0f),
      miniMapDragOffsetY(0.0f),
      statusMessage("Crash-test map pret.")
{
    this->resetSceneState();
}

EditorMapCrashTestScene::~EditorMapCrashTestScene(void)
{
}

void EditorMapCrashTestScene::resetSceneState(void)
{
    this->renderShipLoaded = false;
    this->loadedShipFolderPath.clear();
    this->simPlayers.clear();
    this->retargetCursor = 0U;
    this->retargetTickAccumulatorSec = 0.0;
    this->simulationPaused = false;
    this->simulationShipCount = kCrashTestShipCount;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->miniMapDragActive = false;
    this->miniMapDragOffsetX = 0.0f;
    this->miniMapDragOffsetY = 0.0f;
    this->statusMessage = "Crash-test map pret.";

    this->buttonRebuildRect = SDL_FRect{};
    this->buttonPauseRect = SDL_FRect{};
    this->buttonCenterRect = SDL_FRect{};
    this->buttonZoomOutRect = SDL_FRect{};
    this->buttonZoomInRect = SDL_FRect{};
    this->buttonOceanPrevRect = SDL_FRect{};
    this->buttonOceanNextRect = SDL_FRect{};
    this->miniMapRect = SDL_FRect{};
}

std::vector<std::string> EditorMapCrashTestScene::collectShipFolderCandidates(void) const
{
    std::vector<std::string> candidates;
    candidates.reserve(32);

    auto pushCandidate = [&candidates](const std::string& candidate) {
        const std::string normalized = normalizePathSlashes(candidate);
        if (normalized.empty())
        {
            return;
        }

        if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end())
        {
            candidates.push_back(normalized);
        }
    };

    // Priorite: dossiers explicites utilises pendant les iterations d'editor.
    pushCandidate("assets/images/ships/ship-elite27");
    pushCandidate("assets/images/ships/ship-test");

    std::error_code fsError;
    const std::filesystem::path shipsRoot("assets/images/ships");
    if (!std::filesystem::exists(shipsRoot, fsError) || !std::filesystem::is_directory(shipsRoot, fsError))
    {
        return candidates;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(shipsRoot, fsError))
    {
        if (fsError || !entry.is_directory(fsError))
        {
            continue;
        }

        bool hasAllSprites = true;
        for (int i = 1; i <= 8; ++i)
        {
            const std::filesystem::path spritePath =
                entry.path() / (std::to_string(i) + ".png");
            if (!std::filesystem::exists(spritePath, fsError))
            {
                hasAllSprites = false;
                break;
            }
        }

        if (!hasAllSprites)
        {
            continue;
        }

        pushCandidate(entry.path().generic_string());
    }

    return candidates;
}

bool EditorMapCrashTestScene::loadRenderShipPrototype(void)
{
    this->renderShipPrototype.unloadSprites();
    this->renderShipLoaded = false;
    this->loadedShipFolderPath.clear();

    const std::vector<std::string> candidates = this->collectShipFolderCandidates();
    for (const std::string& folderPath : candidates)
    {
        if (!this->renderShipPrototype.loadSpritesFromFolder(folderPath.c_str()))
        {
            continue;
        }

        this->renderShipLoaded = true;
        this->loadedShipFolderPath = folderPath;
        this->statusMessage = "Ship prototype charge: " + folderPath;
        return true;
    }

    this->statusMessage = "Aucun dossier navire valide (1.png..8.png) dans assets/images/ships.";
    return false;
}

void EditorMapCrashTestScene::rebuildSimulationShips(int shipCount)
{
    Map& map = GetCurrentMap();
    const int clampedCount = (std::max)(shipCount, 1);
    this->simulationShipCount = clampedCount;

    this->simPlayers.clear();
    this->simPlayers.reserve(static_cast<std::size_t>(clampedCount));
    this->retargetCursor = 0U;
    this->retargetTickAccumulatorSec = 0.0;

    uint32_t rng = 0xC0FFEE42U;

    for (int i = 0; i < clampedCount; ++i)
    {
        SimNetworkPlayer player{};
        player.playerId = static_cast<uint32_t>(i + 1);
        Ship& ship = player.ship;

        const float speed = 2.0f + (randomNext01(&rng) * 2.5f);
        ship.setSpeedTilesPerSecond(speed);

        const Ship::HealthVisual visual =
            ((i % 7) == 0) ? Ship::HealthVisual::LOW : Ship::HealthVisual::FULL;
        ship.setHealthVisual(visual);

        const int sectorX = randomIntInclusive(&rng, 0, Map::NUM_SECTORS_X - 1);
        const int sectorY = randomIntInclusive(&rng, 0, Map::NUM_SECTORS_Y - 1);
        SDL_Point tile = map.sectorToTile(sectorX, sectorY);
        tile.x += randomIntInclusive(&rng, -2, 2);
        tile.y += randomIntInclusive(&rng, -2, 2);
        tile = map.clampTile(tile.x, tile.y);
        ship.setPositionTileInt(tile.x, tile.y);

        SimShipState state{};
        state.nextCommandDirectionIndex = randomIntInclusive(&rng, 0, static_cast<int>(kSimulatedClickDirections.size()) - 1);
        state.pauseBeforeNextCommandSec = randomNext01(&rng) * 1.0;
        state.commandRngState = randomNextU32(&rng) ^ (player.playerId * 2654435761U);
        state.healthVisual = visual;
        player.shipState = state;
        player.networkCommandCooldownSec = randomNext01(&rng) * 0.18;

        this->simPlayers.push_back(player);
    }

    this->statusMessage =
        "Flotte crash-test regeneree: " + std::to_string(this->simPlayers.size()) + " joueurs/navires.";
}

bool EditorMapCrashTestScene::computeNextNetworkTargetTileForPlayer(
    std::size_t playerIndex,
    SDL_Point* outTargetTile)
{
    if (playerIndex >= this->simPlayers.size() || outTargetTile == nullptr)
    {
        return false;
    }

    Map& map = GetCurrentMap();
    SimNetworkPlayer& player = this->simPlayers[playerIndex];
    SimShipState& state = player.shipState;
    const SDL_FPoint shipPos = player.ship.getPositionTile();
    const SDL_Point shipTileRaw = map.roundTile(shipPos.x, shipPos.y);
    const SDL_Point shipTileRounded = map.clampTile(
        shipTileRaw.x,
        shipTileRaw.y);

    const int directionCount = static_cast<int>(kSimulatedClickDirections.size());
    const int baseDirection = state.nextCommandDirectionIndex % directionCount;
    for (int attempt = 0; attempt < directionCount; ++attempt)
    {
        const int directionIndex = (baseDirection + attempt) % directionCount;
        const SDL_Point offset = simulatedNetworkDirectionToTileOffset(
            kSimulatedClickDirections[static_cast<std::size_t>(directionIndex)],
            kNetworkCommandDistanceTiles);
        if (offset.x == 0 && offset.y == 0)
        {
            continue;
        }

        const SDL_Point candidate = map.clampTile(
            shipTileRounded.x + offset.x,
            shipTileRounded.y + offset.y);
        if ((candidate.x == shipTileRounded.x && candidate.y == shipTileRounded.y) ||
            map.isTileBlocked(candidate.x, candidate.y))
        {
            continue;
        }

        state.nextCommandDirectionIndex = (directionIndex + 1) % directionCount;
        *outTargetTile = candidate;
        return true;
    }

    // Fallback : petites cibles pseudo-aleatoires proches pour simuler
    // un paquet reseau tileX/tileY brut.
    constexpr int kFallbackAttempts = 10;
    for (int attempt = 0; attempt < kFallbackAttempts; ++attempt)
    {
        const int dx = randomIntInclusive(
            &state.commandRngState,
            -kNetworkCommandDistanceTiles,
            kNetworkCommandDistanceTiles);
        const int dy = randomIntInclusive(
            &state.commandRngState,
            -kNetworkCommandDistanceTiles,
            kNetworkCommandDistanceTiles);
        if (dx == 0 && dy == 0)
        {
            continue;
        }

        const int manhattanDistance = std::abs(dx) + std::abs(dy);
        if (manhattanDistance < kNetworkCommandFallbackMinDistanceTiles)
        {
            continue;
        }

        const SDL_Point candidate = map.clampTile(shipTileRounded.x + dx, shipTileRounded.y + dy);
        if ((candidate.x == shipTileRounded.x && candidate.y == shipTileRounded.y) ||
            map.isTileBlocked(candidate.x, candidate.y))
        {
            continue;
        }

        *outTargetTile = candidate;
        return true;
    }

    return false;
}

bool EditorMapCrashTestScene::issueNextSimulatedNetworkMoveForPlayer(std::size_t playerIndex)
{
    if (playerIndex >= this->simPlayers.size())
    {
        return false;
    }

    Map& map = GetCurrentMap();
    SimNetworkPlayer& player = this->simPlayers[playerIndex];
    Ship& ship = player.ship;
    SimShipState& state = player.shipState;

    SDL_Point targetTile{};
    if (!this->computeNextNetworkTargetTileForPlayer(playerIndex, &targetTile))
    {
        player.networkCommandCooldownSec = 0.10;
        return false;
    }

    ship.moveToTile(map, targetTile.x, targetTile.y);
    if (ship.isMoving())
    {
        state.pauseBeforeNextCommandSec = kPauseAfterArrivalSec;
        const int pseudoLatencyMs = randomIntInclusive(&state.commandRngState, 28, 145);
        player.networkCommandCooldownSec = static_cast<double>(pseudoLatencyMs) / 1000.0;
        return true;
    }

    // Echec de pathfinding sur un ordre reseau: on attend un tick court.
    state.pauseBeforeNextCommandSec = 0.0;
    player.networkCommandCooldownSec = 0.08;
    return false;
}

void EditorMapCrashTestScene::updateSimulation(double dt)
{
    if (this->simPlayers.empty())
    {
        return;
    }

    Map& map = GetCurrentMap();

    int movingShips = 0;
    for (std::size_t i = 0; i < this->simPlayers.size(); ++i)
    {
        SimNetworkPlayer& player = this->simPlayers[i];
        Ship& ship = player.ship;
        ship.update(dt, map);
        if (ship.isMoving())
        {
            movingShips += 1;
        }

        SimShipState& state = player.shipState;
        if (!ship.isMoving() && state.pauseBeforeNextCommandSec > 0.0)
        {
            state.pauseBeforeNextCommandSec =
                (std::max)(0.0, state.pauseBeforeNextCommandSec - dt);
        }

        if (player.networkCommandCooldownSec > 0.0)
        {
            player.networkCommandCooldownSec =
                (std::max)(0.0, player.networkCommandCooldownSec - dt);
        }
    }

    // Limite le cout A* en le faisant tourner par tick fixe.
    this->retargetTickAccumulatorSec += dt;
    if (this->retargetTickAccumulatorSec < kRetargetTickPeriodSec)
    {
        return;
    }
    this->retargetTickAccumulatorSec =
        std::fmod(this->retargetTickAccumulatorSec, kRetargetTickPeriodSec);

    int desiredMovingShips = static_cast<int>(
        std::ceil(static_cast<double>(this->simPlayers.size()) * static_cast<double>(kDesiredMovingRatio)));
    desiredMovingShips = (std::max)(desiredMovingShips, 1);
    desiredMovingShips = (std::min)(desiredMovingShips, static_cast<int>(this->simPlayers.size()));
    desiredMovingShips = (std::min)(desiredMovingShips, kMaxMovingShipsHardCap);

    const int missingMovingShips = (std::max)(0, desiredMovingShips - movingShips);
    const int retargetBudgetThisTick = std::clamp(
        missingMovingShips,
        kRetargetBudgetPerTickMin,
        kRetargetBudgetPerTickMax);

    int retargetedThisTick = 0;
    std::size_t scanned = 0U;
    while (scanned < this->simPlayers.size() &&
           retargetedThisTick < retargetBudgetThisTick &&
           movingShips < desiredMovingShips)
    {
        const std::size_t playerIndex = (this->retargetCursor + scanned) % this->simPlayers.size();
        scanned += 1U;

        if (this->simPlayers[playerIndex].ship.isMoving())
        {
            continue;
        }

        if (this->simPlayers[playerIndex].shipState.pauseBeforeNextCommandSec > 0.0)
        {
            continue;
        }

        if (this->simPlayers[playerIndex].networkCommandCooldownSec > 0.0)
        {
            continue;
        }

        if (this->issueNextSimulatedNetworkMoveForPlayer(playerIndex))
        {
            retargetedThisTick += 1;
            movingShips += 1;
        }
    }

    this->retargetCursor =
        (this->retargetCursor + scanned) % this->simPlayers.size();
}

void EditorMapCrashTestScene::applySelectedOceanColor(void)
{
    if (this->selectedOceanColorIndex < 0)
    {
        this->selectedOceanColorIndex = 0;
    }
    if (this->selectedOceanColorIndex >= static_cast<int>(kOceanColors.size()))
    {
        this->selectedOceanColorIndex = static_cast<int>(kOceanColors.size()) - 1;
    }

    const OceanColorEntry& entry =
        kOceanColors[static_cast<std::size_t>(this->selectedOceanColorIndex)];
    if (!GetOceanShader().load(entry.value))
    {
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapCrashTestScene::requestOceanColorStep(int delta)
{
    if (delta == 0)
    {
        return;
    }

    this->pendingOceanColorDelta += delta;
    this->pendingOceanColorDelta = (std::max)(this->pendingOceanColorDelta, -8);
    this->pendingOceanColorDelta = (std::min)(this->pendingOceanColorDelta, 8);
}

void EditorMapCrashTestScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    const int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapCrashTestScene::cycleOceanColor(int delta)
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

void EditorMapCrashTestScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();

    const float startX = 12.0f;
    const float rowY = map.rect.y + map.rect.h + 4.0f;
    const float h = 24.0f;
    const float gap = 8.0f;
    float x = startX;

    auto setNextButton = [h, gap](SDL_FRect* rect, float* xCursor, float y, float w) {
        if (rect == nullptr || xCursor == nullptr)
        {
            return;
        }
        *rect = SDL_FRect{*xCursor, y, w, h};
        *xCursor += w + gap;
    };

    setNextButton(&this->buttonRebuildRect, &x, rowY, 250.0f);
    setNextButton(&this->buttonPauseRect, &x, rowY, 165.0f);
    setNextButton(&this->buttonCenterRect, &x, rowY, 140.0f);
    setNextButton(&this->buttonZoomOutRect, &x, rowY, 72.0f);
    setNextButton(&this->buttonZoomInRect, &x, rowY, 72.0f);
    setNextButton(&this->buttonOceanPrevRect, &x, rowY, 102.0f);
    setNextButton(&this->buttonOceanNextRect, &x, rowY, 102.0f);

    constexpr float miniMapScale = 0.8f;
    const float miniMapSize = std::clamp(
        map.rect.h * 0.25f * miniMapScale,
        135.0f * miniMapScale,
        240.0f * miniMapScale);
    this->miniMapRect.w = miniMapSize;
    this->miniMapRect.h = miniMapSize;
    this->miniMapRect.x = map.rect.x + map.rect.w - this->miniMapRect.w - 36.0f;
    this->miniMapRect.y = map.rect.y + 36.0f;
}

void EditorMapCrashTestScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
{
    const RC2D_Color fillColor =
        active ? RC2D_Color{95, 145, 190, 210} : RC2D_Color{36, 44, 52, 190};
    const RC2D_Color borderColor =
        active ? RC2D_Color{160, 215, 255, 250} : RC2D_Color{140, 150, 165, 220};
    const RC2D_Color textColor = RC2D_Color{235, 242, 250, 250};

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

    RC2D_Text text =
        rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), label);
    text.color = textColor;
    rc2d_graphics_setTextColor(&text);

    int textW = 0;
    int textH = 0;
    rc2d_graphics_getTextSize(&text, &textW, &textH);
    const float drawX = rect.x + ((rect.w - static_cast<float>(textW)) * 0.5f);
    const float drawY = rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f);
    rc2d_graphics_drawText(&text, drawX, drawY);
    rc2d_graphics_destroyText(&text);
}

void EditorMapCrashTestScene::drawSimulationShips(void)
{
    if (!this->renderShipLoaded || this->simPlayers.empty())
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const float cullPad = 180.0f;
    std::vector<std::size_t> visibleIndices;
    visibleIndices.reserve(this->simPlayers.size());

    for (std::size_t i = 0; i < this->simPlayers.size(); ++i)
    {
        const SDL_FPoint tile = this->simPlayers[i].ship.getPositionTile();
        const SDL_FPoint screen = map.tileToScreenCenterFloat(tile.x, tile.y);
        if (screen.x < (map.rect.x - cullPad) || screen.x > (map.rect.x + map.rect.w + cullPad) ||
            screen.y < (map.rect.y - cullPad) || screen.y > (map.rect.y + map.rect.h + cullPad))
        {
            continue;
        }

        visibleIndices.push_back(i);
    }

    std::sort(visibleIndices.begin(), visibleIndices.end(), [this](std::size_t a, std::size_t b) {
        const SDL_FPoint pa = this->simPlayers[a].ship.getPositionTile();
        const SDL_FPoint pb = this->simPlayers[b].ship.getPositionTile();
        const float depthA = pa.x + pa.y;
        const float depthB = pb.x + pb.y;
        if (std::fabs(depthA - depthB) > 0.0001f)
        {
            return depthA < depthB;
        }
        return pa.y < pb.y;
    });

    for (std::size_t index : visibleIndices)
    {
        const Ship& simShip = this->simPlayers[index].ship;
        const SimShipState& simState = this->simPlayers[index].shipState;
        const SDL_FPoint pos = simShip.getPositionTile();

        this->renderShipPrototype.setPositionTile(pos.x, pos.y);
        this->renderShipPrototype.setPreviewDirection(simShip.getCurrentPreviewDirection());
        this->renderShipPrototype.setHealthVisual(simState.healthVisual);
        this->renderShipPrototype.draw(map);
    }
}

bool EditorMapCrashTestScene::tryBuildMiniMapViewRect(SDL_FRect* outRect) const
{
    if (outRect == nullptr)
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    const float sectorSpanX = static_cast<float>((std::max)(Map::NUM_SECTORS_X, 1));
    const float sectorSpanY = static_cast<float>((std::max)(Map::NUM_SECTORS_Y, 1));
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return false;
    }

    const float viewCenterScreenX = map.rect.x + (map.rect.w * 0.5f);
    const float viewCenterScreenY = map.rect.y + (map.rect.h * 0.5f);
    const SDL_FPoint centerTile = map.screenToTile(viewCenterScreenX, viewCenterScreenY);
    const SDL_FPoint centerSector = map.tileToSectorFloat(centerTile.x, centerTile.y);

    const float halfViewU = (map.rect.w * 0.5f) / halfTileW;
    const float halfViewV = (map.rect.h * 0.5f) / halfTileH;
    const float halfSectorX = halfViewU / (2.0f * static_cast<float>(Map::SECTOR_STEP));
    const float halfSectorY = halfViewV / (2.0f * static_cast<float>(Map::SECTOR_STEP));

    const float minSectorCenterX = centerSector.x - halfSectorX;
    const float maxSectorCenterX = centerSector.x + halfSectorX;
    const float minSectorCenterY = centerSector.y - halfSectorY;
    const float maxSectorCenterY = centerSector.y + halfSectorY;

    float minSectorEdgeX = std::clamp(minSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float maxSectorEdgeX = std::clamp(maxSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float minSectorEdgeY = std::clamp(minSectorCenterY + 0.5f, 0.0f, sectorSpanY);
    float maxSectorEdgeY = std::clamp(maxSectorCenterY + 0.5f, 0.0f, sectorSpanY);

    if (maxSectorEdgeX < minSectorEdgeX)
    {
        std::swap(minSectorEdgeX, maxSectorEdgeX);
    }
    if (maxSectorEdgeY < minSectorEdgeY)
    {
        std::swap(minSectorEdgeY, maxSectorEdgeY);
    }

    SDL_FRect viewRect{};
    viewRect.x = this->miniMapRect.x + ((minSectorEdgeX / sectorSpanX) * this->miniMapRect.w);
    viewRect.y = this->miniMapRect.y + ((minSectorEdgeY / sectorSpanY) * this->miniMapRect.h);
    viewRect.w = ((maxSectorEdgeX - minSectorEdgeX) / sectorSpanX) * this->miniMapRect.w;
    viewRect.h = ((maxSectorEdgeY - minSectorEdgeY) / sectorSpanY) * this->miniMapRect.h;
    viewRect.w = (std::max)(viewRect.w, 2.0f);
    viewRect.h = (std::max)(viewRect.h, 2.0f);
    viewRect.x = std::clamp(viewRect.x, this->miniMapRect.x, this->miniMapRect.x + this->miniMapRect.w - viewRect.w);
    viewRect.y = std::clamp(viewRect.y, this->miniMapRect.y, this->miniMapRect.y + this->miniMapRect.h - viewRect.h);

    *outRect = viewRect;
    return true;
}

void EditorMapCrashTestScene::moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    float localX = miniMapX - this->miniMapRect.x;
    float localY = miniMapY - this->miniMapRect.y;
    if (applyDragOffset)
    {
        localX -= this->miniMapDragOffsetX;
        localY -= this->miniMapDragOffsetY;
    }

    const float nx =
        std::clamp(localX / (std::max)(this->miniMapRect.w, 1.0f), 0.0f, 1.0f);
    const float ny =
        std::clamp(localY / (std::max)(this->miniMapRect.h, 1.0f), 0.0f, 1.0f);

    const float targetSectorX = miniMapNormalizedToSectorCenter(nx, Map::NUM_SECTORS_X);
    const float targetSectorY = miniMapNormalizedToSectorCenter(ny, Map::NUM_SECTORS_Y);
    const float targetTileX =
        static_cast<float>(Map::SECTOR_BASE_X) +
        ((targetSectorX + targetSectorY) * static_cast<float>(Map::SECTOR_STEP));
    const float targetTileY =
        static_cast<float>(Map::SECTOR_BASE_Y) +
        ((targetSectorY - targetSectorX) * static_cast<float>(Map::SECTOR_STEP));

    camera.centerCameraOnTile(targetTileX, targetTileY, map, map.rect);
    camera.update(map, map.rect);
}

bool EditorMapCrashTestScene::handleMiniMapClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->pointInRect(x, y, this->miniMapRect))
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect) && this->pointInRect(x, y, viewRect))
    {
        const float viewCenterX = viewRect.x + (viewRect.w * 0.5f);
        const float viewCenterY = viewRect.y + (viewRect.h * 0.5f);
        this->miniMapDragOffsetX = x - viewCenterX;
        this->miniMapDragOffsetY = y - viewCenterY;
    }
    else
    {
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        this->moveCameraFromMiniMapPoint(x, y, true);
    }

    this->miniMapDragActive = true;
    return true;
}

void EditorMapCrashTestScene::handleMiniMapDragFromMouse(void)
{
    if (!this->miniMapDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->miniMapDragActive = false;
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    this->moveCameraFromMiniMapPoint(mouseX, mouseY, true);
}

void EditorMapCrashTestScene::drawMiniMap(void) const
{
    const float left = this->miniMapRect.x;
    const float top = this->miniMapRect.y;
    const float width = this->miniMapRect.w;
    const float height = this->miniMapRect.h;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{16, 22, 30, 225});
    rc2d_graphics_rectangle("fill", &this->miniMapRect);

    // Grille secteur minimap (pas de 10).
    rc2d_graphics_setColor(RC2D_Color{50, 74, 98, 140});
    for (int i = 1; i < Map::NUM_SECTORS_X; ++i)
    {
        if ((i % 10) != 0)
        {
            continue;
        }

        const float nx = static_cast<float>(i) / static_cast<float>(Map::NUM_SECTORS_X);
        const float x = left + (nx * width);
        rc2d_graphics_line(x, top, x, top + height);
    }
    for (int i = 1; i < Map::NUM_SECTORS_Y; ++i)
    {
        if ((i % 10) != 0)
        {
            continue;
        }

        const float ny = static_cast<float>(i) / static_cast<float>(Map::NUM_SECTORS_Y);
        const float y = top + (ny * height);
        rc2d_graphics_line(left, y, left + width, y);
    }

    rc2d_graphics_setColor(RC2D_Color{135, 150, 168, 235});
    rc2d_graphics_rectangle("line", &this->miniMapRect);

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect))
    {
        rc2d_graphics_setColor(RC2D_Color{125, 198, 255, 55});
        rc2d_graphics_rectangle("fill", &viewRect);
        rc2d_graphics_setColor(RC2D_Color{170, 222, 255, 245});
        rc2d_graphics_rectangle("line", &viewRect);
    }

    const Map& map = GetCurrentMap();
    for (std::size_t i = 0; i < this->simPlayers.size(); ++i)
    {
        const SDL_FPoint tile = this->simPlayers[i].ship.getPositionTile();
        const SDL_FPoint sector = map.tileToSectorFloat(tile.x, tile.y);
        const float nx = (sector.x + 0.5f) / static_cast<float>(Map::NUM_SECTORS_X);
        const float ny = (sector.y + 0.5f) / static_cast<float>(Map::NUM_SECTORS_Y);
        if (!std::isfinite(nx) || !std::isfinite(ny) || nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f)
        {
            continue;
        }

        SDL_FRect dot{};
        dot.w = 1.0f;
        dot.h = 1.0f;
        dot.x = left + (nx * width) - (dot.w * 0.5f);
        dot.y = top + (ny * height) - (dot.h * 0.5f);
        rc2d_graphics_setColor(RC2D_Color{255, 45, 45, 255});
        rc2d_graphics_rectangle("fill", &dot);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCrashTestScene::drawHud(void) const
{
    if (this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    this->drawToolbarButton(this->buttonRebuildRect, "REBUILD 500 NAVIRES", false);
    this->drawToolbarButton(
        this->buttonPauseRect,
        this->simulationPaused ? "RESUME SIMULATION" : "PAUSE SIMULATION",
        this->simulationPaused);
    this->drawToolbarButton(this->buttonCenterRect, "CENTRER MAP", false);
    this->drawToolbarButton(this->buttonZoomOutRect, "ZOOM -", false);
    this->drawToolbarButton(this->buttonZoomInRect, "ZOOM +", false);
    this->drawToolbarButton(this->buttonOceanPrevRect, "OCEAN -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "OCEAN +", false);

    auto drawLine = [this](const char* text, float x, float y, RC2D_Color color) {
        RC2D_Text rendered =
            rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
        rendered.color = color;
        rc2d_graphics_setTextColor(&rendered);
        rc2d_graphics_drawText(&rendered, x, y);
        rc2d_graphics_destroyText(&rendered);
    };

    int movingCount = 0;
    for (const SimNetworkPlayer& player : this->simPlayers)
    {
        if (player.ship.isMoving())
        {
            movingCount += 1;
        }
    }
    const int totalPlayers = static_cast<int>(this->simPlayers.size());
    const int targetMovingCount = (std::max)(
        1,
        static_cast<int>(std::ceil(static_cast<double>(totalPlayers) * static_cast<double>(kDesiredMovingRatio))));
    const float movingPercent =
        (totalPlayers > 0)
            ? (static_cast<float>(movingCount) * 100.0f / static_cast<float>(totalPlayers))
            : 0.0f;

    const SDL_FRect gameRect = GetGameScreen().rect;
    const float zoom = GetCamera().getZoomFactor();
    const char* oceanLabel =
        kOceanColors[static_cast<std::size_t>(this->selectedOceanColorIndex)].label;

    std::string folderLabel = this->loadedShipFolderPath.empty()
        ? std::string("aucun")
        : shortenMiddle(this->loadedShipFolderPath, 84);

    char line0[1024] = {};
    char line1[1024] = {};
    char line2[1024] = {};
    char line3[1024] = {};

    SDL_snprintf(line0, sizeof(line0), "EDITOR MAP CRASHTEST");
    SDL_snprintf(
        line1,
        sizeof(line1),
        "Joueurs/Navires=%d | Moving=%d/%d (%.0f%%, cible=%.0f%%) | Idle=%d | Pause=%s | Zoom=%.2fx | Ocean=%s",
        totalPlayers,
        movingCount,
        targetMovingCount,
        movingPercent,
        kDesiredMovingRatio * 100.0f,
        totalPlayers - movingCount,
        this->simulationPaused ? "ON" : "OFF",
        zoom,
        oceanLabel);
    SDL_snprintf(
        line2,
        sizeof(line2),
        "Prototype ship: %s",
        folderLabel.c_str());
    SDL_snprintf(
        line3,
        sizeof(line3),
        "Status: %s",
        this->statusMessage.c_str());

    drawLine(line0, gameRect.x + 14.0f, gameRect.y + 5.0f, kHudTextColor);
    drawLine(line1, gameRect.x + 14.0f, gameRect.y + 24.0f, kHudTextColor);
    drawLine(line2, gameRect.x + 14.0f, gameRect.y + 43.0f, kHudTextColor);
    drawLine(line3, gameRect.x + 14.0f, gameRect.y + 62.0f, kHudStatusColor);
}

bool EditorMapCrashTestScene::handleToolbarClick(float x, float y)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    if (this->pointInRect(x, y, this->buttonRebuildRect))
    {
        this->rebuildSimulationShips(this->simulationShipCount + kRebuildShipStep);
        return true;
    }

    if (this->pointInRect(x, y, this->buttonPauseRect))
    {
        this->simulationPaused = !this->simulationPaused;
        this->statusMessage = this->simulationPaused
            ? "Simulation mise en pause."
            : "Simulation reprise.";
        return true;
    }

    if (this->pointInRect(x, y, this->buttonCenterRect))
    {
        const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        camera.centerCameraOnTile(
            static_cast<float>(centerTile.x),
            static_cast<float>(centerTile.y),
            map,
            map.rect);
        camera.update(map, map.rect);
        this->statusMessage = "Camera recadree sur la map.";
        return true;
    }

    if (this->pointInRect(x, y, this->buttonZoomOutRect))
    {
        camera.setZoomFactor(camera.getZoomFactor() - 0.05f);
        camera.update(map, map.rect);
        this->statusMessage = "Zoom: " + std::to_string(camera.getZoomFactor());
        return true;
    }

    if (this->pointInRect(x, y, this->buttonZoomInRect))
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        camera.update(map, map.rect);
        this->statusMessage = "Zoom: " + std::to_string(camera.getZoomFactor());
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

bool EditorMapCrashTestScene::pointInRect(float x, float y, const SDL_FRect& rect) const
{
    return (
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

void EditorMapCrashTestScene::convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const
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

bool EditorMapCrashTestScene::getMouseRenderPosition(float* outX, float* outY) const
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

void EditorMapCrashTestScene::unload(void)
{
    GetOceanShader().unload();
    this->scrollBarOverlay.unload();
    this->renderShipPrototype.unloadSprites();
    this->simPlayers.clear();
    rc2d_graphics_closeFont(&this->overlayFont);
    rc2d_graphics_freeImage(&this->backgroundUiImage);
}

void EditorMapCrashTestScene::load(void)
{
    this->resetSceneState();

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

    camera.setZoomFactor(0.60f);
    const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(centerTile.x),
        static_cast<float>(centerTile.y),
        map,
        map.rect);
    camera.update(map, map.rect);

    this->applySelectedOceanColor();
    this->loadRenderShipPrototype();
    this->rebuildSimulationShips(this->simulationShipCount);

    if (!this->renderShipLoaded)
    {
        this->statusMessage += " (simulation active sans rendu ship)";
    }
}

void EditorMapCrashTestScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    map.update();
    this->updateToolbarLayout();
    this->applyPendingOceanColorStep();
    GetOceanShader().update(dt);

    this->scrollBarOverlay.update(dt, camera, map, map.rect);
    (void)GameplayCameraController::updateKeyboardScroll(dt, camera, map, map.rect);

    if (this->scrollBarOverlay.isInteracting())
    {
        this->miniMapDragActive = false;
    }
    this->handleMiniMapDragFromMouse();

    if (!this->simulationPaused)
    {
        this->updateSimulation(dt);
    }

    camera.update(map, map.rect);
}

void EditorMapCrashTestScene::draw(void)
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

    this->drawSimulationShips();
    this->scrollBarOverlay.draw(map.rect, map);
    WorldRenderClip::end(renderer);

    this->drawMiniMap();
    this->drawHud();
}

void EditorMapCrashTestScene::keypressed(
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

    if (!isrepeat && (scancode == SDL_SCANCODE_R || scancode == SDL_SCANCODE_F5))
    {
        this->rebuildSimulationShips(this->simulationShipCount + kRebuildShipStep);
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_SPACE)
    {
        this->simulationPaused = !this->simulationPaused;
        this->statusMessage = this->simulationPaused
            ? "Simulation mise en pause."
            : "Simulation reprise.";
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_C)
    {
        Map& map = GetCurrentMap();
        Camera& camera = GetCamera();
        const SDL_Point centerTile = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
        camera.centerCameraOnTile(
            static_cast<float>(centerTile.x),
            static_cast<float>(centerTile.y),
            map,
            map.rect);
        camera.update(map, map.rect);
        this->statusMessage = "Camera recadree sur la map.";
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_O)
    {
        const bool reverse = ((mod & SDL_KMOD_SHIFT) != 0);
        this->requestOceanColorStep(reverse ? -1 : 1);
        return;
    }

    if (!isrepeat &&
        (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_EQUALS ||
         scancode == SDL_SCANCODE_KP_MINUS || scancode == SDL_SCANCODE_MINUS))
    {
        Map& map = GetCurrentMap();
        Camera& camera = GetCamera();
        const bool zoomIn = (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_EQUALS);
        camera.setZoomFactor(camera.getZoomFactor() + (zoomIn ? 0.05f : -0.05f));
        camera.update(map, map.rect);
        this->statusMessage = "Zoom: " + std::to_string(camera.getZoomFactor());
        return;
    }
}

void EditorMapCrashTestScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    Map& map = GetCurrentMap();
    const float renderX = x;
    const float renderY = y;

    if (this->handleToolbarClick(renderX, renderY))
    {
        return;
    }

    if (this->handleMiniMapClick(renderX, renderY, button))
    {
        return;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT &&
        this->scrollBarOverlay.handleClick(renderX, renderY, map.rect))
    {
        return;
    }
}

void EditorMapCrashTestScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)direction;
    (void)x;
    (void)y;
    (void)integer_x;
    (void)mouse_x;
    (void)mouse_y;
    (void)mouseID;

    if (integer_y == 0)
    {
        return;
    }

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    camera.setZoomFactor(camera.getZoomFactor() + ((integer_y > 0) ? 0.05f : -0.05f));
    camera.update(map, map.rect);
}

#endif // GAME_ENV_DEV
