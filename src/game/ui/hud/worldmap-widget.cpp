#include "game/ui/hud/worldmap-widget.h"

#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <RC2D/RC2D_keyboard.h>
#include <RC2D/RC2D_memory.h>
#include <RC2D/RC2D_storage.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

#include <cJSON.h>

static constexpr float kWorldMapWidgetMinViewportWidth = 420.0f;
static constexpr float kWorldMapWidgetMinViewportHeight = 280.0f;
static constexpr float kWorldMapWidgetMarginX = 42.0f;
static constexpr float kWorldMapWidgetMarginY = 52.0f;
static constexpr float kWorldMapWidgetHeaderHeight = 30.0f;
static constexpr float kWorldMapWidgetBodyInset = 10.0f;
static constexpr float kWorldMapWidgetLegendGap = 8.0f;
static constexpr float kWorldMapWidgetViewportGap = kWorldMapWidgetLegendGap;
static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 255};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
static constexpr RC2D_Color kCaseFill = RC2D_Color{24, 31, 40, 236};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};
static constexpr float kWorldMapLegendDotSize = 10.0f;
static constexpr float kWorldMapLegendItemGap = 18.0f;
static constexpr float kWorldMapLegendRowGap = 6.0f;
static constexpr float kWorldMapLegendPaddingX = 10.0f;
static constexpr float kWorldMapLegendPaddingY = 4.0f;
static constexpr float kWorldMapLegendMinHeight = 20.0f;

static std::string trimAsciiWorldMap(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    return value;
}

static bool isPointInRectWorldMap(float x, float y, const SDL_FRect& rect)
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

static void getMouseRenderPositionWorldMap(float* outX, float* outY)
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

static void drawLeftCenteredTextWorldMap(RC2D_Font* font, const char* text, const SDL_FRect& rect, float drawX, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text renderedText = rc2d_graphics_createText(font, text);
    renderedText.color = color;
    rc2d_graphics_setTextColor(&renderedText);
    int textWidth = 0;
    int textHeight = 0;
    rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);
    rc2d_graphics_drawText(
        &renderedText,
        std::round(drawX),
        std::round(rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f)));
    rc2d_graphics_destroyText(&renderedText);
}

static void drawCenteredTextWorldMap(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text renderedText = rc2d_graphics_createText(font, text);
    renderedText.color = color;
    rc2d_graphics_setTextColor(&renderedText);
    int textWidth = 0;
    int textHeight = 0;
    rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);
    rc2d_graphics_drawText(
        &renderedText,
        std::round(rect.x + ((rect.w - static_cast<float>(textWidth)) * 0.5f)),
        std::round(rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f)));
    rc2d_graphics_destroyText(&renderedText);
}

static float computeLegendHeightWorldMap(
    const RC2D_Font* font,
    const WorldMapDefinition::LegendVisibility& legendVisibility,
    float availableWidth)
{
    const float contentWidth = (std::max)(1.0f, availableWidth - (kWorldMapLegendPaddingX * 2.0f));
    if (font == nullptr || font->sdl_font == nullptr)
    {
        return kWorldMapLegendMinHeight;
    }

    if (!WorldMapDefinition::hasAnyLegendVisible(legendVisibility))
    {
        RC2D_Text text = rc2d_graphics_createText(const_cast<RC2D_Font*>(font), "Legende masquee.");
        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&text, &textWidth, &textHeight);
        rc2d_graphics_destroyText(&text);
        return (std::max)(kWorldMapLegendMinHeight, (kWorldMapLegendPaddingY * 2.0f) + static_cast<float>(textHeight));
    }

    float usedHeight = 0.0f;
    float rowWidth = 0.0f;
    float rowHeight = 0.0f;
    bool hasRow = false;
    for (const WorldMapDefinition::ZoneType zoneType : WorldMapDefinition::getZoneOrder())
    {
        if (!WorldMapDefinition::isLegendVisible(legendVisibility, zoneType))
        {
            continue;
        }

        const WorldMapDefinition::ZoneVisual zoneVisual =
            WorldMapDefinition::getZoneVisual(zoneType);
        RC2D_Text text = rc2d_graphics_createText(const_cast<RC2D_Font*>(font), zoneVisual.legendLabel);
        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&text, &textWidth, &textHeight);
        rc2d_graphics_destroyText(&text);

        const float itemHeight = (std::max)(kWorldMapLegendDotSize, static_cast<float>(textHeight));
        const float itemWidth = kWorldMapLegendDotSize + 6.0f + static_cast<float>(textWidth) + kWorldMapLegendItemGap;
        if (hasRow && (rowWidth + itemWidth) > contentWidth)
        {
            usedHeight += rowHeight + kWorldMapLegendRowGap;
            rowWidth = 0.0f;
            rowHeight = 0.0f;
            hasRow = false;
        }

        rowWidth += itemWidth;
        rowHeight = hasRow ? (std::max)(rowHeight, itemHeight) : itemHeight;
        hasRow = true;
    }

    if (hasRow)
    {
        usedHeight += rowHeight;
    }

    return (std::max)(kWorldMapLegendMinHeight, (kWorldMapLegendPaddingY * 2.0f) + usedHeight);
}

