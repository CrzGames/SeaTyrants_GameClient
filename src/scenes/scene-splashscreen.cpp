#include "scenes/scene-splashscreen.h"

#include "scenes/scene-manager.h"

void SplashScreenScene::unload(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Unloaded\n");
}

void SplashScreenScene::load(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Splash Screen Scene Loaded\n");
}

void SplashScreenScene::update(double dt) 
{

}

void SplashScreenScene::draw(void) 
{

}

void SplashScreenScene::keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) 
{

}

void SplashScreenScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) 
{

}