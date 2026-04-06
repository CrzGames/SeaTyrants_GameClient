#include "game/ui/scroll-bar-overlay.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "core/context.h"
#include "game/camera.h"
#include "game/map/map.h"

ScrollBarOverlay::ScrollBarOverlay(void)
    : barThickness(30.0f),
      cornerSize(50.0f),
      barColor{95, 110, 125, 110},
      cornerColor{105, 120, 135, 125},
      textColor{220, 230, 240, 215},
      font{},
      activeBar(0)
{
}

ScrollBarOverlay::~ScrollBarOverlay(void)
{
}

void ScrollBarOverlay::load(void)
{
    this->font = rc2d_graphics_openFontFromStorage(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);
}

void ScrollBarOverlay::unload(void)
{
    rc2d_graphics_closeFont(&this->font);
}

void ScrollBarOverlay::getColumnLabel(int index, char* out, int outSize) const
{
    // 0-25 -> AA-AZ, 26-33 -> BA-BH.
    if (index < 0 || index >= NUM_COLS)
    {
        out[0] = '\0';
        return;
    }

    char first = 'A' + static_cast<char>(index / 26);
    char second = 'A' + static_cast<char>(index % 26);
    snprintf(out, static_cast<size_t>(outSize), "%c%c", first, second);
}

void ScrollBarOverlay::getRowLabel(int index, char* out, int outSize) const
{
    if (index < 0 || index >= NUM_ROWS)
    {
        out[0] = '\0';
        return;
    }

    snprintf(out, static_cast<size_t>(outSize), "%02d", index);
}

int ScrollBarOverlay::hitTestBar(float x, float y, const SDL_FRect& screenRect) const
{
    const float left   = screenRect.x;
    const float top    = screenRect.y;
    const float right  = screenRect.x + screenRect.w;
    const float bottom = screenRect.y + screenRect.h;

    const float t = this->barThickness;
    const float c = (std::max)(this->cornerSize, t);

    if (x < left || x > right || y < top || y > bottom)
    {
        return 0;
    }

    // --- Coins d'abord ---
    // 5 = haut-gauche
    if (x >= left && x <= left + c && y >= top && y <= top + c)
    {
        return 5;
    }

    // 6 = haut-droite
    if (x >= right - c && x <= right && y >= top && y <= top + c)
    {
        return 6;
    }

    // 7 = bas-gauche
    if (x >= left && x <= left + c && y >= bottom - c && y <= bottom)
    {
        return 7;
    }

    // 8 = bas-droite
    if (x >= right - c && x <= right && y >= bottom - c && y <= bottom)
    {
        return 8;
    }

    // --- Barres ---
    // 1 = haut
    if (y >= top && y <= top + t && x >= left + c && x <= right - c)
    {
        return 1;
    }

    // 2 = bas
    if (y >= bottom - t && y <= bottom && x >= left + c && x <= right - c)
    {
        return 2;
    }

    // 3 = gauche
    if (x >= left && x <= left + t && y >= top + c && y <= bottom - c)
    {
        return 3;
    }

    // 4 = droite
    if (x >= right - t && x <= right && y >= top + c && y <= bottom - c)
    {
        return 4;
    }

    return 0;
}

void ScrollBarOverlay::update(double dt, Camera& camera, const Map& map, const SDL_FRect& screenRect)
{
    // Ignore l'update si le bouton gauche n'est pas maintenu.
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->activeBar = 0;
        return;
    }

    // Récupère la position actuelle de la souris.
    const float mx = rc2d_mouse_getX();
    const float my = rc2d_mouse_getY();

    // On capture la zone au debut du clic, puis on la conserve
    // tant que le bouton reste enfonce.
    if (this->activeBar == 0)
    {
        this->activeBar = this->hitTestBar(mx, my, screenRect);
    }

    // Si aucune barre n'est active, on ignore l'update.
    if (this->activeBar == 0)
    {
        return;
    }

    // Deplace la camera selon la barre active et le temps ecoule.
    const float sectorStep = Camera::CAMERA_SCROLL_SPEED_SECTORS * static_cast<float>(dt);
    const float diagonalFactor = Camera::CAMERA_DIAGONAL_FACTOR;

    float deltaSectorX = 0.0f;
    float deltaSectorY = 0.0f;
    switch (this->activeBar)
    {
        case 1: // Haut
            deltaSectorY = -sectorStep;
            break;

        case 2: // Bas
            deltaSectorY = sectorStep;
            break;

        case 3: // Gauche
            deltaSectorX = -sectorStep;
            break;

        case 4: // Droite
            deltaSectorX = sectorStep;
            break;

        case 5: // Haut-gauche
            deltaSectorX = -sectorStep * diagonalFactor;
            deltaSectorY = -sectorStep * diagonalFactor;
            break;

        case 6: // Haut-droite
            deltaSectorX = sectorStep * diagonalFactor;
            deltaSectorY = -sectorStep * diagonalFactor;
            break;

        case 7: // Bas-gauche
            deltaSectorX = -sectorStep * diagonalFactor;
            deltaSectorY = sectorStep * diagonalFactor;
            break;

        case 8: // Bas-droite
            deltaSectorX = sectorStep * diagonalFactor;
            deltaSectorY = sectorStep * diagonalFactor;
            break;

        default:
            return;
    }

    // Conversion secteur -> tuiles isometriques.
    const float deltaTileX = (deltaSectorX + deltaSectorY) * static_cast<float>(Map::SECTOR_STEP);
    const float deltaTileY = (deltaSectorY - deltaSectorX) * static_cast<float>(Map::SECTOR_STEP);

    // Deplace la camera et applique le changement a la map.
    camera.moveCameraTiles(deltaTileX, deltaTileY, map, screenRect);
    camera.applyToMap(const_cast<Map&>(map), screenRect);
}

