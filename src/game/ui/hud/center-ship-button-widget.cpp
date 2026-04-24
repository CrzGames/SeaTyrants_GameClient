#include "game/ui/hud/center-ship-button-widget.h"
#include "game/assets/title-asset-cache.h"

namespace center_ship_button_widget_internal {

static bool pointInRect(float x, float y, const SDL_FRect& rect)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return false;
    }

    return (
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

static SDL_FRect getCurrentRect(const RC2D_UIImage& uiImage)
{
    const SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();
    if (visibleRect.w <= 0.0f || visibleRect.h <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    float width = 0.0f;
    float height = 0.0f;
    if (uiImage.imageData.sdl_surface != nullptr)
    {
        width = static_cast<float>(uiImage.imageData.sdl_surface->w);
        height = static_cast<float>(uiImage.imageData.sdl_surface->h);
    }
    else if (uiImage.image.sdl_texture == nullptr ||
             !SDL_GetTextureSize(uiImage.image.sdl_texture, &width, &height))
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    float marginX = uiImage.margin_x;
    float marginY = uiImage.margin_y;
    if (uiImage.margin_mode == RC2D_UI_MARGIN_PERCENT)
    {
        marginX = visibleRect.w * uiImage.margin_x;
        marginY = visibleRect.h * uiImage.margin_y;
    }

    switch (uiImage.anchor)
    {
        case RC2D_UI_ANCHOR_TOP_LEFT:
            return SDL_FRect{visibleRect.x + marginX, visibleRect.y + marginY, width, height};
        case RC2D_UI_ANCHOR_TOP_RIGHT:
            return SDL_FRect{visibleRect.x + visibleRect.w - marginX - width, visibleRect.y + marginY, width, height};
        case RC2D_UI_ANCHOR_BOTTOM_LEFT:
            return SDL_FRect{visibleRect.x + marginX, visibleRect.y + visibleRect.h - marginY - height, width, height};
        case RC2D_UI_ANCHOR_BOTTOM_RIGHT:
            return SDL_FRect{visibleRect.x + visibleRect.w - marginX - width, visibleRect.y + visibleRect.h - marginY - height, width, height};
        case RC2D_UI_ANCHOR_TOP_CENTER:
            return SDL_FRect{visibleRect.x + ((visibleRect.w - width) * 0.5f) + marginX, visibleRect.y + marginY, width, height};
        case RC2D_UI_ANCHOR_BOTTOM_CENTER:
            return SDL_FRect{
                visibleRect.x + ((visibleRect.w - width) * 0.5f) + marginX,
                visibleRect.y + visibleRect.h - marginY - height,
                width,
                height
            };
        case RC2D_UI_ANCHOR_CENTER:
            return SDL_FRect{
                visibleRect.x + ((visibleRect.w - width) * 0.5f) + marginX,
                visibleRect.y + ((visibleRect.h - height) * 0.5f) + marginY,
                width,
                height
            };
        default:
            return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }
}

} // namespace center_ship_button_widget_internal

using namespace center_ship_button_widget_internal;

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
    this->buttonUi.image = LoadStorageImage(
        "assets/images/ui-scene-game/center-ship.png",
        RC2D_STORAGE_TITLE);
    this->buttonUi.imageData = LoadStorageImageData(
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
    ResetStorageImageDataRef(&this->buttonUi.imageData);
    ResetStorageImageRef(&this->buttonUi.image);
}

void CenterShipButtonWidget::draw(void)
{
    rc2d_ui_drawImage(&this->buttonUi);
}

bool CenterShipButtonWidget::containsPoint(float x, float y) const
{
    if (this->buttonUi.image.sdl_texture == nullptr || !this->buttonUi.visible || !this->buttonUi.hittable)
    {
        return false;
    }

    return pointInRect(x, y, getCurrentRect(this->buttonUi));
}

HudCursorType CenterShipButtonWidget::getDesiredCursor(float x, float y) const
{
    if (this->containsPoint(x, y))
    {
        return HudCursorType::POINTER;
    }

    return HudCursorType::NONE;
}
