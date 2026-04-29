#pragma once

#include <array>
#include <cstddef>
#include <string>

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"
#include "game/vfx/vfx.h"

class Player;

/**
 * @brief Scene principale gameplay.
 */
class GameScene : public Scene {
private:
    void initializePlayerSpawnAndCamera(void);
    void populateMarketDemoData(void);
    void populateMoneyDemoData(void);
    void populateAccountManagementDemoData(void);
    void populateGuildMortarData(void);
    void populateGuildTowerData(void);
    void syncHudStatusWidgets(const Player& player);

    bool shipAutoFollowEnabled;        /**< True tant que la camera suit auto le navire. */
    std::string playerShipFolderPath;  /**< Dossier ship (ex: assets/images/ships/ship-elite27). */
    int playerExperiencePointsCurrent; /**< Points d'experience courants pilotes par la scene gameplay. */
    //std::string playerVfxFolderPath;   /**< Dossier VFX (ex: assets/images/vfx/vfx-cannon). */
    //VFX shipVfx;                       /**< VFX runtime attache au navire joueur. */

public:
    /**
     * @brief Constructeur de la scene gameplay.
     */
    GameScene(void);

    /**
     * @brief Definit les points d'experience affiches par le HUD gameplay.
     *
     * Cette valeur est memorisee par la scene puis republiee vers les widgets
     * HUD lors des synchronisations de frame.
     *
     * @param points Points d'experience courants a afficher.
     */
    void setExperiencePointsCurrent(int points);

    /**
     * @brief Decharge les ressources de la scene.
     */
    void unload(void) override;

    /**
     * @brief Charge les ressources de la scene.
     */
    void load(void) override;

    /**
     * @brief Met a jour la logique gameplay.
     * @param dt Delta time en secondes.
     */
    void update(double dt) override;

    /**
     * @brief Dessine la scene gameplay.
     */
    void draw(void) override;

    /**
     * @brief Callback clavier de la scene gameplay.
     */
    void keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Callback clic souris de la scene gameplay.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;

    /**
     * @brief Callback molette souris / scroll trackpad.
     */
    void mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID) override;
};