WorldMapWidget::WorldMapWidget(void)
    : titleFont{},
      bodyFont{},
      smallFont{},
      controlIcons{},
      widgetRect{0.0f, 0.0f, 0.0f, 0.0f},
      headerRect{0.0f, 0.0f, 0.0f, 0.0f},
      closeButtonRect{0.0f, 0.0f, 0.0f, 0.0f},
      legendRect{0.0f, 0.0f, 0.0f, 0.0f},
      viewportRect{0.0f, 0.0f, 0.0f, 0.0f},
      visible(false),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      guiWidth(660.0f),
      guiHeight(640.0f),
      renderScale(1.0f),
      renderOffset{0.0f, 0.0f},
      loadedStoragePath{},
      mapCases{},
      mapLinks{},
      legendVisibility{},
      guildTagsByMapName{}
{
}

WorldMapWidget::~WorldMapWidget(void)
{
}

void WorldMapWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->smallFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 13.0f);
    this->controlIcons.load();

    this->visible = false;
    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->guiWidth = 660.0f;
    this->guiHeight = 640.0f;
    this->renderScale = 1.0f;
    this->renderOffset = SDL_FPoint{0.0f, 0.0f};
    this->loadedStoragePath.clear();
    this->legendVisibility = WorldMapDefinition::LegendVisibility{};
    this->clearLoadedWorldMapData();
    (void)this->loadFromTitleStorageJson("assets/data/worldmap.json");
    this->updateWidgetRect();
    this->updateDerivedRects();
}

