#pragma once

#include "game/state.h"
#include "network/state.h"
#include "core/threading/queues/http_to_simulation.h"
#include "core/threading/queues/websocket_to_simulation.h"
#include "core/threading/queues/network_incoming_to_simulation.h"
#include "core/threading/queues/simulation_to_http.h"
#include "core/threading/queues/simulation_to_websocket.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

// Accès global au game state
GameState& GetGameState();

// Accès global au network state
NetworkState& GetNetworkState();

// Accès global aux queues de communication du réseau IN vers le thread simulation
NetworkINToSimulationQueue& GetNetworkINToSimulationQueue();

// Accès global à la queue de communication de la simulation vers le thread réseau
SimulationToNetworkOUTQueue& GetSimulationToNetworkOUTQueue();

// Accès global à la queue de communication de la simulation vers le thread HTTP
SimulationToHttpQueue& GetSimulationToHttpQueue();

// Accès global à la queue de communication du thread HTTP vers le thread simulation
HttpToSimulationQueue& GetHttpToSimulationQueue();

// Accès global à la queue de communication du thread WebSocket vers le thread simulation
WebSocketToSimulationQueue& GetWebSocketToSimulationQueue();

// Accès global à la queue de communication de la simulation vers le thread WebSocket
SimulationToWebSocketQueue& GetSimulationToWebSocketQueue();
