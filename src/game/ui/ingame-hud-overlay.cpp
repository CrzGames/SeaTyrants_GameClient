#include "game/ui/ingame-hud-overlay.h"

#include "core/context.h"

#include <RC2D/RC2D_memory.h>
#include <RC2D/RC2D_storage.h>

#include <SDL3/SDL.h>
#include <cJSON.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

static constexpr const char* kUserSettingsPath = "settings/user_settings.json";
static constexpr Uint64 kMaxUserSettingsBytes = 512ULL * 1024ULL;

static bool waitRc2dUserStorageReady(void)
{
    for (int spin = 0; spin < 4000; ++spin)
    {
        if (rc2d_storage_userReady())
        {
            return true;
        }
        SDL_Delay(1);
    }
    return rc2d_storage_userReady();
}

static constexpr float kHudTooltipOffsetX = 14.0f;
static constexpr float kHudTooltipOffsetY = 18.0f;
static constexpr float kHudTooltipPaddingX = 10.0f;
static constexpr float kHudTooltipPaddingY = 6.0f;
static constexpr RC2D_Color kHudTooltipFill = RC2D_Color{67, 8, 8, 236};
static constexpr RC2D_Color kHudTooltipBorder = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kHudTooltipText = RC2D_Color{217, 200, 134, 255};
static constexpr std::array<GameSettingsWidget::HudScaleTarget, 6> kHudConfiguratorTargets = {{
    GameSettingsWidget::HudScaleTarget::MINIMAP,
    GameSettingsWidget::HudScaleTarget::HP_BAR,
    GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR,
    GameSettingsWidget::HudScaleTarget::MAP_ZOOM,
    GameSettingsWidget::HudScaleTarget::CENTER_SHIP,
    GameSettingsWidget::HudScaleTarget::ACTION_BAR
}};
static constexpr float kHudConfiguratorPanelWidth = 436.0f;
static constexpr float kHudConfiguratorPanelHeight = 238.0f;
static constexpr float kHudConfiguratorPanelMargin = 22.0f;
static constexpr float kHudConfiguratorHighlightPadding = 14.0f;
static constexpr float kHudConfiguratorButtonWidth = 196.0f;
static constexpr float kHudConfiguratorButtonHeight = 26.0f;
static constexpr float kHudConfiguratorDoneButtonWidth = 112.0f;
static constexpr float kHudConfiguratorLabelPaddingX = 10.0f;
static constexpr float kHudConfiguratorLabelPaddingY = 5.0f;
static constexpr RC2D_Color kHudConfiguratorBackdrop = RC2D_Color{5, 10, 18, 138};
static constexpr RC2D_Color kHudConfiguratorPanelFill = RC2D_Color{7, 14, 25, 236};
static constexpr RC2D_Color kHudConfiguratorPanelBorder = RC2D_Color{184, 132, 30, 245};
static constexpr RC2D_Color kHudConfiguratorHighlightFill = RC2D_Color{67, 8, 8, 94};
static constexpr RC2D_Color kHudConfiguratorHighlightFillMuted = RC2D_Color{38, 46, 58, 108};
static constexpr RC2D_Color kHudConfiguratorHighlightBorder = RC2D_Color{227, 210, 153, 250};
static constexpr RC2D_Color kHudConfiguratorHighlightBorderMuted = RC2D_Color{150, 156, 166, 228};
static constexpr RC2D_Color kHudConfiguratorLabelFill = RC2D_Color{9, 15, 25, 240};
static constexpr RC2D_Color kHudConfiguratorText = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kHudConfiguratorTextMuted = RC2D_Color{176, 184, 194, 255};
static constexpr RC2D_Color kHudConfiguratorButtonFill = RC2D_Color{42, 33, 24, 238};
static constexpr RC2D_Color kHudConfiguratorButtonFillHover = RC2D_Color{92, 68, 38, 246};
static constexpr RC2D_Color kHudConfiguratorButtonFillSecondary = RC2D_Color{28, 35, 46, 238};

struct HudConfiguratorPanelLayout {
    SDL_FRect panelRect;
    SDL_FRect headerDragRect;
    std::array<SDL_FRect, kHudConfiguratorTargets.size()> rowRects;
    std::array<SDL_FRect, kHudConfiguratorTargets.size()> rowResetButtonRects;
    SDL_FRect resetAllButtonRect;
    SDL_FRect doneButtonRect;
};

static bool pointInRect(float x, float y, const SDL_FRect& rect)
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

static SDL_FRect inflateRect(const SDL_FRect& rect, float paddingX, float paddingY)
{
    return SDL_FRect{
        rect.x - paddingX,
        rect.y - paddingY,
        rect.w + (paddingX * 2.0f),
        rect.h + (paddingY * 2.0f)
    };
}

static SDL_FRect unionRects(const SDL_FRect& a, const SDL_FRect& b)
{
    if (a.w <= 0.0f || a.h <= 0.0f)
    {
        return b;
    }
    if (b.w <= 0.0f || b.h <= 0.0f)
    {
        return a;
    }

    const float left = (std::min)(a.x, b.x);
    const float top = (std::min)(a.y, b.y);
    const float right = (std::max)(a.x + a.w, b.x + b.w);
    const float bottom = (std::max)(a.y + a.h, b.y + b.h);
    return SDL_FRect{
        left,
        top,
        right - left,
        bottom - top
    };
}

static SDL_FRect buildHudConfiguratorSelectionRectUnclamped(const SDL_FRect& targetRect)
{
    if (targetRect.w <= 0.0f || targetRect.h <= 0.0f)
    {
        return targetRect;
    }

    SDL_FRect selectionRect = inflateRect(targetRect, kHudConfiguratorHighlightPadding, kHudConfiguratorHighlightPadding);
    if (selectionRect.w < 84.0f)
    {
        const float missingWidth = 84.0f - selectionRect.w;
        selectionRect.x -= missingWidth * 0.5f;
        selectionRect.w = 84.0f;
    }
    if (selectionRect.h < 48.0f)
    {
        const float missingHeight = 48.0f - selectionRect.h;
        selectionRect.y -= missingHeight * 0.5f;
        selectionRect.h = 48.0f;
    }

    return selectionRect;
}

static SDL_FRect clampRectInside(const SDL_FRect& rect, const SDL_FRect& bounds)
{
    if (bounds.w <= 0.0f || bounds.h <= 0.0f)
    {
        return rect;
    }

    SDL_FRect clamped = rect;
    if (rect.w >= bounds.w)
    {
        clamped.x = bounds.x;
    }
    else
    {
        clamped.x = std::clamp(rect.x, bounds.x, bounds.x + bounds.w - rect.w);
    }

    if (rect.h >= bounds.h)
    {
        clamped.y = bounds.y;
    }
    else
    {
        clamped.y = std::clamp(rect.y, bounds.y, bounds.y + bounds.h - rect.h);
    }

    return clamped;
}

static void fillAndOutlineRect(const SDL_FRect& rect, RC2D_Color fill, RC2D_Color border)
{
    rc2d_graphics_setColor(fill);
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setColor(border);
    rc2d_graphics_rectangle("line", &rect);
}

static void drawCenteredText(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_drawText(
        &textObject,
        std::round(rect.x + ((rect.w - static_cast<float>(width)) * 0.5f)),
        std::round(rect.y + ((rect.h - static_cast<float>(height)) * 0.5f)));
    rc2d_graphics_destroyText(&textObject);
}

static const char* getHudConfiguratorLabel(GameSettingsWidget::HudScaleTarget target)
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
            return "Minimap";
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            return "Barre HP";
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
            return "Barre XP";
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
            return "Zoom map";
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
            return "Centrer navire";
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
            return "Barre d'action";
        default:
            return "HUD";
    }
}

static HudConfiguratorPanelLayout buildHudConfiguratorPanelLayout(const SDL_FRect& gameScreenRect, const SDL_FPoint& panelOffset)
{
    HudConfiguratorPanelLayout layout{};
    const float panelWidth = (std::min)(kHudConfiguratorPanelWidth, gameScreenRect.w - (kHudConfiguratorPanelMargin * 2.0f));
    const float rowHeight = 28.0f;
    const float rowGap = 8.0f;
    const float rowsTopOffset = 104.0f;
    const float buttonsTopGap = 14.0f;
    const float panelBottomPadding = 16.0f;
    const float requiredPanelHeight =
        rowsTopOffset +
        (static_cast<float>(layout.rowRects.size()) * rowHeight) +
        (static_cast<float>((layout.rowRects.size() > 0U) ? (layout.rowRects.size() - 1U) : 0U) * rowGap) +
        buttonsTopGap +
        kHudConfiguratorButtonHeight +
        panelBottomPadding;
    const float panelHeight = (std::max)(kHudConfiguratorPanelHeight, requiredPanelHeight);
    layout.panelRect = clampRectInside(
        SDL_FRect{
        gameScreenRect.x + kHudConfiguratorPanelMargin + panelOffset.x,
        gameScreenRect.y + kHudConfiguratorPanelMargin + panelOffset.y,
        panelWidth,
        panelHeight
        },
        gameScreenRect);
    layout.headerDragRect = SDL_FRect{
        layout.panelRect.x + 8.0f,
        layout.panelRect.y + 8.0f,
        layout.panelRect.w - 16.0f,
        88.0f
    };

    const float rowsStartY = layout.panelRect.y + rowsTopOffset;
    for (std::size_t index = 0; index < layout.rowRects.size(); ++index)
    {
        layout.rowRects[index] = SDL_FRect{
            layout.panelRect.x + 16.0f,
            rowsStartY + (static_cast<float>(index) * (rowHeight + rowGap)),
            layout.panelRect.w - 32.0f,
            rowHeight
        };
        layout.rowResetButtonRects[index] = SDL_FRect{
            layout.rowRects[index].x + layout.rowRects[index].w - kHudConfiguratorButtonWidth,
            layout.rowRects[index].y + ((layout.rowRects[index].h - kHudConfiguratorButtonHeight) * 0.5f),
            kHudConfiguratorButtonWidth,
            kHudConfiguratorButtonHeight
        };
    }

    const float bottomButtonsY =
        layout.rowRects.back().y +
        layout.rowRects.back().h +
        buttonsTopGap;
    layout.doneButtonRect = SDL_FRect{
        layout.panelRect.x + layout.panelRect.w - kHudConfiguratorDoneButtonWidth - 16.0f,
        bottomButtonsY,
        kHudConfiguratorDoneButtonWidth,
        kHudConfiguratorButtonHeight
    };
    layout.resetAllButtonRect = SDL_FRect{
        layout.doneButtonRect.x - kHudConfiguratorButtonWidth - 10.0f,
        layout.doneButtonRect.y,
        kHudConfiguratorButtonWidth,
        kHudConfiguratorButtonHeight
    };
    return layout;
}

