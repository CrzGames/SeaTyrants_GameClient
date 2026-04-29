#include "game/ui/hud/guild-tower-widget.h"

#include "core/context.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

static constexpr float kRefW = 980.0f;
static constexpr float kRefH = 1040.0f;
static constexpr float kTowerRowHeight = 64.0f;
static constexpr int kTowerVisibleRows = 12;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;

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
static constexpr RC2D_Color kSelectedListFill = RC2D_Color{92, 57, 16, 225};
static constexpr RC2D_Color kHoverListFill = RC2D_Color{50, 35, 20, 220};
static constexpr RC2D_Color kHpBarBack = RC2D_Color{22, 27, 34, 235};
static constexpr RC2D_Color kHpBarFill = RC2D_Color{38, 132, 64, 245};
static constexpr RC2D_Color kHpBarLine = RC2D_Color{88, 196, 108, 255};

struct GuildTowerLayout {
    SDL_FRect outer;
    SDL_FRect inner;
    SDL_FRect header;
    SDL_FRect closeButtonRect;
    SDL_FRect leftPanel;
    SDL_FRect rightPanel;
    SDL_FRect bottomPanel;
    SDL_FRect listViewport;
    SDL_FRect listScrollTrackRect;
    SDL_FRect repairAllButtonRect;
};

struct CurrencyCostVisual {
    const RC2D_Image* icon;
    std::int64_t amount;
};

static SDL_FRect getGuildTowerRectFromGameScreen(void)
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

static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
}

static SDL_Rect toClipRect(const SDL_FRect& rect)
{
    SDL_Rect clipRect{};
    clipRect.x = static_cast<int>(std::floor(rect.x));
    clipRect.y = static_cast<int>(std::floor(rect.y));
    const int clipRight = static_cast<int>(std::ceil(rect.x + rect.w));
    const int clipBottom = static_cast<int>(std::ceil(rect.y + rect.h));
    clipRect.w = (std::max)(clipRight - clipRect.x, 1);
    clipRect.h = (std::max)(clipBottom - clipRect.y, 1);
    return clipRect;
}

