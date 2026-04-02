#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.

// ======================================================================================
// ServerUnreliablePacketType
//
// Type de packet envoyé sur le channel unreliable serveur -> client.
//
// Tous les packets unreliable doivent commencer par ServerUnreliablePacketHeader
// pour permettre au client de dispatcher correctement.
// ======================================================================================
enum class ServerUnreliablePacketType : uint8_t
{
    SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE = 0,
    SERVER_SNAPSHOT_DELTA_PACKET_UNRELIABLE = 1,
    SERVER_CLOCK_SYNC_PACKET_UNRELIABLE = 2,
};

struct ServerUnreliablePacketHeader
{
    // Type de packet envoyé sur le channel unreliable serveur -> client.
    ServerUnreliablePacketType type;
};

// ======================================================================================
// Liste des packets unreliable envoyés par le serveur au client.
// ======================================================================================

struct ServerSnapshotFullPacketUnreliable
{
    ServerUnreliablePacketHeader header;

    // Identifiant unique du snapshot envoyé au client.
    // Incrémenté côté serveur au moment de l'envoi réel.
    // Utilisé plus tard pour :
    // - ACK snapshots
    // - delta compression
    // - debug réseau.
    uint32_t snapshotId;

    // Tick logique de simulation serveur auquel ce snapshot a été construit.
    // Permet au client de :
    // - connaître la timeline autoritaire du serveur
    // - interpoler correctement les snapshots
    // - debug synchronisation serveur/client.
    uint64_t serverTick;

    // Temps monotone du serveur en nanosecondes depuis le démarrage du moteur.
    // Utilisé pour :
    // - estimer la latence réseau
    // - synchroniser l'horloge client avec le serveur
    // - debug réseau.
    uint64_t serverTimeNs;

    // ACK côté serveur des inputs envoyés par CE client.
    // Indique le dernier inputSequenceNumber que le serveur a réellement
    // appliqué dans la simulation.
    //
    // Utilisé côté client pour :
    // - supprimer les inputs déjà validés par le serveur
    // - rejouer les inputs restants (client-side prediction + reconciliation).
    uint32_t lastProcessedInputSequenceNumber;
};