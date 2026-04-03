#include "game/scenes/scene-game.h"

#include "game/scenes/scene-manager.h"

GameScene::Shaders::Shaders(void)
    // Initialise le module shader ocean.
    : oceanShader{},
      // Initialise le module shader fog of war.
      fogOfWarShader{},
      // Initialise la couleur ocean par defaut.
      oceanColor(OceanShader::WaterColor::CORAL)
{
    // Le constructeur du sous-module ne fait pas d'allocation lourde.
}

void GameScene::Shaders::load(void)
{
    // Charge le shader ocean avec la couleur active.
    const bool oceanLoaded = oceanShader.load(oceanColor);

    // Journalise une erreur si le shader ocean ne se charge pas.
    if (!oceanLoaded)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene::Shaders: impossible de charger le shader ocean");
    }

    // Charge le shader fog of war.
    const bool fogLoaded = fogOfWarShader.load();

    // Journalise un avertissement si le shader fog ne se charge pas.
    if (!fogLoaded)
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene::Shaders: impossible de charger le shader fog of war");
    }
}

void GameScene::Shaders::unload(void)
{
    // Libere toutes les ressources GPU du shader ocean.
    oceanShader.unload();

    // Libere toutes les ressources GPU du shader fog of war.
    fogOfWarShader.unload();
}

void GameScene::Shaders::update(double dt)
{
    // Met a jour l'animation et les uniforms du shader ocean.
    oceanShader.update(dt);

    // Met a jour le shader fog avec le mode couleur courant de l'ocean.
    fogOfWarShader.update(dt, oceanShader.getColorMode());
}

void GameScene::Shaders::draw(const SDL_FRect& visibleRect)
{
    // Dessine le pass ocean.
    oceanShader.draw(visibleRect);

    // Dessine ensuite le pass fog au-dessus.
    fogOfWarShader.draw(visibleRect);
}

bool GameScene::Shaders::isReady(void) const
{
    // Retourne vrai si le pass principal ocean est pret.
    return oceanShader.isReady();
}

GameScene::GameScene(void)
    // Initialise le sous-module qui regroupe les shaders de la scene.
    : shaders{}
{
    // Le constructeur de scene ne fait pas d'allocation lourde.
}

void GameScene::unload(void)
{
    // Delegate la liberation des shaders au sous-module.
    shaders.unload();
}

void GameScene::load(void)
{
    // Delegate le chargement des shaders au sous-module.
    shaders.load();
}

void GameScene::update(double dt)
{
    // Delegate la mise a jour des shaders au sous-module.
    shaders.update(dt);
}

void GameScene::draw(void)
{
    // Stoppe le rendu si le sous-module shaders n'est pas pret.
    if (!shaders.isReady())
    {
        return;
    }

    // Recupere la zone visible securisee de la scene.
    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    // Delegate le dessin des shaders au sous-module.
    shaders.draw(visibleRect);
}

void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Marque la variable comme utilisee intentionnellement.
    (void)key;

    // Marque la variable comme utilisee intentionnellement.
    (void)scancode;

    // Marque la variable comme utilisee intentionnellement.
    (void)keycode;

    // Marque la variable comme utilisee intentionnellement.
    (void)mod;

    // Marque la variable comme utilisee intentionnellement.
    (void)isrepeat;

    // Marque la variable comme utilisee intentionnellement.
    (void)keyboardID;
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Marque la variable comme utilisee intentionnellement.
    (void)x;

    // Marque la variable comme utilisee intentionnellement.
    (void)y;

    // Marque la variable comme utilisee intentionnellement.
    (void)button;

    // Marque la variable comme utilisee intentionnellement.
    (void)clicks;

    // Marque la variable comme utilisee intentionnellement.
    (void)mouseID;
}
