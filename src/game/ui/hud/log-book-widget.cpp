#include "game/ui/hud/log-book-widget.h"

#include "core/context.h"

#include <algorithm>
#include <cmath>
#include <sstream>

static constexpr float kRefW = 600.0f;
static constexpr float kRefH = 540.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 242};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextBody = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kScrollTrackColor = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumbColor = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kSeparatorColor = RC2D_Color{134, 102, 39, 220};
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr int kMaxMessagesPerPage = 20;

struct JournalVisualRow {
    std::string dateTime;
    std::string message;
    bool separator;
};

static SDL_FRect getJournalRectFromGameScreen(void)
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

static void applyCursorIfChanged(SDL_SystemCursor id)
{
    static SDL_SystemCursor lastId = static_cast<SDL_SystemCursor>(-1);
    static SDL_Cursor* cached[4] = {nullptr, nullptr, nullptr, nullptr};
    const int index =
        (id == SDL_SYSTEM_CURSOR_DEFAULT) ? 0 :
        (id == SDL_SYSTEM_CURSOR_POINTER) ? 1 :
        (id == SDL_SYSTEM_CURSOR_MOVE) ? 2 : 3;

    if (id == lastId)
    {
        return;
    }
    if (cached[index] == nullptr)
    {
        cached[index] = SDL_CreateSystemCursor(id);
    }
    if (cached[index] != nullptr)
    {
        SDL_SetCursor(cached[index]);
        lastId = id;
    }
}

static void setCursorArrow(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_DEFAULT);
}

static void setCursorHand(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_POINTER);
}

static void setCursorMove(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_MOVE);
}

static void setCursorResizeVertical(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_NS_RESIZE);
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

static float measureLineHeight(RC2D_Font* font)
{
    if (font == nullptr || font->sdl_font == nullptr)
    {
        return 14.0f;
    }

    RC2D_Text probe = rc2d_graphics_createText(font, "Ag");
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&probe, &w, &h);
    rc2d_graphics_destroyText(&probe);
    (void)w;
    return static_cast<float>((std::max)(h, 14));
}

static void drawTextAt(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    rc2d_graphics_drawText(&t, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&t);
}

static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& r, RC2D_Color color)
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
    rc2d_graphics_drawText(
        &t,
        std::round(r.x + ((r.w - static_cast<float>(w)) * 0.5f)),
        std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f)));
    rc2d_graphics_destroyText(&t);
}

static void wrapParagraph(RC2D_Font* font, const std::string& paragraph, float maxWidth, std::vector<std::string>& outLines)
{
    if (paragraph.empty())
    {
        outLines.emplace_back("");
        return;
    }

    std::istringstream iss(paragraph);
    std::string word;
    std::string line;
    while (iss >> word)
    {
        const std::string candidate = line.empty() ? word : (line + " " + word);
        if (measureTextWidth(font, candidate) <= maxWidth)
        {
            line = candidate;
            continue;
        }

        if (!line.empty())
        {
            outLines.push_back(line);
            line.clear();
        }

        if (measureTextWidth(font, word) <= maxWidth)
        {
            line = word;
            continue;
        }

        std::string chunk;
        for (const char c : word)
        {
            const std::string next = chunk + c;
            if (!chunk.empty() && measureTextWidth(font, next) > maxWidth)
            {
                outLines.push_back(chunk);
                chunk.clear();
            }
            chunk.push_back(c);
        }
        line = chunk;
    }

    if (!line.empty())
    {
        outLines.push_back(line);
    }
}

static std::vector<std::string> buildWrappedMessageLines(RC2D_Font* font, const std::string& message, float maxWidth)
{
    std::vector<std::string> wrapped;
    if (maxWidth <= 6.0f)
    {
        return wrapped;
    }

    std::size_t start = 0;
    while (true)
    {
        const std::size_t breakPos = message.find('\n', start);
        const std::string paragraph = (breakPos == std::string::npos) ? message.substr(start) : message.substr(start, breakPos - start);
        wrapParagraph(font, paragraph, maxWidth, wrapped);
        if (breakPos == std::string::npos)
        {
            break;
        }
        start = breakPos + 1;
    }
    return wrapped;
}

