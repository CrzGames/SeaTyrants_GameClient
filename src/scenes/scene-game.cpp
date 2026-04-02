#include <amoredtactics/scenes/scene-game.h>
#include <amoredtactics/scenes/scene-manager.h>

void GameScene::unload(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Game Scene Unloaded\n");
}

void GameScene::load(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Game Scene Loaded\n");
}

void GameScene::update(double dt) 
{

}

void GameScene::draw(void) 
{

}

void GameScene::keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) 
{

}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) 
{

}