void WorldMapWidget::unload(void)
{
    this->clearLoadedWorldMapData();
    this->guildTagsByMapName.clear();
    this->loadedStoragePath.clear();
    this->controlIcons.unload();
    ResetStorageFontRef(&this->smallFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void WorldMapWidget::update(double dt)
{
    (void)dt;

    this->updateWidgetRect();
    this->updateDerivedRects();
    if (!this->visible || !this->widgetDragging)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPositionWorldMap(&mouseX, &mouseY);

    const SDL_FRect baseRect = this->getBaseRectFromGameScreen();
    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    this->updateWidgetRect();
    this->updateDerivedRects();
}

void WorldMapWidget::draw(void) const
{
    WorldMapWidget* self = const_cast<WorldMapWidget*>(this);
    self->updateWidgetRect();
    self->updateDerivedRects();
    if (!self->visible)
    {
        return;
    }

    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &self->headerRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &self->headerRect);
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &self->legendRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &self->legendRect);
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &self->viewportRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &self->viewportRect);

    drawLeftCenteredTextWorldMap(
        &self->titleFont,
        "Carte du monde",
        self->headerRect,
        self->headerRect.x + 10.0f,
        kTextGold);
    self->controlIcons.drawCloseButton(self->closeButtonRect, kHeaderFill, kGold);
    if (WorldMapDefinition::hasAnyLegendVisible(self->legendVisibility))
    {
        float cursorX = self->legendRect.x + kWorldMapLegendPaddingX;
        float cursorY = self->legendRect.y + kWorldMapLegendPaddingY;

        for (const WorldMapDefinition::ZoneType zoneType : WorldMapDefinition::getZoneOrder())
        {
            if (!WorldMapDefinition::isLegendVisible(self->legendVisibility, zoneType))
            {
                continue;
            }

            const WorldMapDefinition::ZoneVisual zoneVisual =
                WorldMapDefinition::getZoneVisual(zoneType);
            RC2D_Text labelText =
                rc2d_graphics_createText(&self->smallFont, zoneVisual.legendLabel);
            labelText.color = kTextGold;
            rc2d_graphics_setTextColor(&labelText);
            int textWidth = 0;
            int textHeight = 0;
            rc2d_graphics_getTextSize(&labelText, &textWidth, &textHeight);
            const float rowHeight = (std::max)(kWorldMapLegendDotSize, static_cast<float>(textHeight));
            const float itemWidth = kWorldMapLegendDotSize + 6.0f + static_cast<float>(textWidth) + kWorldMapLegendItemGap;
            if (cursorX > (self->legendRect.x + kWorldMapLegendPaddingX) &&
                (cursorX + itemWidth) > (self->legendRect.x + self->legendRect.w - kWorldMapLegendPaddingX))
            {
                cursorX = self->legendRect.x + kWorldMapLegendPaddingX;
                cursorY += rowHeight + kWorldMapLegendRowGap;
            }

            const SDL_FRect dotRect{
                cursorX,
                cursorY + ((rowHeight - kWorldMapLegendDotSize) * 0.5f),
                kWorldMapLegendDotSize,
                kWorldMapLegendDotSize
            };
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            rc2d_graphics_setColor(zoneVisual.borderColor);
            rc2d_graphics_rectangle("fill", &dotRect);
            rc2d_graphics_drawText(
                &labelText,
                std::round(dotRect.x + dotRect.w + 6.0f),
                std::round(cursorY + ((rowHeight - static_cast<float>(textHeight)) * 0.5f)));
            rc2d_graphics_destroyText(&labelText);
            cursorX += itemWidth;
        }
    }
    else
    {
        drawCenteredTextWorldMap(&self->smallFont, "Legende masquee.", self->legendRect, kTextWhite);
    }

    const SDL_FRect logicalCanvasRect{
        self->viewportRect.x + self->renderOffset.x,
        self->viewportRect.y + self->renderOffset.y,
        self->guiWidth * self->renderScale,
        self->guiHeight * self->renderScale
    };
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &logicalCanvasRect);

    for (const MapLink& link : self->mapLinks)
    {
        if (link.fromCaseIndex < 0 ||
            link.toCaseIndex < 0 ||
            link.fromCaseIndex >= static_cast<int>(self->mapCases.size()) ||
            link.toCaseIndex >= static_cast<int>(self->mapCases.size()))
        {
            continue;
        }

        const MapCase& fromCase = self->mapCases[static_cast<size_t>(link.fromCaseIndex)];
        const MapCase& toCase = self->mapCases[static_cast<size_t>(link.toCaseIndex)];
        const SDL_FRect fromRect{
            logicalCanvasRect.x + (fromCase.x * self->renderScale),
            logicalCanvasRect.y + (fromCase.y * self->renderScale),
            fromCase.width * self->renderScale,
            fromCase.height * self->renderScale
        };
        const SDL_FRect toRect{
            logicalCanvasRect.x + (toCase.x * self->renderScale),
            logicalCanvasRect.y + (toCase.y * self->renderScale),
            toCase.width * self->renderScale,
            toCase.height * self->renderScale
        };
        const SDL_FPoint start = self->getLinkAnchor(fromRect, link.fromSide);
        const SDL_FPoint end = self->getLinkAnchor(toRect, self->getOppositeLinkSide(link.fromSide));
        const RC2D_Color lineColor = WorldMapDefinition::getNeutralLinkColor();

        rc2d_graphics_setColor(lineColor);
        if (link.fromSide == LinkSide::LEFT || link.fromSide == LinkSide::RIGHT)
        {
            const float bendX = std::round((start.x + end.x) * 0.5f);
            rc2d_graphics_line(start.x, start.y, bendX, start.y);
            rc2d_graphics_line(bendX, start.y, bendX, end.y);
            rc2d_graphics_line(bendX, end.y, end.x, end.y);
        }
        else
        {
            const float bendY = std::round((start.y + end.y) * 0.5f);
            rc2d_graphics_line(start.x, start.y, start.x, bendY);
            rc2d_graphics_line(start.x, bendY, end.x, bendY);
            rc2d_graphics_line(end.x, bendY, end.x, end.y);
        }
    }

    for (const MapCase& mapCase : self->mapCases)
    {
        const SDL_FRect caseRect{
            logicalCanvasRect.x + (mapCase.x * self->renderScale),
            logicalCanvasRect.y + (mapCase.y * self->renderScale),
            mapCase.width * self->renderScale,
            mapCase.height * self->renderScale
        };
        const SDL_FRect innerCaseRect{
            caseRect.x + 2.0f,
            caseRect.y + 2.0f,
            caseRect.w - 4.0f,
            caseRect.h - 4.0f
        };

        rc2d_graphics_setColor(kCaseFill);
        rc2d_graphics_rectangle("fill", &caseRect);
        const RC2D_Color baseBorderColor =
            WorldMapDefinition::getZoneVisual(mapCase.zoneType).borderColor;
        rc2d_graphics_setColor(baseBorderColor);
        rc2d_graphics_rectangle("line", &caseRect);
        rc2d_graphics_setColor(RC2D_Color{90, 101, 114, 180});
        rc2d_graphics_rectangle("line", &innerCaseRect);

        const std::string guildTag = self->getGuildTagLabelForCase(mapCase);
        const float headerHeight = mapCase.showGuildTag ? (caseRect.h * 0.22f) : 0.0f;
        if (mapCase.showGuildTag && !guildTag.empty())
        {
            const SDL_FRect guildRect{
                caseRect.x + 6.0f,
                caseRect.y + 4.0f,
                caseRect.w - 12.0f,
                headerHeight
            };
            drawCenteredTextWorldMap(&self->smallFont, guildTag.c_str(), guildRect, baseBorderColor);
        }

        const SDL_FRect nameRect{
            caseRect.x + 6.0f,
            caseRect.y + 4.0f + headerHeight,
            caseRect.w - 12.0f,
            caseRect.h - 8.0f - headerHeight
        };
        drawCenteredTextWorldMap(&self->bodyFont, mapCase.mapName.c_str(), nameRect, kTextGold);
    }

    if (self->mapCases.empty())
    {
        drawCenteredTextWorldMap(
            &self->bodyFont,
            "Aucune donnee de carte du monde chargee.",
            self->viewportRect,
            kTextWhite);
    }

    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &self->viewportRect);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool WorldMapWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    this->updateWidgetRect();
    this->updateDerivedRects();
    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }
    if (!this->containsPoint(x, y))
    {
        return false;
    }
    if (isPointInRectWorldMap(x, y, this->closeButtonRect))
    {
        this->hide();
        return true;
    }
    if (isPointInRectWorldMap(x, y, this->headerRect))
    {
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }
    return true;
}

