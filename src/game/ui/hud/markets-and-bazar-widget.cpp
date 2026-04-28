#include "game/ui/hud/markets-and-bazar-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

using MarketCategory = MarketsAndBazarWidget::MarketCategory;
using MarketCurrency = MarketsAndBazarWidget::MarketCurrency;
using MarketPriceData = MarketsAndBazarWidget::MarketPriceData;
using BazarRow = MarketsAndBazarWidget::BazarRow;
using MarketRow = MarketsAndBazarWidget::MarketRow;

static constexpr float kRefW = 1040.0f;
static constexpr float kRefH = 670.0f;
static constexpr float kRowHeight = 92.0f;
static constexpr float kCategoryRowHeight = 32.0f;
static constexpr int kMaxRowsPerPage = 20;
static constexpr int kMaxInputDigits = 10;
static constexpr double kCursorBlinkPeriod = 0.55;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 242};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTabInactive = RC2D_Color{26, 20, 14, 238};
static constexpr RC2D_Color kTabActive = RC2D_Color{54, 28, 8, 242};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 236};
static constexpr RC2D_Color kButtonFill = RC2D_Color{7, 35, 52, 236};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextBody = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{126, 132, 142, 255};
static constexpr RC2D_Color kRowLine = RC2D_Color{134, 102, 39, 220};
static constexpr RC2D_Color kScrollTrack = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumb = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kScrollThumbDragFill = RC2D_Color{184, 132, 30, 245};
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;

struct MarketLayout {
    SDL_FRect outer;
    SDL_FRect inner;
    SDL_FRect topBar;
    SDL_FRect closeButton;
    SDL_FRect bazarTab;
    SDL_FRect blackTab;
    SDL_FRect basicTab;
    SDL_FRect eventTab;
    SDL_FRect categoryHeader;
    SDL_FRect categoryBody;
    SDL_FRect categoryResetButton;
    SDL_FRect tableHeader;
    SDL_FRect body;
    SDL_FRect footer;
    SDL_FRect timerBox;
    SDL_FRect scrollTrack;
    float contentWidth;
    float col1X;
    float col1W;
    float col2X;
    float col2W;
    float col3X;
    float col3W;
    float col4X;
    float col4W;
};

static SDL_FRect getWidgetRectFromGameScreen(void)
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

static char extractDigitFromKeyLabel(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    if (key[1] == '\0' && key[0] >= '0' && key[0] <= '9')
    {
        return key[0];
    }

    std::string lower(key);
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool looksLikeKeypad =
        (lower.find("kp") != std::string::npos) ||
        (lower.find("keypad") != std::string::npos) ||
        (lower.find("numpad") != std::string::npos);
    if (!looksLikeKeypad)
    {
        return '\0';
    }

    for (const char c : lower)
    {
        if (c >= '0' && c <= '9')
        {
            return c;
        }
    }

    return '\0';
}

