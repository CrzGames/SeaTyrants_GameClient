#include "game/ui/hud/top-bar-menu-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <cmath>

namespace top_bar_menu_widget_internal {

static constexpr float kIconRowTopOffset = 4.0f;
static constexpr float kTopBarOffsetY = 2.0f;
static constexpr float kButtonGap = 14.0f;
static constexpr float kCenterGap = 210.0f;
static constexpr float kTooltipOffsetX = 14.0f;
static constexpr float kTooltipOffsetY = 18.0f;
static constexpr float kTooltipPaddingX = 10.0f;
static constexpr float kTooltipPaddingY = 6.0f;
static constexpr RC2D_Color kTooltipFill = RC2D_Color{67, 8, 8, 236};
static constexpr RC2D_Color kTooltipBorder = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kTooltipText = RC2D_Color{217, 200, 134, 255};

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

static const char* getTooltipLabel(TopBarMenuWidget::Action action)
{
    switch (action)
    {
        case TopBarMenuWidget::Action::CHAT: return "Tchat";
        case TopBarMenuWidget::Action::GUILD: return "Guilde";
        case TopBarMenuWidget::Action::QUEST: return "Quetes";
        case TopBarMenuWidget::Action::LEADERBOARD: return "Leaderboard";
        case TopBarMenuWidget::Action::MONEY: return "Money";
        case TopBarMenuWidget::Action::SHIP: return "Navire";
        case TopBarMenuWidget::Action::ANNOUNCEMENT: return "Annonces";
        case TopBarMenuWidget::Action::LOGBOOK: return "Journal de bord";
        case TopBarMenuWidget::Action::SETTINGS: return "Parametres";
        case TopBarMenuWidget::Action::DISCONNECT: return "Deconnexion";
        case TopBarMenuWidget::Action::NONE:
        default:
            break;
    }
    return "";
}

} // namespace top_bar_menu_widget_internal

using namespace top_bar_menu_widget_internal;

TopBarMenuWidget::TopBarMenuWidget(void)
    : topBarMenuUiImage{},
      tooltipFont{},
      hoveredAction(Action::NONE),
      hoveredMouseX(0.0f),
      hoveredMouseY(0.0f),
      chatButton{},
      guildButton{},
      questButton{},
      leaderboardButton{},
      moneyButton{},
      shipButton{},
      announcementButton{},
      logbookButton{},
      settingsButton{},
      disconnectButton{}
{
}

TopBarMenuWidget::~TopBarMenuWidget(void)
{
}

void TopBarMenuWidget::load(void)
{
    this->topBarMenuUiImage = LoadStorageImage(
        "assets/images/ui-scene-game/top-bar-menu.png",
        RC2D_STORAGE_TITLE);
    this->tooltipFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        13.0f);
    if (this->topBarMenuUiImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "TopBarMenuWidget: echec chargement top-bar-menu.png");
    }

    this->chatButton.load();
    this->guildButton.load();
    this->questButton.load();
    this->leaderboardButton.load();
    this->moneyButton.load();
    this->shipButton.load();
    this->announcementButton.load();
    this->logbookButton.load();
    this->settingsButton.load();
    this->disconnectButton.load();
    this->updateButtonLayout();
}

void TopBarMenuWidget::unload(void)
{
    this->disconnectButton.unload();
    this->settingsButton.unload();
    this->logbookButton.unload();
    this->announcementButton.unload();
    this->shipButton.unload();
    this->moneyButton.unload();
    this->leaderboardButton.unload();
    this->questButton.unload();
    this->guildButton.unload();
    this->chatButton.unload();
    ResetStorageFontRef(&this->tooltipFont);
    ResetStorageImageRef(&this->topBarMenuUiImage);
}

bool TopBarMenuWidget::update(float mouseX, float mouseY)
{
    this->updateButtonLayout();
    this->hoveredAction = Action::NONE;
    this->hoveredMouseX = mouseX;
    this->hoveredMouseY = mouseY;

    bool hovered = false;
    if (this->chatButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::CHAT;
    }
    if (this->guildButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::GUILD;
    }
    if (this->questButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::QUEST;
    }
    if (this->leaderboardButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::LEADERBOARD;
    }
    if (this->moneyButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::MONEY;
    }
    if (this->shipButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::SHIP;
    }
    if (this->announcementButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::ANNOUNCEMENT;
    }
    if (this->logbookButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::LOGBOOK;
    }
    if (this->settingsButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::SETTINGS;
    }
    if (this->disconnectButton.updateHover(mouseX, mouseY))
    {
        hovered = true;
        this->hoveredAction = Action::DISCONNECT;
    }
    return hovered;
}

void TopBarMenuWidget::draw(void)
{
    this->updateButtonLayout();

    if (this->topBarMenuUiImage.sdl_texture == nullptr)
    {
        return;
    }

    const SDL_FRect gameScreenRect = GetGameScreen().rect;

    rc2d_graphics_drawImage(
        &this->topBarMenuUiImage,
        gameScreenRect.x,
        gameScreenRect.y + kTopBarOffsetY,
        0.0,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        false,
        false);

    this->chatButton.draw();
    this->guildButton.draw();
    this->questButton.draw();
    this->leaderboardButton.draw();
    this->moneyButton.draw();
    this->shipButton.draw();
    this->announcementButton.draw();
    this->logbookButton.draw();
    this->settingsButton.draw();
    this->disconnectButton.draw();
    this->drawHoveredTooltip();
}

