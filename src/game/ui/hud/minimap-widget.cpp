#include "game/ui/hud/minimap-widget.h"

#include "game/assets/title-asset-cache.h"
#include "game/camera/camera.h"
#include "game/map/map.h"

#include <algorithm>
#include <vector>

constexpr RC2D_Color kMiniMapWaterColor = RC2D_Color{28, 63, 103, 255};
constexpr RC2D_Color kMiniMapBorderColor = RC2D_Color{135, 150, 168, 235};
constexpr RC2D_Color kMiniMapViewFillColor = RC2D_Color{125, 198, 255, 55};
constexpr RC2D_Color kMiniMapViewLineColor = RC2D_Color{170, 222, 255, 245};
static constexpr float kHudWidgetScaleMin = 0.75f;
static constexpr float kHudWidgetScaleMax = 1.0f;
static constexpr float kMiniMapMarginRightPercent = 0.03f;
static constexpr float kMiniMapMarginTopPercent = 0.05f;
static constexpr float kMiniMapDefaultOffsetY = 10.0f;

static float clampHudWidgetScale(float scale)
{
    return std::clamp(scale, kHudWidgetScaleMin, kHudWidgetScaleMax);
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

static SDL_FRect getMiniMapRect(const RC2D_ImageData& imageData, const RC2D_Image& image, float scale, const SDL_FPoint& offset)
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

    const float marginRight = visibleRect.w * kMiniMapMarginRightPercent;
    const float marginTop = visibleRect.h * kMiniMapMarginTopPercent;
    return scaleRectFromCenter(
        SDL_FRect{
            visibleRect.x + visibleRect.w - marginRight - width + offset.x,
            visibleRect.y + marginTop + kMiniMapDefaultOffsetY + offset.y,
            width,
            height},
        scale);
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

static bool detectLargestTransparentRect(SDL_Surface* surface, SDL_Rect* outRect)
{
    if (surface == nullptr || outRect == nullptr || surface->w <= 0 || surface->h <= 0)
    {
        return false;
    }

    constexpr Uint8 kTransparentAlphaThreshold = 4;
    const int width = surface->w;
    const int height = surface->h;

    std::vector<int> heights(static_cast<std::size_t>(width), 0);
    SDL_Rect bestRect{0, 0, 0, 0};
    int bestArea = 0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            if (!SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a))
            {
                heights[static_cast<std::size_t>(x)] = 0;
                continue;
            }

            if (a <= kTransparentAlphaThreshold)
            {
                heights[static_cast<std::size_t>(x)] += 1;
            }
            else
            {
                heights[static_cast<std::size_t>(x)] = 0;
            }
        }

        std::vector<int> indexStack;
        indexStack.reserve(static_cast<std::size_t>(width));

        for (int x = 0; x <= width; ++x)
        {
            const int currentHeight =
                (x < width) ? heights[static_cast<std::size_t>(x)] : 0;

            while (!indexStack.empty() &&
                   heights[static_cast<std::size_t>(indexStack.back())] > currentHeight)
            {
                const int topIndex = indexStack.back();
                indexStack.pop_back();

                const int rectHeight = heights[static_cast<std::size_t>(topIndex)];
                const int rectRight = x;
                const int rectLeft = indexStack.empty() ? 0 : (indexStack.back() + 1);
                const int rectWidth = rectRight - rectLeft;
                const int rectArea = rectWidth * rectHeight;
                if (rectArea <= bestArea || rectHeight <= 0 || rectWidth <= 0)
                {
                    continue;
                }

                bestArea = rectArea;
                bestRect.x = rectLeft;
                bestRect.y = y - rectHeight + 1;
                bestRect.w = rectWidth;
                bestRect.h = rectHeight;
            }

            indexStack.push_back(x);
        }
    }

    const int minimumUsefulArea = (width * height) / 20;
    if (bestArea < minimumUsefulArea)
    {
        return false;
    }

    *outRect = bestRect;
    return true;
}