void ScrollBarOverlay::draw(const SDL_FRect& screenRect, const Map& map)
{
    const float left   = screenRect.x;
    const float top    = screenRect.y;
    const float right  = screenRect.x + screenRect.w;
    const float bottom = screenRect.y + screenRect.h;

    const float t = this->barThickness;
    const float c = (std::max)(this->cornerSize, t);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // -------------------------------------------------------------------------
    // Barres de scroll
    // -------------------------------------------------------------------------
    rc2d_graphics_setColor(this->barColor);

    SDL_FRect barTop = {
        left + c,
        top,
        (right - left) - (2.0f * c),
        t
    };
    rc2d_graphics_rectangle("fill", &barTop);

    SDL_FRect barBottom = {
        left + c,
        bottom - t,
        (right - left) - (2.0f * c),
        t
    };
    rc2d_graphics_rectangle("fill", &barBottom);

    SDL_FRect barLeft = {
        left,
        top + c,
        t,
        (bottom - top) - (2.0f * c)
    };
    rc2d_graphics_rectangle("fill", &barLeft);

    SDL_FRect barRight = {
        right - t,
        top + c,
        t,
        (bottom - top) - (2.0f * c)
    };
    rc2d_graphics_rectangle("fill", &barRight);

    // -------------------------------------------------------------------------
    // Coins
    // -------------------------------------------------------------------------
    rc2d_graphics_setColor(this->cornerColor);

    SDL_FRect cornerTL = {left, top, this->cornerSize, this->cornerSize};
    SDL_FRect cornerTR = {right - this->cornerSize, top, this->cornerSize, this->cornerSize};
    SDL_FRect cornerBL = {left, bottom - this->cornerSize, this->cornerSize, this->cornerSize};
    SDL_FRect cornerBR = {right - this->cornerSize, bottom - this->cornerSize, this->cornerSize, this->cornerSize};

    rc2d_graphics_rectangle("fill", &cornerTL);
    rc2d_graphics_rectangle("fill", &cornerTR);
    rc2d_graphics_rectangle("fill", &cornerBL);
    rc2d_graphics_rectangle("fill", &cornerBR);

    // -------------------------------------------------------------------------
    // Coordonnees
    // -------------------------------------------------------------------------
    if (this->font.sdl_font == nullptr)
    {
        return;
    }

    const float barInnerLeft   = left + c;
    const float barInnerRight  = right - c;
    const float barInnerTop    = top + c;
    const float barInnerBottom = bottom - c;

    // -------------------------------------------------------------------------
    // Labels 00-59 sur la barre du haut
    // -------------------------------------------------------------------------
    for (int i = 0; i < Map::NUM_SECTORS_X; ++i)
    {
        const SDL_Point tile = map.sectorToTile(i, 0);
        const SDL_FPoint p = map.tileToScreenCenterFloat(
            static_cast<float>(tile.x),
            static_cast<float>(tile.y));

        if (p.x < barInnerLeft || p.x > barInnerRight)
        {
            continue;
        }

        char label[4];
        this->getRowLabel(i, label, sizeof(label));

        RC2D_Text text = rc2d_graphics_createText(&this->font, label);
        text.color = this->textColor;
        rc2d_graphics_setTextColor(&text);

        int textW = 0;
        int textH = 0;
        rc2d_graphics_getTextSize(&text, &textW, &textH);

        const float drawX = p.x - (static_cast<float>(textW) * 0.5f);
        const float drawY = top + ((t - static_cast<float>(textH)) * 0.5f);

        rc2d_graphics_drawText(&text, drawX, drawY);
        rc2d_graphics_destroyText(&text);
    }

    // -------------------------------------------------------------------------
    // Labels AA-CH sur la barre de gauche
    // -------------------------------------------------------------------------
    for (int i = 0; i < Map::NUM_SECTORS_Y; ++i)
    {
        const SDL_Point tile = map.sectorToTile(0, i);
        const SDL_FPoint p = map.tileToScreenCenterFloat(
            static_cast<float>(tile.x),
            static_cast<float>(tile.y));

        if (p.y < barInnerTop || p.y > barInnerBottom)
        {
            continue;
        }

        char label[4];
        this->getColumnLabel(i, label, sizeof(label));

        RC2D_Text text = rc2d_graphics_createText(&this->font, label);
        text.color = this->textColor;
        rc2d_graphics_setTextColor(&text);

        int textW = 0;
        int textH = 0;
        rc2d_graphics_getTextSize(&text, &textW, &textH);

        const float drawX = left + ((t - static_cast<float>(textW)) * 0.5f);
        const float drawY = p.y - (static_cast<float>(textH) * 0.5f);

        rc2d_graphics_drawText(&text, drawX, drawY);
        rc2d_graphics_destroyText(&text);
    }
}

bool ScrollBarOverlay::handleClick(float x, float y, const SDL_FRect& screenRect)
{
    this->activeBar = this->hitTestBar(x, y, screenRect);
    RC2D_log(RC2D_LOG_DEBUG, "Scroll bar clicked: %d", this->activeBar);
    return (this->activeBar != 0);
}

std::string ScrollBarOverlay::getMapCoordFromTile(int tileX, int tileY) const
{
    const Map& map = GetCurrentMap();

    const SDL_Point sector = map.tileToSectorNearest(
        static_cast<float>(tileX),
        static_cast<float>(tileY));

    if (!map.isInsideSector(sector.x, sector.y))
    {
        return "??-??";
    }

    char rowLabel[4];
    char colLabel[4];

    this->getRowLabel(sector.x, rowLabel, sizeof(rowLabel));
    this->getColumnLabel(sector.y, colLabel, sizeof(colLabel));

    return std::string(rowLabel) + "-" + std::string(colLabel);
}