#include "game/ui/hud/hp-bar-widget.h"

#include "core/context.h"

#include <algorithm>
#include <cmath>
#include <string>

static constexpr float kHudStatusBarWidth = 194.0f;
static constexpr float kHudStatusBarMinWidth = 120.0f;
static constexpr float kHudStatusBarHeight = 24.0f;
static constexpr float kHudStatusBarStackLeftMarginPx = 121.0f;
static constexpr float kHudStatusBarBottomMarginPx = 36.0f;
static constexpr float kHudStatusBarRightMarginPx = 10.0f;
static constexpr float kHudStatusBarInnerPaddingPx = 2.0f;
static constexpr float kHudStatusBarCornerRadiusPx = 5.0f;
static constexpr float kHudWidgetScaleMin = 0.75f;
static constexpr float kHudWidgetScaleMax = 1.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kPanelBorder = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kTrackFill = RC2D_Color{24, 24, 28, 236};
static constexpr RC2D_Color kHpFill = RC2D_Color{34, 130, 64, 244};
static constexpr RC2D_Color kHpFillEdge = RC2D_Color{86, 188, 112, 248};
static constexpr RC2D_Color kLabelColor = RC2D_Color{225, 230, 236, 255};
static constexpr RC2D_Color kLabelShadow = RC2D_Color{8, 10, 12, 220};

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
        scaledHeight
    };
}

static std::string formatWithDots(int value)
{
    if (value <= 0)
    {
        return "0";
    }

    std::string digits = std::to_string(value);
    std::string output;
    output.reserve(digits.size() + (digits.size() / 3));

    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i)
    {
        output.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
        if (count == 3 && i > 0)
        {
            output.push_back('.');
            count = 0;
        }
    }

    std::reverse(output.begin(), output.end());
    return output;
}

static void drawCenteredLabel(RC2D_Font* font, const std::string& text, const SDL_FRect& rect)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text label = rc2d_graphics_createText(font, text.c_str());
    int textWidth = 0;
    int textHeight = 0;
    rc2d_graphics_getTextSize(&label, &textWidth, &textHeight);

    const float drawX = std::round(rect.x + ((rect.w - static_cast<float>(textWidth)) * 0.5f));
    const float drawY = std::round(rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f));

    label.color = kLabelShadow;
    rc2d_graphics_setTextColor(&label);
    rc2d_graphics_drawText(&label, drawX + 1.0f, drawY + 1.0f);

    label.color = kLabelColor;
    rc2d_graphics_setTextColor(&label);
    rc2d_graphics_drawText(&label, drawX, drawY);
    rc2d_graphics_destroyText(&label);
}

static float getCornerRadius(const SDL_FRect& rect)
{
    return std::clamp(
        kHudStatusBarCornerRadiusPx,
        0.0f,
        (std::min)(rect.w * 0.5f, rect.h * 0.5f));
}

static void drawBeveledFilledRect(const SDL_FRect& rect, RC2D_Color color)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    const float radius = getCornerRadius(rect);
    if (radius <= 0.0f)
    {
        rc2d_graphics_setColor(color);
        rc2d_graphics_rectangle("fill", &rect);
        return;
    }

    const SDL_FRect rects[3] = {
        SDL_FRect{rect.x + radius, rect.y, rect.w - (radius * 2.0f), rect.h},
        SDL_FRect{rect.x, rect.y + radius, radius, rect.h - (radius * 2.0f)},
        SDL_FRect{rect.x + rect.w - radius, rect.y + radius, radius, rect.h - (radius * 2.0f)}
    };

    rc2d_graphics_setColor(color);
    rc2d_graphics_rectangles("fill", 3, rects);
}

static void drawBeveledBorder(const SDL_FRect& rect, RC2D_Color color)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    const float radius = getCornerRadius(rect);
    if (radius <= 0.0f)
    {
        rc2d_graphics_setColor(color);
        rc2d_graphics_rectangle("line", &rect);
        return;
    }

    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.w;
    const float bottom = rect.y + rect.h;

    rc2d_graphics_setColor(color);
    rc2d_graphics_line(left + radius, top, right - radius, top);
    rc2d_graphics_line(right - radius, top, right, top + radius);
    rc2d_graphics_line(right, top + radius, right, bottom - radius);
    rc2d_graphics_line(right, bottom - radius, right - radius, bottom);
    rc2d_graphics_line(right - radius, bottom, left + radius, bottom);
    rc2d_graphics_line(left + radius, bottom, left, bottom - radius);
    rc2d_graphics_line(left, bottom - radius, left, top + radius);
    rc2d_graphics_line(left, top + radius, left + radius, top);
}

