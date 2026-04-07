#include "game/ui/hud/ingame-hud-overlay.h"

#include "core/context.h"
#include "game/entities/player.h"
#include "game/map/map.h"

IngameHudOverlay::IngameHudOverlay(void)
    : backgroundUiImage{},
      minimapWidget{},
      centerShipButtonWidget{},
      sectorCoordinateOverlay{}
{
}

IngameHudOverlay::~IngameHudOverlay(void)
{
}

void IngameHudOverlay::load(void)
{
    // Fond UI gameplay (bandes haut/bas).
    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/background.png",
        RC2D_STORAGE_TITLE);
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "IngameHudOverlay: echec chargement background UI gameplay");
    }

    // Widgets HUD extraits dans leurs propres composants.
    this->minimapWidget.load();

    // Bouton de recentrage du navire, garde sa logique/ressources dans son widget dedie.
    this->centerShipButtonWidget.load();

    // Overlay texte du secteur courant.
    this->sectorCoordinateOverlay.load();
}

void IngameHudOverlay::unload(void)
{
    this->sectorCoordinateOverlay.unload();
    this->centerShipButtonWidget.unload();
    this->minimapWidget.unload();

    rc2d_graphics_freeImage(&this->backgroundUiImage);
}

void IngameHudOverlay::drawBackground(void)
{
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        return;
    }

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

void IngameHudOverlay::drawWidgets(const Map& map, const Player& player)
{
    this->sectorCoordinateOverlay.draw(map, player);
    this->minimapWidget.draw();
    this->centerShipButtonWidget.draw();
}
