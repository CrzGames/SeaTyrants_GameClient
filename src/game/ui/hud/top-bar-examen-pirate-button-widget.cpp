#include "game/ui/hud/top-bar-examen-pirate-button-widget.h"

TopBarExamenPirateButtonWidget::TopBarExamenPirateButtonWidget(void)
    : TopBarActionButtonWidget(
          TopBarActionButtonWidget::Action::PIRATE_EXAM,
          "assets/images/ui-scene-game/icon-examenpirate.png")
{
    // Action dediee pour garder un point d'extension propre quand la future GUI
    // sera branchee, tout en ayant deja le hover et le clic consomme.
}

TopBarExamenPirateButtonWidget::~TopBarExamenPirateButtonWidget(void)
{
    // Destructeur trivial: la base gere deja tout l'etat utile.
}
