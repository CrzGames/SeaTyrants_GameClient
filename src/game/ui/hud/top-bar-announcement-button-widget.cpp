#include "game/ui/hud/top-bar-announcement-button-widget.h"

TopBarAnnouncementButtonWidget::TopBarAnnouncementButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::ANNOUNCEMENT,
          "assets/images/ui-scene-game/icon-anouncement.png")
{
}

TopBarAnnouncementButtonWidget::~TopBarAnnouncementButtonWidget(void)
{
}
