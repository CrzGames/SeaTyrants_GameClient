#include "game/ships/ship.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <limits>
#include <queue>
#include <set>

namespace {

bool tileToSeaRowCol(int tileX, int tileY, int& outRow, int& outCol)
{
    // Conversion coordonnees "diamant" (tileX,tileY) vers grille Sea Bandits (row,col).
    const int s = tileX + tileY;       // = row - 1
    const int n = tileX - tileY;       // = 2*col (+1 si row pair)
    const int row = s + 1;

    int col = 0;
    if ((row & 1) == 0)
    {
        // row pair: n doit etre impair.
        if ((n & 1) == 0)
        {
            return false;
        }
        col = (n - 1) / 2;
    }
    else
    {
        // row impair: n doit etre pair.
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

bool seaRowColToTile(int row, int col, int& outTileX, int& outTileY)
{
    // Inverse de tileToSeaRowCol.
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

} // namespace

int Ship::directionToIndex(DiagonalDirection direction)
{
    return static_cast<int>(direction);
}

int Ship::spriteIndexForDirection(DiagonalDirection direction) const
{
    const int directionIndex = directionToIndex(direction);
    if (directionIndex < 0 || directionIndex >= 4)
    {
        return -1;
    }

    // full: 1..4 => indices 0..3, low: 5..8 => indices 4..7
    if (healthVisual == HealthVisual::LOW)
    {
        return 4 + directionIndex;
    }
    return directionIndex;
}

const RC2D_Image* Ship::getCurrentSprite(void) const
{
    if (!areSpritesLoaded())
    {
        return nullptr;
    }

    const DiagonalDirection activeDirection =
        (directionUsesPair && directionToggle) ? directionB : directionA;

    const int spriteIndex = spriteIndexForDirection(activeDirection);
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(sprites.size()))
    {
        return nullptr;
    }

    if (sprites[static_cast<size_t>(spriteIndex)].sdl_texture == nullptr)
    {
        return nullptr;
    }

    return &sprites[static_cast<size_t>(spriteIndex)];
}

Ship::MoveDirection Ship::quantizeScreenDirection(float deltaScreenX, float deltaScreenY)
{
    constexpr float kEpsilon = 0.0001f;
    if (std::fabs(deltaScreenX) <= kEpsilon && std::fabs(deltaScreenY) <= kEpsilon)
    {
        return MoveDirection::NONE;
    }

    // atan2 dans le repere ecran (Y positif vers le bas).
    const float angle = std::atan2(deltaScreenY, deltaScreenX);
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kStep = kPi / 4.0f; // 45 deg

    // Octant le plus proche.
    int octant = static_cast<int>(std::floor((angle + (kStep * 0.5f)) / kStep));

    // Normalise en [0..7].
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
        default: break;
    }

    return MoveDirection::NONE;
}

SDL_FPoint Ship::directionUnitScreen(MoveDirection direction)
{
    SDL_FPoint v = {0.0f, 0.0f};

    switch (direction)
    {
        case MoveDirection::RIGHT:      v = { 1.0f,  0.0f}; break;
        case MoveDirection::DOWN_RIGHT: v = { 1.0f,  1.0f}; break;
        case MoveDirection::DOWN:       v = { 0.0f,  1.0f}; break;
        case MoveDirection::DOWN_LEFT:  v = {-1.0f,  1.0f}; break;
        case MoveDirection::LEFT:       v = {-1.0f,  0.0f}; break;
        case MoveDirection::UP_LEFT:    v = {-1.0f, -1.0f}; break;
        case MoveDirection::UP:         v = { 0.0f, -1.0f}; break;
        case MoveDirection::UP_RIGHT:   v = { 1.0f, -1.0f}; break;
        case MoveDirection::NONE:
        default:                        v = { 0.0f,  0.0f}; break;
    }

    const float lenSq = (v.x * v.x) + (v.y * v.y);
    if (lenSq > 0.0f)
    {
        const float invLen = 1.0f / std::sqrt(lenSq);
        v.x *= invLen;
        v.y *= invLen;
    }

    return v;
}

float Ship::aStarStepCost(const Map& map, int deltaTileX, int deltaTileY)
{
    (void)map;
    (void)deltaTileX;
    (void)deltaTileY;
    // Sea Bandits: cout uniforme par step.
    return 1.0f;
}

float Ship::aStarHeuristic(const Map& map, int fromTileX, int fromTileY, int toTileX, int toTileY)
{
    (void)map;

    int fromRow = 0, fromCol = 0;
    int toRow = 0, toCol = 0;
    if (tileToSeaRowCol(fromTileX, fromTileY, fromRow, fromCol) &&
        tileToSeaRowCol(toTileX, toTileY, toRow, toCol))
    {
        const float dr = static_cast<float>(fromRow - toRow);
        const float dc = static_cast<float>(fromCol - toCol);
        return std::sqrt((dr * dr) + (dc * dc));
    }

    // Fallback robuste.
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

    // -1 = vide, sinon occupe.
    return map.getTileObject(tileX, tileY) < 0;
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
    std::set<OpenEntry, OpenCompare> open;

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
        aStarHeuristic(map, startTile.x, startTile.y, goalTile.x, goalTile.y);
    open.insert(OpenEntry{
        states[static_cast<size_t>(startIndex)].f,
        startRow,
        startCol,
        startIndex});

    // Sea Bandits-style: voisinage 4 selon la parite de row.
    constexpr int kNeighborCount = 4;
    const int oddRowDr[kNeighborCount] = {-1, 1, 1, -1};
    const int oddRowDc[kNeighborCount] = {0, 0, -1, -1};
    const int evenRowDr[kNeighborCount] = {-1, 1, 1, -1};
    const int evenRowDc[kNeighborCount] = {1, 1, 0, 0};

    bool found = false;

    while (!open.empty())
    {
        const OpenEntry current = *open.begin();
        open.erase(open.begin());

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

            const float stepCost = aStarStepCost(map, nextX - currentTile.x, nextY - currentTile.y);
            const float tentativeG = states[static_cast<size_t>(current.index)].g + stepCost;

            if (tentativeG >= states[static_cast<size_t>(nextIndex)].g)
            {
                continue;
            }

            states[static_cast<size_t>(nextIndex)].parent = current.index;
            states[static_cast<size_t>(nextIndex)].g = tentativeG;
            states[static_cast<size_t>(nextIndex)].f =
                tentativeG + aStarHeuristic(map, nextX, nextY, goalTile.x, goalTile.y);
            const SDL_FPoint currentCenter = map.tileToScreenCenter(currentTile.x, currentTile.y);
            const SDL_FPoint nextCenter = map.tileToScreenCenter(nextX, nextY);
            states[static_cast<size_t>(nextIndex)].dirFromParent = quantizeScreenDirection(
                nextCenter.x - currentCenter.x,
                nextCenter.y - currentCenter.y);

            open.insert(OpenEntry{
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

void Ship::updateDirection(double dt, MoveDirection direction)
{
    (void)dt;

    if (direction == MoveDirection::NONE)
    {
        return;
    }

    const DiagonalDirection currentVisualDirection =
        (directionUsesPair && directionToggle) ? directionB : directionA;

    auto selectPairStart = [currentVisualDirection](
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
            // Vers la droite: haut droite <-> bas droite
            selectPairStart(
                DiagonalDirection::UP_RIGHT,
                DiagonalDirection::DOWN_RIGHT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::LEFT:
            // Vers la gauche: haut gauche <-> bas gauche
            selectPairStart(
                DiagonalDirection::UP_LEFT,
                DiagonalDirection::DOWN_LEFT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::DOWN:
            // Vers le bas: bas gauche <-> bas droite
            selectPairStart(
                DiagonalDirection::DOWN_LEFT,
                DiagonalDirection::DOWN_RIGHT,
                nextA,
                nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::UP:
            // Vers le haut: haut gauche <-> haut droite
            selectPairStart(
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

    const bool pairChanged =
        (directionUsesPair != nextUsesPair) || directionA != nextA || directionB != nextB;

    directionUsesPair = nextUsesPair;
    directionA = nextA;
    directionB = nextB;
    moveDirection = direction;

    if (pairChanged)
    {
        directionToggle = false;
        directionTimerSeconds = 0.0;
    }

    if (!directionUsesPair)
    {
        // Diagonale pure: pas d'alternance.
        return;
    }
}

bool Ship::hasLineOfSightTiles(
    const Map& map,
    const SDL_Point& fromTile,
    const SDL_Point& toTile,
    const SDL_Point& startTile,
    const SDL_Point& goalTile) const
{
    const int dx = toTile.x - fromTile.x;
    const int dy = toTile.y - fromTile.y;
    const int steps = (std::max)(std::abs(dx), std::abs(dy));

    if (steps <= 0)
    {
        return true;
    }

    const float stepX = static_cast<float>(dx) / static_cast<float>(steps);
    const float stepY = static_cast<float>(dy) / static_cast<float>(steps);

    float x = static_cast<float>(fromTile.x);
    float y = static_cast<float>(fromTile.y);

    for (int i = 1; i <= steps; ++i)
    {
        x += stepX;
        y += stepY;

        const SDL_Point tile = map.roundTile(x, y);
        if (!isWalkableForPath(map, tile.x, tile.y, startTile, goalTile))
        {
            return false;
        }
    }

    return true;
}

void Ship::simplifyPath(
    const Map& map,
    const SDL_Point& startTile,
    const SDL_Point& goalTile,
    const std::vector<SDL_Point>& rawPath,
    std::vector<SDL_Point>& outPath) const
{
    outPath.clear();
    if (rawPath.empty())
    {
        return;
    }

    SDL_Point anchor = startTile;
    size_t i = 0;

    while (i < rawPath.size())
    {
        size_t best = i;

        for (size_t j = i; j < rawPath.size(); ++j)
        {
            if (hasLineOfSightTiles(map, anchor, rawPath[j], startTile, goalTile))
            {
                best = j;
            }
            else
            {
                break;
            }
        }

        outPath.push_back(rawPath[best]);
        anchor = rawPath[best];
        i = best + 1;
    }
}

void Ship::drawSpriteCentered(const RC2D_Image& sprite, float centerX, float centerY) const
{
    if (sprite.sdl_texture == nullptr)
    {
        return;
    }

    const float width = static_cast<float>(sprite.sdl_texture->w);
    const float height = static_cast<float>(sprite.sdl_texture->h);

    RC2D_Quad quad = {};
    quad.src = SDL_FRect{0.0f, 0.0f, width, height};

    const float drawX = centerX - ((width * config.scaleX) * 0.5f);
    const float drawY = centerY - ((height * config.scaleY) * 0.5f);

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

Ship::Ship(void)
    : sprites{},
      spritesLoaded(false),
      config{2.75f, 0.20f, 1.0f, 1.0f, 0.0f, 0.0f},
      healthVisual(HealthVisual::FULL),
      tilePosition{0.0f, 0.0f},
      tileTarget{0.0f, 0.0f},
      pathTiles{},
      pathDirections{},
      pathIndex(0),
      moving(false),
      directionUsesPair(false),
      directionToggle(false),
      directionTimerSeconds(0.0),
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
        const int index1 = static_cast<int>(i) + 1;
        std::snprintf(path, sizeof(path), "%s/%d.png", folderPath, index1);

        sprites[i] = rc2d_graphics_loadImageFromStorage(path, storageKind);
        if (sprites[i].sdl_texture == nullptr)
        {
            RC2D_log(RC2D_LOG_ERROR, "Ship: impossible de charger le sprite '%s'", path);
            unloadSprites();
            return false;
        }
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

void Ship::setCardinalSwapIntervalSeconds(float intervalSeconds)
{
    if (intervalSeconds > 0.0f)
    {
        config.cardinalSwapIntervalSeconds = intervalSeconds;
    }
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

void Ship::setTargetTile(const Map& map, int tileX, int tileY)
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

    // SeaBandits-style: sans chemin A*, on ne force pas de deplacement direct.
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

    auto completeCurrentSubSegment = [&]() {
        tilePosition = waypointTile;

        // SeaBandits-style: alternance uniquement au changement de sous-segment,
        // jamais via un timer temps-reel.
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
        completeCurrentSubSegment();
        return;
    }

    const float dist = std::sqrt(distSq);

    // Direction visuelle verrouillee sur le segment de path courant
    // (comportement plus proche Sea Bandits / SeaFight).
    SDL_FPoint segmentFromTile = tilePosition;
    if (pathIndex > 0 && (pathIndex - 1) < pathTiles.size())
    {
        segmentFromTile.x = static_cast<float>(pathTiles[pathIndex - 1].x);
        segmentFromTile.y = static_cast<float>(pathTiles[pathIndex - 1].y);
    }
    else
    {
        const SDL_Point rounded = map.roundTile(tilePosition.x, tilePosition.y);
        segmentFromTile.x = static_cast<float>(rounded.x);
        segmentFromTile.y = static_cast<float>(rounded.y);
    }

    MoveDirection snappedDirection = MoveDirection::NONE;
    if (pathIndex < pathDirections.size())
    {
        // Direction derivee du path A* (equivalent dir stockee dans SeaBandits).
        snappedDirection = pathDirections[pathIndex];
    }

    if (snappedDirection == MoveDirection::NONE)
    {
        const SDL_FPoint segmentFromScreen =
            map.tileToScreenCenterFloat(segmentFromTile.x, segmentFromTile.y);
        const SDL_FPoint segmentToScreen =
            map.tileToScreenCenterFloat(waypointTile.x, waypointTile.y);

        snappedDirection = quantizeScreenDirection(
            segmentToScreen.x - segmentFromScreen.x,
            segmentToScreen.y - segmentFromScreen.y);
    }
    updateDirection(dt, snappedDirection);

    const float speedPixelsPerSecond = config.speedTilesPerSecond * map.getTileWidth();
    const float stepPixels = speedPixelsPerSecond * static_cast<float>(dt);

    if (stepPixels >= dist)
    {
        completeCurrentSubSegment();
        return;
    }

    // Important: mouvement sur la vraie direction vers le waypoint.
    // Ne pas utiliser un vecteur quantifie (45deg), sinon on cree des micro-erreurs
    // angulaires et des changements visuels parasites sur les sprites.
    const float moveScreenX = (deltaScreenX / dist) * stepPixels;
    const float moveScreenY = (deltaScreenY / dist) * stepPixels;

    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return;
    }

    // Inverse de la projection iso:
    // screenX = (tileX - tileY) * halfTileW
    // screenY = (tileX + tileY) * halfTileH
    const float tileDeltaX = ((moveScreenX / halfTileW) + (moveScreenY / halfTileH)) * 0.5f;
    const float tileDeltaY = ((moveScreenY / halfTileH) - (moveScreenX / halfTileW)) * 0.5f;

    tilePosition.x += tileDeltaX;
    tilePosition.y += tileDeltaY;

    // Garde-fou: si on arrive tres pres du waypoint, on snap.
    const SDL_FPoint newScreen = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);
    const float remainingX = targetScreen.x - newScreen.x;
    const float remainingY = targetScreen.y - newScreen.y;
    const float remainingDistSq = (remainingX * remainingX) + (remainingY * remainingY);
    if (remainingDistSq <= 4.0f) // ~2px
    {
        completeCurrentSubSegment();
    }
}

void Ship::draw(const Map& map) const
{
    const RC2D_Image* sprite = getCurrentSprite();
    if (sprite == nullptr)
    {
        return;
    }

    const SDL_FPoint center = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);

    drawSpriteCentered(
        *sprite,
        center.x + config.drawOffsetX,
        center.y + config.drawOffsetY);
}
