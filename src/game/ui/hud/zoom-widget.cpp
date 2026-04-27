#include "game/ui/hud/zoom-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cmath>

#include "game/camera.h"

static constexpr float kZoomWidgetScreenMarginPx = 5.0f;
static constexpr float kZoomTooltipOffsetX = 14.0f;
static constexpr float kZoomTooltipOffsetY = 18.0f;
static constexpr float kZoomTooltipPaddingX = 10.0f;
static constexpr float kZoomTooltipPaddingY = 6.0f;
static constexpr float kHudWidgetScaleMin = 0.75f;
static constexpr float kHudWidgetScaleMax = 1.0f;
static constexpr RC2D_Color kZoomTooltipFill = RC2D_Color{67, 8, 8, 236};
static constexpr RC2D_Color kZoomTooltipBorder = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kZoomTooltipText = RC2D_Color{217, 200, 134, 255};

static float clampHudWidgetScale(float scale)
{
    return std::clamp(scale, kHudWidgetScaleMin, kHudWidgetScaleMax);
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

static void getMouseRenderPosition(float* outX, float* outY)
{
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        *outX = windowX;
        *outY = windowY;
        return;
    }

    float renderX = windowX;
    float renderY = windowY;
    if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &renderX, &renderY))
    {
        renderX = windowX;
        renderY = windowY;
    }

    *outX = renderX;
    *outY = renderY;
}

static bool loadImageResources(
    RC2D_Image* image,
    RC2D_ImageData* imageData,
    const char* path)
{
    if (image == nullptr || imageData == nullptr)
    {
        return false;
    }

    *image = LoadStorageImage(path, RC2D_STORAGE_TITLE);
    *imageData = LoadStorageImageData(path, RC2D_STORAGE_TITLE);
    return (image->sdl_texture != nullptr && imageData->sdl_surface != nullptr);
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

static float measureTextWidth(RC2D_Font* font, const char* text)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)height;
    return static_cast<float>(width);
}

static float measureTextHeight(RC2D_Font* font, const char* text)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)width;
    return static_cast<float>(height);
}

static void drawTextAt(RC2D_Font* font, const char* text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    rc2d_graphics_drawText(&textObject, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&textObject);
}

ZoomWidget::ZoomWidget(void)
    : zoomBarImage{},
      zoomBarImageData{},
      zoomSliderImage{},
      zoomSliderImageData{},
      tooltipFont{},
      sliderDragging(false),
      sliderHovered(false),
      sliderDragGrabOffsetX(0.0f),
      hoveredMouseX(0.0f),
      hoveredMouseY(0.0f),
      displayedZoomFactor(Camera::CAMERA_ZOOM_MAX_FACTOR),
      sliderOffsetX(0.0f),
      sliderTravelWidth(0.0f),
      zoomBarWidthPx(0.0f),
      zoomBarHeightPx(0.0f),
      zoomSliderWidthPx(0.0f),
      zoomSliderHeightPx(0.0f),
      uiScale(1.0f),
      positionOffset{0.0f, 0.0f}
{
}

ZoomWidget::~ZoomWidget(void)
{
}

void ZoomWidget::load(void)
{
    if (!loadImageResources(
            &this->zoomBarImage,
            &this->zoomBarImageData,
            "assets/images/ui-scene-game/zoom-bar.png"))
    {
        RC2D_log(RC2D_LOG_WARN, "ZoomWidget: echec chargement zoom-bar.png");
    }

    if (!loadImageResources(
            &this->zoomSliderImage,
            &this->zoomSliderImageData,
            "assets/images/ui-scene-game/zoom-slider.png"))
    {
        RC2D_log(RC2D_LOG_WARN, "ZoomWidget: echec chargement zoom-slider.png");
    }

    this->tooltipFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        13.0f);

    this->zoomBarWidthPx =
        (this->zoomBarImageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomBarImageData.sdl_surface->w)
            : 0.0f;
    this->zoomBarHeightPx =
        (this->zoomBarImageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomBarImageData.sdl_surface->h)
            : 0.0f;

    this->zoomSliderWidthPx =
        (this->zoomSliderImageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomSliderImageData.sdl_surface->w)
            : 0.0f;
    this->zoomSliderHeightPx =
        (this->zoomSliderImageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomSliderImageData.sdl_surface->h)
            : 0.0f;

    // Position initiale: slider tout a droite (zoom max camera).
    this->sliderTravelWidth = (std::max)(this->zoomBarWidthPx - this->zoomSliderWidthPx, 0.0f);
    this->sliderOffsetX = this->sliderTravelWidth;
    this->sliderDragging = false;
    this->sliderHovered = false;
    this->sliderDragGrabOffsetX = 0.0f;
    this->hoveredMouseX = 0.0f;
    this->hoveredMouseY = 0.0f;
    this->displayedZoomFactor = Camera::CAMERA_ZOOM_MAX_FACTOR;
}

