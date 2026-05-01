#include "game/ui/hud/leaderboard-widget.h"

#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <sstream>

#if GAME_ENV_DEV
static constexpr const char* kLeaderboardSiteUrl = "http://localhost:1470/rankings";
#elif GAME_ENV_STAGING
static constexpr const char* kLeaderboardSiteUrl = "https://staging.seatyrants.com/rankings";
#elif GAME_ENV_PRODUCTION
static constexpr const char* kLeaderboardSiteUrl = "https://seatyrants.com/rankings";
#else
#error "Configurer GAME_ENV via CMake (dev, staging ou production)."
#endif

static const char kIntroLeaderboard[] =
    "Les classements en tout genre (rang PvE, rang PvP, etc.) sont disponibles a cette adresse :";

static constexpr float kRefW = 380.0f;
static constexpr float kRefH = 200.0f;
static constexpr float kBodyPadding = 12.0f;
static constexpr float kLineExtraGap = 2.0f;
static constexpr float kBlockGapAfterIntro = 10.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 255};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kBodyText = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kLinkColor = RC2D_Color{130, 190, 255, 255};
static constexpr RC2D_Color kLinkUnderline = RC2D_Color{200, 225, 255, 255};

static SDL_FRect getLeaderboardRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH};
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
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

static void wrapSingleParagraph(RC2D_Font* font, const std::string& paragraph, float maxWidth, std::vector<std::string>& outLines)
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
        std::string candidate = line.empty() ? word : (line + " " + word);
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
        for (char ch : word)
        {
            std::string nextChunk = chunk + ch;
            if (measureTextWidth(font, nextChunk) <= maxWidth)
            {
                chunk = nextChunk;
                continue;
            }

            if (!chunk.empty())
            {
                outLines.push_back(chunk);
                chunk.clear();
            }
            chunk.push_back(ch);
        }

        if (!chunk.empty())
        {
            line = chunk;
        }
    }

    if (!line.empty())
    {
        outLines.push_back(line);
    }
}

static void drawLeftTop(RC2D_Font* font, const char* text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    rc2d_graphics_drawText(&t, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&t);
}

static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& r, float x, RC2D_Color color)
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
    rc2d_graphics_drawText(&t, std::round(x), std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f)));
    rc2d_graphics_destroyText(&t);
}

LeaderboardWidget::LeaderboardWidget(void)
    : introLines{},
      urlLines{},
      urlHitRect{0.0f, 0.0f, 0.0f, 0.0f},
      linkHovered(false),
      titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(false),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      cursorEnabled(true),
      controlIcons{}
{
}

LeaderboardWidget::~LeaderboardWidget(void)
{
}

void LeaderboardWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->controlIcons.load();

    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    this->visible = false;
    this->widgetDragging = false;
    this->linkHovered = false;
    this->introLines.clear();
    this->urlLines.clear();
    this->urlHitRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
}

void LeaderboardWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void LeaderboardWidget::rebuildLayout(void)
{
    this->introLines.clear();
    this->urlLines.clear();
    this->urlHitRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect bodyRect = SDL_FRect{
        outer.x + 10.0f,
        outer.y + 40.0f,
        outer.w - 20.0f,
        outer.h - 50.0f
    };

    const float maxTextW = (std::max)(40.0f, bodyRect.w - (kBodyPadding * 2.0f));
    wrapSingleParagraph(&this->bodyFont, std::string(kIntroLeaderboard), maxTextW, this->introLines);

    if (kLeaderboardSiteUrl != nullptr && kLeaderboardSiteUrl[0] != '\0')
    {
        wrapSingleParagraph(&this->bodyFont, std::string(kLeaderboardSiteUrl), maxTextW, this->urlLines);

        const float lineH = measureLineHeight(&this->bodyFont) + kLineExtraGap;
        float urlY = bodyRect.y + kBodyPadding + (static_cast<float>(this->introLines.size()) * lineH) + kBlockGapAfterIntro;
        float ux0 = outer.x + outer.w;
        float uy0 = outer.y + outer.h;
        float ux1 = outer.x;
        float uy1 = outer.y;

        for (const std::string& line : this->urlLines)
        {
            RC2D_Text measure = rc2d_graphics_createText(&this->bodyFont, line.c_str());
            int tw = 0;
            int th = 0;
            rc2d_graphics_getTextSize(&measure, &tw, &th);
            rc2d_graphics_destroyText(&measure);

            const float lineW = static_cast<float>(tw);
            const float lx = bodyRect.x + (bodyRect.w - lineW) * 0.5f;
            const float ly = urlY;
            ux0 = (std::min)(ux0, lx);
            uy0 = (std::min)(uy0, ly);
            ux1 = (std::max)(ux1, lx + lineW);
            uy1 = (std::max)(uy1, ly + static_cast<float>(th));
            urlY += lineH;
        }

        if (!this->urlLines.empty() && ux1 > ux0 && uy1 > uy0)
        {
            this->urlHitRect = SDL_FRect{ux0, uy0, ux1 - ux0, uy1 - uy0};
        }
    }
}