static GuildTowerLayout getGuildTowerLayout(const SDL_FRect& outer)
{
    GuildTowerLayout layout{};
    layout.outer = outer;
    layout.inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    layout.header = SDL_FRect{layout.inner.x + 1.0f, layout.inner.y + 1.0f, layout.inner.w - 2.0f, 30.0f};
    layout.closeButtonRect = SDL_FRect{outer.x + outer.w - 28.0f, layout.header.y + 5.0f, 20.0f, 20.0f};
    layout.leftPanel = SDL_FRect{outer.x + 12.0f, outer.y + 46.0f, 360.0f, 910.0f};
    layout.rightPanel = SDL_FRect{outer.x + 388.0f, outer.y + 46.0f, 580.0f, 910.0f};
    layout.bottomPanel = SDL_FRect{outer.x + 12.0f, outer.y + 970.0f, 956.0f, 44.0f};
    layout.listViewport = SDL_FRect{
        layout.leftPanel.x + 14.0f,
        layout.leftPanel.y + 52.0f,
        layout.leftPanel.w - 28.0f,
        (static_cast<float>(kTowerVisibleRows) * kTowerRowHeight) + 8.0f
    };
    layout.listScrollTrackRect = SDL_FRect{
        layout.listViewport.x + layout.listViewport.w - (kScrollBarWidth + kScrollBarPadding),
        layout.listViewport.y + kScrollBarPadding,
        kScrollBarWidth,
        layout.listViewport.h - (kScrollBarPadding * 2.0f)
    };
    layout.repairAllButtonRect = SDL_FRect{
        layout.leftPanel.x + 16.0f,
        layout.leftPanel.y + layout.leftPanel.h - 52.0f,
        layout.leftPanel.w - 32.0f,
        34.0f
    };
    return layout;
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

static std::string formatTowerDisplayName(int towerNumber, const GuildTowerWidget::TowerLevelEntry& entry)
{
    const std::string towerPrefix =
        towerNumber > 0 ? ("Tower " + std::to_string(towerNumber) + " - ") : std::string();

    if (entry.name.empty())
    {
        return towerPrefix + "lvl" + std::to_string((std::max)(1, entry.level));
    }

    return towerPrefix + entry.name + " - lvl" + std::to_string((std::max)(1, entry.level));
}

static bool hasUpgradeableLevel(const GuildTowerWidget::TowerLevelEntry& entry)
{
    return entry.level > 0 && entry.level < 4;
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

GuildTowerWidget::GuildTowerWidget(void)
    : towerLevels{},
      selectedTowerLevelIndex(-1),
      treasuryGoldAmount(0),
      treasuryRubiesAmount(0),
      treasuryPearlsAmount(0),
      treasuryCrystalsAmount(0),
      onUpgradeRequested{},
      onRepairAllRequested{},
      titleFont{},
      bodyFont{},
      valueFont{},
      goldIcon{},
      rubiesIcon{},
      pearlsIcon{},
      crystalsIcon{},
      controlIcons{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      towerListFirstRow(0),
      towerListScrollDragging(false),
      towerListScrollDragOffsetY(0.0f)
{
}

GuildTowerWidget::~GuildTowerWidget(void)
{
}

void GuildTowerWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 18.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->valueFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 17.0f);
    this->goldIcon = LoadStorageImage("assets/images/ui-scene-game/money-gold.png", RC2D_STORAGE_TITLE);
    this->rubiesIcon = LoadStorageImage("assets/images/ui-scene-game/money-rubies.png", RC2D_STORAGE_TITLE);
    this->pearlsIcon = LoadStorageImage("assets/images/ui-scene-game/money_pearls.png", RC2D_STORAGE_TITLE);
    this->crystalsIcon = LoadStorageImage("assets/images/ui-scene-game/money_crystals.png", RC2D_STORAGE_TITLE);
    this->controlIcons.load();

    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = baseRect;
    this->visible = true;
    this->widgetDragging = false;
    this->towerListFirstRow = 0;
    this->towerListScrollDragging = false;
    this->towerListScrollDragOffsetY = 0.0f;
    this->clampCurrentHpToSelectedLevel();
}

void GuildTowerWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageImageRef(&this->crystalsIcon);
    ResetStorageImageRef(&this->pearlsIcon);
    ResetStorageImageRef(&this->rubiesIcon);
    ResetStorageImageRef(&this->goldIcon);
    ResetStorageFontRef(&this->valueFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void GuildTowerWidget::update(double dt)
{
    (void)dt;

    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!this->visible || (!this->widgetDragging && !this->towerListScrollDragging))
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        this->towerListScrollDragging = false;
        return;
    }

    if (this->towerListScrollDragging)
    {
        const int totalRows = static_cast<int>(this->towerLevels.size());
        const int maxFirstRow = (std::max)(0, totalRows - kTowerVisibleRows);
        if (maxFirstRow <= 0)
        {
            this->towerListFirstRow = 0;
            this->towerListScrollDragging = false;
            return;
        }

        const GuildTowerLayout layout = getGuildTowerLayout(this->widgetRect);
        const float thumbHeight = (std::min)(
            layout.listScrollTrackRect.h,
            (std::max)(
                kMinThumbHeight,
                layout.listScrollTrackRect.h *
                    static_cast<float>(kTowerVisibleRows) /
                    static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, layout.listScrollTrackRect.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;
        const float thumbTop = clampf(
            mouseY - this->towerListScrollDragOffsetY,
            layout.listScrollTrackRect.y,
            layout.listScrollTrackRect.y + thumbTravel);
        const float ratio = (thumbTop - layout.listScrollTrackRect.y) / thumbTravel;
        this->towerListFirstRow = static_cast<int>(ratio * static_cast<float>(maxFirstRow) + 0.5f);
        this->towerListFirstRow = (std::max)(0, (std::min)(this->towerListFirstRow, maxFirstRow));
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

bool GuildTowerWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
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

    const GuildTowerLayout layout = getGuildTowerLayout(this->widgetRect);
    const TowerLevelEntry selected = this->getSelectedDisplayEntry();
    const bool hasNextTowerLevel = hasUpgradeableLevel(selected);
    const SDL_FRect upgradeButtonRect = SDL_FRect{
        layout.rightPanel.x + 26.0f,
        layout.rightPanel.y + layout.rightPanel.h - 50.0f,
        layout.rightPanel.w - 52.0f,
        34.0f
    };

    const int totalRows = static_cast<int>(this->towerLevels.size());
    const int maxFirstRow = (std::max)(0, totalRows - kTowerVisibleRows);
    const bool showListScrollbar = maxFirstRow > 0;
    const float listRowWidth = layout.listViewport.w - (showListScrollbar ? 20.0f : 8.0f);
    const float thumbHeight = (std::min)(
        layout.listScrollTrackRect.h,
        (std::max)(
            kMinThumbHeight,
            layout.listScrollTrackRect.h *
                static_cast<float>(kTowerVisibleRows) /
                static_cast<float>((std::max)(1, totalRows))));
    const float thumbTravel = (std::max)(1.0f, layout.listScrollTrackRect.h - thumbHeight);
    const float scrollRatio =
        maxFirstRow > 0 ? static_cast<float>(this->towerListFirstRow) / static_cast<float>(maxFirstRow) : 0.0f;
    const SDL_FRect listThumbRect = SDL_FRect{
        layout.listScrollTrackRect.x,
        layout.listScrollTrackRect.y + (thumbTravel * scrollRatio),
        layout.listScrollTrackRect.w,
        thumbHeight
    };

    if (isPointInRect(x, y, layout.closeButtonRect))
    {
        this->hide();
        return true;
    }
    if (isPointInRect(x, y, layout.repairAllButtonRect))
    {
        if (this->onRepairAllRequested)
        {
            this->onRepairAllRequested();
        }
        else
        {
            this->repairAllDamagedTowers();
        }
        return true;
    }
    if (hasNextTowerLevel && isPointInRect(x, y, upgradeButtonRect))
    {
        if (this->onUpgradeRequested)
        {
            TowerUpgradeRequest request{};
            request.currentLevelIndex = this->selectedTowerLevelIndex;
            request.targetLevelIndex = selected.level + 1;
            request.currentLevel = selected;
            request.targetLevel = this->getUpgradePreviewEntry();
            this->onUpgradeRequested(request);
        }
        return true;
    }
    if (showListScrollbar && isPointInRect(x, y, listThumbRect))
    {
        this->towerListScrollDragging = true;
        this->towerListScrollDragOffsetY = y - listThumbRect.y;
        this->widgetDragging = false;
        return true;
    }
    if (showListScrollbar && isPointInRect(x, y, layout.listScrollTrackRect))
    {
        if (y < listThumbRect.y)
        {
            this->towerListFirstRow = (std::max)(0, this->towerListFirstRow - kTowerVisibleRows);
        }
        else if (y > (listThumbRect.y + listThumbRect.h))
        {
            this->towerListFirstRow = (std::min)(maxFirstRow, this->towerListFirstRow + kTowerVisibleRows);
        }
        return true;
    }

    const int visibleCount = (std::min)(kTowerVisibleRows, totalRows - this->towerListFirstRow);
    for (int row = 0; row < visibleCount; ++row)
    {
        const int towerIndex = this->towerListFirstRow + row;
        const SDL_FRect rowRect = SDL_FRect{
            layout.listViewport.x + 4.0f,
            layout.listViewport.y + 4.0f + (static_cast<float>(row) * kTowerRowHeight),
            listRowWidth,
            kTowerRowHeight - 6.0f
        };
        if (isPointInRect(x, y, rowRect))
        {
            this->setSelectedTowerLevelIndex(towerIndex);
            this->widgetDragging = false;
            this->towerListScrollDragging = false;
            return true;
        }
    }

    if (isPointInRect(x, y, layout.header))
    {
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->towerListScrollDragging = false;
        return true;
    }
    return true;
}

bool GuildTowerWidget::mousewheelmoved(
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
    (void)mouseID;

    if (!this->visible)
    {
        return false;
    }

    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const GuildTowerLayout layout = getGuildTowerLayout(this->widgetRect);
    if (!isPointInRect(mouse_x, mouse_y, layout.listViewport))
    {
        return false;
    }

    const int maxFirstRow = (std::max)(0, static_cast<int>(this->towerLevels.size()) - kTowerVisibleRows);
    if (maxFirstRow <= 0)
    {
        this->towerListFirstRow = 0;
        return false;
    }

    if (integer_y > 0)
    {
        this->towerListFirstRow = (std::max)(0, this->towerListFirstRow - 1);
    }
    else if (integer_y < 0)
    {
        this->towerListFirstRow = (std::min)(maxFirstRow, this->towerListFirstRow + 1);
    }

    return true;
}

bool GuildTowerWidget::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;
    (void)scancode;
    (void)keycode;
    (void)mod;
    (void)isrepeat;
    return false;
}

void GuildTowerWidget::draw(void) const
{
    GuildTowerWidget* self = const_cast<GuildTowerWidget*>(this);
    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
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

    const GuildTowerLayout layout = getGuildTowerLayout(self->widgetRect);
    const SDL_FRect outer = layout.outer;

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    const TowerLevelEntry selected = self->getSelectedDisplayEntry();
    const bool hasNextTowerLevel = hasUpgradeableLevel(selected);
    const TowerLevelEntry upgrade = self->getUpgradePreviewEntry();

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &layout.inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.header);
    drawText(&self->titleFont, "Guild Tower", layout.header.x + 10.0f, layout.header.y + 5.0f, kTextGold);
    self->controlIcons.drawCloseButton(layout.closeButtonRect, kHeaderFill, kGold);

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
    drawPanel(layout.leftPanel, "Liste des Towers");
    drawPanel(layout.rightPanel, "Tower selectionnee");

    auto drawRepairLine = [&](const SDL_FRect& area, const std::string& label, const RC2D_Image& icon, const std::string& value) {
        drawCentered(&self->bodyFont, label, SDL_FRect{area.x, area.y, area.w, 18.0f}, kTextMuted);
        const float valueWidth = measureTextWidth(&self->valueFont, value);
        const float contentWidth = 24.0f + 8.0f + valueWidth;
        const float startX = area.x + (std::max)(0.0f, (area.w - contentWidth) * 0.5f);
        const SDL_FRect iconRect = SDL_FRect{startX, area.y + 22.0f, 24.0f, 24.0f};
        drawImageFit(icon, iconRect, 1.0f);
        drawText(&self->valueFont, value, iconRect.x + 32.0f, iconRect.y + 2.0f, kTextGold);
    };

    auto drawHpBlock = [&](const SDL_FRect& panel, const TowerLevelEntry& entry, std::int64_t hpValue) {
        const std::int64_t maxHp = (std::max)(static_cast<std::int64_t>(0), entry.maxHp);
        const std::int64_t clampedHp = (std::max)(static_cast<std::int64_t>(0), (std::min)(hpValue, maxHp));
        const SDL_FRect hpRect = SDL_FRect{panel.x + 20.0f, panel.y + 74.0f, panel.w - 40.0f, 26.0f};
        const float fillRatio =
            maxHp > 0 ? static_cast<float>(clampedHp) / static_cast<float>(maxHp) : 0.0f;
        const SDL_FRect fillRect = SDL_FRect{hpRect.x + 2.0f, hpRect.y + 2.0f, (hpRect.w - 4.0f) * fillRatio, hpRect.h - 4.0f};

        rc2d_graphics_setColor(kHpBarBack);
        rc2d_graphics_rectangle("fill", &hpRect);
        rc2d_graphics_setColor(kHpBarFill);
        rc2d_graphics_rectangle("fill", &fillRect);
        rc2d_graphics_setColor(kHpBarLine);
        rc2d_graphics_rectangle("line", &hpRect);
        drawCentered(
            &self->bodyFont,
            formatWithDots(clampedHp) + " / " + formatWithDots(maxHp),
            hpRect,
            kTextBody);
    };

    const float rightContentMargin = 28.0f;
    const float rightColumnGap = 24.0f;
    const float rightColumnWidth =
        (layout.rightPanel.w - (rightContentMargin * 2.0f) - rightColumnGap) * 0.5f;

    rc2d_graphics_setColor(kHpBarBack);
    rc2d_graphics_rectangle("fill", &layout.listViewport);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.listViewport);

    const int totalRows = static_cast<int>(self->towerLevels.size());
    const int maxFirstRow = (std::max)(0, totalRows - kTowerVisibleRows);
    const bool showListScrollbar = maxFirstRow > 0;
    const float listRowWidth = layout.listViewport.w - (showListScrollbar ? 20.0f : 8.0f);
    self->towerListFirstRow = (std::max)(0, (std::min)(self->towerListFirstRow, maxFirstRow));
    const int visibleCount = (std::min)(kTowerVisibleRows, totalRows - self->towerListFirstRow);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        const SDL_Rect clipRect = toClipRect(layout.listViewport);
        SDL_SetRenderClipRect(renderer, &clipRect);
    }

    for (int row = 0; row < visibleCount; ++row)
    {
        const int towerIndex = self->towerListFirstRow + row;
        const TowerLevelEntry& tower = self->towerLevels[static_cast<std::size_t>(towerIndex)];
        const SDL_FRect rowRect = SDL_FRect{
            layout.listViewport.x + 4.0f,
            layout.listViewport.y + 4.0f + (static_cast<float>(row) * kTowerRowHeight),
            listRowWidth,
            kTowerRowHeight - 6.0f
        };
        const bool isSelected = towerIndex == self->selectedTowerLevelIndex;
        const bool isHovered = isPointInRect(mouseX, mouseY, rowRect);
        const std::int64_t clampedHp = (std::max)(
            static_cast<std::int64_t>(0),
            (std::min)(tower.currentHp, (std::max)(static_cast<std::int64_t>(0), tower.maxHp)));
        const int hpPercent =
            tower.maxHp > 0 ? static_cast<int>((clampedHp * 100) / tower.maxHp) : 0;

        rc2d_graphics_setColor(isSelected ? kSelectedListFill : (isHovered ? kHoverListFill : kPanelSoftFill));
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &rowRect);

        drawText(
            &self->bodyFont,
            formatTowerDisplayName(towerIndex + 1, tower),
            rowRect.x + 10.0f,
            rowRect.y + 8.0f,
            isSelected ? kTextGold : kTextBody);
        drawText(
            &self->bodyFont,
            "HP " + formatWithDots(clampedHp) + " / " + formatWithDots(tower.maxHp) + "  (" + formatWithDots(hpPercent) + "%)",
            rowRect.x + 10.0f,
            rowRect.y + 36.0f,
            kTextMuted);
    }

    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }

    if (showListScrollbar)
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.listScrollTrackRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.listScrollTrackRect);
        const float thumbHeight = (std::min)(
            layout.listScrollTrackRect.h,
            (std::max)(
                kMinThumbHeight,
                layout.listScrollTrackRect.h *
                    static_cast<float>(kTowerVisibleRows) /
                    static_cast<float>((std::max)(1, totalRows))));
        const float thumbTravel = (std::max)(1.0f, layout.listScrollTrackRect.h - thumbHeight);
        const float scrollRatio =
            maxFirstRow > 0 ? static_cast<float>(self->towerListFirstRow) / static_cast<float>(maxFirstRow) : 0.0f;
        const SDL_FRect thumbRect = SDL_FRect{
            layout.listScrollTrackRect.x,
            layout.listScrollTrackRect.y + (thumbTravel * scrollRatio),
            layout.listScrollTrackRect.w,
            thumbHeight
        };
        rc2d_graphics_setColor(
            (self->towerListScrollDragging || isPointInRect(mouseX, mouseY, thumbRect)) ? kButtonHoverFill : kButtonFill);
        rc2d_graphics_rectangle("fill", &thumbRect);
    }

    rc2d_graphics_setColor(isPointInRect(mouseX, mouseY, layout.repairAllButtonRect) ? kButtonHoverFill : kButtonFill);
    rc2d_graphics_rectangle("fill", &layout.repairAllButtonRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.repairAllButtonRect);
    drawCentered(&self->bodyFont, "Reparer toute les tower endommagees", layout.repairAllButtonRect, kTextGold);

    drawCentered(
        &self->valueFont,
        formatTowerDisplayName(self->selectedTowerLevelIndex + 1, selected),
        SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 42.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 24.0f},
        kTextGold);
    drawHpBlock(layout.rightPanel, selected, selected.currentHp);
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Degats d'attaque",
        formatWithDots(selected.damage),
        SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 116.0f, rightColumnWidth, 48.0f});
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Sante maximale",
        formatWithDots(selected.maxHp),
        SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 116.0f, rightColumnWidth, 48.0f});
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Vitesse d'attaque en seconde",
        formatSeconds(selected.attackSpeedSec),
        SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 180.0f, rightColumnWidth, 48.0f});
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Portee d'attaque",
        formatWithDots(selected.attackRange),
        SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 180.0f, rightColumnWidth, 48.0f});
    drawRepairLine(
        SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 248.0f, rightColumnWidth, 46.0f},
        "Prix de la reparation",
        self->goldIcon,
        formatWithDots(selected.repairPriceGold));
    drawStatBlock(
        &self->bodyFont,
        &self->valueFont,
        "Montant de la reparation",
        formatWithDots(selected.repairAmountPer5Sec) + "/5s",
        SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 248.0f, rightColumnWidth, 48.0f});

    rc2d_graphics_setColor(kGold);
    const SDL_FRect divider = SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 326.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 1.0f};
    rc2d_graphics_rectangle("fill", &divider);
    drawCentered(
        &self->bodyFont,
        "Prochaine amelioration",
        SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 338.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 18.0f},
        kTextMuted);

    if (!hasNextTowerLevel)
    {
        drawCentered(
            &self->valueFont,
            "Tower deja au niveau maximum",
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 376.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 140.0f},
            kTextGold);
    }
    else
    {
        drawCentered(
            &self->valueFont,
            formatTowerDisplayName(self->selectedTowerLevelIndex + 1, upgrade),
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 372.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 24.0f},
            kTextGold);
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Degats d'attaque",
            formatWithDots(upgrade.damage),
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 410.0f, rightColumnWidth, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Sante maximale",
            formatWithDots(upgrade.maxHp),
            SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 410.0f, rightColumnWidth, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Vitesse d'attaque",
            formatSeconds(upgrade.attackSpeedSec),
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 474.0f, rightColumnWidth, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Portee d'attaque",
            formatWithDots(upgrade.attackRange),
            SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 474.0f, rightColumnWidth, 48.0f});
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Duree de construction",
            upgrade.constructionDurationMinutes > 0
                ? (formatWithDots(upgrade.constructionDurationMinutes) + " minutes")
                : std::string("-"),
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 538.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 48.0f});
        drawRepairLine(
            SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 604.0f, rightColumnWidth, 46.0f},
            "Prix de la reparation",
            self->goldIcon,
            formatWithDots(upgrade.repairPriceGold));
        drawStatBlock(
            &self->bodyFont,
            &self->valueFont,
            "Montant de la reparation",
            formatWithDots(upgrade.repairAmountPer5Sec) + "/5s",
            SDL_FRect{layout.rightPanel.x + rightContentMargin + rightColumnWidth + rightColumnGap, layout.rightPanel.y + 604.0f, rightColumnWidth, 48.0f});

        std::vector<CurrencyCostVisual> costs;
        if (upgrade.upgradeGoldCost > 0)
        {
            costs.push_back(CurrencyCostVisual{&self->goldIcon, upgrade.upgradeGoldCost});
        }
        if (upgrade.upgradeRubiesCost > 0)
        {
            costs.push_back(CurrencyCostVisual{&self->rubiesIcon, upgrade.upgradeRubiesCost});
        }
        if (upgrade.upgradePearlsCost > 0)
        {
            costs.push_back(CurrencyCostVisual{&self->pearlsIcon, upgrade.upgradePearlsCost});
        }
        if (upgrade.upgradeCrystalsCost > 0)
        {
            costs.push_back(CurrencyCostVisual{&self->crystalsIcon, upgrade.upgradeCrystalsCost});
        }

        if (!costs.empty())
        {
            drawCentered(
                &self->bodyFont,
                "Cout pour ameliorer",
                SDL_FRect{layout.rightPanel.x + rightContentMargin, layout.rightPanel.y + 676.0f, layout.rightPanel.w - (rightContentMargin * 2.0f), 18.0f},
                kTextMuted);
            const int safeColumns = (std::max)(1, static_cast<int>(costs.size()));
            const float slotWidth =
                (layout.rightPanel.w - (rightContentMargin * 2.0f) - (static_cast<float>(safeColumns - 1) * 10.0f)) / static_cast<float>(safeColumns);
            for (std::size_t i = 0; i < costs.size(); ++i)
            {
                const int column = static_cast<int>(i);
                const SDL_FRect area = SDL_FRect{
                    layout.rightPanel.x + rightContentMargin + (static_cast<float>(column) * (slotWidth + 10.0f)),
                    layout.rightPanel.y + 704.0f,
                    slotWidth,
                    24.0f
                };
                const std::string amountText = formatWithDots(costs[i].amount);
                const float valueWidth = measureTextWidth(&self->valueFont, amountText);
                const float contentWidth = 22.0f + 8.0f + valueWidth;
                const float startX = area.x + (std::max)(0.0f, (area.w - contentWidth) * 0.5f);
                const SDL_FRect iconRect = SDL_FRect{startX, area.y + 2.0f, 22.0f, 22.0f};
                drawImageFit(*costs[i].icon, iconRect, 1.0f);
                drawText(&self->valueFont, amountText, startX + 30.0f, area.y + 1.0f, kTextGold);
            }
        }

        const SDL_FRect upgradeButton = SDL_FRect{
            layout.rightPanel.x + 26.0f,
            layout.rightPanel.y + layout.rightPanel.h - 50.0f,
            layout.rightPanel.w - 52.0f,
            34.0f
        };
        rc2d_graphics_setColor(isPointInRect(mouseX, mouseY, upgradeButton) ? kButtonHoverFill : kButtonFill);
        rc2d_graphics_rectangle("fill", &upgradeButton);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &upgradeButton);
        drawCentered(&self->bodyFont, "Mettre a niveau la tower selectionnee", upgradeButton, kTextGold);
    }

    const SDL_FRect treasuryPanel = layout.bottomPanel;
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &treasuryPanel);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &treasuryPanel);

    auto drawTreasuryCurrency = [&](const RC2D_Image& icon, std::int64_t amount, const SDL_FRect& area) {
        const std::string amountText = formatWithDots(amount);
        const float valueWidth = measureTextWidth(&self->bodyFont, amountText);
        const float contentWidth = 26.0f + 8.0f + valueWidth;
        const float startX = area.x + (std::max)(0.0f, (area.w - contentWidth) * 0.5f);
        const SDL_FRect iconRect = SDL_FRect{startX, treasuryPanel.y + 9.0f, 26.0f, 26.0f};
        drawImageFit(icon, iconRect, 1.0f);
        drawText(&self->bodyFont, amountText, iconRect.x + 34.0f, iconRect.y + 4.0f, kTextBody);
    };
    drawText(&self->titleFont, "Tresorerie de la guild", treasuryPanel.x + 14.0f, treasuryPanel.y + 10.0f, kTextGold);
    const float treasuryStartX = treasuryPanel.x + 190.0f;
    const float treasuryAvailableWidth = treasuryPanel.w - 204.0f;
    const float treasuryGap = 8.0f;
    const float treasurySlotWidth = (treasuryAvailableWidth - (treasuryGap * 3.0f)) / 4.0f;
    drawTreasuryCurrency(self->goldIcon, self->treasuryGoldAmount, SDL_FRect{treasuryStartX, treasuryPanel.y, treasurySlotWidth, treasuryPanel.h});
    drawTreasuryCurrency(self->rubiesIcon, self->treasuryRubiesAmount, SDL_FRect{treasuryStartX + treasurySlotWidth + treasuryGap, treasuryPanel.y, treasurySlotWidth, treasuryPanel.h});
    drawTreasuryCurrency(self->pearlsIcon, self->treasuryPearlsAmount, SDL_FRect{treasuryStartX + ((treasurySlotWidth + treasuryGap) * 2.0f), treasuryPanel.y, treasurySlotWidth, treasuryPanel.h});
    drawTreasuryCurrency(self->crystalsIcon, self->treasuryCrystalsAmount, SDL_FRect{treasuryStartX + ((treasurySlotWidth + treasuryGap) * 3.0f), treasuryPanel.y, treasurySlotWidth, treasuryPanel.h});

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void GuildTowerWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
    this->towerListScrollDragging = false;
}

void GuildTowerWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->towerListScrollDragging = false;
}

bool GuildTowerWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType GuildTowerWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }
    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }
    if (this->towerListScrollDragging)
    {
        return HudCursorType::POINTER;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const SDL_FRect baseRect = getGuildTowerRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const GuildTowerLayout layout = getGuildTowerLayout(currentRect);
    const TowerLevelEntry selected = this->getSelectedDisplayEntry();
    const bool hasNextTowerLevel = hasUpgradeableLevel(selected);
    const int totalRows = static_cast<int>(this->towerLevels.size());
    const int maxFirstRow = (std::max)(0, totalRows - kTowerVisibleRows);
    const bool showListScrollbar = maxFirstRow > 0;
    const float listRowWidth = layout.listViewport.w - (showListScrollbar ? 20.0f : 8.0f);
    const SDL_FRect upgradeButtonRect = SDL_FRect{
        layout.rightPanel.x + 26.0f,
        layout.rightPanel.y + layout.rightPanel.h - 50.0f,
        layout.rightPanel.w - 52.0f,
        34.0f
    };

    if (isPointInRect(x, y, layout.closeButtonRect) ||
        isPointInRect(x, y, layout.repairAllButtonRect) ||
        (hasNextTowerLevel && isPointInRect(x, y, upgradeButtonRect)))
    {
        return HudCursorType::POINTER;
    }
    if (isPointInRect(x, y, layout.header))
    {
        return HudCursorType::MOVE;
    }

    const int visibleCount = (std::min)(kTowerVisibleRows, totalRows - this->towerListFirstRow);
    for (int row = 0; row < visibleCount; ++row)
    {
        const SDL_FRect rowRect = SDL_FRect{
            layout.listViewport.x + 4.0f,
            layout.listViewport.y + 4.0f + (static_cast<float>(row) * kTowerRowHeight),
            listRowWidth,
            kTowerRowHeight - 6.0f
        };
        if (isPointInRect(x, y, rowRect) || (showListScrollbar && isPointInRect(x, y, layout.listScrollTrackRect)))
        {
            return HudCursorType::POINTER;
        }
    }

    return HudCursorType::DEFAULT;
}

