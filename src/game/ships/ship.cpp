#include "game/ships/ship.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

#include <cJSON.h>

/**
 * @brief Convertit des coordonnées tile iso vers le repère Sea-like (row/col).
 */
bool tileToSeaRowCol(int tileX, int tileY, int& outRow, int& outCol)
{
    const int s = tileX + tileY;
    const int n = tileX - tileY;
    const int row = s + 1;

    int col = 0;
    if ((row & 1) == 0)
    {
        if ((n & 1) == 0)
        {
            return false;
        }
        col = (n - 1) / 2;
    }
    else
    {
        if ((n & 1) != 0)
        {
            return false;
        }
        col = n / 2;
    }

    outRow = row;
    outCol = col;
    return true;
}

/**
 * @brief Convertit le repère Sea-like (row/col) vers des coordonnées tile iso.
 */
bool seaRowColToTile(int row, int col, int& outTileX, int& outTileY)
{
    const int s = row - 1;
    const int n = (2 * col) + (((row & 1) == 0) ? 1 : 0);

    const int sumX = s + n;
    const int sumY = s - n;
    if ((sumX & 1) != 0 || (sumY & 1) != 0)
    {
        return false;
    }

    outTileX = sumX / 2;
    outTileY = sumY / 2;
    return true;
}

int Ship::directionToIndex(DiagonalDirection direction)
{
    return static_cast<int>(direction);
}

int Ship::spriteIndexForDirection(DiagonalDirection direction) const
{
    const int dirIndex = directionToIndex(direction);
    if (dirIndex < 0 || dirIndex >= 4)
    {
        return -1;
    }

    // FULL: 1..4 => index 0..3, LOW: 5..8 => index 4..7
    return (healthVisual == HealthVisual::LOW) ? (4 + dirIndex) : dirIndex;
}

int Ship::getCurrentSpriteIndex(void) const
{
    if (!spritesLoaded)
    {
        return -1;
    }

    const DiagonalDirection visualDirection =
        (directionUsesPair && directionToggle) ? directionB : directionA;

    const int spriteIndex = spriteIndexForDirection(visualDirection);
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(sprites.size()))
    {
        return -1;
    }

    if (sprites[static_cast<size_t>(spriteIndex)].sdl_texture == nullptr)
    {
        return -1;
    }

    return spriteIndex;
}

Ship::MoveDirection Ship::quantizeScreenDirection(float deltaScreenX, float deltaScreenY)
{
    constexpr float kEpsilon = 0.0001f;
    if (std::fabs(deltaScreenX) <= kEpsilon && std::fabs(deltaScreenY) <= kEpsilon)
    {
        return MoveDirection::NONE;
    }

    const float angle = std::atan2(deltaScreenY, deltaScreenX);
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kStep = kPi / 4.0f;

    int octant = static_cast<int>(std::floor((angle + (kStep * 0.5f)) / kStep));
    octant %= 8;
    if (octant < 0)
    {
        octant += 8;
    }

    switch (octant)
    {
        case 0: return MoveDirection::RIGHT;
        case 1: return MoveDirection::DOWN_RIGHT;
        case 2: return MoveDirection::DOWN;
        case 3: return MoveDirection::DOWN_LEFT;
        case 4: return MoveDirection::LEFT;
        case 5: return MoveDirection::UP_LEFT;
        case 6: return MoveDirection::UP;
        case 7: return MoveDirection::UP_RIGHT;
        default: return MoveDirection::NONE;
    }
}

float Ship::aStarHeuristic(int fromTileX, int fromTileY, int toTileX, int toTileY)
{
    int fromRow = 0;
    int fromCol = 0;
    int toRow = 0;
    int toCol = 0;

    if (tileToSeaRowCol(fromTileX, fromTileY, fromRow, fromCol) &&
        tileToSeaRowCol(toTileX, toTileY, toRow, toCol))
    {
        const float dr = static_cast<float>(fromRow - toRow);
        const float dc = static_cast<float>(fromCol - toCol);
        return std::sqrt((dr * dr) + (dc * dc));
    }

    // Fallback sûr si conversion Sea-like impossible.
    const float dx = static_cast<float>(fromTileX - toTileX);
    const float dy = static_cast<float>(fromTileY - toTileY);
    return std::sqrt((dx * dx) + (dy * dy));
}