static void applyCursorIfChanged(SDL_SystemCursor id)
{
    static SDL_SystemCursor lastId = static_cast<SDL_SystemCursor>(-1);
    static SDL_Cursor* cached[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    const int index =
        (id == SDL_SYSTEM_CURSOR_POINTER) ? 1 :
        (id == SDL_SYSTEM_CURSOR_TEXT) ? 2 :
        (id == SDL_SYSTEM_CURSOR_MOVE) ? 3 :
        (id == SDL_SYSTEM_CURSOR_EW_RESIZE) ? 4 :
        (id == SDL_SYSTEM_CURSOR_NS_RESIZE) ? 5 :
        (id == SDL_SYSTEM_CURSOR_NWSE_RESIZE) ? 6 : 0;

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

static void setCursorIBeam(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_TEXT);
}

static void setCursorMove(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_MOVE);
}

static void setCursorResizeHorizontal(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_EW_RESIZE);
}

static void setCursorResizeVertical(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_NS_RESIZE);
}

static void setCursorResizeDiagonal(void)
{
    applyCursorIfChanged(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
}

static void applyHudCursor(HudCursorType cursor)
{
    switch (cursor)
    {
        case HudCursorType::POINTER:
            setCursorHand();
            break;
        case HudCursorType::TEXT:
            setCursorIBeam();
            break;
        case HudCursorType::MOVE:
            setCursorMove();
            break;
        case HudCursorType::RESIZE_HORIZONTAL:
            setCursorResizeHorizontal();
            break;
        case HudCursorType::RESIZE_VERTICAL:
            setCursorResizeVertical();
            break;
        case HudCursorType::RESIZE_DIAGONAL:
            setCursorResizeDiagonal();
            break;
        case HudCursorType::DEFAULT:
        case HudCursorType::NONE:
        default:
            setCursorArrow();
            break;
    }
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

static float measureTextWidth(RC2D_Font* font, const char* text)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)height;
    return static_cast<float>(width);
}

static float measureTextHeight(RC2D_Font* font, const char* text)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)width;
    return static_cast<float>(height);
}

static void drawTextAt(RC2D_Font* font, const char* text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    rc2d_graphics_drawText(&textObject, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&textObject);
}

IngameHudOverlay::IngameHudOverlay(void)
    : backgroundWidget{},
      topBarMenuWidget{},
      sectorCoordinateOverlay{},
      tileClickMarkerOverlay{},
      scrollBarOverlay{},
      minimapWidget{},
      minimapEspionButtonWidget{},
      minimapParamsButtonWidget{},
      barreActionWidget{},
      centerShipButtonWidget{},
      zoomWidget{},
      hpBarWidget{},
      experienceBarWidget{},
      chatWidget{},
      espionSearchPlayerWidget{},
      moneyWidget{},
      paramsMinimapWidget{},
      gameSettingsWidget{},
      announcementsWidget{},
      logBookWidget{},
      marketsAndBazarWidget{},
      accountManagementWidget{},
      captchaWidget{},
      leaderboardWidget{},
      tooltipFont{},
      hudWidgetVisibility{},
      hudConfiguratorMode(false),
      hudConfiguratorRestoreGameSettingsVisibility(false),
      hudConfiguratorDragging(false),
      hudConfiguratorDraggedTarget(GameSettingsWidget::HudScaleTarget::MINIMAP),
      hudConfiguratorDragGrabOffsetX(0.0f),
      hudConfiguratorDragGrabOffsetY(0.0f),
      hudConfiguratorDragStartOffset{0.0f, 0.0f},
      hudConfiguratorDragStartRect{0.0f, 0.0f, 0.0f, 0.0f},
      hudConfiguratorDragStartTargetRect{0.0f, 0.0f, 0.0f, 0.0f},
      hudConfiguratorPanelDragging(false),
      hudConfiguratorPanelDragGrabOffsetX(0.0f),
      hudConfiguratorPanelDragGrabOffsetY(0.0f),
      hudConfiguratorPanelOffset{0.0f, 0.0f},
      hudConfiguratorPanelDragStartOffset{0.0f, 0.0f},
      hudConfiguratorPanelDragStartRect{0.0f, 0.0f, 0.0f, 0.0f},
      windowDrawOrder{
          WindowLayer::CHAT,
          WindowLayer::ESPION,
          WindowLayer::MONEY,
          WindowLayer::PARAMS_MINIMAP,
          WindowLayer::GAME_SETTINGS,
          WindowLayer::ANNOUNCEMENTS,
          WindowLayer::LOG_BOOK,
          WindowLayer::MARKETS_AND_BAZAR,
          WindowLayer::ACCOUNT_MANAGEMENT,
          WindowLayer::CAPTCHA,
          WindowLayer::LEADERBOARD},
      hoveredMinimapTooltip(MinimapWidget::Tooltip::NONE),
      hoveredMinimapTooltipMouseX(0.0f),
      hoveredMinimapTooltipMouseY(0.0f),
      prevChatVisible(false),
      prevEspionVisible(false),
      prevMoneyVisible(false),
      prevParamsMiniMapVisible(false),
      prevGameSettingsVisible(false),
      prevAnnouncementsVisible(false),
      prevLogBookVisible(false),
      prevMarketsAndBazarVisible(false),
      prevAccountManagementVisible(false),
      prevCaptchaVisible(false),
      prevLeaderboardVisible(false),
      keepDefaultCursorAfterClose(false),
      keepDefaultCursorMouseX(0.0f),
      keepDefaultCursorMouseY(0.0f)
{
    this->hudWidgetVisibility.fill(true);
}

IngameHudOverlay::~IngameHudOverlay(void)
{
}

void IngameHudOverlay::bringWindowToFront(WindowLayer layer)
{
    const auto begin = this->windowDrawOrder.begin();
    const auto end = this->windowDrawOrder.end();
    const auto it = std::find(begin, end, layer);
    if (it == end)
    {
        return;
    }
    std::rotate(it, it + 1, end);
}

bool IngameHudOverlay::isWindowLayerVisible(WindowLayer layer) const
{
    switch (layer)
    {
        case WindowLayer::CHAT:
            return this->chatWidget.isVisible();
        case WindowLayer::ESPION:
            return this->espionSearchPlayerWidget.isVisible();
        case WindowLayer::MONEY:
            return this->moneyWidget.isVisible();
        case WindowLayer::PARAMS_MINIMAP:
            return this->paramsMinimapWidget.isVisible();
        case WindowLayer::GAME_SETTINGS:
            return this->gameSettingsWidget.isVisible();
        case WindowLayer::ANNOUNCEMENTS:
            return this->announcementsWidget.isVisible();
        case WindowLayer::LOG_BOOK:
            return this->logBookWidget.isVisible();
        case WindowLayer::MARKETS_AND_BAZAR:
            return this->marketsAndBazarWidget.isVisible();
        case WindowLayer::ACCOUNT_MANAGEMENT:
            return this->accountManagementWidget.isVisible();
        case WindowLayer::CAPTCHA:
            return this->captchaWidget.isVisible();
        case WindowLayer::LEADERBOARD:
            return this->leaderboardWidget.isVisible();
        default:
            return false;
    }
}

bool IngameHudOverlay::isHudWidgetVisible(GameSettingsWidget::HudScaleTarget target) const
{
    const std::size_t index = static_cast<std::size_t>(target);
    if (index >= this->hudWidgetVisibility.size())
    {
        return true;
    }

    return this->hudWidgetVisibility[index];
}

void IngameHudOverlay::setHudWidgetVisible(GameSettingsWidget::HudScaleTarget target, bool visible)
{
    const std::size_t index = static_cast<std::size_t>(target);
    if (index >= this->hudWidgetVisibility.size())
    {
        return;
    }

    this->hudWidgetVisibility[index] = visible;
    this->gameSettingsWidget.setHudVisibilityValue(target, visible);
    if (!visible && target == GameSettingsWidget::HudScaleTarget::MINIMAP)
    {
        this->paramsMinimapWidget.hide();
    }
}

void IngameHudOverlay::toggleHudWidgetVisibility(GameSettingsWidget::HudScaleTarget target)
{
    this->setHudWidgetVisible(target, !this->isHudWidgetVisible(target));
}

void IngameHudOverlay::startHudConfiguratorMode(void)
{
    if (this->hudConfiguratorMode)
    {
        return;
    }

    this->hudConfiguratorMode = true;
    this->hudConfiguratorRestoreGameSettingsVisibility = this->gameSettingsWidget.isVisible();
    this->hudConfiguratorDragging = false;
    this->hudConfiguratorDraggedTarget = GameSettingsWidget::HudScaleTarget::MINIMAP;
    this->hudConfiguratorDragGrabOffsetX = 0.0f;
    this->hudConfiguratorDragGrabOffsetY = 0.0f;
    this->hudConfiguratorDragStartOffset = SDL_FPoint{0.0f, 0.0f};
    this->hudConfiguratorDragStartRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hudConfiguratorDragStartTargetRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hudConfiguratorPanelDragging = false;
    this->hudConfiguratorPanelDragGrabOffsetX = 0.0f;
    this->hudConfiguratorPanelDragGrabOffsetY = 0.0f;
    this->hudConfiguratorPanelDragStartOffset = this->hudConfiguratorPanelOffset;
    this->hudConfiguratorPanelDragStartRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->keepDefaultCursorAfterClose = false;

    if (this->hudConfiguratorRestoreGameSettingsVisibility)
    {
        this->gameSettingsWidget.hide();
    }
}

void IngameHudOverlay::stopHudConfiguratorMode(void)
{
    if (!this->hudConfiguratorMode)
    {
        return;
    }

    this->hudConfiguratorMode = false;
    this->hudConfiguratorDragging = false;
    this->hudConfiguratorDragGrabOffsetX = 0.0f;
    this->hudConfiguratorDragGrabOffsetY = 0.0f;
    this->hudConfiguratorPanelDragging = false;
    this->hudConfiguratorPanelDragGrabOffsetX = 0.0f;
    this->hudConfiguratorPanelDragGrabOffsetY = 0.0f;
    applyHudCursor(HudCursorType::DEFAULT);

    if (this->hudConfiguratorRestoreGameSettingsVisibility)
    {
        this->gameSettingsWidget.show();
        this->bringWindowToFront(WindowLayer::GAME_SETTINGS);
    }

    this->hudConfiguratorRestoreGameSettingsVisibility = false;
}

