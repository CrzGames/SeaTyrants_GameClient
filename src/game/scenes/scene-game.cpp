#include "game/scenes/scene-game.h"

#include "game/game_screen.h"
#include "game/scenes/scene-manager.h"

GameScene::Shaders::Shaders(void)
    // Initialise le module shader ocean.
    : oceanShader{},
      // Initialise le module shader fog of war.
      fogOfWarShader{},
      // Initialise la couleur ocean par defaut.
      oceanColor(OceanShader::WaterColor::BLUE)
{
    // Le constructeur du sous-module ne fait pas d'allocation lourde.
}

void GameScene::Shaders::load(void)
{
    // Charge le shader ocean avec la couleur active.
    const bool oceanLoaded = oceanShader.load(oceanColor);

    // Journalise une erreur si le shader ocean ne se charge pas.
    if (!oceanLoaded)
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene::Shaders: impossible de charger le shader ocean");
    }

    // Charge le shader fog of war.
    const bool fogLoaded = fogOfWarShader.load();

    // Journalise un avertissement si le shader fog ne se charge pas.
    if (!fogLoaded)
    {
        RC2D_log(RC2D_LOG_WARN, "GameScene::Shaders: impossible de charger le shader fog of war");
    }
}

void GameScene::Shaders::unload(void)
{
    // Libere toutes les ressources GPU du shader ocean.
    oceanShader.unload();

    // Libere toutes les ressources GPU du shader fog of war.
    fogOfWarShader.unload();
}

void GameScene::Shaders::update(double dt)
{
    // Met a jour l'animation et les uniforms du shader ocean.
    oceanShader.update(dt);

    // Met a jour le shader fog avec le mode couleur courant de l'ocean.
    fogOfWarShader.update(dt, oceanShader.getColorMode());
}

void GameScene::Shaders::draw(const SDL_FRect& visibleRect)
{
    // Dessine le pass ocean.
    oceanShader.draw(visibleRect);

    // Dessine ensuite le pass fog au-dessus.
    //fogOfWarShader.draw(visibleRect);
}

bool GameScene::Shaders::isReady(void) const
{
    // Retourne vrai si le pass principal ocean est pret.
    return oceanShader.isReady();
}

GameScene::GameScene(void)
    // Initialise le sous-module qui regroupe les shaders de la scene.
    : shaders{},
      map{},
      playerShip{},
      playerShipOccupiedTile{0, 0},
      playerShipTileInitialized(false),
      playerShipObjectId(1)
{
    // Parametres de base type SeaFight (tuile 48x32).
    map.setTileSize(48.0f, 32.0f);

    // Taille de grille de dev (facile a ajuster ensuite).
    map.setMapSize(64, 64);

    // Couleurs debug lisibles sur l'ocean.
    map.setDebugFillColors(
        RC2D_Color{0, 194, 255, 130},
        RC2D_Color{0, 166, 236, 130});
    map.setDebugLineColor(RC2D_Color{0, 110, 175, 220});

    // Reglages de base du navire joueur.
    // Calibrage proche Sea Bandits/SeaFight (deplacement par steps diagonaux).
    playerShip.setSpeedTilesPerSecond(3.00f);
    playerShip.setCardinalSwapIntervalSeconds(0.20f);
    playerShip.setDrawScale(1.0f, 1.0f);
    playerShip.setDrawOffset(0.0f, 0.0f);
}

void GameScene::unload(void)
{
    // Delegate la liberation des shaders au sous-module.
    shaders.unload();

    // Libere l'atlas du navire.
    playerShip.unloadSprites();

    // Nettoie l'occupation logique des tiles.
    map.clearAllTileObjects();
    playerShipTileInitialized = false;
}

void GameScene::load(void)
{
    // Delegate le chargement des shaders au sous-module.
    shaders.load();

    // Charge l'atlas du navire joueur.
    if (!playerShip.loadSpritesFromFolder("assets/atlas/elite21", RC2D_STORAGE_TITLE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameScene: echec chargement sprites navire elite21");
    }

    // Spawn navire au centre logique de la map.
    const int spawnTileX = map.getWidthTiles() / 2;
    const int spawnTileY = map.getHeightTiles() / 2;

    playerShip.setPositionTileInt(spawnTileX, spawnTileY);
    playerShipOccupiedTile = SDL_Point{spawnTileX, spawnTileY};
    playerShipTileInitialized = true;

    map.clearAllTileObjects();
    map.setTileObject(spawnTileX, spawnTileY, playerShipObjectId);
}

void GameScene::update(double dt)
{
    // Delegate la mise a jour des shaders au sous-module.
    shaders.update(dt);

    // Recentre la map sur l'ecran de jeu logique.
    map.centerOnRect(gameScreen.rect);

    // Met a jour le deplacement du navire.
    playerShip.update(dt, map);

    // Synchronise l'occupation logique de tile du navire.
    SDL_FPoint shipTileFloat = playerShip.getPositionTile();
    SDL_Point shipTile = map.roundTile(shipTileFloat.x, shipTileFloat.y);
    shipTile = map.clampTile(shipTile.x, shipTile.y);

    if (!playerShipTileInitialized)
    {
        map.setTileObject(shipTile.x, shipTile.y, playerShipObjectId);
        playerShipOccupiedTile = shipTile;
        playerShipTileInitialized = true;
    }
    else if (playerShipOccupiedTile.x != shipTile.x || playerShipOccupiedTile.y != shipTile.y)
    {
        map.clearTileObject(playerShipOccupiedTile.x, playerShipOccupiedTile.y);
        map.setTileObject(shipTile.x, shipTile.y, playerShipObjectId);
        playerShipOccupiedTile = shipTile;
    }
}

void GameScene::draw(void)
{
    // Dessine les shaders uniquement s'ils sont prets.
    if (shaders.isReady())
    {
        // Delegate le dessin des shaders au sous-module.
        shaders.draw(gameScreen.rect);
    }

    // Dessine ensuite la grille isometrique de debug.
    //map.drawDebug();

    // Dessine le navire joueur centre sur sa tile.
    playerShip.draw(map);
}

void GameScene::keypressed(
    const char *key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Marque la variable comme utilisee intentionnellement.
    (void)key;

    // Marque la variable comme utilisee intentionnellement.
    (void)scancode;

    // Marque la variable comme utilisee intentionnellement.
    (void)keycode;

    // Marque la variable comme utilisee intentionnellement.
    (void)mod;

    // Marque la variable comme utilisee intentionnellement.
    (void)isrepeat;

    // Marque la variable comme utilisee intentionnellement.
    (void)keyboardID;
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Ne gere que le click gauche.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Convertit le click ecran en coordonnee tile.
    SDL_Point tile = map.screenToTileNearest(x, y);

    // Ignore si la tile est hors map.
    if (!map.isInside(tile.x, tile.y))
    {
        return;
    }

    // Defini la nouvelle destination du navire.
    playerShip.setTargetTile(map, tile.x, tile.y);

    // Marque la variable comme utilisee intentionnellement.
    (void)clicks;

    // Marque la variable comme utilisee intentionnellement.
    (void)mouseID;
}
