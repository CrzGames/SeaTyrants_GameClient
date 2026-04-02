#ifndef SCENE_H
#define SCENE_H

#include <RC2D/RC2D.h>

class SceneManager; // Declaration anticipee

class Scene {
    protected:
        SceneManager* sceneManager;

    public:
        Scene(void) : sceneManager(nullptr) {}
        virtual ~Scene(void) {}

        void setSceneManager(SceneManager* sceneManager) {
            this->sceneManager = sceneManager;
        }

        virtual void load(void) = 0;
        virtual void unload(void) = 0; 
        virtual void update(double dt) = 0;
        virtual void draw(void) = 0;
        virtual void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) = 0;
        virtual void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) = 0;
        // Ajoutez d'autres callbacks selon les besoins...
};

#endif // SCENE_H