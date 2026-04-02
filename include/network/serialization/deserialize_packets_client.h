#pragma once

#include <cstddef> // size_t

#include "network/packets/client/reliable.h"
#include "network/packets/client/unreliable.h"

bool deserializeClientInputPacketUnreliable(const void* data, size_t size, ClientInputPacketUnreliable& outPacket);