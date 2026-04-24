#include "game/ui/hud/top-bar-logbook-button-widget.h"

TopBarLogbookButtonWidget::TopBarLogbookButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::LOGBOOK,
          "assets/images/ui-scene-game/icon-logbook.png")
{
}

TopBarLogbookButtonWidget::~TopBarLogbookButtonWidget(void)
{
}
