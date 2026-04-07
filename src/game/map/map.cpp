#include "game/map/map.h"

#include <algorithm>
#include <cmath>

#include "core/context.h"

// =============================================================================
// Constructeur / Destructeur
// =============================================================================

Map::Map(void)
    : widthTiles(Map::WORLD_SIZE_TILES),    // Grille carree : largeur = WORLD_SIZE_TILES tuiles.
      heightTiles(Map::WORLD_SIZE_TILES),    // Grille carree : hauteur = WORLD_SIZE_TILES tuiles.
      tileWidth(48.0f),                      // Largeur de base d'une tuile iso (avant zoom).
      tileHeight(32.0f),                     // Hauteur de base d'une tuile iso (avant zoom).
      originX(0.0f),                         // Origine ecran X initiale (sera recalculee par centerOnRect/centerOnTileInRect).
      originY(0.0f),                         // Origine ecran Y initiale.
      rect{0.0f, Map::MAP_TOP_UI_MARGIN_PX, 1.0f, 1.0f}, // Rectangle de rendu monde : demarre sous la marge UI haute.
      blockedTiles(static_cast<size_t>(Map::WORLD_SIZE_TILES * Map::WORLD_SIZE_TILES), static_cast<Uint8>(0)) // Grille collision : tout traversable.
{
}

Map::~Map(void)
{
}

// =============================================================================
// Conversions Secteur <-> Tuile
// =============================================================================

SDL_FPoint Map::tileToSectorFloat(float tileX, float tileY) const
{
    // --- Changement de base : tuile iso -> coordonnees secteur flottantes ---
    //
    // La grille secteurs est ancree a (SECTOR_BASE_X, SECTOR_BASE_Y) dans la map technique.
    // On normalise d'abord par rapport a cette ancre, puis on divise par le pas inter-secteur.
    //
    // 'a' represente l'avancement le long de l'axe tileX (en nombre de secteurs).
    // 'b' represente l'avancement le long de l'axe tileY (en nombre de secteurs).
    const float a =
        (tileX - static_cast<float>(Map::SECTOR_BASE_X)) /
        static_cast<float>(Map::SECTOR_STEP);

    const float b =
        (tileY - static_cast<float>(Map::SECTOR_BASE_Y)) /
        static_cast<float>(Map::SECTOR_STEP);

    // La grille secteurs est tournee de 45 degres par rapport a la grille tuiles.
    // Pour retrouver les coordonnees secteur a partir de (a, b), on applique
    // la rotation inverse :
    //   sectorX = (a - b) / 2   -> axe horizontal secteurs (00..59)
    //   sectorY = (a + b) / 2   -> axe vertical secteurs   (AA..CH)
    SDL_FPoint p{};
    p.x = (a - b) * 0.5f;
    p.y = (a + b) * 0.5f;
    return p;
}

SDL_Point Map::tileToSectorNearest(float tileX, float tileY) const
{
    // Calcule le secteur flottant, puis arrondit au secteur entier le plus proche.
    const SDL_FPoint f = this->tileToSectorFloat(tileX, tileY);

    SDL_Point p{};
    p.x = static_cast<int>(std::lround(f.x));
    p.y = static_cast<int>(std::lround(f.y));
    return p;
}

SDL_Point Map::sectorToTile(int sectorX, int sectorY) const
{
    // --- Conversion inverse : secteur entier -> tuile iso ---
    //
    // On clamp d'abord le secteur dans les bornes valides [0..59].
    sectorX = std::clamp(sectorX, 0, Map::NUM_SECTORS_X - 1);
    sectorY = std::clamp(sectorY, 0, Map::NUM_SECTORS_Y - 1);

    // Formule de projection secteur -> tuile :
    //   tileX = BASE_X + (sectorX + sectorY) * STEP
    //   tileY = BASE_Y + (sectorY - sectorX) * STEP
    //
    // Quand sectorX augmente (droite ecran), tileX augmente et tileY diminue.
    // Quand sectorY augmente (bas ecran),    tileX augmente et tileY augmente.
    SDL_Point p{};
    p.x = Map::SECTOR_BASE_X + (sectorX + sectorY) * Map::SECTOR_STEP;
    p.y = Map::SECTOR_BASE_Y + (sectorY - sectorX) * Map::SECTOR_STEP;
    return p;
}

