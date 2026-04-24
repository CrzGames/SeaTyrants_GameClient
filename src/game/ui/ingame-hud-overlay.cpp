#include "game/ui/ingame-hud-overlay.h"

#include "core/context.h"

#include <algorithm>
#include <cmath>

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

IngameHudOverlay::IngameHudOverlay(void)
    : backgroundWidget{},
      topBarMenuWidget{},
      sectorCoordinateOverlay{},
      tileClickMarkerOverlay{},
      scrollBarOverlay{},
      minimapWidget{},
      barreActionWidget{},
      centerShipButtonWidget{},
      zoomWidget{},
      chatWidget{},
      espionSearchPlayerWidget{},
      paramsMinimapWidget{},
      announcementsWidget{},
      logBookWidget{},
      marketsAndBazarWidget{},
      accountManagementWidget{},
      windowDrawOrder{},
      prevChatVisible(false),
      prevEspionVisible(false),
      prevParamsMiniMapVisible(false),
      prevAnnouncementsVisible(false),
      prevLogBookVisible(false),
      prevMarketsAndBazarVisible(false),
      prevAccountManagementVisible(false),
      keepDefaultCursorAfterClose(false),
      keepDefaultCursorMouseX(0.0f),
      keepDefaultCursorMouseY(0.0f)
{
}

IngameHudOverlay::~IngameHudOverlay(void)
{
}

void IngameHudOverlay::bringWindowToFront(WindowLayer layer)
{
    const auto it = std::find(this->windowDrawOrder.begin(), this->windowDrawOrder.end(), layer);
    if (it == this->windowDrawOrder.end())
    {
        return;
    }

    const WindowLayer target = *it;
    this->windowDrawOrder.erase(it);
    this->windowDrawOrder.push_back(target);
}

