#include "game/ui/hud/background-widget.h"
#include "game/assets/title-asset-cache.h"

BackgroundWidget::BackgroundWidget(void)
    : backgroundUiImage{}
{
}

BackgroundWidget::~BackgroundWidget(void)
{
}

void BackgroundWidget::load(void)
{
    this->backgroundUiImage = LoadStorageImage(
        "assets/images/ui-scene-game/background.png",
        RC2D_STORAGE_TITLE);
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "BackgroundWidget: echec chargement background UI gameplay");
    }
}

void BackgroundWidget::unload(void)
{
    ResetStorageImageRef(&this->backgroundUiImage);
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