bool Ship::isWalkableForPath(
    const Map& map,
    int tileX,
    int tileY,
    const SDL_Point& startTile,
    const SDL_Point& goalTile) const
{
    if (!map.isInside(tileX, tileY))
    {
        return false;
    }

    if ((tileX == startTile.x && tileY == startTile.y) ||
        (tileX == goalTile.x && tileY == goalTile.y))
    {
        return true;
    }

    return !map.isTileBlocked(tileX, tileY);
}

bool Ship::buildPathAStar(
    const Map& map,
    const SDL_Point& startTile,
    const SDL_Point& goalTile,
    std::vector<SDL_Point>& outPath,
    std::vector<MoveDirection>& outDirections) const
{
    outPath.clear();
    outDirections.clear();

    if (!map.isInside(startTile.x, startTile.y) || !map.isInside(goalTile.x, goalTile.y))
    {
        return false;
    }

    if (startTile.x == goalTile.x && startTile.y == goalTile.y)
    {
        return true;
    }

    const int width = map.getWidthTiles();
    const int height = map.getHeightTiles();
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    const int total = width * height;

    auto indexOf = [width](int x, int y) -> int {
        return (y * width) + x;
    };

    auto pointOf = [width](int index) -> SDL_Point {
        SDL_Point p = {};
        p.x = index % width;
        p.y = index / width;
        return p;
    };

    struct NodeState {
        float g;
        float f;
        int parent;
        bool closed;
        MoveDirection dirFromParent;
    };

    struct OpenEntry {
        float f;
        int row;
        int col;
        int index;
    };

    struct OpenCompare {
        bool operator()(const OpenEntry& a, const OpenEntry& b) const
        {
            constexpr float kEpsilon = 0.000001f;
            const float df = a.f - b.f;
            if (std::fabs(df) > kEpsilon)
            {
                return a.f < b.f;
            }

            if (a.row != b.row)
            {
                return a.row < b.row;
            }

            if (a.col != b.col)
            {
                return a.col < b.col;
            }

            return a.index < b.index;
        }
    };

    const float inf = std::numeric_limits<float>::infinity();
    std::vector<NodeState> states(
        static_cast<size_t>(total),
        NodeState{inf, inf, -1, false, MoveDirection::NONE});

    std::set<OpenEntry, OpenCompare> openSet;

    const int startIndex = indexOf(startTile.x, startTile.y);
    const int goalIndex = indexOf(goalTile.x, goalTile.y);

    int startRow = 0;
    int startCol = 0;
    if (!tileToSeaRowCol(startTile.x, startTile.y, startRow, startCol))
    {
        return false;
    }

    states[static_cast<size_t>(startIndex)].g = 0.0f;
    states[static_cast<size_t>(startIndex)].f =
        aStarHeuristic(startTile.x, startTile.y, goalTile.x, goalTile.y);

    openSet.insert(OpenEntry{
        states[static_cast<size_t>(startIndex)].f,
        startRow,
        startCol,
        startIndex});

    // Voisinage 4 du repère Sea-like (parité de row).
    constexpr int kNeighborCount = 4;
    const int oddRowDr[kNeighborCount] = {-1, 1, 1, -1};
    const int oddRowDc[kNeighborCount] = {0, 0, -1, -1};
    const int evenRowDr[kNeighborCount] = {-1, 1, 1, -1};
    const int evenRowDc[kNeighborCount] = {1, 1, 0, 0};

    bool found = false;

    while (!openSet.empty())
    {
        const OpenEntry current = *openSet.begin();
        openSet.erase(openSet.begin());

        if (states[static_cast<size_t>(current.index)].closed)
        {
            continue;
        }

        if (current.index == goalIndex)
        {
            found = true;
            break;
        }

        states[static_cast<size_t>(current.index)].closed = true;

        const SDL_Point currentTile = pointOf(current.index);

        int currentRow = 0;
        int currentCol = 0;
        if (!tileToSeaRowCol(currentTile.x, currentTile.y, currentRow, currentCol))
        {
            continue;
        }

        const bool evenRow = ((currentRow & 1) == 0);

        for (int i = 0; i < kNeighborCount; ++i)
        {
            const int nextRow = currentRow + (evenRow ? evenRowDr[i] : oddRowDr[i]);
            const int nextCol = currentCol + (evenRow ? evenRowDc[i] : oddRowDc[i]);

            int nextX = 0;
            int nextY = 0;
            if (!seaRowColToTile(nextRow, nextCol, nextX, nextY))
            {
                continue;
            }

            if (!isWalkableForPath(map, nextX, nextY, startTile, goalTile))
            {
                continue;
            }

            const int nextIndex = indexOf(nextX, nextY);
            if (states[static_cast<size_t>(nextIndex)].closed)
            {
                continue;
            }

            const float tentativeG = states[static_cast<size_t>(current.index)].g + 1.0f;
            if (tentativeG >= states[static_cast<size_t>(nextIndex)].g)
            {
                continue;
            }

            states[static_cast<size_t>(nextIndex)].parent = current.index;
            states[static_cast<size_t>(nextIndex)].g = tentativeG;
            states[static_cast<size_t>(nextIndex)].f =
                tentativeG + aStarHeuristic(nextX, nextY, goalTile.x, goalTile.y);

            const SDL_FPoint currentCenter = map.tileToScreenCenter(currentTile.x, currentTile.y);
            const SDL_FPoint nextCenter = map.tileToScreenCenter(nextX, nextY);
            states[static_cast<size_t>(nextIndex)].dirFromParent = quantizeScreenDirection(
                nextCenter.x - currentCenter.x,
                nextCenter.y - currentCenter.y);

            openSet.insert(OpenEntry{
                states[static_cast<size_t>(nextIndex)].f,
                nextRow,
                nextCol,
                nextIndex});
        }
    }

    if (!found)
    {
        return false;
    }

    int walk = goalIndex;
    while (walk != startIndex)
    {
        if (walk < 0 || walk >= total)
        {
            outPath.clear();
            outDirections.clear();
            return false;
        }

        outPath.push_back(pointOf(walk));
        outDirections.push_back(states[static_cast<size_t>(walk)].dirFromParent);

        walk = states[static_cast<size_t>(walk)].parent;
        if (walk < 0)
        {
            outPath.clear();
            outDirections.clear();
            return false;
        }
    }

    std::reverse(outPath.begin(), outPath.end());
    std::reverse(outDirections.begin(), outDirections.end());
    return true;
}