bool IngameHudOverlay::handleHudConfiguratorMousePressed(float x, float y, RC2D_MouseButton button)
{
    if (!this->hudConfiguratorMode)
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    const HudConfiguratorPanelLayout layout = buildHudConfiguratorPanelLayout(GetGameScreen().rect, this->hudConfiguratorPanelOffset);
    if (pointInRect(x, y, layout.doneButtonRect))
    {
        this->stopHudConfiguratorMode();
        return true;
    }
    if (pointInRect(x, y, layout.resetAllButtonRect))
    {
        this->resetAllHudConfiguratorPositions();
        this->hudConfiguratorDragging = false;
        this->hudConfiguratorPanelDragging = false;
        return true;
    }

    for (std::size_t index = 0; index < kHudConfiguratorTargets.size(); ++index)
    {
        if (!pointInRect(x, y, layout.rowResetButtonRects[index]))
        {
            continue;
        }

        this->resetHudWidgetPositionOffset(kHudConfiguratorTargets[index]);
        if (this->hudConfiguratorDragging && this->hudConfiguratorDraggedTarget == kHudConfiguratorTargets[index])
        {
            this->hudConfiguratorDragging = false;
        }
        this->hudConfiguratorPanelDragging = false;
        return true;
    }

    if (pointInRect(x, y, layout.headerDragRect))
    {
        this->hudConfiguratorDragging = false;
        this->hudConfiguratorPanelDragging = true;
        this->hudConfiguratorPanelDragGrabOffsetX = x - layout.panelRect.x;
        this->hudConfiguratorPanelDragGrabOffsetY = y - layout.panelRect.y;
        this->hudConfiguratorPanelDragStartOffset = this->hudConfiguratorPanelOffset;
        this->hudConfiguratorPanelDragStartRect = layout.panelRect;
        return true;
    }

    if (pointInRect(x, y, layout.panelRect))
    {
        this->hudConfiguratorDragging = false;
        return true;
    }

    for (auto it = kHudConfiguratorTargets.rbegin(); it != kHudConfiguratorTargets.rend(); ++it)
    {
        const GameSettingsWidget::HudScaleTarget target = *it;
        const SDL_FRect targetRect = this->getHudConfiguratorTargetRect(target);
        const SDL_FRect selectionRect = buildHudConfiguratorSelectionRectUnclamped(targetRect);
        if (!pointInRect(x, y, selectionRect))
        {
            continue;
        }

        this->hudConfiguratorPanelDragging = false;
        this->hudConfiguratorDragging = true;
        this->hudConfiguratorDraggedTarget = target;
        this->hudConfiguratorDragGrabOffsetX = x - selectionRect.x;
        this->hudConfiguratorDragGrabOffsetY = y - selectionRect.y;
        this->hudConfiguratorDragStartOffset = this->getHudWidgetPositionOffset(target);
        this->hudConfiguratorDragStartRect = selectionRect;
        this->hudConfiguratorDragStartTargetRect = targetRect;
        return true;
    }

    return true;
}

void IngameHudOverlay::updateHudConfigurator(float mouseX, float mouseY)
{
    if (!this->hudConfiguratorMode)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        const bool endedDrag = this->hudConfiguratorDragging || this->hudConfiguratorPanelDragging;
        this->hudConfiguratorDragging = false;
        this->hudConfiguratorPanelDragging = false;
        if (endedDrag && !this->suppressUserSettingsSave)
        {
            this->saveUserSettingsToDisk();
        }
        return;
    }

    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    if (this->hudConfiguratorPanelDragging)
    {
        SDL_FRect desiredPanelRect = this->hudConfiguratorPanelDragStartRect;
        desiredPanelRect.x = mouseX - this->hudConfiguratorPanelDragGrabOffsetX;
        desiredPanelRect.y = mouseY - this->hudConfiguratorPanelDragGrabOffsetY;
        desiredPanelRect = clampRectInside(desiredPanelRect, gameScreenRect);

        this->hudConfiguratorPanelOffset = SDL_FPoint{
            this->hudConfiguratorPanelDragStartOffset.x + (desiredPanelRect.x - this->hudConfiguratorPanelDragStartRect.x),
            this->hudConfiguratorPanelDragStartOffset.y + (desiredPanelRect.y - this->hudConfiguratorPanelDragStartRect.y)
        };
        return;
    }

    if (!this->hudConfiguratorDragging)
    {
        return;
    }

    SDL_FRect desiredRect = this->hudConfiguratorDragStartRect;
    desiredRect.x = mouseX - this->hudConfiguratorDragGrabOffsetX;
    desiredRect.y = mouseY - this->hudConfiguratorDragGrabOffsetY;
    const float desiredDeltaX = desiredRect.x - this->hudConfiguratorDragStartRect.x;
    const float desiredDeltaY = desiredRect.y - this->hudConfiguratorDragStartRect.y;

    SDL_FRect desiredTargetRect = this->hudConfiguratorDragStartTargetRect;
    desiredTargetRect.x += desiredDeltaX;
    desiredTargetRect.y += desiredDeltaY;
    desiredTargetRect = clampRectInside(desiredTargetRect, gameScreenRect);

    const float clampedDeltaX = desiredTargetRect.x - this->hudConfiguratorDragStartTargetRect.x;
    const float clampedDeltaY = desiredTargetRect.y - this->hudConfiguratorDragStartTargetRect.y;
    this->setHudWidgetPositionOffset(
        this->hudConfiguratorDraggedTarget,
        SDL_FPoint{
            this->hudConfiguratorDragStartOffset.x + clampedDeltaX,
            this->hudConfiguratorDragStartOffset.y + clampedDeltaY
        });
}

void IngameHudOverlay::drawHudConfiguratorOverlay(void) const
{
    SDL_FRect gameScreenRect = GetGameScreen().rect;
    if (gameScreenRect.w <= 0.0f || gameScreenRect.h <= 0.0f)
    {
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    const HudConfiguratorPanelLayout layout = buildHudConfiguratorPanelLayout(gameScreenRect, this->hudConfiguratorPanelOffset);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kHudConfiguratorBackdrop);
    rc2d_graphics_rectangle("fill", &gameScreenRect);

    for (const GameSettingsWidget::HudScaleTarget target : kHudConfiguratorTargets)
    {
        const SDL_FRect selectionRect = this->getHudConfiguratorSelectionRect(target);
        if (selectionRect.w <= 0.0f || selectionRect.h <= 0.0f)
        {
            continue;
        }

        const bool isVisible = this->isHudWidgetVisible(target);
        const bool isActiveDrag = this->hudConfiguratorDragging && this->hudConfiguratorDraggedTarget == target;
        fillAndOutlineRect(
            selectionRect,
            isVisible ? kHudConfiguratorHighlightFill : kHudConfiguratorHighlightFillMuted,
            isActiveDrag
                ? kHudConfiguratorPanelBorder
                : (isVisible ? kHudConfiguratorHighlightBorder : kHudConfiguratorHighlightBorderMuted));

        std::string label = getHudConfiguratorLabel(target);
        if (!isVisible)
        {
            label += " (masque)";
        }

        const float labelWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->tooltipFont), label.c_str());
        const float labelHeight = measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), label.c_str());
        SDL_FRect labelRect = SDL_FRect{
            selectionRect.x + 8.0f,
            selectionRect.y - (labelHeight + (kHudConfiguratorLabelPaddingY * 2.0f) + 5.0f),
            labelWidth + (kHudConfiguratorLabelPaddingX * 2.0f),
            labelHeight + (kHudConfiguratorLabelPaddingY * 2.0f)
        };
        if (labelRect.y < gameScreenRect.y + 6.0f)
        {
            labelRect.y = selectionRect.y + 8.0f;
        }
        if (labelRect.x + labelRect.w > gameScreenRect.x + gameScreenRect.w - 6.0f)
        {
            labelRect.x = gameScreenRect.x + gameScreenRect.w - labelRect.w - 6.0f;
        }

        fillAndOutlineRect(
            labelRect,
            kHudConfiguratorLabelFill,
            isVisible ? kHudConfiguratorHighlightBorder : kHudConfiguratorHighlightBorderMuted);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->tooltipFont),
            label.c_str(),
            labelRect.x + kHudConfiguratorLabelPaddingX,
            labelRect.y + kHudConfiguratorLabelPaddingY,
            isVisible ? kHudConfiguratorText : kHudConfiguratorTextMuted);
    }

    fillAndOutlineRect(layout.panelRect, kHudConfiguratorPanelFill, kHudConfiguratorPanelBorder);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Mode configuration HUD",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 16.0f,
        kHudConfiguratorText);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Glissez les cadres pour deplacer les interfaces.",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 42.0f,
        kHudConfiguratorTextMuted);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Glissez aussi ce panneau si besoin pour liberer l'ecran.",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 60.0f,
        kHudConfiguratorTextMuted);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Echap ou Terminer pour revenir aux parametres.",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 78.0f,
        kHudConfiguratorTextMuted);

    for (std::size_t index = 0; index < kHudConfiguratorTargets.size(); ++index)
    {
        const GameSettingsWidget::HudScaleTarget target = kHudConfiguratorTargets[index];
        const bool isVisible = this->isHudWidgetVisible(target);
        const SDL_FRect& rowRect = layout.rowRects[index];
        const SDL_FRect& buttonRect = layout.rowResetButtonRects[index];
        const bool hoveredReset = pointInRect(mouseX, mouseY, buttonRect);

        fillAndOutlineRect(
            rowRect,
            kHudConfiguratorLabelFill,
            isVisible ? kHudConfiguratorHighlightBorderMuted : kHudConfiguratorHighlightBorderMuted);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->tooltipFont),
            getHudConfiguratorLabel(target),
            rowRect.x + 12.0f,
            rowRect.y + ((rowRect.h - measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), getHudConfiguratorLabel(target))) * 0.5f),
            kHudConfiguratorText);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->tooltipFont),
            isVisible ? "Visible" : "Masque",
            buttonRect.x - 66.0f,
            rowRect.y + ((rowRect.h - measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), "Visible")) * 0.5f),
            isVisible ? kHudConfiguratorTextMuted : kHudConfiguratorText);

        fillAndOutlineRect(
            buttonRect,
            hoveredReset ? kHudConfiguratorButtonFillHover : kHudConfiguratorButtonFillSecondary,
            hoveredReset ? kHudConfiguratorPanelBorder : kHudConfiguratorHighlightBorderMuted);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->tooltipFont),
            "Réinitialiser par défaut",
            buttonRect,
            kHudConfiguratorText);
    }

    const bool hoveredResetAll = pointInRect(mouseX, mouseY, layout.resetAllButtonRect);
    const bool hoveredDone = pointInRect(mouseX, mouseY, layout.doneButtonRect);
    fillAndOutlineRect(
        layout.resetAllButtonRect,
        hoveredResetAll ? kHudConfiguratorButtonFillHover : kHudConfiguratorButtonFillSecondary,
        hoveredResetAll ? kHudConfiguratorPanelBorder : kHudConfiguratorHighlightBorderMuted);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Réinitialiser tout par défaut",
        layout.resetAllButtonRect,
        kHudConfiguratorText);

    fillAndOutlineRect(
        layout.doneButtonRect,
        hoveredDone ? kHudConfiguratorButtonFillHover : kHudConfiguratorButtonFill,
        hoveredDone ? kHudConfiguratorPanelBorder : kHudConfiguratorHighlightBorder);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        "Terminer",
        layout.doneButtonRect,
        kHudConfiguratorText);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