static int parsePositiveInt(const std::string& value)
{
    if (value.empty())
    {
        return 0;
    }

    int result = 0;
    for (const char c : value)
    {
        if (c < '0' || c > '9')
        {
            continue;
        }
        const int digit = static_cast<int>(c - '0');
        if (result > (2147483647 - digit) / 10)
        {
            return 2147483647;
        }
        result = (result * 10) + digit;
    }
    return result;
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

static void drawImageFit(RC2D_Image* image, const SDL_FRect& target)
{
    if (image == nullptr || image->sdl_texture == nullptr)
    {
        return;
    }

    float texW = 0.0f;
    float texH = 0.0f;
    if (!SDL_GetTextureSize(image->sdl_texture, &texW, &texH) || texW <= 0.0f || texH <= 0.0f)
    {
        return;
    }

    const float scale = (std::min)(target.w / texW, target.h / texH);
    const float drawW = texW * scale;
    const float drawH = texH * scale;
    const float drawX = target.x + ((target.w - drawW) * 0.5f);
    const float drawY = target.y + ((target.h - drawH) * 0.5f);

    const RC2D_Quad quad = rc2d_graphics_newQuad(image, 0.0f, 0.0f, texW, texH);
    if (quad.src.w <= 0.0f || quad.src.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawQuad(image, &quad, drawX, drawY, 0.0, scale, scale, -1.0f, -1.0f, false, false);
}

static RC2D_Image loadImageFromTitleOrEmpty(const std::string& imagePath)
{
    if (imagePath.empty())
    {
        return RC2D_Image{};
    }
    return LoadStorageImage(imagePath.c_str(), RC2D_STORAGE_TITLE);
}

static std::string keepDigitsOrFallback(const std::string& rawValue, const std::string& fallback)
{
    std::string filtered;
    filtered.reserve(rawValue.size());
    for (const char c : rawValue)
    {
        if (c >= '0' && c <= '9')
        {
            filtered.push_back(c);
        }
    }

    if (filtered.empty())
    {
        return fallback;
    }

    std::size_t nonZero = filtered.find_first_not_of('0');
    if (nonZero == std::string::npos)
    {
        return "0";
    }
    return filtered.substr(nonZero);
}

static constexpr std::array<MarketCategory, 11> kCategoryOrder = {
    MarketCategory::ACTIVABLES,
    MarketCategory::BOOSTER,
    MarketCategory::CANNONS,
    MarketCategory::CONSOMMABLES,
    MarketCategory::HARPONEUSE,
    MarketCategory::MATELOTS,
    MarketCategory::MUNITION_DE_CANNON,
    MarketCategory::MUNITION_DE_HARPON,
    MarketCategory::NAVIRES,
    MarketCategory::UTILISABLE_SUR_CIBLE,
    MarketCategory::VOILES
};

static std::size_t categoryToIndex(MarketCategory category)
{
    const int raw = static_cast<int>(category);
    if (raw < 0 || raw >= static_cast<int>(MarketCategory::COUNT))
    {
        return 0U;
    }
    return static_cast<std::size_t>(raw);
}

static MarketCategory sanitizeCategory(MarketCategory category)
{
    const int raw = static_cast<int>(category);
    if (raw < 0 || raw >= static_cast<int>(MarketCategory::COUNT))
    {
        return MarketCategory::ACTIVABLES;
    }
    return category;
}

static MarketCurrency sanitizeCurrency(MarketCurrency currency)
{
    switch (currency)
    {
        case MarketCurrency::GOLD:
        case MarketCurrency::CRISTAUX:
        case MarketCurrency::RUBIES:
        case MarketCurrency::FACTIONS:
            return currency;
        default:
            break;
    }
    return MarketCurrency::GOLD;
}

static const char* marketCurrencyLabel(MarketCurrency currency)
{
    switch (sanitizeCurrency(currency))
    {
        case MarketCurrency::GOLD:
            return "gold";
        case MarketCurrency::CRISTAUX:
            return "cristaux";
        case MarketCurrency::RUBIES:
            return "rubies";
        case MarketCurrency::FACTIONS:
            return "factions";
        default:
            break;
    }
    return "gold";
}

static const char* categoryLabel(MarketCategory category)
{
    switch (category)
    {
        case MarketCategory::ACTIVABLES:
            return "Activables";
        case MarketCategory::BOOSTER:
            return "Booster";
        case MarketCategory::CANNONS:
            return "Cannons";
        case MarketCategory::CONSOMMABLES:
            return "Consommables";
        case MarketCategory::HARPONEUSE:
            return "Harponeuse";
        case MarketCategory::MATELOTS:
            return "Matelots";
        case MarketCategory::MUNITION_DE_CANNON:
            return "Munition de cannon";
        case MarketCategory::MUNITION_DE_HARPON:
            return "Munition de harpon";
        case MarketCategory::NAVIRES:
            return "Navires";
        case MarketCategory::UTILISABLE_SUR_CIBLE:
            return "Utilisable sur cible";
        case MarketCategory::VOILES:
            return "Voiles";
        default:
            break;
    }
    return "Categorie";
}

static MarketLayout buildLayout(const SDL_FRect& outer, bool bazarTab)
{
    MarketLayout layout{};
    layout.outer = outer;
    layout.inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    layout.topBar = SDL_FRect{layout.inner.x + 1.0f, layout.inner.y + 1.0f, layout.inner.w - 2.0f, 34.0f};
    layout.closeButton = SDL_FRect{
        outer.x + outer.w - 28.0f,
        layout.topBar.y + ((layout.topBar.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    const float tabY = layout.topBar.y + 3.0f;
    const float tabH = layout.topBar.h - 6.0f;
    layout.basicTab = SDL_FRect{outer.x + 10.0f, tabY, 140.0f, tabH};
    layout.eventTab = SDL_FRect{layout.basicTab.x + layout.basicTab.w + 2.0f, tabY, 170.0f, tabH};
    layout.bazarTab = SDL_FRect{layout.eventTab.x + layout.eventTab.w + 2.0f, tabY, 128.0f, tabH};
    layout.blackTab = SDL_FRect{layout.bazarTab.x + layout.bazarTab.w + 2.0f, tabY, 128.0f, tabH};

    const float sectionY = layout.topBar.y + layout.topBar.h + 8.0f;
    const float sectionH = 30.0f;
    const float sectionGapX = 8.0f;
    const float categoryW = 248.0f;
    const float footerReserve = 38.0f;

    layout.categoryHeader = SDL_FRect{outer.x + 10.0f, sectionY, categoryW, sectionH};
    layout.tableHeader = SDL_FRect{
        layout.categoryHeader.x + layout.categoryHeader.w + sectionGapX,
        sectionY,
        outer.w - 20.0f - categoryW - sectionGapX,
        sectionH
    };

    const float bodyY = sectionY + sectionH + 2.0f;
    const float bodyH = outer.y + outer.h - footerReserve - bodyY;
    layout.categoryBody = SDL_FRect{layout.categoryHeader.x, bodyY, layout.categoryHeader.w, bodyH};
    layout.body = SDL_FRect{layout.tableHeader.x, bodyY, layout.tableHeader.w, bodyH};

    layout.footer = SDL_FRect{layout.tableHeader.x, layout.body.y + layout.body.h + 6.0f, layout.tableHeader.w, 26.0f};
    layout.timerBox = SDL_FRect{layout.footer.x + layout.footer.w - 124.0f, layout.footer.y, 124.0f, layout.footer.h};
    layout.categoryResetButton = SDL_FRect{
        layout.categoryHeader.x,
        layout.categoryBody.y + layout.categoryBody.h + 6.0f,
        layout.categoryHeader.w,
        26.0f
    };

    const float scrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
    layout.contentWidth = layout.body.w - scrollReserve - 2.0f;
    if (layout.contentWidth < 100.0f)
    {
        layout.contentWidth = layout.body.w;
    }
    layout.scrollTrack = SDL_FRect{
        layout.body.x + layout.body.w - (kScrollBarWidth + kScrollBarPadding),
        layout.body.y + kScrollBarPadding,
        kScrollBarWidth,
        layout.body.h - (kScrollBarPadding * 2.0f)
    };

    layout.col1X = layout.body.x;
    if (bazarTab)
    {
        layout.col1W = std::floor(layout.contentWidth * 0.44f);
        layout.col2W = std::floor(layout.contentWidth * 0.14f);
        layout.col3W = std::floor(layout.contentWidth * 0.20f);
        layout.col4W = layout.contentWidth - layout.col1W - layout.col2W - layout.col3W;
    }
    else
    {
        layout.col1W = std::floor(layout.contentWidth * 0.45f);
        layout.col2W = std::floor(layout.contentWidth * 0.18f);
        layout.col3W = std::floor(layout.contentWidth * 0.18f);
        layout.col4W = layout.contentWidth - layout.col1W - layout.col2W - layout.col3W;
    }
    layout.col2X = layout.col1X + layout.col1W;
    layout.col3X = layout.col2X + layout.col2W;
    layout.col4X = layout.col3X + layout.col3W;

    return layout;
}

static SDL_FRect getCategoryRowRect(const MarketLayout& layout, int rowIndex)
{
    return SDL_FRect{
        layout.categoryBody.x,
        layout.categoryBody.y + (static_cast<float>(rowIndex) * kCategoryRowHeight),
        layout.categoryBody.w,
        kCategoryRowHeight
    };
}

static SDL_FRect getHeaderDragRect(const MarketLayout& layout)
{
    // Zone de drag volontairement limitee entre le dernier onglet et la croix.
    const float left = layout.blackTab.x + layout.blackTab.w + 4.0f;
    const float right = layout.closeButton.x - 4.0f;
    if (right <= left)
    {
        return layout.topBar;
    }
    return SDL_FRect{left, layout.topBar.y, right - left, layout.topBar.h};
}

MarketsAndBazarWidget::MarketsAndBazarWidget(void)
    : titleFont{},
      bodyFont{},
      smallFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      cursorEnabled(true),
      controlIcons{},
      activeTab(ActiveTab::BASIC_MARKET),
      bazarRows{},
      blackMarketRows{},
      basicMarketRows{},
      eventMarketRows{},
      bazarRowIcons{},
      blackMarketRowIcons{},
      basicMarketRowIcons{},
      eventMarketRowIcons{},
      bazarFirstRow(0),
      blackFirstRow(0),
      basicFirstRow(0),
      eventFirstRow(0),
      categoryFilterEnabled{},
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
      scrollBarWheelHighlightSec(0.0f),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      inputFocused(false),
      focusedTab(ActiveTab::BASIC_MARKET),
      focusedRow(-1),
      cursorIndex(0),
      cursorVisible(false),
      cursorBlinkElapsed(0.0),
      timerText("15:02")
{
}

MarketsAndBazarWidget::~MarketsAndBazarWidget(void)
{
}

bool MarketsAndBazarWidget::isBazarActive(void) const
{
    return this->activeTab == ActiveTab::BAZARD;
}

bool MarketsAndBazarWidget::hasAnyCategoryFilterEnabled(void) const
{
    for (const bool enabled : this->categoryFilterEnabled)
    {
        if (enabled)
        {
            return true;
        }
    }
    return false;
}

bool MarketsAndBazarWidget::isCategoryEnabled(MarketCategory category) const
{
    return this->categoryFilterEnabled[categoryToIndex(sanitizeCategory(category))];
}

bool MarketsAndBazarWidget::passesCategoryFilter(MarketCategory category) const
{
    if (!this->hasAnyCategoryFilterEnabled())
    {
        return true;
    }
    return this->isCategoryEnabled(category);
}

void MarketsAndBazarWidget::resetCategoryFilters(void)
{
    this->categoryFilterEnabled.fill(false);
}

std::vector<int> MarketsAndBazarWidget::buildFilteredRowIndices(ActiveTab tab) const
{
    std::vector<int> indices;
    if (tab == ActiveTab::BAZARD)
    {
        indices.reserve(this->bazarRows.size());
        for (std::size_t i = 0; i < this->bazarRows.size(); ++i)
        {
            const BazarRow& row = this->bazarRows[i];
            if (this->passesCategoryFilter(row.category))
            {
                indices.push_back(static_cast<int>(i));
            }
        }
        return indices;
    }

    const std::vector<MarketRow>* rows = this->getMarketRowsForTab(tab);
    if (rows == nullptr)
    {
        return indices;
    }

    indices.reserve(rows->size());
    for (std::size_t i = 0; i < rows->size(); ++i)
    {
        const MarketRow& row = (*rows)[i];
        if (this->passesCategoryFilter(row.category))
        {
            indices.push_back(static_cast<int>(i));
        }
    }
    return indices;
}

void MarketsAndBazarWidget::clearIcons(std::vector<RC2D_Image>& icons)
{
    for (RC2D_Image& icon : icons)
    {
        ResetStorageImageRef(&icon);
    }
    icons.clear();
}

std::vector<MarketsAndBazarWidget::MarketRow>* MarketsAndBazarWidget::getMarketRowsForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::BLACK_MARKET:
            return &this->blackMarketRows;
        case ActiveTab::BASIC_MARKET:
            return &this->basicMarketRows;
        case ActiveTab::EVENT_MARKET:
            return &this->eventMarketRows;
        case ActiveTab::BAZARD:
        default:
            break;
    }
    return nullptr;
}

const std::vector<MarketsAndBazarWidget::MarketRow>* MarketsAndBazarWidget::getMarketRowsForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::BLACK_MARKET:
            return &this->blackMarketRows;
        case ActiveTab::BASIC_MARKET:
            return &this->basicMarketRows;
        case ActiveTab::EVENT_MARKET:
            return &this->eventMarketRows;
        case ActiveTab::BAZARD:
        default:
            break;
    }
    return nullptr;
}

std::vector<RC2D_Image>* MarketsAndBazarWidget::getMarketIconsForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::BLACK_MARKET:
            return &this->blackMarketRowIcons;
        case ActiveTab::BASIC_MARKET:
            return &this->basicMarketRowIcons;
        case ActiveTab::EVENT_MARKET:
            return &this->eventMarketRowIcons;
        case ActiveTab::BAZARD:
        default:
            break;
    }
    return nullptr;
}