void Ship::updateDirection(MoveDirection direction)
{
    if (direction == MoveDirection::NONE)
    {
        return;
    }

    const DiagonalDirection currentVisualDirection =
        (directionUsesPair && directionToggle) ? directionB : directionA;

    auto keepNaturalPairStart = [currentVisualDirection](
                                    DiagonalDirection pairA,
                                    DiagonalDirection pairB,
                                    DiagonalDirection& outA,
                                    DiagonalDirection& outB) {
        if (currentVisualDirection == pairA)
        {
            outA = pairA;
            outB = pairB;
            return;
        }

        if (currentVisualDirection == pairB)
        {
            outA = pairB;
            outB = pairA;
            return;
        }

        outA = pairA;
        outB = pairB;
    };

    DiagonalDirection nextA = directionA;
    DiagonalDirection nextB = directionB;
    bool nextUsesPair = directionUsesPair;

    switch (direction)
    {
        case MoveDirection::RIGHT:
            keepNaturalPairStart(
                DiagonalDirection::UP_RIGHT,
                DiagonalDirection::DOWN_RIGHT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::LEFT:
            keepNaturalPairStart(
                DiagonalDirection::UP_LEFT,
                DiagonalDirection::DOWN_LEFT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::DOWN:
            keepNaturalPairStart(
                DiagonalDirection::DOWN_LEFT,
                DiagonalDirection::DOWN_RIGHT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::UP:
            keepNaturalPairStart(
                DiagonalDirection::UP_LEFT,
                DiagonalDirection::UP_RIGHT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::UP_RIGHT:
            nextA = DiagonalDirection::UP_RIGHT;
            nextB = DiagonalDirection::UP_RIGHT;
            nextUsesPair = false;
            break;

        case MoveDirection::DOWN_RIGHT:
            nextA = DiagonalDirection::DOWN_RIGHT;
            nextB = DiagonalDirection::DOWN_RIGHT;
            nextUsesPair = false;
            break;

        case MoveDirection::DOWN_LEFT:
            nextA = DiagonalDirection::DOWN_LEFT;
            nextB = DiagonalDirection::DOWN_LEFT;
            nextUsesPair = false;
            break;

        case MoveDirection::UP_LEFT:
            nextA = DiagonalDirection::UP_LEFT;
            nextB = DiagonalDirection::UP_LEFT;
            nextUsesPair = false;
            break;

        case MoveDirection::NONE:
        default:
            return;
    }

    const bool visualPairChanged =
        (directionUsesPair != nextUsesPair) ||
        (directionA != nextA) ||
        (directionB != nextB);

    directionUsesPair = nextUsesPair;
    directionA = nextA;
    directionB = nextB;
    moveDirection = direction;

    if (visualPairChanged)
    {
        directionToggle = false;
    }
}

void Ship::drawSpriteCentered(const RC2D_Image& sprite, float centerX, float centerY) const
{
    if (sprite.sdl_texture == nullptr)
    {
        return;
    }

    const float spriteW = static_cast<float>(sprite.sdl_texture->w);
    const float spriteH = static_cast<float>(sprite.sdl_texture->h);

    RC2D_Quad quad = {};
    quad.src = SDL_FRect{0.0f, 0.0f, spriteW, spriteH};

    const float anchorPixelX = spriteW * config.drawAnchorX;
    const float anchorPixelY = spriteH * config.drawAnchorY;

    const float drawX = (centerX + config.drawOffsetX) - (anchorPixelX * config.scaleX);
    const float drawY = (centerY + config.drawOffsetY) - (anchorPixelY * config.scaleY);

    rc2d_graphics_drawQuad(
        (RC2D_Image*)&sprite,
        &quad,
        drawX,
        drawY,
        0.0,
        config.scaleX,
        config.scaleY,
        -1.0f,
        -1.0f,
        false,
        false);
}

bool Ship::loadDrawAnchorFromJson(const char* folderPath, RC2D_StorageKind storageKind)
{
    if (folderPath == nullptr || folderPath[0] == '\0')
    {
        return false;
    }

    char jsonPath[640] = {0};
    void* bytes = nullptr;
    Uint64 len = 0;
    bool readOk = false;

    std::snprintf(jsonPath, sizeof(jsonPath), "%s/%s", folderPath, "ship_anchor.json");

    if (storageKind == RC2D_STORAGE_TITLE)
    {
        readOk = rc2d_storage_titleReadFile(jsonPath, &bytes, &len);
    }
    else if (storageKind == RC2D_STORAGE_USER)
    {
        readOk = rc2d_storage_userReadFile(jsonPath, &bytes, &len);
    }

    if (!readOk || bytes == nullptr || len == 0)
    {
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(static_cast<const char*>(bytes), static_cast<size_t>(len));
    RC2D_free(bytes);
    bytes = nullptr;
    if (root == nullptr)
    {
        return false;
    }

    auto setAnchorIfValid = [this](float anchorX, float anchorY) -> bool {
        if (!std::isfinite(anchorX) || !std::isfinite(anchorY))
        {
            return false;
        }

        setDrawAnchor(anchorX, anchorY);
        return true;
    };

    auto getSprite0Size = [this](float& outW, float& outH) -> bool {
        if (sprites[0].sdl_texture == nullptr)
        {
            return false;
        }

        outW = static_cast<float>(sprites[0].sdl_texture->w);
        outH = static_cast<float>(sprites[0].sdl_texture->h);
        return (outW > 0.0f && outH > 0.0f);
    };

    auto setAnchorFromPixels = [&](float anchorPixelX, float anchorPixelY) -> bool {
        float w = 0.0f;
        float h = 0.0f;
        if (!getSprite0Size(w, h))
        {
            return false;
        }

        return setAnchorIfValid(anchorPixelX / w, anchorPixelY / h);
    };

    bool loaded = false;

    const cJSON* rootAnchorX = cJSON_GetObjectItemCaseSensitive(root, "anchorX");
    const cJSON* rootAnchorY = cJSON_GetObjectItemCaseSensitive(root, "anchorY");
    if (cJSON_IsNumber(rootAnchorX) && cJSON_IsNumber(rootAnchorY))
    {
        loaded = setAnchorIfValid(
            static_cast<float>(rootAnchorX->valuedouble),
            static_cast<float>(rootAnchorY->valuedouble));
    }

    const cJSON* defaultObj = cJSON_GetObjectItemCaseSensitive(root, "default");
    if (!loaded && cJSON_IsObject(defaultObj))
    {
        const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorX");
        const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorY");
        if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
        {
            loaded = setAnchorIfValid(
                static_cast<float>(jAnchorX->valuedouble),
                static_cast<float>(jAnchorY->valuedouble));
        }

        if (!loaded)
        {
            const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelX");
            const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelY");

            if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
            {
                loaded = setAnchorFromPixels(
                    static_cast<float>(jAnchorPixelX->valuedouble),
                    static_cast<float>(jAnchorPixelY->valuedouble));
            }
        }
    }

    const cJSON* framesObj = cJSON_GetObjectItemCaseSensitive(root, "frames");
    if (!loaded && cJSON_IsObject(framesObj))
    {
        const cJSON* frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, "1.png");
        if (!cJSON_IsObject(frameObj))
        {
            frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, "1");
        }

        if (cJSON_IsObject(frameObj))
        {
            const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorX");
            const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorY");
            if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
            {
                loaded = setAnchorIfValid(
                    static_cast<float>(jAnchorX->valuedouble),
                    static_cast<float>(jAnchorY->valuedouble));
            }

            if (!loaded)
            {
                const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelX");
                const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelY");

                if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
                {
                    loaded = setAnchorFromPixels(
                        static_cast<float>(jAnchorPixelX->valuedouble),
                        static_cast<float>(jAnchorPixelY->valuedouble));
                }
            }
        }
    }

    cJSON_Delete(root);

    if (loaded)
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "Ship: ancre chargee depuis '%s' => anchor=(%.4f, %.4f)",
            jsonPath,
            config.drawAnchorX,
            config.drawAnchorY);
    }

    return loaded;
}
Ship::Ship(void)
    : sprites{},
      spritesLoaded(false),
      config{3.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.5088f, 0.6346f},
      healthVisual(HealthVisual::FULL),
      tilePosition{0.0f, 0.0f},
      tileTarget{0.0f, 0.0f},
      pathTiles{},
      pathDirections{},
      pathIndex(0),
      moving(false),
      directionUsesPair(false),
      directionToggle(false),
      moveDirection(MoveDirection::NONE),
      directionA(DiagonalDirection::DOWN_RIGHT),
      directionB(DiagonalDirection::DOWN_RIGHT)
{
}

Ship::~Ship(void)
{
    unloadSprites();
}

bool Ship::loadSpritesFromFolder(const char* folderPath, RC2D_StorageKind storageKind)
{
    if (folderPath == nullptr || folderPath[0] == '\0')
    {
        RC2D_log(RC2D_LOG_ERROR, "Ship: dossier de sprites invalide");
        return false;
    }

    unloadSprites();

    for (size_t i = 0; i < sprites.size(); ++i)
    {
        char path[512] = {0};
        std::snprintf(path, sizeof(path), "%s/%d.png", folderPath, static_cast<int>(i) + 1);

        sprites[i] = rc2d_graphics_loadImageFromStorage(path, storageKind);
        if (sprites[i].sdl_texture == nullptr)
        {
            RC2D_log(RC2D_LOG_ERROR, "Ship: impossible de charger le sprite '%s'", path);
            unloadSprites();
            return false;
        }
    }
    if (!loadDrawAnchorFromJson(folderPath, storageKind))
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "Ship: fichier JSON d'ancre absent/invalide, ancre par defaut conservee => anchor=(%.4f, %.4f)",
            config.drawAnchorX,
            config.drawAnchorY);
    }

    spritesLoaded = true;
    return true;
}
void Ship::unloadSprites(void)
{
    for (size_t i = 0; i < sprites.size(); ++i)
    {
        if (sprites[i].sdl_texture != nullptr)
        {
            rc2d_graphics_freeImage(&sprites[i]);
        }
    }

    spritesLoaded = false;
}

