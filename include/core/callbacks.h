#pragma once

#include <RC2D/RC2D.h>

void rc2d_unload(void);
void rc2d_load(void);
void rc2d_update(double dt);
void rc2d_simulation_update(uint64_t currentTick, uint64_t dtNs, double dt);
void rc2d_network_host_setup(ENetHost* host);
void rc2d_network_incoming_update(ENetHost* host, const ENetEvent* event);
void rc2d_network_outgoing_update(ENetHost* host);
void rc2d_http_update(void);
void rc2d_websocket_update(void);
void rc2d_wake_blocking_threads(void);
void rc2d_draw(void);
void rc2d_mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID);
void rc2d_textinput(const RC2D_TextInputEventInfo* info);
void rc2d_mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID);
void rc2d_keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID);
void rc2d_localechanged(RC2D_Locale* locales);
