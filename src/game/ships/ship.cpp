#include "game/ships/ship.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <queue>

#include <cJSON.h>

#include "core/context.h"

// Generateur d'ID runtime : chaque navire cree recoit un identifiant unique
// utilise pour le sillage ocean (wake trail). Atomique car thread-safe.
static std::atomic<uint64_t> g_nextRuntimeShipId{1u};

// =============================================================================
// Helpers grille Sea-like (fonctions libres)
// =============================================================================
//
// Le pathfinding utilise un repere "Sea-like" (row/col) au lieu du repere
// iso (tileX/tileY). Dans ce repere, chaque tuile a exactement 4 voisins
// (pas 8), ce qui correspond au mouvement autorise dans Seafight.
//
// Conversion :
//   s = tileX + tileY  (diagonale montante iso)
//   n = tileX - tileY  (diagonale descendante iso)
//   row = s + 1         (decalage pour eviter row=0 sur la premiere diagonale)
//   col depend de la parite de row :
//     - row impair : col = n / 2         (n doit etre pair)
//     - row pair    : col = (n - 1) / 2  (n doit etre impair)
//
// Si la parite n'est pas respectee, le point iso ne correspond pas a une
// case valide du repere Sea-like -> la conversion echoue.
// =============================================================================

static bool tileToSeaRowCol(int tileX, int tileY, int& outRow, int& outCol)
{
    // Etape 1 : passage en diagonales iso.
    const int s = tileX + tileY;
    const int n = tileX - tileY;

    // Etape 2 : row dans le repere Sea-like.
    const int row = s + 1;

    // Etape 3 : col, avec verification de la parite.
    int col = 0;

    if ((row & 1) == 0)
    {
        // Row paire : n doit etre impair.
        if ((n & 1) == 0)
        {
            return false;
        }
        col = (n - 1) / 2;
    }
    else
    {
        // Row impaire : n doit etre pair.
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

static bool seaRowColToTile(int row, int col, int& outTileX, int& outTileY)
{
    // Etape 1 : reconstruction des diagonales iso.
    const int s = row - 1;
    const int n = (2 * col) + (((row & 1) == 0) ? 1 : 0);

    // Etape 2 : conversion vers tileX/tileY.
    // tileX = (s + n) / 2, tileY = (s - n) / 2.
    // Les deux sommes doivent etre paires pour que la division soit exacte.
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

// =============================================================================
// Sprites / Direction visuelle
// =============================================================================

int Ship::directionToIndex(DiagonalDirection direction)
{
    // Cast direct de l'enum [DOWN_LEFT=0, UP_RIGHT=1, UP_LEFT=2, DOWN_RIGHT=3].
    return static_cast<int>(direction);
}

int Ship::spriteIndexForDirection(DiagonalDirection direction) const
{
    const int dirIndex = directionToIndex(direction);

    // 4 directions valides : indices 0..3.
    if (dirIndex < 0 || dirIndex >= 4)
    {
        return -1;
    }

    // Les sprites sont organises en 2 blocs de 4 :
    //   [0..3] = coque pleine (FULL)
    //   [4..7] = coque abimee (LOW)
    return (this->healthVisual == HealthVisual::LOW) ? (4 + dirIndex) : dirIndex;
}

int Ship::getCurrentSpriteIndex(void) const
{
    // Pas de sprites charges -> rien a afficher.
    if (!this->spritesLoaded)
    {
        return -1;
    }

    // La direction visuelle courante depend de l'alternance (toggle A/B).
    // Si on utilise une paire et que le toggle est actif, on prend directionB.
    // Sinon, directionA.
    const DiagonalDirection visualDirection =
        (this->directionUsesPair && this->directionToggle) ? this->directionB : this->directionA;

    // Conversion direction -> index sprite [0..7].
    const int spriteIndex = this->spriteIndexForDirection(visualDirection);

    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->sprites.size()))
    {
        return -1;
    }

    // Verification que la texture est effectivement chargee.
    if (this->sprites[static_cast<size_t>(spriteIndex)].sdl_texture == nullptr)
    {
        return -1;
    }

    return spriteIndex;
}

Ship::MoveDirection Ship::quantizeScreenDirection(float deltaScreenX, float deltaScreenY)
{
    // --- Quantifie un vecteur ecran en 8 directions (octants) ---

    // Vecteur nul -> pas de direction.
    constexpr float kEpsilon = 0.0001f;
    if (std::fabs(deltaScreenX) <= kEpsilon && std::fabs(deltaScreenY) <= kEpsilon)
    {
        return MoveDirection::NONE;
    }

    // Calcul de l'angle du vecteur par rapport a l'axe X positif.
    // atan2 retourne [-pi, +pi].
    const float angle = std::atan2(deltaScreenY, deltaScreenX);

    // Chaque octant couvre pi/4 radians (45 degres).
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kStep = kPi / 4.0f;

    // On decale de demi-octant pour centrer chaque octant sur sa direction pure,
    // puis on divise par la taille d'un octant pour obtenir l'index [0..7].
    int octant = static_cast<int>(std::floor((angle + (kStep * 0.5f)) / kStep));

    // Normalisation dans [0, 7] (modulo positif).
    octant %= 8;
    if (octant < 0)
    {
        octant += 8;
    }

    // Mapping octant -> direction.
    // octant 0 = droite (angle ~0), puis sens horaire.
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

void Ship::updateDirection(MoveDirection direction)
{
    // --- Convertit une direction de mouvement en direction(s) visuelle(s) de sprite ---
    //
    // On dispose de 4 sprites diagonaux (DOWN_LEFT, UP_RIGHT, UP_LEFT, DOWN_RIGHT).
    // Pour les axes cardinaux (RIGHT, LEFT, UP, DOWN), on alterne entre 2 diagonales
    // a chaque sous-segment du path, ce qui donne un mouvement "en crabe" naturel
    // (style Seafight). Pour les diagonales pures, un seul sprite fixe suffit.

    if (direction == MoveDirection::NONE)
    {
        return;
    }

    // Direction visuelle actuellement affichee.
    const DiagonalDirection currentVisualDirection =
        (this->directionUsesPair && this->directionToggle) ? this->directionB : this->directionA;

    // Lambda : si la direction courante fait deja partie de la paire,
    // on la garde en premiere position pour un enchainement naturel
    // (pas de "saut" visuel brutal).
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

        // Aucun match : on prend l'ordre par defaut.
        outA = pairA;
        outB = pairB;
    };

    DiagonalDirection nextA = this->directionA;
    DiagonalDirection nextB = this->directionB;
    bool nextUsesPair = this->directionUsesPair;

    switch (direction)
    {
        // --- Axes cardinaux : alternance de 2 diagonales ---

        case MoveDirection::RIGHT:
            keepNaturalPairStart(DiagonalDirection::UP_RIGHT, DiagonalDirection::DOWN_RIGHT, nextA, nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::LEFT:
            keepNaturalPairStart(DiagonalDirection::UP_LEFT, DiagonalDirection::DOWN_LEFT, nextA, nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::DOWN:
            keepNaturalPairStart(DiagonalDirection::DOWN_LEFT, DiagonalDirection::DOWN_RIGHT, nextA, nextB);
            nextUsesPair = true;
            break;

        case MoveDirection::UP:
            keepNaturalPairStart(DiagonalDirection::UP_LEFT, DiagonalDirection::UP_RIGHT, nextA, nextB);
            nextUsesPair = true;
            break;

        // --- Diagonales pures : 1 seul sprite fixe ---

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

    // Detection de changement de paire visuelle :
    // si la paire a change, on reset le toggle pour repartir de A.
    const bool visualPairChanged =
        (this->directionUsesPair != nextUsesPair) ||
        (this->directionA != nextA) ||
        (this->directionB != nextB);

    this->directionUsesPair = nextUsesPair;
    this->directionA = nextA;
    this->directionB = nextB;
    this->moveDirection = direction;

    if (visualPairChanged)
    {
        this->directionToggle = false;
    }
}

// =============================================================================
// Pathfinding A*
// =============================================================================

float Ship::aStarHeuristic(int fromTileX, int fromTileY, int toTileX, int toTileY)
{
    // --- Heuristique A* : distance euclidienne dans le repere Sea-like ---
    //
    // On utilise le repere Sea-like (row/col) plutot que le repere iso car
    // le voisinage A* est defini en Sea-like (4 voisins). L'heuristique
    // doit etre coherente avec ce voisinage pour rester admissible.

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

    // Fallback si conversion Sea-like impossible : distance euclidienne iso.
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
    (void)goalTile;

    // Regle 1 : une tuile hors map est toujours interdite.
    if (!map.isInside(tileX, tileY))
    {
        return false;
    }

    // Regle 2 : la tuile de depart est toujours autorisee, meme si elle est
    // bloquee, pour permettre au navire d'en sortir si l'edition a change la
    // collision sous ses pieds.
    if (tileX == startTile.x && tileY == startTile.y)
    {
        return true;
    }

    // Regle 3 : toutes les autres tuiles (y compris la cible) respectent
    // strictement la couche collision.
    return !map.isTileBlocked(tileX, tileY);
}

bool Ship::buildPathAStar(
    const Map& map,
    const SDL_Point& startTile,
    const SDL_Point& goalTile,
    std::vector<SDL_Point>& outPath,
    std::vector<MoveDirection>& outDirections) const
{
    // --- A* avec voisinage Sea-like (4 directions) ---
    //
    // Construit un chemin start -> goal, puis reconstruit la liste
    // des waypoints (tuiles) et des directions visuelles par segment.

    outPath.clear();
    outDirections.clear();

    // Validation des bornes.
    if (!map.isInside(startTile.x, startTile.y) || !map.isInside(goalTile.x, goalTile.y))
    {
        return false;
    }

    // Deja sur place : rien a faire.
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

    // --- Lambdas de conversion index <-> tuile ---

    auto indexOf = [width](int x, int y) -> int {
        return (y * width) + x;
    };

    auto pointOf = [width](int index) -> SDL_Point {
        SDL_Point p = {};
        p.x = index % width;
        p.y = index / width;
        return p;
    };

    // --- Structures A* ---

    // Etat d'un noeud dans la grille.
    struct NodeState {
        float g;                    // Cout reel depuis le depart.
        float f;                    // f = g + heuristique.
        int parent;                 // Index du noeud parent (-1 = aucun).
        bool closed;                // True si le noeud a ete traite.
        MoveDirection dirFromParent; // Direction visuelle pour l'animation.
    };

    // Entree dans l'open set (triee par f croissant).
    struct OpenEntry {
        float f;
        int row;   // row Sea-like (pour le tri deterministe).
        int col;   // col Sea-like (pour le tri deterministe).
        int index; // Index lineaire dans la grille.
    };

    // Comparateur pour min-heap : tri par f, puis (row, col, index)
    // pour garder un ordre deterministe.
    struct OpenCompare {
        bool operator()(const OpenEntry& a, const OpenEntry& b) const
        {
            if (a.f != b.f)
            {
                return a.f > b.f;
            }

            if (a.row != b.row)
            {
                return a.row > b.row;
            }

            if (a.col != b.col)
            {
                return a.col > b.col;
            }

            return a.index > b.index;
        }
    };

    // --- Initialisation ---

    const float inf = std::numeric_limits<float>::infinity();

    // Buffers reutilises entre appels pour eviter des reallocations massives.
    thread_local std::vector<NodeState> states;
    thread_local std::vector<uint32_t> stateStamp;
    thread_local uint32_t stateGeneration = 0U;

    if (states.size() < static_cast<size_t>(total))
    {
        states.resize(static_cast<size_t>(total));
    }
    if (stateStamp.size() < static_cast<size_t>(total))
    {
        stateStamp.resize(static_cast<size_t>(total), 0U);
    }

    // Increment de generation (avec protection overflow).
    stateGeneration += 1U;
    if (stateGeneration == 0U)
    {
        std::fill(stateStamp.begin(), stateStamp.end(), 0U);
        stateGeneration = 1U;
    }

    std::vector<NodeState>& statesRef = states;
    std::vector<uint32_t>& stateStampRef = stateStamp;
    const uint32_t currentGeneration = stateGeneration;

    auto ensureState = [&statesRef, &stateStampRef, currentGeneration, inf](int tileIndex) -> NodeState& {
        if (stateStampRef[static_cast<size_t>(tileIndex)] != currentGeneration)
        {
            stateStampRef[static_cast<size_t>(tileIndex)] = currentGeneration;
            statesRef[static_cast<size_t>(tileIndex)] =
                NodeState{inf, inf, -1, false, MoveDirection::NONE};
        }

        return statesRef[static_cast<size_t>(tileIndex)];
    };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenCompare> openSet;

    const int startIndex = indexOf(startTile.x, startTile.y);
    const int goalIndex = indexOf(goalTile.x, goalTile.y);

    // Conversion du point de depart en repere Sea-like.
    int startRow = 0;
    int startCol = 0;
    if (!tileToSeaRowCol(startTile.x, startTile.y, startRow, startCol))
    {
        return false;
    }

    int goalRow = 0;
    int goalCol = 0;
    const bool goalSeaValid = tileToSeaRowCol(goalTile.x, goalTile.y, goalRow, goalCol);

    auto heuristicFromSea = [goalSeaValid, goalRow, goalCol](int fromRow, int fromCol) -> float {
        if (!goalSeaValid)
        {
            return 0.0f;
        }

        const float dr = static_cast<float>(fromRow - goalRow);
        const float dc = static_cast<float>(fromCol - goalCol);
        return std::sqrt((dr * dr) + (dc * dc));
    };

    // Le noeud de depart a un cout g=0.
    NodeState& startState = ensureState(startIndex);
    startState.g = 0.0f;
    startState.f = goalSeaValid
        ? heuristicFromSea(startRow, startCol)
        : aStarHeuristic(startTile.x, startTile.y, goalTile.x, goalTile.y);

    openSet.push(OpenEntry{
        startState.f,
        startRow,
        startCol,
        startIndex});

    // --- Tables de voisinage Sea-like ---
    //
    // Chaque tuile a 4 voisins. Les deltas (dr, dc) dependent de la parite
    // de la ligne (row) dans le repere Sea-like.
    constexpr int kNeighborCount = 4;
    const int oddRowDr[kNeighborCount]  = {-1,  1,  1, -1};
    const int oddRowDc[kNeighborCount]  = { 0,  0, -1, -1};
    const int evenRowDr[kNeighborCount] = {-1,  1,  1, -1};
    const int evenRowDc[kNeighborCount] = { 1,  1,  0,  0};

    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;

    // --- Boucle principale A* ---

    bool found = false;

    while (!openSet.empty())
    {
        // Extraction du noeud avec le plus petit f.
        const OpenEntry current = openSet.top();
        openSet.pop();

        NodeState& currentState = ensureState(current.index);

        // Noeud deja ferme (doublon stale) -> on l'ignore.
        if (currentState.closed)
        {
            continue;
        }

        // Entree stale (ancienne valeur de f) -> on l'ignore.
        if (current.f > currentState.f)
        {
            continue;
        }

        // Goal atteint -> on sort pour reconstruire le chemin.
        if (current.index == goalIndex)
        {
            found = true;
            break;
        }

        // Fermer le noeud courant.
        currentState.closed = true;

        // Reconversion de l'index en tuile puis en (row, col) Sea-like.
        const SDL_Point currentTile = pointOf(current.index);

        int currentRow = 0;
        int currentCol = 0;
        if (!tileToSeaRowCol(currentTile.x, currentTile.y, currentRow, currentCol))
        {
            continue;
        }

        const bool evenRow = ((currentRow & 1) == 0);

        // --- Expansion des 4 voisins ---
        for (int i = 0; i < kNeighborCount; ++i)
        {
            // Calcul du voisin dans le repere Sea-like.
            const int nextRow = currentRow + (evenRow ? evenRowDr[i] : oddRowDr[i]);
            const int nextCol = currentCol + (evenRow ? evenRowDc[i] : oddRowDc[i]);

            // Conversion retour en tuile iso.
            int nextX = 0;
            int nextY = 0;
            if (!seaRowColToTile(nextRow, nextCol, nextX, nextY))
            {
                continue;
            }

            // Verification de traversabilite.
            if (!this->isWalkableForPath(map, nextX, nextY, startTile, goalTile))
            {
                continue;
            }

            const int nextIndex = indexOf(nextX, nextY);
            NodeState& nextState = ensureState(nextIndex);

            // Noeud deja ferme -> on l'ignore.
            if (nextState.closed)
            {
                continue;
            }

            // Cout tentative : chaque deplacement coute 1.
            const float tentativeG = currentState.g + 1.0f;

            // Ce chemin n'ameliore pas le meilleur cout connu -> on l'ignore.
            if (tentativeG >= nextState.g)
            {
                continue;
            }

            // Mise a jour du noeud voisin.
            nextState.parent = current.index;
            nextState.g = tentativeG;
            nextState.f = tentativeG + (goalSeaValid
                    ? heuristicFromSea(nextRow, nextCol)
                    : aStarHeuristic(nextX, nextY, goalTile.x, goalTile.y));

            // Calcul de la direction visuelle du segment courant -> voisin.
            // Equivalence de tileToScreenCenter(next) - tileToScreenCenter(current),
            // sans repasser par deux conversions completes.
            const float deltaTileX = static_cast<float>(nextX - currentTile.x);
            const float deltaTileY = static_cast<float>(nextY - currentTile.y);
            const float deltaScreenX = (deltaTileX - deltaTileY) * halfTileW;
            const float deltaScreenY = (deltaTileX + deltaTileY) * halfTileH;
            nextState.dirFromParent = quantizeScreenDirection(deltaScreenX, deltaScreenY);

            // Insertion dans l'open set.
            openSet.push(OpenEntry{
                nextState.f,
                nextRow,
                nextCol,
                nextIndex});
        }
    }

    if (!found)
    {
        return false;
    }

    // --- Reconstruction du chemin ---
    //
    // On remonte les parents depuis le goal jusqu'au start.
    // Le start lui-meme n'est pas inclus dans le chemin de sortie.
    int walk = goalIndex;
    while (walk != startIndex)
    {
        if (walk < 0 || walk >= total)
        {
            outPath.clear();
            outDirections.clear();
            return false;
        }

        if (stateStamp[static_cast<size_t>(walk)] != stateGeneration)
        {
            outPath.clear();
            outDirections.clear();
            return false;
        }

        const NodeState& walkState = states[static_cast<size_t>(walk)];
        outPath.push_back(pointOf(walk));
        outDirections.push_back(walkState.dirFromParent);

        walk = walkState.parent;

        if (walk < 0)
        {
            outPath.clear();
            outDirections.clear();
            return false;
        }
    }

    // Le chemin a ete construit a l'envers (goal -> start), on l'inverse.
    std::reverse(outPath.begin(), outPath.end());
    std::reverse(outDirections.begin(), outDirections.end());
    return true;
}

// =============================================================================
// Constructeur / Destructeur
// =============================================================================

Ship::Ship(void)
    : sprites{},
      spriteDrawAnchors{},
      spritesLoaded(false),
      runtimeShipId(g_nextRuntimeShipId.fetch_add(1u)),
      config{3.0f},
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
      directionB(DiagonalDirection::DOWN_RIGHT),
      drawScale(1.0f),
      drawAlpha(255)
{
    // Anchor par defaut : centre du sprite (0.5, 0.5).
    // Sera surcharge par ship_anchor.json lors du chargement des sprites.
    for (SDL_FPoint& anchor : this->spriteDrawAnchors)
    {
        anchor.x = 0.5f;
        anchor.y = 0.5f;
    }
}

Ship::~Ship(void)
{
    unloadSprites();
}

// =============================================================================
// Chargement sprites + anchors JSON
// =============================================================================

bool Ship::loadSpritesFromFolder(const char* folderPath, RC2D_StorageKind storageKind)
{
    if (folderPath == nullptr || folderPath[0] == '\0')
    {
        RC2D_log(RC2D_LOG_ERROR, "Ship: dossier de sprites invalide");
        return false;
    }

    // Nettoyage prealable si on recharge un atlas.
    unloadSprites();

    // Chargement des 8 sprites (1.png .. 8.png).
    // 4 directions x 2 etats de vie (FULL + LOW).
    for (size_t i = 0; i < this->sprites.size(); ++i)
    {
        char path[512] = {0};
        std::snprintf(path, sizeof(path), "%s/%d.png", folderPath, static_cast<int>(i) + 1);

        this->sprites[i] = rc2d_graphics_loadImageFromStorage(path, storageKind);

        if (this->sprites[i].sdl_texture == nullptr)
        {
            RC2D_log(RC2D_LOG_ERROR, "Ship: impossible de charger le sprite '%s'", path);
            unloadSprites();
            return false;
        }
    }

    // Chargement des anchors depuis ship_anchor.json.
    // Si le fichier n'existe pas ou est invalide, les anchors restent a (0.5, 0.5).
    this->loadAnchorsFromJson(folderPath, storageKind);

    this->spritesLoaded = true;
    return true;
}

void Ship::unloadSprites(void)
{
    for (size_t i = 0; i < this->sprites.size(); ++i)
    {
        if (this->sprites[i].sdl_texture != nullptr)
        {
            rc2d_graphics_freeImage(&this->sprites[i]);
        }
    }

    this->spritesLoaded = false;
}

bool Ship::loadAnchorsFromJson(const char* folderPath, RC2D_StorageKind storageKind)
{
    // --- Charge les anchors de rendu depuis ship_anchor.json ---
    //
    // Priorite de lecture :
    //   1) Racine : { "anchorX": ..., "anchorY": ... }         -> anchor global
    //   2) Bloc "default" : { "anchorX": ... } ou { "anchorPixelX": ... }  -> anchor global
    //   3) Bloc "frames" : { "1": { "anchorX": ... }, ... }    -> anchor par sprite
    //
    // L'anchor global s'applique aux 8 sprites. Les frames ecrasent individuellement.

    if (folderPath == nullptr || folderPath[0] == '\0')
    {
        return false;
    }

    // Construction du chemin vers ship_anchor.json.
    char jsonPath[640] = {0};
    std::snprintf(jsonPath, sizeof(jsonPath), "%s/%s", folderPath, "ship_anchor.json");

    // Lecture du fichier selon le type de storage.
    void* bytes = nullptr;
    Uint64 len = 0;
    bool readOk = false;

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

    // Parse JSON.
    cJSON* root = cJSON_ParseWithLength(static_cast<const char*>(bytes), static_cast<size_t>(len));
    RC2D_free(bytes);
    bytes = nullptr;

    if (root == nullptr)
    {
        return false;
    }

    // --- Lambda utilitaires ---

    // Retourne la taille en pixels d'un sprite (pour convertir anchorPixel -> anchorNorm).
    auto getSpriteSize = [this](int spriteIndex, float& outW, float& outH) -> bool {
        if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->sprites.size()))
        {
            return false;
        }

        const RC2D_Image& sprite = this->sprites[static_cast<size_t>(spriteIndex)];
        if (sprite.sdl_texture == nullptr)
        {
            return false;
        }

        outW = static_cast<float>(sprite.sdl_texture->w);
        outH = static_cast<float>(sprite.sdl_texture->h);
        return (outW > 0.0f && outH > 0.0f);
    };

    // Valide et ecrit l'anchor d'un sprite specifique.
    auto setSpriteAnchorIfValid = [this](int spriteIndex, float anchorX, float anchorY) -> bool {
        if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->spriteDrawAnchors.size()))
        {
            return false;
        }

        if (!std::isfinite(anchorX) || !std::isfinite(anchorY))
        {
            return false;
        }

        this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)].x = std::clamp(anchorX, 0.0f, 1.0f);
        this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)].y = std::clamp(anchorY, 0.0f, 1.0f);
        return true;
    };

    // Convertit un anchor en pixels vers normalise, puis l'ecrit sur un sprite.
    auto setSpriteAnchorFromPixels = [&](int spriteIndex, float anchorPixelX, float anchorPixelY) -> bool {
        float w = 0.0f;
        float h = 0.0f;
        if (!getSpriteSize(spriteIndex, w, h))
        {
            return false;
        }

        return setSpriteAnchorIfValid(spriteIndex, anchorPixelX / w, anchorPixelY / h);
    };

    bool loadedAny = false;
    bool loadedGlobal = false;

    // --- 1) Anchor global en racine : { "anchorX": ..., "anchorY": ... } ---

    const cJSON* rootAnchorX = cJSON_GetObjectItemCaseSensitive(root, "anchorX");
    const cJSON* rootAnchorY = cJSON_GetObjectItemCaseSensitive(root, "anchorY");

    if (cJSON_IsNumber(rootAnchorX) && cJSON_IsNumber(rootAnchorY))
    {
        const float ax = static_cast<float>(rootAnchorX->valuedouble);
        const float ay = static_cast<float>(rootAnchorY->valuedouble);

        if (std::isfinite(ax) && std::isfinite(ay))
        {
            // Appliquer l'anchor global a tous les sprites.
            for (SDL_FPoint& anchor : this->spriteDrawAnchors)
            {
                anchor.x = std::clamp(ax, 0.0f, 1.0f);
                anchor.y = std::clamp(ay, 0.0f, 1.0f);
            }

            loadedGlobal = true;
            loadedAny = true;
        }
    }

    // --- 2) Bloc "default" (si pas deja charge en racine) ---

    const cJSON* defaultObj = cJSON_GetObjectItemCaseSensitive(root, "default");

    if (!loadedGlobal && cJSON_IsObject(defaultObj))
    {
        // Essayer anchorX / anchorY normalise.
        const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorX");
        const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorY");

        if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
        {
            const float ax = static_cast<float>(jAnchorX->valuedouble);
            const float ay = static_cast<float>(jAnchorY->valuedouble);

            if (std::isfinite(ax) && std::isfinite(ay))
            {
                for (SDL_FPoint& anchor : this->spriteDrawAnchors)
                {
                    anchor.x = std::clamp(ax, 0.0f, 1.0f);
                    anchor.y = std::clamp(ay, 0.0f, 1.0f);
                }

                loadedGlobal = true;
                loadedAny = true;
            }
        }

        // Sinon essayer anchorPixelX / anchorPixelY.
        if (!loadedGlobal)
        {
            const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelX");
            const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(defaultObj, "anchorPixelY");

            if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
            {
                // Convertir pixels -> normalise en utilisant le sprite 0.
                float w = 0.0f;
                float h = 0.0f;

                if (getSpriteSize(0, w, h))
                {
                    const float ax = static_cast<float>(jAnchorPixelX->valuedouble) / w;
                    const float ay = static_cast<float>(jAnchorPixelY->valuedouble) / h;

                    if (std::isfinite(ax) && std::isfinite(ay))
                    {
                        for (SDL_FPoint& anchor : this->spriteDrawAnchors)
                        {
                            anchor.x = std::clamp(ax, 0.0f, 1.0f);
                            anchor.y = std::clamp(ay, 0.0f, 1.0f);
                        }

                        loadedGlobal = true;
                        loadedAny = true;
                    }
                }
            }
        }
    }

    // --- 3) Bloc "frames" : surcharge par sprite ---

    const cJSON* framesObj = cJSON_GetObjectItemCaseSensitive(root, "frames");

    if (cJSON_IsObject(framesObj))
    {
        for (int spriteNum = 1; spriteNum <= static_cast<int>(this->spriteDrawAnchors.size()); ++spriteNum)
        {
            // Chercher la cle "1.png", "2.png"... ou "1", "2"...
            char frameKeyPng[16] = {0};
            char frameKeyNum[8] = {0};
            std::snprintf(frameKeyPng, sizeof(frameKeyPng), "%d.png", spriteNum);
            std::snprintf(frameKeyNum, sizeof(frameKeyNum), "%d", spriteNum);

            const cJSON* frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, frameKeyPng);
            if (!cJSON_IsObject(frameObj))
            {
                frameObj = cJSON_GetObjectItemCaseSensitive(framesObj, frameKeyNum);
            }
            if (!cJSON_IsObject(frameObj))
            {
                continue;
            }

            const int spriteIndex = spriteNum - 1;
            bool loadedFrameAnchor = false;

            // Essayer anchorX / anchorY normalise.
            const cJSON* jAnchorX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorX");
            const cJSON* jAnchorY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorY");

            if (cJSON_IsNumber(jAnchorX) && cJSON_IsNumber(jAnchorY))
            {
                loadedFrameAnchor = setSpriteAnchorIfValid(
                    spriteIndex,
                    static_cast<float>(jAnchorX->valuedouble),
                    static_cast<float>(jAnchorY->valuedouble));
            }

            // Sinon essayer anchorPixelX / anchorPixelY.
            if (!loadedFrameAnchor)
            {
                const cJSON* jAnchorPixelX = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelX");
                const cJSON* jAnchorPixelY = cJSON_GetObjectItemCaseSensitive(frameObj, "anchorPixelY");

                if (cJSON_IsNumber(jAnchorPixelX) && cJSON_IsNumber(jAnchorPixelY))
                {
                    loadedFrameAnchor = setSpriteAnchorFromPixels(
                        spriteIndex,
                        static_cast<float>(jAnchorPixelX->valuedouble),
                        static_cast<float>(jAnchorPixelY->valuedouble));
                }
            }

            loadedAny = loadedAny || loadedFrameAnchor;
        }
    }

    cJSON_Delete(root);

    if (loadedAny)
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "Ship: anchors charges depuis '%s' (sprite1=(%.4f, %.4f))",
            jsonPath,
            this->spriteDrawAnchors[0].x,
            this->spriteDrawAnchors[0].y);
    }

    return loadedAny;
}

