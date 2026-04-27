#include "core/context.h"

#include "game/ui/ingame-hud-overlay.h"

static GameState g_gameState;
static GameScreen g_gameScreen;
static Camera g_camera;
static NetworkState g_networkState;
static NetworkINToSimulationQueue g_netToSimQueue;
static SimulationToNetworkOUTQueue g_simToNetQueue;
static SimulationToHttpQueue g_simToHttpQueue;
static HttpToSimulationQueue g_httpToSimQueue;
static SimulationToWebSocketQueue g_simToWsQueue;
static WebSocketToSimulationQueue g_wsToSimQueue;
static Map g_currentMap;
static OceanShader g_oceanShader;
static FogOfWarShader g_fogOfWarShader;
static VisionCloudShader g_visionCloudShader;
static TitleAssetCache g_titleAssetCache;
static ClientLanguageState g_clientLanguageState;
static IngameHudOverlay g_ingameHudOverlay;

GameState& GetGameState()
{
    return g_gameState;
}

GameScreen& GetGameScreen()
{
    return g_gameScreen;
}

Camera& GetCamera()
{
    return g_camera;
}

NetworkState& GetNetworkState()
{
    return g_networkState;
}

NetworkINToSimulationQueue& GetNetworkINToSimulationQueue()
{
    return g_netToSimQueue;
}

SimulationToNetworkOUTQueue& GetSimulationToNetworkOUTQueue()
{
    return g_simToNetQueue;
}

SimulationToHttpQueue& GetSimulationToHttpQueue()
{
    return g_simToHttpQueue;
}

HttpToSimulationQueue& GetHttpToSimulationQueue()
{
    return g_httpToSimQueue;
}

WebSocketToSimulationQueue& GetWebSocketToSimulationQueue()
{
    return g_wsToSimQueue;
}

SimulationToWebSocketQueue& GetSimulationToWebSocketQueue()
{
    return g_simToWsQueue;
}

Map& GetCurrentMap()
{
    return g_currentMap;
}

OceanShader& GetOceanShader()
{
    return g_oceanShader;
}

FogOfWarShader& GetFogOfWarShader()
{
    return g_fogOfWarShader;
}

VisionCloudShader& GetVisionCloudShader()
{
    return g_visionCloudShader;
}

TitleAssetCache& GetTitleAssetCache()
{
    return g_titleAssetCache;
}

ClientLanguageState& GetClientLanguageState()
{
    return g_clientLanguageState;
}

IngameHudOverlay& GetIngameHudOverlay()
{
    return g_ingameHudOverlay;
}
