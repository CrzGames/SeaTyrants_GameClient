#include "game/ui/hud/center-ship-button-widget.h"
#include "game/assets/title-asset-cache.h"

static constexpr float kHudWidgetScaleMin = 0.50f;
static constexpr float kHudWidgetScaleMax = 1.0f;
static constexpr float kCenterShipLeftMarginPx = 5.0f;
static constexpr float kCenterShipGapAboveZoomBarPx = 6.0f;
static constexpr float kReferenceZoomBarWidthPx = 104.0f;
static constexpr float kReferenceZoomBarHeightPx = 9.0f;
static constexpr float kReferenceZoomBarBottomMarginPx = 5.0f;

static float clampHudWidgetScale(float scale)
{
    return (scale < kHudWidgetScaleMin)
               ? kHudWidgetScaleMin
               : ((scale > kHudWidgetScaleMax) ? kHudWidgetScaleMax : scale);
}

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

static SDL_FRect scaleRectFromCenter(const SDL_FRect& rect, float scale)
{
    const float clampedScale = clampHudWidgetScale(scale);
    const float scaledWidth = rect.w * clampedScale;
    const float scaledHeight = rect.h * clampedScale;
    return SDL_FRect{
        rect.x + ((rect.w - scaledWidth) * 0.5f),
        rect.y + ((rect.h - scaledHeight) * 0.5f),
        scaledWidth,
        scaledHeight};
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

static SDL_FRect getCurrentRect(const RC2D_ImageData& imageData, const RC2D_Image& image, float scale, const SDL_FPoint& offset)
{
    const SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();
    if (visibleRect.w <= 0.0f || visibleRect.h <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    float width = 0.0f;
    float height = 0.0f;
    if (!getImageSize(imageData, image, &width, &height))
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float centeredAboveZoomBarX =
        visibleRect.x +
        kCenterShipLeftMarginPx +
        ((kReferenceZoomBarWidthPx - width) * 0.5f) +
        offset.x;
    return scaleRectFromCenter(
        SDL_FRect{
            centeredAboveZoomBarX,
            visibleRect.y + visibleRect.h - kReferenceZoomBarBottomMarginPx - kReferenceZoomBarHeightPx - kCenterShipGapAboveZoomBarPx - height + offset.y,
            width,
            height},
        scale);
}

static void drawImageToRect(RC2D_Image* image, const SDL_FRect& drawRect)
{
    if (image == nullptr || image->sdl_texture == nullptr || drawRect.w <= 0.0f || drawRect.h <= 0.0f)
    {
        return;
    }

    float textureWidth = 0.0f;
    float textureHeight = 0.0f;
    if (!SDL_GetTextureSize(image->sdl_texture, &textureWidth, &textureHeight) ||
        textureWidth <= 0.0f ||
        textureHeight <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawImage(
        image,
        drawRect.x,
        drawRect.y,
        0.0,
        drawRect.w / textureWidth,
        drawRect.h / textureHeight,
        0.0f,
        0.0f,
        false,
        false);
}

CenterShipButtonWidget::CenterShipButtonWidget(void)
    : buttonImage{},
      buttonImageData{},
      uiScale(1.0f),
      positionOffset{0.0f, 0.0f}
{
}

CenterShipButtonWidget::~CenterShipButtonWidget(void)
{
}

void CenterShipButtonWidget::load(void)
{
    this->buttonImage = LoadStorageImage(
        "assets/images/ui-scene-game/center-ship.png",
        RC2D_STORAGE_TITLE);
    this->buttonImageData = LoadStorageImageData(
        "assets/images/ui-scene-game/center-ship.png",
        RC2D_STORAGE_TITLE);
}

void CenterShipButtonWidget::unload(void)
{
    ResetStorageImageDataRef(&this->buttonImageData);
    ResetStorageImageRef(&this->buttonImage);
}

void CenterShipButtonWidget::draw(void)
{
    drawImageToRect(&this->buttonImage, this->getCurrentRect());
}

void CenterShipButtonWidget::setUiScale(float scale)
{
    this->uiScale = clampHudWidgetScale(scale);
}

void CenterShipButtonWidget::setPositionOffset(float offsetX, float offsetY)
{
    this->positionOffset = SDL_FPoint{offsetX, offsetY};
}

SDL_FPoint CenterShipButtonWidget::getPositionOffset(void) const
{
    return this->positionOffset;
}

void CenterShipButtonWidget::resetPositionOffset(void)
{
    this->positionOffset = SDL_FPoint{0.0f, 0.0f};
}

bool CenterShipButtonWidget::containsPoint(float x, float y) const
{
    if (this->buttonImage.sdl_texture == nullptr)
    {
        return false;
    }

    return pointInRect(x, y, this->getCurrentRect());
}

HudCursorType CenterShipButtonWidget::getDesiredCursor(float x, float y) const
{
    if (this->containsPoint(x, y))
    {
        return HudCursorType::POINTER;
    }

    return HudCursorType::NONE;
}

SDL_FRect CenterShipButtonWidget::getCurrentRect(void) const
{
    return ::getCurrentRect(this->buttonImageData, this->buttonImage, this->uiScale, this->positionOffset);
}
