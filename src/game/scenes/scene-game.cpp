#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"
#include "game/shaders/gameplay-shader-controller.h"

GameScene::GameScene(void)
    : clickMarker{},
      scrollBarOverlay{},
      shipAutoFollowEnabled(true),
      hudOverlay{},
      playerShipFolderPath("assets/images/ships/ship-elite27"),
      playerVfxFolderPath("assets/images/vfx/vfx-cannon"),
      shipVfx{}
{
}

void GameScene::initializePlayerSpawnAndCamera(void)
{
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    Camera& camera = GetCamera();
    const std::string& shipFolderPath = this->playerShipFolderPath;

    // Load le navire du joueur.
    if (!player.loadShip(shipFolderPath.c_str()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement navire '%s'",
            shipFolderPath.c_str());
    }

    // Load le VFX de tir du joueur.
    if (!this->shipVfx.loadFromFolders(this->playerShipFolderPath.c_str(), this->playerVfxFolderPath.c_str()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement VFX (shipFolder='%s', vfxFolder='%s')",
            this->playerShipFolderPath.c_str(),
            this->playerVfxFolderPath.c_str());
        return;
    }

    // Spawn au secteur 30-AE (centre approximatif).
    player.spawnOnSector(map, 30, 30);

    // Centre la camera sur le joueur au debut.
    GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
    this->shipAutoFollowEnabled = true;
    camera.update(map, map.rect);
}

void GameScene::unload(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Player& player = GetGameState().player;

    // Libere les ressources du jeu.
    GameplayShaderController::unloadAll();
    this->shipVfx.unload();
    player.unload();
    this->clickMarker.hide();
    this->scrollBarOverlay.unload();
    this->hudOverlay.unload();
}

void GameScene::load(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;

    // Configure le joueur.
    player.load();

    // Charge les shaders de gameplay.
    GameplayShaderController::loadAll();

    // Charge les ressources HUD (interface utilisateur).
    this->hudOverlay.load();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Initialise le spawn joueur + camera de depart.
    this->initializePlayerSpawnAndCamera();

    // Charge l'overlay des barres de scroll.
    this->scrollBarOverlay.load();
}

void GameScene::update(double dt)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    Camera& camera = GetCamera();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Met a jour le shader ocean.
    oceanShader.update(dt);

    // Met a jour le joueur (deplacement, animation, etc).
    // + synchronisation avec le shader ocean pour les effets de wake.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    oceanShader.endWakeFrame(map, map.rect);
    this->shipVfx.update(dt, player.getShip());

    // Met a jour les shaders de visibilite (nuages + fog).
    GameplayShaderController::updateVisibility(dt, player);

    // Met a jour le marqueur de clic.
    this->clickMarker.update(dt);

    // Met a jour les widgets HUD interactifs (chat, curseur, scrollbar chat...).
    this->hudOverlay.update(dt);

    // Met a jour les barres de scroll.
    this->scrollBarOverlay.update(dt, camera, map, map.rect);

    // Deplacement camera continu aux fleches clavier.
    if (GameplayCameraController::updateKeyboardScroll(dt, camera, map, map.rect))
    {
        // L'utilisateur prend le controle manuel de la camera.
        this->shipAutoFollowEnabled = false;
    }

    // Tant qu'aucun controle camera manuel n'est utilise,
    // la camera suit en permanence le navire.
    if (this->shipAutoFollowEnabled)
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
    }

    // Applique la camera finale (zoom + position) sur la map.
    camera.update(map, map.rect);
}

void GameScene::draw(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    // Dessine le fond UI en premier (coordonnees logiques absolues).
    this->hudOverlay.drawBackground();

    // Clip strict du rendu gameplay dans la zone map.
    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    // Dessine l'ocean.
    if (oceanShader.isReady())
    {
        oceanShader.draw(map.rect);
    }

    // Dessine le fog-of-war au-dessus de l'ocean.
    if (fogOfWarShader.isReady())
    {
        fogOfWarShader.draw(map.rect);
    }

    // Dessine le marqueur de clic.
    this->clickMarker.draw(map);

    // Dessine les VFX derriere le ship.
    this->shipVfx.draw(map, player.getShip(), true);

    // Dessine le joueur.
    player.draw(map);

    // Dessine les VFX devant le ship.
    this->shipVfx.draw(map, player.getShip(), false);

    // Dessine les nuages par-dessus le joueur pour un rendu "au-dessus".
    if (visionCloudShader.isReady())
    {
        visionCloudShader.draw(map.rect);
    }

    // Dessine les barres de scroll par-dessus tout.
    this->scrollBarOverlay.draw(map.rect, map);

    // Fin du clip monde: l'overlay/UI peut dessiner librement.
    WorldRenderClip::end(renderer);

    // Dessine les elements d'interface.
    this->hudOverlay.drawWidgets(map, player);
}

void GameScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    Camera& camera = GetCamera();
    bool cameraChanged = false;

    // Priorite au chat HUD: si la touche est consommee par l'UI, on stop ici.
    if (this->hudOverlay.keypressed(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }
    
    // Seules les touches de zoom et recentrage sont traitees ici.
    if (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_EQUALS)
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        cameraChanged = true;
        RC2D_log(RC2D_LOG_DEBUG, "Camera zoom: %.2f", camera.getZoomFactor());
    }
    else if (scancode == SDL_SCANCODE_KP_MINUS || scancode == SDL_SCANCODE_MINUS)
    {
        camera.setZoomFactor(camera.getZoomFactor() - 0.05f);
        cameraChanged = true;
        RC2D_log(RC2D_LOG_DEBUG, "Camera zoom: %.2f", camera.getZoomFactor());
    }
    else if (scancode == SDL_SCANCODE_SPACE)
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
        this->shipAutoFollowEnabled = true;
        cameraChanged = true;
    }
    // Applique la camera si elle a ete modifiee.
    if (cameraChanged)
    {
        camera.update(map, map.rect);
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;

    // Priorite au chat HUD: clic consomme => pas de propagation gameplay.
    if (this->hudOverlay.mousepressed(x, y, button, clicks, mouseID))
    {
        return;
    }

    // Si ce n'est pas un clic gauche, on ignore l'evenement.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Si le clic tombe sur une barre de scroll, on ne le propage pas au reste.
    if (this->scrollBarOverlay.handleClick(x, y, map.rect))
    {
        this->shipAutoFollowEnabled = false;
        return;
    }

    // Ignore les clics en dehors de la zone map (GUI en haut/bas).
    if (x < map.rect.x || x > (map.rect.x + map.rect.w) ||
        y < map.rect.y || y > (map.rect.y + map.rect.h))
    {
        return;
    }

    // Convertit les coordonnees de clic en coordonnees de tuile.
    const SDL_Point tile = map.screenToTileNearest(x, y);

    // Verifie que la tuile est dans la map et traversable.
    if (map.isInside(tile.x, tile.y) && !map.isTileBlocked(tile.x, tile.y))
    {
        // Deplace le joueur vers la tuile cliquee.
        player.moveToTile(map, tile.x, tile.y);

        // Affiche le marqueur de clic sur la tuile cliquee.
        this->clickMarker.show(tile.x, tile.y);
    }
}

void GameScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    // Priorite au chat HUD pour la molette (souris + trackpad).
    if (this->hudOverlay.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
    {
        return;
    }
}