void ZoomWidget::unload(void)
{
    ResetStorageFontRef(&this->tooltipFont);
    ResetStorageImageDataRef(&this->zoomSliderImageData);
    ResetStorageImageRef(&this->zoomSliderImage);
    ResetStorageImageDataRef(&this->zoomBarImageData);
    ResetStorageImageRef(&this->zoomBarImage);
}

int ZoomWidget::getZoomStepsCount(void)
{
    const float zoomRange = Camera::CAMERA_ZOOM_MAX_FACTOR - Camera::CAMERA_ZOOM_MIN_FACTOR;
    if (Camera::CAMERA_ZOOM_STEP_FACTOR <= 0.0f || zoomRange <= 0.0f)
    {
        return 1;
    }

    const int computedSteps =
        static_cast<int>(std::round(zoomRange / Camera::CAMERA_ZOOM_STEP_FACTOR));
    return (std::max)(computedSteps, 1);
}

void ZoomWidget::refreshTravelWidthFromDrawnRects(void)
{
    this->sliderTravelWidth =
        (std::max)((this->zoomBarWidthPx * this->uiScale) - (this->zoomSliderWidthPx * this->uiScale), 0.0f);
}

void ZoomWidget::setSliderOffsetFromRawValue(float rawOffset)
{
    this->refreshTravelWidthFromDrawnRects();

    if (this->sliderTravelWidth <= 0.0f)
    {
        this->sliderOffsetX = 0.0f;
        return;
    }

    const float clamped = std::clamp(rawOffset, 0.0f, this->sliderTravelWidth);
    const float normalized = clamped / this->sliderTravelWidth;
    const int zoomStepsCount = ZoomWidget::getZoomStepsCount();
    const float snappedStep =
        std::round(normalized * static_cast<float>(zoomStepsCount));
    const float snappedNormalized =
        snappedStep / static_cast<float>(zoomStepsCount);

    this->sliderOffsetX = snappedNormalized * this->sliderTravelWidth;
}

void ZoomWidget::syncSliderFromCameraZoom(const Camera& camera)
{
    this->refreshTravelWidthFromDrawnRects();

    if (this->sliderTravelWidth <= 0.0f)
    {
        this->sliderOffsetX = 0.0f;
        return;
    }

    const float zoomMin = Camera::CAMERA_ZOOM_MIN_FACTOR;
    const float zoomMax = Camera::CAMERA_ZOOM_MAX_FACTOR;
    const float zoomRange = zoomMax - zoomMin;
    if (zoomRange <= 0.0f)
    {
        this->sliderOffsetX = 0.0f;
        return;
    }

    const float zoom = std::clamp(camera.getZoomFactor(), zoomMin, zoomMax);
    const float normalized = (zoomMax - zoom) / zoomRange;
    const int zoomStepsCount = ZoomWidget::getZoomStepsCount();
    const float snappedStep =
        std::round(normalized * static_cast<float>(zoomStepsCount));
    const float snappedNormalized =
        snappedStep / static_cast<float>(zoomStepsCount);

    this->sliderOffsetX = snappedNormalized * this->sliderTravelWidth;
}

