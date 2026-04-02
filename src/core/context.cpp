#include "core/context.h"

static GameState g_gameState;
static NetworkState g_networkState;
static NetworkINToSimulationQueue g_netToSimQueue;
static SimulationToNetworkOUTQueue g_simToNetQueue;
static SimulationToHttpQueue g_simToHttpQueue;
static HttpToSimulationQueue g_httpToSimQueue;
static SimulationToWebSocketQueue g_simToWsQueue;
static WebSocketToSimulationQueue g_wsToSimQueue;

GameState& GetGameState()
{
    return g_gameState;
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
