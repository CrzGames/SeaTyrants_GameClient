#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.

// Version protocole réseau client -> serveur
static constexpr uint16_t CLIENT_NETWORK_PROTOCOL_VERSION_MAJOR = 1;
static constexpr uint16_t CLIENT_NETWORK_PROTOCOL_VERSION_MINOR = 2;
static constexpr uint16_t CLIENT_NETWORK_PROTOCOL_VERSION_PATCH = 5;

static constexpr uint32_t CLIENT_NETWORK_PROTOCOL_VERSION = (
    (static_cast<uint32_t>(CLIENT_NETWORK_PROTOCOL_VERSION_MAJOR) << 16) |
    (static_cast<uint32_t>(CLIENT_NETWORK_PROTOCOL_VERSION_MINOR) << 8) |
    (static_cast<uint32_t>(CLIENT_NETWORK_PROTOCOL_VERSION_PATCH))
);
