#pragma once

#include <RC2D/RC2D.h>

#include "game/entities/player.h"
#include "game/map/map.h"
#include "game/scenes/scene.h"
#include "game/shaders/fog-of-war-shader.h"
#include "game/shaders/ocean-shader.h"
#include "game/ui/tile-click-marker.h"

/**
 * @brief Scene principale gameplay.
 *
 * Cette scene orchestre:
 * - les shaders ocean/fog;
 * - la map isometrique;
 * - l'entite joueur locale;
 * - le feedback visuel des clics sur tuiles.
 */
class GameScene : public Scene {
private:
    /**
     * @brief Sous-module shaders pour garder GameScene compacte.
     */
    struct Shaders {
        OceanShader oceanShader;            /**< Shader ocean. */
        FogOfWarShader fogOfWarShader;      /**< Shader fog-of-war. */
        OceanShader::WaterColor oceanColor; /**< Couleur ocean active. */

        /**
         * @brief Constructeur du sous-module shaders.
         */
        Shaders(void);

        /**
         * @brief Charge les shaders de la scene.
         */
        void load(void);

        /**
         * @brief Decharge les shaders de la scene.
         */
        void unload(void);

        /**
         * @brief Met a jour les shaders.
         * @param dt Delta time en secondes.
         */
        void update(double dt);

        /**
         * @brief Dessine les shaders.
         * @param visibleRect Rectangle visible de rendu.
         */
        void draw(const SDL_FRect& visibleRect);
    };

    Shaders shaders;             /**< Module shaders de la scene. */
    Map map;                     /**< Map de gameplay. */
    Player player;               /**< Joueur local. */
    TileClickMarker clickMarker; /**< Marqueur de clic sur tuile. */

    /**
     * @brief Configure la base gameplay (map + joueur).
     */
    void configureGameplay(void);

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
     * @param key Representation texte de la touche.
     * @param scancode Scancode physique SDL.
     * @param keycode Keycode logique SDL.
     * @param mod Modificateurs clavier SDL.
     * @param isrepeat True si repetition clavier.
     * @param keyboardID Identifiant clavier SDL.
     */
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;

    /**
     * @brief Callback clic souris de la scene gameplay.
     * @param x Position ecran X du clic.
     * @param y Position ecran Y du clic.
     * @param button Bouton souris RC2D.
     * @param clicks Nombre de clics.
     * @param mouseID Identifiant souris SDL.
     */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
