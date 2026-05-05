#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/state.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"
#include "game/controllers/gameplay-shader-controller.h"
#include "game/ui/ingame-hud-overlay.h"
#include "game/ui/hud/game-settings-widget.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// Intervalle de temps entre deux salves demo envoyees a l'autre joueur.
constexpr float kDemoSalvoIntervalSec = 4.0f;
constexpr std::uint32_t kDemoIlluminatedSalvoEntryId = 1U;

// ---------------------------------------------------------------------------
// Constructeur
// ---------------------------------------------------------------------------

GameScene::GameScene(void)
    : shipAutoFollowEnabled(true),
      demoSalvoTimerSec(0.0f)
{
}

// ---------------------------------------------------------------------------
// Spawn joueur + premiere pose camera
// ---------------------------------------------------------------------------

void GameScene::initializePlayerSpawnAndCamera(void)
{
    // Grille monde.
    Map& map = GetCurrentMap();

    // Etat runtime (monstres, npcs, joueurs, VFX coque GameState, projectiles internes a la salve).
    GameState& gameState = GetGameState();

    // Camera + zoom pour le gameplay.
    Camera& camera = GetCamera();

    // Charge les sprites du navire du joueur depuis le dossier configure.
    if (!gameState.player.loadShip("assets/images/ships/bateau elite 10"))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement navire '%s'",
            "assets/images/ships/bateau elite 10");
    }

    // Place le joueur local sur une tuile de depart (secteur 30-AE).
    gameState.player.spawnOnSector(map, 30, 30);

    // Premiere frame: centre la vue sur le navire du joueur.
    GameplayCameraController::centerOnPlayer(camera, map, map.rect, gameState.player);

    // Reactive le suivi auto: le joueur n'a pas encore deplace la camera a la main.
    this->shipAutoFollowEnabled = true;

    // Met à jour immediatement la camera (position + zoom) sur la map.
    camera.update(map, map.rect);
}

// ---------------------------------------------------------------------------
// Decharge la scene: GPU, VFX gameplay (GameState), tracking salve, navires, HUD
// ---------------------------------------------------------------------------

void GameScene::unload(void)
{
    // Etat runtime (monstres, npcs, joueurs, VFX coque GameState, projectiles internes a la salve).
    GameState& gameState = GetGameState();

    // Libere les ressources GPU des shaders de la couche gameplay (eau, fog, etc.).
    GameplayShaderController::unloadAll();

    // Clear les vfx ships.
    gameState.clearGameplayVfx();

    // Clear le suivi de salve maritime et les boulets en vol.
    GetMaritimeCannonSalvoSystem().clear();

    // Unload le navire du joueur local.
    gameState.player.unload();

    // Unload les navires des joueurs distants, npcs et monstres.
    for (Player& otherPlayer : gameState.otherPlayers)
    {
        otherPlayer.unload();
    }
    for (Player& npc : gameState.npcs)
    {
        npc.unload();
    }
    for (Player& monster : gameState.monsters)
    {
        monster.unload();
    }

    // Vide les listes de joueurs distants, npcs et monstres.
    gameState.otherPlayers.clear();
    gameState.npcs.clear();
    gameState.monsters.clear();

    // Ferme / decharge les textures et etats des widgets HUD.
    GetIngameHudOverlay().unload();

}

// ---------------------------------------------------------------------------
// Charge la scene: joueur, shaders, HUD, navire, camera, monde secondaire
// ---------------------------------------------------------------------------

