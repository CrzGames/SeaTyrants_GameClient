#pragma once

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

/**
 * @brief Instance globale de la zone de jeu, mise a jour dans callbacks.cpp.
 */
extern GameScreen gameScreen;
