#include "game/ui/hud/top-bar-guild-button-widget.h"

TopBarGuildButtonWidget::TopBarGuildButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::GUILD,
          "assets/images/ui-scene-game/icon-guild.png")
{
}

TopBarGuildButtonWidget::~TopBarGuildButtonWidget(void)
{
}
