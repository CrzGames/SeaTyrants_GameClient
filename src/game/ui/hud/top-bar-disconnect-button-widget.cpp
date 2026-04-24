#include "game/ui/hud/top-bar-disconnect-button-widget.h"

TopBarDisconnectButtonWidget::TopBarDisconnectButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::DISCONNECT,
          "assets/images/ui-scene-game/icon-disconnect.png")
{
}

TopBarDisconnectButtonWidget::~TopBarDisconnectButtonWidget(void)
{
}