// =============================================================================
// Getters / Setters
// =============================================================================

bool Ship::areSpritesLoaded(void) const
{
    return this->spritesLoaded;
}

void Ship::setHealthVisual(HealthVisual health)
{
    this->healthVisual = health;
}

Ship::HealthVisual Ship::getHealthVisual(void) const
{
    return this->healthVisual;
}

bool Ship::loadSpritesFromFolder(const char* folderPath)
{
    // Runtime gameplay: le chargement ship se fait toujours depuis le storage TITLE.
    return this->loadSpritesFromFolder(folderPath, RC2D_STORAGE_TITLE);
}

void Ship::setPreviewDirection(PreviewDirection direction)
{
    DiagonalDirection diagonal = DiagonalDirection::DOWN_LEFT;
    switch (direction)
    {
    case PreviewDirection::DOWN_LEFT:
        diagonal = DiagonalDirection::DOWN_LEFT;
        break;
    case PreviewDirection::UP_RIGHT:
        diagonal = DiagonalDirection::UP_RIGHT;
        break;
    case PreviewDirection::UP_LEFT:
        diagonal = DiagonalDirection::UP_LEFT;
        break;
    case PreviewDirection::DOWN_RIGHT:
        diagonal = DiagonalDirection::DOWN_RIGHT;
        break;
    default:
        break;
    }

    // Force une direction diagonale fixe sans alterner A/B.
    this->directionUsesPair = false;
    this->directionToggle = false;
    this->directionA = diagonal;
    this->directionB = diagonal;
    this->moveDirection = MoveDirection::NONE;
}

