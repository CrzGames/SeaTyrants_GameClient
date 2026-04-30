#pragma once

#include <RC2D/RC2D.h>

#include <cctype>

enum class TextInputShortcut {
    SELECT_ALL,
    COPY,
    PASTE,
    UNDO
};

inline bool isTextInputShortcutPressed(
    TextInputShortcut shortcut,
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (isrepeat || (mod & SDL_KMOD_CTRL) == 0 || (mod & SDL_KMOD_ALT) != 0)
    {
        return false;
    }

    char expectedLetter = '\0';
    SDL_Scancode expectedScancode = SDL_SCANCODE_UNKNOWN;
    SDL_Keycode expectedKeycode = SDLK_UNKNOWN;

    switch (shortcut)
    {
        case TextInputShortcut::SELECT_ALL:
            expectedLetter = 'a';
            expectedScancode = SDL_SCANCODE_A;
            expectedKeycode = SDLK_A;
            break;
        case TextInputShortcut::COPY:
            expectedLetter = 'c';
            expectedScancode = SDL_SCANCODE_C;
            expectedKeycode = SDLK_C;
            break;
        case TextInputShortcut::PASTE:
            expectedLetter = 'v';
            expectedScancode = SDL_SCANCODE_V;
            expectedKeycode = SDLK_V;
            break;
        case TextInputShortcut::UNDO:
            expectedLetter = 'z';
            expectedScancode = SDL_SCANCODE_Z;
            expectedKeycode = SDLK_Z;
            break;
        default:
            return false;
    }

    if (scancode == expectedScancode || keycode == expectedKeycode)
    {
        return true;
    }

    if (key != nullptr && key[0] != '\0' && key[1] == '\0')
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(key[0]))) == expectedLetter;
    }

    return false;
}
