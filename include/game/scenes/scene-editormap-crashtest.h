#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/overlay/scroll-bar-overlay.h"

/**
 * @class EditorMapCrashTestScene
 * @brief Scene de stress-test map avec flotte scalable.
 *
 * Objectif:
 * - reutiliser le vrai runtime Ship (A* + update deplacements);
 * - simuler des ordres reseau tileX/tileY puis appeler moveToTile;
 * - valider camera, zoom, scrollbars et minimap sous forte charge.
 */
class EditorMapCrashTestScene : public Scene {
private:
    struct SimShipState {
        int nextCommandDirectionIndex = 0; /**< Prochaine direction de cible reseau [0..7]. */
        double pauseBeforeNextCommandSec = 0.0; /**< Pause idle avant prochain ordre reseau. */
        uint32_t commandRngState = 0U; /**< Etat RNG local pour les cibles fallback. */
        Ship::HealthVisual healthVisual = Ship::HealthVisual::FULL; /**< Variante visuelle appliquee au rendu. */
    };
    struct SimNetworkPlayer {
        uint32_t playerId = 0U; /**< Identifiant joueur simule (proxy reseau). */
        Ship ship; /**< Navire runtime associe au joueur. */
        SimShipState shipState; /**< Etat de simulation reseau du navire. */
        double networkCommandCooldownSec = 0.0; /**< Delai avant prochain paquet de deplacement reseau. */
    };

    BackgroundWidget backgroundWidget; /**< Fond UI scene de jeu. */
    RC2D_Font overlayFont; /**< Police overlay/hud. */
    ScrollBarOverlay scrollBarOverlay; /**< Scrollbars monde (haut/bas/gauche/droite). */

    Ship renderShipPrototype; /**< Prototype unique charge pour dessiner tous les navires. */
    bool renderShipLoaded; /**< True si le prototype est charge. */
    std::string loadedShipFolderPath; /**< Dossier sprites utilise pour le rendu. */

    std::vector<SimNetworkPlayer> simPlayers; /**< Liste de joueurs simules (1 joueur = 1 navire). */
    std::size_t retargetCursor; /**< Curseur round-robin des retargets A*. */
    double retargetTickAccumulatorSec; /**< Accumulateur pour limiter la frequence des retargets A*. */
    bool simulationPaused; /**< Pause/reprise globale de la simulation. */
    int simulationShipCount; /**< Nombre cible de navires simules. */

    int selectedOceanColorIndex; /**< Couleur ocean active. */
    int pendingOceanColorDelta; /**< Delta ocean en attente. */

    SDL_FRect buttonRebuildRect; /**< Bouton rebuild (+500 navires). */
    SDL_FRect buttonPauseRect; /**< Bouton pause/reprise simulation. */
    SDL_FRect buttonCenterRect; /**< Bouton centrage camera map. */
    SDL_FRect buttonZoomOutRect; /**< Bouton zoom -. */
    SDL_FRect buttonZoomInRect; /**< Bouton zoom +. */
    SDL_FRect buttonOceanPrevRect; /**< Bouton ocean -. */
    SDL_FRect buttonOceanNextRect; /**< Bouton ocean +. */
    SDL_FRect miniMapRect; /**< Rectangle minimap. */

    bool miniMapDragActive; /**< Drag minimap actif. */
    float miniMapDragOffsetX; /**< Offset drag minimap X. */
    float miniMapDragOffsetY; /**< Offset drag minimap Y. */

    std::string statusMessage; /**< Message statut HUD. */

    /** @brief Reinitialise l'etat runtime scene. */
    void resetSceneState(void);
    /** @brief Charge le navire prototype (dossier auto-detecte). */
    bool loadRenderShipPrototype(void);
    /** @brief Liste les dossiers ship candidats (1.png..8.png). */
    std::vector<std::string> collectShipFolderCandidates(void) const;
    /** @brief Construit la flotte de simulation. */
    void rebuildSimulationShips(int shipCount);
    /** @brief Met a jour la simulation deplacement. */
    void updateSimulation(double dt);
    /** @brief Calcule la prochaine cible tileX/tileY provenant du "reseau" simule. */
    bool computeNextNetworkTargetTileForPlayer(std::size_t playerIndex, SDL_Point* outTargetTile);
    /** @brief Simule la reception d'un ordre de deplacement reseau (tileX/tileY). */
    bool issueNextSimulatedNetworkMoveForPlayer(std::size_t playerIndex);

    /** @brief Applique la couleur ocean selectionnee. */
    void applySelectedOceanColor(void);
    /** @brief Enfile un changement de couleur ocean. */
    void requestOceanColorStep(int delta);
    /** @brief Applique le delta ocean pending. */
    void applyPendingOceanColorStep(void);
    /** @brief Cycle direct de couleur ocean. */
    void cycleOceanColor(int delta);

    /** @brief Recalcule les layouts de toolbar/minimap. */
    void updateToolbarLayout(void);
    /** @brief Dessine un bouton toolbar standard. */
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    /** @brief Dessine les navires de simulation visibles. */
    void drawSimulationShips(void);
    /** @brief Dessine la minimap de stress-test. */
    void drawMiniMap(void) const;
    /** @brief Dessine le HUD texte + toolbar. */
    void drawHud(void) const;

    /** @brief Construit le rect de vue camera sur minimap. */
    bool tryBuildMiniMapViewRect(SDL_FRect* outRect) const;
    /** @brief Deplace la camera depuis un point minimap. */
    void moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset);
    /** @brief Gere un clic minimap. */
    bool handleMiniMapClick(float x, float y, RC2D_MouseButton button);
    /** @brief Gere le drag minimap continu. */
    void handleMiniMapDragFromMouse(void);

    /** @brief Gere un clic toolbar. */
    bool handleToolbarClick(float x, float y);
    /** @brief Test point dans rectangle. */
    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    /** @brief Convertit des coordonnees fenetre en coordonnees render. */
    void convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const;
    /** @brief Recupere la souris en coordonnees render. */
    bool getMouseRenderPosition(float* outX, float* outY) const;

public:
    EditorMapCrashTestScene(void);
    ~EditorMapCrashTestScene(void) override;

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
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

#endif // GAME_ENV_DEV