void ZoomWidget::applySliderToCameraZoom(Camera& camera) const
{
    if (this->sliderTravelWidth <= 0.0f)
    {
        camera.setZoomFactor(Camera::CAMERA_ZOOM_MAX_FACTOR);
        return;
    }

    const float normalized = std::clamp(this->sliderOffsetX / this->sliderTravelWidth, 0.0f, 1.0f);
    const int zoomStepsCount = ZoomWidget::getZoomStepsCount();
    const float snappedStep =
        std::round(normalized * static_cast<float>(zoomStepsCount));
    const float zoom =
        Camera::CAMERA_ZOOM_MAX_FACTOR - (snappedStep * Camera::CAMERA_ZOOM_STEP_FACTOR);

    camera.setZoomFactor(
        std::clamp(
            zoom,
            Camera::CAMERA_ZOOM_MIN_FACTOR,
            Camera::CAMERA_ZOOM_MAX_FACTOR));
}

void ZoomWidget::update(Camera& camera)
{
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    this->hoveredMouseX = mouseX;
    this->hoveredMouseY = mouseY;
    this->sliderHovered = ZoomWidget::pointInRect(mouseX, mouseY, this->computeSliderRect());

    // Pas de drag actif: le slider suit les autres changements de zoom (clavier, code, etc.).
    if (!this->sliderDragging)
    {
        this->syncSliderFromCameraZoom(camera);
        this->displayedZoomFactor = camera.getZoomFactor();
        return;
    }

    // Fin de drag si le bouton gauche est relache.
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->sliderDragging = false;
        this->syncSliderFromCameraZoom(camera);
        this->sliderHovered = this->isSliderHovered(mouseX, mouseY);
        this->displayedZoomFactor = camera.getZoomFactor();
        return;
    }

    this->refreshTravelWidthFromDrawnRects();

    // Position slider en coords locales de barre, puis snap sur les crans de zoom.
    const SDL_FRect barRect = this->computeBarRect();
    const float rawOffset = (mouseX - barRect.x) - this->sliderDragGrabOffsetX;
    this->setSliderOffsetFromRawValue(rawOffset);
    this->applySliderToCameraZoom(camera);
    this->displayedZoomFactor = camera.getZoomFactor();
}

void ZoomWidget::draw(void)
{
    if (this->zoomBarImage.sdl_texture == nullptr || this->zoomSliderImage.sdl_texture == nullptr)
    {
        return;
    }

    this->refreshTravelWidthFromDrawnRects();
    this->sliderOffsetX = std::clamp(this->sliderOffsetX, 0.0f, this->sliderTravelWidth);
    drawImageToRect(&this->zoomBarImage, this->computeBarRect());
    drawImageToRect(&this->zoomSliderImage, this->computeSliderRect());
}

void ZoomWidget::drawTooltip(void) const
{
    this->drawHoveredTooltip();
}

bool ZoomWidget::mousepressed(float x, float y, RC2D_MouseButton button)
{
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect sliderRect = this->computeSliderRect();
    if (!ZoomWidget::pointInRect(x, y, sliderRect))
    {
        return false;
    }

    this->sliderDragging = true;
    this->sliderDragGrabOffsetX = x - sliderRect.x;
    this->sliderDragGrabOffsetX =
        std::clamp(this->sliderDragGrabOffsetX, 0.0f, sliderRect.w);
    return true;
}

bool ZoomWidget::isDraggingSlider(void) const
{
    return this->sliderDragging;
}

bool ZoomWidget::isSliderHovered(float x, float y) const
{
    return ZoomWidget::pointInRect(x, y, this->computeSliderRect());
}

SDL_FRect ZoomWidget::getBarRect(void) const
{
    return this->computeBarRect();
}

SDL_FRect ZoomWidget::getCurrentRect(void) const
{
    const SDL_FRect barRect = this->computeBarRect();
    const SDL_FRect sliderRect = this->computeSliderRect();
    if (barRect.w <= 0.0f || barRect.h <= 0.0f)
    {
        return sliderRect;
    }
    if (sliderRect.w <= 0.0f || sliderRect.h <= 0.0f)
    {
        return barRect;
    }

    const float left = (std::min)(barRect.x, sliderRect.x);
    const float top = (std::min)(barRect.y, sliderRect.y);
    const float right = (std::max)(barRect.x + barRect.w, sliderRect.x + sliderRect.w);
    const float bottom = (std::max)(barRect.y + barRect.h, sliderRect.y + sliderRect.h);
    return SDL_FRect{
        left,
        top,
        right - left,
        bottom - top
    };
}