HpBarWidget::HpBarWidget(void)
    : labelFont{},
      currentHp(0),
      maxHp(0),
      uiScale(1.0f),
      positionOffset{0.0f, 0.0f}
{
}

HpBarWidget::~HpBarWidget(void)
{
}

void HpBarWidget::load(void)
{
    this->labelFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        13.0f);
    this->currentHp = 0;
    this->maxHp = 0;
}

void HpBarWidget::unload(void)
{
    ResetStorageFontRef(&this->labelFont);
}

void HpBarWidget::setCurrentHp(int value)
{
    this->currentHp = std::clamp(value, 0, this->maxHp);
}

void HpBarWidget::setMaxHp(int value)
{
    this->maxHp = (std::max)(0, value);
    if (this->currentHp > this->maxHp)
    {
        this->currentHp = this->maxHp;
    }
}

void HpBarWidget::setUiScale(float scale)
{
    this->uiScale = clampHudWidgetScale(scale);
}

void HpBarWidget::setPositionOffset(float offsetX, float offsetY)
{
    this->positionOffset = SDL_FPoint{offsetX, offsetY};
}

SDL_FPoint HpBarWidget::getPositionOffset(void) const
{
    return this->positionOffset;
}

void HpBarWidget::resetPositionOffset(void)
{
    this->positionOffset = SDL_FPoint{0.0f, 0.0f};
}

SDL_FRect HpBarWidget::getCurrentRect(void) const
{
    return this->getBarRect();
}

float HpBarWidget::getFillRatio(void) const
{
    if (this->maxHp <= 0)
    {
        return 0.0f;
    }

    return std::clamp(
        static_cast<float>(this->currentHp) / static_cast<float>(this->maxHp),
        0.0f,
        1.0f);
}

std::string HpBarWidget::buildLabel(void) const
{
    return
        "HP : " +
        formatWithDots(this->currentHp) +
        " / " +
        formatWithDots(this->maxHp);
}

SDL_FRect HpBarWidget::getBarRect(void) const
{
    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    const float baseX = safeRect.x + kHudStatusBarStackLeftMarginPx;
    const float availableWidth = (safeRect.x + safeRect.w) - baseX - kHudStatusBarRightMarginPx;
    if (availableWidth < kHudStatusBarMinWidth)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const SDL_FRect baseRect = SDL_FRect{
        baseX + this->positionOffset.x,
        safeRect.y + safeRect.h - kHudStatusBarBottomMarginPx - kHudStatusBarHeight + this->positionOffset.y,
        (std::min)(kHudStatusBarWidth, availableWidth),
        kHudStatusBarHeight
    };
    return scaleRectFromCenter(baseRect, this->uiScale);
}

void HpBarWidget::draw(void) const
{
    if (this->labelFont.sdl_font == nullptr)
    {
        return;
    }

    const SDL_FRect barRect = this->getBarRect();
    if (barRect.w <= 0.0f || barRect.h <= 0.0f)
    {
        return;
    }

    const float innerPadding = (std::max)(1.0f, kHudStatusBarInnerPaddingPx * this->uiScale);
    const SDL_FRect innerRect = SDL_FRect{
        barRect.x + innerPadding,
        barRect.y + innerPadding,
        barRect.w - (innerPadding * 2.0f),
        barRect.h - (innerPadding * 2.0f)
    };

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    drawBeveledFilledRect(barRect, kPanelFill);
    drawBeveledBorder(barRect, kPanelBorder);

    drawBeveledFilledRect(innerRect, kTrackFill);

    const float fillRatio = this->getFillRatio();
    if (fillRatio > 0.0f)
    {
        SDL_FRect fillRect = innerRect;
        fillRect.w = std::round(innerRect.w * fillRatio);
        drawBeveledFilledRect(fillRect, kHpFill);

        SDL_FRect highlightRect = fillRect;
        highlightRect.h = (std::max)(1.0f, std::floor(fillRect.h * 0.35f));
        drawBeveledFilledRect(highlightRect, kHpFillEdge);
    }

    drawCenteredLabel(
        const_cast<RC2D_Font*>(&this->labelFont),
        this->buildLabel(),
        barRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
