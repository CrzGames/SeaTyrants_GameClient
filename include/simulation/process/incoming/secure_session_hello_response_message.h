#pragma once

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite la reponse secure-session envoyee par le serveur.
 *
 * Le handler:
 * - verifie statut + nonce echo,
 * - verifie l'attestation signee (Ed25519 pinne cote client),
 * - derive les cles rx/tx client,
 * - active le chiffrement transport ENet.
 */
void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSecureSessionHelloResponseMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage);