void GuildTowerWidget::addTowerLevel(const TowerLevelEntry& entry)
{
    TowerLevelEntry sanitized = entry;
    sanitized.level = (std::max)(1, sanitized.level);
    sanitized.damage = (std::max)(0, sanitized.damage);
    sanitized.attackSpeedSec = (std::max)(0.0f, sanitized.attackSpeedSec);
    sanitized.attackRange = (std::max)(0, sanitized.attackRange);
    sanitized.maxHp = (std::max)(static_cast<std::int64_t>(0), sanitized.maxHp);
    sanitized.currentHp = (std::max)(static_cast<std::int64_t>(0), sanitized.currentHp);
    sanitized.constructionDurationMinutes = (std::max)(0, sanitized.constructionDurationMinutes);
    sanitized.repairPriceGold = (std::max)(static_cast<std::int64_t>(0), sanitized.repairPriceGold);
    sanitized.repairAmountPer5Sec = (std::max)(static_cast<std::int64_t>(0), sanitized.repairAmountPer5Sec);
    sanitized.upgradeGoldCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradeGoldCost);
    sanitized.upgradeRubiesCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradeRubiesCost);
    sanitized.upgradePearlsCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradePearlsCost);
    sanitized.upgradeCrystalsCost = (std::max)(static_cast<std::int64_t>(0), sanitized.upgradeCrystalsCost);
    sanitized.currentHp = (std::min)(sanitized.currentHp, sanitized.maxHp);
    this->towerLevels.push_back(sanitized);
    if (this->selectedTowerLevelIndex < 0)
    {
        this->selectedTowerLevelIndex = 0;
    }
    this->clampCurrentHpToSelectedLevel();
}