const std::vector<RC2D_Image>* MarketsAndBazarWidget::getMarketIconsForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::BLACK_MARKET:
            return &this->blackMarketRowIcons;
        case ActiveTab::BASIC_MARKET:
            return &this->basicMarketRowIcons;
        case ActiveTab::EVENT_MARKET:
            return &this->eventMarketRowIcons;
        case ActiveTab::BAZARD:
        default:
            break;
    }
    return nullptr;
}

int* MarketsAndBazarWidget::getFirstRowForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::BAZARD:
            return &this->bazarFirstRow;
        case ActiveTab::BLACK_MARKET:
            return &this->blackFirstRow;
        case ActiveTab::BASIC_MARKET:
            return &this->basicFirstRow;
        case ActiveTab::EVENT_MARKET:
            return &this->eventFirstRow;
        default:
            break;
    }
    return nullptr;
}

const int* MarketsAndBazarWidget::getFirstRowForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::BAZARD:
            return &this->bazarFirstRow;
        case ActiveTab::BLACK_MARKET:
            return &this->blackFirstRow;
        case ActiveTab::BASIC_MARKET:
            return &this->basicFirstRow;
        case ActiveTab::EVENT_MARKET:
            return &this->eventFirstRow;
        default:
            break;
    }
    return nullptr;
}

void MarketsAndBazarWidget::clearRows(void)
{
    this->bazarRows.clear();
    this->blackMarketRows.clear();
    this->basicMarketRows.clear();
    this->eventMarketRows.clear();

    this->clearIcons(this->bazarRowIcons);
    this->clearIcons(this->blackMarketRowIcons);
    this->clearIcons(this->basicMarketRowIcons);
    this->clearIcons(this->eventMarketRowIcons);
}

void MarketsAndBazarWidget::setMarketFamilyRows(ActiveTab tab, const std::vector<MarketRow>& rows)
{
    std::vector<MarketRow>* targetRows = this->getMarketRowsForTab(tab);
    std::vector<RC2D_Image>* targetIcons = this->getMarketIconsForTab(tab);
    int* firstRow = this->getFirstRowForTab(tab);
    if (targetRows == nullptr || targetIcons == nullptr || firstRow == nullptr)
    {
        return;
    }

    targetRows->clear();
    this->clearIcons(*targetIcons);

    targetRows->reserve(rows.size());
    targetIcons->reserve(rows.size());

    for (const MarketRow& row : rows)
    {
        MarketRow internalRow{};
        internalRow.imagePath = row.imagePath;
        internalRow.itemName = row.itemName;
        internalRow.itemDescription = row.itemDescription;
        internalRow.category = sanitizeCategory(row.category);
        internalRow.quantity = (std::max)(0, row.quantity);
        internalRow.yourOffer = keepDigitsOrFallback(row.yourOffer, "1");

        for (const MarketPriceData& priceLine : row.prices)
        {
            if (priceLine.amount <= 0)
            {
                continue;
            }

            MarketPriceData cleanPrice{};
            cleanPrice.amount = priceLine.amount;
            cleanPrice.currency = sanitizeCurrency(priceLine.currency);
            internalRow.prices.push_back(std::move(cleanPrice));
        }

        targetRows->push_back(std::move(internalRow));
        targetIcons->push_back(loadImageFromTitleOrEmpty(row.imagePath));
    }

    *firstRow = 0;
    if (this->focusedTab == tab)
    {
        this->clearInputFocusInternal();
    }
}

void MarketsAndBazarWidget::setBazarRows(const std::vector<BazarRow>& rows)
{
    this->bazarRows.clear();
    this->clearIcons(this->bazarRowIcons);

    this->bazarRows.reserve(rows.size());
    this->bazarRowIcons.reserve(rows.size());

    for (const BazarRow& row : rows)
    {
        BazarRow internalRow{};
        internalRow.imagePath = row.imagePath;
        internalRow.itemName = row.itemName;
        internalRow.itemDescription = row.itemDescription;
        internalRow.category = sanitizeCategory(row.category);
        internalRow.quantity = (std::max)(0, row.quantity);
        internalRow.highestBidder = row.highestBidder;
        internalRow.yourOffer = keepDigitsOrFallback(row.yourOffer, "0");

        this->bazarRows.push_back(std::move(internalRow));
        this->bazarRowIcons.push_back(loadImageFromTitleOrEmpty(row.imagePath));
    }

    this->bazarFirstRow = 0;
    if (this->focusedTab == ActiveTab::BAZARD)
    {
        this->clearInputFocusInternal();
    }
}

