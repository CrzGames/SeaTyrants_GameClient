#include "game/scenes/scene-game.h"

#include "core/context.h"

GameScene::GameScene(void)
    : clickMarker{}
{

}

void GameScene::unload(void)
{
    // Récupère les références aux systèmes et objets nécessaires.
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    Player& player = GetGameState().player;

    // Libère les ressources du jeu.
    oceanShader.unload();
    fogOfWarShader.unload();
    player.unload();
    this->clickMarker.hide();
}

void GameScene::load(void)
{
    // Récupère les références aux systèmes et objets nécessaires.
    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    Player& player = gameState.player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    // Configurer le joueur.
    player.load();

    // Configurer la map de gameplay.
    map.setTileSize(48.0f, 32.0f);
    map.setMapSize(256, 256);

    // Charger les shaders.
    if (!oceanShader.load(OceanShader::WaterColor::TURQUOISE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement ocean shader");
    }
    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene: echec chargement fog-of-war shader");
    }

    // Configurer l'UI de minimap.
    this->minimapUI.image       = rc2d_graphics_loadImageFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.imageData   = rc2d_graphics_loadImageDataFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    this->minimapUI.anchor      = RC2D_UI_ANCHOR_TOP_RIGHT;
    this->minimapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->minimapUI.margin_x    = 0.01f;
    this->minimapUI.margin_y    = 0.01f;
    this->minimapUI.visible     = true;
    this->minimapUI.hittable    = true;

    // Configurer l'UI du bouton centrer la map.
    this->buttonCenterMapUI.image       = rc2d_graphics_loadImageFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUI.imageData   = rc2d_graphics_loadImageDataFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    this->buttonCenterMapUI.anchor      = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    this->buttonCenterMapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    this->buttonCenterMapUI.margin_x    = 0.0f;
    this->buttonCenterMapUI.margin_y    = 0.25f;
    this->buttonCenterMapUI.visible     = true;
    this->buttonCenterMapUI.hittable    = true;

    // Configurer le joueur sur la map.
    if (!player.loadShip("assets/atlas/elite20", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", "assets/atlas/elite20");
    }
    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;
    player.spawnOnTile(map, spawnTileX, spawnTileY);
}

void GameScene::update(double dt)
{
    // Récupère les références aux systèmes et objets nécessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();

    // Centrer la map dans le game screen.
    map.centerOnRect(gameScreen.rect);

    // Met à jour les shaders d'océan et de fog.
    oceanShader.update(dt);
    fogOfWarShader.update(dt, oceanShader.getColorMode());

    // Met à jour les éléments de la scène.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    this->clickMarker.update(dt);
    oceanShader.endWakeFrame(map, gameScreen.rect);
}

void GameScene::draw(void)
{
    // Récupère les références aux systèmes et objets nécessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameScreen& gameScreen = GetGameScreen();

    // Dessine l'océan et le fog-of-war.
    if (oceanShader.isReady() && fogOfWarShader.isReady())
    {
        oceanShader.draw(gameScreen.rect);
        // fogOfWarShader.draw(gameScreen.rect);
    }

    // Dessine les éléments de la scène.
    this->clickMarker.draw(map);
    player.draw(map);

    // Dessine les éléments d'interface.
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

}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Récupère les références aux systèmes et objets nécessaires.
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
