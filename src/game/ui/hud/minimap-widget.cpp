#include "game/ui/hud/minimap-widget.h"
#include "game/assets/title-asset-cache.h"

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
    this->minimapUi.image = LoadStorageImage(
        "assets/images/ui-scene-game/minimap.png",
        RC2D_STORAGE_TITLE);
    this->minimapUi.imageData = LoadStorageImageData(
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
    ResetStorageImageDataRef(&this->minimapUi.imageData);
    ResetStorageImageRef(&this->minimapUi.image);
}

void MinimapWidget::draw(void)
{
    rc2d_ui_drawImage(&this->minimapUi);
}
