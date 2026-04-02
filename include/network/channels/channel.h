#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.

// channel 0 = échange de clés (packet non chiffré)
// channel 1 = vérification du token (packet chiffré)
// channel 2 = données de jeu fiables (packet chiffré)
// channel 3 = données de jeu non fiables (packet chiffré)
enum class NetworkChannel : uint8_t
{
    SECURE_SESSION_RELIABLE = 0,
    AUTH_RELIABLE = 1,
    GAME_RELIABLE = 2,
    GAME_UNRELIABLE = 3,

    COUNT
};