void MarketsAndBazarWidget::setBlackMarketRows(const std::vector<MarketRow>& rows)
{
    this->setMarketFamilyRows(ActiveTab::BLACK_MARKET, rows);
}

void MarketsAndBazarWidget::setBasicMarketRows(const std::vector<MarketRow>& rows)
{
    this->setMarketFamilyRows(ActiveTab::BASIC_MARKET, rows);
}

void MarketsAndBazarWidget::setEventMarketRows(const std::vector<MarketRow>& rows)
{
    this->setMarketFamilyRows(ActiveTab::EVENT_MARKET, rows);
}

void MarketsAndBazarWidget::clearInputFocusInternal(void)
{
    this->inputFocused = false;
    this->focusedRow = -1;
    this->cursorIndex = 0;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
}

void MarketsAndBazarWidget::submitFocusedInput(void)
{
    if (!this->inputFocused || this->focusedRow < 0)
    {
        return;
    }

    if (this->focusedTab == ActiveTab::BAZARD)
    {
        const int row = this->focusedRow;
        if (row < 0 || row >= static_cast<int>(this->bazarRows.size()))
        {
            return;
        }

        const int value = parsePositiveInt(this->bazarRows[static_cast<std::size_t>(row)].yourOffer);
        if (value <= 0)
        {
            return;
        }

        this->bazarRows[static_cast<std::size_t>(row)].highestBidder = "Vous";
    }
    else
    {
        std::vector<MarketRow>* marketRowsForTab = this->getMarketRowsForTab(this->focusedTab);
        if (marketRowsForTab == nullptr)
        {
            return;
        }

        const int row = this->focusedRow;
        if (row < 0 || row >= static_cast<int>(marketRowsForTab->size()))
        {
            return;
        }

        MarketRow& marketRow = (*marketRowsForTab)[static_cast<std::size_t>(row)];
        int amount = parsePositiveInt(marketRow.yourOffer);
        if (amount <= 0)
        {
            return;
        }

        amount = (std::min)(amount, marketRow.quantity);
        marketRow.quantity -= amount;
        marketRow.quantity = (std::max)(0, marketRow.quantity);
        marketRow.yourOffer = "1";
        this->cursorIndex = marketRow.yourOffer.size();
    }
}

void MarketsAndBazarWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->smallFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 13.0f);
    this->controlIcons.load();

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    this->visible = false;
    this->activeTab = ActiveTab::BASIC_MARKET;
    this->bazarFirstRow = 0;
    this->blackFirstRow = 0;
    this->basicFirstRow = 0;
    this->eventFirstRow = 0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
    this->scrollBarWheelHighlightSec = 0.0f;
    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->timerText = "15:02";

    this->clearInputFocusInternal();
    this->resetCategoryFilters();
    this->clearRows();
}

