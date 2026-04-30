#include "game/ui/hud/top-bar-main-currency-widget.h"

#include "core/context.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cmath>
#include <string>

static constexpr float kCurrencyRowHeight = 25.0f;
static constexpr float kCurrencyIconSize = 16.0f;
static constexpr float kCurrencyIconTextGap = 6.0f;
static constexpr float kCurrencyGroupGap = 18.0f;
static constexpr float kCurrencyShadowOffset = 1.0f;
static constexpr RC2D_Color kCurrencyText = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kCurrencyTextShadow = RC2D_Color{20, 10, 4, 220};

static std::string formatWithDots(std::int64_t value)
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

static float measureTextWidth(RC2D_Font* font, const std::string& text)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)height;
    return static_cast<float>(width);
}

static float measureTextHeight(RC2D_Font* font, const std::string& text)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)width;
    return static_cast<float>(height);
}

static void drawTextAt(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    rc2d_graphics_drawText(&textObject, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&textObject);
}

static void drawImageFit(const RC2D_Image& image, const SDL_FRect& target)
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

    const float scale = (std::min)(target.w / texW, target.h / texH);
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

TopBarMainCurrencyWidget::TopBarMainCurrencyWidget(void)
    : rubiesIcon{},
      goldIcon{},
      amountFont{},
      localX(22.0f),
      localY(2.0f),
      rubiesAmount(0),
      goldAmount(0)
{
}

TopBarMainCurrencyWidget::~TopBarMainCurrencyWidget(void)
{
}

void TopBarMainCurrencyWidget::load(void)
{
    this->rubiesIcon = LoadStorageImage(
        "assets/images/ui-scene-game/money-rubies.png",
        RC2D_STORAGE_TITLE);
    this->goldIcon = LoadStorageImage(
        "assets/images/ui-scene-game/money-gold.png",
        RC2D_STORAGE_TITLE);
    this->amountFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        12.0f);
}

void TopBarMainCurrencyWidget::unload(void)
{
    ResetStorageFontRef(&this->amountFont);
    ResetStorageImageRef(&this->goldIcon);
    ResetStorageImageRef(&this->rubiesIcon);
}

void TopBarMainCurrencyWidget::setLocalPosition(float newLocalX, float newLocalY)
{
    this->localX = newLocalX;
    this->localY = newLocalY;
}

void TopBarMainCurrencyWidget::draw(void) const
{
    if (this->amountFont.sdl_font == nullptr)
    {
        return;
    }

    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float baseX = gameScreenRect.x + this->localX;
    const float baseY = gameScreenRect.y + this->localY;
    const float iconY = baseY + ((kCurrencyRowHeight - kCurrencyIconSize) * 0.5f);
    const float goldIconX = baseX;
    const std::string goldText = formatWithDots(this->goldAmount);
    const float goldTextX = goldIconX + kCurrencyIconSize + kCurrencyIconTextGap;
    const float textY = baseY + ((kCurrencyRowHeight - measureTextHeight(const_cast<RC2D_Font*>(&this->amountFont), goldText)) * 0.5f);
    const float goldTextWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->amountFont), goldText);
    const float rubiesIconX = goldTextX + goldTextWidth + kCurrencyGroupGap;
    const std::string rubiesText = formatWithDots(this->rubiesAmount);
    const float rubiesTextX = rubiesIconX + kCurrencyIconSize + kCurrencyIconTextGap;

    drawImageFit(this->goldIcon, SDL_FRect{goldIconX, iconY, kCurrencyIconSize, kCurrencyIconSize});
    drawTextAt(
        const_cast<RC2D_Font*>(&this->amountFont),
        goldText,
        goldTextX + kCurrencyShadowOffset,
        textY + kCurrencyShadowOffset,
        kCurrencyTextShadow);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->amountFont),
        goldText,
        goldTextX,
        textY,
        kCurrencyText);

    drawImageFit(this->rubiesIcon, SDL_FRect{rubiesIconX, iconY, kCurrencyIconSize, kCurrencyIconSize});
    drawTextAt(
        const_cast<RC2D_Font*>(&this->amountFont),
        rubiesText,
        rubiesTextX + kCurrencyShadowOffset,
        textY + kCurrencyShadowOffset,
        kCurrencyTextShadow);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->amountFont),
        rubiesText,
        rubiesTextX,
        textY,
        kCurrencyText);
}

void TopBarMainCurrencyWidget::setRubiesAmount(std::int64_t amount)
{
    this->rubiesAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void TopBarMainCurrencyWidget::setGoldAmount(std::int64_t amount)
{
    this->goldAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}