Ship::PreviewDirection Ship::getCurrentPreviewDirection(void) const
{
    const DiagonalDirection visualDirection =
        (this->directionUsesPair && this->directionToggle) ? this->directionB : this->directionA;

    switch (visualDirection)
    {
    case DiagonalDirection::DOWN_LEFT:
        return PreviewDirection::DOWN_LEFT;
    case DiagonalDirection::UP_RIGHT:
        return PreviewDirection::UP_RIGHT;
    case DiagonalDirection::UP_LEFT:
        return PreviewDirection::UP_LEFT;
    case DiagonalDirection::DOWN_RIGHT:
        return PreviewDirection::DOWN_RIGHT;
    default:
        return PreviewDirection::DOWN_LEFT;
    }
}

void Ship::setSpeedTilesPerSecond(float speed)
{
    if (speed > 0.0f)
    {
        this->config.speedTilesPerSecond = speed;
    }
}

float Ship::getSpeedTilesPerSecond(void) const
{
    return this->config.speedTilesPerSecond;
}

void Ship::setDrawAnchorForSprite(int spriteIndex, float anchorX, float anchorY)
{
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->spriteDrawAnchors.size()))
    {
        return;
    }

    this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)].x = std::clamp(anchorX, 0.0f, 1.0f);
    this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)].y = std::clamp(anchorY, 0.0f, 1.0f);
}

