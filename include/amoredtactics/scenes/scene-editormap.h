#ifndef SCENE_EDITORMAP_H
#define SCENE_EDITORMAP_H

#include <RC2D/RC2D.h>

#include <amoredtactics/scenes/scene.h>

class EditorMapScene : public Scene {
    public:
        void unload(void) override;
        void load(void) override;
        void update(double dt) override;
        void draw(void) override;
        void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
        void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};

#endif // SCENE_EDITORMAP_H