#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene-manager.h"

class Scene {
    protected:
        SceneManager* sceneManager;

    public:
        Scene(void) : sceneManager(nullptr) {}
        virtual ~Scene(void) {}

        void setSceneManager(SceneManager* manager) {
            this->sceneManager = manager;
        }

        virtual void load(void) = 0;
        virtual void unload(void) = 0; 
        virtual void update(double dt) = 0;
        virtual void draw(void) = 0;
        virtual void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) = 0;
        virtual void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) = 0;
        virtual void mousewheelmoved(
            RC2D_MouseWheelDirection direction,
            float x,
            float y,
            Sint32 integer_x,
            Sint32 integer_y,
            float mouse_x,
            float mouse_y,
            SDL_MouseID mouseID) {}
        // Ajoutez d'autres callbacks selon les besoins...
};