bool Ship::areSpritesLoaded(void) const
{
    return spritesLoaded;
}

void Ship::setHealthVisual(HealthVisual health)
{
    healthVisual = health;
}

Ship::HealthVisual Ship::getHealthVisual(void) const
{
    return healthVisual;
}

void Ship::setSpeedTilesPerSecond(float speed)
{
    if (speed > 0.0f)
    {
        config.speedTilesPerSecond = speed;
    }
}

float Ship::getSpeedTilesPerSecond(void) const
{
    return config.speedTilesPerSecond;
}

void Ship::setDrawScale(float scaleX, float scaleY)
{
    if (scaleX <= 0.0f || scaleY <= 0.0f)
    {
        return;
    }

    config.scaleX = scaleX;
    config.scaleY = scaleY;
}

void Ship::setDrawOffset(float offsetX, float offsetY)
{
    config.drawOffsetX = offsetX;
    config.drawOffsetY = offsetY;
}

void Ship::setDrawAnchor(float anchorX, float anchorY)
{
    anchorX = std::clamp(anchorX, 0.0f, 1.0f);
    anchorY = std::clamp(anchorY, 0.0f, 1.0f);

    config.drawAnchorX = anchorX;
    config.drawAnchorY = anchorY;
}

void Ship::setPositionTile(float tileX, float tileY)
{
    tilePosition.x = tileX;
    tilePosition.y = tileY;

    tileTarget = tilePosition;
    pathTiles.clear();
    pathDirections.clear();
    pathIndex = 0;
    moving = false;
    moveDirection = MoveDirection::NONE;
}

