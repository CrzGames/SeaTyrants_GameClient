#include "game/ui/hud/top-bar-money-button-widget.h"

TopBarMoneyButtonWidget::TopBarMoneyButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::MONEY,
          "assets/images/ui-scene-game/icon-money.png")
{
}

TopBarMoneyButtonWidget::~TopBarMoneyButtonWidget(void)
{
}