bool Map::isInsideSector(int sectorX, int sectorY) const
{
    // Verifie que le secteur est dans la grille valide [0..59] x [0..59].
    return (
        sectorX >= 0 && sectorX < Map::NUM_SECTORS_X &&
        sectorY >= 0 && sectorY < Map::NUM_SECTORS_Y);
}

SDL_Point Map::clampSector(int sectorX, int sectorY) const
{
    // Force le secteur dans les bornes valides [0..59] x [0..59].
    SDL_Point p{};
    p.x = std::clamp(sectorX, 0, Map::NUM_SECTORS_X - 1);
    p.y = std::clamp(sectorY, 0, Map::NUM_SECTORS_Y - 1);
    return p;
}

// =============================================================================
// Configuration de la map
// =============================================================================

void Map::setMapSize(int width, int height)
{
    // Garde-fou : dimensions minimales = 1x1 pour eviter une map vide.
    if (width < 1)
    {
        width = 1;
    }

    if (height < 1)
    {
        height = 1;
    }

    this->widthTiles = width;
    this->heightTiles = height;

    // On recree la grille de collisions a la nouvelle taille.
    // Toutes les tuiles repartent en "traversable" (0).
    this->blockedTiles.assign(static_cast<size_t>(this->widthTiles * this->heightTiles), static_cast<Uint8>(0));
}

void Map::setTileSize(float width, float height)
{
    // On refuse une taille <= 0 pour eviter des divisions par zero
    // dans les conversions tile <-> ecran.
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    this->tileWidth = width;
    this->tileHeight = height;
}

void Map::setOrigin(float x, float y)
{
    // Definit directement l'origine ecran de la projection iso.
    // La tuile (0,0) sera dessinee a cette position ecran.
    this->originX = x;
    this->originY = y;
}

// =============================================================================
// Rectangle de rendu / mise a jour
// =============================================================================

void Map::updateMapRect(const SDL_FRect& gameScreenRect)
{
    // Calcule le rectangle de rendu monde a partir du game screen logique.
    // On travaille en coordonnees logiques (pas physiques) pour etre
    // independant de la resolution reelle, du plein ecran et de l'overscan.

    // Largeur et hauteur disponibles, min 1 pixel pour eviter un rect degenere.
    const float availableW = (std::max)(gameScreenRect.w, 1.0f);
    const float availableH = (std::max)(gameScreenRect.h, 1.0f);

    // X : bord gauche du game screen.
    this->rect.x = gameScreenRect.x;

    // Y : on descend de MAP_TOP_UI_MARGIN_PX (35px) pour laisser place a la GUI haute.
    this->rect.y = gameScreenRect.y + Map::MAP_TOP_UI_MARGIN_PX;

    // W : toute la largeur disponible.
    this->rect.w = availableW;

    // H : hauteur restante apres retrait des marges haute (35px) et basse (70px).
    this->rect.h = (std::max)(
        availableH - Map::MAP_TOP_UI_MARGIN_PX - Map::MAP_BOTTOM_UI_MARGIN_PX,
        1.0f);
}

void Map::update(void)
{
    // Synchronise le rectangle map avec le game screen courant.
    this->updateMapRect(GetGameScreen().rect);
}

// =============================================================================
// Centrage de la map / camera
// =============================================================================

