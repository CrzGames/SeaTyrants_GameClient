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
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    Player& player = GetGameState().player;

    // Libere les ressources du jeu.
    oceanShader.unload();
    fogOfWarShader.unload();
    player.unload();
    this->clickMarker.hide();
}

void GameScene::load(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    Player& player = gameState.player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();

    // Configure le joueur.
    player.load();

    // Configure la map de gameplay.
    map.setTileSize(48.0f, 32.0f);
    map.setMapSize(256, 256);

    // Charge les shaders.
    if (!oceanShader.load(OceanShader::WaterColor::BLUE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement ocean shader");
    }
    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: echec chargement fog-of-war shader");
    }

    // Configure l'UI de minimap.
    this->minimapUI.image = rc2d_graphics_loadImageFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.imageData = rc2d_graphics_loadImageDataFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.anchor = RC2D_UI_ANCHOR_TOP_RIGHT;
    this->minimapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->minimapUI.margin_x = 0.01f;
    this->minimapUI.margin_y = 0.01f;
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

    // Configure le joueur sur la map.
    if (!player.loadShip("assets/atlas/elite20", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", "assets/atlas/elite20");
    }

    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;
    player.spawnOnTile(map, spawnTileX, spawnTileY);

    // Initialise la camera gameplay sur la position du joueur.
    camera.centerCameraOnTile(static_cast<float>(spawnTileX), static_cast<float>(spawnTileY), map, gameScreen.rect);
    camera.applyToMap(map, gameScreen.rect);
}

void GameScene::update(double dt)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();

    // Applique la camera (position + zoom) sur la map.
    camera.applyToMap(map, gameScreen.rect);

    // Met a jour les shaders ocean et fog.
    oceanShader.update(dt);
    fogOfWarShader.update(dt, oceanShader.getColorMode());

    // Met a jour les elements de la scene.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    this->clickMarker.update(dt);
    oceanShader.endWakeFrame(map, gameScreen.rect);
}

void GameScene::draw(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();

    // Dessine l'ocean et le fog-of-war.
    if (oceanShader.isReady() && fogOfWarShader.isReady())
    {
        oceanShader.draw(gameScreen.rect);
        // fogOfWarShader.draw(gameScreen.rect);
    }

    // Dessine les elements de la scene.
    this->clickMarker.draw(map);
    player.draw(map);

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
    (void)key;
    (void)keycode;
    (void)mod;
    (void)isrepeat;
    (void)keyboardID;

    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    GameScreen& gameScreen = GetGameScreen();
    Camera& camera = GetCamera();
    bool cameraChanged = false;

    // Deplacement camera avec les fleches (combinaisons incluses pour diagonales).
    // Mapping isometrique:
    // up         = (-1, -1)
    // down       = (+1, +1)
    // left       = (-1, +1)
    // right      = (+1, -1)
    // up+left    = (-2,  0)
    // up+right   = ( 0, -2)
    // down+left  = ( 0, +2)
    // down+right = (+2,  0)
    const float cameraStepTiles = 1.0f;
    if (scancode == SDL_SCANCODE_UP ||
        scancode == SDL_SCANCODE_DOWN ||
        scancode == SDL_SCANCODE_LEFT ||
        scancode == SDL_SCANCODE_RIGHT)
    {
        const bool upPressed = (scancode == SDL_SCANCODE_UP) ||
                               rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_UP);
        const bool downPressed = (scancode == SDL_SCANCODE_DOWN) ||
                                 rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_DOWN);
        const bool leftPressed = (scancode == SDL_SCANCODE_LEFT) ||
                                 rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_LEFT);
        const bool rightPressed = (scancode == SDL_SCANCODE_RIGHT) ||
                                  rc2d_keyboard_isScancodeDown((RC2D_Scancode)SDL_SCANCODE_RIGHT);

        float deltaTileX = 0.0f;
        float deltaTileY = 0.0f;

        if (upPressed)
        {
            deltaTileX -= cameraStepTiles;
            deltaTileY -= cameraStepTiles;
        }
        if (downPressed)
        {
            deltaTileX += cameraStepTiles;
            deltaTileY += cameraStepTiles;
        }
        if (leftPressed)
        {
            deltaTileX -= cameraStepTiles;
            deltaTileY += cameraStepTiles;
        }
        if (rightPressed)
        {
            deltaTileX += cameraStepTiles;
            deltaTileY -= cameraStepTiles;
        }

        if (deltaTileX != 0.0f || deltaTileY != 0.0f)
        {
            camera.moveCameraTiles(deltaTileX, deltaTileY, map, gameScreen.rect);
            cameraChanged = true;
        }
    }
    else if (scancode == SDL_SCANCODE_KP_PLUS || scancode == SDL_SCANCODE_EQUALS)
    {
        camera.setZoomFactor(camera.getZoomFactor() + 0.05f);
        const SDL_FPoint shipTile = player.getTilePosition();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, gameScreen.rect);
        cameraChanged = true;
        RC2D_log(RC2D_LOG_DEBUG, "Camera zoom: %.2f", camera.getZoomFactor());
    }
    else if (scancode == SDL_SCANCODE_KP_MINUS || scancode == SDL_SCANCODE_MINUS)
    {
        camera.setZoomFactor(camera.getZoomFactor() - 0.05f);
        const SDL_FPoint shipTile = player.getTilePosition();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, gameScreen.rect);
        cameraChanged = true;
        RC2D_log(RC2D_LOG_DEBUG, "Camera zoom: %.2f", camera.getZoomFactor());
    }
    else if (scancode == SDL_SCANCODE_SPACE)
    {
        const SDL_FPoint shipTile = player.getTilePosition();
        camera.centerCameraOnTile(shipTile.x, shipTile.y, map, gameScreen.rect);
        cameraChanged = true;
    }

    if (cameraChanged)
    {
        camera.applyToMap(map, gameScreen.rect);
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;

    // Si ce n'est pas un clic gauche, on ignore l'evenement.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
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
