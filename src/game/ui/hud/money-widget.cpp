#include "game/ui/hud/money-widget.h"

#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cmath>

static constexpr float kRefW = 372.0f;
static constexpr float kRefH = 576.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{124, 109, 84, 255};
static constexpr RC2D_Color kScrollTrackColor = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumbColor = RC2D_Color{124, 132, 142, 240};

static constexpr float kBodyPadding = 10.0f;
static constexpr float kRowHeight = 42.0f;
static constexpr float kRowGap = 8.0f;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;

static SDL_FRect getMoneyRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH
    };
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
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

static float measureTextWidth(RC2D_Font* font, const std::string& text)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_destroyText(&t);
    (void)h;
    return static_cast<float>(w);
}

static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    const float drawX = std::round(rect.x + ((rect.w - static_cast<float>(w)) * 0.5f));
    const float drawY = std::round(rect.y + ((rect.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& rect, float x, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    (void)w;
    const float drawX = std::round(x);
    const float drawY = std::round(rect.y + ((rect.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static void drawImageFit(const RC2D_Image& image, const SDL_FRect& target, float padding)
{
    if (image.sdl_texture == nullptr)
    {
        return;
    }

    float texW = 0.0f;
    float texH = 0.0f;
    if (!SDL_GetTextureSize(image.sdl_texture, &texW, &texH) || texW <= 0.0f || texH <= 0.0f)
    {
        return;
    }

    const float clampedPadding = (std::max)(0.0f, padding);
    const float maxWidth = (std::max)(1.0f, target.w - (clampedPadding * 2.0f));
    const float maxHeight = (std::max)(1.0f, target.h - (clampedPadding * 2.0f));
    const float scale = (std::min)(maxWidth / texW, maxHeight / texH);
    const float drawW = texW * scale;
    const float drawH = texH * scale;
    const float drawX = std::round(target.x + ((target.w - drawW) * 0.5f));
    const float drawY = std::round(target.y + ((target.h - drawH) * 0.5f));

    RC2D_Image imageCopy = image;
    const RC2D_Quad quad = rc2d_graphics_newQuad(&imageCopy, 0.0f, 0.0f, texW, texH);
    if (quad.src.w <= 0.0f || quad.src.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawQuad(
        &imageCopy,
        &quad,
        drawX,
        drawY,
        0.0,
        scale,
        scale,
        -1.0f,
        -1.0f,
        false,
        false);
}

static int getVisibleMoneyRowCount(float bodyHeight)
{
    const float usableHeight = (std::max)(1.0f, bodyHeight - (kBodyPadding * 2.0f));
    return (std::max)(1, static_cast<int>(std::floor((usableHeight + kRowGap) / (kRowHeight + kRowGap))));
}

MoneyWidget::MoneyWidget(void)
    : currencyEntries{},
      titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      widgetDragging(false),
      scrollFirstRow(0),
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      cursorEnabled(true),
      controlIcons{}
{
}

MoneyWidget::~MoneyWidget(void)
{
}

void MoneyWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->controlIcons.load();

    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    this->visible = false;
    this->widgetDragging = false;
    this->scrollFirstRow = 0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->clearCurrencyEntries();
}

void MoneyWidget::unload(void)
{
    this->clearCurrencyEntries();
    this->controlIcons.unload();
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void MoneyWidget::update(double dt)
{
    (void)dt;

    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!this->visible)
    {
        return;
    }

    if (!this->widgetDragging && !this->scrollBarDragging)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        this->scrollBarDragging = false;
        return;
    }

    const SDL_FRect bodyRect = SDL_FRect{
        this->widgetRect.x + 10.0f,
        this->widgetRect.y + 40.0f,
        this->widgetRect.w - 20.0f,
        this->widgetRect.h - 50.0f
    };
    const int totalRows = static_cast<int>(this->currencyEntries.size());
    const int visibleRows = getVisibleMoneyRowCount(bodyRect.h);
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);

    if (this->scrollBarDragging)
    {
        if (maxFirstRow <= 0)
        {
            this->scrollFirstRow = 0;
            this->scrollBarDragging = false;
            return;
        }

        const SDL_FRect scrollTrackRect = SDL_FRect{
            bodyRect.x + bodyRect.w - (kScrollBarWidth + kScrollBarPadding),
            bodyRect.y + kScrollBarPadding,
            kScrollBarWidth,
            bodyRect.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (scrollTrackRect.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, scrollTrackRect.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;

        const float thumbTop = clampf(
            mouseY - this->scrollDragOffsetY,
            scrollTrackRect.y,
            scrollTrackRect.y + thumbTravel);
        const float t = (thumbTop - scrollTrackRect.y) / thumbTravel;
        this->scrollFirstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
        this->scrollFirstRow = (std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow));
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    this->widgetRect.x = baseRect.x + this->widgetOffsetX;
    this->widgetRect.y = baseRect.y + this->widgetOffsetY;
}

bool MoneyWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    const SDL_FRect headerRect = SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        30.0f
    };
    const SDL_FRect closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 28.0f,
        headerRect.y + ((headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect bodyRect = SDL_FRect{
        this->widgetRect.x + 10.0f,
        this->widgetRect.y + 40.0f,
        this->widgetRect.w - 20.0f,
        this->widgetRect.h - 50.0f
    };

    if (isPointInRect(x, y, closeButtonRect))
    {
        this->hide();
        return true;
    }

    const int totalRows = static_cast<int>(this->currencyEntries.size());
    const int visibleRows = getVisibleMoneyRowCount(bodyRect.h);
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    if (maxFirstRow > 0)
    {
        const SDL_FRect scrollTrackRect = SDL_FRect{
            bodyRect.x + bodyRect.w - (kScrollBarWidth + kScrollBarPadding),
            bodyRect.y + kScrollBarPadding,
            kScrollBarWidth,
            bodyRect.h - (kScrollBarPadding * 2.0f)
        };
        if (isPointInRect(x, y, scrollTrackRect))
        {
            const float thumbHeight = (std::max)(
                kMinThumbHeight,
                (scrollTrackRect.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
            const float thumbTravel = (std::max)(1.0f, scrollTrackRect.h - thumbHeight);
            const float currentT = static_cast<float>((std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow))) /
                                   static_cast<float>(maxFirstRow);
            const float thumbY = scrollTrackRect.y + (thumbTravel * currentT);
            const SDL_FRect scrollThumbRect = SDL_FRect{
                scrollTrackRect.x,
                thumbY,
                scrollTrackRect.w,
                thumbHeight
            };

            this->scrollBarDragging = true;
            this->widgetDragging = false;
            if (isPointInRect(x, y, scrollThumbRect))
            {
                this->scrollDragOffsetY = y - scrollThumbRect.y;
            }
            else
            {
                this->scrollDragOffsetY = thumbHeight * 0.5f;
                const float thumbTop = clampf(
                    y - this->scrollDragOffsetY,
                    scrollTrackRect.y,
                    scrollTrackRect.y + thumbTravel);
                const float t = (thumbTop - scrollTrackRect.y) / thumbTravel;
                this->scrollFirstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
                this->scrollFirstRow = (std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow));
            }
            return true;
        }
    }

    if (isPointInRect(x, y, headerRect))
    {
        this->widgetDragging = true;
        this->scrollBarDragging = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }

    if (isPointInRect(x, y, bodyRect))
    {
        this->widgetDragging = false;
        return true;
    }

    return true;
}

bool MoneyWidget::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float wheel_x,
    float wheel_y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)wheel_x;
    (void)integer_x;
    (void)mouseID;

    if (!this->visible)
    {
        return false;
    }

    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const SDL_FRect bodyRect = SDL_FRect{
        this->widgetRect.x + 10.0f,
        this->widgetRect.y + 40.0f,
        this->widgetRect.w - 20.0f,
        this->widgetRect.h - 50.0f
    };
    if (!isPointInRect(mouse_x, mouse_y, bodyRect))
    {
        return false;
    }

    const int totalRows = static_cast<int>(this->currencyEntries.size());
    const int visibleRows = getVisibleMoneyRowCount(bodyRect.h);
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    if (maxFirstRow <= 0)
    {
        this->scrollFirstRow = 0;
        return true;
    }

    int delta = static_cast<int>(integer_y);
    if (delta == 0)
    {
        if (wheel_y > 0.0f) { delta = 1; }
        else if (wheel_y < 0.0f) { delta = -1; }
    }
    if (delta == 0)
    {
        if (direction == RC2D_SCROLL_UP) { delta = 1; }
        else if (direction == RC2D_SCROLL_DOWN) { delta = -1; }
    }

    if (delta != 0)
    {
        this->scrollFirstRow -= delta;
        this->scrollFirstRow = (std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow));
    }
    return true;
}

void MoneyWidget::draw(void) const
{
    MoneyWidget* self = const_cast<MoneyWidget*>(this);
    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!self->visible)
    {
        return;
    }

    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect bodyRect = SDL_FRect{
        outer.x + 10.0f,
        outer.y + 40.0f,
        outer.w - 20.0f,
        outer.h - 50.0f
    };

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);
    drawLeftCenteredY(&self->titleFont, "Money", header, header.x + 10.0f, kTextGold);
    self->controlIcons.drawCloseButton(closeButtonRect, kHeaderFill, kGold);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &bodyRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &bodyRect);

    if (self->currencyEntries.empty())
    {
        drawCentered(&self->bodyFont, "Aucune monnaie alimentee", bodyRect, kTextMuted);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    const int totalRows = static_cast<int>(self->currencyEntries.size());
    const int visibleRows = getVisibleMoneyRowCount(bodyRect.h);
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    self->scrollFirstRow = (std::max)(0, (std::min)(self->scrollFirstRow, maxFirstRow));
    const bool showScrollBar = maxFirstRow > 0;
    const float scrollReservedWidth = showScrollBar ? (kScrollBarWidth + (kScrollBarPadding * 2.0f)) : 0.0f;

    const int firstRow = self->scrollFirstRow;
    const int lastRow = (std::min)(totalRows, firstRow + visibleRows);
    const int renderedRowCount = (std::max)(0, lastRow - firstRow);
    const float renderedContentHeight =
        (renderedRowCount > 0)
            ? ((static_cast<float>(renderedRowCount) * kRowHeight) + (static_cast<float>(renderedRowCount - 1) * kRowGap))
            : 0.0f;
    // Quand la liste deborde, on centre le bloc visible pour equilibrer la marge haute/basse.
    // Si tout tient sans scrollbar, on garde un depart naturel en haut du contenu.
    float rowY = showScrollBar
                     ? (bodyRect.y + ((bodyRect.h - renderedContentHeight) * 0.5f))
                     : (bodyRect.y + kBodyPadding);
    for (int rowIndex = firstRow; rowIndex < lastRow; ++rowIndex)
    {
        const CurrencyEntryRuntime& entry = self->currencyEntries[static_cast<std::size_t>(rowIndex)];
        const SDL_FRect rowRect = SDL_FRect{
            bodyRect.x + kBodyPadding,
            rowY,
            bodyRect.w - (kBodyPadding * 2.0f) - scrollReservedWidth,
            kRowHeight
        };
        const SDL_FRect iconSlotRect = SDL_FRect{
            rowRect.x + 8.0f,
            rowRect.y + 5.0f,
            32.0f,
            32.0f
        };
        const std::string valueText = std::to_string((std::max)(0, entry.value));
        const float valueWidth = measureTextWidth(&self->bodyFont, valueText);
        const float valueX = std::round(rowRect.x + rowRect.w - 12.0f - valueWidth);
        const float labelX = iconSlotRect.x + iconSlotRect.w + 10.0f;

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &rowRect);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &iconSlotRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &iconSlotRect);
        drawImageFit(entry.iconImage, iconSlotRect, 2.0f);

        drawLeftCenteredY(
            &self->bodyFont,
            self->getCurrencyLabel(entry.type),
            rowRect,
            labelX,
            kTextWhite);
        drawLeftCenteredY(
            &self->bodyFont,
            valueText.c_str(),
            rowRect,
            valueX,
            kTextGold);

        rowY += kRowHeight + kRowGap;
    }

    if (showScrollBar)
    {
        const SDL_FRect scrollTrackRect = SDL_FRect{
            bodyRect.x + bodyRect.w - (kScrollBarWidth + kScrollBarPadding),
            bodyRect.y + kScrollBarPadding,
            kScrollBarWidth,
            bodyRect.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (scrollTrackRect.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, scrollTrackRect.h - thumbHeight);
        const float scrollRatio = static_cast<float>(self->scrollFirstRow) / static_cast<float>(maxFirstRow);
        const SDL_FRect scrollThumbRect = SDL_FRect{
            scrollTrackRect.x,
            scrollTrackRect.y + (thumbTravel * scrollRatio),
            scrollTrackRect.w,
            thumbHeight
        };

        rc2d_graphics_setColor(kScrollTrackColor);
        rc2d_graphics_rectangle("fill", &scrollTrackRect);
        rc2d_graphics_setColor(kScrollThumbColor);
        rc2d_graphics_rectangle("fill", &scrollThumbRect);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void MoneyWidget::setCurrencyEntry(CurrencyType type, const std::string& iconPath, int value)
{
    const std::size_t index = this->findCurrencyEntryIndex(type);
    if (index == this->currencyEntries.size())
    {
        CurrencyEntryRuntime entry{
            type,
            iconPath,
            (std::max)(0, value),
            RC2D_Image{}
        };
        this->reloadCurrencyIcon(entry);
        this->currencyEntries.push_back(entry);
    }
    else
    {
        CurrencyEntryRuntime& entry = this->currencyEntries[index];
        entry.iconPath = iconPath;
        entry.value = (std::max)(0, value);
        this->reloadCurrencyIcon(entry);
    }

    std::sort(
        this->currencyEntries.begin(),
        this->currencyEntries.end(),
        [](const CurrencyEntryRuntime& lhs, const CurrencyEntryRuntime& rhs)
        {
            return static_cast<int>(lhs.type) < static_cast<int>(rhs.type);
        });
}

void MoneyWidget::setCurrencyEntries(const std::vector<CurrencyEntry>& entries)
{
    this->clearCurrencyEntries();
    for (const CurrencyEntry& entry : entries)
    {
        this->setCurrencyEntry(entry.type, entry.iconPath, entry.value);
    }
}

void MoneyWidget::clearCurrencyEntries(void)
{
    for (CurrencyEntryRuntime& entry : this->currencyEntries)
    {
        ResetStorageImageRef(&entry.iconImage);
    }
    this->currencyEntries.clear();
    this->scrollFirstRow = 0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
}

void MoneyWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
    this->scrollBarDragging = false;
}

void MoneyWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->scrollBarDragging = false;
}

