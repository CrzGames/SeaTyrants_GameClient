#include "game/ui/hud/background-widget.h"

BackgroundWidget::BackgroundWidget(void)
    : backgroundUiImage{}
{
}

BackgroundWidget::~BackgroundWidget(void)
{
}

void BackgroundWidget::load(void)
{
    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/background.png",
        RC2D_STORAGE_TITLE);
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "BackgroundWidget: echec chargement background UI gameplay");
    }
}

void BackgroundWidget::unload(void)
{
    rc2d_graphics_freeImage(&this->backgroundUiImage);
}

void BackgroundWidget::draw(void)
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
