#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.

// ======================================================================================
// ClientUnreliablePacketType
//
// Type de packet envoyé sur le channel unreliable client -> serveur.
//
// Tous les packets unreliable doivent commencer par ClientUnreliablePacketHeader
// pour permettre au serveur de dispatcher correctement.
// ======================================================================================
enum class ClientUnreliablePacketType : uint8_t
{
    CLIENT_INPUT_PACKET_UNRELIABLE = 0,
    CLIENT_CLOCK_SYNC_PACKET_UNRELIABLE = 1,
};

struct ClientUnreliablePacketHeader
{
    // Type de packet envoyé sur le channel unreliable client -> serveur.
    ClientUnreliablePacketType type;
};

// ======================================================================================
// Liste des packets unreliable envoyés par le client au serveur.
// ======================================================================================

enum class MovementFlags : uint8_t
{
    None  = 0,
    Up    = 1u << 0,
    Down  = 1u << 1,
    Left  = 1u << 2,
    Right = 1u << 3,
};
enum class ActionFlags : uint8_t
{
    None   = 0,
    Jump   = 1u << 0,
    Dash   = 1u << 1,
    Sprint = 1u << 2,
    Attack = 1u << 3,
};

struct ClientInputPacketUnreliable
{
    // Header commun à tous les packets unreliable client -> serveur
    ClientUnreliablePacketHeader header;

    // =========================================================================
    // Ordonnancement / Synchronisation
    // =========================================================================

    // Numéro de séquence strictement monotone côté client.
    // Permet au serveur :
    //  - de traiter les inputs dans l'ordre correct
    //  - d’ignorer les doublons (retransmissions)
    //  - d’envoyer un ACK pour la reconciliation côté client
    uint32_t inputSequenceNumber = 0;

    // Identifiant du dernier snapshot reçu par le client (ACK implicite).
    // Utile pour le serveur afin de savoir quel snapshot a été reçu par le client
    // et ainsi :
    //  - estimer la latence réseau
    //  - implémenter une compression différentielle (delta compression)
    //  - debug réseau.
    uint32_t lastReceivedSnapshotId = 0;
    

    // =========================================================================
    // MOUVEMENT
    // =========================================================================
    // Etat des directions du joueur (bitmask MovementFlags).
    // Représente uniquement des intentions gameplay, indépendantes du clavier.

    // Directions actuellement maintenues.
    // C’est la source principale utilisée par le serveur pour
    // calculer la vitesse et la direction à chaque tick.
    uint8_t movementHeldFlags = 0;

    // Directions qui viennent d’être activées sur CE tick (transition 0 -> 1).
    // Utile pour détecter un "tap" directionnel ou déclencher
    // une action dépendante d’une direction précise (ex: dash).
    // Optionnel si ton gameplay ne nécessite pas de détection de transition.
    uint8_t movementPressedFlags = 0;

    // Directions qui viennent d’être désactivées sur CE tick (transition 1 -> 0).
    // Rarement nécessaire pour un mouvement classique,
    // mais utile si certaines mécaniques dépendent du relâchement.
    uint8_t movementReleasedFlags = 0;


    // =========================================================================
    // ACTIONS (Jump, Dash, Sprint, Shoot, etc.)
    // =========================================================================
    // Intentions gameplay abstraites (indépendantes du périphérique d’entrée).

    // Actions actuellement maintenues.
    // Utilisé pour les actions continues : sprint, tir automatique, etc.
    uint8_t actionHeldFlags = 0;

    // Actions déclenchées exactement sur ce tick (transition 0 -> 1).
    // Indispensable pour les actions instantanées : jump, dash,
    // tir semi-automatique, interaction.
    // Garantit qu’une action ne soit déclenchée qu’une seule fois
    // même en cas de maintien du bouton ou de reconciliation réseau.
    uint8_t actionPressedFlags = 0;

    // Actions relâchées sur ce tick (transition 1 -> 0).
    // Utile pour les mécaniques dépendant du relâchement :
    // attaque chargée, arrêt d’un sprint, fin d’une visée, etc.
    // Peut être omis si ton gameplay ne l’utilise pas.
    uint8_t actionReleasedFlags = 0;
};

struct ClientClockSyncPacketUnreliable
{
    ClientUnreliablePacketHeader header;

    // Temps monotone local du client (ns) au moment de l'envoi.
    uint64_t clientTimeNs = 0;

    // Dernier snapshot serveur reçu au moment de l'envoi.
    uint32_t lastReceivedSnapshotId = 0;
};