bool IngameHudOverlay::isWindowLayerVisible(WindowLayer layer) const
{
    switch (layer)
    {
        case WindowLayer::CHAT:
            return this->chatWidget.isVisible();
        case WindowLayer::ESPION:
            return this->espionSearchPlayerWidget.isVisible();
        case WindowLayer::PARAMS_MINIMAP:
            return this->paramsMinimapWidget.isVisible();
        case WindowLayer::ANNOUNCEMENTS:
            return this->announcementsWidget.isVisible();
        case WindowLayer::LOG_BOOK:
            return this->logBookWidget.isVisible();
        case WindowLayer::MARKETS_AND_BAZAR:
            return this->marketsAndBazarWidget.isVisible();
        case WindowLayer::ACCOUNT_MANAGEMENT:
            return this->accountManagementWidget.isVisible();
        default:
            return false;
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
        case WindowLayer::PARAMS_MINIMAP:
            return this->paramsMinimapWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::ANNOUNCEMENTS:
            return this->announcementsWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::LOG_BOOK:
            return this->logBookWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::MARKETS_AND_BAZAR:
            return this->marketsAndBazarWidget.getDesiredCursor(mouseX, mouseY);
        case WindowLayer::ACCOUNT_MANAGEMENT:
            return this->accountManagementWidget.getDesiredCursor(mouseX, mouseY);
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
    const bool paramsVisible = this->paramsMinimapWidget.isVisible();
    const bool announcementsVisible = this->announcementsWidget.isVisible();
    const bool logBookVisible = this->logBookWidget.isVisible();
    const bool marketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();
    const bool accountManagementVisible = this->accountManagementWidget.isVisible();

    if (chatVisible && !this->prevChatVisible)
    {
        this->bringWindowToFront(WindowLayer::CHAT);
    }
    if (espionVisible && !this->prevEspionVisible)
    {
        this->bringWindowToFront(WindowLayer::ESPION);
    }
    if (paramsVisible && !this->prevParamsMiniMapVisible)
    {
        this->bringWindowToFront(WindowLayer::PARAMS_MINIMAP);
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

    this->prevChatVisible = chatVisible;
    this->prevEspionVisible = espionVisible;
    this->prevParamsMiniMapVisible = paramsVisible;
    this->prevAnnouncementsVisible = announcementsVisible;
    this->prevLogBookVisible = logBookVisible;
    this->prevMarketsAndBazarVisible = marketsAndBazarVisible;
    this->prevAccountManagementVisible = accountManagementVisible;
}

void IngameHudOverlay::load(void)
{
    this->backgroundWidget.load();
    this->topBarMenuWidget.load();
    this->scrollBarOverlay.load();

    this->minimapWidget.load();
    this->barreActionWidget.load();
    this->centerShipButtonWidget.load();
    this->zoomWidget.load();
    this->sectorCoordinateOverlay.load();

    this->chatWidget.load();
    this->espionSearchPlayerWidget.load();
    this->paramsMinimapWidget.load();
    this->announcementsWidget.load();
    this->logBookWidget.load();
    this->marketsAndBazarWidget.load();
    this->accountManagementWidget.load();

    this->espionSearchPlayerWidget.show();
    this->paramsMinimapWidget.show();
    this->marketsAndBazarWidget.openBasicMarket();

    this->windowDrawOrder = {
        WindowLayer::CHAT,
        WindowLayer::ESPION,
        WindowLayer::PARAMS_MINIMAP,
        WindowLayer::ANNOUNCEMENTS,
        WindowLayer::LOG_BOOK,
        WindowLayer::MARKETS_AND_BAZAR,
        WindowLayer::ACCOUNT_MANAGEMENT
    };
    this->prevChatVisible = this->chatWidget.isVisible();
    this->prevEspionVisible = this->espionSearchPlayerWidget.isVisible();
    this->prevParamsMiniMapVisible = this->paramsMinimapWidget.isVisible();
    this->prevAnnouncementsVisible = this->announcementsWidget.isVisible();
    this->prevLogBookVisible = this->logBookWidget.isVisible();
    this->prevMarketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();
    this->prevAccountManagementVisible = this->accountManagementWidget.isVisible();
}

void IngameHudOverlay::unload(void)
{
    this->accountManagementWidget.unload();
    this->marketsAndBazarWidget.unload();
    this->logBookWidget.unload();
    this->announcementsWidget.unload();
    this->paramsMinimapWidget.unload();
    this->espionSearchPlayerWidget.unload();
    this->chatWidget.unload();
    this->sectorCoordinateOverlay.unload();
    this->centerShipButtonWidget.unload();
    this->barreActionWidget.unload();
    this->minimapWidget.unload();
    this->zoomWidget.unload();
    this->tileClickMarkerOverlay.hide();
    this->scrollBarOverlay.unload();
    this->topBarMenuWidget.unload();
    this->backgroundWidget.unload();
}

void IngameHudOverlay::update(double dt, Camera& camera, const Map& map)
{
    this->syncWindowOrderOnOpen();
    this->syncTopBarActionState();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
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
            case WindowLayer::PARAMS_MINIMAP:
                this->paramsMinimapWidget.update(dt);
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
        }
    }
    this->tileClickMarkerOverlay.update(dt);
    this->zoomWidget.update(camera);

    HudCursorType desiredCursor = HudCursorType::DEFAULT;
    if (!keepDefaultCursor)
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
            if (this->zoomWidget.isDraggingSlider() || this->zoomWidget.isSliderHovered(mouseX, mouseY))
            {
                desiredCursor = HudCursorType::RESIZE_HORIZONTAL;
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

    applyHudCursor(desiredCursor);
    this->scrollBarOverlay.update(dt, camera, map, map.rect);
}

void IngameHudOverlay::drawBackgroundWidget(void)
{
    this->backgroundWidget.draw();
}

void IngameHudOverlay::drawWidgets(const Map& map, const Player& player)
{
    this->topBarMenuWidget.draw();
    this->sectorCoordinateOverlay.draw(map, player);
    this->minimapWidget.draw();
    this->barreActionWidget.draw();
    this->centerShipButtonWidget.draw();
    this->zoomWidget.draw();

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
            case WindowLayer::PARAMS_MINIMAP:
                this->paramsMinimapWidget.draw();
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
        }
    }
}

