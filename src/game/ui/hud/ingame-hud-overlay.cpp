#include "game/ui/hud/ingame-hud-overlay.h"

#include "core/context.h"
#include "game/entities/player.h"
#include "game/map/map.h"
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
    : backgroundUiImage{},
      minimapWidget{},
      barreActionWidget{},
      centerShipButtonWidget{},
      sectorCoordinateOverlay{},
      chatWidget{},
      espionSearchPlayerWidget{},
      paramsMinimapWidget{},
      annoncesWidget{},
      journalBordWidget{},
      marcheWidget{},
      windowDrawOrder{},
      prevChatVisible(false),
      prevEspionVisible(false),
      prevParamsVisible(false),
      prevAnnoncesVisible(false),
      prevJournalBordVisible(false),
      prevMarcheVisible(false)
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
    const bool annoncesVisible = this->annoncesWidget.isVisible();
    const bool journalBordVisible = this->journalBordWidget.isVisible();
    const bool marcheOffreNoirVisible = this->marcheWidget.isVisible();

    if (chatVisible && !this->prevChatVisible)
    {
        this->bringWindowToFront(WindowLayer::CHAT);
    }
    if (espionVisible && !this->prevEspionVisible)
    {
        this->bringWindowToFront(WindowLayer::ESPION);
    }
    if (paramsVisible && !this->prevParamsVisible)
    {
        this->bringWindowToFront(WindowLayer::PARAMS_MINIMAP);
    }
    if (annoncesVisible && !this->prevAnnoncesVisible)
    {
        this->bringWindowToFront(WindowLayer::ANNONCES);
    }
    if (journalBordVisible && !this->prevJournalBordVisible)
    {
        this->bringWindowToFront(WindowLayer::JOURNAL_BORD);
    }
    if (marcheOffreNoirVisible && !this->prevMarcheVisible)
    {
        this->bringWindowToFront(WindowLayer::MARCHE_OFFRE_NOIR);
    }

    this->prevChatVisible = chatVisible;
    this->prevEspionVisible = espionVisible;
    this->prevParamsVisible = paramsVisible;
    this->prevAnnoncesVisible = annoncesVisible;
    this->prevJournalBordVisible = journalBordVisible;
    this->prevMarcheVisible = marcheOffreNoirVisible;
}

void IngameHudOverlay::load(void)
{
    // Fond UI gameplay (bandes haut/bas).
    this->backgroundUiImage = rc2d_graphics_loadImageFromStorage(
        "assets/images/ui-scene-game/background.png",
        RC2D_STORAGE_TITLE);
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "IngameHudOverlay: echec chargement background UI gameplay");
    }

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
    this->annoncesWidget.load();
    this->journalBordWidget.load();
    this->marcheWidget.load();

    // Ordre par defaut (bas -> haut), puis etat visible initial.
    this->windowDrawOrder = {
        WindowLayer::CHAT,
        WindowLayer::ESPION,
        WindowLayer::PARAMS_MINIMAP,
        WindowLayer::ANNONCES,
        WindowLayer::JOURNAL_BORD,
        WindowLayer::MARCHE_OFFRE_NOIR
    };
    this->prevChatVisible = this->chatWidget.isVisible();
    this->prevEspionVisible = this->espionSearchPlayerWidget.isVisible();
    this->prevParamsVisible = this->paramsMinimapWidget.isVisible();
    this->prevAnnoncesVisible = this->annoncesWidget.isVisible();
    this->prevJournalBordVisible = this->journalBordWidget.isVisible();
    this->prevMarcheVisible = this->marcheWidget.isVisible();
}

void IngameHudOverlay::unload(void)
{
    this->marcheWidget.unload();
    this->journalBordWidget.unload();
    this->annoncesWidget.unload();
    this->paramsMinimapWidget.unload();
    this->espionSearchPlayerWidget.unload();
    this->chatWidget.unload();
    this->sectorCoordinateOverlay.unload();
    this->centerShipButtonWidget.unload();
    this->barreActionWidget.unload();
    this->minimapWidget.unload();

    rc2d_graphics_freeImage(&this->backgroundUiImage);
}

