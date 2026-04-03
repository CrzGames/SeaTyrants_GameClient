#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/game_screen.h"

static RC2D_UIImage minimapUI = {0};
static RC2D_UIImage buttonCenterMapUI = {0};

GameScene::Shaders::Shaders(void)
    : oceanShader{},
      fogOfWarShader{},
      oceanColor(OceanShader::WaterColor::GREEN)
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
    // fogOfWarShader.draw(visibleRect);
}

GameScene::GameScene(void)
    : shaders{},
      map{},
      player{},
      clickMarker{}
{
    
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
    GetOceanWakeSystem().reset();
    shaders.oceanShader.clearWakePoints();

    shaders.unload();

    player.unloadShip();

    clickMarker.hide();
}

void GameScene::load(void)
{
    // Configure les elements de gameplay avant le chargement des ressources.
    configureGameplay();

    // Charge les shaders avant tout autre element de gameplay.
    shaders.load();

    /* =========================
       MINIMAP - HAUT DROIT
       ========================= */
    minimapUI.image       = rc2d_graphics_loadImageFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    minimapUI.imageData   = rc2d_graphics_loadImageDataFromStorage("assets/images/minimap.png", RC2D_STORAGE_TITLE);
    minimapUI.anchor      = RC2D_UI_ANCHOR_TOP_RIGHT;
    minimapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    minimapUI.margin_x    = 0.01f;
    minimapUI.margin_y    = 0.01f;
    minimapUI.visible     = true;
    minimapUI.hittable    = true;

    /* =========================
       BOUTON CENTRER LA CARTE - BAS CENTRE
       ========================= */
    buttonCenterMapUI.image       = rc2d_graphics_loadImageFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    buttonCenterMapUI.imageData   = rc2d_graphics_loadImageDataFromStorage("assets/images/button-centermap-ingame.png", RC2D_STORAGE_TITLE);
    buttonCenterMapUI.anchor      = RC2D_UI_ANCHOR_BOTTOM_CENTER;
    buttonCenterMapUI.margin_mode = RC2D_UI_MARGIN_PERCENT;
    buttonCenterMapUI.margin_x    = 0.0f;
    buttonCenterMapUI.margin_y    = 0.25f;
    buttonCenterMapUI.visible     = true;
    buttonCenterMapUI.hittable    = true;

    // A remplacer plus tard par le navire reel du joueur.
    if (!player.loadShip("assets/atlas/redcosar_lvl1", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement navire '%s'", "assets/atlas/elite10");
    }

    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;

    // Spawn le joueur au centre de la map.
    player.spawnOnTile(map, spawnTileX, spawnTileY);

    // Cache le marqueur de clic au depart.
    clickMarker.hide();
}

void GameScene::update(double dt)
{
    // La map reste centree sur le game screen global.
    map.centerOnRect(gameScreen.rect);

    // Mise a jour des shaders pour les animations et effets dynamiques.
    shaders.update(dt);

    // Debut de frame du systeme de sillage (global a tous les navires).
    GetOceanWakeSystem().beginFrame(dt);

    // Mise a jour du joueur et de ses interactions avec la map.
    player.update(dt, map);

    // Mise a jour du marqueur de clic pour les animations de click sur tuile.
    clickMarker.update(dt);

    // Fin de frame du systeme de sillage: push vers shader ocean.
    GetOceanWakeSystem().endFrame(map, gameScreen.rect, shaders.oceanShader);
}

void GameScene::draw(void)
{
    // Shaders ocean et fog-of-war.
    shaders.draw(gameScreen.rect);

    // Affichage du marqueur lors du clic sur une tuile.
    clickMarker.draw(map);

    // Affichage du joueur (navire) par dessus les shaders et le marqueur de clic.
    player.draw(map);

    // UI par dessus tout le reste.
    rc2d_ui_drawImage(&minimapUI);
    rc2d_ui_drawImage(&buttonCenterMapUI);
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
        clickMarker.show(tile.x, tile.y);
    }
}