void IngameHudOverlay::drawTileClickMarkerOverlay(const Map& map)
{
    this->tileClickMarkerOverlay.draw(map);
}

void IngameHudOverlay::drawScrollBarOverlay(const Map& map)
{
    this->scrollBarOverlay.draw(map.rect, map);
}

bool IngameHudOverlay::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
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
            case WindowLayer::PARAMS_MINIMAP:
                consumed = this->paramsMinimapWidget.mousepressed(x, y, button, clicks, mouseID);
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
        }
        else if (layer == WindowLayer::ESPION)
        {
            this->chatWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
        }
        else if (layer == WindowLayer::MARKETS_AND_BAZAR)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
        }
        else if (layer == WindowLayer::ACCOUNT_MANAGEMENT)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
        }
        else
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
            this->accountManagementWidget.clearFocus();
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
        return true;
    }

    if (this->zoomWidget.mousepressed(x, y, button))
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
        this->accountManagementWidget.clearFocus();
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
            case WindowLayer::MARKETS_AND_BAZAR:
                if (this->marketsAndBazarWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
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

bool IngameHudOverlay::handleMapOverlayMousePressed(float x, float y, RC2D_MouseButton button, const Map& map)
{
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }
    return this->scrollBarOverlay.handleClick(x, y, map.rect);
}

void IngameHudOverlay::notifyMapTileClicked(int tileX, int tileY)
{
    this->tileClickMarkerOverlay.show(tileX, tileY);
}

bool IngameHudOverlay::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
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

void IngameHudOverlay::handleTopBarAction(TopBarMenuWidget::Action action)
{
    switch (action)
    {
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
        case TopBarMenuWidget::Action::GUILD:
        case TopBarMenuWidget::Action::QUEST:
        case TopBarMenuWidget::Action::LEADERBOARD:
        case TopBarMenuWidget::Action::MONEY:
        case TopBarMenuWidget::Action::SETTINGS:
        case TopBarMenuWidget::Action::DISCONNECT:
        case TopBarMenuWidget::Action::NONE:
        default:
            break;
    }
}

void IngameHudOverlay::syncTopBarActionState(void)
{
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::CHAT, this->chatWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::GUILD, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::QUEST, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::LEADERBOARD, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::MONEY, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::SHIP, this->accountManagementWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::ANNOUNCEMENT, this->announcementsWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::LOGBOOK, this->logBookWidget.isVisible());
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::SETTINGS, false);
    this->topBarMenuWidget.setActionActive(TopBarMenuWidget::Action::DISCONNECT, false);
}

void IngameHudOverlay::publishAnnouncementRow(const std::string& rowText)
{
    this->announcementsWidget.publishAnnouncementRow(rowText);
}

void IngameHudOverlay::publishEspionSearchResult(const std::string& resultText)
{
    this->espionSearchPlayerWidget.publishSearchResult(resultText);
}

void IngameHudOverlay::publishLogbookRow(const LogBookWidget::LogBookRow& row)
{
    this->logBookWidget.publishLogBookRow(row);
}

void IngameHudOverlay::setBazarRows(const std::vector<MarketsAndBazarWidget::BazarRow>& rows)
{
    this->marketsAndBazarWidget.setBazarRows(rows);
}

void IngameHudOverlay::setBlackMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows)
{
    this->marketsAndBazarWidget.setBlackMarketRows(rows);
}

void IngameHudOverlay::setBasicMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows)
{
    this->marketsAndBazarWidget.setBasicMarketRows(rows);
}

void IngameHudOverlay::setEventMarketRows(const std::vector<MarketsAndBazarWidget::MarketRow>& rows)
{
    this->marketsAndBazarWidget.setEventMarketRows(rows);
}