static bool tryBuildMiniMapViewRect(const Map& map, const SDL_FRect& miniMapRect, SDL_FRect* outRect)
{
    if (outRect == nullptr || miniMapRect.w <= 0.0f || miniMapRect.h <= 0.0f)
    {
        return false;
    }

    const float sectorSpanX = static_cast<float>((std::max)(Map::NUM_SECTORS_X, 1));
    const float sectorSpanY = static_cast<float>((std::max)(Map::NUM_SECTORS_Y, 1));
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return false;
    }

    const float viewCenterScreenX = map.rect.x + (map.rect.w * 0.5f);
    const float viewCenterScreenY = map.rect.y + (map.rect.h * 0.5f);
    const SDL_FPoint centerTile = map.screenToTile(viewCenterScreenX, viewCenterScreenY);
    const SDL_FPoint centerSector = map.tileToSectorFloat(centerTile.x, centerTile.y);

    const float halfViewU = (map.rect.w * 0.5f) / halfTileW;
    const float halfViewV = (map.rect.h * 0.5f) / halfTileH;
    const float halfSectorX = halfViewU / (2.0f * static_cast<float>(Map::SECTOR_STEP));
    const float halfSectorY = halfViewV / (2.0f * static_cast<float>(Map::SECTOR_STEP));

    const float minSectorCenterX = centerSector.x - halfSectorX;
    const float maxSectorCenterXVisible = centerSector.x + halfSectorX;
    const float minSectorCenterY = centerSector.y - halfSectorY;
    const float maxSectorCenterYVisible = centerSector.y + halfSectorY;

    float minSectorEdgeX = std::clamp(minSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float maxSectorEdgeX = std::clamp(maxSectorCenterXVisible + 0.5f, 0.0f, sectorSpanX);
    float minSectorEdgeY = std::clamp(minSectorCenterY + 0.5f, 0.0f, sectorSpanY);
    float maxSectorEdgeY = std::clamp(maxSectorCenterYVisible + 0.5f, 0.0f, sectorSpanY);

    if (maxSectorEdgeX < minSectorEdgeX)
    {
        std::swap(minSectorEdgeX, maxSectorEdgeX);
    }
    if (maxSectorEdgeY < minSectorEdgeY)
    {
        std::swap(minSectorEdgeY, maxSectorEdgeY);
    }

    SDL_FRect viewRect{};
    viewRect.x = miniMapRect.x + ((minSectorEdgeX / sectorSpanX) * miniMapRect.w);
    viewRect.y = miniMapRect.y + ((minSectorEdgeY / sectorSpanY) * miniMapRect.h);
    viewRect.w = ((maxSectorEdgeX - minSectorEdgeX) / sectorSpanX) * miniMapRect.w;
    viewRect.h = ((maxSectorEdgeY - minSectorEdgeY) / sectorSpanY) * miniMapRect.h;

    viewRect.w = (std::max)(viewRect.w, 2.0f);
    viewRect.h = (std::max)(viewRect.h, 2.0f);
    viewRect.x = std::clamp(viewRect.x, miniMapRect.x, miniMapRect.x + miniMapRect.w - viewRect.w);
    viewRect.y = std::clamp(viewRect.y, miniMapRect.y, miniMapRect.y + miniMapRect.h - viewRect.h);

    *outRect = viewRect;
    return true;
}

static float miniMapNormalizedToSectorCenter(float normalizedValue, int sectorCount)
{
    const float span = static_cast<float>((std::max)(sectorCount, 1));
    const float maxSectorCenter = static_cast<float>((std::max)(sectorCount - 1, 0));
    return std::clamp((normalizedValue * span) - 0.5f, 0.0f, maxSectorCenter);
}

