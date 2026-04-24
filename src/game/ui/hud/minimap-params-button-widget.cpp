#include "game/ui/hud/minimap-params-button-widget.h"
#include "game/assets/title-asset-cache.h"

namespace minimap_params_button_widget_internal {

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

static bool getImageSize(const RC2D_ImageData& imageData, const RC2D_Image& image, float* outWidth, float* outHeight)
{
    if (outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    if (imageData.sdl_surface != nullptr)
    {
        *outWidth = static_cast<float>(imageData.sdl_surface->w);
        *outHeight = static_cast<float>(imageData.sdl_surface->h);
        return true;
    }

    if (image.sdl_texture != nullptr && SDL_GetTextureSize(image.sdl_texture, outWidth, outHeight))
    {
        return true;
    }

    *outWidth = 0.0f;
    *outHeight = 0.0f;
    return false;
}

} // namespace minimap_params_button_widget_internal

using namespace minimap_params_button_widget_internal;

MinimapParamsButtonWidget::MinimapParamsButtonWidget(void)
    : iconImage{},
      iconImageData{}
{
}

MinimapParamsButtonWidget::~MinimapParamsButtonWidget(void)
{
}

void MinimapParamsButtonWidget::load(void)
{
    this->iconImage = LoadStorageImage(
        "assets/images/ui-scene-game/icon-paramsminimap.png",
        RC2D_STORAGE_TITLE);
    this->iconImageData = LoadStorageImageData(
        "assets/images/ui-scene-game/icon-paramsminimap.png",
        RC2D_STORAGE_TITLE);
}

void MinimapParamsButtonWidget::unload(void)
{
    ResetStorageImageDataRef(&this->iconImageData);
    ResetStorageImageRef(&this->iconImage);
}

SDL_FRect MinimapParamsButtonWidget::getCurrentRect(const MinimapWidget& minimapWidget) const
{
    const SDL_FRect minimapRect = minimapWidget.getCurrentRect();
    if (minimapRect.w <= 0.0f || minimapRect.h <= 0.0f || this->iconImage.sdl_texture == nullptr)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    float width = 0.0f;
    float height = 0.0f;
    if (!getImageSize(this->iconImageData, this->iconImage, &width, &height))
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    return SDL_FRect{
        minimapRect.x - (width * 0.5f),
        minimapRect.y + ((minimapRect.h - height) * 0.5f),
        width,
        height
    };
}

void MinimapParamsButtonWidget::draw(const MinimapWidget& minimapWidget) const
{
    if (this->iconImage.sdl_texture == nullptr)
    {
        return;
    }

    const SDL_FRect rect = this->getCurrentRect(minimapWidget);
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawImage(&const_cast<RC2D_Image&>(this->iconImage), rect.x, rect.y, 0.0, 1.0f, 1.0f, 0.0f, 0.0f, false, false);
}

bool MinimapParamsButtonWidget::containsPoint(float x, float y, const MinimapWidget& minimapWidget) const
{
    return pointInRect(x, y, this->getCurrentRect(minimapWidget));
}

HudCursorType MinimapParamsButtonWidget::getDesiredCursor(float x, float y, const MinimapWidget& minimapWidget) const
{
    if (this->containsPoint(x, y, minimapWidget))
    {
        return HudCursorType::POINTER;
    }

    return HudCursorType::NONE;
}

bool MinimapParamsButtonWidget::mousepressed(float x, float y, RC2D_MouseButton button, const MinimapWidget& minimapWidget) const
{
    return (button == RC2D_MOUSE_BUTTON_LEFT && this->containsPoint(x, y, minimapWidget));
}
