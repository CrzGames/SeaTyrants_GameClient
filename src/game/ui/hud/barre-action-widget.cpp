#include "game/ui/hud/barre-action-widget.h"
#include "game/assets/title-asset-cache.h"

BarreActionWidget::BarreActionWidget(void)
    : actionBarUi{}
{
}

BarreActionWidget::~BarreActionWidget(void)
{
}

void BarreActionWidget::load(void)
{
    this->actionBarUi.image = LoadStorageImage(
        "assets/images/ui-scene-game/barre-action.png",
        RC2D_STORAGE_TITLE);
    this->actionBarUi.imageData = LoadStorageImageData(
        "assets/images/ui-scene-game/barre-action.png",
        RC2D_STORAGE_TITLE);
    this->actionBarUi.anchor = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    this->actionBarUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->actionBarUi.margin_x = 0.0f;
    this->actionBarUi.margin_y = 0.002f;
    this->actionBarUi.visible = true;
    this->actionBarUi.hittable = false;
}

void BarreActionWidget::unload(void)
{
    ResetStorageImageDataRef(&this->actionBarUi.imageData);
    ResetStorageImageRef(&this->actionBarUi.image);
}

void BarreActionWidget::draw(void)
{
    rc2d_ui_drawImage(&this->actionBarUi);
}
