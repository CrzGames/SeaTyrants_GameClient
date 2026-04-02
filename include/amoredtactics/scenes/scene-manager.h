#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <map>
#include <string>

#include <RC2D/RC2D.h>

class Scene; // Declaration anticipée

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
    // Ajoutez d'autres callbacks selon les besoins...
};

#endif // SCENE_MANAGER_H