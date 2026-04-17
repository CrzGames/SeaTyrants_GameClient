#include "game/ui/ingame-hud-overlay.h"

#include "core/context.h"
#include <algorithm>

static void setCursorArrow(void)
{
    static SDL_Cursor* c = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    if (c != nullptr)
    {
        SDL_SetCursor(c);
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
      sectorCoordinateOverlay{},
      tileClickMarkerOverlay{},
      scrollBarOverlay{},
      minimapWidget{},
      barreActionWidget{},
      centerShipButtonWidget{},
      chatWidget{},
      espionSearchPlayerWidget{},
      paramsMinimapWidget{},
      announcementsWidget{},
      logBookWidget{},
      marketsAndBazarWidget{},
      windowDrawOrder{},
      prevChatVisible(false),
      prevEspionVisible(false),
      prevParamsMiniMapVisible(false),
      prevAnnouncementsVisible(false),
      prevLogBookVisible(false),
      prevMarketsAndBazarVisible(false)
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

void IngameHudOverlay::syncWindowOrderOnOpen(void)
{
    const bool chatVisible = this->chatWidget.isVisible();
    const bool espionVisible = this->espionSearchPlayerWidget.isVisible();
    const bool paramsVisible = this->paramsMinimapWidget.isVisible();
    const bool announcementsVisible = this->announcementsWidget.isVisible();
    const bool logBookVisible = this->logBookWidget.isVisible();
    const bool marketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();

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

    this->prevChatVisible = chatVisible;
    this->prevEspionVisible = espionVisible;
    this->prevParamsMiniMapVisible = paramsVisible;
    this->prevAnnouncementsVisible = announcementsVisible;
    this->prevLogBookVisible = logBookVisible;
    this->prevMarketsAndBazarVisible = marketsAndBazarVisible;
}

void IngameHudOverlay::load(void)
{
    this->backgroundWidget.load();
    this->scrollBarOverlay.load();

    // Widgets HUD extraits dans leurs propres composants.
    this->minimapWidget.load();
    this->barreActionWidget.load();

    // Bouton de recentrage du navire, garde sa logique/ressources dans son widget dedie.
    this->centerShipButtonWidget.load();

    // Overlay texte du secteur courant.
    this->sectorCoordinateOverlay.load();

    // Fenetre chat interactive.
    this->chatWidget.load();
    this->espionSearchPlayerWidget.load();
    this->paramsMinimapWidget.load();
    this->announcementsWidget.load();
    this->logBookWidget.load();
    this->marketsAndBazarWidget.load();

    // Ordre par defaut (bas -> haut), puis etat visible initial.
    this->windowDrawOrder = {
        WindowLayer::CHAT,
        WindowLayer::ESPION,
        WindowLayer::PARAMS_MINIMAP,
        WindowLayer::ANNOUNCEMENTS,
        WindowLayer::LOG_BOOK,
        WindowLayer::MARKETS_AND_BAZAR
    };
    this->prevChatVisible = this->chatWidget.isVisible();
    this->prevEspionVisible = this->espionSearchPlayerWidget.isVisible();
    this->prevParamsMiniMapVisible = this->paramsMinimapWidget.isVisible();
    this->prevAnnouncementsVisible = this->announcementsWidget.isVisible();
    this->prevLogBookVisible = this->logBookWidget.isVisible();
    this->prevMarketsAndBazarVisible = this->marketsAndBazarWidget.isVisible();
}

void IngameHudOverlay::unload(void)
{
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
    this->tileClickMarkerOverlay.hide();
    this->scrollBarOverlay.unload();
    this->backgroundWidget.unload();
}

void IngameHudOverlay::update(double dt, Camera& camera, const Map& map)
{
    this->syncWindowOrderOnOpen();

    // Par defaut, aucun widget ne peut toucher le curseur.
    this->chatWidget.setCursorEnabled(false);
    this->espionSearchPlayerWidget.setCursorEnabled(false);
    this->paramsMinimapWidget.setCursorEnabled(false);
    this->announcementsWidget.setCursorEnabled(false);
    this->logBookWidget.setCursorEnabled(false);
    this->marketsAndBazarWidget.setCursorEnabled(false);

    // On detecte la fenetre top-most sous la souris et elle seule pilotera le curseur.
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    bool hoveredTopWindow = false;
    for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
    {
        const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
        bool hovered = false;
        switch (layer)
        {
            case WindowLayer::CHAT:
                hovered = this->chatWidget.isVisible() && this->chatWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->chatWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::ESPION:
                hovered = this->espionSearchPlayerWidget.isVisible() && this->espionSearchPlayerWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->espionSearchPlayerWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::PARAMS_MINIMAP:
                hovered = this->paramsMinimapWidget.isVisible() && this->paramsMinimapWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->paramsMinimapWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::ANNOUNCEMENTS:
                hovered = this->announcementsWidget.isVisible() && this->announcementsWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->announcementsWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::LOG_BOOK:
                hovered = this->logBookWidget.isVisible() && this->logBookWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->logBookWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::MARKETS_AND_BAZAR:
                hovered = this->marketsAndBazarWidget.isVisible() && this->marketsAndBazarWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->marketsAndBazarWidget.setCursorEnabled(true); }
                break;
        }

        if (hovered)
        {
            hoveredTopWindow = true;
            break;
        }
    }

    // Si aucune fenetre flottante n'est sous la souris, on force le curseur normal.
    if (!hoveredTopWindow)
    {
        setCursorArrow();
    }

    // Update de bas vers haut pour que la GUI la plus en avant garde le dernier curseur applique.
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
        }
    }
    this->tileClickMarkerOverlay.update(dt);
    this->scrollBarOverlay.update(dt, camera, map, map.rect);
}

void IngameHudOverlay::drawBackgroundWidget(void)
{
    this->backgroundWidget.draw();
}

void IngameHudOverlay::drawWidgets(const Map& map, const Player& player)
{
    this->sectorCoordinateOverlay.draw(map, player);
    this->minimapWidget.draw();
    this->barreActionWidget.draw();
    this->centerShipButtonWidget.draw();

    // Draw des fenetres flottantes de bas vers haut.
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
    // Hit-test de haut vers bas pour respecter la superposition visuelle.
    for (int i = static_cast<int>(this->windowDrawOrder.size()) - 1; i >= 0; --i)
    {
        const WindowLayer layer = this->windowDrawOrder[static_cast<std::size_t>(i)];
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
        }

        if (!consumed)
        {
            continue;
        }

        // La derniere GUI interagie (ouverte/active) passe devant.
        if (button == RC2D_MOUSE_BUTTON_LEFT)
        {
            this->bringWindowToFront(layer);
        }

        if (layer == WindowLayer::CHAT)
        {
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
        }
        else if (layer == WindowLayer::ESPION)
        {
            this->chatWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
        }
        else if (layer == WindowLayer::MARKETS_AND_BAZAR)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
        }
        else
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marketsAndBazarWidget.clearFocus();
        }
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marketsAndBazarWidget.clearFocus();
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
    // Molette traitee de haut vers bas pour que la fenetre au-dessus gagne.
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
    // Wrapper overlays map: actuellement seule la scrollbar consomme le clic.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }
    return this->scrollBarOverlay.handleClick(x, y, map.rect);
}

void IngameHudOverlay::notifyMapTileClicked(int tileX, int tileY)
{
    // Wrapper overlays map: actuellement on affiche le marker de clic.
    this->tileClickMarkerOverlay.show(tileX, tileY);
}

bool IngameHudOverlay::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    // Clavier de haut vers bas, utile si plusieurs inputs seraient potentiellement actifs.
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