void GameScene::load(void)
{
    // Map affichee dans le game screen.
    Map& map = GetCurrentMap(); 

    // Etat runtime (monstres, npcs, joueurs, VFX coque GameState, projectiles internes a la salve).
    GameState& gameState = GetGameState();

    // Initialise l'etat logique du Player.
    gameState.player.load();

    // Charge et prepare les shaders de la couche gameplay (eau, fog, etc.).
    GameplayShaderController::loadAll();

    // Construit l'arborescence HUD (widgets, themes, liens entree clavier).
    GetIngameHudOverlay().load();

    MaritimeCannonSalvoSystem& salvoSystem = GetMaritimeCannonSalvoSystem();
    (void)MaritimeCannonSalvoSystem::loadProjectileTrajectoryTuningsFromFile();
    salvoSystem.clearSalvoEntries();
    MaritimeCannonSalvoSystem::SalvoEntry demoSalvoEntry{};
    demoSalvoEntry.entryId = kDemoIlluminatedSalvoEntryId;
    demoSalvoEntry.debugName = "demo_illuminated_round";
    demoSalvoEntry.projectileVfxClassicFolder = "assets/images/vfxclassic/vfx-ammo-explo";
    demoSalvoEntry.illuminatedProjectile.enabled = true;
    demoSalvoEntry.startActionVfxShipFolderForAttacker = "assets/images/vfxship/vfx-cannon";
    MaritimeCannonSalvoSystem::SalvoEntry::EndActionVfxShipFolderForTarget demoImpactVfx{};
    demoImpactVfx.vfxShipFolder = "assets/images/vfxship/vfx-hitsimple";
    demoSalvoEntry.endActionVfxShipFoldersForTarget.push_back(demoImpactVfx);
    salvoSystem.addSalvoEntry(demoSalvoEntry);

    // Recalcule map.rect a partir de la taille de la zone de rendu gameplay.
    map.update();

    // 1er joueur de test: spawn dans le secteur central, avec le navire configure pour le joueur.
    this->initializePlayerSpawnAndCamera();

    // 2eme joueur de test: spawn dans un secteur voisin, avec un navire different.
    gameState.otherPlayers.clear();
    gameState.otherPlayers.emplace_back();
    Player& enemy = gameState.otherPlayers.back();
    enemy.load();
    enemy.loadShip("assets/images/ships/bateau elite 10");
    enemy.spawnOnSector(map, 33, 30);
}

// ---------------------------------------------------------------------------
// Mise a jour logique: map, ocean, navires, salve, visibilite, HUD, camera
// ---------------------------------------------------------------------------

void GameScene::update(double dt)
{
    // Contextes principaux pour la logique gameplay: map, joueurs, shaders, camera, HUD.
    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    OceanShader& oceanShader = GetOceanShader();
    Camera& camera = GetCamera();
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Synchronise le rectangle map avec le game screen (redimensionnement fenetre / UI).
    map.update();

    // Met a jour l'ocean: temps, uniforms, et prise en compte de l'option sillage (wake) via le 2e parametre.
    oceanShader.update(dt, gameSettings.getShipWakeTrailsEnabled());

    // Collecte des points de sillage sur cette frame: debute l'enregistrement.
    oceanShader.beginWakeFrame(dt);
    // Deplace / anime le joueur; le navire emet des points de wake consommes par l'ocean.
    gameState.player.update(dt, map);
    // Idem pour chaque autre Player (NPC / joueurs distants cote client).
    for (Player& otherPlayer : gameState.otherPlayers)
    {
        otherPlayer.update(dt, map);
    }
    // Finalise la passe wake: envoie les points au shader ocean pour ce frame.
    oceanShader.endWakeFrame(map, map.rect);

    // Demo: salve maritime du joueur local vers le premier autre joueur toutes les N secondes.
    if (!gameState.otherPlayers.empty())
    {
        const MaritimeCannonSalvoSystem::SalvoEntry* demoSalvoEntry =
            GetMaritimeCannonSalvoSystem().findSalvoEntryById(kDemoIlluminatedSalvoEntryId);
        this->demoSalvoTimerSec += static_cast<float>(dt);
        while (demoSalvoEntry != nullptr && this->demoSalvoTimerSec >= kDemoSalvoIntervalSec)
        {
            this->demoSalvoTimerSec -= kDemoSalvoIntervalSec;
            GetMaritimeCannonSalvoSystem().fireSalvo(
                gameState.player.getShip(),
                gameState.otherPlayers[0].getShip(),
                *demoSalvoEntry);
        }
    }

    // Deplace les projectiles de salve, met a jour VFX muzzle, nettoie les entrees finies.
    GetMaritimeCannonSalvoSystem().update(dt);

    // Anime nuages / masques de visibilite; le fog est pilote par l'option (mais l'update reste ici).
    GameplayShaderController::updateVisibility(dt, gameState.player, gameSettings.getFogOfWarEnabled());

    // Met a jour tout le HUD (minimap, chat, reglages, etc.) selon la camera et la map.
    GetIngameHudOverlay().update(dt, camera, map);

    // Tant qu'aucun champ texte ne monopolise le clavier, le defilement camera via le clavier est actif.
    if (!GetIngameHudOverlay().isBlockingGameplayKeyboardInput())
    {
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
            // L'utilisateur deplace la carte a la main: on coupe le suivi du navire.
            this->shipAutoFollowEnabled = false;
        }
    }

    // Si le suivi auto est encore actif, recolle la camera au joueur chaque frame.
    if (this->shipAutoFollowEnabled)
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, gameState.player);
    }

    // Applique zoom et translation finaux de la camera pour les draws suivants.
    camera.update(map, map.rect);
}