bool MoneyWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType MoneyWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->scrollBarDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const SDL_FRect baseRect = getMoneyRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const SDL_FRect headerRect = SDL_FRect{
        currentRect.x + 5.0f,
        currentRect.y + 5.0f,
        currentRect.w - 10.0f,
        30.0f
    };
    const SDL_FRect closeButtonRect = SDL_FRect{
        currentRect.x + currentRect.w - 28.0f,
        headerRect.y + ((headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect bodyRect = SDL_FRect{
        currentRect.x + 10.0f,
        currentRect.y + 40.0f,
        currentRect.w - 20.0f,
        currentRect.h - 50.0f
    };

    const int totalRows = static_cast<int>(this->currencyEntries.size());
    const int visibleRows = getVisibleMoneyRowCount(bodyRect.h);
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    const SDL_FRect scrollTrackRect = SDL_FRect{
        bodyRect.x + bodyRect.w - (kScrollBarWidth + kScrollBarPadding),
        bodyRect.y + kScrollBarPadding,
        kScrollBarWidth,
        bodyRect.h - (kScrollBarPadding * 2.0f)
    };

    if (isPointInRect(x, y, closeButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (maxFirstRow > 0 && isPointInRect(x, y, scrollTrackRect))
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (isPointInRect(x, y, headerRect))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

std::size_t MoneyWidget::findCurrencyEntryIndex(CurrencyType type) const
{
    for (std::size_t i = 0; i < this->currencyEntries.size(); ++i)
    {
        if (this->currencyEntries[i].type == type)
        {
            return i;
        }
    }
    return this->currencyEntries.size();
}

void MoneyWidget::reloadCurrencyIcon(CurrencyEntryRuntime& entry)
{
    ResetStorageImageRef(&entry.iconImage);
    entry.iconImage = RC2D_Image{};
    if (!entry.iconPath.empty())
    {
        entry.iconImage = LoadStorageImage(entry.iconPath.c_str(), RC2D_STORAGE_TITLE);
    }
}

const char* MoneyWidget::getCurrencyLabel(CurrencyType type) const
{
    switch (type)
    {
        case CurrencyType::RUBIES:
            return "Rubies";
        case CurrencyType::GOLD:
            return "Gold";
        default:
            break;
    }
    return "Monnaie";
}