void Map::centerOnRect(const SDL_FRect& targetRect)
{
    // --- Centre le losange isometrique complet de la map dans un rectangle ecran ---

    // Demi-dimensions d'une tuile iso (avec zoom applique).
    const float halfTileW = this->getTileWidth() * 0.5f;
    const float halfTileH = this->getTileHeight() * 0.5f;

    // Calcul des bornes ecran du losange global de la map.
    //
    // En projection iso, la tuile la plus a gauche est (0, heightTiles-1)
    // et la plus a droite est (widthTiles-1, 0).
    // La projection ecran X d'une tuile (tx, ty) est : (tx - ty) * halfTileW.
    //
    // minCenterX = projection de la tuile (0, heightTiles-1) = -(heightTiles-1) * halfTileW
    // maxCenterX = projection de la tuile (widthTiles-1, 0)  = +(widthTiles-1) * halfTileW
    const float minCenterX = -(static_cast<float>(this->heightTiles - 1) * halfTileW);
    const float maxCenterX =  (static_cast<float>(this->widthTiles - 1) * halfTileW);

    // La projection ecran Y d'une tuile (tx, ty) est : (tx + ty) * halfTileH.
    // minCenterY = projection de la tuile (0, 0) = 0
    // maxCenterY = projection de la tuile (widthTiles-1, heightTiles-1)
    const float minCenterY = 0.0f;
    const float maxCenterY = static_cast<float>(this->widthTiles + this->heightTiles - 2) * halfTileH;

    // Centre geometrique du losange map en ecran.
    const float mapCenterX = (minCenterX + maxCenterX) * 0.5f;
    const float mapCenterY = (minCenterY + maxCenterY) * 0.5f;

    // On translate l'origine pour que le centre du losange
    // coincide avec le centre du rectangle cible.
    this->originX = (targetRect.x + (targetRect.w * 0.5f)) - mapCenterX;
    this->originY = (targetRect.y + (targetRect.h * 0.5f)) - mapCenterY;
}

void Map::centerOnTileInRect(float tileX, float tileY, const SDL_FRect& targetRect)
{
    // --- Calcule l'origine pour qu'une tuile donnee apparaisse au centre du rectangle ---

    // Centre ecran du rectangle cible.
    const float screenCenterX = targetRect.x + (targetRect.w * 0.5f);
    const float screenCenterY = targetRect.y + (targetRect.h * 0.5f);

    // Demi-dimensions d'une tuile iso (avec zoom).
    const float halfTileW = this->getTileWidth() * 0.5f;
    const float halfTileH = this->getTileHeight() * 0.5f;

    // La projection iso donne :
    //   screenX = originX + (tileX - tileY) * halfTileW
    //   screenY = originY + (tileX + tileY) * halfTileH
    //
    // On veut que screenX = screenCenterX et screenY = screenCenterY,
    // donc on isole originX et originY :
    this->originX = screenCenterX - ((tileX - tileY) * halfTileW);
    this->originY = screenCenterY - ((tileX + tileY) * halfTileH);
}

// =============================================================================
// Conversions Tuile <-> Ecran
// =============================================================================

SDL_FPoint Map::tileToScreenCenter(int tileX, int tileY) const
{
    // Wrapper de commodite : convertit les int en float et delegue.
    return tileToScreenCenterFloat(static_cast<float>(tileX), static_cast<float>(tileY));
}

SDL_FPoint Map::tileToScreenCenterFloat(float tileX, float tileY) const
{
    // --- Projection isometrique : tuile -> ecran ---
    //
    // Formule standard d'une projection isometrique 2:1 :
    //   screenX = originX + (tileX - tileY) * halfTileW
    //   screenY = originY + (tileX + tileY) * halfTileH
    //
    // (tileX - tileY) donne la composante horizontale : quand tileX augmente
    // on va a droite, quand tileY augmente on va a gauche.
    //
    // (tileX + tileY) donne la composante verticale : les deux axes
    // tirent vers le bas.
    const float halfTileW = this->getTileWidth() * 0.5f;
    const float halfTileH = this->getTileHeight() * 0.5f;

    SDL_FPoint result = {};
    result.x = this->originX + ((tileX - tileY) * halfTileW);
    result.y = this->originY + ((tileX + tileY) * halfTileH);
    return result;
}

SDL_FPoint Map::screenToTile(float screenX, float screenY) const
{
    // --- Projection inverse : ecran -> tuile flottante ---
    //
    // On inverse le systeme lineaire de tileToScreenCenterFloat.

    const float halfTileW = this->getTileWidth() * 0.5f;
    const float halfTileH = this->getTileHeight() * 0.5f;

    SDL_FPoint result = {};

    // Securite : si les tuiles ont une dimension nulle, on ne peut pas inverser.
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return result;
    }

    // On normalise les coordonnees ecran par rapport a l'origine et aux demi-tuiles.
    //   dx = (screenX - originX) / halfTileW  =>  tileX - tileY
    //   dy = (screenY - originY) / halfTileH  =>  tileX + tileY
    const float dx = (screenX - this->originX) / halfTileW;
    const float dy = (screenY - this->originY) / halfTileH;

    // Inversion du systeme :
    //   dx = tileX - tileY
    //   dy = tileX + tileY
    //
    //   tileX = (dx + dy) / 2
    //   tileY = (dy - dx) / 2
    result.x = (dx + dy) * 0.5f;
    result.y = (dy - dx) * 0.5f;
    return result;
}