void WorldMapWidget::show(void)
{
    this->visible = true;
    this->updateWidgetRect();
    this->updateDerivedRects();
}

void WorldMapWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
}

bool WorldMapWidget::containsPoint(float x, float y) const
{
    return isPointInRectWorldMap(x, y, this->widgetRect);
}

HudCursorType WorldMapWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }
    if (isPointInRectWorldMap(x, y, this->closeButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (isPointInRectWorldMap(x, y, this->headerRect))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::NONE;
}

bool WorldMapWidget::loadFromTitleStorageJson(const char* storagePath)
{
    std::string jsonText;
    if (!this->readTitleTextFile(storagePath, &jsonText))
    {
        this->clearLoadedWorldMapData();
        return false;
    }

    cJSON* root = cJSON_Parse(jsonText.c_str());
    if (root == nullptr)
    {
        this->clearLoadedWorldMapData();
        return false;
    }

    std::vector<MapCase> importedCases;
    std::vector<MapLink> importedLinks;

    const cJSON* guiItem = cJSON_GetObjectItemCaseSensitive(root, "gui");
    const cJSON* guiWidthItem = cJSON_IsObject(guiItem) ? cJSON_GetObjectItemCaseSensitive(guiItem, "width") : nullptr;
    const cJSON* guiHeightItem = cJSON_IsObject(guiItem) ? cJSON_GetObjectItemCaseSensitive(guiItem, "height") : nullptr;
    if (cJSON_IsNumber(guiWidthItem))
    {
        this->guiWidth = static_cast<float>(guiWidthItem->valuedouble);
    }
    if (cJSON_IsNumber(guiHeightItem))
    {
        this->guiHeight = static_cast<float>(guiHeightItem->valuedouble);
    }

    WorldMapDefinition::LegendVisibility importedLegendVisibility{};
    bool hasExplicitLegendVisibility = false;
    const cJSON* legendItem = cJSON_GetObjectItemCaseSensitive(root, "legend");
    if (cJSON_IsObject(legendItem))
    {
        hasExplicitLegendVisibility = true;
        const cJSON* showTerrestrialItem = cJSON_GetObjectItemCaseSensitive(legendItem, "showTerrestrialZone");
        const cJSON* showCityItem = cJSON_GetObjectItemCaseSensitive(legendItem, "showCity");
        const cJSON* showNordicItem = cJSON_GetObjectItemCaseSensitive(legendItem, "showNordicZone");
        const cJSON* showCelestialItem = cJSON_GetObjectItemCaseSensitive(legendItem, "showCelestialZone");
        const cJSON* showVolcanicItem = cJSON_GetObjectItemCaseSensitive(legendItem, "showVolcanicZone");
        if (cJSON_IsBool(showTerrestrialItem))
        {
            importedLegendVisibility.showTerrestrial = cJSON_IsTrue(showTerrestrialItem);
        }
        if (cJSON_IsBool(showCityItem))
        {
            importedLegendVisibility.showCity = cJSON_IsTrue(showCityItem);
        }
        if (cJSON_IsBool(showNordicItem))
        {
            importedLegendVisibility.showNordic = cJSON_IsTrue(showNordicItem);
        }
        if (cJSON_IsBool(showCelestialItem))
        {
            importedLegendVisibility.showCelestial = cJSON_IsTrue(showCelestialItem);
        }
        if (cJSON_IsBool(showVolcanicItem))
        {
            importedLegendVisibility.showVolcanic = cJSON_IsTrue(showVolcanicItem);
        }
    }

    const cJSON* casesArray = cJSON_GetObjectItemCaseSensitive(root, "cases");
    if (!cJSON_IsArray(casesArray))
    {
        cJSON_Delete(root);
        this->clearLoadedWorldMapData();
        return false;
    }

    struct PendingLink {
        int fromCaseIndex = -1;
        std::string targetMapName;
        LinkSide fromSide = LinkSide::RIGHT;
    };
    std::vector<PendingLink> pendingLinks;

    cJSON* caseItem = nullptr;
    cJSON_ArrayForEach(caseItem, casesArray)
    {
        if (!cJSON_IsObject(caseItem))
        {
            continue;
        }

        MapCase mapCase{};
        const cJSON* xItem = cJSON_GetObjectItemCaseSensitive(caseItem, "x");
        const cJSON* yItem = cJSON_GetObjectItemCaseSensitive(caseItem, "y");
        const cJSON* widthItem = cJSON_GetObjectItemCaseSensitive(caseItem, "width");
        const cJSON* heightItem = cJSON_GetObjectItemCaseSensitive(caseItem, "height");
        const cJSON* mapNameItem = cJSON_GetObjectItemCaseSensitive(caseItem, "mapName");
        const cJSON* typeItem = cJSON_GetObjectItemCaseSensitive(caseItem, "type");
        const cJSON* guildTagItem = cJSON_GetObjectItemCaseSensitive(caseItem, "guildTag");

        mapCase.x = cJSON_IsNumber(xItem) ? static_cast<float>(xItem->valuedouble) : 0.0f;
        mapCase.y = cJSON_IsNumber(yItem) ? static_cast<float>(yItem->valuedouble) : 0.0f;
        mapCase.width = cJSON_IsNumber(widthItem) ? static_cast<float>(widthItem->valuedouble) : 80.0f;
        mapCase.height = cJSON_IsNumber(heightItem) ? static_cast<float>(heightItem->valuedouble) : 67.0f;
        mapCase.mapName =
            (cJSON_IsString(mapNameItem) && mapNameItem->valuestring != nullptr)
                ? trimAsciiWorldMap(mapNameItem->valuestring)
                : std::string("MAP");
        mapCase.zoneType =
            cJSON_IsString(typeItem) && typeItem->valuestring != nullptr
                ? WorldMapDefinition::parseZoneTypeId(typeItem->valuestring)
                : WorldMapDefinition::ZoneType::TERRESTRIAL;

        if (cJSON_IsObject(guildTagItem))
        {
            const cJSON* enabledItem = cJSON_GetObjectItemCaseSensitive(guildTagItem, "enabled");
            const cJSON* maxCharsItem = cJSON_GetObjectItemCaseSensitive(guildTagItem, "maxChars");
            const cJSON* heightRatioItem = cJSON_GetObjectItemCaseSensitive(guildTagItem, "heightRatio");
            mapCase.showGuildTag = cJSON_IsBool(enabledItem) ? cJSON_IsTrue(enabledItem) : false;
            mapCase.guildTagMaxChars = cJSON_IsNumber(maxCharsItem) ? maxCharsItem->valueint : 3;
            mapCase.guildTagHeightRatio = cJSON_IsNumber(heightRatioItem) ? static_cast<float>(heightRatioItem->valuedouble) : 0.22f;
        }
        if (!hasExplicitLegendVisibility)
        {
            WorldMapDefinition::enableLegendForZone(&importedLegendVisibility, mapCase.zoneType);
        }

        importedCases.push_back(mapCase);

        const cJSON* linksArray = cJSON_GetObjectItemCaseSensitive(caseItem, "links");
        if (!cJSON_IsArray(linksArray))
        {
            continue;
        }

        cJSON* linkItem = nullptr;
        cJSON_ArrayForEach(linkItem, linksArray)
        {
            if (!cJSON_IsObject(linkItem))
            {
                continue;
            }

            const cJSON* sideItem = cJSON_GetObjectItemCaseSensitive(linkItem, "side");
            const cJSON* targetMapNameItem = cJSON_GetObjectItemCaseSensitive(linkItem, "targetMapName");
            if (!cJSON_IsString(targetMapNameItem) || targetMapNameItem->valuestring == nullptr)
            {
                continue;
            }

            LinkSide fromSide = LinkSide::RIGHT;
            if (cJSON_IsString(sideItem) && sideItem->valuestring != nullptr)
            {
                if (std::strcmp(sideItem->valuestring, "left") == 0)
                {
                    fromSide = LinkSide::LEFT;
                }
                else if (std::strcmp(sideItem->valuestring, "top") == 0)
                {
                    fromSide = LinkSide::TOP;
                }
                else if (std::strcmp(sideItem->valuestring, "bottom") == 0)
                {
                    fromSide = LinkSide::BOTTOM;
                }
            }

            pendingLinks.push_back(
                PendingLink{
                    static_cast<int>(importedCases.size()) - 1,
                    trimAsciiWorldMap(targetMapNameItem->valuestring),
                    fromSide});
        }
    }

    cJSON_Delete(root);

    for (const PendingLink& pendingLink : pendingLinks)
    {
        if (pendingLink.fromCaseIndex < 0 ||
            pendingLink.fromCaseIndex >= static_cast<int>(importedCases.size()))
        {
            continue;
        }

        const auto targetIt = std::find_if(
            importedCases.begin(),
            importedCases.end(),
            [&pendingLink](const MapCase& mapCase) {
                return trimAsciiWorldMap(mapCase.mapName) == pendingLink.targetMapName;
            });
        if (targetIt == importedCases.end())
        {
            continue;
        }

        importedLinks.push_back(
            MapLink{
                pendingLink.fromCaseIndex,
                static_cast<int>(std::distance(importedCases.begin(), targetIt)),
                pendingLink.fromSide});
    }

    this->mapCases = importedCases;
    this->mapLinks = importedLinks;
    this->legendVisibility = importedLegendVisibility;
    this->loadedStoragePath = (storagePath != nullptr) ? storagePath : "";
    this->updateWidgetRect();
    this->updateDerivedRects();
    return true;
}

void WorldMapWidget::setGuildTagForMapName(const std::string& mapName, const std::string& guildTag)
{
    const std::string normalizedMapName = trimAsciiWorldMap(mapName);
    if (normalizedMapName.empty())
    {
        return;
    }

    const std::string normalizedGuildTag = trimAsciiWorldMap(guildTag);
    if (normalizedGuildTag.empty())
    {
        this->guildTagsByMapName.erase(normalizedMapName);
        return;
    }

    this->guildTagsByMapName[normalizedMapName] = normalizedGuildTag;
}

void WorldMapWidget::clearGuildTagForMapName(const std::string& mapName)
{
    const std::string normalizedMapName = trimAsciiWorldMap(mapName);
    if (normalizedMapName.empty())
    {
        return;
    }

    this->guildTagsByMapName.erase(normalizedMapName);
}

void WorldMapWidget::clearAllGuildTags(void)
{
    this->guildTagsByMapName.clear();
}

SDL_FRect WorldMapWidget::getBaseRectFromGameScreen(void) const
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    const float availableViewportWidth = (std::max)(screenRect.w - (kWorldMapWidgetMarginX * 2.0f) - (kWorldMapWidgetBodyInset * 2.0f), kWorldMapWidgetMinViewportWidth);
    const float viewportWidth = (std::min)(guiWidth, availableViewportWidth);
    const float legendHeight = computeLegendHeightWorldMap(&this->smallFont, this->legendVisibility, viewportWidth);
    const float availableViewportHeight =
        (std::max)(
            screenRect.h - (kWorldMapWidgetMarginY * 2.0f) - kWorldMapWidgetHeaderHeight - legendHeight - kWorldMapWidgetLegendGap - kWorldMapWidgetViewportGap - (kWorldMapWidgetBodyInset * 2.0f),
            kWorldMapWidgetMinViewportHeight);
    const float viewportHeight = (std::min)(guiHeight, availableViewportHeight);
    const float panelWidth = viewportWidth + (kWorldMapWidgetBodyInset * 2.0f);
    const float panelHeight =
        kWorldMapWidgetHeaderHeight +
        kWorldMapWidgetLegendGap +
        legendHeight +
        kWorldMapWidgetViewportGap +
        viewportHeight +
        kWorldMapWidgetBodyInset +
        kWorldMapWidgetBodyInset;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - panelWidth) * 0.5f),
        screenRect.y + ((screenRect.h - panelHeight) * 0.5f),
        panelWidth,
        panelHeight
    };
}