HudCursorType IngameHudOverlay::getHudConfiguratorDesiredCursor(float mouseX, float mouseY) const
{
    if (!this->hudConfiguratorMode)
    {
        return HudCursorType::NONE;
    }

    if (this->hudConfiguratorDragging || this->hudConfiguratorPanelDragging)
    {
        return HudCursorType::MOVE;
    }

    const HudConfiguratorPanelLayout layout = buildHudConfiguratorPanelLayout(GetGameScreen().rect, this->hudConfiguratorPanelOffset);
    if (pointInRect(mouseX, mouseY, layout.doneButtonRect) ||
        pointInRect(mouseX, mouseY, layout.resetAllButtonRect))
    {
        return HudCursorType::POINTER;
    }

    for (const SDL_FRect& buttonRect : layout.rowResetButtonRects)
    {
        if (pointInRect(mouseX, mouseY, buttonRect))
        {
            return HudCursorType::POINTER;
        }
    }

    if (pointInRect(mouseX, mouseY, layout.headerDragRect))
    {
        return HudCursorType::MOVE;
    }

    if (pointInRect(mouseX, mouseY, layout.panelRect))
    {
        return HudCursorType::DEFAULT;
    }

    for (const GameSettingsWidget::HudScaleTarget target : kHudConfiguratorTargets)
    {
        if (pointInRect(mouseX, mouseY, this->getHudConfiguratorSelectionRect(target)))
        {
            return HudCursorType::MOVE;
        }
    }

    return HudCursorType::DEFAULT;
}

SDL_FRect IngameHudOverlay::getHudConfiguratorTargetRect(GameSettingsWidget::HudScaleTarget target) const
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
        {
            SDL_FRect minimapRect = this->minimapWidget.getCurrentRect();
            minimapRect = unionRects(minimapRect, this->minimapEspionButtonWidget.getCurrentRect(this->minimapWidget));
            minimapRect = unionRects(minimapRect, this->minimapParamsButtonWidget.getCurrentRect(this->minimapWidget));
            return minimapRect;
        }
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            return this->hpBarWidget.getCurrentRect();
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
            return this->experienceBarWidget.getCurrentRect();
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
            return this->zoomWidget.getCurrentRect();
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
            return this->centerShipButtonWidget.getCurrentRect();
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
            return this->barreActionWidget.getCurrentRect();
        default:
            return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }
}

SDL_FRect IngameHudOverlay::getHudConfiguratorSelectionRect(GameSettingsWidget::HudScaleTarget target) const
{
    const SDL_FRect targetRect = this->getHudConfiguratorTargetRect(target);
    return buildHudConfiguratorSelectionRectUnclamped(targetRect);
}

SDL_FPoint IngameHudOverlay::getHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target) const
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
            return this->minimapWidget.getPositionOffset();
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            return this->hpBarWidget.getPositionOffset();
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
            return this->experienceBarWidget.getPositionOffset();
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
            return this->zoomWidget.getPositionOffset();
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
            return this->centerShipButtonWidget.getPositionOffset();
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
            return this->barreActionWidget.getPositionOffset();
        default:
            return SDL_FPoint{0.0f, 0.0f};
    }
}

void IngameHudOverlay::setHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target, const SDL_FPoint& offset)
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
            this->minimapWidget.setPositionOffset(offset.x, offset.y);
            break;
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            this->hpBarWidget.setPositionOffset(offset.x, offset.y);
            break;
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
            this->experienceBarWidget.setPositionOffset(offset.x, offset.y);
            break;
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
            this->zoomWidget.setPositionOffset(offset.x, offset.y);
            break;
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
            this->centerShipButtonWidget.setPositionOffset(offset.x, offset.y);
            break;
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
            this->barreActionWidget.setPositionOffset(offset.x, offset.y);
            break;
        default:
            break;
    }

    if (!this->suppressUserSettingsSave)
    {
        this->saveUserSettingsToDisk();
    }
}

void IngameHudOverlay::resetHudWidgetPositionOffset(GameSettingsWidget::HudScaleTarget target)
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
            this->minimapWidget.resetPositionOffset();
            break;
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            this->hpBarWidget.resetPositionOffset();
            break;
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
            this->experienceBarWidget.resetPositionOffset();
            break;
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
            this->zoomWidget.resetPositionOffset();
            break;
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
            this->centerShipButtonWidget.resetPositionOffset();
            break;
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
            this->barreActionWidget.resetPositionOffset();
            break;
        default:
            break;
    }

    if (!this->suppressUserSettingsSave)
    {
        this->saveUserSettingsToDisk();
    }
}

void IngameHudOverlay::resetAllHudConfiguratorPositions(void)
{
    const bool prevSuppress = this->suppressUserSettingsSave;
    this->suppressUserSettingsSave = true;
    for (const GameSettingsWidget::HudScaleTarget target : kHudConfiguratorTargets)
    {
        this->resetHudWidgetPositionOffset(target);
    }
    this->suppressUserSettingsSave = prevSuppress;
    if (!this->suppressUserSettingsSave)
    {
        this->saveUserSettingsToDisk();
    }
}

HudCursorType IngameHudOverlay::getWindowLayerDesiredCursor(WindowLayer layer, float mouseX, float mouseY) const
{
    switch (layer)
    {
        case WindowLayer::CHAT:
            return this->chatWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::ESPION:
            return this->espionSearchPlayerWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::MONEY:
            return this->moneyWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::PARAMS_MINIMAP:
            return this->paramsMinimapWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::GAME_SETTINGS:
            return this->gameSettingsWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::ANNOUNCEMENTS:
            return this->announcementsWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::LOG_BOOK:
            return this->logBookWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::MARKETS_AND_BAZAR:
            return this->marketsAndBazarWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::ACCOUNT_MANAGEMENT:
            return this->accountManagementWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::CAPTCHA:
            return this->captchaWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::LEADERBOARD:
            return this->leaderboardWidget.getDesiredCursor(mouseX, mouseY);
        default:
            return HudCursorType::NONE;
    }
}

void IngameHudOverlay::beginCursorResetAfterClose(float mouseX, float mouseY)
{
    this->keepDefaultCursorAfterClose = true;
    this->keepDefaultCursorMouseX = mouseX;
    this->keepDefaultCursorMouseY = mouseY;
    applyHudCursor(HudCursorType::DEFAULT);
}

bool IngameHudOverlay::shouldKeepDefaultCursorAfterClose(float mouseX, float mouseY)
{
    if (!this->keepDefaultCursorAfterClose)
    {
        return false;
    }

    constexpr float kMouseMoveEpsilon = 0.5f;
    if (std::fabs(mouseX - this->keepDefaultCursorMouseX) > kMouseMoveEpsilon ||
        std::fabs(mouseY - this->keepDefaultCursorMouseY) > kMouseMoveEpsilon)
    {
        this->keepDefaultCursorAfterClose = false;
        return false;
    }

    return true;
}

void IngameHudOverlay::syncWindowOrderOnOpen(void)
{
    const bool chatVisible = this->chatWidget.isVisible();
    const bool espionVisible = this->espionSearchPlayerWidget.isVisible();
    const bool moneyVisible = this->moneyWidget.isVisible();
    const bool paramsVisible = this->paramsMinimapWidget.isVisible();
    const bool gameSettingsVisible = this->gameSettingsWidget.isVisible();
    const bool announcementsVisible = this->announcementsWidget.isVisible();
    const bool logBookVisible = this->logBookWidget.isVisible();
    const bool marketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();
    const bool accountManagementVisible = this->accountManagementWidget.isVisible();
    const bool captchaVisible = this->captchaWidget.isVisible();
    const bool leaderboardVisible = this->leaderboardWidget.isVisible();

    if (chatVisible && !this->prevChatVisible)
    {
        this->bringWindowToFront(WindowLayer::CHAT);
    }
    if (espionVisible && !this->prevEspionVisible)
    {
        this->bringWindowToFront(WindowLayer::ESPION);
    }
    if (moneyVisible && !this->prevMoneyVisible)
    {
        this->bringWindowToFront(WindowLayer::MONEY);
    }
    if (paramsVisible && !this->prevParamsMiniMapVisible)
    {
        this->bringWindowToFront(WindowLayer::PARAMS_MINIMAP);
    }
    if (gameSettingsVisible && !this->prevGameSettingsVisible)
    {
        this->bringWindowToFront(WindowLayer::GAME_SETTINGS);
    }
    if (announcementsVisible && !this->prevAnnouncementsVisible)
    {
        this->bringWindowToFront(WindowLayer::ANNOUNCEMENTS);
    }
    if (logBookVisible && !this->prevLogBookVisible)
    {
        this->bringWindowToFront(WindowLayer::LOG_BOOK);
    }
    if (marketsAndBazarVisible && !this->prevMarketsAndBazarVisible)
    {
        this->bringWindowToFront(WindowLayer::MARKETS_AND_BAZAR);
    }
    if (accountManagementVisible && !this->prevAccountManagementVisible)
    {
        this->bringWindowToFront(WindowLayer::ACCOUNT_MANAGEMENT);
    }
    if (captchaVisible && !this->prevCaptchaVisible)
    {
        this->bringWindowToFront(WindowLayer::CAPTCHA);
    }
    if (leaderboardVisible && !this->prevLeaderboardVisible)
    {
        this->bringWindowToFront(WindowLayer::LEADERBOARD);
    }

    this->prevChatVisible = chatVisible;
    this->prevEspionVisible = espionVisible;
    this->prevMoneyVisible = moneyVisible;
    this->prevParamsMiniMapVisible = paramsVisible;
    this->prevGameSettingsVisible = gameSettingsVisible;
    this->prevAnnouncementsVisible = announcementsVisible;
    this->prevLogBookVisible = logBookVisible;
    this->prevMarketsAndBazarVisible = marketsAndBazarVisible;
    this->prevAccountManagementVisible = accountManagementVisible;
    this->prevCaptchaVisible = captchaVisible;
    this->prevLeaderboardVisible = leaderboardVisible;
}