template <typename EntryContainerT>
static std::vector<JournalVisualRow> buildVisualRows(
    RC2D_Font* font,
    const EntryContainerT& logRows,
    float messageWrapWidth)
{
    std::vector<JournalVisualRow> rows;
    for (std::size_t i = 0; i < logRows.size(); ++i)
    {
        const std::vector<std::string> wrapped = buildWrappedMessageLines(font, logRows[i].message, messageWrapWidth);
        if (wrapped.empty())
        {
            rows.push_back(JournalVisualRow{logRows[i].dateTime, std::string{}, false});
        }
        else
        {
            for (std::size_t lineIndex = 0; lineIndex < wrapped.size(); ++lineIndex)
            {
                rows.push_back(JournalVisualRow{
                    (lineIndex == 0) ? logRows[i].dateTime : std::string{},
                    wrapped[lineIndex],
                    false
                });
            }
        }

        if (i + 1 < logRows.size())
        {
            rows.push_back(JournalVisualRow{std::string{}, std::string{}, true});
        }
    }

    if (rows.empty())
    {
        rows.push_back(JournalVisualRow{std::string{}, std::string{}, false});
    }
    return rows;
}

LogBookWidget::LogBookWidget(void)
    : titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      rows{},
      scrollFirstRow(0),
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      cursorEnabled(true)
{
}

LogBookWidget::~LogBookWidget(void)
{
}

void LogBookWidget::load(void)
{
    this->titleFont = rc2d_graphics_openFontFromStorage("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 24.0f);
    this->bodyFont = rc2d_graphics_openFontFromStorage("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 14.0f);

    const SDL_FRect baseRect = getJournalRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    this->visible = true;
    this->rows.clear();
    this->scrollFirstRow = 0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;

}

void LogBookWidget::unload(void)
{
    rc2d_graphics_closeFont(&this->bodyFont);
    rc2d_graphics_closeFont(&this->titleFont);
}

void LogBookWidget::publishLogBookRow(const LogBookWidget::LogBookRow& row)
{
    if (row.message.empty())
    {
        return;
    }

    this->rows.push_back(LogBookWidget::LogBookRow{
        row.dateTime.empty() ? std::string("??.?? ??:??") : row.dateTime,
        row.message
    });

    static constexpr std::size_t kMaxRows = 300;
    if (this->rows.size() > kMaxRows)
    {
        const std::size_t overflow = this->rows.size() - kMaxRows;
        this->rows.erase(this->rows.begin(), this->rows.begin() + overflow);
    }
}

void LogBookWidget::update(double dt)
{
    (void)dt;

    const SDL_FRect baseRect = getJournalRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 32.0f};
    const SDL_FRect navBar = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, 28.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, navBar.y + navBar.h + 6.0f, outer.w - 16.0f, outer.h - (navBar.y + navBar.h + 14.0f - outer.y)};
    const SDL_FRect prevButtonRect = SDL_FRect{navBar.x + 8.0f, navBar.y + 4.0f, 24.0f, navBar.h - 8.0f};
    const SDL_FRect nextButtonRect = SDL_FRect{navBar.x + navBar.w - 32.0f, navBar.y + 4.0f, 24.0f, navBar.h - 8.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    const float dateColW = 96.0f;
    const float messageX = body.x + 8.0f + dateColW + 10.0f;
    const float showScrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
    const float wrapW = body.w - (messageX - body.x) - 10.0f - showScrollReserve;
    const std::vector<JournalVisualRow> rows = buildVisualRows(&this->bodyFont, this->rows, wrapW);
    const float lineHeight = measureLineHeight(&this->bodyFont) + 2.0f;
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor((body.h - 12.0f) / lineHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxMessagesPerPage));
    const int maxFirstRow = (std::max)(0, static_cast<int>(rows.size()) - visibleRows);
    this->scrollFirstRow = (std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow));

    if (this->visible && this->cursorEnabled)
    {
        float mx = 0.0f;
        float my = 0.0f;
        getMouseRenderPosition(&mx, &my);
        if (isPointInRect(mx, my, this->widgetRect))
        {
            const SDL_FRect scrollTrack = SDL_FRect{
                body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
                body.y + kScrollBarPadding,
                kScrollBarWidth,
                body.h - (kScrollBarPadding * 2.0f)
            };
            if (isPointInRect(mx, my, closeButtonRect) ||
                isPointInRect(mx, my, prevButtonRect) ||
                isPointInRect(mx, my, nextButtonRect))
            {
                setCursorHand();
            }
            else if (maxFirstRow > 0 && isPointInRect(mx, my, scrollTrack))
            {
                setCursorResizeVertical();
            }
            else if (isPointInRect(mx, my, header))
            {
                setCursorMove();
            }
            else
            {
                setCursorArrow();
            }
        }
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

    if (this->scrollBarDragging)
    {
        if (maxFirstRow <= 0)
        {
            this->scrollFirstRow = 0;
            this->scrollBarDragging = false;
            return;
        }

        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(kMinThumbHeight, (scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, static_cast<int>(rows.size())))));
        const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;

        const float thumbTop = clampf(mouseY - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
        const float t = (thumbTop - scrollTrack.y) / thumbTravel;
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

bool LogBookWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getJournalRectFromGameScreen();
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

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 32.0f};
    const SDL_FRect navBar = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, 28.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, navBar.y + navBar.h + 6.0f, outer.w - 16.0f, outer.h - (navBar.y + navBar.h + 14.0f - outer.y)};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    if (isPointInRect(x, y, closeButtonRect))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->scrollBarDragging = false;
        return true;
    }

    if (isPointInRect(x, y, header))
    {
        this->widgetDragging = true;
        this->scrollBarDragging = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }

    if (isPointInRect(x, y, body))
    {
        const float dateColW = 96.0f;
        const float messageX = body.x + 8.0f + dateColW + 10.0f;
        const float showScrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
        const float wrapW = body.w - (messageX - body.x) - 10.0f - showScrollReserve;
        const std::vector<JournalVisualRow> rows = buildVisualRows(&this->bodyFont, this->rows, wrapW);
        const float lineHeight = measureLineHeight(&this->bodyFont) + 2.0f;
        const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor((body.h - 12.0f) / lineHeight)));
        const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxMessagesPerPage));
        const int totalRows = static_cast<int>(rows.size());
        const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
        if (maxFirstRow <= 0)
        {
            return true;
        }

        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        if (!isPointInRect(x, y, scrollTrack))
        {
            return true;
        }

        const float thumbHeight = (std::max)(kMinThumbHeight, (scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);
        const float currentT = static_cast<float>((std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow))) / static_cast<float>(maxFirstRow);
        const float thumbY = scrollTrack.y + (thumbTravel * currentT);
        const SDL_FRect scrollThumb = SDL_FRect{scrollTrack.x, thumbY, scrollTrack.w, thumbHeight};

        this->scrollBarDragging = true;
        this->widgetDragging = false;
        if (isPointInRect(x, y, scrollThumb))
        {
            this->scrollDragOffsetY = y - scrollThumb.y;
        }
        else
        {
            this->scrollDragOffsetY = thumbHeight * 0.5f;
            const float thumbTop = clampf(y - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
            const float t = (thumbTop - scrollTrack.y) / thumbTravel;
            this->scrollFirstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
            this->scrollFirstRow = (std::max)(0, (std::min)(this->scrollFirstRow, maxFirstRow));
        }
        return true;
    }

    return true;
}

