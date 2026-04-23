#include "game/ui/hud/top-bar-menu-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

TopBarMenuWidget::TopBarMenuWidget(void)
    : topBarMenuUiImage{}
{
}

TopBarMenuWidget::~TopBarMenuWidget(void)
{
}

void TopBarMenuWidget::load(void)
{
    this->topBarMenuUiImage = LoadStorageImage(
        "assets/images/ui-scene-game/top-bar-menu.png",
        RC2D_STORAGE_TITLE);
    if (this->topBarMenuUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "TopBarMenuWidget: echec chargement top-bar-menu.png");
    }
}

void TopBarMenuWidget::unload(void)
{
    ResetStorageImageRef(&this->topBarMenuUiImage);
}

void TopBarMenuWidget::draw(void)
{
    if (this->topBarMenuUiImage.sdl_texture == nullptr)
    {
        return;
    }

    const SDL_FRect gameScreenRect = GetGameScreen().rect;

    rc2d_graphics_drawImage(
        &this->topBarMenuUiImage,
        gameScreenRect.x,
        gameScreenRect.y + 2.0f,
        0.0,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        false,
        false);
}
