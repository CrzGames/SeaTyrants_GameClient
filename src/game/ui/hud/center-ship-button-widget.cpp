#include "game/ui/hud/center-ship-button-widget.h"

CenterShipButtonWidget::CenterShipButtonWidget(void)
    : buttonUi{}
{
}

CenterShipButtonWidget::~CenterShipButtonWidget(void)
{
}

void CenterShipButtonWidget::load(void)
{
    // Bouton centre en bas de l'ecran, dans la zone UI.
    this->buttonUi.image = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/center-ship.png",
        RC2D_STORAGE_TITLE);
    this->buttonUi.imageData = rc2d_graphics_loadImageDataFromStorage(
        "assets/images/ui-scene-game/center-ship.png",
        RC2D_STORAGE_TITLE);
    this->buttonUi.anchor = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    this->buttonUi.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->buttonUi.margin_x = 0.0f;
    this->buttonUi.margin_y = 0.25f;
    this->buttonUi.visible = true;
    this->buttonUi.hittable = true;
}

void CenterShipButtonWidget::unload(void)
{
    rc2d_graphics_freeImageData(&this->buttonUi.imageData);
    rc2d_graphics_freeImage(&this->buttonUi.image);
}

void CenterShipButtonWidget::draw(void)
{
    rc2d_ui_drawImage(&this->buttonUi);
}