static SDL_FRect getScaledContentRect(
    const RC2D_ImageData& imageData,
    const RC2D_Image& image,
    const SDL_Rect& sourceRect,
    float scale,
    const SDL_FPoint& offset)
{
    if (sourceRect.w <= 0 || sourceRect.h <= 0)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const SDL_FRect widgetRect = getMiniMapRect(imageData, image, scale, offset);
    if (widgetRect.w <= 0.0f || widgetRect.h <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    float sourceWidth = 0.0f;
    float sourceHeight = 0.0f;
    if (!getImageSize(imageData, image, &sourceWidth, &sourceHeight) ||
        sourceWidth <= 0.0f ||
        sourceHeight <= 0.0f)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float scaleX = widgetRect.w / sourceWidth;
    const float scaleY = widgetRect.h / sourceHeight;
    return SDL_FRect{
        widgetRect.x + (static_cast<float>(sourceRect.x) * scaleX),
        widgetRect.y + (static_cast<float>(sourceRect.y) * scaleY),
        static_cast<float>(sourceRect.w) * scaleX,
        static_cast<float>(sourceRect.h) * scaleY
    };
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

MinimapWidget::MinimapWidget(void)
    : minimapImage{},
      minimapImageData{},
      minimapContentSourceRect{0, 0, 0, 0},
      hasMinimapContentRect(false),
      minimapDragActive(false),
      minimapDragOffsetX(0.0f),
      minimapDragOffsetY(0.0f),
      uiScale(1.0f),
      positionOffset{0.0f, 0.0f}
{
}

MinimapWidget::~MinimapWidget(void)
{
}

void MinimapWidget::load(void)
{
    this->minimapImage = LoadStorageImage(
        "assets/images/ui-scene-game/minimap.png",
        RC2D_STORAGE_TITLE);
    this->minimapImageData = LoadStorageImageData(
        "assets/images/ui-scene-game/minimap.png",
        RC2D_STORAGE_TITLE);
    this->hasMinimapContentRect = detectLargestTransparentRect(
        this->minimapImageData.sdl_surface,
        &this->minimapContentSourceRect);
}

void MinimapWidget::update(Camera& camera, Map& map)
{
    if (!this->minimapDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->minimapDragActive = false;
        this->minimapDragOffsetX = 0.0f;
        this->minimapDragOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    if (!this->containsContentPoint(mouseX, mouseY))
    {
        return;
    }

    this->moveCameraFromMiniMapPoint(mouseX, mouseY, true, camera, map);
}

void MinimapWidget::unload(void)
{
    this->hasMinimapContentRect = false;
    this->minimapContentSourceRect = SDL_Rect{0, 0, 0, 0};
    this->minimapDragActive = false;
    this->minimapDragOffsetX = 0.0f;
    this->minimapDragOffsetY = 0.0f;
    ResetStorageImageDataRef(&this->minimapImageData);
    ResetStorageImageRef(&this->minimapImage);
}

void MinimapWidget::draw(const Map& map) const
{
    if (this->minimapImage.sdl_texture == nullptr)
    {
        return;
    }

    const SDL_FRect minimapContentRect = this->getContentRect();
    if (minimapContentRect.w > 0.0f && minimapContentRect.h > 0.0f)
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(kMiniMapWaterColor);
        rc2d_graphics_rectangle("fill", &minimapContentRect);
        rc2d_graphics_setColor(kMiniMapBorderColor);
        rc2d_graphics_rectangle("line", &minimapContentRect);

        SDL_FRect viewRect{};
        if (tryBuildMiniMapViewRect(map, minimapContentRect, &viewRect))
        {
            rc2d_graphics_setColor(kMiniMapViewFillColor);
            rc2d_graphics_rectangle("fill", &viewRect);
            rc2d_graphics_setColor(kMiniMapViewLineColor);
            rc2d_graphics_rectangle("line", &viewRect);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }

    drawImageToRect(
        &const_cast<RC2D_Image&>(this->minimapImage),
        this->getCurrentRect());
}

void MinimapWidget::setUiScale(float scale)
{
    this->uiScale = clampHudWidgetScale(scale);
}

void MinimapWidget::setPositionOffset(float offsetX, float offsetY)
{
    this->positionOffset = SDL_FPoint{offsetX, offsetY};
}

SDL_FPoint MinimapWidget::getPositionOffset(void) const
{
    return this->positionOffset;
}

void MinimapWidget::resetPositionOffset(void)
{
    this->positionOffset = SDL_FPoint{0.0f, 0.0f};
}

SDL_FRect MinimapWidget::getCurrentRect(void) const
{
    if (this->minimapImage.sdl_texture == nullptr)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    return getMiniMapRect(this->minimapImageData, this->minimapImage, this->uiScale, this->positionOffset);
}

SDL_FRect MinimapWidget::getContentRect(void) const
{
    if (!this->hasMinimapContentRect ||
        this->minimapImage.sdl_texture == nullptr)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    return getScaledContentRect(this->minimapImageData, this->minimapImage, this->minimapContentSourceRect, this->uiScale, this->positionOffset);
}

bool MinimapWidget::containsContentPoint(float x, float y) const
{
    return pointInRect(x, y, this->getContentRect());
}

bool MinimapWidget::mousepressed(float x, float y, RC2D_MouseButton button, Camera& camera, Map& map)
{
    if (!pointInRect(x, y, this->getCurrentRect()))
    {
        return false;
    }

    if (!this->containsContentPoint(x, y))
    {
        this->minimapDragActive = false;
        this->minimapDragOffsetX = 0.0f;
        this->minimapDragOffsetY = 0.0f;
        return true;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    const SDL_FRect minimapContentRect = this->getContentRect();
    SDL_FRect viewRect{};
    if (tryBuildMiniMapViewRect(map, minimapContentRect, &viewRect) &&
        pointInRect(x, y, viewRect))
    {
        const float viewCenterX = viewRect.x + (viewRect.w * 0.5f);
        const float viewCenterY = viewRect.y + (viewRect.h * 0.5f);
        this->minimapDragOffsetX = x - viewCenterX;
        this->minimapDragOffsetY = y - viewCenterY;
    }
    else
    {
        this->minimapDragOffsetX = 0.0f;
        this->minimapDragOffsetY = 0.0f;
        this->moveCameraFromMiniMapPoint(x, y, true, camera, map);
    }

    this->minimapDragActive = true;
    return true;
}

void MinimapWidget::moveCameraFromMiniMapPoint(
    float miniMapX,
    float miniMapY,
    bool applyDragOffset,
    Camera& camera,
    Map& map) const
{
    const SDL_FRect minimapContentRect = this->getContentRect();
    if (minimapContentRect.w <= 0.0f || minimapContentRect.h <= 0.0f)
    {
        return;
    }

    float localX = miniMapX - minimapContentRect.x;
    float localY = miniMapY - minimapContentRect.y;
    if (applyDragOffset)
    {
        localX -= this->minimapDragOffsetX;
        localY -= this->minimapDragOffsetY;
    }

    const float nx = std::clamp(localX / (std::max)(minimapContentRect.w, 1.0f), 0.0f, 1.0f);
    const float ny = std::clamp(localY / (std::max)(minimapContentRect.h, 1.0f), 0.0f, 1.0f);
    const float targetSectorX = miniMapNormalizedToSectorCenter(nx, Map::NUM_SECTORS_X);
    const float targetSectorY = miniMapNormalizedToSectorCenter(ny, Map::NUM_SECTORS_Y);
    const float targetTileX =
        static_cast<float>(Map::SECTOR_BASE_X) +
        ((targetSectorX + targetSectorY) * static_cast<float>(Map::SECTOR_STEP));
    const float targetTileY =
        static_cast<float>(Map::SECTOR_BASE_Y) +
        ((targetSectorY - targetSectorX) * static_cast<float>(Map::SECTOR_STEP));

    camera.centerCameraOnTile(targetTileX, targetTileY, map, map.rect);
    camera.update(map, map.rect);
}
