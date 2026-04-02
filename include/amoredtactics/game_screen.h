#ifndef GAME_SCREEN_H
#define GAME_SCREEN_H

#include <SDL3/SDL.h>

class GameScreen {
    private:
        void updateGameScreenRect(void);

    public:
        SDL_FRect rect; /**< Rectangle de la zone de jeu (game screen) en coordonnées logiques. */

        GameScreen();
        ~GameScreen();

        void update(double dt);
};

#endif // GAME_SCREEN_H