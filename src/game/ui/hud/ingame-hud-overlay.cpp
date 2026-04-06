#include "game/ui/hud/ingame-hud-overlay.h"

#include "core/context.h"

IngameHudOverlay::IngameHudOverlay(void)
    : backgroundUiImage{},
      minimapUi{},
      buttonCenterMapUi{}
{
}

IngameHudOverlay::~IngameHudOverlay(void)
{
}

void IngameHudOverlay::load(void)
{
    // Fond UI gameplay (bandes haut/bas).
    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/background-ui-ingame.png",
        RC2D_STORAGE_TITLE);

    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "IngameHudOverlay: echec chargement background UI gameplay");
    }

    // UI minimap.
    this->minimapUi.image = rc2d_graphics_loadImageFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUi.anchor = RC2D_UI_ANCHOR_TOP_RIGHT;
    this->minimapUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->minimapUi.margin_x = 0.03f;
    this->minimapUi.margin_y = 0.05f;
    this->minimapUi.visible = true;
    this->minimapUi.hittable = true;

    // UI bouton "centrer la map".
    this->buttonCenterMapUi.image = rc2d_graphics_loadImageFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUi.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUi.anchor = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    this->buttonCenterMapUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->buttonCenterMapUi.margin_x = 0.0f;
    this->buttonCenterMapUi.margin_y = 0.25f;
    this->buttonCenterMapUi.visible = true;
    this->buttonCenterMapUi.hittable = true;
}

void IngameHudOverlay::unload(void)
{
    rc2d_graphics_freeImageData(&this->buttonCenterMapUi.imageData);
    rc2d_graphics_freeImage(&this->buttonCenterMapUi.image);

    rc2d_graphics_freeImageData(&this->minimapUi.imageData);
    rc2d_graphics_freeImage(&this->minimapUi.image);

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

void IngameHudOverlay::drawWidgets(void)
{
    rc2d_ui_drawImage(&this->minimapUi);
    rc2d_ui_drawImage(&this->buttonCenterMapUi);
}
