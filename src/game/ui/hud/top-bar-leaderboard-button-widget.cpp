#include "game/ui/hud/top-bar-leaderboard-button-widget.h"

TopBarLeaderboardButtonWidget::TopBarLeaderboardButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::LEADERBOARD,
          "assets/images/ui-scene-game/icon-leaderboard.png")
{
}

TopBarLeaderboardButtonWidget::~TopBarLeaderboardButtonWidget(void)
{
}