TopBarMenuWidget::Action TopBarMenuWidget::mousepressed(float x, float y, RC2D_MouseButton button)
{
    this->updateButtonLayout();

    const Action actions[] = {
        this->chatButton.mousepressed(x, y, button),
        this->guildButton.mousepressed(x, y, button),
        this->questButton.mousepressed(x, y, button),
        this->leaderboardButton.mousepressed(x, y, button),
        this->moneyButton.mousepressed(x, y, button),
        this->shipButton.mousepressed(x, y, button),
        this->announcementButton.mousepressed(x, y, button),
        this->logbookButton.mousepressed(x, y, button),
        this->settingsButton.mousepressed(x, y, button),
        this->disconnectButton.mousepressed(x, y, button)
    };

    for (const Action action : actions)
    {
        if (action != Action::NONE)
        {
            return action;
        }
    }
    return Action::NONE;
}

void TopBarMenuWidget::setActionActive(Action action, bool active)
{
    switch (action)
    {
        case Action::CHAT: this->chatButton.setActive(active); break;
        case Action::GUILD: this->guildButton.setActive(active); break;
        case Action::QUEST: this->questButton.setActive(active); break;
        case Action::LEADERBOARD: this->leaderboardButton.setActive(active); break;
        case Action::MONEY: this->moneyButton.setActive(active); break;
        case Action::SHIP: this->shipButton.setActive(active); break;
        case Action::ANNOUNCEMENT: this->announcementButton.setActive(active); break;
        case Action::LOGBOOK: this->logbookButton.setActive(active); break;
        case Action::SETTINGS: this->settingsButton.setActive(active); break;
        case Action::DISCONNECT: this->disconnectButton.setActive(active); break;
        case Action::NONE:
        default:
            break;
    }
}

void TopBarMenuWidget::updateButtonLayout(void)
{
    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float buttonWidth = 42.0f;
    const float groupWidth = (buttonWidth * 5.0f) + (kButtonGap * 4.0f);
    const float centerX = gameScreenRect.w * 0.5f;
    float leftGroupX = centerX - (kCenterGap * 0.5f) - groupWidth;
    float rightGroupX = centerX + (kCenterGap * 0.5f);

    this->chatButton.setLocalPosition(leftGroupX, kIconRowTopOffset);
    leftGroupX += buttonWidth + kButtonGap;
    this->guildButton.setLocalPosition(leftGroupX, kIconRowTopOffset);
    leftGroupX += buttonWidth + kButtonGap;
    this->questButton.setLocalPosition(leftGroupX, kIconRowTopOffset);
    leftGroupX += buttonWidth + kButtonGap;
    this->leaderboardButton.setLocalPosition(leftGroupX, kIconRowTopOffset);
    leftGroupX += buttonWidth + kButtonGap;
    this->moneyButton.setLocalPosition(leftGroupX, kIconRowTopOffset);

    this->shipButton.setLocalPosition(rightGroupX, kIconRowTopOffset);
    rightGroupX += buttonWidth + kButtonGap;
    this->announcementButton.setLocalPosition(rightGroupX, kIconRowTopOffset);
    rightGroupX += buttonWidth + kButtonGap;
    this->logbookButton.setLocalPosition(rightGroupX, kIconRowTopOffset);
    rightGroupX += buttonWidth + kButtonGap;
    this->settingsButton.setLocalPosition(rightGroupX, kIconRowTopOffset);
    rightGroupX += buttonWidth + kButtonGap;
    this->disconnectButton.setLocalPosition(rightGroupX, kIconRowTopOffset);
}

void TopBarMenuWidget::drawHoveredTooltip(void) const
{
    if (this->hoveredAction == Action::NONE || this->tooltipFont.sdl_font == nullptr)
    {
        return;
    }

    const char* label = getTooltipLabel(this->hoveredAction);
    if (label == nullptr || label[0] == '\0')
    {
        return;
    }

    const SDL_FRect gameScreenRect = GetGameScreen().rect;
    const float textWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    const float textHeight = measureTextHeight(const_cast<RC2D_Font*>(&this->tooltipFont), label);
    SDL_FRect tooltipRect = SDL_FRect{
        this->hoveredMouseX + kTooltipOffsetX,
        this->hoveredMouseY + kTooltipOffsetY,
        textWidth + (kTooltipPaddingX * 2.0f),
        textHeight + (kTooltipPaddingY * 2.0f)
    };

    const float maxX = (gameScreenRect.x + gameScreenRect.w) - tooltipRect.w;
    const float maxY = (gameScreenRect.y + gameScreenRect.h) - tooltipRect.h;
    tooltipRect.x = (std::max)(gameScreenRect.x, (std::min)(tooltipRect.x, maxX));
    tooltipRect.y = (std::max)(gameScreenRect.y, (std::min)(tooltipRect.y, maxY));

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kTooltipFill);
    rc2d_graphics_rectangle("fill", &tooltipRect);
    rc2d_graphics_setColor(kTooltipBorder);
    rc2d_graphics_rectangle("line", &tooltipRect);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->tooltipFont),
        label,
        tooltipRect.x + kTooltipPaddingX,
        tooltipRect.y + kTooltipPaddingY,
        kTooltipText);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
