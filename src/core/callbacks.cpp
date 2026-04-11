#include "core/callbacks.h"

#include "core/context.h"
#include "crypto/kx.h"
#include "game/scenes/scene-game.h"
#include "game/scenes/scene-manager.h"
#include "game/scenes/scene-menu.h"
#include "game/scenes/scene-splashscreen.h"
#include "network/protocol/secure_session.h"
#include "network/transport/compression/enet_host_lz4_compressor.h"
#include "network/transport/encryption/enet_host_xchacha20poly1305_encryptor.h"
#include "network/transport/incoming/entrypoint.h"
#include "network/transport/outgoing/entrypoint.h"
#include "services/http/entrypoint.h"
#include "services/websocket/entrypoint.h"
#include "simulation/entrypoint.h"

#if GAME_ENV_DEV
#include "game/scenes/scene-editormap-anchorship.h"
#include "game/scenes/scene-editormap-crashtest.h"
#include "game/scenes/scene-editormap-creatormap.h"
#include "game/scenes/scene-editormap-vfx.h"
#endif

SceneManager sceneManager;

void rc2d_unload(void)
{
    sceneManager.unload();
}

void rc2d_load(void)
{
    // Récupère l'état réseau global du client.
    NetworkState& networkState = GetNetworkState();

    // Initialise les clés de cryptographie KX côté client.
    // Si l'initialisation échoue, on demande l'arrêt du moteur.
    if (!ClientCryptoKx_Initialize(networkState.cryptoKxState))
    {
        // Demande d'arrêt propre du moteur.
        rc2d_event_quit();

        // Log explicite de l'erreur pour diagnostic.
        RC2D_log(RC2D_LOG_ERROR, "Failed to initialize client crypto KX state");

        // Sort de la fonction de callback pour éviter de continuer l'initialisation du client dans un état potentiellement instable.
        return;
    }

    // Convertir la clé publique Ed25519 épinglée du format hexadécimal une seule fois au démarrage.
    // Si le format hexadécimal est invalide, arrêter immédiatement le client.
    if (!ClientSecureSession_InitializePinnedServerSigningPublicKey())
    {
        rc2d_event_quit();
        RC2D_log(RC2D_LOG_ERROR, "Failed to initialize pinned server signing public key");
        return;
    }

    // Crée les scènes du jeu et affiche la scène de splashscreen.
    //sceneManager.addScene("menu", new MenuScene());
#if GAME_ENV_DEV
    sceneManager.addScene("editormap-creatormap", new EditorMapCreateMapScene());
    sceneManager.addScene("editormap-anchorship", new EditorMapAnchorShipScene());
    sceneManager.addScene("editormap-vfx", new EditorMapVfxScene());
    sceneManager.addScene("editormap-crashtest", new EditorMapCrashTestScene());
#endif
    //sceneManager.addScene("splashscreen", new SplashScreenScene());
    sceneManager.addScene("game", new GameScene());
    sceneManager.changeScene("game");

    // Mettre en plein écran.
    //rc2d_window_setFullscreen(true, RC2D_FULLSCREEN_EXCLUSIVE, true);

    // Cache le curseur de la souris, pour une meilleure immersion.
    // Le client vas le réafficher dans la scène de menu pour permettre l'interaction avec l'UI.
    //rc2d_mouse_setVisible(false);

    // À ce stade, l'initialisation applicative est terminée.
    // Le client peut commencer à accepter et traiter son activité normale.
    RC2D_log(RC2D_LOG_INFO, "Client is ready");

    // --------------------------------------------------------------------
    // Bootstrap temporaire (DEV):
    // - en attendant l'UI de login, on envoie un signup puis un signin
    //   hardcodes au thread HTTP.
    // - le signin reussi declenchera ensuite la connexion reseau jeu.
    // --------------------------------------------------------------------
    /*SimulationToHttpQueue& simToHttpQueue = GetSimulationToHttpQueue();

    SimulationToHttpMessage signUpMessage{};
    signUpMessage.type = SimulationToHttpMessageType::AUTH_SIGNUP_REQUEST;
    signUpMessage.authSignUpRequest.username = "coco";
    signUpMessage.authSignUpRequest.email = "coco@orangexxx.fr";
    signUpMessage.authSignUpRequest.password = "toto35000!xx";
    simToHttpQueue.push(signUpMessage);

    SimulationToHttpMessage signInMessage{};
    signInMessage.type = SimulationToHttpMessageType::AUTH_SIGNIN_REQUEST;
    signInMessage.authSignInRequest.email = "coco@orangexxx.fr";
    signInMessage.authSignInRequest.password = "toto35000!xx";
    simToHttpQueue.push(signInMessage);*/
}