void IngameHudOverlay::saveUserSettingsToDisk(void)
{
    if (this->suppressUserSettingsSave)
    {
        return;
    }
    if (!waitRc2dUserStorageReady())
    {
        return;
    }

    cJSON* root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return;
    }
    cJSON_AddNumberToObject(root, "version", 1);

    cJSON* hud = cJSON_AddObjectToObject(root, "hud");
    cJSON* scales = cJSON_AddObjectToObject(hud, "scale");
    cJSON* visible = cJSON_AddObjectToObject(hud, "visible");
    cJSON* offsets = cJSON_AddObjectToObject(hud, "offset");
    cJSON* panel = cJSON_AddObjectToObject(hud, "configuratorPanelOffset");
    cJSON_AddNumberToObject(panel, "x", this->hudConfiguratorPanelOffset.x);
    cJSON_AddNumberToObject(panel, "y", this->hudConfiguratorPanelOffset.y);

    auto addHudKey = [&](GameSettingsWidget::HudScaleTarget tgt, const char* key)
    {
        cJSON_AddNumberToObject(scales, key, this->gameSettingsWidget.getHudScaleValue(tgt));
        cJSON_AddBoolToObject(visible, key, this->gameSettingsWidget.getHudVisibilityValue(tgt) ? 1 : 0);
        const SDL_FPoint off = this->getHudWidgetPositionOffset(tgt);
        cJSON* offObj = cJSON_AddObjectToObject(offsets, key);
        cJSON_AddNumberToObject(offObj, "x", off.x);
        cJSON_AddNumberToObject(offObj, "y", off.y);
    };

    addHudKey(GameSettingsWidget::HudScaleTarget::MINIMAP, "MINIMAP");
    addHudKey(GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR, "EXPERIENCE_BAR");
    addHudKey(GameSettingsWidget::HudScaleTarget::HP_BAR, "HP_BAR");
    addHudKey(GameSettingsWidget::HudScaleTarget::MAP_ZOOM, "MAP_ZOOM");
    addHudKey(GameSettingsWidget::HudScaleTarget::CENTER_SHIP, "CENTER_SHIP");
    addHudKey(GameSettingsWidget::HudScaleTarget::ACTION_BAR, "ACTION_BAR");

    const MapPlayfieldFrameMarginsPercent mapFrameMargins = this->gameSettingsWidget.getMapPlayfieldFrameMarginsPercent();
    cJSON* mapFrameJson = cJSON_AddObjectToObject(hud, "mapPlayfieldFrameMarginsPercent");
    cJSON_AddNumberToObject(mapFrameJson, "left", mapFrameMargins.left);
    cJSON_AddNumberToObject(mapFrameJson, "right", mapFrameMargins.right);
    cJSON_AddNumberToObject(mapFrameJson, "bottom", mapFrameMargins.bottom);

    cJSON* controls = cJSON_AddObjectToObject(root, "controls");
    cJSON_AddNumberToObject(controls, "cameraScrollSpeedSectors", this->gameSettingsWidget.getCameraScrollSpeedSectors());
    cJSON_AddNumberToObject(controls, "mapWorldZoomFactor", GetCamera().getZoomFactor());
    cJSON* bindings = cJSON_AddObjectToObject(controls, "bindings");

    auto addBind = [&](GameSettingsWidget::ControlAction action, const char* key)
    {
        cJSON_AddNumberToObject(bindings, key, static_cast<int>(this->gameSettingsWidget.getControlActionScancode(action)));
    };

    addBind(GameSettingsWidget::ControlAction::CAMERA_MOVE_UP, "CAMERA_MOVE_UP");
    addBind(GameSettingsWidget::ControlAction::CAMERA_MOVE_DOWN, "CAMERA_MOVE_DOWN");
    addBind(GameSettingsWidget::ControlAction::CAMERA_MOVE_LEFT, "CAMERA_MOVE_LEFT");
    addBind(GameSettingsWidget::ControlAction::CAMERA_MOVE_RIGHT, "CAMERA_MOVE_RIGHT");
    addBind(GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP, "CENTER_CAMERA_ON_SHIP");
    addBind(GameSettingsWidget::ControlAction::TOOLBAR_ATTACK, "TOOLBAR_ATTACK");
    addBind(GameSettingsWidget::ControlAction::TOOLBAR_CANCEL_ATTACK, "TOOLBAR_CANCEL_ATTACK");
    addBind(GameSettingsWidget::ControlAction::TOOLBAR_BOARDING, "TOOLBAR_BOARDING");
    addBind(GameSettingsWidget::ControlAction::TOOLBAR_REPAIR, "TOOLBAR_REPAIR");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_1, "SHORTCUT_1");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_2, "SHORTCUT_2");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_3, "SHORTCUT_3");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_4, "SHORTCUT_4");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_5, "SHORTCUT_5");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_6, "SHORTCUT_6");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_7, "SHORTCUT_7");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_8, "SHORTCUT_8");
    addBind(GameSettingsWidget::ControlAction::SHORTCUT_9, "SHORTCUT_9");
    addBind(GameSettingsWidget::ControlAction::JUMP_MAP, "JUMP_MAP");
    addBind(GameSettingsWidget::ControlAction::FORCE_CLICKED_POSITION_MOVE, "FORCE_CLICKED_POSITION_MOVE");
    addBind(GameSettingsWidget::ControlAction::TOGGLE_MINIMAP, "TOGGLE_MINIMAP");

    cJSON* graphics = cJSON_AddObjectToObject(root, "graphics");
    cJSON_AddBoolToObject(graphics, "hideCoordinateBackground", this->gameSettingsWidget.getHideCoordinateBackground() ? 1 : 0);
    cJSON_AddBoolToObject(graphics, "fogOfWarEnabled", this->gameSettingsWidget.getFogOfWarEnabled() ? 1 : 0);
    cJSON_AddBoolToObject(graphics, "shipWakeTrailsEnabled", this->gameSettingsWidget.getShipWakeTrailsEnabled() ? 1 : 0);
    cJSON_AddBoolToObject(graphics, "hideOtherPlayersVfx", this->gameSettingsWidget.getHideOtherPlayersVfxEnabled() ? 1 : 0);
    cJSON_AddNumberToObject(graphics, "salvoBulletPreset", static_cast<int>(this->gameSettingsWidget.getSalvoBulletPreset()));

    char* rendered = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (rendered == nullptr)
    {
        return;
    }

    rc2d_storage_userMkdir("settings");
    (void)rc2d_storage_userWriteFile(
        kUserSettingsPath,
        rendered,
        static_cast<Uint64>(std::strlen(rendered)));
    cJSON_free(rendered);
}