void GuildTowerWidget::setTowerLevels(const std::vector<TowerLevelEntry>& entries)
{
    this->towerLevels.clear();
    for (const TowerLevelEntry& entry : entries)
    {
        this->addTowerLevel(entry);
    }
    if (this->towerLevels.empty())
    {
        this->selectedTowerLevelIndex = -1;
        this->towerListFirstRow = 0;
    }
    this->clampCurrentHpToSelectedLevel();
}

void GuildTowerWidget::clearTowerLevels(void)
{
    this->towerLevels.clear();
    this->selectedTowerLevelIndex = -1;
    this->towerListFirstRow = 0;
}

void GuildTowerWidget::setSelectedTowerLevelIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->towerLevels.size()))
    {
        return;
    }
    this->selectedTowerLevelIndex = index;
    this->clampCurrentHpToSelectedLevel();
    if (this->selectedTowerLevelIndex < this->towerListFirstRow)
    {
        this->towerListFirstRow = this->selectedTowerLevelIndex;
    }
    else if (this->selectedTowerLevelIndex >= (this->towerListFirstRow + kTowerVisibleRows))
    {
        this->towerListFirstRow = this->selectedTowerLevelIndex - kTowerVisibleRows + 1;
    }
}

void GuildTowerWidget::setCurrentHp(std::int64_t hp)
{
    if (this->selectedTowerLevelIndex < 0 ||
        this->selectedTowerLevelIndex >= static_cast<int>(this->towerLevels.size()))
    {
        return;
    }

    this->towerLevels[static_cast<std::size_t>(this->selectedTowerLevelIndex)].currentHp =
        (std::max)(static_cast<std::int64_t>(0), hp);
    this->clampCurrentHpToSelectedLevel();
}

