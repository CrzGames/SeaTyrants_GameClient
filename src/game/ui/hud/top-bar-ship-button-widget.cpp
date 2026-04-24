#include "game/ui/hud/top-bar-ship-button-widget.h"

TopBarShipButtonWidget::TopBarShipButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::SHIP,
          "assets/images/ui-scene-game/icon-ship.png")
{
}

TopBarShipButtonWidget::~TopBarShipButtonWidget(void)
{
}
