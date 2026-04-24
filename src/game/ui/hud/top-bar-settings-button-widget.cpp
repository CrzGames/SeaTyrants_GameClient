#include "game/ui/hud/top-bar-settings-button-widget.h"

TopBarSettingsButtonWidget::TopBarSettingsButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::SETTINGS,
          "assets/images/ui-scene-game/icon-settings.png")
{
}

TopBarSettingsButtonWidget::~TopBarSettingsButtonWidget(void)
{
}
