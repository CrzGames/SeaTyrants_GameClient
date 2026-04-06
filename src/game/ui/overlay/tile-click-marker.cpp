#include "game/ui/overlay/tile-click-marker.h"

#include <cmath>

TileClickMarker::TileClickMarker(void)
    : visible(false),
      tile{0, 0},
      elapsedSeconds(0.0),
      durationSeconds(0.85),
      pulseAmplitude(0.04f),
      pulseSpeed(22.0f),
      fillColor{233, 245, 255, 90},
      lineColor{255, 255, 255, 245}
{
}

TileClickMarker::~TileClickMarker(void)
{
}

void TileClickMarker::show(int tileX, int tileY)
{
    // Memorise la tuile cible et redemarre l'animation.
    this->tile.x = tileX;
    this->tile.y = tileY;
    this->elapsedSeconds = 0.0;
    this->visible = true;
}

void TileClickMarker::hide(void)
{
    // Cache immediatement le marqueur et remet son timer a zero.
    this->visible = false;
    this->elapsedSeconds = 0.0;
}

void TileClickMarker::update(double dt)
{
    // Si le marqueur est inactif, aucun calcul n'est necessaire.
    if (!this->visible)
    {
        return;
    }

    // Auto-expiration apres la duree configuree.
    // Si durationSeconds <= 0, le marqueur reste visible jusqu'a hide()
    // ou au prochain show().
    this->elapsedSeconds += dt;
    if (this->durationSeconds > 0.0 && this->elapsedSeconds >= this->durationSeconds)
    {
        this->visible = false;
    }
}

void TileClickMarker::draw(const Map& map) const
{
    // On dessine uniquement pendant la fenetre de visibilite.
    if (!this->visible)
    {
        return;
    }

    // Conversion tuile -> centre ecran pour positionner le losange.
    const SDL_FPoint center = map.tileToScreenCenter(this->tile.x, this->tile.y);

    // Pulsation sinusoidale autour d'une echelle 1.0.
    const float pulse =
        1.0f + (this->pulseAmplitude * std::sin(static_cast<float>(this->elapsedSeconds) * this->pulseSpeed));

    // Le marqueur suit les proportions de la tuile map.
    const float markerW = map.getTileWidth() * pulse;
    const float markerH = map.getTileHeight() * pulse;

    // Pass 1: remplissage translucide.
    rc2d_graphics_setColor(this->fillColor);
    rc2d_graphics_drawTileIsometric("fill", center.x, center.y, markerW, markerH);

    // Pass 2: contour lisible.
    rc2d_graphics_setColor(this->lineColor);
    rc2d_graphics_drawTileIsometric("line", center.x, center.y, markerW, markerH);
}

void TileClickMarker::setDurationSeconds(double value)
{
    // value > 0  : auto-expiration active.
    // value <= 0 : mode persistant (pas d'auto-expiration).
    if (value <= 0.0)
    {
        this->durationSeconds = 0.0;
        return;
    }

    if (value > 0.0)
    {
        this->durationSeconds = value;
    }
}

bool TileClickMarker::isVisible(void) const
{
    return this->visible;
}