SDL_Point Map::screenToTileNearest(float screenX, float screenY) const
{
    // Convertit ecran -> tuile flottante, puis arrondit a la tuile entiere la plus proche.
    const SDL_FPoint tileFloat = screenToTile(screenX, screenY);
    return roundTile(tileFloat.x, tileFloat.y);
}

SDL_Point Map::roundTile(float tileX, float tileY) const
{
    // Arrondi au plus proche via lround (arrondi bancaire standard).
    // Le cast via double est necessaire car std::lround attend un double
    // pour garantir la precision d'arrondi sur les valeurs proches de .5.
    SDL_Point tile = {};
    tile.x = static_cast<int>(std::lround(static_cast<double>(tileX)));
    tile.y = static_cast<int>(std::lround(static_cast<double>(tileY)));
    return tile;
}

// =============================================================================
// Bornes et collision
// =============================================================================

SDL_Point Map::clampTile(int tileX, int tileY) const
{
    SDL_Point result = {};

    // Si la map est vide, on retourne (0, 0) par securite.
    if (this->widthTiles <= 0 || this->heightTiles <= 0)
    {
        return result;
    }

    // Force la tuile dans [0, widthTiles-1] x [0, heightTiles-1].
    tileX = std::clamp(tileX, 0, this->widthTiles - 1);
    tileY = std::clamp(tileY, 0, this->heightTiles - 1);

    result.x = tileX;
    result.y = tileY;
    return result;
}

bool Map::isInside(int tileX, int tileY) const
{
    // Verifie que la tuile est dans les bornes [0..widthTiles) x [0..heightTiles).
    return (tileX >= 0 && tileX < this->widthTiles && tileY >= 0 && tileY < this->heightTiles);
}

bool Map::setTileBlocked(int tileX, int tileY, bool blocked)
{
    // Ecriture protegee : on refuse une tuile hors map.
    if (!this->isInside(tileX, tileY))
    {
        return false;
    }

    // Ecrit 1 (bloquee) ou 0 (traversable) dans la grille de collision.
    this->blockedTiles[static_cast<size_t>(this->tileIndex(tileX, tileY))] = blocked ? static_cast<Uint8>(1) : static_cast<Uint8>(0);
    return true;
}

bool Map::isTileBlocked(int tileX, int tileY) const
{
    // Politique defensive : une tuile hors map est consideree bloquee.
    if (!this->isInside(tileX, tileY))
    {
        return true;
    }

    return this->blockedTiles[static_cast<size_t>(this->tileIndex(tileX, tileY))] != 0;
}

void Map::clearBlockedTiles(void)
{
    // Remet toutes les tuiles en "traversable" (0).
    for (size_t i = 0; i < this->blockedTiles.size(); ++i)
    {
        this->blockedTiles[i] = static_cast<Uint8>(0);
    }
}

// =============================================================================
// Utilitaires internes
// =============================================================================

int Map::tileIndex(int tileX, int tileY) const
{
    // Index lineaire row-major : chaque ligne Y contient widthTiles tuiles.
    return (tileY * this->widthTiles) + tileX;
}

// =============================================================================
// Accesseurs
// =============================================================================

int Map::getWidthTiles(void) const
{
    return this->widthTiles;
}

int Map::getHeightTiles(void) const
{
    return this->heightTiles;
}

float Map::getTileWidth(void) const
{
    // La taille retournee integre le zoom camera.
    // Ex: tileWidth=48, zoom=1.3 => retourne 62.4 pixels.
    return this->tileWidth * GetCamera().getZoomFactor();
}

float Map::getTileHeight(void) const
{
    // Idem getTileWidth, mais sur la hauteur.
    return this->tileHeight * GetCamera().getZoomFactor();
}

float Map::getOriginX(void) const
{
    return this->originX;
}

float Map::getOriginY(void) const
{
    return this->originY;
}