bool Ship::getCurrentSpriteCenterOffsetPixels(float* outOffsetX, float* outOffsetY) const
{
    if (outOffsetX == nullptr || outOffsetY == nullptr)
    {
        return false;
    }

    const int spriteIndex = this->getCurrentSpriteIndex();
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->sprites.size()))
    {
        return false;
    }

    const RC2D_Image& sprite = this->sprites[static_cast<size_t>(spriteIndex)];
    if (sprite.sdl_texture == nullptr)
    {
        return false;
    }

    const float spriteW = static_cast<float>(sprite.sdl_texture->w);
    const float spriteH = static_cast<float>(sprite.sdl_texture->h);
    const SDL_FPoint& anchor = this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)];
    const float cameraZoom = GetCamera().getZoomFactor() * this->drawScale;

    *outOffsetX = (0.5f - anchor.x) * spriteW * cameraZoom;
    *outOffsetY = (0.5f - anchor.y) * spriteH * cameraZoom;
    return true;
}

bool Ship::getCurrentSpriteSizePixels(float* outWidth, float* outHeight) const
{
    if (outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    const int spriteIndex = this->getCurrentSpriteIndex();
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->sprites.size()))
    {
        return false;
    }

    const RC2D_Image& sprite = this->sprites[static_cast<size_t>(spriteIndex)];
    if (sprite.sdl_texture == nullptr)
    {
        return false;
    }

    *outWidth = static_cast<float>(sprite.sdl_texture->w);
    *outHeight = static_cast<float>(sprite.sdl_texture->h);
    return true;
}

