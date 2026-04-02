#include <amoredtactics/game_screen.h>
#include <RC2D/RC2D_engine.h> // <- pour rc2d_engine_getVisibleSafeRectRender()

GameScreen::GameScreen() 
{
    this->updateGameScreenRect();
}

GameScreen::~GameScreen() {}

void GameScreen::update(double dt) 
{
    this->updateGameScreenRect();
}

void GameScreen::updateGameScreenRect(void) 
{
    this->rect = rc2d_engine_getVisibleSafeRectRender();
}