void IngameHudOverlay::update(double dt)
{
    this->syncWindowOrderOnOpen();

    // Par defaut, aucun widget ne peut toucher le curseur.
    this->chatWidget.setCursorEnabled(false);
    this->espionSearchPlayerWidget.setCursorEnabled(false);
    this->paramsMinimapWidget.setCursorEnabled(false);
    this->annoncesWidget.setCursorEnabled(false);
    this->journalBordWidget.setCursorEnabled(false);
    this->marcheWidget.setCursorEnabled(false);

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
            case WindowLayer::ANNONCES:
                hovered = this->annoncesWidget.isVisible() && this->annoncesWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->annoncesWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::JOURNAL_BORD:
                hovered = this->journalBordWidget.isVisible() && this->journalBordWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->journalBordWidget.setCursorEnabled(true); }
                break;
            case WindowLayer::MARCHE_OFFRE_NOIR:
                hovered = this->marcheWidget.isVisible() && this->marcheWidget.containsPoint(mouseX, mouseY);
                if (hovered) { this->marcheWidget.setCursorEnabled(true); }
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
            case WindowLayer::ANNONCES:
                this->annoncesWidget.update(dt);
                break;
            case WindowLayer::JOURNAL_BORD:
                this->journalBordWidget.update(dt);
                break;
            case WindowLayer::MARCHE_OFFRE_NOIR:
                this->marcheWidget.update(dt);
                break;
        }
    }
}

void IngameHudOverlay::drawBackground(void)
{
    if (this->backgroundUiImage.sdl_texture == nullptr)
    {
        return;
    }

    rc2d_graphics_drawImage(
        &this->backgroundUiImage,
        0.0f,
        0.0f,
        0.0,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        false,
        false);
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
            case WindowLayer::ANNONCES:
                this->annoncesWidget.draw();
                break;
            case WindowLayer::JOURNAL_BORD:
                this->journalBordWidget.draw();
                break;
            case WindowLayer::MARCHE_OFFRE_NOIR:
                this->marcheWidget.draw();
                break;
        }
    }
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
            case WindowLayer::ANNONCES:
                consumed = this->annoncesWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::JOURNAL_BORD:
                consumed = this->journalBordWidget.mousepressed(x, y, button, clicks, mouseID);
                break;
            case WindowLayer::MARCHE_OFFRE_NOIR:
                consumed = this->marcheWidget.mousepressed(x, y, button, clicks, mouseID);
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
            this->marcheWidget.clearFocus();
        }
        else if (layer == WindowLayer::ESPION)
        {
            this->chatWidget.clearFocus();
            this->marcheWidget.clearFocus();
        }
        else if (layer == WindowLayer::MARCHE_OFFRE_NOIR)
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
        }
        else
        {
            this->chatWidget.clearFocus();
            this->espionSearchPlayerWidget.clearFocus();
            this->marcheWidget.clearFocus();
        }
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->chatWidget.clearFocus();
        this->espionSearchPlayerWidget.clearFocus();
        this->marcheWidget.clearFocus();
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
            case WindowLayer::ANNONCES:
                if (this->annoncesWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::JOURNAL_BORD:
                if (this->journalBordWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
                {
                    return true;
                }
                break;
            case WindowLayer::MARCHE_OFFRE_NOIR:
                if (this->marcheWidget.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
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
            case WindowLayer::MARCHE_OFFRE_NOIR:
                if (this->marcheWidget.keypressed(key, scancode, keycode, mod, isrepeat))
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

void IngameHudOverlay::publishAnnouncement(const std::string& message)
{
    this->annoncesWidget.pushAnnouncement(message);
}

void IngameHudOverlay::publishSearchResult(const std::string& resultText)
{
    this->espionSearchPlayerWidget.publishSearchResult(resultText);
}

void IngameHudOverlay::publishLogbookEntry(const std::string& dateTime, const std::string& message)
{
    this->journalBordWidget.pushEntry(dateTime, message);
}

void IngameHudOverlay::setBazardMarketRows(const std::vector<MarcheWidget::BazardItemData>& rows)
{
    this->marcheWidget.setBazardRows(rows);
}

void IngameHudOverlay::setNoirMarketRows(const std::vector<MarcheWidget::MarketItemData>& rows)
{
    this->marcheWidget.setMarcheNoirRows(rows);
}

void IngameHudOverlay::setBasiqueMarketRows(const std::vector<MarcheWidget::MarketItemData>& rows)
{
    this->marcheWidget.setMarcheBasiqueRows(rows);
}

void IngameHudOverlay::setEvenementMarketRows(const std::vector<MarcheWidget::MarketItemData>& rows)
{
    this->marcheWidget.setMarcheEvenementRows(rows);
}

