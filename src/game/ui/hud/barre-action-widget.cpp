#include "game/ui/hud/barre-action-widget.h"
#include "game/assets/title-asset-cache.h"

static constexpr float kHudWidgetScaleMin = 0.75f;
static constexpr float kHudWidgetScaleMax = 1.0f;
static constexpr float kActionBarBottomMarginPercent = 0.002f;

static float clampHudWidgetScale(float scale)
{
    return (scale < kHudWidgetScaleMin)
               ? kHudWidgetScaleMin
               : ((scale > kHudWidgetScaleMax) ? kHudWidgetScaleMax : scale);
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

static SDL_FRect computeActionBarRect(const RC2D_ImageData& imageData, const RC2D_Image& image, float scale, const SDL_FPoint& offset)
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

    const float bottomMargin = visibleRect.h * kActionBarBottomMarginPercent;
    return scaleRectFromCenter(
        SDL_FRect{
            visibleRect.x + ((visibleRect.w - width) * 0.5f) + offset.x,
            visibleRect.y + visibleRect.h - bottomMargin - height + offset.y,
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

BarreActionWidget::BarreActionWidget(void)
    : actionBarImage{},
      actionBarImageData{},
      uiScale(1.0f),
      positionOffset{0.0f, 0.0f}
{
}

BarreActionWidget::~BarreActionWidget(void)
{
}

void BarreActionWidget::load(void)
{
    this->actionBarImage = LoadStorageImage(
        "assets/images/ui-scene-game/barre-action.png",
        RC2D_STORAGE_TITLE);
    this->actionBarImageData = LoadStorageImageData(
        "assets/images/ui-scene-game/barre-action.png",
        RC2D_STORAGE_TITLE);
}

void BarreActionWidget::unload(void)
{
    ResetStorageImageDataRef(&this->actionBarImageData);
    ResetStorageImageRef(&this->actionBarImage);
}

void BarreActionWidget::draw(void)
{
    drawImageToRect(
        &this->actionBarImage,
        this->getCurrentRect());
}

void BarreActionWidget::setUiScale(float scale)
{
    this->uiScale = clampHudWidgetScale(scale);
}

void BarreActionWidget::setPositionOffset(float offsetX, float offsetY)
{
    this->positionOffset = SDL_FPoint{offsetX, offsetY};
}

SDL_FPoint BarreActionWidget::getPositionOffset(void) const
{
    return this->positionOffset;
}

void BarreActionWidget::resetPositionOffset(void)
{
    this->positionOffset = SDL_FPoint{0.0f, 0.0f};
}

SDL_FRect BarreActionWidget::getCurrentRect(void) const
{
    return computeActionBarRect(this->actionBarImageData, this->actionBarImage, this->uiScale, this->positionOffset);
}
