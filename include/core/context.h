#pragma once

#include "core/threading/queues/http_to_simulation.h"
#include "core/threading/queues/network_incoming_to_simulation.h"
#include "core/threading/queues/simulation_to_http.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"
#include "core/threading/queues/simulation_to_websocket.h"
#include "core/threading/queues/websocket_to_simulation.h"
#include "game/camera.h"
#include "game/game_screen.h"
#include "game/map/map.h"
#include "game/shaders/fog-of-war-shader.h"
#include "game/shaders/ocean-shader.h"
#include "game/shaders/vision-cloud-shader.h"
#include "game/state.h"
#include "network/state.h"

/**
 * @brief Acces global au game state.
 * @return Reference mutable vers l'etat gameplay.
 */
GameState& GetGameState();

/**
 * @brief Acces global au game screen.
 * @return Reference mutable vers la zone de rendu gameplay.
 */
GameScreen& GetGameScreen();

/**
 * @brief Acces global a la camera gameplay.
 * @return Reference mutable vers la camera.
 */
Camera& GetCamera();

/**
 * @brief Acces global au network state.
 * @return Reference mutable vers l'etat reseau.
 */
NetworkState& GetNetworkState();

/**
 * @brief Acces global a la queue reseau IN -> simulation.
 * @return Reference mutable vers la queue.
 */
NetworkINToSimulationQueue& GetNetworkINToSimulationQueue();

/**
 * @brief Acces global a la queue simulation -> reseau OUT.
 * @return Reference mutable vers la queue.
 */
SimulationToNetworkOUTQueue& GetSimulationToNetworkOUTQueue();

/**
 * @brief Acces global a la queue simulation -> HTTP.
 * @return Reference mutable vers la queue.
 */
SimulationToHttpQueue& GetSimulationToHttpQueue();

/**
 * @brief Acces global a la queue HTTP -> simulation.
 * @return Reference mutable vers la queue.
 */
HttpToSimulationQueue& GetHttpToSimulationQueue();

/**
 * @brief Acces global a la queue WebSocket -> simulation.
 * @return Reference mutable vers la queue.
 */
WebSocketToSimulationQueue& GetWebSocketToSimulationQueue();

/**
 * @brief Acces global a la queue simulation -> WebSocket.
 * @return Reference mutable vers la queue.
 */
SimulationToWebSocketQueue& GetSimulationToWebSocketQueue();

/**
 * @brief Acces global a la map active.
 * @return Reference mutable vers la map.
 */
Map& GetCurrentMap();

/**
 * @brief Acces global au shader ocean.
 * @return Reference mutable vers l'instance globale.
 */
OceanShader& GetOceanShader();

/**
 * @brief Acces global au shader fog-of-war.
 * @return Reference mutable vers l'instance globale.
 */
FogOfWarShader& GetFogOfWarShader();

/**
 * @brief Acces global au shader de nuages visibles dans la portee.
 * @return Reference mutable vers l'instance globale.
 */
VisionCloudShader& GetVisionCloudShader();
