#include "simulation/process/incoming/snapshot_full_message.h"

#include <RC2D/RC2D.h>

#include <mutex>

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSnapshotFullMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage)
{
    // Raccourci vers le packet de snapshot full recu du serveur.
    const ServerSnapshotFullPacketUnreliable& packet = networkInToSimMessage.snapshotFullPacket;

    // Voir ensuite ce qu'ont fait..

    RC2D_log(
        RC2D_LOG_DEBUG,
        "[CLIENT] [SIMULATION] [SNAPSHOT] id=%u tick=%llu serverTimeNs=%llu lastProcessedInput=%u",
        packet.snapshotId,
        static_cast<unsigned long long>(packet.serverTick),
        static_cast<unsigned long long>(packet.serverTimeNs),
        packet.lastProcessedInputSequenceNumber);
}