void Ship::setPositionTileInt(int tileX, int tileY)
{
    setPositionTile(static_cast<float>(tileX), static_cast<float>(tileY));
}

SDL_FPoint Ship::getPositionTile(void) const
{
    return tilePosition;
}

void Ship::moveToTile(const Map& map, int tileX, int tileY)
{
    const SDL_Point startRaw = map.roundTile(tilePosition.x, tilePosition.y);
    const SDL_Point startTile = map.clampTile(startRaw.x, startRaw.y);

    const SDL_Point goalRaw = SDL_Point{tileX, tileY};
    const SDL_Point goalTile = map.clampTile(goalRaw.x, goalRaw.y);

    tileTarget.x = static_cast<float>(goalTile.x);
    tileTarget.y = static_cast<float>(goalTile.y);

    pathTiles.clear();
    pathDirections.clear();
    pathIndex = 0;

    if (startTile.x == goalTile.x && startTile.y == goalTile.y)
    {
        moving = false;
        return;
    }

    if (buildPathAStar(map, startTile, goalTile, pathTiles, pathDirections) && !pathTiles.empty())
    {
        moving = true;
        return;
    }

    moving = false;
}

SDL_FPoint Ship::getTargetTile(void) const
{
    return tileTarget;
}

bool Ship::isMoving(void) const
{
    return moving;
}

