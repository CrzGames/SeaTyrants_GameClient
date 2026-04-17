#include "game/camera.h"

#include <algorithm>

#include "game/map/map.h"

// =============================================================================
// Constructeur / Destructeur
// =============================================================================

Camera::Camera()
    : cameraTileX(0.0f),      // Centre camera X en coordonnee tuile (sera recalcule par resetCamera).
      cameraTileY(0.0f),      // Centre camera Y en coordonnee tuile.
      zoomFactor(Camera::CAMERA_ZOOM_MAX_FACTOR), // Zoom initial = 100 % (taille native des tuiles).
      minZoomFactor(Camera::CAMERA_ZOOM_MIN_FACTOR), // Dezoom max autorise (40 %).
      maxZoomFactor(Camera::CAMERA_ZOOM_MAX_FACTOR)  // Zoom max autorise (100 %).
{
}

Camera::~Camera()
{
}

// =============================================================================
// Positionnement de la camera
// =============================================================================

void Camera::resetCamera(const Map& map, const SDL_FRect& viewportRect)
{
    // Place la camera au centre geometrique de la grille de tuiles.
    // (widthTiles-1) * 0.5 donne la tuile centrale, pas le pixel.
    this->cameraTileX = static_cast<float>(map.getWidthTiles() - 1) * 0.5f;
    this->cameraTileY = static_cast<float>(map.getHeightTiles() - 1) * 0.5f;

    // On applique le clamp pour rester dans les bornes secteurs visibles.
    this->clampCameraToMap(map, viewportRect);
}

void Camera::centerCameraOnTile(float tileX, float tileY, const Map& map, const SDL_FRect& viewportRect)
{
    // Affecte directement la position camera, puis clamp dans les bornes.
    this->cameraTileX = tileX;
    this->cameraTileY = tileY;

    this->clampCameraToMap(map, viewportRect);
}

void Camera::moveCameraTiles(float deltaTileX, float deltaTileY, const Map& map, const SDL_FRect& viewportRect)
{
    // Deplacement relatif en tuiles, puis clamp.
    this->cameraTileX += deltaTileX;
    this->cameraTileY += deltaTileY;

    this->clampCameraToMap(map, viewportRect);
}

// =============================================================================
// Zoom
// =============================================================================

void Camera::setZoomFactor(float value)
{
    // Clamp entre min et max pour eviter un zoom trop extreme.
    this->zoomFactor = std::clamp(value, this->minZoomFactor, this->maxZoomFactor);
}

float Camera::getZoomFactor(void) const
{
    return this->zoomFactor;
}

// =============================================================================
// Application a la map
// =============================================================================

void Camera::applyToMap(Map& map, const SDL_FRect& viewportRect)
{
    // 1) Clamp la camera dans les bornes secteurs.
    this->clampCameraToMap(map, viewportRect);

    // 2) Recalcule l'origine de la map pour que la tuile camera
    //    soit au centre du viewport.
    map.centerOnTileInRect(this->cameraTileX, this->cameraTileY, viewportRect);
}

void Camera::update(Map& map, const SDL_FRect& viewportRect)
{
    // Alias pour les boucles d'update gameplay.
    this->applyToMap(map, viewportRect);
}

// =============================================================================
// Clamp de la camera dans les bornes de la zone secteurs
// =============================================================================

