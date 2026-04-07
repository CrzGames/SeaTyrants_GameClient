#include "game/ui/hud/minimap-widget.h"

MinimapWidget::MinimapWidget(void)
    : minimapUi{}
{
}

MinimapWidget::~MinimapWidget(void)
{
}

void MinimapWidget::load(void)
{
    // Minimap ancree en haut a droite, avec des marges relatives a l'ecran.
    this->minimapUi.image = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/minimap.png",
        RC2D_STORAGE_TITLE);
    this->minimapUi.imageData = rc2d_graphics_loadImageDataFromStorage(
        "assets/images/ui-scene-game/minimap.png",
        RC2D_STORAGE_TITLE);
    this->minimapUi.anchor = RC2D_UI_ANCHOR_TOP_RIGHT;
    this->minimapUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->minimapUi.margin_x = 0.03f;
    this->minimapUi.margin_y = 0.05f;
    this->minimapUi.visible = true;
    this->minimapUi.hittable = true;
}

void MinimapWidget::unload(void)
{
    rc2d_graphics_freeImageData(&this->minimapUi.imageData);
    rc2d_graphics_freeImage(&this->minimapUi.image);
}

void MinimapWidget::draw(void)
{
    rc2d_ui_drawImage(&this->minimapUi);
}
