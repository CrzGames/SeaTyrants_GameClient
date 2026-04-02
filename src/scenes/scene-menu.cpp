#include <cstring>  // strcmp

#include <amoredtactics/scenes/scene-menu.h>
#include <amoredtactics/scenes/scene-manager.h>

void MenuScene::unload(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Unloaded\n");
}

void MenuScene::load(void) 
{
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Loaded\n");
}

void MenuScene::update(double dt) 
{

}

void MenuScene::draw(void) 
{
    // Couleur rouge
    rc2d_graphics_setColor({255, 0, 0, 255});

    SDL_FRect rect;
    rect.x = 400.0f;
    rect.y = 250.0f;
    rect.w = 200.0f;
    rect.h = 150.0f;

    // Dessin rempli
    rc2d_graphics_rectangle("fill", &rect);
}

void MenuScene::keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) 
{
    if (strcmp(key, "K") == 0 && !isrepeat) 
    {
        RC2D_log(RC2D_LOG_INFO, "Enter K Pressed change scene \n");
        sceneManager->changeScene("editormap");
    }
}

void MenuScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) 
{

}