void Camera::clampCameraToMap(const Map& map, const SDL_FRect& viewportRect)
{
    // --- But : empecher la camera de montrer du vide au-dela de la zone secteurs ---
    //
    // On travaille en espace isometrique (u, v) plutot qu'en (tileX, tileY)
    // car les axes u et v correspondent directement aux axes ecran (horizontal / vertical).
    // Cela rend le clamp intuitif : on limite chaque axe ecran independamment.

    const int mapWidth = map.getWidthTiles();
    const int mapHeight = map.getHeightTiles();

    // Securite : map vide -> camera a l'origine.
    if (mapWidth <= 0 || mapHeight <= 0)
    {
        this->cameraTileX = 0.0f;
        this->cameraTileY = 0.0f;
        return;
    }

    // Demi-dimensions d'une tuile iso (avec zoom applique).
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;

    // Securite : tuiles de taille nulle -> pas de clamp possible.
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        this->cameraTileX = 0.0f;
        this->cameraTileY = 0.0f;
        return;
    }

    // --- Etape 1 : calculer les bornes (u, v) de la zone secteurs ---
    //
    // Rappel :
    //   u = tileX - tileY   (axe horizontal ecran, unites tuiles)
    //   v = tileX + tileY   (axe vertical ecran, unites tuiles)
    //
    // Le secteur (0, 0) est a la tuile (SECTOR_BASE_X, SECTOR_BASE_Y).
    // Le secteur (59, 0) est a la tuile (BASE_X + 59*STEP, BASE_Y - 59*STEP).
    // Le secteur (0, 59) est a la tuile (BASE_X + 59*STEP, BASE_Y + 59*STEP).
    //
    // On calcule les extremes u et v de cette grille.

    // u min/max : variation horizontale de la grille secteurs.
    const float minU =
        static_cast<float>(Map::SECTOR_BASE_X - Map::SECTOR_BASE_Y);

    const float maxU =
        minU + static_cast<float>((Map::NUM_SECTORS_X - 1) * 2 * Map::SECTOR_STEP);

    // v min/max : variation verticale de la grille secteurs.
    const float minV =
        static_cast<float>(Map::SECTOR_BASE_X + Map::SECTOR_BASE_Y);

    const float maxV =
        minV + static_cast<float>((Map::NUM_SECTORS_Y - 1) * 2 * Map::SECTOR_STEP);

    // --- Etape 2 : convertir la taille du viewport en espace (u, v) ---
    //
    // halfViewU = nombre de tuiles visibles horizontalement depuis le centre.
    // halfViewV = nombre de tuiles visibles verticalement depuis le centre.
    //
    // Division par halfTileW/H car 1 unite u/v = halfTileW/H pixels ecran.
    const float halfViewU = (viewportRect.w * 0.5f) / halfTileW;
    const float halfViewV = (viewportRect.h * 0.5f) / halfTileH;

    // --- Etape 3 : convertir la camera en (u, v) ---
    float cameraU = this->cameraTileX - this->cameraTileY;
    float cameraV = this->cameraTileX + this->cameraTileY;

    // --- Etape 4 : clamper camera U et V ---
    //
    // Le centre de la camera doit rester assez loin du bord pour que
    // le viewport ne deborde pas de la zone secteurs.
    //
    // allowedMin = minU + halfViewU  (le bord gauche du viewport touche minU)
    // allowedMax = maxU - halfViewU  (le bord droit du viewport touche maxU)
    //
    // Si la zone est plus petite que le viewport (allowedMin > allowedMax),
    // on centre simplement la camera sur la zone.
    const float allowedMinU = minU + halfViewU;
    const float allowedMaxU = maxU - halfViewU;
    const float allowedMinV = minV + halfViewV;
    const float allowedMaxV = maxV - halfViewV;

    if (allowedMinU <= allowedMaxU)
    {
        cameraU = std::clamp(cameraU, allowedMinU, allowedMaxU);
    }
    else
    {
        // Viewport plus large que la zone secteurs : centrer.
        cameraU = (minU + maxU) * 0.5f;
    }

    if (allowedMinV <= allowedMaxV)
    {
        cameraV = std::clamp(cameraV, allowedMinV, allowedMaxV);
    }
    else
    {
        // Viewport plus haut que la zone secteurs : centrer.
        cameraV = (minV + maxV) * 0.5f;
    }

    // --- Etape 5 : reconvertir (u, v) en (tileX, tileY) ---
    //
    // tileX = (u + v) / 2
    // tileY = (v - u) / 2
    this->cameraTileX = (cameraU + cameraV) * 0.5f;
    this->cameraTileY = (cameraV - cameraU) * 0.5f;

    // --- Etape 6 : garde-fou final ---
    // On borne dans les limites absolues de la grille technique.
    this->cameraTileX = std::clamp(this->cameraTileX, 0.0f, static_cast<float>(mapWidth - 1));
    this->cameraTileY = std::clamp(this->cameraTileY, 0.0f, static_cast<float>(mapHeight - 1));
}
