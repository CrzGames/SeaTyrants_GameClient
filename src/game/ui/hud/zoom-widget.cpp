#include "game/ui/hud/zoom-widget.h"

#include <algorithm>
#include <cmath>

#include "game/camera.h"

static constexpr float kZoomWidgetScreenMarginPx = 5.0f;

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

static bool loadUiImageWithFallback(
    RC2D_UIImage* uiImage,
    const char* primaryPath,
    const char* fallbackPath)
{
    if (uiImage == nullptr)
    {
        return false;
    }

    auto loadFromPath = [uiImage](const char* path) -> bool
    {
        uiImage->image = rc2d_graphics_loadImageFromStorage(path, RC2D_STORAGE_TITLE);
        uiImage->imageData = rc2d_graphics_loadImageDataFromStorage(path, RC2D_STORAGE_TITLE);
        return (uiImage->image.sdl_texture != nullptr && uiImage->imageData.sdl_surface != nullptr);
    };

    if (loadFromPath(primaryPath))
    {
        return true;
    }

    rc2d_graphics_freeImageData(&uiImage->imageData);
    rc2d_graphics_freeImage(&uiImage->image);

    if (fallbackPath != nullptr && loadFromPath(fallbackPath))
    {
        return true;
    }

    rc2d_graphics_freeImageData(&uiImage->imageData);
    rc2d_graphics_freeImage(&uiImage->image);
    return false;
}

ZoomWidget::ZoomWidget(void)
    : zoomBarUi{},
      zoomSliderUi{},
      sliderDragging(false),
      sliderDragGrabOffsetX(0.0f),
      sliderOffsetX(0.0f),
      sliderTravelWidth(0.0f),
      zoomBarWidthPx(0.0f),
      zoomBarHeightPx(0.0f),
      zoomSliderWidthPx(0.0f),
      zoomSliderHeightPx(0.0f)
{
}

ZoomWidget::~ZoomWidget(void)
{
}

void ZoomWidget::load(void)
{
    // Barre: compat avec l'ancien nom "zoom-barre.png".
    if (!loadUiImageWithFallback(
            &this->zoomBarUi,
            "assets/images/ui-scene-game/zoom-bar.png",
            "assets/images/ui-scene-game/zoom-barre.png"))
    {
        RC2D_log(RC2D_LOG_WARN, "ZoomWidget: echec chargement zoom-bar(.png)/zoom-barre.png");
    }

    if (!loadUiImageWithFallback(
            &this->zoomSliderUi,
            "assets/images/ui-scene-game/zoom-slider.png",
            nullptr))
    {
        RC2D_log(RC2D_LOG_WARN, "ZoomWidget: echec chargement zoom-slider.png");
    }

    this->zoomBarUi.anchor = RC2D_UI_ANCHOR_BOTTOM_LEFT;
    this->zoomBarUi.margin_mode = RC2D_UI_MARGIN_PIXELS;
    this->zoomBarUi.margin_x = kZoomWidgetScreenMarginPx;
    this->zoomBarUi.margin_y = kZoomWidgetScreenMarginPx;
    this->zoomBarUi.visible = true;
    this->zoomBarUi.hittable = true;

    this->zoomSliderUi.anchor = RC2D_UI_ANCHOR_BOTTOM_LEFT;
    this->zoomSliderUi.margin_mode = RC2D_UI_MARGIN_PIXELS;
    this->zoomSliderUi.margin_x = kZoomWidgetScreenMarginPx;
    this->zoomSliderUi.margin_y = kZoomWidgetScreenMarginPx;
    this->zoomSliderUi.visible = true;
    this->zoomSliderUi.hittable = true;

    this->zoomBarWidthPx =
        (this->zoomBarUi.imageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomBarUi.imageData.sdl_surface->w)
            : 0.0f;
    this->zoomBarHeightPx =
        (this->zoomBarUi.imageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomBarUi.imageData.sdl_surface->h)
            : 0.0f;

    this->zoomSliderWidthPx =
        (this->zoomSliderUi.imageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomSliderUi.imageData.sdl_surface->w)
            : 0.0f;
    this->zoomSliderHeightPx =
        (this->zoomSliderUi.imageData.sdl_surface != nullptr)
            ? static_cast<float>(this->zoomSliderUi.imageData.sdl_surface->h)
            : 0.0f;

    // Position initiale: slider tout a droite (zoom max camera).
    this->sliderTravelWidth = (std::max)(this->zoomBarWidthPx - this->zoomSliderWidthPx, 0.0f);
    this->sliderOffsetX = this->sliderTravelWidth;
    this->sliderDragging = false;
    this->sliderDragGrabOffsetX = 0.0f;
}