bool LogBookWidget::mousewheelmoved(
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

    const SDL_FRect baseRect = getJournalRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!isPointInRect(mouse_x, mouse_y, this->widgetRect))
    {
        return false;
    }

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 32.0f};
    const SDL_FRect navBar = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, 28.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, navBar.y + navBar.h + 6.0f, outer.w - 16.0f, outer.h - (navBar.y + navBar.h + 14.0f - outer.y)};
    const float dateColW = 96.0f;
    const float messageX = body.x + 8.0f + dateColW + 10.0f;
    const float showScrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
    const float wrapW = body.w - (messageX - body.x) - 10.0f - showScrollReserve;
    const std::vector<JournalVisualRow> rows = buildVisualRows(&this->bodyFont, this->rows, wrapW);
    const float lineHeight = measureLineHeight(&this->bodyFont) + 2.0f;
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor((body.h - 12.0f) / lineHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxMessagesPerPage));
    const int maxFirstRow = (std::max)(0, static_cast<int>(rows.size()) - visibleRows);
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

bool LogBookWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getJournalRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

void LogBookWidget::draw(void) const
{
    LogBookWidget* self = const_cast<LogBookWidget*>(this);

    const SDL_FRect baseRect = getJournalRectFromGameScreen();
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
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 32.0f};
    const SDL_FRect navBar = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, 28.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, navBar.y + navBar.h + 6.0f, outer.w - 16.0f, outer.h - (navBar.y + navBar.h + 14.0f - outer.y)};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    const float dateColW = 96.0f;
    const float dateColX = body.x + 8.0f;
    const float separatorX = dateColX + dateColW + 4.0f;
    const float messageX = separatorX + 8.0f;
    const float showScrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
    const float messageWrapWidth = body.w - (messageX - body.x) - 10.0f - showScrollReserve;

    std::vector<JournalVisualRow> rows = buildVisualRows(&self->bodyFont, self->rows, messageWrapWidth);
    const float lineHeight = measureLineHeight(&self->bodyFont) + 2.0f;
    const float textTop = body.y + 6.0f;
    const float textHeight = body.h - 12.0f;
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(textHeight / lineHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxMessagesPerPage));
    const int totalRows = static_cast<int>(rows.size());
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    self->scrollFirstRow = (std::max)(0, (std::min)(self->scrollFirstRow, maxFirstRow));

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
    drawCentered(&self->titleFont, "Journal de bord", header, kTextGold);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &navBar);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &navBar);

    const SDL_FRect prevButton = SDL_FRect{navBar.x + 8.0f, navBar.y + 4.0f, 24.0f, navBar.h - 8.0f};
    const SDL_FRect nextButton = SDL_FRect{navBar.x + navBar.w - 32.0f, navBar.y + 4.0f, 24.0f, navBar.h - 8.0f};
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &prevButton);
    rc2d_graphics_rectangle("fill", &nextButton);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &prevButton);
    rc2d_graphics_rectangle("line", &nextButton);
    rc2d_graphics_line(prevButton.x + 14.0f, prevButton.y + 5.0f, prevButton.x + 9.0f, prevButton.y + (prevButton.h * 0.5f));
    rc2d_graphics_line(prevButton.x + 9.0f, prevButton.y + (prevButton.h * 0.5f), prevButton.x + 14.0f, prevButton.y + prevButton.h - 5.0f);
    rc2d_graphics_line(nextButton.x + 10.0f, nextButton.y + 5.0f, nextButton.x + 15.0f, nextButton.y + (nextButton.h * 0.5f));
    rc2d_graphics_line(nextButton.x + 15.0f, nextButton.y + (nextButton.h * 0.5f), nextButton.x + 10.0f, nextButton.y + nextButton.h - 5.0f);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &body);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &body);

    rc2d_graphics_setColor(kSeparatorColor);
    rc2d_graphics_line(separatorX, body.y + 1.0f, separatorX, body.y + body.h - 1.0f);

    const int firstRow = self->scrollFirstRow;
    const int lastRow = (std::min)(totalRows, firstRow + visibleRows);
    float drawY = textTop;
    for (int rowIndex = firstRow; rowIndex < lastRow; ++rowIndex)
    {
        const JournalVisualRow& row = rows[static_cast<std::size_t>(rowIndex)];
        if (row.separator)
        {
            rc2d_graphics_setColor(kSeparatorColor);
            const float y = drawY + (lineHeight * 0.5f);
            rc2d_graphics_line(body.x + 4.0f, y, body.x + body.w - 12.0f, y);
        }
        else
        {
            if (!row.dateTime.empty())
            {
                drawTextAt(&self->bodyFont, row.dateTime, dateColX, drawY, kTextGold);
            }
            if (!row.message.empty())
            {
                drawTextAt(&self->bodyFont, row.message, messageX, drawY, kTextBody);
            }
        }
        drawY += lineHeight;
    }

    if (maxFirstRow > 0)
    {
        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(kMinThumbHeight, (scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);
        const float scrollRatio = static_cast<float>(firstRow) / static_cast<float>(maxFirstRow);
        const SDL_FRect scrollThumb = SDL_FRect{
            scrollTrack.x,
            scrollTrack.y + (thumbTravel * scrollRatio),
            scrollTrack.w,
            thumbHeight
        };

        rc2d_graphics_setColor(kScrollTrackColor);
        rc2d_graphics_rectangle("fill", &scrollTrack);
        rc2d_graphics_setColor(kScrollThumbColor);
        rc2d_graphics_rectangle("fill", &scrollThumb);
    }

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &closeButtonRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &closeButtonRect);
    rc2d_graphics_line(
        closeButtonRect.x + 5.0f,
        closeButtonRect.y + 5.0f,
        closeButtonRect.x + closeButtonRect.w - 5.0f,
        closeButtonRect.y + closeButtonRect.h - 5.0f);
    rc2d_graphics_line(
        closeButtonRect.x + closeButtonRect.w - 5.0f,
        closeButtonRect.y + 5.0f,
        closeButtonRect.x + 5.0f,
        closeButtonRect.y + closeButtonRect.h - 5.0f);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