void IngameHudOverlay::loadUserSettingsFromDisk(void)
{
    if (!waitRc2dUserStorageReady())
    {
        return;
    }

    Uint64 settingsLen = 0;
    if (!rc2d_storage_userGetFileSize(kUserSettingsPath, &settingsLen) || settingsLen == 0)
    {
        rc2d_storage_userMkdir("settings");
        const bool prevSuppress = this->suppressUserSettingsSave;
        this->suppressUserSettingsSave = false;
        this->saveUserSettingsToDisk();
        this->suppressUserSettingsSave = prevSuppress;
        return;
    }

    if (settingsLen > kMaxUserSettingsBytes ||
        settingsLen > static_cast<Uint64>((std::numeric_limits<std::size_t>::max)()))
    {
        return;
    }

    void* fileData = nullptr;
    Uint64 fileLen = 0;
    if (!rc2d_storage_userReadFile(kUserSettingsPath, &fileData, &fileLen) || fileData == nullptr || fileLen == 0)
    {
        RC2D_safe_free(fileData);
        return;
    }

    if (fileLen > kMaxUserSettingsBytes)
    {
        RC2D_safe_free(fileData);
        return;
    }

    cJSON* root = cJSON_ParseWithLength(static_cast<const char*>(fileData), static_cast<std::size_t>(fileLen));
    RC2D_safe_free(fileData);
    if (root == nullptr)
    {
        return;
    }

    const cJSON* hud = cJSON_GetObjectItemCaseSensitive(root, "hud");
    if (cJSON_IsObject(hud))
    {
        const cJSON* scales = cJSON_GetObjectItemCaseSensitive(hud, "scale");
        const cJSON* visJson = cJSON_GetObjectItemCaseSensitive(hud, "visible");
        const cJSON* offsets = cJSON_GetObjectItemCaseSensitive(hud, "offset");
        const cJSON* panel = cJSON_GetObjectItemCaseSensitive(hud, "configuratorPanelOffset");

        for (std::size_t i = 0; i < static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT); ++i)
        {
            const auto target = static_cast<GameSettingsWidget::HudScaleTarget>(i);
            const char* key = nullptr;
            switch (target)
            {
                case GameSettingsWidget::HudScaleTarget::MINIMAP:
                    key = "MINIMAP";
                    break;
                case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
                    key = "EXPERIENCE_BAR";
                    break;
                case GameSettingsWidget::HudScaleTarget::HP_BAR:
                    key = "HP_BAR";
                    break;
                case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
                    key = "MAP_ZOOM";
                    break;
                case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
                    key = "CENTER_SHIP";
                    break;
                case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
                    key = "ACTION_BAR";
                    break;
                case GameSettingsWidget::HudScaleTarget::COUNT:
                default:
                    continue;
            }

            if (cJSON_IsObject(scales))
            {
                const cJSON* v = cJSON_GetObjectItemCaseSensitive(scales, key);
                if (cJSON_IsNumber(v))
                {
                    const float scale = static_cast<float>(v->valuedouble);
                    this->gameSettingsWidget.setHudScaleValue(target, scale);
                    switch (target)
                    {
                        case GameSettingsWidget::HudScaleTarget::MINIMAP:
                            this->minimapWidget.setUiScale(scale);
                            break;
                        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
                            this->experienceBarWidget.setUiScale(scale);
                            break;
                        case GameSettingsWidget::HudScaleTarget::HP_BAR:
                            this->hpBarWidget.setUiScale(scale);
                            break;
                        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
                            this->zoomWidget.setUiScale(scale);
                            break;
                        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
                            this->centerShipButtonWidget.setUiScale(scale);
                            break;
                        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
                            this->barreActionWidget.setUiScale(scale);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (cJSON_IsObject(visJson))
            {
                const cJSON* v = cJSON_GetObjectItemCaseSensitive(visJson, key);
                if (cJSON_IsBool(v))
                {
                    const bool vis = cJSON_IsTrue(v);
                    this->gameSettingsWidget.setHudVisibilityValue(target, vis);
                    const std::size_t idx = static_cast<std::size_t>(target);
                    if (idx < this->hudWidgetVisibility.size())
                    {
                        this->hudWidgetVisibility[idx] = vis;
                    }
                }
            }

            if (cJSON_IsObject(offsets))
            {
                const cJSON* off = cJSON_GetObjectItemCaseSensitive(offsets, key);
                if (cJSON_IsObject(off))
                {
                    const cJSON* ox = cJSON_GetObjectItemCaseSensitive(off, "x");
                    const cJSON* oy = cJSON_GetObjectItemCaseSensitive(off, "y");
                    if (cJSON_IsNumber(ox) && cJSON_IsNumber(oy))
                    {
                        this->setHudWidgetPositionOffset(
                            target,
                            SDL_FPoint{static_cast<float>(ox->valuedouble), static_cast<float>(oy->valuedouble)});
                    }
                }
            }
        }

        if (cJSON_IsObject(panel))
        {
            const cJSON* px = cJSON_GetObjectItemCaseSensitive(panel, "x");
            const cJSON* py = cJSON_GetObjectItemCaseSensitive(panel, "y");
            if (cJSON_IsNumber(px) && cJSON_IsNumber(py))
            {
                this->hudConfiguratorPanelOffset = SDL_FPoint{
                    static_cast<float>(px->valuedouble),
                    static_cast<float>(py->valuedouble)};
            }
        }

        const cJSON* mapFrameJson = cJSON_GetObjectItemCaseSensitive(hud, "mapPlayfieldFrameMarginsPercent");
        if (cJSON_IsObject(mapFrameJson))
        {
            const cJSON* jl = cJSON_GetObjectItemCaseSensitive(mapFrameJson, "left");
            const cJSON* jr = cJSON_GetObjectItemCaseSensitive(mapFrameJson, "right");
            const cJSON* jb = cJSON_GetObjectItemCaseSensitive(mapFrameJson, "bottom");
            if (cJSON_IsNumber(jl) && cJSON_IsNumber(jr) && cJSON_IsNumber(jb))
            {
                MapPlayfieldFrameMarginsPercent margins{};
                margins.left = static_cast<int>(jl->valuedouble);
                margins.right = static_cast<int>(jr->valuedouble);
                margins.bottom = static_cast<int>(jb->valuedouble);
                this->gameSettingsWidget.setMapPlayfieldFrameMarginsPercent(margins, false);
            }
        }
    }

    const cJSON* controls = cJSON_GetObjectItemCaseSensitive(root, "controls");
    if (cJSON_IsObject(controls))
    {
        const cJSON* cam = cJSON_GetObjectItemCaseSensitive(controls, "cameraScrollSpeedSectors");
        if (cJSON_IsNumber(cam))
        {
            this->gameSettingsWidget.setCameraScrollSpeedSectors(static_cast<float>(cam->valuedouble));
        }

        const cJSON* mapZoom = cJSON_GetObjectItemCaseSensitive(controls, "mapWorldZoomFactor");
        if (cJSON_IsNumber(mapZoom))
        {
            Camera& gameCam = GetCamera();
            gameCam.setZoomFactor(static_cast<float>(mapZoom->valuedouble));
            this->zoomWidget.syncSliderToCamera(gameCam);
        }

        const cJSON* binds = cJSON_GetObjectItemCaseSensitive(controls, "bindings");
        if (cJSON_IsObject(binds))
        {
            for (std::size_t i = 0; i < static_cast<std::size_t>(GameSettingsWidget::ControlAction::COUNT); ++i)
            {
                const auto action = static_cast<GameSettingsWidget::ControlAction>(i);
                const char* bk = nullptr;
                switch (action)
                {
                    case GameSettingsWidget::ControlAction::CAMERA_MOVE_UP:
                        bk = "CAMERA_MOVE_UP";
                        break;
                    case GameSettingsWidget::ControlAction::CAMERA_MOVE_DOWN:
                        bk = "CAMERA_MOVE_DOWN";
                        break;
                    case GameSettingsWidget::ControlAction::CAMERA_MOVE_LEFT:
                        bk = "CAMERA_MOVE_LEFT";
                        break;
                    case GameSettingsWidget::ControlAction::CAMERA_MOVE_RIGHT:
                        bk = "CAMERA_MOVE_RIGHT";
                        break;
                    case GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP:
                        bk = "CENTER_CAMERA_ON_SHIP";
                        break;
                    case GameSettingsWidget::ControlAction::TOOLBAR_ATTACK:
                        bk = "TOOLBAR_ATTACK";
                        break;
                    case GameSettingsWidget::ControlAction::TOOLBAR_CANCEL_ATTACK:
                        bk = "TOOLBAR_CANCEL_ATTACK";
                        break;
                    case GameSettingsWidget::ControlAction::TOOLBAR_BOARDING:
                        bk = "TOOLBAR_BOARDING";
                        break;
                    case GameSettingsWidget::ControlAction::TOOLBAR_REPAIR:
                        bk = "TOOLBAR_REPAIR";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_1:
                        bk = "SHORTCUT_1";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_2:
                        bk = "SHORTCUT_2";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_3:
                        bk = "SHORTCUT_3";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_4:
                        bk = "SHORTCUT_4";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_5:
                        bk = "SHORTCUT_5";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_6:
                        bk = "SHORTCUT_6";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_7:
                        bk = "SHORTCUT_7";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_8:
                        bk = "SHORTCUT_8";
                        break;
                    case GameSettingsWidget::ControlAction::SHORTCUT_9:
                        bk = "SHORTCUT_9";
                        break;
                    case GameSettingsWidget::ControlAction::JUMP_MAP:
                        bk = "JUMP_MAP";
                        break;
                    case GameSettingsWidget::ControlAction::FORCE_CLICKED_POSITION_MOVE:
                        bk = "FORCE_CLICKED_POSITION_MOVE";
                        break;
                    case GameSettingsWidget::ControlAction::TOGGLE_MINIMAP:
                        bk = "TOGGLE_MINIMAP";
                        break;
                    case GameSettingsWidget::ControlAction::COUNT:
                    default:
                        continue;
                }

                const cJSON* bv = cJSON_GetObjectItemCaseSensitive(binds, bk);
                if (cJSON_IsNumber(bv))
                {
                    const int sc = bv->valueint;
                    if (sc >= 0 && sc <= 512)
                    {
                        this->gameSettingsWidget.setControlActionScancode(action, static_cast<SDL_Scancode>(sc));
                    }
                }
            }
        }
    }

    const cJSON* graphics = cJSON_GetObjectItemCaseSensitive(root, "graphics");
    if (cJSON_IsObject(graphics))
    {
        const cJSON* hideCoord = cJSON_GetObjectItemCaseSensitive(graphics, "hideCoordinateBackground");
        if (cJSON_IsBool(hideCoord))
        {
            this->gameSettingsWidget.setHideCoordinateBackground(cJSON_IsTrue(hideCoord));
        }

        const cJSON* fog = cJSON_GetObjectItemCaseSensitive(graphics, "fogOfWarEnabled");
        if (cJSON_IsBool(fog))
        {
            this->gameSettingsWidget.setFogOfWarEnabled(cJSON_IsTrue(fog));
        }

        const cJSON* wake = cJSON_GetObjectItemCaseSensitive(graphics, "shipWakeTrailsEnabled");
        if (cJSON_IsBool(wake))
        {
            this->gameSettingsWidget.setShipWakeTrailsEnabled(cJSON_IsTrue(wake));
        }

        const cJSON* hideVfx = cJSON_GetObjectItemCaseSensitive(graphics, "hideOtherPlayersVfx");
        if (cJSON_IsBool(hideVfx))
        {
            this->gameSettingsWidget.setHideOtherPlayersVfxEnabled(cJSON_IsTrue(hideVfx));
        }

        const cJSON* preset = cJSON_GetObjectItemCaseSensitive(graphics, "salvoBulletPreset");
        if (cJSON_IsNumber(preset))
        {
            const int p = preset->valueint;
            if (p >= 0 && p <= 2)
            {
                this->gameSettingsWidget.setSalvoBulletPreset(static_cast<GameSettingsWidget::SalvoBulletPreset>(p));
            }
        }
    }

    cJSON_Delete(root);
}

void IngameHudOverlay::load(void)
{
    this->suppressUserSettingsSave = true;

    this->windowDrawOrder = {
        WindowLayer::CHAT,
        WindowLayer::ESPION,
        WindowLayer::MONEY,
        WindowLayer::PARAMS_MINIMAP,
        WindowLayer::GAME_SETTINGS,
        WindowLayer::ANNOUNCEMENTS,
        WindowLayer::LOG_BOOK,
        WindowLayer::MARKETS_AND_BAZAR,
        WindowLayer::ACCOUNT_MANAGEMENT,
        WindowLayer::CAPTCHA,
        WindowLayer::LEADERBOARD};

    this->backgroundWidget.load();
    this->topBarMenuWidget.load();
    this->scrollBarOverlay.load();

    this->minimapWidget.load();
    this->minimapEspionButtonWidget.load();
    this->minimapParamsButtonWidget.load();
    this->barreActionWidget.load();
    this->centerShipButtonWidget.load();
    this->zoomWidget.load();
    this->zoomWidget.setOnMapWorldZoomCommit(
        [this]()
        {
            if (!this->suppressUserSettingsSave)
            {
                this->saveUserSettingsToDisk();
            }
        });
    this->hpBarWidget.load();
    this->experienceBarWidget.load();
    this->sectorCoordinateOverlay.load();

    this->chatWidget.load();
    this->espionSearchPlayerWidget.load();
    this->moneyWidget.load();
    this->paramsMinimapWidget.load();
    this->gameSettingsWidget.load();
    this->announcementsWidget.load();
    this->logBookWidget.load();
    this->marketsAndBazarWidget.load();
    this->accountManagementWidget.load();
    this->captchaWidget.load();
    this->captchaWidget.show();
    this->leaderboardWidget.load();
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::MINIMAP, this->minimapWidget.getUiScale());
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR, this->experienceBarWidget.getUiScale());
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::HP_BAR, this->hpBarWidget.getUiScale());
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::MAP_ZOOM, this->zoomWidget.getUiScale());
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::CENTER_SHIP, this->centerShipButtonWidget.getUiScale());
    this->gameSettingsWidget.setHudScaleValue(GameSettingsWidget::HudScaleTarget::ACTION_BAR, this->barreActionWidget.getUiScale());
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::MINIMAP, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP));
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR));
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::HP_BAR, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::HP_BAR));
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::MAP_ZOOM, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM));
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::CENTER_SHIP, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::CENTER_SHIP));
    this->gameSettingsWidget.setHudVisibilityValue(GameSettingsWidget::HudScaleTarget::ACTION_BAR, this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::ACTION_BAR));
    this->gameSettingsWidget.setOnHudScaleChanged(
        [this](GameSettingsWidget::HudScaleTarget target, float scale)
        {
            switch (target)
            {
                case GameSettingsWidget::HudScaleTarget::MINIMAP:
                    this->minimapWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
                    this->experienceBarWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::HP_BAR:
                    this->hpBarWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
                    this->zoomWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
                    this->centerShipButtonWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
                    this->barreActionWidget.setUiScale(scale);
                    break;
                case GameSettingsWidget::HudScaleTarget::COUNT:
                default:
                    break;
            }

            if (!this->suppressUserSettingsSave)
            {
                this->saveUserSettingsToDisk();
            }
        });
    this->gameSettingsWidget.setOnHudVisibilityChanged(
        [this](GameSettingsWidget::HudScaleTarget target, bool visible)
        {
            const std::size_t index = static_cast<std::size_t>(target);
            if (index >= this->hudWidgetVisibility.size())
            {
                return;
            }

            this->hudWidgetVisibility[index] = visible;
            if (!this->suppressUserSettingsSave)
            {
                this->saveUserSettingsToDisk();
            }
        });
    this->gameSettingsWidget.setOnStartUiConfiguratorRequested(
        [this]()
        {
            this->startHudConfiguratorMode();
        });
    this->tooltipFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        13.0f);

    this->marketsAndBazarWidget.openBasicMarket();

    this->gameSettingsWidget.setOnUserSettingsChanged(
        [this]()
        {
            if (!this->suppressUserSettingsSave)
            {
                this->saveUserSettingsToDisk();
            }
        });

    this->loadUserSettingsFromDisk();
    this->suppressUserSettingsSave = false;

    this->prevChatVisible = this->chatWidget.isVisible();
    this->prevEspionVisible = this->espionSearchPlayerWidget.isVisible();
    this->prevMoneyVisible = this->moneyWidget.isVisible();
    this->prevParamsMiniMapVisible = this->paramsMinimapWidget.isVisible();
    this->prevGameSettingsVisible = this->gameSettingsWidget.isVisible();
    this->prevAnnouncementsVisible = this->announcementsWidget.isVisible();
    this->prevLogBookVisible = this->logBookWidget.isVisible();
    this->prevMarketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();
    this->prevAccountManagementVisible = this->accountManagementWidget.isVisible();
    this->prevCaptchaVisible = this->captchaWidget.isVisible();
    this->prevLeaderboardVisible = this->leaderboardWidget.isVisible();
    this->hoveredMinimapTooltip = MinimapWidget::Tooltip::NONE;
    this->hoveredMinimapTooltipMouseX = 0.0f;
    this->hoveredMinimapTooltipMouseY = 0.0f;
    this->hudConfiguratorMode = false;
    this->hudConfiguratorRestoreGameSettingsVisibility = false;
    this->hudConfiguratorDragging = false;
    this->hudConfiguratorPanelDragging = false;
    this->hudConfiguratorPanelOffset = SDL_FPoint{0.0f, 0.0f};
}

