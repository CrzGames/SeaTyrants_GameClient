#include "game/ui/hud/top-bar-chat-button-widget.h"

TopBarChatButtonWidget::TopBarChatButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::CHAT,
          "assets/images/ui-scene-game/icon-tchat.png")
{
}

TopBarChatButtonWidget::~TopBarChatButtonWidget(void)
{
}
