#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"
#include "game/shaders/gameplay-shader-controller.h"
#include "game/ui/ingame-hud-overlay.h"

#include <algorithm>
#include <vector>

GameScene::GameScene(void)
    : shipAutoFollowEnabled(true),
      playerShipFolderPath{},
      playerExperiencePointsCurrent(0)
      //playerVfxFolderPath("assets/images/vfx/vfx-speedwhitedeux"),
      //shipVfx{}
{
}

void GameScene::setExperiencePointsCurrent(int points)
{
    this->playerExperiencePointsCurrent = (std::max)(0, points);
}

void GameScene::populateMoneyDemoData(void)
{
    GetIngameHudOverlay().getMoneyWidget().setCurrencyEntries(
        std::vector<MoneyWidget::CurrencyEntry>{
            {MoneyWidget::CurrencyType::GOLD, "assets/images/ui-scene-game/money-gold.png", 1250000},
            {MoneyWidget::CurrencyType::RUBIES, "assets/images/ui-scene-game/money-rubies.png", 3500}
        });
}

void GameScene::populateAccountManagementDemoData(void)
{
    using ShipEntry = AccountManagementWidget::ShipEntry;
    using OptionEntry = AccountManagementWidget::OptionEntry;
    using EliteProgressData = AccountManagementWidget::EliteProgressData;

    // Exemple de flux d'alimentation:
    // 1. le gameplay choisit le dossier du navire reel a charger en scene,
    // 2. il injecte ensuite les images d'apercu et les listes dans le widget.
    this->playerShipFolderPath = "assets/images/ships/bateau elite 4";

    GetIngameHudOverlay().getAccountManagementWidget().setPlayerIdentifier("1985");
    GetIngameHudOverlay().getAccountManagementWidget().setPirateSince("01.02.2024");
    GetIngameHudOverlay().getAccountManagementWidget().setPlayerLevel(10);
    GetIngameHudOverlay().getAccountManagementWidget().setExperiencePointsCurrent(this->playerExperiencePointsCurrent);
    GetIngameHudOverlay().getAccountManagementWidget().setEliteProgressData(
        EliteProgressData{
            4250000,
            5000000,
            true
        });
    GetIngameHudOverlay().getAccountManagementWidget().setCombatPointsCurrent(1460);
    GetIngameHudOverlay().getAccountManagementWidget().setPremiumSince("06.04.2026");
    GetIngameHudOverlay().getAccountManagementWidget().setProfileName(".Crows");

    GetIngameHudOverlay().getAccountManagementWidget().setEliteAcquiredShips(
        std::vector<ShipEntry>{
            {"Elite 1", "assets/images/ships/bateau elite 1"},
            {"Elite 2", "assets/images/ships/bateau elite 2"},
            {"Elite 3", "assets/images/ships/bateau elite 3"},
            {"Elite 4", "assets/images/ships/bateau elite 4"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setSpecialAcquiredShips(
        std::vector<ShipEntry>{
            {"Boreas", "assets/images/ships/Boreas (1)"},
            {"Fly dutchman", "assets/images/ships/Fly dutchman (1)"},
            {"Morgan Boucanier", "assets/images/ships/Morgan Boucanier (1)"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setShipBonusOptions(
        std::vector<OptionEntry>{
            {"Bateau elite 4", "assets/images/ships/bateau elite 4"},
            {"Bateau elite 3", "assets/images/ships/bateau elite 3"},
            {"Bateau elite 2", "assets/images/ships/bateau elite 2"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setShipStyleOptions(
        std::vector<OptionEntry>{
            {"Bateau elite 4", "assets/images/ships/bateau elite 4"},
            {"Bateau elite 5", "assets/images/ships/bateau elite 5"},
            {"Bateau elite 7", "assets/images/ships/bateau elite 7"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setRepairStyleOptions(
        std::vector<OptionEntry>{
            {"Reparation par defaut", "assets/images/ammo/bazar-marche/ammo-rep-icon.png"},
            {"Reparation emeraude", "assets/images/ammo/bazar-marche/ammo-creux-icon.png"},
            {"Reparation abyssale", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setSpeedStyleOptions(
        std::vector<OptionEntry>{
            {"Vitesse blanche", "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/2.png"},
            {"Vitesse tempete", "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/10.png"},
            {"Vitesse neon", "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/30.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setProjectileImpactStyleOptions(
        std::vector<OptionEntry>{
            {"Impact standard", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"},
            {"Impact royal", "assets/images/ammo/bazar-marche/ammo-illu-icon.png"},
            {"Impact titan", "assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setRocketStyleOptions(
        std::vector<OptionEntry>{
            {"Fusee comete", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"},
            {"Fusee oracle", "assets/images/ammo/bazar-marche/ammo-creux-icon.png"},
            {"Fusee solaire", "assets/images/ammo/bazar-marche/ammo-rep-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setProjectileStyleOptions(
        std::vector<OptionEntry>{
            {"Boulet lourd", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"},
            {"Boulet arc", "assets/images/ammo/bazar-marche/ammo-illu-icon.png"},
            {"Boulet obsidienne", "assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setMoveClickStyleOptions(
        std::vector<OptionEntry>{
            {"Clic tempete", "assets/images/ammo/bazar-marche/ammo-rep-icon.png"},
            {"Clic royal", "assets/images/ammo/bazar-marche/ammo-illu-icon.png"},
            {"Clic aurore", "assets/images/ammo/bazar-marche/ammo-creux-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setEmoteOptions(
        std::vector<OptionEntry>{
            {"Hello", "assets/images/ammo/bazar-marche/ammo-illu-icon.png"},
            {"Attack", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"},
            {"Laugh", "assets/images/ammo/bazar-marche/ammo-creux-icon.png"},
            {"Lets go", "assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png"},
            {"Support", "assets/images/ammo/bazar-marche/ammo-rep-icon.png"},
            {"Bravo", "assets/images/ammo/bazar-marche/ammo-illu-icon.png"}
        });

    GetIngameHudOverlay().getAccountManagementWidget().setStorageEquipmentOptions(
        std::vector<OptionEntry>{
            {"Cannons", "assets/images/ammo/bazar-marche/ammo-explo-icon.png"},
            {"Harponneuse", "assets/images/ammo/bazar-marche/ammo-rep-icon.png"},
            {"Voiles", "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/6.png"}
        });
}

void GameScene::syncHudStatusWidgets(const Player& player)
{
    GetIngameHudOverlay().getHpBarWidget().setMaxHp(player.getHpMax());
    GetIngameHudOverlay().getHpBarWidget().setCurrentHp(player.getHpCurrent());
    GetIngameHudOverlay().getExperienceBarWidget().setCurrentExperiencePoints(this->playerExperiencePointsCurrent);
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

    // Load le VFX du navire du joueur.
    /*if (!this->shipVfx.loadFromFolders(this->playerShipFolderPath.c_str(), this->playerVfxFolderPath.c_str()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement VFX (shipFolder='%s', vfxFolder='%s')",
            this->playerShipFolderPath.c_str(),
            this->playerVfxFolderPath.c_str());
        return;
    }*/

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
    //this->shipVfx.unload();
    player.unload();
    GetIngameHudOverlay().unload();
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
    GetIngameHudOverlay().load();
    this->syncHudStatusWidgets(player);
    this->populateMoneyDemoData();
    this->populateAccountManagementDemoData();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Initialise le spawn joueur + camera de depart.
    this->initializePlayerSpawnAndCamera();

}

void GameScene::update(double dt)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    Camera& camera = GetCamera();
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Applique les options graphiques qui pilotent les passes animees.
    oceanShader.setWakeTrailsEnabled(gameSettings.getShipWakeTrailsEnabled());

    // Met a jour le shader ocean.
    oceanShader.update(dt);

    // Met a jour le joueur (deplacement, animation, etc).
    // + synchronisation avec le shader ocean pour les effets de wake.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    oceanShader.endWakeFrame(map, map.rect);

    // Met a jour les VFX du navire du joueur.
    //this->shipVfx.update(dt, player.getShip(), nullptr);

    // Met a jour les shaders de visibilite (nuages + fog).
    GameplayShaderController::updateVisibility(dt, player, gameSettings.getFogOfWarEnabled());

    this->syncHudStatusWidgets(player);

    // Met a jour tout le HUD (widgets + overlays monde).
    GetIngameHudOverlay().update(dt, camera, map);

    // Deplacement camera continu avec les touches configurees dans l'onglet Controles.
    if (GameplayCameraController::updateKeyboardScroll(
            dt,
            camera,
            map,
            map.rect,
            GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_UP),
            GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_DOWN),
            GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_LEFT),
            GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_RIGHT),
            GetIngameHudOverlay().getGameSettingsWidget().getCameraScrollSpeedSectors()))
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
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Dessine le fond UI en premier (coordonnees logiques absolues).
    GetIngameHudOverlay().drawBackgroundWidget();

    // Clip strict du rendu gameplay dans la zone map.
    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    // Dessine l'ocean.
    if (oceanShader.isReady())
    {
        oceanShader.draw(map.rect);
    }

    // Dessine le fog-of-war au-dessus de l'ocean.
    if (gameSettings.getFogOfWarEnabled() && fogOfWarShader.isReady())
    {
        fogOfWarShader.draw(map.rect);
    }

    // Dessine le marqueur de clic.
    GetIngameHudOverlay().drawTileClickMarkerOverlay(map);

    // Dessine les VFX derriere le ship.
    //this->shipVfx.draw(map, player.getShip(), true);

    // Dessine le joueur.
    player.draw(map);

    // Dessine les VFX devant le ship.
    //this->shipVfx.draw(map, player.getShip(), false);

    // Dessine les nuages par-dessus le joueur pour un rendu "au-dessus".
    if (visionCloudShader.isReady())
    {
        visionCloudShader.draw(map.rect);
    }

    // Dessine les barres de scroll par-dessus tout.
    GetIngameHudOverlay().drawScrollBarOverlay(map);

    // Fin du clip monde: l'overlay/UI peut dessiner librement.
    WorldRenderClip::end(renderer);

    // Dessine les elements d'interface.
    GetIngameHudOverlay().drawWidgets(map, player);
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
    if (GetIngameHudOverlay().keypressed(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    // La touche configuree recentre la camera sur le joueur et reactive le suivi auto.
    if (scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
        this->shipAutoFollowEnabled = true;
        cameraChanged = true;
    }

    if (!isrepeat &&
        scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::TOGGLE_MINIMAP))
    {
        GetIngameHudOverlay().toggleHudWidgetVisibility(GameSettingsWidget::HudScaleTarget::MINIMAP);
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
    Camera& camera = GetCamera();
    IngameHudOverlay& hudOverlay = GetIngameHudOverlay();

    // Priorite au chat HUD: clic consomme => pas de propagation gameplay.
    if (hudOverlay.mousepressed(x, y, button, clicks, mouseID))
    {
        return;
    }

    if (hudOverlay.centerShipButtonMousepressed(x, y, button))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
        this->shipAutoFollowEnabled = true;
        camera.update(map, map.rect);
        return;
    }

    // Si le clic tombe sur une barre de scroll, on ne le propage pas au reste.
    if (hudOverlay.handleMapOverlayMousePressed(x, y, button, camera, map))
    {
        this->shipAutoFollowEnabled = false;
        return;
    }

    // Seuls les clics gauche sont traites pour le gameplay.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
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
    if (!map.isInside(tile.x, tile.y) || map.isTileBlocked(tile.x, tile.y))
    {
        return;
    }
    else
    {
        // Deplace le joueur vers la tuile cliquee.
        player.moveToTile(map, tile.x, tile.y);

        // Affiche le marqueur de clic sur la tuile cliquee.
        GetIngameHudOverlay().notifyMapTileClicked(tile.x, tile.y);
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
    if (GetIngameHudOverlay().mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
    {
        return;
    }
}
