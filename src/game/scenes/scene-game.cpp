#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"
#include "game/shaders/gameplay-shader-controller.h"

#include <vector>

GameScene::GameScene(void)
    : shipAutoFollowEnabled(true),
      hudOverlay{},
      playerShipFolderPath("assets/images/ships/ship-elite27")
      //playerVfxFolderPath("assets/images/vfx/vfx-speedwhitedeux"),
      //shipVfx{}
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

void GameScene::populateMarketDemoData(void)
{
    using Category = MarketsAndBazarWidget::MarketCategory;

    const std::vector<MarketsAndBazarWidget::BazarRow> bazarRows = {
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "Eliteball", "Degats: 50", Category::MUNITION_DE_CANNON, 12, "PerFeck", "0"},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Elite Class 1", "Sante: 75.000", Category::NAVIRES, 1, "", "0"},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "San Salvador", "Sante: 50.000", Category::NAVIRES, 1, "", "0"},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "15Kg Cannon", "Dommages canon: 24", Category::CANNONS, 9, "Canakkale_1915", "0"},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Advanced Sail", "Bonus vitesse: 6%", Category::VOILES, 7, "Canakkale_1915", "0"},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Healball", "Reparation: 25", Category::CONSOMMABLES, 2500, "BeNiZz", "0"},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Gold Harpoon", "Degats: 250", Category::MUNITION_DE_HARPON, 15, "PerFeck", "0"},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Ruthless Pirate", "Navire legendaire", Category::NAVIRES, 1, "", "0"},
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "King's Legacy", "Dommages harpon: +8%", Category::BOOSTER, 3, "Santiago", "0"},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Abyss Cannon", "Precision: +5%", Category::CANNONS, 2, "Asterion", "0"},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Frostburn Sail", "Vitesse: +4%", Category::VOILES, 4, "Nox", "0"},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Guardian Hull", "Resistance: +7%", Category::BOOSTER, 5, "", "0"}
    };

    const auto buildPriceLines = [](
        int goldAmount,
        int rubiesAmount,
        int crystalAmount) -> std::vector<MarketsAndBazarWidget::MarketPriceData>
    {
        std::vector<MarketsAndBazarWidget::MarketPriceData> lines;
        if (goldAmount > 0)
        {
            lines.push_back(MarketsAndBazarWidget::MarketPriceData{goldAmount, MarketsAndBazarWidget::MarketCurrency::GOLD});
        }
        if (rubiesAmount > 0)
        {
            lines.push_back(MarketsAndBazarWidget::MarketPriceData{rubiesAmount, MarketsAndBazarWidget::MarketCurrency::RUBIES});
        }
        if (crystalAmount > 0)
        {
            lines.push_back(MarketsAndBazarWidget::MarketPriceData{crystalAmount, MarketsAndBazarWidget::MarketCurrency::CRISTAUX});
        }
        return lines;
    };

    const std::vector<MarketsAndBazarWidget::MarketRow> blackRows = {
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Silver Harpoon", "Degats: 150", Category::MUNITION_DE_HARPON, 7155, "1", buildPriceLines(300, 125, 2)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Explosive Rocket", "Degats: 5.000", Category::MUNITION_DE_CANNON, 626, "1", buildPriceLines(50000, 10000, 4)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Deceleration Rocket", "Ralentissement: 50%", Category::ACTIVABLES, 592, "1", buildPriceLines(80000, 0, 6)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Co2 Cartridge", "+20% degats harpon", Category::BOOSTER, 78846, "1", buildPriceLines(7500, 1515, 0)},
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "Hollowball", "Degats: 8", Category::MUNITION_DE_CANNON, 588573, "1", buildPriceLines(1, 0, 1)},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Sailors Salvation", "Supprime effets negatifs", Category::CONSOMMABLES, 481, "1", buildPriceLines(65000, 13015, 0)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Dragon Powder", "Dommages critiques +12%", Category::BOOSTER, 902, "1", buildPriceLines(9000, 3025, 8)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Titan Plate", "Coque renforcee", Category::BOOSTER, 1540, "1", buildPriceLines(14000, 2815, 0)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Phoenix Ammo", "Degats feu: 17", Category::MUNITION_DE_CANNON, 3200, "1", buildPriceLines(5200, 0, 10)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Storm Rocket", "Impact etourdissement", Category::UTILISABLE_SUR_CIBLE, 410, "1", buildPriceLines(33000, 11025, 0)}
    };

    const std::vector<MarketsAndBazarWidget::MarketRow> basicRows = {
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "Bois de coque", "Materiau de base de construction", Category::BOOSTER, 24000, "1", buildPriceLines(120, 35, 1)},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Toile de voile", "Renfort voilure", Category::VOILES, 18000, "1", buildPriceLines(145, 0, 0)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Fer brut", "Ressource de forge", Category::BOOSTER, 12500, "1", buildPriceLines(260, 70, 0)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Canon 12 livres", "Dommages canon: 12", Category::CANNONS, 3800, "1", buildPriceLines(2100, 0, 0)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Boulet perce-coque", "Degats coque: +6%", Category::MUNITION_DE_CANNON, 5300, "1", buildPriceLines(780, 200, 0)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Kit matelot", "Reduction cout equipage", Category::MATELOTS, 7200, "1", buildPriceLines(420, 0, 1)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Goudron naval", "Reparation progressive", Category::CONSOMMABLES, 4100, "1", buildPriceLines(930, 235, 0)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Lentille de vigie", "Portee de vue +3%", Category::BOOSTER, 2100, "1", buildPriceLines(1350, 0, 0)},
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "Carte de route", "XP navigation +2%", Category::UTILISABLE_SUR_CIBLE, 6500, "1", buildPriceLines(350, 95, 0)},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Pioche d'abordage", "Force abordage +4%", Category::HARPONEUSE, 2700, "1", buildPriceLines(1600, 0, 0)}
    };

    const std::vector<MarketsAndBazarWidget::MarketRow> eventRows = {
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Flamme lunaire", "Degats evenement: +10%", Category::BOOSTER, 850, "1", buildPriceLines(9900, 4990, 3)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Cle de faille", "Ouvre un coffre special", Category::UTILISABLE_SUR_CIBLE, 540, "1", buildPriceLines(18000, 9040, 4)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Banniere tempete", "Chance butin +6%", Category::ACTIVABLES, 720, "1", buildPriceLines(14500, 7290, 5)},
        {"assets/images/ammo/bazar-marche/ammo-rep-icon.png", "Poudre astrale", "Critique +9% en event", Category::BOOSTER, 430, "1", buildPriceLines(22000, 11040, 6)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Plaque abyssale", "Reduction degats boss", Category::CONSOMMABLES, 390, "1", buildPriceLines(25000, 12540, 3)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Harpon spectral", "Degats monstres marins +12%", Category::HARPONEUSE, 610, "1", buildPriceLines(16750, 8415, 4)},
        {"assets/images/ammo/bazar-marche/ammo-creux-icon.png", "Sceau royal", "Bonus reputation event", Category::CONSOMMABLES, 980, "1", buildPriceLines(8300, 4190, 5)},
        {"assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png", "Totem du capitaine", "Recharge competence -5%", Category::ACTIVABLES, 340, "1", buildPriceLines(27500, 13790, 6)},
        {"assets/images/ammo/bazar-marche/ammo-illu-icon.png", "Carte eclipse", "Acces zone cachee", Category::UTILISABLE_SUR_CIBLE, 250, "1", buildPriceLines(31000, 15540, 3)},
        {"assets/images/ammo/bazar-marche/ammo-explo-icon.png", "Voile comete", "Vitesse +7% pendant event", Category::VOILES, 460, "1", buildPriceLines(19800, 9940, 4)}
    };

    this->hudOverlay.setBazarRows(bazarRows);
    this->hudOverlay.setBlackMarketRows(blackRows);
    this->hudOverlay.setBasicMarketRows(basicRows);
    this->hudOverlay.setEventMarketRows(eventRows);
}

void GameScene::unload(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Player& player = GetGameState().player;

    // Libere les ressources du jeu.
    GameplayShaderController::unloadAll();
    //this->shipVfx.unload();
    player.unload();
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
    this->populateMarketDemoData();

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

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

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
    GameplayShaderController::updateVisibility(dt, player);

    // Met a jour tout le HUD (widgets + overlays monde).
    this->hudOverlay.update(dt, camera, map);

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
    this->hudOverlay.drawBackgroundWidget();

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
    this->hudOverlay.drawTileClickMarkerOverlay(map);

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
    this->hudOverlay.drawScrollBarOverlay(map);

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
    
    // Espace recentre la camera sur le joueur et reactive le suivi auto.
    if (scancode == SDL_SCANCODE_SPACE)
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

    // Si le clic tombe sur une barre de scroll, on ne le propage pas au reste.
    if (this->hudOverlay.handleMapOverlayMousePressed(x, y, button, map))
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
        this->hudOverlay.notifyMapTileClicked(tile.x, tile.y);
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




