#include "game.h"

#include "game_screen.h"
#include "scenes/scene-menu.h"
#include "scenes/scene-editormap.h"
#include "scenes/scene-manager.h"
#include "scenes/scene-splashscreen.h"
#include "scenes/scene-game.h"

SceneManager sceneManager;
GameScreen gameScreen;

void rc2d_unload(void)
{
    sceneManager.unload();
}

void rc2d_load(void)
{
    sceneManager.addScene("menu", new MenuScene());
    sceneManager.addScene("editormap", new EditorMapScene());
    sceneManager.addScene("game", new GameScene());
    sceneManager.addScene("splashscreen", new SplashScreenScene());
    sceneManager.changeScene("splashscreen");
}

void rc2d_update(double dt)
{
    gameScreen.update(dt);
    sceneManager.update(dt);
}

void rc2d_draw(void)
{
    sceneManager.draw();
}

void rc2d_keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID)
{
    sceneManager.keypressed(key, scancode, keycode, mod, isrepeat, keyboardID);
}

void rc2d_mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    sceneManager.mousepressed(x, y, button, clicks, mouseID);
}