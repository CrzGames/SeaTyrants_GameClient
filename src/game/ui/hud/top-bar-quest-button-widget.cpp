#include "game/ui/hud/top-bar-quest-button-widget.h"

TopBarQuestButtonWidget::TopBarQuestButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::QUEST,
          "assets/images/ui-scene-game/icon-quest.png")
{
}

TopBarQuestButtonWidget::~TopBarQuestButtonWidget(void)
{
}
