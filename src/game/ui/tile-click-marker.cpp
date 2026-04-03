#include "game/ui/tile-click-marker.h"

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
    tile.x = tileX;
    tile.y = tileY;
    elapsedSeconds = 0.0;
    visible = true;
}

void TileClickMarker::hide(void)
{
    visible = false;
    elapsedSeconds = 0.0;
}

void TileClickMarker::update(double dt)
{
    if (!visible)
    {
        return;
    }

    elapsedSeconds += dt;
    if (elapsedSeconds >= durationSeconds)
    {
        visible = false;
    }
}

void TileClickMarker::draw(const Map& map) const
{
    if (!visible)
    {
        return;
    }

    const SDL_FPoint center = map.tileToScreenCenter(tile.x, tile.y);

    const float pulse =
        1.0f + (pulseAmplitude * std::sin(static_cast<float>(elapsedSeconds) * pulseSpeed));

    const float markerW = map.getTileWidth() * pulse;
    const float markerH = map.getTileHeight() * pulse;

    rc2d_graphics_setColor(fillColor);
    rc2d_graphics_drawTileIsometric("fill", center.x, center.y, markerW, markerH);

    rc2d_graphics_setColor(lineColor);
    rc2d_graphics_drawTileIsometric("line", center.x, center.y, markerW, markerH);
}

void TileClickMarker::setDurationSeconds(double value)
{
    if (value > 0.0)
    {
        durationSeconds = value;
    }
}

bool TileClickMarker::isVisible(void) const
{
    return visible;
}