void MarketsAndBazarWidget::unload(void)
{
    this->controlIcons.unload();
    this->clearRows();
    ResetStorageFontRef(&this->smallFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void MarketsAndBazarWidget::update(double dt)
{
    const float dtF = static_cast<float>(dt);
    if (this->scrollBarWheelHighlightSec > 0.0f)
    {
        this->scrollBarWheelHighlightSec -= dtF;
        if (this->scrollBarWheelHighlightSec < 0.0f)
        {
            this->scrollBarWheelHighlightSec = 0.0f;
        }
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const ActiveTab currentTab = this->activeTab;
    const bool bazarTab = currentTab == ActiveTab::BAZARD;
    const MarketLayout layout = buildLayout(this->widgetRect, bazarTab);
    int firstRowFallback = 0;
    int* firstRowPtr = this->getFirstRowForTab(currentTab);
    int& firstRow = firstRowPtr != nullptr ? *firstRowPtr : firstRowFallback;
    const std::vector<int> filteredRowIndices = this->buildFilteredRowIndices(currentTab);
    const int totalRows = static_cast<int>(filteredRowIndices.size());
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));

    if (this->inputFocused && this->focusedTab == currentTab)
    {
        if (std::find(filteredRowIndices.begin(), filteredRowIndices.end(), this->focusedRow) == filteredRowIndices.end())
        {
            this->clearInputFocusInternal();
        }
    }

    if (this->inputFocused)
    {
        this->cursorBlinkElapsed += dt;
        while (this->cursorBlinkElapsed >= kCursorBlinkPeriod)
        {
            this->cursorBlinkElapsed -= kCursorBlinkPeriod;
            this->cursorVisible = !this->cursorVisible;
        }
    }
    else
    {
        this->cursorBlinkElapsed = 0.0;
        this->cursorVisible = false;
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
            firstRow = 0;
            this->scrollBarDragging = false;
            return;
        }

        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (layout.scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, layout.scrollTrack.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;

        const float thumbTop = clampf(mouseY - this->scrollDragOffsetY, layout.scrollTrack.y, layout.scrollTrack.y + thumbTravel);
        const float t = (thumbTop - layout.scrollTrack.y) / thumbTravel;
        firstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
        firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));
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

bool MarketsAndBazarWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
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

    const ActiveTab currentTab = this->activeTab;
    const bool bazarTab = currentTab == ActiveTab::BAZARD;
    MarketLayout layout = buildLayout(this->widgetRect, bazarTab);
    int firstRowFallback = 0;
    int* firstRowPtr = this->getFirstRowForTab(currentTab);
    int& firstRow = firstRowPtr != nullptr ? *firstRowPtr : firstRowFallback;
    std::vector<int> filteredRowIndices = this->buildFilteredRowIndices(currentTab);
    int totalRows = static_cast<int>(filteredRowIndices.size());
    int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));
    const SDL_FRect headerDragRect = getHeaderDragRect(layout);

    if (isPointInRect(x, y, layout.closeButton))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, layout.bazarTab))
    {
        this->activeTab = ActiveTab::BAZARD;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, layout.blackTab))
    {
        this->activeTab = ActiveTab::BLACK_MARKET;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, layout.basicTab))
    {
        this->activeTab = ActiveTab::BASIC_MARKET;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, layout.eventTab))
    {
        this->activeTab = ActiveTab::EVENT_MARKET;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, headerDragRect))
    {
        this->widgetDragging = true;
        this->scrollBarDragging = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearInputFocusInternal();
        return true;
    }

    if (isPointInRect(x, y, layout.categoryBody))
    {
        for (int i = 0; i < static_cast<int>(kCategoryOrder.size()); ++i)
        {
            const SDL_FRect rowRect = getCategoryRowRect(layout, i);
            if (rowRect.y + rowRect.h > layout.categoryBody.y + layout.categoryBody.h)
            {
                break;
            }
            if (!isPointInRect(x, y, rowRect))
            {
                continue;
            }

            const MarketCategory category = kCategoryOrder[static_cast<std::size_t>(i)];
            const std::size_t categoryIndex = categoryToIndex(category);
            this->categoryFilterEnabled[categoryIndex] = !this->categoryFilterEnabled[categoryIndex];
            this->bazarFirstRow = 0;
            this->blackFirstRow = 0;
            this->basicFirstRow = 0;
            this->eventFirstRow = 0;
            this->scrollBarDragging = false;
            this->clearInputFocusInternal();
            return true;
        }
    }

    if (isPointInRect(x, y, layout.categoryResetButton))
    {
        this->resetCategoryFilters();
        this->bazarFirstRow = 0;
        this->blackFirstRow = 0;
        this->basicFirstRow = 0;
        this->eventFirstRow = 0;
        this->scrollBarDragging = false;
        this->clearInputFocusInternal();
        return true;
    }

    layout = buildLayout(this->widgetRect, this->isBazarActive());
    filteredRowIndices = this->buildFilteredRowIndices(this->activeTab);
    totalRows = static_cast<int>(filteredRowIndices.size());
    visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    maxFirstRow = (std::max)(0, totalRows - visibleRows);
    firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));

    if (isPointInRect(x, y, layout.body))
    {
        if (maxFirstRow > 0 && isPointInRect(x, y, layout.scrollTrack))
        {
            const float thumbHeight = (std::max)(
                kMinThumbHeight,
                (layout.scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
            const float thumbTravel = (std::max)(1.0f, layout.scrollTrack.h - thumbHeight);
            const float currentT = static_cast<float>((std::max)(0, (std::min)(firstRow, maxFirstRow))) / static_cast<float>(maxFirstRow);
            const float thumbY = layout.scrollTrack.y + (thumbTravel * currentT);
            const SDL_FRect thumb = SDL_FRect{layout.scrollTrack.x, thumbY, layout.scrollTrack.w, thumbHeight};

            this->scrollBarDragging = true;
            this->widgetDragging = false;
            if (isPointInRect(x, y, thumb))
            {
                this->scrollDragOffsetY = y - thumb.y;
            }
            else
            {
                this->scrollDragOffsetY = thumbHeight * 0.5f;
                const float thumbTop = clampf(y - this->scrollDragOffsetY, layout.scrollTrack.y, layout.scrollTrack.y + thumbTravel);
                const float t = (thumbTop - layout.scrollTrack.y) / thumbTravel;
                firstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
                firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));
            }
            return true;
        }

        const int visualRow = static_cast<int>(std::floor((y - layout.body.y) / kRowHeight));
        const int displayIndex = firstRow + visualRow;
        if (visualRow < 0 || visualRow >= visibleRows || displayIndex < 0 || displayIndex >= totalRows)
        {
            this->clearInputFocusInternal();
            return true;
        }
        const int row = filteredRowIndices[static_cast<std::size_t>(displayIndex)];

        const float rowY = layout.body.y + (static_cast<float>(visualRow) * kRowHeight);
        SDL_FRect inputRect{};
        SDL_FRect submitRect{};
        if (this->isBazarActive())
        {
            inputRect = SDL_FRect{layout.col4X + 8.0f, rowY + 12.0f, layout.col4W - 16.0f, 29.0f};
            submitRect = SDL_FRect{layout.col4X + 8.0f, rowY + 46.0f, layout.col4W - 16.0f, 30.0f};
        }
        else
        {
            inputRect = SDL_FRect{layout.col4X + 8.0f, rowY + 12.0f, layout.col4W - 16.0f, 29.0f};
            submitRect = SDL_FRect{layout.col4X + 8.0f, rowY + 46.0f, layout.col4W - 16.0f, 30.0f};
        }

        if (isPointInRect(x, y, inputRect))
        {
            this->inputFocused = true;
            this->focusedTab = this->activeTab;
            this->focusedRow = row;
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;

            std::string* inputValue = nullptr;
            if (this->isBazarActive())
            {
                inputValue = &this->bazarRows[static_cast<std::size_t>(row)].yourOffer;
            }
            else
            {
                std::vector<MarketRow>* activeMarketRows = this->getMarketRowsForTab(this->activeTab);
                if (activeMarketRows != nullptr)
                {
                    inputValue = &(*activeMarketRows)[static_cast<std::size_t>(row)].yourOffer;
                }
            }

            if (inputValue != nullptr)
            {
                const float inputTextX = inputRect.x + 8.0f;
                const float localX = (std::max)(0.0f, x - inputTextX);
                std::size_t newCursor = 0;
                for (std::size_t i = 0; i <= inputValue->size(); ++i)
                {
                    const float w = measureTextWidth(&this->bodyFont, inputValue->substr(0, i));
                    if (localX <= w)
                    {
                        newCursor = i;
                        break;
                    }
                    newCursor = i;
                }
                this->cursorIndex = (std::min)(newCursor, inputValue->size());
            }
            return true;
        }

        if (isPointInRect(x, y, submitRect))
        {
            this->inputFocused = true;
            this->focusedTab = this->activeTab;
            this->focusedRow = row;
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            this->submitFocusedInput();
            return true;
        }

        this->clearInputFocusInternal();
        return true;
    }

    this->clearInputFocusInternal();
    return true;
}

bool MarketsAndBazarWidget::mousewheelmoved(
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

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
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

    const ActiveTab currentTab = this->activeTab;
    const bool bazarTab = currentTab == ActiveTab::BAZARD;
    const MarketLayout layout = buildLayout(this->widgetRect, bazarTab);
    int firstRowFallback = 0;
    int* firstRowPtr = this->getFirstRowForTab(currentTab);
    int& firstRow = firstRowPtr != nullptr ? *firstRowPtr : firstRowFallback;
    const std::vector<int> filteredRowIndices = this->buildFilteredRowIndices(currentTab);
    const int totalRows = static_cast<int>(filteredRowIndices.size());
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);

    if (maxFirstRow <= 0)
    {
        firstRow = 0;
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
        const int rowBefore = firstRow;
        firstRow -= delta;
        firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));
        if (firstRow != rowBefore)
        {
            this->scrollBarWheelHighlightSec = kScrollThumbWheelHighlightSec;
        }
    }
    return true;
}

