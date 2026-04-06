#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"
#include "game/ui/hud/ingame-hud-overlay.h"
#include "game/ui/overlay/scroll-bar-overlay.h"
#include "game/ui/overlay/tile-click-marker.h"

/**
 * @brief Scene principale gameplay.
 */
class GameScene : public Scene {
private:
    void initializePlayerSpawnAndCamera(void);

    TileClickMarker clickMarker;       /**< Marqueur visuel de clic sur tuile. */
    ScrollBarOverlay scrollBarOverlay; /**< Barres de scroll avec coordonnees. */
    bool shipAutoFollowEnabled;        /**< True tant que la camera suit auto le navire. */
    IngameHudOverlay hudOverlay;       /**< UI gameplay (fond + minimap + bouton centre). */

public:
    /**
     * @brief Constructeur de la scene gameplay.
     */
    GameScene(void);

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
};