void Ship::setDrawScale(float scale)
{
    if (!std::isfinite(scale) || scale <= 0.0f)
    {
        return;
    }
    this->drawScale = scale;
}

float Ship::getDrawScale(void) const
{
    return this->drawScale;
}

void Ship::setDrawAlpha(Uint8 alpha)
{
    this->drawAlpha = alpha;
}

Uint8 Ship::getDrawAlpha(void) const
{
    return this->drawAlpha;
}

// =============================================================================
// Position / Navigation
// =============================================================================

void Ship::setPositionTile(float tileX, float tileY)
{
    // Position forcee : on annule tout mouvement/path en cours
    // pour eviter un etat incoherent.
    this->tilePosition.x = tileX;
    this->tilePosition.y = tileY;

    this->tileTarget = this->tilePosition;
    this->pathTiles.clear();
    this->pathDirections.clear();
    this->pathIndex = 0;
    this->moving = false;
    this->moveDirection = MoveDirection::NONE;
}

void Ship::setPositionTileInt(int tileX, int tileY)
{
    this->setPositionTile(static_cast<float>(tileX), static_cast<float>(tileY));
}

SDL_FPoint Ship::getPositionTile(void) const
{
    return this->tilePosition;
}

void Ship::moveToTile(const Map& map, int tileX, int tileY)
{
    // --- Prepare un nouveau trajet complet vers la cible ---

    // Arrondir la position courante a la tuile la plus proche,
    // puis clamper dans les bornes de la map.
    const SDL_Point startRaw = map.roundTile(this->tilePosition.x, this->tilePosition.y);
    const SDL_Point startTile = map.clampTile(startRaw.x, startRaw.y);

    // Clamper la cible dans les bornes de la map.
    const SDL_Point goalRaw = SDL_Point{tileX, tileY};
    const SDL_Point goalTile = map.clampTile(goalRaw.x, goalRaw.y);

    // Stocker la cible en float pour le systeme de deplacement.
    this->tileTarget.x = static_cast<float>(goalTile.x);
    this->tileTarget.y = static_cast<float>(goalTile.y);

    // Reset du path courant.
    this->pathTiles.clear();
    this->pathDirections.clear();
    this->pathIndex = 0;

    // Deja sur place -> rien a faire.
    if (startTile.x == goalTile.x && startTile.y == goalTile.y)
    {
        this->moving = false;
        return;
    }

    // Construire le chemin A*.
    if (this->buildPathAStar(map, startTile, goalTile, this->pathTiles, this->pathDirections) && !this->pathTiles.empty())
    {
        this->moving = true;
        return;
    }

    // Pas de chemin trouve -> le navire ne bouge pas.
    this->moving = false;
}

