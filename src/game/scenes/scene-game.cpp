#include "game/scenes/scene-game.h"

#include "game/game_screen.h"

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
    if (!fogOfWarShader.isReady())
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
    // Taille de tuile proche de SeaFight.
    map.setTileSize(48.0f, 32.0f);

    // Taille de map de base.
    map.setMapSize(256, 256);
}

void GameScene::unload(void)
{
    shaders.unload();
    player.unloadShip();
    clickMarker.hide();
}

void GameScene::load(void)
{
    // Charge les shaders avant tout autre élément de gameplay.
    shaders.load();

    /**
     * A SUPPRIMER, EN ATTENDANT.
    */
    if (!player.loadShip("assets/atlas/elite21", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", "assets/atlas/elite21");
    }
    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;

    // Spawn le joueur sur les cordonnées des tuiles centrales de la map.
    player.spawnOnTile(map, spawnTileX, spawnTileY);

    // Cache le marqueur de clic au départ.
    clickMarker.hide();
}

void GameScene::update(double dt)
{
    // Mise à jour des shaders pour les animations et effets dynamiques.
    shaders.update(dt);

    // La map reste centrée sur le game screen global.
    map.centerOnRect(gameScreen.rect);

    // Mise à jour du joueur et de ses interactions avec la map.
    player.update(dt, map);

    // Mise à jour du marqueur de clic pour les animations de click sur une Tile.
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

}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Si ce n'est pas un clic gauche, on ignore l'événement.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Convertit les coordonnées de clic en coordonnées de tuile.
    const SDL_Point tile = map.screenToTileNearest(x, y);

    // Vérifie que la tuile est à l'intérieur de la map et n'est pas bloquée par des obstacles (collisions).
    if (map.isInside(tile.x, tile.y) && !map.isTileBlocked(tile.x, tile.y))
    {
        // Déplace le joueur vers la tuile cliquée.
        player.moveToTile(map, tile.x, tile.y);

        // Affiche le marqueur de clic sur la tuile cliquée.
        clickMarker.show(tile.x, tile.y);
    }

    // Faire d'autre événements de click sur GUI, etc.
}
