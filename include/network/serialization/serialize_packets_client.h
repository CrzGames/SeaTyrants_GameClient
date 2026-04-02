#pragma once

#include <vector>  // std::vector
#include <cstdint> // uint8_t, uint32_t, etc.

#include "network/packets/client/reliable.h"
#include "network/packets/client/unreliable.h"

std::vector<uint8_t> serializeClientSecureSessionHelloPacketReliable(const ClientSecureSessionHelloPacketReliable& packet);
std::vector<uint8_t> serializeClientAuthPacketReliable(const ClientAuthPacketReliable& packet);
std::vector<uint8_t> serializeClientReadyForMatchPacketReliable(const ClientReadyForMatchPacketReliable& packet);
std::vector<uint8_t> serializeClientInputPacketUnreliable(const ClientInputPacketUnreliable& packet);
std::vector<uint8_t> serializeClientClockSyncPacketUnreliable(const ClientClockSyncPacketUnreliable& packet);
