#include "core/context.h"

static GameState g_gameState;
static GameScreen g_gameScreen;
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

GameState& GetGameState()
{
    return g_gameState;
}

GameScreen& GetGameScreen()
{
    return g_gameScreen;
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
