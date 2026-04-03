#include "game/scenes/scene-game.h"

#include "game/scenes/scene-manager.h"

GameScene::GameScene(void)
    // Initialise le renderer océan.
    : oceanShader{},
      // Initialise le renderer brouillard de guerre.
      fogOfWarShader{}
{
    // Le constructeur ne fait pas d'allocation lourde.
}

void GameScene::unload(void)
{
    // Libère toutes les ressources GPU de l'océan.
    oceanShader.unload();

    // Libère toutes les ressources GPU du brouillard de guerre.
    fogOfWarShader.unload();
}

void GameScene::load(void)
{
    // Sélectionne la couleur d'océan chargée dans cette scène.
    const OceanShader::WaterColor oceanColor = OceanShader::WaterColor::RED;

    // Charge le shader océan avec la couleur choisie.
    const bool oceanLoaded = oceanShader.load(oceanColor);

    // Journalise une erreur si le renderer océan ne se charge pas.
    if (!oceanLoaded)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: impossible de charger le renderer ocean");
    }

    // Charge le renderer de brouillard de guerre.
    const bool fogLoaded = fogOfWarShader.load();

    // Journalise un avertissement si le renderer fog est indisponible.
    if (!fogLoaded)
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: impossible de charger le renderer fog of war");
    }
}

void GameScene::update(double dt)
{
    // Met à jour l'animation et les uniforms de l'océan.
    oceanShader.update(dt);

    // Met à jour l'animation du fog en se basant sur le mode couleur océan.
    fogOfWarShader.update(dt, oceanShader.getColorMode());
}

void GameScene::draw(void)
{
    // Stoppe le rendu si l'océan n'a pas été chargé.
    if (!oceanShader.isReady())
    {
        return;
    }

    // Récupère la zone visible sécurisée de la scène.
    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    // Dessine d'abord l'océan.
    oceanShader.draw(visibleRect);

    // Dessine ensuite le fog au-dessus de l'océan.
    fogOfWarShader.draw(visibleRect);
}

void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{

}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{

}
