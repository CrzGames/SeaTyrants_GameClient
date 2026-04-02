#include <amoredtactics/scenes/scene-manager.h>
#include <amoredtactics/scenes/scene.h>

SceneManager::SceneManager(void) : currentScene(nullptr) {}

SceneManager::~SceneManager(void) 
{
    for (auto& scene : scenes) 
    {
        delete scene.second; // Libère la mémoire de toutes les scènes
    }

    scenes.clear(); // Vide la map pour éviter les fuites de mémoire
}

void SceneManager::addScene(const std::string& name, Scene* scene) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        delete it->second; // Assurez-vous de libérer la mémoire si une scène avec le même nom existait déjà
    }

    scene->setSceneManager(this); // Définir le gestionnaire de scènes pour la scène
    scenes[name] = scene;
}

void SceneManager::changeScene(const std::string& name) 
{
    auto it = scenes.find(name);
    if (it != scenes.end()) 
    {
        if (currentScene) 
        {
            currentScene->unload();
        }
        
        currentScene = it->second;
        currentScene->load();
    }
}

void SceneManager::unload(void) 
{
    if (currentScene) 
    {
        currentScene->unload();
    }
}

void SceneManager::load(void) 
{
    if (currentScene) 
    {
        currentScene->load();
    }
}

void SceneManager::update(double dt) 
{
    if (currentScene) 
    {
        currentScene->update(dt);
    }
}

void SceneManager::draw(void) 
{
    if (currentScene) 
    {
        currentScene->draw();
    }
}

void SceneManager::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) 
{
    if (currentScene) 
    {
        currentScene->keypressed(key, scancode, keycode, mod, isrepeat, keyboardID);
    }
}

void SceneManager::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) 
{
    if (currentScene) 
    {
        currentScene->mousepressed(x, y, button, clicks, mouseID);
    }
}