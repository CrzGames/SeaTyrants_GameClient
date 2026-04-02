#pragma once

#include "network/packets/server/reliable.h"

// Verifie l'attestation secure-session envoyee par le serveur:
// - reconstruit le message canonique signe
// - verifie la signature Ed25519 avec la cle publique pinnee cote client
// Retourne true si l'attestation est valide.
bool ClientSecureSession_VerifyServerHelloResponseAttestation(
    const ServerSecureSessionHelloResponsePacketReliable& packet);
