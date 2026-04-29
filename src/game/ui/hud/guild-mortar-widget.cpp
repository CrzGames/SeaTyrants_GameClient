#include "game/ui/hud/guild-mortar-widget.h"

#include "core/context.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

static constexpr float kRefW = 760.0f;
static constexpr float kRefH = 527.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kPanelSoftFill = RC2D_Color{12, 12, 14, 224};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextBody = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{132, 112, 88, 255};
static constexpr RC2D_Color kButtonFill = RC2D_Color{52, 45, 37, 238};
static constexpr RC2D_Color kButtonHoverFill = RC2D_Color{78, 62, 44, 245};
static constexpr RC2D_Color kButtonActiveFill = RC2D_Color{24, 86, 62, 210};
static constexpr RC2D_Color kButtonActiveHoverFill = RC2D_Color{31, 112, 78, 228};
static constexpr RC2D_Color kInputFill = RC2D_Color{8, 11, 18, 238};
static constexpr RC2D_Color kInputSelectionFill = RC2D_Color{67, 96, 144, 215};
static constexpr RC2D_Color kRowSelectedFill = RC2D_Color{48, 24, 12, 236};
static constexpr RC2D_Color kScrollTrack = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumb = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kScrollThumbDragFill = RC2D_Color{184, 132, 30, 245};
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr float kLevelRowHeight = 26.0f;
static constexpr int kLevelVisibleRows = 3;
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
static constexpr std::size_t kTransferGoldMaxDigits = 16U;

static SDL_FRect getGuildMortarRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH
    };
}

static bool isPointInRect(float x, float y, const SDL_FRect& rect)
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
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

static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
}