void IngameHudOverlay::unload(void)
{
    ResetStorageFontRef(&this->tooltipFont);
    this->captchaWidget.unload();
    this->leaderboardWidget.unload();
    this->accountManagementWidget.unload();
    this->marketsAndBazarWidget.unload();
    this->logBookWidget.unload();
    this->announcementsWidget.unload();
    this->gameSettingsWidget.unload();
    this->paramsMinimapWidget.unload();
    this->moneyWidget.unload();
    this->espionSearchPlayerWidget.unload();
    this->chatWidget.unload();
    this->experienceBarWidget.unload();
    this->hpBarWidget.unload();
    this->sectorCoordinateOverlay.unload();
    this->centerShipButtonWidget.unload();
    this->barreActionWidget.unload();
    this->minimapParamsButtonWidget.unload();
    this->minimapEspionButtonWidget.unload();
    this->minimapWidget.unload();
    this->zoomWidget.unload();
    this->tileClickMarkerOverlay.hide();
    this->scrollBarOverlay.unload();
    this->topBarMenuWidget.unload();
    this->backgroundWidget.unload();
}

void IngameHudOverlay::update(double dt, Camera& camera, Map& map)
{
    this->syncWindowOrderOnOpen();
    this->syncTopBarActionState();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    this->hoveredMinimapTooltip = MinimapWidget::Tooltip::NONE;
    this->hoveredMinimapTooltipMouseX = mouseX;
    this->hoveredMinimapTooltipMouseY = mouseY;
    const bool keepDefaultCursor = this->shouldKeepDefaultCursorAfterClose(mouseX, mouseY);
    const bool hoveredTopBar = this->topBarMenuWidget.update(mouseX, mouseY);

    for (const WindowLayer layer : this->windowDrawOrder)
    {
        switch (layer)
        {
            case WindowLayer::CHAT:
                this->chatWidget.update(dt);
                break;
            case WindowLayer::ESPION:
                this->espionSearchPlayerWidget.update(dt);
                break;
            case WindowLayer::MONEY:
                this->moneyWidget.update(dt);
                break;
            case WindowLayer::PARAMS_MINIMAP:
                this->paramsMinimapWidget.update(dt);
                break;
            case WindowLayer::GAME_SETTINGS:
                this->gameSettingsWidget.update(dt);
                break;
            case WindowLayer::ANNOUNCEMENTS:
                this->announcementsWidget.update(dt);
                break;
            case WindowLayer::LOG_BOOK:
                this->logBookWidget.update(dt);
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                this->marketsAndBazarWidget.update(dt);
                break;
            case WindowLayer::ACCOUNT_MANAGEMENT:
                this->accountManagementWidget.update(dt);
                break;
            case WindowLayer::CAPTCHA:
                this->captchaWidget.update(dt);
                break;
            case WindowLayer::LEADERBOARD:
                this->leaderboardWidget.update(dt);
                break;
        }
    }
    this->tileClickMarkerOverlay.update(dt);
    if (!this->hudConfiguratorMode &&
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP))
    {
        this->minimapWidget.update(camera, map);
    }
    if (!this->hudConfiguratorMode &&
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM))
    {
        this->zoomWidget.update(camera);
    }
    this->updateHudConfigurator(mouseX, mouseY);

    const bool minimapVisible = this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP);
    const bool zoomVisible = this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM);
    const bool centerShipVisible = this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::CENTER_SHIP);
    const bool minimapHovered =
        minimapVisible &&
        this->minimapWidget.containsContentPoint(mouseX, mouseY);
    const bool minimapEspionHovered =
        minimapVisible &&
        this->minimapEspionButtonWidget.containsPoint(mouseX, mouseY, this->minimapWidget);
    const bool minimapParamsHovered =
        minimapVisible &&
        this->minimapParamsButtonWidget.containsPoint(mouseX, mouseY, this->minimapWidget);
    const HudCursorType minimapEspionCursor =
        minimapVisible
            ? this->minimapEspionButtonWidget.getDesiredCursor(mouseX, mouseY, this->minimapWidget)
            : HudCursorType::NONE;
    const HudCursorType minimapParamsCursor =
        minimapVisible
            ? this->minimapParamsButtonWidget.getDesiredCursor(mouseX, mouseY, this->minimapWidget)
            : HudCursorType::NONE;
    const HudCursorType centerShipCursor =
        centerShipVisible
            ? this->centerShipButtonWidget.getDesiredCursor(mouseX, mouseY)
            : HudCursorType::NONE;

    if (minimapEspionHovered)
    {
        this->hoveredMinimapTooltip = MinimapWidget::Tooltip::ESPION;
    }
    else if (minimapParamsHovered)
    {
        this->hoveredMinimapTooltip = MinimapWidget::Tooltip::PARAMS_MINIMAP;
    }

    HudCursorType desiredCursor = HudCursorType::DEFAULT;
    if (!keepDefaultCursor)
    {
        if (this->hudConfiguratorMode)
        {
            desiredCursor = this->getHudConfiguratorDesiredCursor(mouseX, mouseY);
        }
        else
        {
            desiredCursor = HudCursorType::NONE;

            for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
            {
                const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
                if (!this->isWindowLayerVisible(layer))
                {
                    continue;
                }

                desiredCursor = this->getWindowLayerDesiredCursor(layer, mouseX, mouseY);
                if (desiredCursor != HudCursorType::NONE)
                {
                    break;
                }
            }

            if (desiredCursor == HudCursorType::NONE)
            {
                if (zoomVisible && this->zoomWidget.isSliderHovered(mouseX, mouseY))
                {
                    desiredCursor = HudCursorType::RESIZE_HORIZONTAL;
                }
                else if (minimapEspionCursor != HudCursorType::NONE)
                {
                    desiredCursor = minimapEspionCursor;
                }
                else if (minimapParamsCursor != HudCursorType::NONE)
                {
                    desiredCursor = minimapParamsCursor;
                }
                else if (minimapHovered)
                {
                    desiredCursor = HudCursorType::POINTER;
                }
                else if (centerShipCursor != HudCursorType::NONE)
                {
                    desiredCursor = centerShipCursor;
                }
                else if (hoveredTopBar)
                {
                    desiredCursor = HudCursorType::POINTER;
                }
                else
                {
                    desiredCursor = HudCursorType::DEFAULT;
                }
            }
        }
    }

    applyHudCursor(desiredCursor);
    if (this->gameSettingsWidget.getHideCoordinateBackground())
    {
        this->scrollBarOverlay.clearInteraction();
    }
    else
    {
        this->scrollBarOverlay.update(
            dt,
            camera,
            map,
            map.rect,
            this->gameSettingsWidget.getCameraScrollSpeedSectors());
    }
}

void IngameHudOverlay::drawBackgroundWidget(void)
{
    this->backgroundWidget.draw();
}

void IngameHudOverlay::drawWidgets(const Map& map, const Player& player)
{
    const bool showMinimap =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP) ||
        this->hudConfiguratorMode;
    const bool showActionBar =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::ACTION_BAR) ||
        this->hudConfiguratorMode;
    const bool showZoom =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM) ||
        this->hudConfiguratorMode;
    const bool showExperienceBar =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR) ||
        this->hudConfiguratorMode;
    const bool showHpBar =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::HP_BAR) ||
        this->hudConfiguratorMode;
    const bool showCenterShip =
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::CENTER_SHIP) ||
        this->hudConfiguratorMode;

    this->topBarMenuWidget.draw();
    this->sectorCoordinateOverlay.draw(map, player);
    if (showMinimap)
    {
        this->minimapWidget.draw(map);
        this->minimapParamsButtonWidget.draw(this->minimapWidget);
        this->minimapEspionButtonWidget.draw(this->minimapWidget);
        if (!this->hudConfiguratorMode)
        {
            this->drawHoveredMinimapTooltip();
        }
    }
    if (showActionBar)
    {
        this->barreActionWidget.draw();
    }
    if (showZoom)
    {
        this->zoomWidget.draw();
    }
    if (showExperienceBar)
    {
        this->experienceBarWidget.draw();
    }
    if (showHpBar)
    {
        this->hpBarWidget.draw();
    }
    if (showCenterShip)
    {
        this->centerShipButtonWidget.draw();
    }

    for (const WindowLayer layer : this->windowDrawOrder)
    {
        switch (layer)
        {
            case WindowLayer::CHAT:
                this->chatWidget.draw();
                break;
            case WindowLayer::ESPION:
                this->espionSearchPlayerWidget.draw();
                break;
            case WindowLayer::MONEY:
                this->moneyWidget.draw();
                break;
            case WindowLayer::PARAMS_MINIMAP:
                this->paramsMinimapWidget.draw();
                break;
            case WindowLayer::GAME_SETTINGS:
                this->gameSettingsWidget.draw();
                break;
            case WindowLayer::ANNOUNCEMENTS:
                this->announcementsWidget.draw();
                break;
            case WindowLayer::LOG_BOOK:
                this->logBookWidget.draw();
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                this->marketsAndBazarWidget.draw();
                break;
            case WindowLayer::ACCOUNT_MANAGEMENT:
                this->accountManagementWidget.draw();
                break;
            case WindowLayer::CAPTCHA:
                this->captchaWidget.draw();
                break;
            case WindowLayer::LEADERBOARD:
                this->leaderboardWidget.draw();
                break;
        }
    }

    if (this->hudConfiguratorMode)
    {
        this->drawHudConfiguratorOverlay();
    }
    else if (this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM))
    {
        this->zoomWidget.drawTooltip();
    }
}

void IngameHudOverlay::drawHoveredMinimapTooltip(void) const
{
    if (this->hoveredMinimapTooltip == MinimapWidget::Tooltip::NONE || this->tooltipFont.sdl_font == nullptr)
    {
        return;
    }

    const char* label =
        (this->hoveredMinimapTooltip == MinimapWidget::Tooltip::ESPION)
            ? "Espion"
            : "Params minimap";
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float textWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    const float textHeight = measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    SDL_FRect tooltipRect = SDL_FRect{
        this->hoveredMinimapTooltipMouseX + kHudTooltipOffsetX,
        this->hoveredMinimapTooltipMouseY + kHudTooltipOffsetY,
        textWidth + (kHudTooltipPaddingX * 2.0f),
        textHeight + (kHudTooltipPaddingY * 2.0f)
    };

    const float maxX = (gameScreenRect.x + gameScreenRect.w) - tooltipRect.w;
    const float maxY = (gameScreenRect.y + gameScreenRect.h) - tooltipRect.h;
    tooltipRect.x = (std::max)(gameScreenRect.x, (std::min)(tooltipRect.x, maxX));
    tooltipRect.y = (std::max)(gameScreenRect.y, (std::min)(tooltipRect.y, maxY));

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kHudTooltipFill);
    rc2d_graphics_rectangle("fill", &tooltipRect);
    rc2d_graphics_setColor(kHudTooltipBorder);
    rc2d_graphics_rectangle("line", &tooltipRect);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        label,
        tooltipRect.x + kHudTooltipPaddingX,
        tooltipRect.y + kHudTooltipPaddingY,
        kHudTooltipText);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void IngameHudOverlay::drawTileClickMarkerOverlay(const Map& map)
{
    this->tileClickMarkerOverlay.draw(map);
}

