#include "game/scenes/scene-game.h"

#include "game/game_screen.h"

namespace {

constexpr const char* kPlayerShipFolder = "assets/atlas/elite21";

} // namespace

GameScene::Shaders::Shaders(void)
    : oceanShader{},
      fogOfWarShader{},
      oceanColor(OceanShader::WaterColor::BLUE)
{
}

void GameScene::Shaders::load(void)
{
    if (!oceanShader.load(oceanColor))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene::Shaders: echec chargement ocean shader");
    }

    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene::Shaders: echec chargement fog-of-war shader");
    }
}

void GameScene::Shaders::unload(void)
{
    oceanShader.unload();
    fogOfWarShader.unload();
}

void GameScene::Shaders::update(double dt)
{
    oceanShader.update(dt);
    fogOfWarShader.update(dt, oceanShader.getColorMode());
}

void GameScene::Shaders::draw(const SDL_FRect& visibleRect)
{
    if (!oceanShader.isReady())
    {
        return;
    }

    oceanShader.draw(visibleRect);
    //fogOfWarShader.draw(visibleRect);
}

GameScene::GameScene(void)
    : shaders{},
      map{},
      player{},
      clickMarker{}
{
    configureGameplay();
}

void GameScene::configureGameplay(void)
{
    // Taille de tuile proche SeaFight.
    map.setTileSize(48.0f, 32.0f);

    // Taille de map de base (modifiable ensuite).
    map.setMapSize(64, 64);

    // Stats de base joueur.
    player.setHpMax(100);
    player.setHpCurrent(100);
    player.setMoveSpeedTilesPerSecond(3.0f);

    // Réglages de rendu navire à conserver.
    player.getShip().setDrawScale(1.0f, 1.0f);
    player.getShip().setDrawOffset(0.0f, 0.0f);
}

void GameScene::unload(void)
{
    shaders.unload();
    player.unloadShip();
    clickMarker.hide();
}

void GameScene::load(void)
{
    shaders.load();

    if (!player.loadShip(kPlayerShipFolder, RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", kPlayerShipFolder);
    }

    // Spawn joueur au centre de map.
    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;
    player.spawnOnTile(map, spawnTileX, spawnTileY);

    clickMarker.hide();
}

void GameScene::update(double dt)
{
    shaders.update(dt);

    // La map reste centrée sur le game screen global.
    map.centerOnRect(gameScreen.rect);

    player.update(dt, map);
    clickMarker.update(dt);
}

void GameScene::draw(void)
{
    shaders.draw(gameScreen.rect);
    clickMarker.draw(map);
    player.draw(map);
}

void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)scancode;
    (void)keycode;
    (void)mod;
    (void)isrepeat;
    (void)keyboardID;
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Conversion écran -> tile, puis déplacement joueur.
    const SDL_Point tile = map.screenToTileNearest(x, y);
    if (!map.isInside(tile.x, tile.y) || map.isTileBlocked(tile.x, tile.y))
    {
        return;
    }

    player.moveToTile(map, tile.x, tile.y);
    clickMarker.show(tile.x, tile.y);
}