void rc2d_update(double dt)
{
    GetGameScreen().update(dt);
    sceneManager.update(dt);
}

void rc2d_network_host_setup(ENetHost* host)
{
    if (host == nullptr)
    {
        return;
    }

    // Installe l'encryptor au niveau host une seule fois juste après enet_host_create.
    ClientNetworkEncryption_EnsureHostEncryptorInstalled(host);

    // Installe le compresseur au niveau host une seule fois juste après enet_host_create.
    ClientNetworkCompression_EnsureHostCompressorInstalled(host);
}

void rc2d_network_incoming_update(ENetHost* host, const ENetEvent* event)
{
    // Délègue tout le traitement des événements ENet entrants à la couche réseau applicative.
    ClientNetworkIncoming_ProcessENetEvent(host, event);
}

void rc2d_network_outgoing_update(ENetHost* host)
{
    // Demande à la couche réseau sortante de :
    // - drainer les messages produits par simulation
    // - sérialiser / préparer si nécessaire
    // - envoyer les packets via ENet
    ClientNetworkOutgoing_DrainSimulationMessages_And_RunOutgoingNetworkLogic(host);
}

void rc2d_simulation_update(uint64_t currentTick, uint64_t dtNs, double dt)
{
    // À chaque tick simulation :
    // - on draine les messages entrants (Network IN / HTTP / WebSocket)
    // - puis on exécute la logique de simulation pour le tick courant
    ClientSimulation_DrainNetworkIncomingAndHttpAndWebSocketMessages_And_RunSimulationLogic(
        currentTick,
        dtNs,
        dt
    );
}

void rc2d_http_update(void)
{
    // Exécute une unité de travail du thread HTTP.
    // Cette fonction peut bloquer en attendant un job depuis la queue Simulation -> HTTP.
    ClientHttp_WaitAndProcessOneSimulationMessage_And_RunHttpLogic();
}

void rc2d_websocket_update(void)
{
    // Exécute une unité de travail du thread WebSocket.
    // Cette fonction peut bloquer en attendant un job depuis la queue Simulation -> WebSocket.
    ClientWebSocket_WaitAndProcessOneSimulationMessage_And_RunWebSocketLogic();
}

void rc2d_wake_blocking_threads(void)
{
    // Reveille les waits bloquants des workers HTTP/WebSocket pour permettre
    // a rc2d_engine_stop_worker_threads() de joindre proprement les threads.
    GetSimulationToHttpQueue().stop();
    GetSimulationToWebSocketQueue().stop();
}

void rc2d_draw(void)
{
    sceneManager.draw();
}

void rc2d_keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID)
{
#if GAME_ENV_DEV
    // Raccourcis globaux DEV pour naviguer entre gameplay et scenes editeur.
    if (!isrepeat && scancode == SDL_SCANCODE_F9)
    {
        sceneManager.changeScene("editormap-creatormap");
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_F10)
    {
        sceneManager.changeScene("editormap-anchorship");
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_F11)
    {
        sceneManager.changeScene("editormap-vfx");
        return;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_F12)
    {
        sceneManager.changeScene("editormap-crashtest");
        return;
    }
#endif

    sceneManager.keypressed(key, scancode, keycode, mod, isrepeat, keyboardID);
}

void rc2d_mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    sceneManager.mousepressed(x, y, button, clicks, mouseID);
}

void rc2d_mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    sceneManager.mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID);
}