void IngameHudOverlay::drawScrollBarOverlay(const Map& map)
{
    this->scrollBarOverlay.draw(
        map.rect,
        map,
        !this->gameSettingsWidget.getHideCoordinateBackground());
}

bool IngameHudOverlay::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    if (this->hudConfiguratorMode)
    {
        (void)clicks;
        (void)mouseID;
        return this->handleHudConfiguratorMousePressed(x, y, button);
    }

    for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
    {
        const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
        const bool wasVisible = this->isWindowLayerVisible(layer);
        bool consumed = false;

        switch (layer)
        {
            case WindowLayer::CHAT:
                consumed = this->chatWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::ESPION:
                consumed = this->espionSearchPlayerWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::MONEY:
                consumed = this->moneyWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::PARAMS_MINIMAP:
                consumed = this->paramsMinimapWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::GAME_SETTINGS:
                consumed = this->gameSettingsWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::ANNOUNCEMENTS:
                consumed = this->announcementsWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::LOG_BOOK:
                consumed = this->logBookWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                consumed = this->marketsAndBazarWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::ACCOUNT_MANAGEMENT:
                consumed = this->accountManagementWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::CAPTCHA:
                consumed = this->captchaWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::LEADERBOARD:
                consumed = this->leaderboardWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
        }

        if (!consumed)
        {
            continue;
        }

        const bool isStillVisible = this->isWindowLayerVisible(layer);
        if (wasVisible && !isStillVisible)
        {
            this->beginCursorResetAfterClose(x, y);
        }

        if (button == RC2D_MOUSE_BUTTON_LEFT && isStillVisible)
        {
            this->bringWindowToFront(layer);
        }

        if (layer == WindowLayer::CHAT)
        {
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        else if (layer == WindowLayer::ESPION)
        {
            this->chatWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        else if (layer == WindowLayer::MARKETS_AND_BAZAR)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        else if (layer == WindowLayer::ACCOUNT_MANAGEMENT)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        else if (layer == WindowLayer::GAME_SETTINGS)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        else if (layer == WindowLayer::CAPTCHA)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
        }
        else
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
            this->gameSettingsWidget.clearFocus();
            this->captchaWidget.clearFocus();
        }
        return true;
    }

    const TopBarMenuWidget::Action topBarAction = this->topBarMenuWidget.mousepressed(x, y, button);
    if (topBarAction != TopBarMenuWidget::Action::NONE)
    {
        this->handleTopBarAction(topBarAction);
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        this->gameSettingsWidget.clearFocus();
        this->captchaWidget.clearFocus();
        return true;
    }

    if (this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP) &&
        this->minimapEspionButtonWidget.mousepressed(x, y, button, this->minimapWidget))
    {
        if (this->espionSearchPlayerWidget.isVisible())
        {
            this->espionSearchPlayerWidget.hide();
        }
        else
        {
            this->espionSearchPlayerWidget.show();
            this->bringWindowToFront(WindowLayer::ESPION);
        }
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        this->gameSettingsWidget.clearFocus();
        this->captchaWidget.clearFocus();
        return true;
    }

    if (this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP) &&
        this->minimapParamsButtonWidget.mousepressed(x, y, button, this->minimapWidget))
    {
        if (this->paramsMinimapWidget.isVisible())
        {
            this->paramsMinimapWidget.hide();
        }
        else
        {
            this->paramsMinimapWidget.show();
            this->bringWindowToFront(WindowLayer::PARAMS_MINIMAP);
        }
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        this->gameSettingsWidget.clearFocus();
        this->captchaWidget.clearFocus();
        return true;
    }

    if (this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MAP_ZOOM) &&
        this->zoomWidget.mousepressed(x, y, button))
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        this->gameSettingsWidget.clearFocus();
        this->captchaWidget.clearFocus();
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        this->gameSettingsWidget.clearFocus();
        this->captchaWidget.clearFocus();
    }
    return false;
}

bool IngameHudOverlay::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    if (this->hudConfiguratorMode)
    {
        return true;
    }

    for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
    {
        const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
        switch (layer)
        {
            case WindowLayer::ANNOUNCEMENTS:
                if (this->announcementsWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::LOG_BOOK:
                if (this->logBookWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::MONEY:
                if (this->moneyWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                if (this->marketsAndBazarWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::GAME_SETTINGS:
                if (this->gameSettingsWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::ACCOUNT_MANAGEMENT:
                if (this->accountManagementWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::CHAT:
                if (this->chatWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}

bool IngameHudOverlay::handleMapOverlayMousePressed(float x, float y, RC2D_MouseButton button, Camera& camera, Map& map)
{
    if (this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::MINIMAP) &&
        this->minimapWidget.mousepressed(x, y, button, camera, map))
    {
        return true;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }
    if (this->gameSettingsWidget.getHideCoordinateBackground())
    {
        return false;
    }
    return this->scrollBarOverlay.handleClick(x, y, map.rect);
}

bool IngameHudOverlay::centerShipButtonMousepressed(float x, float y, RC2D_MouseButton button) const
{
    return (
        button == RC2D_MOUSE_BUTTON_LEFT &&
        this->isHudWidgetVisible(GameSettingsWidget::HudScaleTarget::CENTER_SHIP) &&
        this->centerShipButtonWidget.containsPoint(x, y));
}

void IngameHudOverlay::notifyMapTileClicked(int tileX, int tileY)
{
    this->tileClickMarkerOverlay.show(tileX, tileY);
}

bool IngameHudOverlay::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    if (this->hudConfiguratorMode)
    {
        if (scancode == SDL_SCANCODE_ESCAPE)
        {
            this->stopHudConfiguratorMode();
        }

        (void)key;
        (void)keycode;
        (void)mod;
        (void)isrepeat;
        return true;
    }

    for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
    {
        const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
        switch (layer)
        {
            case WindowLayer::ESPION:
                if (this->espionSearchPlayerWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                if (this->marketsAndBazarWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            case WindowLayer::ACCOUNT_MANAGEMENT:
                if (this->accountManagementWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            case WindowLayer::GAME_SETTINGS:
                if (this->gameSettingsWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            case WindowLayer::CAPTCHA:
                if (this->captchaWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            case WindowLayer::CHAT:
                if (this->chatWidget.keypressed(key, scancode, keycode, mod, isrepeat))
                {
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}

bool IngameHudOverlay::isBlockingGameplayKeyboardInput(void) const
{
    if (this->chatWidget.hasBlockingTextInputFocus())
    {
        return true;
    }
    if (this->espionSearchPlayerWidget.hasBlockingTextInputFocus())
    {
        return true;
    }
    if (this->marketsAndBazarWidget.hasBlockingOfferInputFocus())
    {
        return true;
    }
    if (this->accountManagementWidget.hasBlockingProfileNameInputFocus())
    {
        return true;
    }
    if (this->gameSettingsWidget.hasBlockingRedeemInputFocus())
    {
        return true;
    }
    if (this->captchaWidget.hasBlockingTextInputFocus())
    {
        return true;
    }
    return false;
}

void IngameHudOverlay::handleTopBarAction(TopBarMenuWidget::Action action)
{
    switch (action)
    {
        case TopBarMenuWidget::Action::PIRATE_EXAM:
            // Point d'entree reserve a la future GUI "Examen pirate".
            break;
        case TopBarMenuWidget::Action::CHAT:
            if (this->chatWidget.isVisible())
            {
                this->chatWidget.hide();
            }
            else
            {
                this->chatWidget.show();
                this->bringWindowToFront(WindowLayer::CHAT);
            }
            break;
        case TopBarMenuWidget::Action::SHIP:
            if (this->accountManagementWidget.isVisible())
            {
                this->accountManagementWidget.hide();
            }
            else
            {
                this->accountManagementWidget.openShipManagement();
                this->bringWindowToFront(WindowLayer::ACCOUNT_MANAGEMENT);
            }
            break;
        case TopBarMenuWidget::Action::ANNOUNCEMENT:
            if (this->announcementsWidget.isVisible())
            {
                this->announcementsWidget.hide();
            }
            else
            {
                this->announcementsWidget.show();
                this->bringWindowToFront(WindowLayer::ANNOUNCEMENTS);
            }
            break;
        case TopBarMenuWidget::Action::MONEY:
            if (this->moneyWidget.isVisible())
            {
                this->moneyWidget.hide();
            }
            else
            {
                this->moneyWidget.show();
                this->bringWindowToFront(WindowLayer::MONEY);
            }
            break;
        case TopBarMenuWidget::Action::LOGBOOK:
            if (this->logBookWidget.isVisible())
            {
                this->logBookWidget.hide();
            }
            else
            {
                this->logBookWidget.show();
                this->bringWindowToFront(WindowLayer::LOG_BOOK);
            }
            break;
        case TopBarMenuWidget::Action::SETTINGS:
            if (this->gameSettingsWidget.isVisible())
            {
                this->gameSettingsWidget.hide();
            }
            else
            {
                this->gameSettingsWidget.show();
                this->bringWindowToFront(WindowLayer::GAME_SETTINGS);
            }
            break;
        case TopBarMenuWidget::Action::LEADERBOARD:
            if (this->leaderboardWidget.isVisible())
            {
                this->leaderboardWidget.hide();
            }
            else
            {
                this->leaderboardWidget.show();
                this->bringWindowToFront(WindowLayer::LEADERBOARD);
            }
            break;
        case TopBarMenuWidget::Action::GUILD:
        case TopBarMenuWidget::Action::QUEST:
        case TopBarMenuWidget::Action::DISCONNECT:
        case TopBarMenuWidget::Action::NONE:
        default:
            break;
    }
}

void IngameHudOverlay::syncTopBarActionState(void)
{
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::PIRATE_EXAM, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::CHAT, this->chatWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::GUILD, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::QUEST, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::LEADERBOARD, this->leaderboardWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::MONEY, this->moneyWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::SHIP, this->accountManagementWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::ANNOUNCEMENT, this->announcementsWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::LOGBOOK, this->logBookWidget.isVisible());
    this->topBarMenuWidget.setActionActive(
        TopBarMenuWidget::Action::SETTINGS,
        this->gameSettingsWidget.isVisible() || this->hudConfiguratorMode);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::DISCONNECT, false);
}