bool MarketsAndBazarWidget::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    (void)mod;
    (void)isrepeat;

    if (!this->visible || !this->inputFocused || this->focusedRow < 0)
    {
        return false;
    }

    std::string* input = nullptr;
    if (this->focusedTab == ActiveTab::BAZARD)
    {
        if (this->focusedRow >= static_cast<int>(this->bazarRows.size()))
        {
            return false;
        }
        input = &this->bazarRows[static_cast<std::size_t>(this->focusedRow)].yourOffer;
    }
    else
    {
        std::vector<MarketRow>* focusedMarketRows = this->getMarketRowsForTab(this->focusedTab);
        if (focusedMarketRows == nullptr || this->focusedRow >= static_cast<int>(focusedMarketRows->size()))
        {
            return false;
        }
        input = &(*focusedMarketRows)[static_cast<std::size_t>(this->focusedRow)].yourOffer;
    }

    if (input == nullptr)
    {
        return false;
    }

    char digit = '\0';
    digit = extractDigitFromKeyLabel(key);
    if (digit == '\0' && scancode >= SDL_SCANCODE_KP_0 && scancode <= SDL_SCANCODE_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(scancode - SDL_SCANCODE_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_KP_0 && keycode <= SDLK_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_0 && keycode <= SDLK_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_0));
    }

    if (digit != '\0')
    {
        if (input->size() == 1 && (*input)[0] == '0')
        {
            input->clear();
            this->cursorIndex = 0;
        }
        if (input->size() >= static_cast<std::size_t>(kMaxInputDigits))
        {
            return true;
        }
        this->cursorIndex = (std::min)(this->cursorIndex, input->size());
        input->insert(this->cursorIndex, 1, digit);
        ++this->cursorIndex;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    switch (scancode)
    {
        case SDL_SCANCODE_LEFT:
            if (this->cursorIndex > 0) { --this->cursorIndex; }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RIGHT:
            if (this->cursorIndex < input->size()) { ++this->cursorIndex; }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_HOME:
            this->cursorIndex = 0;
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_END:
            this->cursorIndex = input->size();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_BACKSPACE:
            if (this->cursorIndex > 0 && !input->empty())
            {
                input->erase(this->cursorIndex - 1, 1);
                --this->cursorIndex;
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_DELETE:
            if (this->cursorIndex < input->size())
            {
                input->erase(this->cursorIndex, 1);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            this->submitFocusedInput();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_ESCAPE:
            this->clearInputFocusInternal();
            return true;
        default:
            break;
    }

    return false;
}

bool MarketsAndBazarWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType MarketsAndBazarWidget::getDesiredCursor(float x, float y) const
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

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const ActiveTab currentTab = this->activeTab;
    const bool bazarTab = currentTab == ActiveTab::BAZARD;
    const MarketLayout layout = buildLayout(currentRect, bazarTab);
    const SDL_FRect headerDragRect = getHeaderDragRect(layout);
    const std::vector<int> filteredRowIndices = this->buildFilteredRowIndices(currentTab);
    const int totalRows = static_cast<int>(filteredRowIndices.size());
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    const int* firstRowPtr = this->getFirstRowForTab(currentTab);
    const int firstRow = firstRowPtr != nullptr ? *firstRowPtr : 0;
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);

    bool onInput = false;
    bool onSubmit = false;
    bool onCategoryFilter = false;
    bool onResetFilters = false;

    if (isPointInRect(x, y, layout.body))
    {
        const int visualRow = static_cast<int>(std::floor((y - layout.body.y) / kRowHeight));
        const int displayIndex = firstRow + visualRow;
        if (visualRow >= 0 && visualRow < visibleRows && displayIndex >= 0 && displayIndex < totalRows)
        {
            const float rowY = layout.body.y + (static_cast<float>(visualRow) * kRowHeight);
            const SDL_FRect inputRect = SDL_FRect{layout.col4X + 8.0f, rowY + 12.0f, layout.col4W - 16.0f, 29.0f};
            const SDL_FRect submitRect = SDL_FRect{layout.col4X + 8.0f, rowY + 46.0f, layout.col4W - 16.0f, 30.0f};
            onInput = isPointInRect(x, y, inputRect);
            onSubmit = isPointInRect(x, y, submitRect);
        }
    }

    if (isPointInRect(x, y, layout.categoryBody))
    {
        for (int i = 0; i < static_cast<int>(kCategoryOrder.size()); ++i)
        {
            const SDL_FRect rowRect = getCategoryRowRect(layout, i);
            if (rowRect.y + rowRect.h > layout.categoryBody.y + layout.categoryBody.h)
            {
                break;
            }
            if (isPointInRect(x, y, rowRect))
            {
                onCategoryFilter = true;
                break;
            }
        }
    }
    onResetFilters = isPointInRect(x, y, layout.categoryResetButton);

    if (isPointInRect(x, y, layout.closeButton) ||
        isPointInRect(x, y, layout.bazarTab) ||
        isPointInRect(x, y, layout.blackTab) ||
        isPointInRect(x, y, layout.basicTab) ||
        isPointInRect(x, y, layout.eventTab) ||
        onCategoryFilter ||
        onResetFilters ||
        onSubmit)
    {
        return HudCursorType::POINTER;
    }
    if (onInput)
    {
        return HudCursorType::TEXT;
    }
    if (maxFirstRow > 0 && isPointInRect(x, y, layout.scrollTrack))
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (isPointInRect(x, y, headerDragRect))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

void MarketsAndBazarWidget::clearFocus(void)
{
    this->clearInputFocusInternal();
}

void MarketsAndBazarWidget::openBasicMarket(void)
{
    this->visible = true;
    this->activeTab = ActiveTab::BASIC_MARKET;
    this->widgetDragging = false;
    this->scrollBarDragging = false;
    this->scrollBarWheelHighlightSec = 0.0f;
    this->clearFocus();
}

void MarketsAndBazarWidget::draw(void) const
{
    MarketsAndBazarWidget* self = const_cast<MarketsAndBazarWidget*>(this);

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
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

    const ActiveTab currentTab = self->activeTab;
    const bool bazarTab = currentTab == ActiveTab::BAZARD;
    const MarketLayout layout = buildLayout(self->widgetRect, bazarTab);
    int firstRowFallback = 0;
    int* firstRowPtr = self->getFirstRowForTab(currentTab);
    int& firstRow = firstRowPtr != nullptr ? *firstRowPtr : firstRowFallback;
    std::vector<MarketRow>* activeMarketRows = bazarTab ? nullptr : self->getMarketRowsForTab(currentTab);
    std::vector<RC2D_Image>* activeMarketIcons = bazarTab ? nullptr : self->getMarketIconsForTab(currentTab);
    const std::vector<int> filteredRowIndices = self->buildFilteredRowIndices(currentTab);
    const int totalRows = static_cast<int>(filteredRowIndices.size());
    const int visibleRowsByHeight = (std::max)(1, static_cast<int>(std::floor(layout.body.h / kRowHeight)));
    const int visibleRows = (std::max)(1, (std::min)(visibleRowsByHeight, kMaxRowsPerPage));
    const int maxFirstRow = (std::max)(0, totalRows - visibleRows);
    firstRow = (std::max)(0, (std::min)(firstRow, maxFirstRow));
    const int lastDisplayRow = (std::min)(totalRows, firstRow + visibleRows);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &layout.outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &layout.inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.topBar);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.topBar);

    rc2d_graphics_setColor(self->activeTab == ActiveTab::BASIC_MARKET ? kTabActive : kTabInactive);
    rc2d_graphics_rectangle("fill", &layout.basicTab);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.basicTab);
    drawCentered(&self->smallFont, "Marche basique", layout.basicTab, self->activeTab == ActiveTab::BASIC_MARKET ? kTextGold : kTextMuted);

    rc2d_graphics_setColor(self->activeTab == ActiveTab::EVENT_MARKET ? kTabActive : kTabInactive);
    rc2d_graphics_rectangle("fill", &layout.eventTab);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.eventTab);
    drawCentered(&self->smallFont, "Marche d'event", layout.eventTab, self->activeTab == ActiveTab::EVENT_MARKET ? kTextGold : kTextMuted);

    rc2d_graphics_setColor(bazarTab ? kTabActive : kTabInactive);
    rc2d_graphics_rectangle("fill", &layout.bazarTab);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.bazarTab);
    drawCentered(&self->bodyFont, "Bazar", layout.bazarTab, bazarTab ? kTextGold : kTextMuted);

    rc2d_graphics_setColor(self->activeTab == ActiveTab::BLACK_MARKET ? kTabActive : kTabInactive);
    rc2d_graphics_rectangle("fill", &layout.blackTab);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.blackTab);
    drawCentered(&self->bodyFont, "Marche noir", layout.blackTab, self->activeTab == ActiveTab::BLACK_MARKET ? kTextGold : kTextMuted);

    self->controlIcons.drawCloseButton(layout.closeButton, kHeaderFill, kGold);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.categoryHeader);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.categoryHeader);
    drawCentered(&self->smallFont, "Categories", layout.categoryHeader, kTextGold);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &layout.categoryBody);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.categoryBody);

    for (int i = 0; i < static_cast<int>(kCategoryOrder.size()); ++i)
    {
        const SDL_FRect rowRect = getCategoryRowRect(layout, i);
        if (rowRect.y + rowRect.h > layout.categoryBody.y + layout.categoryBody.h)
        {
            break;
        }

        rc2d_graphics_setColor(kRowLine);
        rc2d_graphics_line(rowRect.x, rowRect.y, rowRect.x + rowRect.w, rowRect.y);

        const MarketCategory category = kCategoryOrder[static_cast<std::size_t>(i)];
        const bool enabled = self->isCategoryEnabled(category);
        const SDL_FRect checkbox = SDL_FRect{
            rowRect.x + 8.0f,
            rowRect.y + ((rowRect.h - 12.0f) * 0.5f),
            12.0f,
            12.0f
        };

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &checkbox);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &checkbox);

        if (enabled)
        {
            rc2d_graphics_line(checkbox.x + 2.0f, checkbox.y + 6.0f, checkbox.x + 5.0f, checkbox.y + 9.0f);
            rc2d_graphics_line(checkbox.x + 5.0f, checkbox.y + 9.0f, checkbox.x + 10.0f, checkbox.y + 2.5f);
        }

        drawTextAt(
            &self->smallFont,
            categoryLabel(category),
            checkbox.x + checkbox.w + 8.0f,
            rowRect.y + ((rowRect.h - 16.0f) * 0.5f),
            enabled ? kTextGold : kTextBody);
    }

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.categoryResetButton);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.categoryResetButton);
    drawCentered(&self->smallFont, "Reinitialiser les filtres", layout.categoryResetButton, kTextGold);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.tableHeader);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.tableHeader);

    if (bazarTab)
    {
        const SDL_FRect h1 = SDL_FRect{layout.col1X, layout.tableHeader.y, layout.col1W, layout.tableHeader.h};
        const SDL_FRect h2 = SDL_FRect{layout.col2X, layout.tableHeader.y, layout.col2W, layout.tableHeader.h};
        const SDL_FRect h3 = SDL_FRect{layout.col3X, layout.tableHeader.y, layout.col3W, layout.tableHeader.h};
        const SDL_FRect h4 = SDL_FRect{layout.col4X, layout.tableHeader.y, layout.col4W, layout.tableHeader.h};
        drawCentered(&self->smallFont, "Objet de la vente aux encheres", h1, kTextGold);
        drawCentered(&self->smallFont, "Quantite", h2, kTextGold);
        drawCentered(&self->smallFont, "Le plus offrant", h3, kTextGold);
        drawCentered(&self->smallFont, "Votre offre", h4, kTextGold);
        rc2d_graphics_setColor(kRowLine);
        rc2d_graphics_line(
            layout.col2X,
            layout.tableHeader.y + 1.0f,
            layout.col2X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
        rc2d_graphics_line(
            layout.col3X,
            layout.tableHeader.y + 1.0f,
            layout.col3X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
        rc2d_graphics_line(
            layout.col4X,
            layout.tableHeader.y + 1.0f,
            layout.col4X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
    }
    else
    {
        const SDL_FRect h1 = SDL_FRect{layout.col1X, layout.tableHeader.y, layout.col1W, layout.tableHeader.h};
        const SDL_FRect h2 = SDL_FRect{layout.col2X, layout.tableHeader.y, layout.col2W, layout.tableHeader.h};
        const SDL_FRect h3 = SDL_FRect{layout.col3X, layout.tableHeader.y, layout.col3W, layout.tableHeader.h};
        const SDL_FRect h4 = SDL_FRect{layout.col4X, layout.tableHeader.y, layout.col4W, layout.tableHeader.h};
        drawCentered(&self->smallFont, "Objet a vendre", h1, kTextGold);
        drawCentered(&self->smallFont, "Quantite", h2, kTextGold);
        drawCentered(&self->smallFont, "Prix", h3, kTextGold);
        drawCentered(&self->smallFont, "Votre offre", h4, kTextGold);
        rc2d_graphics_setColor(kRowLine);
        rc2d_graphics_line(
            layout.col2X,
            layout.tableHeader.y + 1.0f,
            layout.col2X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
        rc2d_graphics_line(
            layout.col3X,
            layout.tableHeader.y + 1.0f,
            layout.col3X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
        rc2d_graphics_line(
            layout.col4X,
            layout.tableHeader.y + 1.0f,
            layout.col4X,
            layout.tableHeader.y + layout.tableHeader.h - 1.0f);
    }

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &layout.body);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.body);

    for (int displayIndex = firstRow; displayIndex < lastDisplayRow; ++displayIndex)
    {
        const int sourceRow = filteredRowIndices[static_cast<std::size_t>(displayIndex)];
        const int visualRow = displayIndex - firstRow;
        const float rowY = layout.body.y + (static_cast<float>(visualRow) * kRowHeight);
        const SDL_FRect rowRect = SDL_FRect{layout.body.x, rowY, layout.contentWidth, kRowHeight};

        rc2d_graphics_setColor(kRowLine);
        rc2d_graphics_line(rowRect.x, rowRect.y, rowRect.x + rowRect.w, rowRect.y);

        const SDL_FRect iconRect = SDL_FRect{rowRect.x + 8.0f, rowRect.y + 8.0f, 76.0f, 76.0f};
        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &iconRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &iconRect);

        const float textX = iconRect.x + iconRect.w + 10.0f;
        const float nameY = rowRect.y + 14.0f;
        const float descY = rowRect.y + 45.0f;

        if (bazarTab)
        {
            const BazarRow& bazar = self->bazarRows[static_cast<std::size_t>(sourceRow)];
            if (sourceRow >= 0 && sourceRow < static_cast<int>(self->bazarRowIcons.size()))
            {
                drawImageFit(&self->bazarRowIcons[static_cast<std::size_t>(sourceRow)], iconRect);
            }
            drawTextAt(&self->bodyFont, bazar.itemName, textX, nameY, kTextGold);
            drawTextAt(&self->smallFont, bazar.itemDescription, textX, descY, kTextMuted);

            const SDL_FRect quantityRect = SDL_FRect{layout.col2X + 8.0f, rowRect.y + 8.0f, layout.col2W - 16.0f, rowRect.h - 16.0f};
            const std::string quantityText = formatWithDots(bazar.quantity);
            drawCentered(&self->bodyFont, quantityText.c_str(), quantityRect, kTextBody);

            SDL_FRect bidderRect = SDL_FRect{layout.col3X + 8.0f, rowRect.y + 8.0f, layout.col3W - 16.0f, rowRect.h - 16.0f};
            const std::string bidder = bazar.highestBidder.empty() ? "-" : bazar.highestBidder;
            drawCentered(&self->bodyFont, bidder.c_str(), bidderRect, kTextBody);

            SDL_FRect inputRect = SDL_FRect{layout.col4X + 8.0f, rowRect.y + 12.0f, layout.col4W - 16.0f, 29.0f};
            SDL_FRect submitRect = SDL_FRect{layout.col4X + 8.0f, rowRect.y + 46.0f, layout.col4W - 16.0f, 30.0f};
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &inputRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &inputRect);
            drawTextAt(&self->bodyFont, bazar.yourOffer, inputRect.x + 8.0f, inputRect.y + 5.0f, kTextGold);

            const bool focused = self->inputFocused &&
                                 self->focusedTab == ActiveTab::BAZARD &&
                                 self->focusedRow == sourceRow &&
                                 self->cursorVisible;
            if (focused)
            {
                const std::size_t cursor = (std::min)(self->cursorIndex, bazar.yourOffer.size());
                const std::string prefix = bazar.yourOffer.substr(0, cursor);
                const float cx = inputRect.x + 8.0f + measureTextWidth(&self->bodyFont, prefix);
                rc2d_graphics_setColor(kTextGold);
                rc2d_graphics_line(cx, inputRect.y + 5.0f, cx, inputRect.y + inputRect.h - 5.0f);
            }

            rc2d_graphics_setColor(kButtonFill);
            rc2d_graphics_rectangle("fill", &submitRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &submitRect);
            drawCentered(&self->smallFont, "Soumettre", submitRect, kTextBody);
        }
        else
        {
            if (activeMarketRows == nullptr || sourceRow >= static_cast<int>(activeMarketRows->size()))
            {
                continue;
            }

            const MarketRow& black = (*activeMarketRows)[static_cast<std::size_t>(sourceRow)];
            if (activeMarketIcons != nullptr && sourceRow >= 0 && sourceRow < static_cast<int>(activeMarketIcons->size()))
            {
                drawImageFit(&(*activeMarketIcons)[static_cast<std::size_t>(sourceRow)], iconRect);
            }
            drawTextAt(&self->bodyFont, black.itemName, textX, nameY, kTextGold);
            drawTextAt(&self->smallFont, black.itemDescription, textX, descY, kTextMuted);

            const SDL_FRect quantityRect = SDL_FRect{layout.col2X + 8.0f, rowRect.y + 8.0f, layout.col2W - 16.0f, rowRect.h - 16.0f};
            const SDL_FRect priceRect = SDL_FRect{layout.col3X + 8.0f, rowRect.y + 8.0f, layout.col3W - 16.0f, rowRect.h - 16.0f};
            const std::string quantityText = formatWithDots(black.quantity);
            drawCentered(&self->bodyFont, quantityText.c_str(), quantityRect, kTextBody);

            if (black.prices.empty())
            {
                drawCentered(&self->bodyFont, "-", priceRect, kTextGold);
            }
            else
            {
                const float lineHeight = 18.0f;
                const float topPadding = 2.0f;
                const int maxLines = (std::max)(
                    1,
                    static_cast<int>(std::floor((priceRect.h - (topPadding * 2.0f)) / lineHeight)));
                const int lineCount = (std::min)(static_cast<int>(black.prices.size()), maxLines);

                for (int i = 0; i < lineCount; ++i)
                {
                    const MarketPriceData& priceLine = black.prices[static_cast<std::size_t>(i)];
                    std::string priceText = formatWithDots((std::max)(0, priceLine.amount));
                    priceText.push_back(' ');
                    priceText += marketCurrencyLabel(priceLine.currency);

                    const SDL_FRect lineRect = SDL_FRect{
                        priceRect.x,
                        priceRect.y + topPadding + (static_cast<float>(i) * lineHeight),
                        priceRect.w,
                        lineHeight
                    };
                    drawCentered(&self->smallFont, priceText.c_str(), lineRect, kTextGold);
                }
            }

            SDL_FRect inputRect = SDL_FRect{layout.col4X + 8.0f, rowRect.y + 12.0f, layout.col4W - 16.0f, 29.0f};
            SDL_FRect submitRect = SDL_FRect{layout.col4X + 8.0f, rowRect.y + 46.0f, layout.col4W - 16.0f, 30.0f};
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &inputRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &inputRect);
            drawTextAt(&self->bodyFont, black.yourOffer, inputRect.x + 8.0f, inputRect.y + 5.0f, kTextGold);

            const bool focused = self->inputFocused &&
                                 self->focusedTab == self->activeTab &&
                                 self->focusedRow == sourceRow &&
                                 self->cursorVisible;
            if (focused)
            {
                const std::size_t cursor = (std::min)(self->cursorIndex, black.yourOffer.size());
                const std::string prefix = black.yourOffer.substr(0, cursor);
                const float cx = inputRect.x + 8.0f + measureTextWidth(&self->bodyFont, prefix);
                rc2d_graphics_setColor(kTextGold);
                rc2d_graphics_line(cx, inputRect.y + 5.0f, cx, inputRect.y + inputRect.h - 5.0f);
            }

            rc2d_graphics_setColor(kButtonFill);
            rc2d_graphics_rectangle("fill", &submitRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &submitRect);
            drawCentered(&self->smallFont, "Acheter", submitRect, kTextBody);
        }
    }

    rc2d_graphics_setColor(kRowLine);
    const int displayedRowCount = lastDisplayRow - firstRow;
    if (displayedRowCount > 0)
    {
        const float lastRowSeparatorY = layout.body.y + (static_cast<float>(displayedRowCount) * kRowHeight);
        rc2d_graphics_line(layout.body.x, lastRowSeparatorY, layout.body.x + layout.contentWidth, lastRowSeparatorY);
    }
    rc2d_graphics_line(layout.body.x, layout.body.y + layout.body.h, layout.body.x + layout.contentWidth, layout.body.y + layout.body.h);
    rc2d_graphics_line(layout.col2X, layout.body.y + 1.0f, layout.col2X, layout.body.y + layout.body.h - 1.0f);
    rc2d_graphics_line(layout.col3X, layout.body.y + 1.0f, layout.col3X, layout.body.y + layout.body.h - 1.0f);
    rc2d_graphics_line(layout.col4X, layout.body.y + 1.0f, layout.col4X, layout.body.y + layout.body.h - 1.0f);

    if (maxFirstRow > 0)
    {
        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (layout.scrollTrack.h * static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, layout.scrollTrack.h - thumbHeight);
        const float ratio = static_cast<float>(firstRow) / static_cast<float>(maxFirstRow);
        const SDL_FRect thumb = SDL_FRect{
            layout.scrollTrack.x,
            layout.scrollTrack.y + (thumbTravel * ratio),
            layout.scrollTrack.w,
            thumbHeight
        };

        rc2d_graphics_setColor(kScrollTrack);
        rc2d_graphics_rectangle("fill", &layout.scrollTrack);
        rc2d_graphics_setColor(self->scrollBarDragging || (self->scrollBarWheelHighlightSec > 0.0f) ? kScrollThumbDragFill : kScrollThumb);
        rc2d_graphics_rectangle("fill", &thumb);
    }

    if (bazarTab)
    {
        rc2d_graphics_setColor(kHeaderFill);
        rc2d_graphics_rectangle("fill", &layout.timerBox);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.timerBox);
        drawCentered(&self->bodyFont, self->timerText.c_str(), layout.timerBox, kTextGold);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}