void WorldMapWidget::updateWidgetRect(void)
{
    const SDL_FRect baseRect = this->getBaseRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
}

void WorldMapWidget::updateDerivedRects(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    const SDL_FRect baseRect = this->getBaseRectFromGameScreen();
    const float legendHeight =
        computeLegendHeightWorldMap(&this->smallFont, this->legendVisibility, this->widgetRect.w - (kWorldMapWidgetBodyInset * 2.0f));
    this->widgetRect.x = std::clamp(
        this->widgetRect.x,
        screenRect.x + 8.0f,
        screenRect.x + screenRect.w - baseRect.w - 8.0f);
    this->widgetRect.y = std::clamp(
        this->widgetRect.y,
        screenRect.y + 8.0f,
        screenRect.y + screenRect.h - baseRect.h - 8.0f);
    this->widgetOffsetX = this->widgetRect.x - baseRect.x;
    this->widgetOffsetY = this->widgetRect.y - baseRect.y;

    this->headerRect = SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        kWorldMapWidgetHeaderHeight
    };
    this->closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 28.0f,
        this->headerRect.y + ((this->headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    this->legendRect = SDL_FRect{
        this->widgetRect.x + kWorldMapWidgetBodyInset,
        this->headerRect.y + this->headerRect.h + kWorldMapWidgetLegendGap,
        this->widgetRect.w - (kWorldMapWidgetBodyInset * 2.0f),
        legendHeight
    };
    this->viewportRect = SDL_FRect{
        this->widgetRect.x + kWorldMapWidgetBodyInset,
        this->legendRect.y + this->legendRect.h + kWorldMapWidgetViewportGap,
        this->widgetRect.w - (kWorldMapWidgetBodyInset * 2.0f),
        (this->widgetRect.y + this->widgetRect.h) - (this->legendRect.y + this->legendRect.h + kWorldMapWidgetViewportGap) - kWorldMapWidgetBodyInset
    };

    const float scaleX = (guiWidth > 0.0f) ? (this->viewportRect.w / guiWidth) : 1.0f;
    const float scaleY = (guiHeight > 0.0f) ? (this->viewportRect.h / guiHeight) : 1.0f;
    this->renderScale = (std::min)(scaleX, scaleY);
    this->renderOffset = SDL_FPoint{
        (this->viewportRect.w - (guiWidth * this->renderScale)) * 0.5f,
        (this->viewportRect.h - (guiHeight * this->renderScale)) * 0.5f
    };
}

std::string WorldMapWidget::getGuildTagLabelForCase(const MapCase& mapCase) const
{
    if (!mapCase.showGuildTag)
    {
        return std::string();
    }

    auto formatGuildTag = [this](const std::string& rawValue, int innerMaxChars) {
        std::string value = trimAsciiWorldMap(rawValue);
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
        {
            value = value.substr(1, value.size() - 2);
        }

        const std::string truncatedValue = this->truncateAscii(value, innerMaxChars);
        return std::string("[") + truncatedValue + "]";
    };

    const auto it = this->guildTagsByMapName.find(trimAsciiWorldMap(mapCase.mapName));
    if (it != this->guildTagsByMapName.end())
    {
        return formatGuildTag(it->second, mapCase.guildTagMaxChars);
    }

    return formatGuildTag("TAG", mapCase.guildTagMaxChars);
}

SDL_FPoint WorldMapWidget::getLinkAnchor(const SDL_FRect& caseRect, LinkSide side) const
{
    switch (side)
    {
        case LinkSide::LEFT:
            return SDL_FPoint{caseRect.x, caseRect.y + (caseRect.h * 0.5f)};
        case LinkSide::TOP:
            return SDL_FPoint{caseRect.x + (caseRect.w * 0.5f), caseRect.y};
        case LinkSide::RIGHT:
            return SDL_FPoint{caseRect.x + caseRect.w, caseRect.y + (caseRect.h * 0.5f)};
        case LinkSide::BOTTOM:
            return SDL_FPoint{caseRect.x + (caseRect.w * 0.5f), caseRect.y + caseRect.h};
        default:
            return SDL_FPoint{caseRect.x + caseRect.w, caseRect.y + (caseRect.h * 0.5f)};
    }
}

WorldMapWidget::LinkSide WorldMapWidget::getOppositeLinkSide(LinkSide side) const
{
    switch (side)
    {
        case LinkSide::LEFT:
            return LinkSide::RIGHT;
        case LinkSide::TOP:
            return LinkSide::BOTTOM;
        case LinkSide::RIGHT:
            return LinkSide::LEFT;
        case LinkSide::BOTTOM:
            return LinkSide::TOP;
        default:
            return LinkSide::LEFT;
    }
}

std::string WorldMapWidget::truncateAscii(const std::string& value, int maxChars) const
{
    if (maxChars <= 0)
    {
        return std::string();
    }
    if (static_cast<int>(value.size()) <= maxChars)
    {
        return value;
    }
    return value.substr(0, static_cast<size_t>(maxChars));
}

bool WorldMapWidget::readTitleTextFile(const char* storagePath, std::string* outText) const
{
    if (storagePath == nullptr || storagePath[0] == '\0' || outText == nullptr)
    {
        return false;
    }

    void* fileBytes = nullptr;
    Uint64 fileLen = 0;
    const bool readOk = rc2d_storage_titleReadFile(storagePath, &fileBytes, &fileLen);
    if (!readOk || fileBytes == nullptr || fileLen == 0)
    {
        RC2D_safe_free(fileBytes);
        return false;
    }

    outText->assign(static_cast<const char*>(fileBytes), static_cast<size_t>(fileLen));
    RC2D_safe_free(fileBytes);
    return true;
}

void WorldMapWidget::clearLoadedWorldMapData(void)
{
    this->mapCases.clear();
    this->mapLinks.clear();
    this->legendVisibility = WorldMapDefinition::LegendVisibility{};
}