SDL_FPoint Ship::getTargetTile(void) const
{
    return this->tileTarget;
}

bool Ship::isMoving(void) const
{
    return this->moving;
}

// =============================================================================
// Update (deplacement frame par frame)
// =============================================================================

void Ship::update(double dt, const Map& map)
{
    // --- Etape 0 : alimenter le sillage ocean a chaque frame ---
    // Le shader ocean utilise les echantillons de position pour tracer le sillage.
    GetOceanShader().submitWakeSample(this->runtimeShipId, map, this->tilePosition, this->moving);

    // Pas en mouvement -> rien d'autre a faire.
    if (!this->moving)
    {
        return;
    }

    // --- Etape 1 : determiner le waypoint courant ---
    // Si on a encore des waypoints dans le path, on vise le prochain.
    // Sinon, on vise la cible finale (tileTarget).
    SDL_FPoint waypointTile = this->tileTarget;

    if (this->pathIndex < this->pathTiles.size())
    {
        waypointTile.x = static_cast<float>(this->pathTiles[this->pathIndex].x);
        waypointTile.y = static_cast<float>(this->pathTiles[this->pathIndex].y);
    }

    // --- Etape 2 : calculer la distance ecran vers le waypoint ---
    // On travaille en pixels ecran (pas en tuiles) pour que la vitesse
    // soit visuellement constante quel que soit l'angle iso.
    const SDL_FPoint currentScreen = map.tileToScreenCenterFloat(this->tilePosition.x, this->tilePosition.y);
    const SDL_FPoint targetScreen = map.tileToScreenCenterFloat(waypointTile.x, waypointTile.y);

    const float deltaScreenX = targetScreen.x - currentScreen.x;
    const float deltaScreenY = targetScreen.y - currentScreen.y;
    const float distSq = (deltaScreenX * deltaScreenX) + (deltaScreenY * deltaScreenY);

    // --- Helper : completer le segment courant et avancer au suivant ---
    auto completeCurrentSegment = [&]() {
        // Snap sur le waypoint exact.
        this->tilePosition = waypointTile;

        // Alternance de sprite au passage d'un waypoint (style Seafight).
        if (this->pathIndex < this->pathTiles.size() && this->directionUsesPair)
        {
            this->directionToggle = !this->directionToggle;
        }

        // Avancer au waypoint suivant.
        if (this->pathIndex < this->pathTiles.size())
        {
            ++this->pathIndex;

            // Dernier waypoint atteint -> fin du mouvement.
            if (this->pathIndex >= this->pathTiles.size())
            {
                this->moving = false;
            }
        }
        else
        {
            this->moving = false;
        }
    };

    // --- Etape 3 : si on est deja sur le waypoint (ou tres proche) ---
    if (distSq <= 0.0001f)
    {
        completeCurrentSegment();
        return;
    }

    // --- Etape 4 : mettre a jour la direction visuelle ---
    // On utilise la direction precalculee par le A* si disponible,
    // sinon on la calcule a partir du vecteur ecran.
    MoveDirection snappedDirection = MoveDirection::NONE;

    if (this->pathIndex < this->pathDirections.size())
    {
        snappedDirection = this->pathDirections[this->pathIndex];
    }

    if (snappedDirection == MoveDirection::NONE)
    {
        snappedDirection = quantizeScreenDirection(deltaScreenX, deltaScreenY);
    }

    this->updateDirection(snappedDirection);

    // --- Etape 5 : calculer le deplacement de cette frame ---
    // Vitesse en pixels/s = vitesse en tuiles/s * largeur d'une tuile en pixels.
    const float dist = std::sqrt(distSq);
    const float speedPixelsPerSecond = this->config.speedTilesPerSecond * map.getTileWidth();
    const float stepPixels = speedPixelsPerSecond * static_cast<float>(dt);

    // Si le pas depasse la distance restante, on snap directement.
    if (stepPixels >= dist)
    {
        completeCurrentSegment();
        return;
    }

    // --- Etape 6 : avancer le navire en pixels ecran ---
    // Direction unitaire * pas en pixels.
    const float moveScreenX = (deltaScreenX / dist) * stepPixels;
    const float moveScreenY = (deltaScreenY / dist) * stepPixels;

    // --- Etape 7 : convertir le deplacement ecran en delta tuile iso ---
    // Inversion de la projection isometrique :
    //   tileDeltaX = ((moveScreenX / halfTileW) + (moveScreenY / halfTileH)) * 0.5
    //   tileDeltaY = ((moveScreenY / halfTileH) - (moveScreenX / halfTileW)) * 0.5
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;

    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return;
    }

    const float tileDeltaX = ((moveScreenX / halfTileW) + (moveScreenY / halfTileH)) * 0.5f;
    const float tileDeltaY = ((moveScreenY / halfTileH) - (moveScreenX / halfTileW)) * 0.5f;

    this->tilePosition.x += tileDeltaX;
    this->tilePosition.y += tileDeltaY;

    // --- Etape 8 : verification de convergence ---
    // Si apres le deplacement on est tres proche du waypoint (< 2 pixels),
    // on snap pour eviter des micro-oscillations.
    const SDL_FPoint newScreen = map.tileToScreenCenterFloat(this->tilePosition.x, this->tilePosition.y);
    const float remainX = targetScreen.x - newScreen.x;
    const float remainY = targetScreen.y - newScreen.y;
    const float remainDistSq = (remainX * remainX) + (remainY * remainY);

    if (remainDistSq <= 4.0f)
    {
        completeCurrentSegment();
    }
}

