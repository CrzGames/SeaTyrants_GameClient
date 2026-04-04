#include "game/scenes/scene-manager.h"

#include "game/scenes/scene.h"

SceneManager::SceneManager(void) : currentScene(nullptr) {}

SceneManager::~SceneManager(void) 
{
    for (auto& scene : this->scenes) 
    {
        delete scene.second; // Libère la mémoire de toutes les scènes
    }

    this->scenes.clear(); // Vide la map pour éviter les fuites de mémoire
}

void SceneManager::addScene(const std::string& name, Scene* scene) {
    auto it = this->scenes.find(name);
    if (it != this->scenes.end()) {
        delete it->second; // Assurez-vous de libérer la mémoire si une scène avec le même nom existait déjà
    }

    scene->setSceneManager(this); // Définir le gestionnaire de scènes pour la scène
    this->scenes[name] = scene;
}

void SceneManager::changeScene(const std::string& name) 
{
    auto it = this->scenes.find(name);
    if (it != this->scenes.end()) 
    {
        if (this->currentScene) 
        {
            this->currentScene->unload();
        }
        
        this->currentScene = it->second;
        this->currentScene->load();
    }
}

void SceneManager::unload(void) 
{
    if (this->currentScene) 
    {
        this->currentScene->unload();
    }
}

void SceneManager::load(void) 
{
    if (this->currentScene) 
    {
        this->currentScene->load();
    }
}

void SceneManager::update(double dt) 
{
    if (this->currentScene) 
    {
        this->currentScene->update(dt);
    }
}

void SceneManager::draw(void) 
{
    if (this->currentScene) 
    {
        this->currentScene->draw();
    }
}

void SceneManager::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) 
{
    if (this->currentScene) 
    {
        this->currentScene->keypressed(key, scancode, keycode, mod, isrepeat, keyboardID);
    }
}

void SceneManager::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) 
{
    if (this->currentScene) 
    {
        this->currentScene->mousepressed(x, y, button, clicks, mouseID);
    }
}