void Ship::update(double dt, const Map& map)
{
    if (!moving)
    {
        return;
    }

    SDL_FPoint waypointTile = tileTarget;
    if (pathIndex < pathTiles.size())
    {
        waypointTile.x = static_cast<float>(pathTiles[pathIndex].x);
        waypointTile.y = static_cast<float>(pathTiles[pathIndex].y);
    }

    const SDL_FPoint currentScreen = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);
    const SDL_FPoint targetScreen = map.tileToScreenCenterFloat(waypointTile.x, waypointTile.y);

    const float deltaScreenX = targetScreen.x - currentScreen.x;
    const float deltaScreenY = targetScreen.y - currentScreen.y;
    const float distSq = (deltaScreenX * deltaScreenX) + (deltaScreenY * deltaScreenY);

    auto completeCurrentSegment = [&]() {
        tilePosition = waypointTile;

        // Alternance sprite uniquement au passage d'un waypoint (style Sea-like).
        if (pathIndex < pathTiles.size() && directionUsesPair)
        {
            directionToggle = !directionToggle;
        }

        if (pathIndex < pathTiles.size())
        {
            ++pathIndex;
            if (pathIndex >= pathTiles.size())
            {
                moving = false;
            }
        }
        else
        {
            moving = false;
        }
    };

    if (distSq <= 0.0001f)
    {
        completeCurrentSegment();
        return;
    }

    MoveDirection snappedDirection = MoveDirection::NONE;
    if (pathIndex < pathDirections.size())
    {
        snappedDirection = pathDirections[pathIndex];
    }

    if (snappedDirection == MoveDirection::NONE)
    {
        snappedDirection = quantizeScreenDirection(deltaScreenX, deltaScreenY);
    }

    updateDirection(snappedDirection);

    const float dist = std::sqrt(distSq);
    const float speedPixelsPerSecond = config.speedTilesPerSecond * map.getTileWidth();
    const float stepPixels = speedPixelsPerSecond * static_cast<float>(dt);

    if (stepPixels >= dist)
    {
        completeCurrentSegment();
        return;
    }

    const float moveScreenX = (deltaScreenX / dist) * stepPixels;
    const float moveScreenY = (deltaScreenY / dist) * stepPixels;

    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return;
    }

    const float tileDeltaX = ((moveScreenX / halfTileW) + (moveScreenY / halfTileH)) * 0.5f;
    const float tileDeltaY = ((moveScreenY / halfTileH) - (moveScreenX / halfTileW)) * 0.5f;

    tilePosition.x += tileDeltaX;
    tilePosition.y += tileDeltaY;

    const SDL_FPoint newScreen = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);
    const float remainX = targetScreen.x - newScreen.x;
    const float remainY = targetScreen.y - newScreen.y;
    const float remainDistSq = (remainX * remainX) + (remainY * remainY);
    if (remainDistSq <= 4.0f)
    {
        completeCurrentSegment();
    }
}

void Ship::draw(const Map& map) const
{
    const int spriteIndex = getCurrentSpriteIndex();
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(sprites.size()))
    {
        return;
    }

    const RC2D_Image& sprite = sprites[static_cast<size_t>(spriteIndex)];
    if (sprite.sdl_texture == nullptr)
    {
        return;
    }

    const SDL_FPoint center = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);
    drawSpriteCentered(sprite, center.x, center.y);
}