static std::string formatWithDots(std::int64_t value)
{
    std::string raw = std::to_string((std::max)(static_cast<std::int64_t>(0), value));
    std::string out;
    int count = 0;
    for (int i = static_cast<int>(raw.size()) - 1; i >= 0; --i)
    {
        if (count == 3)
        {
            out.push_back('.');
            count = 0;
        }
        out.push_back(raw[static_cast<std::size_t>(i)]);
        ++count;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

static std::string formatSeconds(float seconds)
{
    const float clamped = (std::max)(0.0f, seconds);
    const int roundedTenths = static_cast<int>(std::round(clamped * 10.0f));
    if ((roundedTenths % 10) == 0)
    {
        return std::to_string(roundedTenths / 10);
    }
    return std::to_string(roundedTenths / 10) + "," + std::to_string(roundedTenths % 10);
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

    const bool looksLikeKeypad = (lower.find("kp") != std::string::npos) ||
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

static bool parseGoldAmountInput(const std::string& text, std::int64_t* outAmount)
{
    if (outAmount == nullptr)
    {
        return false;
    }
    *outAmount = 0;
    if (text.empty())
    {
        return false;
    }

    std::int64_t value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }

        const std::int64_t digit = static_cast<std::int64_t>(c - '0');
        if (value > ((std::numeric_limits<std::int64_t>::max)() - digit) / 10)
        {
            return false;
        }
        value = (value * 10) + digit;
    }

    if (value <= 0)
    {
        return false;
    }

    *outAmount = value;
    return true;
}

static void drawText(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
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

static void drawCentered(RC2D_Font* font, const std::string& text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_drawText(
        &t,
        std::round(rect.x + ((rect.w - static_cast<float>(w)) * 0.5f)),
        std::round(rect.y + ((rect.h - static_cast<float>(h)) * 0.5f)));
    rc2d_graphics_destroyText(&t);
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

static void drawStatBlock(
    RC2D_Font* labelFont,
    RC2D_Font* valueFont,
    const std::string& label,
    const std::string& value,
    const SDL_FRect& rect)
{
    drawCentered(labelFont, label, SDL_FRect{rect.x, rect.y, rect.w, 18.0f}, kTextMuted);
    drawCentered(valueFont, value, SDL_FRect{rect.x, rect.y + 22.0f, rect.w, 24.0f}, kTextBody);
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

    const float maxW = (std::max)(1.0f, target.w - (padding * 2.0f));
    const float maxH = (std::max)(1.0f, target.h - (padding * 2.0f));
    const float scale = (std::min)(maxW / texW, maxH / texH);
    const float drawW = texW * scale;
    const float drawH = texH * scale;
    RC2D_Image imageCopy = image;
    const RC2D_Quad quad = rc2d_graphics_newQuad(&imageCopy, 0.0f, 0.0f, texW, texH);
    rc2d_graphics_drawQuad(
        &imageCopy,
        &quad,
        std::round(target.x + ((target.w - drawW) * 0.5f)),
        std::round(target.y + ((target.h - drawH) * 0.5f)),
        0.0,
        scale,
        scale,
        -1.0f,
        -1.0f,
        false,
        false);
}

GuildMortarWidget::GuildMortarWidget(void)
    : mortarLevels{},
      selectedMortarLevelIndex(-1),
      mortarEnabled(false),
      treasuryGoldAmount(0),
      treasuryRubiesAmount(0),
      treasuryPearlsAmount(0),
      treasuryCrystalsAmount(0),
      transferGoldInput{},
      transferGoldCursorIndex(0U),
      transferGoldSelectionAnchorIndex(0U),
      transferGoldInputFocused(false),
      transferGoldSelectingWithMouse(false),
      transferGoldCursorVisible(true),
      transferGoldCursorBlinkSec(0.0f),
      titleFont{},
      bodyFont{},
      valueFont{},
      goldIcon{},
      rubiesIcon{},
      pearlsIcon{},
      crystalsIcon{},
      arrowDownIcon{},
      controlIcons{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      levelDropdownOpen(false),
      levelFirstRow(0),
      levelScrollDragging(false),
      levelScrollDragOffsetY(0.0f),
      levelScrollWheelHighlightSec(0.0f)
{
}

GuildMortarWidget::~GuildMortarWidget(void)
{
}

void GuildMortarWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 18.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->valueFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 17.0f);
    this->goldIcon = LoadStorageImage("assets/images/ui-scene-game/money-gold.png", RC2D_STORAGE_TITLE);
    this->rubiesIcon = LoadStorageImage("assets/images/ui-scene-game/money-rubies.png", RC2D_STORAGE_TITLE);
    this->pearlsIcon = LoadStorageImage("assets/images/ui-scene-game/money_pearls.png", RC2D_STORAGE_TITLE);
    this->crystalsIcon = LoadStorageImage("assets/images/ui-scene-game/money_crystals.png", RC2D_STORAGE_TITLE);
    this->arrowDownIcon = LoadStorageImage("assets/images/ui-scene-game/icon-arrowdown.png", RC2D_STORAGE_TITLE);
    this->controlIcons.load();

    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = baseRect;
    this->visible = true;
    this->widgetDragging = false;
    this->levelDropdownOpen = false;
    this->levelFirstRow = 0;
    this->levelScrollDragging = false;
    this->levelScrollWheelHighlightSec = 0.0f;
    this->transferGoldInput.clear();
    this->transferGoldCursorIndex = 0U;
    this->transferGoldSelectionAnchorIndex = 0U;
    this->transferGoldInputFocused = false;
    this->transferGoldSelectingWithMouse = false;
    this->transferGoldCursorVisible = true;
    this->transferGoldCursorBlinkSec = 0.0f;
}

void GuildMortarWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageImageRef(&this->arrowDownIcon);
    ResetStorageImageRef(&this->crystalsIcon);
    ResetStorageImageRef(&this->pearlsIcon);
    ResetStorageImageRef(&this->rubiesIcon);
    ResetStorageImageRef(&this->goldIcon);
    ResetStorageFontRef(&this->valueFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void GuildMortarWidget::update(double dt)
{
    const float dtF = static_cast<float>(dt);
    if (this->levelScrollWheelHighlightSec > 0.0f)
    {
        this->levelScrollWheelHighlightSec -= dtF;
        if (this->levelScrollWheelHighlightSec < 0.0f)
        {
            this->levelScrollWheelHighlightSec = 0.0f;
        }
    }
    if (this->transferGoldInputFocused)
    {
        this->transferGoldCursorBlinkSec += dtF;
        while (this->transferGoldCursorBlinkSec >= 0.5f)
        {
            this->transferGoldCursorBlinkSec -= 0.5f;
            this->transferGoldCursorVisible = !this->transferGoldCursorVisible;
        }
    }

    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!this->visible || (!this->widgetDragging && !this->levelScrollDragging && !this->transferGoldSelectingWithMouse))
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        this->levelScrollDragging = false;
        this->transferGoldSelectingWithMouse = false;
        return;
    }

    if (this->levelScrollDragging)
    {
        const int totalRows = static_cast<int>(this->mortarLevels.size());
        const int maxFirstRow = (std::max)(0, totalRows - kLevelVisibleRows);
        if (maxFirstRow <= 0)
        {
            this->levelFirstRow = 0;
            this->levelScrollDragging = false;
            return;
        }

        const SDL_FRect listRect = SDL_FRect{
            this->widgetRect.x + 402.0f,
            this->widgetRect.y + 126.0f,
            332.0f,
            (static_cast<float>(kLevelVisibleRows) * kLevelRowHeight) + 8.0f
        };
        const SDL_FRect scrollTrackRect = SDL_FRect{
            listRect.x + listRect.w - (kScrollBarWidth + kScrollBarPadding),
            listRect.y + kScrollBarPadding,
            kScrollBarWidth,
            listRect.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            scrollTrackRect.h * static_cast<float>(kLevelVisibleRows) / static_cast<float>((std::max)(1, totalRows)));
        const float thumbTravel = (std::max)(1.0f, scrollTrackRect.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;
        const float thumbTop = clampf(mouseY - this->levelScrollDragOffsetY, scrollTrackRect.y, scrollTrackRect.y + thumbTravel);
        const float t = (thumbTop - scrollTrackRect.y) / thumbTravel;
        this->levelFirstRow = static_cast<int>(t * static_cast<float>(maxFirstRow) + 0.5f);
        this->levelFirstRow = (std::max)(0, (std::min)(this->levelFirstRow, maxFirstRow));
        return;
    }

    if (this->transferGoldSelectingWithMouse)
    {
        const SDL_FRect transferInputRect = SDL_FRect{
            this->widgetRect.x + 34.0f,
            this->widgetRect.y + 402.0f,
            190.0f,
            30.0f
        };
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseY;
        this->transferGoldCursorIndex = this->getTransferGoldCursorIndexFromPosition(mouseX, transferInputRect);
        this->transferGoldCursorVisible = true;
        this->transferGoldCursorBlinkSec = 0.0f;
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

bool GuildMortarWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
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

    const SDL_FRect headerRect = SDL_FRect{this->widgetRect.x + 5.0f, this->widgetRect.y + 5.0f, this->widgetRect.w - 10.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{this->widgetRect.x + this->widgetRect.w - 28.0f, headerRect.y + 5.0f, 20.0f, 20.0f};
    const SDL_FRect toggleButtonRect = SDL_FRect{this->widgetRect.x + 34.0f, this->widgetRect.y + 122.0f, 320.0f, 34.0f};
    const SDL_FRect upgradeButtonRect = SDL_FRect{this->widgetRect.x + 416.0f, this->widgetRect.y + 388.0f, 304.0f, 34.0f};
    const SDL_FRect transferInputRect = SDL_FRect{this->widgetRect.x + 34.0f, this->widgetRect.y + 402.0f, 190.0f, 30.0f};
    const SDL_FRect transferButtonRect = SDL_FRect{this->widgetRect.x + 232.0f, this->widgetRect.y + 402.0f, 122.0f, 30.0f};
    const bool hasNextMortarLevel =
        this->selectedMortarLevelIndex >= 0 &&
        (this->selectedMortarLevelIndex + 1) < static_cast<int>(this->mortarLevels.size());

    if (isPointInRect(x, y, closeButtonRect))
    {
        this->hide();
        return true;
    }
    if (isPointInRect(x, y, transferInputRect))
    {
        this->transferGoldInputFocused = true;
        this->transferGoldCursorVisible = true;
        this->transferGoldCursorBlinkSec = 0.0f;
        this->levelDropdownOpen = false;
        if (clicks >= 2)
        {
            this->transferGoldSelectionAnchorIndex = 0U;
            this->transferGoldCursorIndex = this->transferGoldInput.size();
            this->transferGoldSelectingWithMouse = false;
            this->widgetDragging = false;
            return true;
        }

        const std::size_t clickedIndex = this->getTransferGoldCursorIndexFromPosition(x, transferInputRect);
        this->transferGoldCursorIndex = clickedIndex;
        this->transferGoldSelectionAnchorIndex = clickedIndex;
        this->transferGoldSelectingWithMouse = true;
        this->widgetDragging = false;
        return true;
    }
    this->transferGoldInputFocused = false;
    this->transferGoldSelectingWithMouse = false;
    this->transferGoldCursorIndex = (std::min)(this->transferGoldCursorIndex, this->transferGoldInput.size());
    this->clearTransferGoldSelection();
    if (isPointInRect(x, y, transferButtonRect))
    {
        this->levelDropdownOpen = false;
        if (this->selectedMortarLevelIndex >= 0 &&
            this->selectedMortarLevelIndex < static_cast<int>(this->mortarLevels.size()))
        {
            std::int64_t transferAmount = 0;
            if (parseGoldAmountInput(this->transferGoldInput, &transferAmount) &&
                transferAmount <= this->treasuryGoldAmount)
            {
                MortarLevelEntry& selectedMortar =
                    this->mortarLevels[static_cast<std::size_t>(this->selectedMortarLevelIndex)];
                const bool chestCanReceive =
                    selectedMortar.unitChestGoldAmount <=
                    ((std::numeric_limits<std::int64_t>::max)() - transferAmount);
                if (chestCanReceive)
                {
                    this->treasuryGoldAmount -= transferAmount;
                    selectedMortar.unitChestGoldAmount += transferAmount;
                    this->transferGoldInput.clear();
                    this->transferGoldCursorIndex = 0U;
                    this->transferGoldSelectionAnchorIndex = 0U;
                    this->transferGoldCursorVisible = true;
                    this->transferGoldCursorBlinkSec = 0.0f;
                }
            }
        }
        return true;
    }
    this->levelDropdownOpen = false;
    if (isPointInRect(x, y, toggleButtonRect))
    {
        this->mortarEnabled = !this->mortarEnabled;
        return true;
    }
    if (hasNextMortarLevel && isPointInRect(x, y, upgradeButtonRect))
    {
        return true;
    }
    if (isPointInRect(x, y, headerRect))
    {
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }
    return true;
}

bool GuildMortarWidget::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    (void)mod;
    (void)isrepeat;

    if (!this->visible || !this->transferGoldInputFocused)
    {
        return false;
    }

    if (scancode == SDL_SCANCODE_ESCAPE || scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER)
    {
        this->transferGoldInputFocused = false;
        this->transferGoldSelectingWithMouse = false;
        this->clearTransferGoldSelection();
        return true;
    }

    char digit = extractDigitFromKeyLabel(key);
    if (digit == '\0' && scancode >= SDL_SCANCODE_0 && scancode <= SDL_SCANCODE_9)
    {
        digit = static_cast<char>('0' + (scancode - SDL_SCANCODE_0));
    }
    if (digit == '\0' && scancode >= SDL_SCANCODE_KP_0 && scancode <= SDL_SCANCODE_KP_9)
    {
        digit = static_cast<char>('0' + (scancode - SDL_SCANCODE_KP_0));
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
        if (this->hasTransferGoldSelection())
        {
            this->deleteSelectedTransferGoldText();
        }
        if (this->transferGoldInput.size() < kTransferGoldMaxDigits)
        {
            this->transferGoldCursorIndex = (std::min)(this->transferGoldCursorIndex, this->transferGoldInput.size());
            this->transferGoldInput.insert(this->transferGoldCursorIndex, 1, digit);
            ++this->transferGoldCursorIndex;
        }
        this->clearTransferGoldSelection();
        this->transferGoldCursorVisible = true;
        this->transferGoldCursorBlinkSec = 0.0f;
        return true;
    }

    switch (scancode)
    {
        case SDL_SCANCODE_LEFT:
            if (this->hasTransferGoldSelection())
            {
                this->transferGoldCursorIndex = this->getTransferGoldSelectionStart();
            }
            else if (this->transferGoldCursorIndex > 0U)
            {
                --this->transferGoldCursorIndex;
            }
            this->clearTransferGoldSelection();
            break;
        case SDL_SCANCODE_RIGHT:
            if (this->hasTransferGoldSelection())
            {
                this->transferGoldCursorIndex = this->getTransferGoldSelectionEnd();
            }
            else if (this->transferGoldCursorIndex < this->transferGoldInput.size())
            {
                ++this->transferGoldCursorIndex;
            }
            this->clearTransferGoldSelection();
            break;
        case SDL_SCANCODE_HOME:
            this->transferGoldCursorIndex = 0U;
            this->clearTransferGoldSelection();
            break;
        case SDL_SCANCODE_END:
            this->transferGoldCursorIndex = this->transferGoldInput.size();
            this->clearTransferGoldSelection();
            break;
        case SDL_SCANCODE_BACKSPACE:
            if (this->hasTransferGoldSelection())
            {
                this->deleteSelectedTransferGoldText();
            }
            else if (this->transferGoldCursorIndex > 0U && !this->transferGoldInput.empty())
            {
                this->transferGoldInput.erase(this->transferGoldCursorIndex - 1U, 1U);
                --this->transferGoldCursorIndex;
            }
            this->clearTransferGoldSelection();
            break;
        case SDL_SCANCODE_DELETE:
            if (this->hasTransferGoldSelection())
            {
                this->deleteSelectedTransferGoldText();
            }
            else if (this->transferGoldCursorIndex < this->transferGoldInput.size())
            {
                this->transferGoldInput.erase(this->transferGoldCursorIndex, 1U);
            }
            this->clearTransferGoldSelection();
            break;
        default:
            return false;
    }

    this->transferGoldCursorVisible = true;
    this->transferGoldCursorBlinkSec = 0.0f;
    return true;
}

bool GuildMortarWidget::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float wheel_x,
    float wheel_y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)direction;
    (void)wheel_x;
    (void)wheel_y;
    (void)integer_x;
    (void)integer_y;
    (void)mouse_x;
    (void)mouse_y;
    (void)mouseID;

    return false;
}

void GuildMortarWidget::draw(void) const
{
    GuildMortarWidget* self = const_cast<GuildMortarWidget*>(this);
    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
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
    const SDL_FRect closeButtonRect = SDL_FRect{outer.x + outer.w - 28.0f, header.y + 5.0f, 20.0f, 20.0f};

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    const SDL_FRect leftPanel = SDL_FRect{outer.x + 12.0f, outer.y + 46.0f, 360.0f, 407.0f};
    const SDL_FRect rightPanel = SDL_FRect{outer.x + 388.0f, outer.y + 46.0f, 360.0f, 407.0f};
    const SDL_FRect treasuryPanel = SDL_FRect{outer.x + 12.0f, outer.y + 467.0f, 736.0f, 44.0f};
    const SDL_FRect toggleButton = SDL_FRect{leftPanel.x + 22.0f, leftPanel.y + 76.0f, leftPanel.w - 44.0f, 34.0f};

    const MortarLevelEntry selected = self->getSelectedDisplayEntry();
    const bool hasNextMortarLevel =
        self->selectedMortarLevelIndex >= 0 &&
        (self->selectedMortarLevelIndex + 1) < static_cast<int>(self->mortarLevels.size());
    const MortarLevelEntry upgrade =
        hasNextMortarLevel
            ? self->mortarLevels[static_cast<std::size_t>(self->selectedMortarLevelIndex + 1)]
            : MortarLevelEntry{};

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
    drawText(&self->titleFont, "Guild Mortier", header.x + 10.0f, header.y + 5.0f, kTextGold);
    self->controlIcons.drawCloseButton(closeButtonRect, kHeaderFill, kGold);

    auto drawPanel = [&](const SDL_FRect& panel, const std::string& title) {
        rc2d_graphics_setColor(kPanelSoftFill);
        rc2d_graphics_rectangle("fill", &panel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &panel);
        rc2d_graphics_setColor(kHeaderFill);
        const SDL_FRect panelHeader = SDL_FRect{panel.x, panel.y, panel.w, 28.0f};
        rc2d_graphics_rectangle("fill", &panelHeader);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &panelHeader);
        drawText(&self->bodyFont, title, panelHeader.x + 10.0f, panelHeader.y + 5.0f, kTextGold);
    };
    drawPanel(leftPanel, "Etat actuel du mortier");
    drawPanel(rightPanel, "Mise a niveau du Mortier");

    drawText(&self->valueFont, selected.name.empty() ? "Mortier" : selected.name, leftPanel.x + 22.0f, leftPanel.y + 42.0f, kTextGold);
    const bool toggleHovered = isPointInRect(mouseX, mouseY, toggleButton);
    rc2d_graphics_setColor(self->mortarEnabled
        ? (toggleHovered ? kButtonActiveHoverFill : kButtonActiveFill)
        : (toggleHovered ? kButtonHoverFill : kButtonFill));
    rc2d_graphics_rectangle("fill", &toggleButton);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &toggleButton);
    drawCentered(&self->valueFont, self->mortarEnabled ? "Desactiver" : "Activer", toggleButton, kTextGold);

    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Degats d'attaque",
        formatWithDots(selected.damage),
        SDL_FRect{leftPanel.x + 20.0f, leftPanel.y + 126.0f, leftPanel.w - 40.0f, 48.0f});
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Vitesse d'attaque en seconde",
        formatSeconds(selected.attackSpeedSec),
        SDL_FRect{leftPanel.x + 20.0f, leftPanel.y + 184.0f, 150.0f, 48.0f});
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Portee d'attaque",
        formatWithDots(selected.attackRange),
        SDL_FRect{leftPanel.x + 190.0f, leftPanel.y + 184.0f, 150.0f, 48.0f});

    const SDL_FRect goldCostIcon = SDL_FRect{leftPanel.x + 22.0f, leftPanel.y + 254.0f, 24.0f, 24.0f};
    drawText(&self->bodyFont, "Cout par tir de mortier", leftPanel.x + 20.0f, leftPanel.y + 232.0f, kTextMuted);
    drawImageFit(self->goldIcon, goldCostIcon, 1.0f);
    drawText(&self->valueFont, formatWithDots(selected.shotCostGold), leftPanel.x + 58.0f, goldCostIcon.y + 1.0f, kTextGold);
    drawText(&self->bodyFont, "Montant actuel du coffre :", leftPanel.x + 20.0f, leftPanel.y + 286.0f, kTextMuted);
    const SDL_FRect chestInlineGoldIcon = SDL_FRect{leftPanel.x + 218.0f, leftPanel.y + 282.0f, 24.0f, 24.0f};
    drawImageFit(self->goldIcon, chestInlineGoldIcon, 1.0f);
    drawText(&self->valueFont, formatWithDots(selected.unitChestGoldAmount), chestInlineGoldIcon.x + 32.0f, chestInlineGoldIcon.y + 1.0f, kTextGold);

    drawText(&self->bodyFont, "Transferer de l'or de la tresorerie de la guild", leftPanel.x + 20.0f, leftPanel.y + 320.0f, kTextMuted);
    drawText(&self->bodyFont, "vers le coffre du mortier", leftPanel.x + 20.0f, leftPanel.y + 338.0f, kTextMuted);
    const SDL_FRect transferInput = SDL_FRect{leftPanel.x + 22.0f, leftPanel.y + 356.0f, 190.0f, 30.0f};
    const SDL_FRect transferButton = SDL_FRect{leftPanel.x + 220.0f, leftPanel.y + 356.0f, leftPanel.w - 242.0f, 30.0f};
    rc2d_graphics_setColor(kInputFill);
    rc2d_graphics_rectangle("fill", &transferInput);
    rc2d_graphics_setColor(self->transferGoldInputFocused ? kSilver : kGold);
    rc2d_graphics_rectangle("line", &transferInput);
    if (self->hasTransferGoldSelection())
    {
        const std::size_t selectionStart = self->getTransferGoldSelectionStart();
        const std::size_t selectionEnd = self->getTransferGoldSelectionEnd();
        const std::string beforeSelection = self->transferGoldInput.substr(0, selectionStart);
        const std::string selectedText = self->transferGoldInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(transferInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        const float selectionW = measureTextWidth(&self->bodyFont, selectedText);
        const SDL_FRect selectionRect = SDL_FRect{
            selectionX - 1.0f,
            transferInput.y + 5.0f,
            (std::max)(2.0f, selectionW + 2.0f),
            transferInput.h - 10.0f
        };
        rc2d_graphics_setColor(kInputSelectionFill);
        rc2d_graphics_rectangle("fill", &selectionRect);
    }
    const std::string transferText = self->transferGoldInput.empty() ? "Montant en or" : self->transferGoldInput;
    drawText(&self->bodyFont, transferText, transferInput.x + 8.0f, transferInput.y + 6.0f, self->transferGoldInput.empty() ? kTextMuted : kTextGold);
    if (self->hasTransferGoldSelection())
    {
        const std::size_t selectionStart = self->getTransferGoldSelectionStart();
        const std::size_t selectionEnd = self->getTransferGoldSelectionEnd();
        const std::string beforeSelection = self->transferGoldInput.substr(0, selectionStart);
        const std::string selectedText = self->transferGoldInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(transferInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        drawText(&self->bodyFont, selectedText, selectionX, transferInput.y + 6.0f, kTextBody);
    }
    if (self->transferGoldInputFocused && self->transferGoldCursorVisible)
    {
        const std::size_t cursor = (std::min)(self->transferGoldCursorIndex, self->transferGoldInput.size());
        const float cursorX = transferInput.x + 8.0f + measureTextWidth(&self->bodyFont, self->transferGoldInput.substr(0, cursor));
        rc2d_graphics_setColor(kTextGold);
        rc2d_graphics_line(cursorX, transferInput.y + 6.0f, cursorX, transferInput.y + transferInput.h - 6.0f);
    }
    rc2d_graphics_setColor(isPointInRect(mouseX, mouseY, transferButton) ? kButtonHoverFill : kButtonFill);
    rc2d_graphics_rectangle("fill", &transferButton);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &transferButton);
    drawCentered(&self->bodyFont, "Transferer", transferButton, kTextGold);

    if (!hasNextMortarLevel)
    {
        drawCentered(
            &self->valueFont,
            "Mortier a deja le lvl maximum atteint",
            SDL_FRect{rightPanel.x + 22.0f, rightPanel.y + 80.0f, rightPanel.w - 44.0f, rightPanel.h - 110.0f},
            kTextGold);
    }
    else
    {
        drawText(
            &self->valueFont,
            upgrade.name.empty() ? "Mortier" : upgrade.name,
            rightPanel.x + 28.0f,
            rightPanel.y + 52.0f,
            kTextGold);

        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Degats d'attaque",
            formatWithDots(upgrade.damage),
            SDL_FRect{rightPanel.x + 28.0f, rightPanel.y + 112.0f, rightPanel.w - 56.0f, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Vitesse d'attaque en seconde",
            formatSeconds(upgrade.attackSpeedSec),
            SDL_FRect{rightPanel.x + 28.0f, rightPanel.y + 170.0f, 140.0f, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Portee d'attaque",
            formatWithDots(upgrade.attackRange),
            SDL_FRect{rightPanel.x + 194.0f, rightPanel.y + 170.0f, 140.0f, 48.0f});

        auto drawUpgradeCurrencyCost = [&](const RC2D_Image& icon, std::int64_t amount, const SDL_FRect& area) {
            const SDL_FRect iconRect = SDL_FRect{area.x, area.y + 2.0f, 22.0f, 22.0f};
            const std::string amountText = formatWithDots(amount);
            const float valueWidth = measureTextWidth(&self->valueFont, amountText);
            const float contentWidth = 22.0f + 8.0f + valueWidth;
            const float startX = area.x + (std::max)(0.0f, (area.w - contentWidth) * 0.5f);
            const SDL_FRect centeredIconRect = SDL_FRect{startX, iconRect.y, iconRect.w, iconRect.h};
            drawImageFit(icon, centeredIconRect, 1.0f);
            drawText(&self->valueFont, amountText, startX + 30.0f, area.y + 1.0f, kTextGold);
        };

        drawText(&self->bodyFont, "Cout par tir de mortier", rightPanel.x + 28.0f, rightPanel.y + 228.0f, kTextMuted);
        const SDL_FRect upgradeShotCostIcon = SDL_FRect{rightPanel.x + 28.0f, rightPanel.y + 252.0f, 24.0f, 24.0f};
        drawImageFit(self->goldIcon, upgradeShotCostIcon, 1.0f);
        drawText(
            &self->valueFont,
            formatWithDots(upgrade.shotCostGold),
            upgradeShotCostIcon.x + 36.0f,
            upgradeShotCostIcon.y + 1.0f,
            kTextGold);

        drawText(&self->bodyFont, "Cout pour ameliorer", rightPanel.x + 28.0f, rightPanel.y + 286.0f, kTextMuted);
        drawText(&self->bodyFont, "Cout pour ameliorer", rightPanel.x + 194.0f, rightPanel.y + 286.0f, kTextMuted);
        drawUpgradeCurrencyCost(
            self->rubiesIcon,
            upgrade.upgradeRubiesCost,
            SDL_FRect{rightPanel.x + 28.0f, rightPanel.y + 310.0f, 140.0f, 26.0f});
        drawUpgradeCurrencyCost(
            self->pearlsIcon,
            upgrade.upgradePearlsCost,
            SDL_FRect{rightPanel.x + 194.0f, rightPanel.y + 310.0f, 140.0f, 26.0f});

        const SDL_FRect upgradeButton = SDL_FRect{rightPanel.x + 28.0f, rightPanel.y + 342.0f, rightPanel.w - 56.0f, 34.0f};
        rc2d_graphics_setColor(isPointInRect(mouseX, mouseY, upgradeButton) ? kButtonHoverFill : kButtonFill);
        rc2d_graphics_rectangle("fill", &upgradeButton);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &upgradeButton);
        drawCentered(&self->bodyFont, "Mettre a niveau le mortier actuel", upgradeButton, kTextGold);
    }

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &treasuryPanel);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &treasuryPanel);
    drawText(&self->titleFont, "Tresorerie de la guild", treasuryPanel.x + 14.0f, treasuryPanel.y + 10.0f, kTextGold);
    auto drawTreasuryCurrency = [&](const RC2D_Image& icon, std::int64_t amount, float x) {
        const SDL_FRect iconRect = SDL_FRect{x, treasuryPanel.y + 9.0f, 26.0f, 26.0f};
        drawImageFit(icon, iconRect, 1.0f);
        drawText(&self->bodyFont, formatWithDots(amount), iconRect.x + 34.0f, iconRect.y + 4.0f, kTextBody);
    };
    drawTreasuryCurrency(self->goldIcon, self->treasuryGoldAmount, treasuryPanel.x + 200.0f);
    drawTreasuryCurrency(self->rubiesIcon, self->treasuryRubiesAmount, treasuryPanel.x + 340.0f);
    drawTreasuryCurrency(self->pearlsIcon, self->treasuryPearlsAmount, treasuryPanel.x + 480.0f);
    drawTreasuryCurrency(self->crystalsIcon, self->treasuryCrystalsAmount, treasuryPanel.x + 620.0f);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void GuildMortarWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
    this->transferGoldInputFocused = false;
    this->transferGoldSelectingWithMouse = false;
    this->clearTransferGoldSelection();
}

void GuildMortarWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->levelScrollDragging = false;
    this->transferGoldInputFocused = false;
    this->transferGoldSelectingWithMouse = false;
    this->clearTransferGoldSelection();
}

bool GuildMortarWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType GuildMortarWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
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

    const SDL_FRect baseRect = getGuildMortarRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const SDL_FRect headerRect = SDL_FRect{currentRect.x + 5.0f, currentRect.y + 5.0f, currentRect.w - 10.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{currentRect.x + currentRect.w - 28.0f, headerRect.y + 5.0f, 20.0f, 20.0f};
    const SDL_FRect toggleButtonRect = SDL_FRect{currentRect.x + 34.0f, currentRect.y + 122.0f, 320.0f, 34.0f};
    const SDL_FRect upgradeButtonRect = SDL_FRect{currentRect.x + 416.0f, currentRect.y + 388.0f, 304.0f, 34.0f};
    const SDL_FRect transferInputRect = SDL_FRect{currentRect.x + 34.0f, currentRect.y + 402.0f, 190.0f, 30.0f};
    const SDL_FRect transferButtonRect = SDL_FRect{currentRect.x + 232.0f, currentRect.y + 402.0f, 122.0f, 30.0f};
    const bool hasNextMortarLevel =
        this->selectedMortarLevelIndex >= 0 &&
        (this->selectedMortarLevelIndex + 1) < static_cast<int>(this->mortarLevels.size());

    if (this->levelScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (isPointInRect(x, y, transferInputRect))
    {
        return HudCursorType::TEXT;
    }
    if (isPointInRect(x, y, closeButtonRect) ||
        isPointInRect(x, y, toggleButtonRect) ||
        (hasNextMortarLevel && isPointInRect(x, y, upgradeButtonRect)) ||
        isPointInRect(x, y, transferButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (isPointInRect(x, y, headerRect))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

void GuildMortarWidget::addMortarLevel(const MortarLevelEntry& entry)
{
    MortarLevelEntry sanitized = entry;
    sanitized.damage = (std::max)(0, sanitized.damage);
    sanitized.attackSpeedSec = (std::max)(0.0f, sanitized.attackSpeedSec);
    sanitized.attackRange = (std::max)(0, sanitized.attackRange);
    sanitized.shotCostGold = (std::max)(static_cast<std::int64_t>(0), sanitized.shotCostGold);
    sanitized.unitChestGoldAmount = (std::max)(static_cast<std::int64_t>(0), sanitized.unitChestGoldAmount);
    sanitized.upgradeRubiesCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradeRubiesCost);
    sanitized.upgradePearlsCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradePearlsCost);
    this->mortarLevels.push_back(sanitized);
    if (this->selectedMortarLevelIndex < 0)
    {
        this->selectedMortarLevelIndex = 0;
    }
}

void GuildMortarWidget::setMortarLevels(const std::vector<MortarLevelEntry>& entries)
{
    this->mortarLevels.clear();
    for (const MortarLevelEntry& entry : entries)
    {
        this->addMortarLevel(entry);
    }
    if (this->mortarLevels.empty())
    {
        this->selectedMortarLevelIndex = -1;
    }
}

void GuildMortarWidget::clearMortarLevels(void)
{
    this->mortarLevels.clear();
    this->selectedMortarLevelIndex = -1;
}

void GuildMortarWidget::setSelectedMortarLevelIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->mortarLevels.size()))
    {
        return;
    }
    this->selectedMortarLevelIndex = index;
}