void ZoomWidget::unload(void)
{
    rc2d_graphics_freeImageData(&this->zoomSliderUi.imageData);
    rc2d_graphics_freeImage(&this->zoomSliderUi.image);
    rc2d_graphics_freeImageData(&this->zoomBarUi.imageData);
    rc2d_graphics_freeImage(&this->zoomBarUi.image);
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
    if (this->zoomBarUi.last_drawn_rect.w > 0.0f && this->zoomSliderUi.last_drawn_rect.w > 0.0f)
    {
        this->sliderTravelWidth =
            (std::max)(this->zoomBarUi.last_drawn_rect.w - this->zoomSliderUi.last_drawn_rect.w, 0.0f);
    }
    else
    {
        this->sliderTravelWidth = (std::max)(this->zoomBarWidthPx - this->zoomSliderWidthPx, 0.0f);
    }
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
    // Pas de drag actif: le slider suit les autres changements de zoom (clavier, code, etc.).
    if (!this->sliderDragging)
    {
        this->syncSliderFromCameraZoom(camera);
        return;
    }

    // Fin de drag si le bouton gauche est relache.
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->sliderDragging = false;
        this->syncSliderFromCameraZoom(camera);
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    this->refreshTravelWidthFromDrawnRects();

    // Position slider en coords locales de barre, puis snap sur les crans de zoom.
    const float rawOffset =
        (mouseX - this->zoomBarUi.last_drawn_rect.x) - this->sliderDragGrabOffsetX;
    this->setSliderOffsetFromRawValue(rawOffset);
    this->applySliderToCameraZoom(camera);
}

void ZoomWidget::draw(void)
{
    if (this->zoomBarUi.image.sdl_texture == nullptr || this->zoomSliderUi.image.sdl_texture == nullptr)
    {
        return;
    }

    // Barre: 5 px du bas/gauche.
    this->zoomBarUi.margin_x = kZoomWidgetScreenMarginPx;
    this->zoomBarUi.margin_y = kZoomWidgetScreenMarginPx;
    rc2d_ui_drawImage(&this->zoomBarUi);

    // Slider: sur la barre (centre vertical), avec offset horizontal pilote par le zoom.
    this->refreshTravelWidthFromDrawnRects();
    this->sliderOffsetX = std::clamp(this->sliderOffsetX, 0.0f, this->sliderTravelWidth);

    this->zoomSliderUi.margin_x = this->zoomBarUi.margin_x + this->sliderOffsetX;
    const float centeredYOffset = (this->zoomBarUi.last_drawn_rect.h - this->zoomSliderUi.last_drawn_rect.h) * 0.5f;
    this->zoomSliderUi.margin_y = this->zoomBarUi.margin_y + centeredYOffset;
    rc2d_ui_drawImage(&this->zoomSliderUi);
}

bool ZoomWidget::mousepressed(float x, float y, RC2D_MouseButton button)
{
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    if (!ZoomWidget::pointInRect(x, y, this->zoomSliderUi.last_drawn_rect))
    {
        return false;
    }

    this->sliderDragging = true;
    this->sliderDragGrabOffsetX = x - this->zoomSliderUi.last_drawn_rect.x;
    this->sliderDragGrabOffsetX =
        std::clamp(this->sliderDragGrabOffsetX, 0.0f, this->zoomSliderUi.last_drawn_rect.w);
    return true;
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

