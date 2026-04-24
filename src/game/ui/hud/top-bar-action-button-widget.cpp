#include "game/ui/hud/top-bar-action-button-widget.h"

#include "core/context.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>

namespace top_bar_action_button_widget_internal {

static constexpr RC2D_Color kHoverFill = RC2D_Color{12, 20, 34, 210};
static constexpr RC2D_Color kActiveFill = RC2D_Color{54, 28, 8, 240};
static constexpr RC2D_Color kBorderColor = RC2D_Color{184, 132, 30, 250};
static constexpr float kFramePaddingX = 5.0f;
static constexpr float kFramePaddingY = 4.0f;
static constexpr float kHitPaddingX = 6.0f;
static constexpr float kHitPaddingY = 6.0f;

static bool isPointInRect(float x, float y, const SDL_FRect& rect)
{
    return (x >= rect.x && x <= (rect.x + rect.w) && y >= rect.y && y <= (rect.y + rect.h));
}

} // namespace top_bar_action_button_widget_internal

using namespace top_bar_action_button_widget_internal;

TopBarActionButtonWidget::TopBarActionButtonWidget(Action action, const char* imagePath)
    : action(action),
      imagePath(imagePath == nullptr ? "" : imagePath),
      uiImage{},
      hovered(false),
      active(false)
{
}

TopBarActionButtonWidget::~TopBarActionButtonWidget(void)
{
}

void TopBarActionButtonWidget::load(void)
{
    this->uiImage.image = LoadStorageImage(this->imagePath.c_str(), RC2D_STORAGE_TITLE);
    this->uiImage.imageData = LoadStorageImageData(this->imagePath.c_str(), RC2D_STORAGE_TITLE);
    this->uiImage.anchor = RC2D_UI_ANCHOR_TOP_LEFT;
    this->uiImage.margin_mode = RC2D_UI_MARGIN_PIXELS;
    this->uiImage.margin_x = 0.0f;
    this->uiImage.margin_y = 0.0f;
    this->uiImage.visible = true;
    this->uiImage.hittable = true;
    this->hovered = false;
    this->active = false;
}

void TopBarActionButtonWidget::unload(void)
{
    ResetStorageImageDataRef(&this->uiImage.imageData);
    ResetStorageImageRef(&this->uiImage.image);
}

void TopBarActionButtonWidget::setLocalPosition(float localX, float localY)
{
    this->uiImage.margin_x = localX;
    this->uiImage.margin_y = localY;
}

void TopBarActionButtonWidget::setActive(bool isActive)
{
    this->active = isActive;
}

bool TopBarActionButtonWidget::updateHover(float mouseX, float mouseY)
{
    this->hovered = this->containsPoint(mouseX, mouseY);
    return this->hovered;
}

void TopBarActionButtonWidget::draw(void)
{
    const SDL_FRect iconRect = this->getCurrentRect();
    if (iconRect.w > 0.0f && iconRect.h > 0.0f && (this->hovered || this->active))
    {
        const SDL_FRect frameRect = SDL_FRect{
            iconRect.x - kFramePaddingX,
            iconRect.y - kFramePaddingY,
            iconRect.w + (kFramePaddingX * 2.0f),
            iconRect.h + (kFramePaddingY * 2.0f)
        };
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(this->active ? kActiveFill : kHoverFill);
        rc2d_graphics_rectangle("fill", &frameRect);
        rc2d_graphics_setColor(kBorderColor);
        rc2d_graphics_rectangle("line", &frameRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }

    rc2d_ui_drawImage(&this->uiImage);
}

TopBarActionButtonWidget::Action TopBarActionButtonWidget::mousepressed(
    float x,
    float y,
    RC2D_MouseButton button) const
{
    if (button != RC2D_MOUSE_BUTTON_LEFT || !this->containsPoint(x, y))
    {
        return Action::NONE;
    }
    return this->action;
}

SDL_FRect TopBarActionButtonWidget::getCurrentRect(void) const
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    float width = 0.0f;
    float height = 0.0f;

    if (this->uiImage.imageData.sdl_surface != nullptr)
    {
        width = static_cast<float>(this->uiImage.imageData.sdl_surface->w);
        height = static_cast<float>(this->uiImage.imageData.sdl_surface->h);
    }
    else if (this->uiImage.image.sdl_texture != nullptr)
    {
        SDL_GetTextureSize(this->uiImage.image.sdl_texture, &width, &height);
    }

    return SDL_FRect{
        screenRect.x + this->uiImage.margin_x,
        screenRect.y + this->uiImage.margin_y,
        width,
        height
    };
}

bool TopBarActionButtonWidget::containsPoint(float x, float y) const
{
    if (this->uiImage.image.sdl_texture == nullptr)
    {
        return false;
    }

    const SDL_FRect iconRect = this->getCurrentRect();
    if (iconRect.w <= 0.0f || iconRect.h <= 0.0f)
    {
        return false;
    }

    const SDL_FRect hitRect = SDL_FRect{
        iconRect.x - kHitPaddingX,
        iconRect.y - kHitPaddingY,
        iconRect.w + (kHitPaddingX * 2.0f),
        iconRect.h + (kHitPaddingY * 2.0f)
    };
    return isPointInRect(x, y, hitRect);
}