// =============================================================================
// Rendu
// =============================================================================

void Ship::drawSpriteCentered(const RC2D_Image& sprite, int spriteIndex, float centerX, float centerY) const
{
    // Securite : pas de texture -> rien a dessiner.
    if (sprite.sdl_texture == nullptr)
    {
        return;
    }

    // Dimensions du sprite en pixels.
    const float spriteW = static_cast<float>(sprite.sdl_texture->w);
    const float spriteH = static_cast<float>(sprite.sdl_texture->h);

    // Quad source : toute la texture.
    RC2D_Quad quad = {};
    quad.src = SDL_FRect{0.0f, 0.0f, spriteW, spriteH};

    // Le zoom camera est le seul facteur d'echelle.
    const float cameraZoom = GetCamera().getZoomFactor() * this->drawScale;

    // Anchor normalise [0..1] -> position en pixels dans le sprite.
    const SDL_FPoint& anchor = this->spriteDrawAnchors[static_cast<size_t>(spriteIndex)];
    const float anchorPixelX = spriteW * anchor.x;
    const float anchorPixelY = spriteH * anchor.y;

    // Position finale :
    // Le point (centerX, centerY) est le centre de la tuile a l'ecran.
    // On retire l'anchor (multipliee par le zoom) pour que le point pivot
    // du sprite coincide avec le centre de la tuile.
    const float drawX = centerX - (anchorPixelX * cameraZoom);
    const float drawY = centerY - (anchorPixelY * cameraZoom);

    Uint8 previousAlpha = 255;
    SDL_GetTextureAlphaMod(sprite.sdl_texture, &previousAlpha);
    SDL_SetTextureAlphaMod(sprite.sdl_texture, this->drawAlpha);
    rc2d_graphics_drawQuad(
        (RC2D_Image*)&sprite,
        &quad,
        drawX,
        drawY,
        0.0,
        cameraZoom,
        cameraZoom,
        -1.0f,
        -1.0f,
        false,
        false);
    SDL_SetTextureAlphaMod(sprite.sdl_texture, previousAlpha);
}

void Ship::draw(const Map& map) const
{
    // Dessin defensif : on sort si un prerequis visuel manque.
    const int spriteIndex = this->getCurrentSpriteIndex();
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(this->sprites.size()))
    {
        return;
    }

    const RC2D_Image& sprite = this->sprites[static_cast<size_t>(spriteIndex)];
    if (sprite.sdl_texture == nullptr)
    {
        return;
    }

    // Projection de la position tuile courante vers l'ecran.
    const SDL_FPoint center = map.tileToScreenCenterFloat(this->tilePosition.x, this->tilePosition.y);

    this->drawSpriteCentered(sprite, spriteIndex, center.x, center.y);
}
