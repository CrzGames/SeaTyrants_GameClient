#include "game/ui/overlay/tile-click-marker-overlay.h"

#include <cmath>

TileClickMarkerOverlay::TileClickMarkerOverlay(void)
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

TileClickMarkerOverlay::~TileClickMarkerOverlay(void)
{
}

void TileClickMarkerOverlay::show(int tileX, int tileY)
{
    // Memorise la tuile cible et redemarre l'animation.
    this->tile.x = tileX;
    this->tile.y = tileY;
    this->elapsedSeconds = 0.0;
    this->visible = true;
}

void TileClickMarkerOverlay::hide(void)
{
    // Cache immediatement le marqueur et remet son timer a zero.
    this->visible = false;
    this->elapsedSeconds = 0.0;
}

void TileClickMarkerOverlay::update(double dt)
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

void TileClickMarkerOverlay::draw(const Map& map) const
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

    // Couleur dynamique:
    // - blanc/bleute: tuile valide et traversable
    // - rouge: tuile bloquee ou hors map
    const bool isBlockedOrOutside = map.isTileBlocked(this->tile.x, this->tile.y);
    const RC2D_Color currentFillColor = isBlockedOrOutside
        ? RC2D_Color{255, 72, 72, 92}
        : this->fillColor;
    const RC2D_Color currentLineColor = isBlockedOrOutside
        ? RC2D_Color{255, 85, 85, 245}
        : this->lineColor;

    // Le marqueur utilise un fill alpha: on force BLEND localement pour
    // eviter un rendu opaque/blanchi si l'etat courant est en NONE.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Pass 1: remplissage translucide.
    rc2d_graphics_setColor(currentFillColor);
    rc2d_graphics_drawTileIsometric("fill", center.x, center.y, markerW, markerH);

    // Pass 2: contour lisible.
    rc2d_graphics_setColor(currentLineColor);
    rc2d_graphics_drawTileIsometric("line", center.x, center.y, markerW, markerH);

    // Restaure un etat neutre pour ne pas impacter les autres draws.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void TileClickMarkerOverlay::setDurationSeconds(double value)
{
    // value <= 0 : persistant jusqu'a hide()/show().
    this->durationSeconds = value;

    // Si la nouvelle duree est positive et deja depassee, on masque tout de suite.
    if (this->durationSeconds > 0.0 &&
        this->visible &&
        this->elapsedSeconds >= this->durationSeconds)
    {
        this->visible = false;
    }
}

bool TileClickMarkerOverlay::isVisible(void) const
{
    return this->visible;
}
