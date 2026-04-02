#include "network/transport/outgoing/message_preparation.h"

void ClientNetworkOutgoing_SplitReliableAndCoalesceUnreliableMessages(
    const std::deque<SimulationToNetworkOUTMessage>& simToNetOutMessages,
    ClientNetworkOutgoingPreparedMessages& preparedMessages)
{
    // Parcourir tous les messages sortants produits par la simulation.
    for (std::deque<SimulationToNetworkOUTMessage>::const_iterator it = simToNetOutMessages.begin();
         it != simToNetOutMessages.end();
         ++it)
    {
        // Référence directe vers le message courant.
        const SimulationToNetworkOUTMessage& msg = *it;

        // Si le message est reliable, on le conserve tel quel dans l'ordre.
        if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE ||
            msg.type == SimulationToNetworkOUTMessageType::CLIENT_AUTH_PACKET_RELIABLE ||
            msg.type == SimulationToNetworkOUTMessageType::CLIENT_READY_FOR_MATCH_PACKET_RELIABLE)
        {
            // Ajouter le message à la liste des reliable à envoyer.
            preparedMessages.reliableMessages.push_back(msg);
        }
        else if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_INPUT_PACKET_UNRELIABLE)
        {
            // Pour les unreliable, ne garder que le dernier message pendant ce tick réseau sortant.
            preparedMessages.hasInputUnreliable = true;
            preparedMessages.lastInputUnreliable = msg;
        }
        else if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_CLOCK_SYNC_PACKET_UNRELIABLE)
        {
            // Pour les unreliable, ne garder que le dernier message pendant ce tick réseau sortant.
            preparedMessages.hasClockSyncUnreliable = true;
            preparedMessages.lastClockSyncUnreliable = msg;
        }
    }
}