// ---------------------------------------------------------------------------
// Rendu carte: ocean, fog, marqueurs, VFX salve (sous / au-dessus), joueurs, UI carte
// ---------------------------------------------------------------------------

void GameScene::draw(void)
{
    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Panneaux / fonds UI.
    GetIngameHudOverlay().drawBackgroundWidget();

    // Restreint le SDL renderer a la zone map (le reste = UI full window).
    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    // Shader d'ocean.
    oceanShader.draw(map.rect);

    // Brouillard de guerre.
    fogOfWarShader.draw(map.rect, gameSettings.getFogOfWarEnabled());

    // Clic sur tuile: affiche un marqueur de selection de tuile.
    GetIngameHudOverlay().drawTileClickMarkerOverlay(map);

    // Re-synchronise les VFX navire avec leur contexte runtime courant avant le rendu
    // pour que TARGET_RELATIVE_AB dispose toujours du navire de reference.
    for (GameplayVfxShipSlot& slot : gameState.vfxShips)
    {
        Ship* anchorShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.attackerShip
                : slot.targetShip;
        if (anchorShip == nullptr)
        {
            continue;
        }

        const SDL_FPoint* targetTile = nullptr;
        SDL_FPoint relativeTargetTile{};
        Ship* relativeShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.targetShip
                : slot.attackerShip;
        if (relativeShip != nullptr)
        {
            relativeTargetTile = relativeShip->getPositionTile();
            targetTile = &relativeTargetTile;
        }

        slot.vfx.update(0.0, *anchorShip, targetTile);
    }

    // VFX navire (ex. flash canon): derriere les coques, depuis GameState (alimente par la salve au tir).
    for (const GameplayVfxShipSlot& slot : gameState.vfxShips)
    {
        Ship* anchorShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.attackerShip
                : slot.targetShip;
        if (anchorShip != nullptr)
        {
            slot.vfx.draw(map, *anchorShip, true);
        }
    }

    for (const Player& otherPlayer : gameState.otherPlayers)
    {
        otherPlayer.draw(map);
    }
    gameState.player.draw(map);

    for (const GameplayVfxShipSlot& slot : gameState.vfxShips)
    {
        Ship* anchorShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.attackerShip
                : slot.targetShip;
        if (anchorShip != nullptr)
        {
            slot.vfx.draw(map, *anchorShip, false);
        }
    }

    // Boulets (spritesheets internes a MaritimeCannonSalvoSystem), au-dessus des navires.
    GetMaritimeCannonSalvoSystem().drawSalvoProjectiles();

    // Barres de defilement des panneaux qui se superposent a la zone map.
    GetIngameHudOverlay().drawScrollBarOverlay(map);

    // Leve le clip: le HUD global (texte, fenetres) dessine en coordonnees ecran libres.
    WorldRenderClip::end(renderer);

    // Widgets principaux (minimap, tchat, etc.); le Player sert pour le contexte d'affichage.
    GetIngameHudOverlay().drawWidgets(map, gameState.player);
}

// ---------------------------------------------------------------------------
// Clavier: priorite UI, raccourcis camera / minimap
// ---------------------------------------------------------------------------

void GameScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;        // Conserve la signature RC2D; l'UI peut consommer le caractere.
    (void)keycode;    // Idem.
    (void)mod;        // Modificateurs (Ctrl, etc.) laisses au widget actif.
    (void)keyboardID; // Multi-claviers: non utilise ici.
    // isrepeat: utilise plus bas pour eviter de toggler la minimap en maintien de touche.

    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    Camera& camera = GetCamera();
    bool cameraChanged = false; // Sert a n'appeler camera.update qu'en cas de changement.

    // Le chat / champs texte consomment d'abord la touche.
    if (GetIngameHudOverlay().keypressed(key, scancode, keycode, mod, isrepeat))
    {
        GetIngameHudOverlay().syncPlatformTextInputState();
        return;
    }

    // Aligne l'etat plateforme (IME) avec le HUD.
    GetIngameHudOverlay().syncPlatformTextInputState();

    // Tant qu'un widget texte bloque le gameplay, on n'applique pas les raccourcis monde.
    if (GetIngameHudOverlay().isBlockingGameplayKeyboardInput())
    {
        return;
    }

    // Raccourci: recentrer la camera sur le navire et reactiver le suivi auto.
    if (scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, gameState.player);
        this->shipAutoFollowEnabled = true;
        cameraChanged = true;
    }

    // Raccourci: afficher / masquer la minimap (pas sur repetition de touche).
    if (!isrepeat &&
        scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::TOGGLE_MINIMAP))
    {
        GetIngameHudOverlay().toggleHudWidgetVisibility(GameSettingsWidget::HudScaleTarget::MINIMAP);
    }

    // Si la camera a bouge, applique immediatement pour ce frame.
    if (cameraChanged)
    {
        camera.update(map, map.rect);
    }
}

// ---------------------------------------------------------------------------
// Texte compose (IME / chat): delegue au HUD
// ---------------------------------------------------------------------------

void GameScene::textinput(const RC2D_TextInputEventInfo* info)
{
    if (info == nullptr)
    {
        return;
    }

    (void)GetIngameHudOverlay().textinput(info);
}

// ---------------------------------------------------------------------------
// Souris: HUD d'abord, puis bouton centre-navire, scroll map, ordres de mouvement
// ---------------------------------------------------------------------------

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;   // Nombre de clics: le HUD peut en tenir compte en interne.
    (void)mouseID; // Multi-souris: non utilise pour la logique gameplay ici.

    Map& map = GetCurrentMap();
    GameState& gameState = GetGameState();
    Camera& camera = GetCamera();
    IngameHudOverlay& hudOverlay = GetIngameHudOverlay();

    // Si un widget consomme le clic (bouton, chat, etc.), ne pas envoyer d'ordre au monde.
    const bool hudConsumed = hudOverlay.mousepressed(x, y, button, clicks, mouseID);
    hudOverlay.syncPlatformTextInputState();
    if (hudConsumed)
    {
        return;
    }

    // Bouton UI "centrer sur le navire": meme effet que le raccourci clavier.
    if (hudOverlay.centerShipButtonMousepressed(x, y, button))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, gameState.player);
        this->shipAutoFollowEnabled = true;
        camera.update(map, map.rect);
        return;
    }

    // Clic sur une scrollbar de panneau au-dessus de la map: desactive le suivi camera.
    if (hudOverlay.handleMapOverlayMousePressed(x, y, button, camera, map))
    {
        this->shipAutoFollowEnabled = false;
        return;
    }

    // Seuls le clic gauche et droit declenchent des ordres sur la carte.
    if (button != RC2D_MOUSE_BUTTON_LEFT && button != RC2D_MOUSE_BUTTON_RIGHT)
    {
        return;
    }

    // Hors du rectangle map: bandeaux UI (pas de conversion tuile).
    if (x < map.rect.x || x > (map.rect.x + map.rect.w) ||
        y < map.rect.y || y > (map.rect.y + map.rect.h))
    {
        return;
    }

    // Conversion pixel -> tuile la plus proche du clic.
    const SDL_Point tile = map.screenToTileNearest(x, y);
    // Tuile invalide ou obstacle: pas d'ordre de deplacement.
    if (!map.isInside(tile.x, tile.y) || map.isTileBlocked(tile.x, tile.y))
    {
        return;
    }

    // Clic gauche: deplace le joueur; notifie le HUD pour certains widgets (ex: clic tuile).
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        gameState.player.moveToTile(map, tile.x, tile.y);
        GetIngameHudOverlay().notifyMapTileClicked(tile.x, tile.y);
    }
    // Clic droit: deplace le premier autre Player si present (demo / second slot).
    else if (!gameState.otherPlayers.empty())
    {
        gameState.otherPlayers[0].moveToTile(map, tile.x, tile.y);
    }
}

// ---------------------------------------------------------------------------
// Molette: delegue au HUD (chat scroll, listes); sinon aucun effet en scene.
// ---------------------------------------------------------------------------

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
    if (GetIngameHudOverlay().mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
    {
        return;
    }
}
