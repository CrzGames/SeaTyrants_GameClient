#include "game/ui/hud/ingame-hud-overlay.h"

#include "core/context.h"
#include "game/entities/player.h"
#include "game/map/map.h"

IngameHudOverlay::IngameHudOverlay(void)
    : backgroundUiImage{},
      minimapWidget{},
      centerShipButtonWidget{},
      sectorCoordinateOverlay{},
      chatWidget{},
      espionSearchPlayerWidget{}
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

    // Fenetre chat interactive.
    this->chatWidget.load();
    this->espionSearchPlayerWidget.load();
}

void IngameHudOverlay::unload(void)
{
    this->espionSearchPlayerWidget.unload();
    this->chatWidget.unload();
    this->sectorCoordinateOverlay.unload();
    this->centerShipButtonWidget.unload();
    this->minimapWidget.unload();

    rc2d_graphics_freeImage(&this->backgroundUiImage);
}

void IngameHudOverlay::update(double dt)
{
    this->chatWidget.update(dt);
    this->espionSearchPlayerWidget.update(dt);
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
    this->chatWidget.draw();
    this->espionSearchPlayerWidget.draw();
}

bool IngameHudOverlay::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    if (this->espionSearchPlayerWidget.mousepressed(x, y, button, clicks, mouseID))
    {
        this->chatWidget.clearFocus();
        return true;
    }
    if (this->chatWidget.mousepressed(x, y, button, clicks, mouseID))
    {
        this->espionSearchPlayerWidget.clearFocus();
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
    }
    return false;
}

bool IngameHudOverlay::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    return this->chatWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID);
}

bool IngameHudOverlay::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    if (this->espionSearchPlayerWidget.keypressed(key, scancode, keycode, mod, isrepeat))
    {
        return true;
    }
    return this->chatWidget.keypressed(key, scancode, keycode, mod, isrepeat);
}