void GuildTowerWidget::setTreasuryGoldAmount(std::int64_t amount)
{
    this->treasuryGoldAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildTowerWidget::setTreasuryRubiesAmount(std::int64_t amount)
{
    this->treasuryRubiesAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildTowerWidget::setTreasuryPearlsAmount(std::int64_t amount)
{
    this->treasuryPearlsAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildTowerWidget::setTreasuryCrystalsAmount(std::int64_t amount)
{
    this->treasuryCrystalsAmount = (std::max)(static_cast<std::int64_t>(0), amount);
}

void GuildTowerWidget::setOnUpgradeRequested(TowerUpgradeRequestedCallback callback)
{
    this->onUpgradeRequested = std::move(callback);
}

void GuildTowerWidget::setOnRepairAllRequested(RepairAllRequestedCallback callback)
{
    this->onRepairAllRequested = std::move(callback);
}

GuildTowerWidget::TowerLevelEntry GuildTowerWidget::getSelectedDisplayEntry(void) const
{
    if (this->selectedTowerLevelIndex >= 0 &&
        this->selectedTowerLevelIndex < static_cast<int>(this->towerLevels.size()))
    {
        return this->towerLevels[static_cast<std::size_t>(this->selectedTowerLevelIndex)];
    }

    return TowerLevelEntry{};
}

GuildTowerWidget::TowerLevelEntry GuildTowerWidget::getUpgradePreviewEntry(void) const
{
    const TowerLevelEntry selected = this->getSelectedDisplayEntry();
    if (!hasUpgradeableLevel(selected))
    {
        return TowerLevelEntry{};
    }

    TowerLevelEntry preview = selected;
    preview.level = selected.level + 1;
    preview.damage = static_cast<int>(std::round((static_cast<float>(selected.damage) * 1.18f) + (selected.level * 120.0f)));
    preview.attackSpeedSec = (std::max)(1.2f, selected.attackSpeedSec - 0.2f);
    preview.attackRange = selected.attackRange + 1;
    preview.maxHp = selected.maxHp + (std::max)(static_cast<std::int64_t>(18000), selected.maxHp / 4);
    preview.currentHp = preview.maxHp;
    preview.constructionDurationMinutes = selected.constructionDurationMinutes + 45;
    preview.repairPriceGold = selected.repairPriceGold + (std::max)(static_cast<std::int64_t>(500), selected.repairPriceGold / 3);
    preview.repairAmountPer5Sec =
        selected.repairAmountPer5Sec + (std::max)(static_cast<std::int64_t>(2500), selected.repairAmountPer5Sec / 5);
    return preview;
}

void GuildTowerWidget::repairAllDamagedTowers(void)
{
    for (TowerLevelEntry& tower : this->towerLevels)
    {
        tower.currentHp = tower.maxHp;
    }
}

void GuildTowerWidget::clampCurrentHpToSelectedLevel(void)
{
    for (TowerLevelEntry& tower : this->towerLevels)
    {
        tower.currentHp = (std::max)(
            static_cast<std::int64_t>(0),
            (std::min)(tower.currentHp, (std::max)(static_cast<std::int64_t>(0), tower.maxHp)));
    }
}