void GuildMortarWidget::setMortarEnabled(bool enabled)
{
    this->mortarEnabled = enabled;
}

void GuildMortarWidget::updateMortarChestGoldAmount(int index, std::int64_t amount)
{
    if (index < 0 || index >= static_cast<int>(this->mortarLevels.size()))
    {
        return;
    }
    this->mortarLevels[static_cast<std::size_t>(index)].unitChestGoldAmount =
        (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildMortarWidget::updateMortarChestGoldAmount(const std::string& name, std::int64_t amount)
{
    for (MortarLevelEntry& entry : this->mortarLevels)
    {
        if (entry.name == name)
        {
            entry.unitChestGoldAmount = (std::max)(static_cast<std::int64_t>(0), amount);
            return;
        }
    }
}

void GuildMortarWidget::setTreasuryGoldAmount(std::int64_t amount)
{
    this->treasuryGoldAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildMortarWidget::setTreasuryRubiesAmount(std::int64_t amount)
{
    this->treasuryRubiesAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildMortarWidget::setTreasuryPearlsAmount(std::int64_t amount)
{
    this->treasuryPearlsAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildMortarWidget::setTreasuryCrystalsAmount(std::int64_t amount)
{
    this->treasuryCrystalsAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

GuildMortarWidget::MortarLevelEntry GuildMortarWidget::getSelectedDisplayEntry(void) const
{
    if (this->selectedMortarLevelIndex >= 0 &&
        this->selectedMortarLevelIndex < static_cast<int>(this->mortarLevels.size()))
    {
        return this->mortarLevels[static_cast<std::size_t>(this->selectedMortarLevelIndex)];
    }

    return MortarLevelEntry{};
}

std::size_t GuildMortarWidget::getTransferGoldCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const
{
    const float inputTextX = inputRect.x + 8.0f;
    const float localX = (std::max)(0.0f, renderX - inputTextX);
    std::size_t newCursor = 0U;
    for (std::size_t i = 0U; i <= this->transferGoldInput.size(); ++i)
    {
        const float width = measureTextWidth(
            const_cast<RC2D_Font*>(&this->bodyFont),
            this->transferGoldInput.substr(0U, i));
        if (localX <= width)
        {
            newCursor = i;
            break;
        }
        newCursor = i;
    }
    return (std::min)(newCursor, this->transferGoldInput.size());
}

bool GuildMortarWidget::hasTransferGoldSelection(void) const
{
    return this->getTransferGoldSelectionStart() != this->getTransferGoldSelectionEnd();
}

std::size_t GuildMortarWidget::getTransferGoldSelectionStart(void) const
{
    const std::size_t clampedCursor = (std::min)(this->transferGoldCursorIndex, this->transferGoldInput.size());
    const std::size_t clampedAnchor = (std::min)(this->transferGoldSelectionAnchorIndex, this->transferGoldInput.size());
    return (std::min)(clampedAnchor, clampedCursor);
}

std::size_t GuildMortarWidget::getTransferGoldSelectionEnd(void) const
{
    const std::size_t clampedCursor = (std::min)(this->transferGoldCursorIndex, this->transferGoldInput.size());
    const std::size_t clampedAnchor = (std::min)(this->transferGoldSelectionAnchorIndex, this->transferGoldInput.size());
    return (std::max)(clampedAnchor, clampedCursor);
}

void GuildMortarWidget::clearTransferGoldSelection(void)
{
    this->transferGoldSelectionAnchorIndex = (std::min)(this->transferGoldCursorIndex, this->transferGoldInput.size());
}

void GuildMortarWidget::deleteSelectedTransferGoldText(void)
{
    if (!this->hasTransferGoldSelection())
    {
        return;
    }

    const std::size_t selectionStart = this->getTransferGoldSelectionStart();
    const std::size_t selectionEnd = this->getTransferGoldSelectionEnd();
    this->transferGoldInput.erase(selectionStart, selectionEnd - selectionStart);
    this->transferGoldCursorIndex = selectionStart;
    this->clearTransferGoldSelection();
}
