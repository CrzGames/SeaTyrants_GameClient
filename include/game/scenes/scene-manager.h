#pragma once

#include <map>    // std::map
#include <string> // std::string

#include <RC2D/RC2D.h>

// Forward declaration des scenes pour éviter les inclusions circulaires
class Scene;

class SceneManager {
private:
    std::map<std::string, Scene*> scenes;
    Scene* currentScene;

public:
    SceneManager(void);
    ~SceneManager(void);

    void addScene(const std::string& name, Scene* scene);
    void changeScene(const std::string& name);

    void unload(void);
    void load(void);
    void update(double dt);
    void draw(void);
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID);
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);
    void mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID);
    // Ajoutez d'autres callbacks selon les besoins...
};