void LeaderboardWidget::refreshLinkHover(float mouseX, float mouseY)
{
    this->linkHovered = false;
    if (kLeaderboardSiteUrl == nullptr || kLeaderboardSiteUrl[0] == '\0' || this->urlLines.empty() ||
        this->urlHitRect.w <= 0.0f || this->urlHitRect.h <= 0.0f)
    {
        return;
    }

    if (isPointInRect(mouseX, mouseY, this->urlHitRect))
    {
        this->linkHovered = true;
    }
}

void LeaderboardWidget::update(double dt)
{
    (void)dt;

    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!this->visible)
    {
        this->linkHovered = false;
        return;
    }

    if (this->widgetDragging && !rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
    }

    if (this->widgetDragging)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
        this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
        this->widgetRect.x = baseRect.x + this->widgetOffsetX;
        this->widgetRect.y = baseRect.y + this->widgetOffsetY;
    }

    this->rebuildLayout();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    this->refreshLinkHover(mouseX, mouseY);
}

void LeaderboardWidget::draw(void) const
{
    LeaderboardWidget* self = const_cast<LeaderboardWidget*>(this);
    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
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

    self->rebuildLayout();

    float mouseDrawX = 0.0f;
    float mouseDrawY = 0.0f;
    getMouseRenderPosition(&mouseDrawX, &mouseDrawY);
    self->refreshLinkHover(mouseDrawX, mouseDrawY);

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
    drawLeftCenteredY(&self->titleFont, "Classements", header, header.x + 10.0f, kTextGold);
    self->controlIcons.drawCloseButton(closeButtonRect, kHeaderFill, kGold);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &bodyRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &bodyRect);

    const float lineH = measureLineHeight(&self->bodyFont) + kLineExtraGap;
    float textY = bodyRect.y + kBodyPadding;

    for (const std::string& line : self->introLines)
    {
        drawLeftTop(&self->bodyFont, line.c_str(), bodyRect.x + kBodyPadding, textY, kBodyText);
        textY += lineH;
    }

    if (!self->urlLines.empty())
    {
        textY += (kBlockGapAfterIntro - kLineExtraGap);
        for (const std::string& line : self->urlLines)
        {
            const float lineW = measureTextWidth(&self->bodyFont, line);
            const float drawX = bodyRect.x + (bodyRect.w - lineW) * 0.5f;
            const RC2D_Color color = self->linkHovered ? kLinkUnderline : kLinkColor;
            drawLeftTop(&self->bodyFont, line.c_str(), drawX, textY, color);

            if (self->linkHovered)
            {
                const float underlineY = textY + measureLineHeight(&self->bodyFont) + 1.0f;
                rc2d_graphics_setColor(kLinkUnderline);
                rc2d_graphics_line(
                    std::round(drawX),
                    std::round(underlineY),
                    std::round(drawX + lineW),
                    std::round(underlineY));
            }

            textY += lineH;
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool LeaderboardWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
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

    this->rebuildLayout();

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    if (isPointInRect(x, y, closeButtonRect))
    {
        this->hide();
        return true;
    }

    if (
        kLeaderboardSiteUrl != nullptr && kLeaderboardSiteUrl[0] != '\0' && !this->urlLines.empty() && this->urlHitRect.w > 0.0f &&
        isPointInRect(x, y, this->urlHitRect))
    {
        (void)SDL_OpenURL(kLeaderboardSiteUrl);
        return true;
    }

    const SDL_FRect headerDragRect = SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        30.0f
    };
    if (isPointInRect(x, y, headerDragRect))
    {
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }

    return true;
}

bool LeaderboardWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType LeaderboardWidget::getDesiredCursor(float x, float y) const
{
    if (!this->cursorEnabled || !this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }

    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    LeaderboardWidget* self = const_cast<LeaderboardWidget*>(this);
    const SDL_FRect baseRect = getLeaderboardRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    self->rebuildLayout();

    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    if (isPointInRect(x, y, closeButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (
        kLeaderboardSiteUrl != nullptr && kLeaderboardSiteUrl[0] != '\0' && !self->urlLines.empty() && self->urlHitRect.w > 0.0f &&
        isPointInRect(x, y, self->urlHitRect))
    {
        return HudCursorType::POINTER;
    }
    if (isPointInRect(x, y, header))
    {
        return HudCursorType::MOVE;
    }

    return HudCursorType::DEFAULT;
}

void LeaderboardWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
}

void LeaderboardWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->linkHovered = false;
}