void ZoomWidget::setUiScale(float scale)
{
    this->uiScale = clampHudWidgetScale(scale);
    this->refreshTravelWidthFromDrawnRects();
    this->sliderOffsetX = std::clamp(this->sliderOffsetX, 0.0f, this->sliderTravelWidth);
}

void ZoomWidget::setPositionOffset(float offsetX, float offsetY)
{
    this->positionOffset = SDL_FPoint{offsetX, offsetY};
}

SDL_FPoint ZoomWidget::getPositionOffset(void) const
{
    return this->positionOffset;
}

void ZoomWidget::resetPositionOffset(void)
{
    this->positionOffset = SDL_FPoint{0.0f, 0.0f};
}

bool ZoomWidget::pointInRect(float x, float y, const SDL_FRect& rect)
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

SDL_FRect ZoomWidget::computeBarRect(void) const
{
    return scaleRectFromCenter(this->computeBaseBarRect(), this->uiScale);
}

SDL_FRect ZoomWidget::computeBaseBarRect(void) const
{
    if (this->zoomBarWidthPx <= 0.0f || this->zoomBarHeightPx <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    return SDL_FRect{
        safeRect.x + kZoomWidgetScreenMarginPx + this->positionOffset.x,
        safeRect.y + safeRect.h - kZoomWidgetScreenMarginPx - this->zoomBarHeightPx + this->positionOffset.y,
        this->zoomBarWidthPx,
        this->zoomBarHeightPx
    };
}

SDL_FRect ZoomWidget::computeSliderRect(void) const
{
    if (this->zoomSliderWidthPx <= 0.0f || this->zoomSliderHeightPx <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const SDL_FRect barRect = this->computeBarRect();
    if (barRect.w <= 0.0f || barRect.h <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float width = this->zoomSliderWidthPx * this->uiScale;
    const float height = this->zoomSliderHeightPx * this->uiScale;
    const float clampedOffset = std::clamp(this->sliderOffsetX, 0.0f, (std::max)(barRect.w - width, 0.0f));
    return SDL_FRect{
        barRect.x + clampedOffset,
        barRect.y + ((barRect.h - height) * 0.5f),
        width,
        height
    };
}

void ZoomWidget::drawHoveredTooltip(void) const
{
    if (!this->sliderHovered || this->tooltipFont.sdl_font == nullptr)
    {
        return;
    }

    char label[32] = {};
    SDL_snprintf(
        label,
        sizeof(label),
        "Zoom Map : %.0f%%",
        std::round(this->displayedZoomFactor * 100.0f));

    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float textWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    const float textHeight = measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    SDL_FRect tooltipRect = SDL_FRect{
        this->hoveredMouseX + kZoomTooltipOffsetX,
        this->hoveredMouseY + kZoomTooltipOffsetY,
        textWidth + (kZoomTooltipPaddingX * 2.0f),
        textHeight + (kZoomTooltipPaddingY * 2.0f)
    };

    const float maxX = (gameScreenRect.x + gameScreenRect.w) - tooltipRect.w;
    const float maxY = (gameScreenRect.y + gameScreenRect.h) - tooltipRect.h;
    tooltipRect.x = (std::max)(gameScreenRect.x, (std::min)(tooltipRect.x, maxX));
    tooltipRect.y = (std::max)(gameScreenRect.y, (std::min)(tooltipRect.y, maxY));

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kZoomTooltipFill);
    rc2d_graphics_rectangle("fill", &tooltipRect);
    rc2d_graphics_setColor(kZoomTooltipBorder);
    rc2d_graphics_rectangle("line", &tooltipRect);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        label,
        tooltipRect.x + kZoomTooltipPaddingX,
        tooltipRect.y + kZoomTooltipPaddingY,
        kZoomTooltipText);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

