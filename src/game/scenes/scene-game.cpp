#include "game/scenes/scene-game.h"

#include "game/scenes/scene-manager.h"

GameScene::GameScene(void)
    // Initialise le renderer océan.
    : oceanRenderer{},
      // Initialise le renderer brouillard de guerre.
      fogOfWarRenderer{}
{
    // Le constructeur ne fait pas d'allocation lourde.
}

void GameScene::unload(void)
{
    // Libère toutes les ressources GPU de l'océan.
    oceanRenderer.unload();

    // Libère toutes les ressources GPU du brouillard de guerre.
    fogOfWarRenderer.unload();
}

void GameScene::load(void)
{
    // Définit la texture de base utilisée pour l'océan.
    const char* oceanBaseTexturePath = "assets/images/tile-water-base-blue.png";

    // Définit la texture de détail utilisée pour l'océan.
    const char* oceanDetailTexturePath = "assets/images/tile-water-detail-blue.png";

    // Charge le renderer océan avec les textures demandées.
    const bool oceanLoaded = oceanRenderer.load(oceanBaseTexturePath, oceanDetailTexturePath);

    // Journalise une erreur si le renderer océan ne se charge pas.
    if (!oceanLoaded)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: impossible de charger le renderer ocean");
    }

    // Charge le renderer de brouillard de guerre.
    const bool fogLoaded = fogOfWarRenderer.load();

    // Journalise un avertissement si le renderer fog est indisponible.
    if (!fogLoaded)
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: impossible de charger le renderer fog of war");
    }
}

void GameScene::update(double dt)
{
    // Met à jour l'animation et les uniforms de l'océan.
    oceanRenderer.update(dt);

    // Met à jour l'animation du fog en se basant sur le mode couleur océan.
    fogOfWarRenderer.update(dt, oceanRenderer.getColorMode());
}

void GameScene::draw(void)
{
    // Stoppe le rendu si l'océan n'a pas été chargé.
    if (!oceanRenderer.isReady())
    {
        return;
    }

    // Récupère la zone visible sécurisée de la scène.
    SDL_FRect visibleRect = rc2d_engine_getVisibleSafeRectRender();

    // Dessine d'abord l'océan.
    oceanRenderer.draw(visibleRect);

    // Dessine ensuite le fog au-dessus de l'océan.
    fogOfWarRenderer.draw(visibleRect);
}

void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Marque la variable comme utilisée intentionnellement.
    (void)key;

    // Marque la variable comme utilisée intentionnellement.
    (void)scancode;

    // Marque la variable comme utilisée intentionnellement.
    (void)mod;

    // Marque la variable comme utilisée intentionnellement.
    (void)keyboardID;

    // Ignore les répétitions automatiques clavier.
    if (isrepeat)
    {
        return;
    }

    // Retourne au menu principal.
    if (keycode == SDLK_M && sceneManager != nullptr)
    {
        sceneManager->changeScene("menu");
        return;
    }

    // Ouvre la scène éditeur de carte.
    if (keycode == SDLK_E && sceneManager != nullptr)
    {
        sceneManager->changeScene("editormap");
        return;
    }

    // Quitte le jeu avec la touche Echap.
    if (keycode == SDLK_ESCAPE)
    {
        rc2d_event_quit();
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Marque la variable comme utilisée intentionnellement.
    (void)x;

    // Marque la variable comme utilisée intentionnellement.
    (void)y;

    // Marque la variable comme utilisée intentionnellement.
    (void)button;

    // Marque la variable comme utilisée intentionnellement.
    (void)clicks;

    // Marque la variable comme utilisée intentionnellement.
    (void)mouseID;
}
