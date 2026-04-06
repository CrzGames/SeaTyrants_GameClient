#include "game/scenes/scene-game.h"

#include "core/context.h"

GameScene::GameScene(void)
    : clickMarker{}
{
}

void GameScene::unload(void)
{
    // Recupere les references aux systemes et objets necessaires.
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    Player& player = GetGameState().player;

    // Libere les ressources du jeu.
    oceanShader.unload();
    visionCloudShader.unload();
    fogOfWarShader.unload();
    player.unload();
    this->clickMarker.hide();
    this->scrollBarOverlay.unload();
}

void GameScene::load(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    Player& player = gameState.player;
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();

    // Configure le joueur.
    player.load();

    // Charge les shaders.
    if (!oceanShader.load(OceanShader::WaterColor::YELLOW))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement ocean shader");
    }
    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: echec chargement fog-of-war shader");
    }
    if (!visionCloudShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: echec chargement vision-cloud shader");
    }

    // Configure l'UI de minimap.
    this->minimapUI.image = rc2d_graphics_loadImageFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.anchor = RC2D_UI_ANCHOR_TOP_RIGHT;
    this->minimapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->minimapUI.margin_x = 0.03f;
    this->minimapUI.margin_y = 0.05f;
    this->minimapUI.visible = true;
    this->minimapUI.hittable = true;

    // Configure l'UI du bouton centrer la map.
    this->buttonCenterMapUI.image = rc2d_graphics_loadImageFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUI.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUI.anchor = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    this->buttonCenterMapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->buttonCenterMapUI.margin_x = 0.0f;
    this->buttonCenterMapUI.margin_y = 0.25f;
    this->buttonCenterMapUI.visible = true;
    this->buttonCenterMapUI.hittable = true;

    // Load le navire du joueur.
    if (!player.loadShip("assets/atlas/elite21", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", "assets/atlas/elite20");
    }

    // Spawn au secteur 30-AE (centre approximatif).
    const SDL_Point spawnTile = map.sectorToTile(30, 30);
    player.spawnOnTile(map, spawnTile.x, spawnTile.y);

    // Centre la camera sur le joueur au debut.
    camera.centerCameraOnTile(
        static_cast<float>(spawnTile.x),
        static_cast<float>(spawnTile.y),
        map,
        gameScreen.rect);
    camera.applyToMap(map, gameScreen.rect);

    // Charge l'overlay des barres de scroll.
    this->scrollBarOverlay.load();
}

void GameScene::update(double dt)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();

    // Applique la camera (position + zoom) sur la map.
    camera.applyToMap(map, gameScreen.rect);

    // Met a jour le shader ocean.
    oceanShader.update(dt);

    // Met a jour le joueur (deplacement, animation, etc).
    // + synchronisation avec le shader ocean pour les effets de wake.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    oceanShader.endWakeFrame(map, gameScreen.rect);

    // Met a jour le shader fog a partir des donnees joueur.
    const SDL_FPoint playerTile = player.getTilePosition();
    const float playerViewRangeTiles = player.getViewRangeTiles();
    const float visionCloudFalloffTiles = 5.5f;
    visionCloudShader.update(
        dt,
        playerTile,
        playerViewRangeTiles,
        visionCloudFalloffTiles);

    const float fogViewFalloffTiles = 4.5f; // Ajustable: plus grand = bord plus doux.
    fogOfWarShader.update(
        dt,
        oceanShader.getColorMode(),
        playerTile,
        playerViewRangeTiles,
        fogViewFalloffTiles);

    // Met a jour le marqueur de clic.
    this->clickMarker.update(dt);

    // Met a jour les barres de scroll.
    this->scrollBarOverlay.update(dt, camera, map, gameScreen.rect);

    // Deplace la camera avec les fleches du clavier (scroll continu).
    const bool upPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_UP);
    const bool downPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_DOWN);
    const bool leftPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LEFT);
    const bool rightPressed =
        rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RIGHT);

    float deltaSectorX = 0.0f;
    float deltaSectorY = 0.0f;
    deltaSectorX *= Camera::CAMERA_DIAGONAL_FACTOR;
    deltaSectorY *= Camera::CAMERA_DIAGONAL_FACTOR;

    if (upPressed)
    {
        deltaSectorY -= 1.0f;
    }
    if (downPressed)
    {
        deltaSectorY += 1.0f;
    }
    if (leftPressed)
    {
        deltaSectorX -= 1.0f;
    }
    if (rightPressed)
    {
        deltaSectorX += 1.0f;
    }

    if (deltaSectorX != 0.0f || deltaSectorY != 0.0f)
    {
        // Normalisation des diagonales pour garder la meme vitesse
        // que les directions simples.
        if (deltaSectorX != 0.0f && deltaSectorY != 0.0f)
        {
            deltaSectorX *= Camera::CAMERA_DIAGONAL_FACTOR;
            deltaSectorY *= Camera::CAMERA_DIAGONAL_FACTOR;
        }

        const float sectorDistance = Camera::CAMERA_SCROLL_SPEED_SECTORS * static_cast<float>(dt);

        deltaSectorX *= sectorDistance;
        deltaSectorY *= sectorDistance;

        const float deltaTileX =
            (deltaSectorX + deltaSectorY) * static_cast<float>(Map::SECTOR_STEP);
        const float deltaTileY =
            (deltaSectorY - deltaSectorX) * static_cast<float>(Map::SECTOR_STEP);

        camera.moveCameraTiles(deltaTileX, deltaTileY, map, gameScreen.rect);
    }
}

void GameScene::draw(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();

    // Dessine l'ocean.
    if (oceanShader.isReady())
    {
        oceanShader.draw(gameScreen.rect);
    }

    // Dessine le fog-of-war au-dessus de l'ocean.
    if (fogOfWarShader.isReady())
    {
        fogOfWarShader.draw(gameScreen.rect);
    }

    // Dessine le marqueur de clic.
    this->clickMarker.draw(map);

    // Dessine le joueur par-dessus l'ocean et le fog.
    player.draw(map);

    // Dessine les nuages par-dessus le joueur pour un rendu "au-dessus".
    if (visionCloudShader.isReady())
    {
        visionCloudShader.draw(gameScreen.rect);
    }

    // Dessine les barres de scroll par-dessus tout.
    this->scrollBarOverlay.draw(gameScreen.rect, map);

    // Dessine les elements d'interface.
    rc2d_ui_drawImage(&this->minimapUI);
    rc2d_ui_drawImage(&this->buttonCenterMapUI);
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
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();
    bool cameraChanged = false;

    // Seules les touches de zoom et recentrage sont traitées ici.
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
        const SDL_FPoint shipTile = player.getTilePosition();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, gameScreen.rect);
        cameraChanged = true;
    }

    // Applique la camera si elle a été modifiée.
    if (cameraChanged)
    {
        camera.applyToMap(map, gameScreen.rect);
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;

    // Si ce n'est pas un clic gauche, on ignore l'evenement.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Si le clic tombe sur une barre de scroll, on ne le propage pas au reste.
    GameScreen& gameScreen = GetGameScreen();
    if (this->scrollBarOverlay.handleClick(x, y, gameScreen.rect))
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
