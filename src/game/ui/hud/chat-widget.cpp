#include "game/ui/hud/chat-widget.h"
#include "game/assets/title-asset-cache.h"

#include <RC2D/RC2D_keyboard.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "core/context.h"

// ---------------------------------------------------------------------------
// Constantes de reference visuelle (maquette 502x349 + palette).
// ---------------------------------------------------------------------------
static constexpr float kRefW = 502.0f;
static constexpr float kRefH = 349.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTabFill = RC2D_Color{84, 10, 9, 233};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kInputFill = RC2D_Color{36, 36, 37, 235};
static constexpr RC2D_Color kInputSelectionFill = RC2D_Color{67, 96, 144, 215};
static constexpr RC2D_Color kMessageTextColor = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kPlaceholderTextColor = RC2D_Color{145, 152, 166, 235};
static constexpr RC2D_Color kCursorColor = RC2D_Color{239, 226, 163, 255};
static constexpr RC2D_Color kScrollTrackColor = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumbColor = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kScrollThumbDragFill = RC2D_Color{184, 132, 30, 245};

static constexpr float kCursorBlinkPeriod = 0.55f;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
static constexpr std::size_t kMaxChatInputBytes = 2048U;

/**
 * @brief Ligne visuelle issue du wrapping d'un message.
 */
struct ChatWrappedLine
{
    std::string text;        /**< Texte affiche sur cette ligne. */
    int messageIndex;        /**< Index du message source dans ChatWidget::chatMessages. */
    std::size_t startIndex;  /**< Debut (inclus) dans la chaine source. */
    std::size_t endIndex;    /**< Fin (exclue) dans la chaine source. */
};

/**
 * @brief Rectangle chat calcule depuis le gameScreen.
 *
 * Ce calcul est volontairement local au ChatWidget pour eviter
 * que le HUD externe gere ses details de placement interne.
 */
static SDL_FRect getChatWidgetRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    constexpr float kChatW = 502.0f;
    constexpr float kChatH = 349.0f;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kChatW) * 0.5f),
        screenRect.y + ((screenRect.h - kChatH) * 0.5f),
        kChatW,
        kChatH
    };
}

/**
 * @brief Teste si un point est a l'interieur d'un rectangle.
 */
static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

/**
 * @brief Clamp float basique.
 */
static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
}

/**
 * @brief Recupere la position souris en coordonnees de rendu.
 *
 * RC2D expose la souris en coordonnees fenetre, mais nos hit-tests HUD
 * utilisent les coordonnees de rendu. Cette conversion evite les decrochages
 * visuels lors des drags quand le viewport est scale.
 */
static void getMouseRenderPosition(float* outX, float* outY)
{
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        *outX = windowX;
        *outY = windowY;
        return;
    }

    float renderX = windowX;
    float renderY = windowY;
    if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &renderX, &renderY))
    {
        renderX = windowX;
        renderY = windowY;
    }

    *outX = renderX;
    *outY = renderY;
}

/**
 * @brief Mesure la largeur pixel d'un texte.
 */
static float measureTextWidth(RC2D_Font* font, const std::string& text)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_destroyText(&t);
    return static_cast<float>(w);
}

/**
 * @brief Mesure la hauteur de ligne moyenne de la police.
 */
static float measureTextHeight(RC2D_Font* font)
{
    if (font == nullptr || font->sdl_font == nullptr)
    {
        return 16.0f;
    }

    RC2D_Text t = rc2d_graphics_createText(font, "Ag");
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_destroyText(&t);
    return static_cast<float>((std::max)(h, 14));
}

/**
 * @brief Dessine une chaine a position absolue.
 */
static void drawTextAt(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color c)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty()) { return; }
    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    t.color = c;
    rc2d_graphics_setTextColor(&t);
    rc2d_graphics_drawText(&t, x, y);
    rc2d_graphics_destroyText(&t);
}

/**
 * @brief Dessine un texte centre dans un rectangle.
 */
static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& r, RC2D_Color c)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0') { return; }
    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = c;
    rc2d_graphics_setTextColor(&t);
    int w = 0; int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_drawText(&t, r.x + ((r.w - static_cast<float>(w)) * 0.5f), r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_destroyText(&t);
}

/**
 * @brief Dessine un texte aligne a gauche, centre verticalement.
 */
static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& r, float x, RC2D_Color c)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0') { return; }
    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = c;
    rc2d_graphics_setTextColor(&t);
    int w = 0; int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_drawText(&t, x, r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_destroyText(&t);
}

/**
 * @brief Construit les lignes visuelles wrappees de tous les messages du chat.
 *
 * Le wrapping est volontairement caractere-par-caractere pour occuper
 * exactement toute la largeur utile jusqu'a la scrollbar.
 */
static std::vector<ChatWrappedLine> buildWrappedLines(
    const std::vector<std::string>& chatMessages,
    RC2D_Font* font,
    float maxWidth)
{
    std::vector<ChatWrappedLine> lines;

    if (maxWidth <= 0.0f)
    {
        return lines;
    }

    for (std::size_t messageIndex = 0; messageIndex < chatMessages.size(); ++messageIndex)
    {
        const std::string& message = chatMessages[messageIndex];

        // Message vide => reserve une ligne vide.
        if (message.empty())
        {
            lines.push_back(ChatWrappedLine{std::string{}, static_cast<int>(messageIndex), 0, 0});
            continue;
        }

        // Gestion des paragraphes separes par '\n'.
        std::size_t paragraphStart = 0;
        while (paragraphStart <= message.size())
        {
            const std::size_t paragraphEnd = message.find('\n', paragraphStart);
            const std::size_t end = (paragraphEnd == std::string::npos) ? message.size() : paragraphEnd;
            const std::size_t paragraphLen = end - paragraphStart;

            // Paragraphe vide => ligne vide explicite.
            if (paragraphLen == 0)
            {
                lines.push_back(ChatWrappedLine{
                    std::string{},
                    static_cast<int>(messageIndex),
                    paragraphStart,
                    paragraphStart
                });
            }
            else
            {
                // Wrap caractere-par-caractere.
                std::size_t localStart = 0;
                while (localStart < paragraphLen)
                {
                    std::size_t localFit = localStart;
                    while (localFit < paragraphLen)
                    {
                        const std::size_t candidateLen = (localFit - localStart) + 1;
                        const std::string candidate = message.substr(paragraphStart + localStart, candidateLen);
                        if (measureTextWidth(font, candidate) > maxWidth)
                        {
                            break;
                        }
                        ++localFit;
                    }

                    // Au moins un caractere par ligne, meme si police/width extremes.
                    if (localFit == localStart)
                    {
                        localFit = localStart + 1;
                    }

                    const std::size_t globalStart = paragraphStart + localStart;
                    const std::size_t globalEnd = paragraphStart + localFit;
                    lines.push_back(ChatWrappedLine{
                        message.substr(globalStart, globalEnd - globalStart),
                        static_cast<int>(messageIndex),
                        globalStart,
                        globalEnd
                    });

                    localStart = localFit;
                }
            }

            if (paragraphEnd == std::string::npos)
            {
                break;
            }
            paragraphStart = paragraphEnd + 1;
        }
    }

    if (lines.empty())
    {
        lines.push_back(ChatWrappedLine{std::string{}, -1, 0, 0});
    }
    return lines;
}

/**
 * @brief Convertit une touche clavier en caractere imprimable.
 */
static bool keyToPrintableChar(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    char* outChar)
{
    if (outChar == nullptr)
    {
        return false;
    }
    *outChar = '\0';

    // Ignore les raccourcis Ctrl/Alt (on ne veut pas polluer l'input chat).
    if ((mod & SDL_KMOD_CTRL) != 0 || (mod & SDL_KMOD_ALT) != 0)
    {
        return false;
    }

    // Priorite a la chaine event si elle contient exactement 1 caractere imprimable.
    if (key != nullptr && std::strlen(key) == 1)
    {
        const unsigned char c = static_cast<unsigned char>(key[0]);
        if (std::isprint(c) != 0)
        {
            *outChar = static_cast<char>(c);
            return true;
        }
    }

    const bool shiftDown = ((mod & SDL_KMOD_SHIFT) != 0);
    const bool capsDown = ((mod & SDL_KMOD_CAPS) != 0);

    // Lettres A-Z.
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
    {
        const int index = static_cast<int>(scancode - SDL_SCANCODE_A);
        char c = static_cast<char>('a' + index);
        if (shiftDown != capsDown)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        *outChar = c;
        return true;
    }

    // Chiffres pavé numérique (num lock on).
    if (scancode >= SDL_SCANCODE_KP_0 && scancode <= SDL_SCANCODE_KP_9)
    {
        const int index = static_cast<int>(scancode - SDL_SCANCODE_KP_0);
        *outChar = static_cast<char>('0' + index);
        return true;
    }

    // Ponctuation/chiffres.
    switch (keycode)
    {
        case SDLK_SPACE: *outChar = ' '; return true;
        case SDLK_PERIOD: *outChar = shiftDown ? '>' : '.'; return true;
        case SDLK_COMMA: *outChar = shiftDown ? '<' : ','; return true;
        case SDLK_MINUS: *outChar = shiftDown ? '_' : '-'; return true;
        case SDLK_EQUALS: *outChar = shiftDown ? '+' : '='; return true;
        case SDLK_SEMICOLON: *outChar = shiftDown ? ':' : ';'; return true;
        case SDLK_APOSTROPHE: *outChar = shiftDown ? '"' : '\''; return true;
        case SDLK_SLASH: *outChar = shiftDown ? '?' : '/'; return true;
        case SDLK_BACKSLASH: *outChar = shiftDown ? '|' : '\\'; return true;
        case SDLK_LEFTBRACKET: *outChar = shiftDown ? '{' : '['; return true;
        case SDLK_RIGHTBRACKET: *outChar = shiftDown ? '}' : ']'; return true;
        case SDLK_0: *outChar = shiftDown ? ')' : '0'; return true;
        case SDLK_1: *outChar = shiftDown ? '!' : '1'; return true;
        case SDLK_2: *outChar = shiftDown ? '@' : '2'; return true;
        case SDLK_3: *outChar = shiftDown ? '#' : '3'; return true;
        case SDLK_4: *outChar = shiftDown ? '$' : '4'; return true;
        case SDLK_5: *outChar = shiftDown ? '%' : '5'; return true;
        case SDLK_6: *outChar = shiftDown ? '^' : '6'; return true;
        case SDLK_7: *outChar = shiftDown ? '&' : '7'; return true;
        case SDLK_8: *outChar = shiftDown ? '*' : '8'; return true;
        case SDLK_9: *outChar = shiftDown ? '(' : '9'; return true;
        case SDLK_KP_0: *outChar = '0'; return true;
        case SDLK_KP_1: *outChar = '1'; return true;
        case SDLK_KP_2: *outChar = '2'; return true;
        case SDLK_KP_3: *outChar = '3'; return true;
        case SDLK_KP_4: *outChar = '4'; return true;
        case SDLK_KP_5: *outChar = '5'; return true;
        case SDLK_KP_6: *outChar = '6'; return true;
        case SDLK_KP_7: *outChar = '7'; return true;
        case SDLK_KP_8: *outChar = '8'; return true;
        case SDLK_KP_9: *outChar = '9'; return true;
        case SDLK_KP_PERIOD: *outChar = '.'; return true;
        default:
            break;
    }

    return false;
}

static bool isUtf8ContinuationByte(unsigned char byteValue)
{
    return (byteValue & 0xC0U) == 0x80U;
}

static std::vector<std::size_t> buildUtf8CodepointOffsets(const std::string& text)
{
    std::vector<std::size_t> offsets{};
    offsets.reserve(text.size() + 1U);
    offsets.push_back(0U);

    std::size_t index = 0U;
    while (index < text.size())
    {
        ++index;
        while (index < text.size() &&
               isUtf8ContinuationByte(static_cast<unsigned char>(text[index])))
        {
            ++index;
        }

        offsets.push_back(index);
    }

    return offsets;
}

static std::size_t clampByteOffsetToUtf8Boundary(const std::vector<std::size_t>& offsets, std::size_t byteOffset)
{
    if (offsets.empty())
    {
        return 0U;
    }

    const std::size_t clamped = (std::min)(byteOffset, offsets.back());
    std::size_t previousOffset = 0U;
    for (std::size_t offset : offsets)
    {
        if (offset == clamped)
        {
            return offset;
        }
        if (offset > clamped)
        {
            break;
        }

        previousOffset = offset;
    }

    return previousOffset;
}

static std::size_t findCodepointIndexForByteOffset(const std::vector<std::size_t>& offsets, std::size_t byteOffset)
{
    if (offsets.empty())
    {
        return 0U;
    }

    const std::size_t clampedOffset = clampByteOffsetToUtf8Boundary(offsets, byteOffset);
    for (std::size_t index = 0U; index < offsets.size(); ++index)
    {
        if (offsets[index] == clampedOffset)
        {
            return index;
        }
    }

    return offsets.size() - 1U;
}

static std::string sanitizeTextInput(const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        return {};
    }

    std::string sanitized{};
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0U; ++cursor)
    {
        if ((*cursor < 32U || *cursor == 127U) && *cursor != ' ')
        {
            continue;
        }

        sanitized.push_back(static_cast<char>(*cursor));
    }

    return sanitized;
}

ChatWidget::ChatWidget(void)
    : chatMessages{},
      titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      inputBuffer{},
      cursorIndex(0),
      selectionAnchorIndex(0),
      inputFocused(false),
      inputSelectingWithMouse(false),
      cursorVisible(true),
      cursorBlinkElapsed(0.0),
      scrollFirstLine(0),
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
      scrollBarWheelHighlightSec(0.0f),
      visible(true),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      widgetWidth(kRefW),
      widgetHeight(kRefH),
      widgetDragLocked(false),
      widgetResizing(false),
      resizeStartMouseX(0.0f),
      resizeStartMouseY(0.0f),
      resizeStartWidth(kRefW),
      resizeStartHeight(kRefH),
      cursorEnabled(true),
      controlIcons{}
{
}

ChatWidget::~ChatWidget(void)
{
}

void ChatWidget::load(void)
{
    // 1) Chargement des polices.
    this->titleFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        20.0f);
    this->bodyFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        14.0f);
    this->controlIcons.load();

    // 2) Reset de l'etat runtime du chat.
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetWidth = baseRect.w;
    this->widgetHeight = baseRect.h;
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };
    this->inputBuffer.clear();
    this->cursorIndex = 0;
    this->selectionAnchorIndex = 0;
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
    this->scrollBarWheelHighlightSec = 0.0f;
    this->visible = false;
    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->widgetDragLocked = false;
    this->widgetResizing = false;
    this->resizeStartMouseX = 0.0f;
    this->resizeStartMouseY = 0.0f;
    this->resizeStartWidth = this->widgetWidth;
    this->resizeStartHeight = this->widgetHeight;
    this->chatMessages.clear();
    this->scrollFirstLine = 0;
    this->syncPlatformTextInput();

    // 3) Messages systeme initiaux propres au widget.
    this->publishChatMessage(ChatWidget::ChatMessageAuthor::SYSTEM, "Bienvenue dans le chat du serveur ! Sois respectueux et amuse-toi ! On surveille...");
}

void ChatWidget::unload(void)
{
    this->inputFocused = false;
    this->syncPlatformTextInput();
    this->controlIcons.unload();
    // Libere les ressources TTF.
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void ChatWidget::publishChatMessage(ChatWidget::ChatMessageAuthor author, const std::string& message, const std::string& playerName)
{
    if (message.empty())
    {
        return;
    }

    std::string finalMessage;
    switch (author)
    {
        case ChatWidget::ChatMessageAuthor::SYSTEM:
            finalMessage = "System: " + message;
            break;
        case ChatWidget::ChatMessageAuthor::PLAYER:
            finalMessage = (playerName.empty() ? "Joueur" : playerName) + ": " + message;
            break;
        case ChatWidget::ChatMessageAuthor::SELF:
            finalMessage = "Moi: " + message;
            break;
        default:
            finalMessage = message;
            break;
    }
    
    // Ignore les messages vides.
    if (finalMessage.empty())
    {
        return;
    }

    // Ajoute le message en fin d'historique.
    this->chatMessages.push_back(finalMessage);

    // Respecte la limite max d'historique.
    if (this->chatMessages.size() > maxStoredChatMessages)
    {
        const std::size_t overflowCount = this->chatMessages.size() - maxStoredChatMessages;
        this->chatMessages.erase(this->chatMessages.begin(), this->chatMessages.begin() + overflowCount);
    }

    // Force un scroll tout en bas (valeur volontairement haute, clamp plus tard).
    this->scrollFirstLine = 1000000;
}

void ChatWidget::update(double dt)
{
    // Met a jour le rectangle chat depuis le gameScreen + offsets de drag.
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };

    // Si le drag est verrouille, on stoppe un drag en cours.
    if (this->widgetDragLocked)
    {
        this->widgetDragging = false;
    }

    // Redimensionnement (encoche bas-droite).
    if (this->widgetResizing)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->widgetResizing = false;
        }
        else
        {
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            const float deltaX = mouseX - this->resizeStartMouseX;
            const float deltaY = mouseY - this->resizeStartMouseY;

            const SDL_FRect screenRect = GetGameScreen().rect;
            const float minWidth = kRefW;
            const float minHeight = kRefH;
            const float maxWidth = (std::max)(minWidth, screenRect.w - 8.0f);
            const float maxHeight = (std::max)(minHeight, screenRect.h - 8.0f);

            this->widgetWidth = clampf(this->resizeStartWidth + deltaX, minWidth, maxWidth);
            this->widgetHeight = clampf(this->resizeStartHeight + deltaY, minHeight, maxHeight);
            this->widgetRect.w = this->widgetWidth;
            this->widgetRect.h = this->widgetHeight;
        }
    }

    // Drag fenetre (barre Chat).
    if (this->widgetDragging)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->widgetDragging = false;
        }
        else
        {
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
            this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
            this->widgetRect.x = baseRect.x + this->widgetOffsetX;
            this->widgetRect.y = baseRect.y + this->widgetOffsetY;
        }
    }

    // Si le widget est ferme, on coupe ici toute update interactive interne.
    if (!this->visible)
    {
        this->clearFocus();
        this->scrollBarDragging = false;
        this->widgetResizing = false;
        this->scrollBarWheelHighlightSec = 0.0f;
        return;
    }

    const float wheelDt = static_cast<float>(dt);
    if (this->scrollBarWheelHighlightSec > 0.0f)
    {
        this->scrollBarWheelHighlightSec -= wheelDt;
        if (this->scrollBarWheelHighlightSec < 0.0f)
        {
            this->scrollBarWheelHighlightSec = 0.0f;
        }
    }

    // --- Selection de texte au drag dans la barre input ---
    if (this->inputSelectingWithMouse)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->inputSelectingWithMouse = false;
        }
        else if (this->inputFocused)
        {
            const float extraW = this->widgetRect.w - kRefW;
            const float extraH = this->widgetRect.h - kRefH;
            const auto RX = [&](float x) { return this->widgetRect.x + x; };
            const auto RY = [&](float y) { return this->widgetRect.y + y; };
            const auto RW = [&](float w) { return w; };
            const auto RH = [&](float h) { return h; };
            const SDL_FRect inputArea = SDL_FRect{RX(12.0f), RY(298.0f) + extraH, RW(478.0f) + extraW, RH(39.0f)};

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            (void)mouseY;
            this->cursorIndex = this->getInputCursorIndexFromPosition(mouseX, inputArea);
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
        }
    }

    // --- Blink curseur ---
    if (this->inputFocused)
    {
        this->cursorBlinkElapsed += dt;
        while (this->cursorBlinkElapsed >= kCursorBlinkPeriod)
        {
            this->cursorBlinkElapsed -= kCursorBlinkPeriod;
            this->cursorVisible = !this->cursorVisible;
        }
    }
    else
    {
        this->cursorBlinkElapsed = 0.0;
        this->cursorVisible = false;
    }

    // --- Drag scrollbar ---
    if (!this->scrollBarDragging)
    {
        return;
    }
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->scrollBarDragging = false;
        return;
    }

    // Recalcule les zones de layout.
    const float extraW = this->widgetRect.w - kRefW;
    const float extraH = this->widgetRect.h - kRefH;
    const auto RX = [&](float x) { return this->widgetRect.x + x; };
    const auto RY = [&](float y) { return this->widgetRect.y + y; };
    const auto RW = [&](float w) { return w; };
    const auto RH = [&](float h) { return h; };

    // Seule la zone des messages (et scrollbar) s'agrandit.
    const SDL_FRect msgArea = SDL_FRect{RX(12.0f), RY(82.0f), RW(478.0f) + extraW, RH(214.0f) + extraH};
    const SDL_FRect msgTextClip = SDL_FRect{
        msgArea.x + RW(6.0f),
        msgArea.y + RH(6.0f),
        msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
        msgArea.h - RH(6.0f * 2.0f)
    };
    const SDL_FRect scrollTrack = SDL_FRect{
        msgArea.x + msgArea.w - RW(kScrollBarWidth + kScrollBarPadding),
        msgArea.y + RH(kScrollBarPadding),
        RW(kScrollBarWidth),
        msgArea.h - RH(kScrollBarPadding * 2.0f)
    };

    // Mesure line-height et nombre de lignes reelles (apres wrapping).
    const float lineHeight = measureTextHeight(&this->bodyFont) + RH(2.0f);
    const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(this->chatMessages, &this->bodyFont, msgTextClip.w);
    const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
    const int totalLines = static_cast<int>(wrappedLines.size());
    const int maxFirstLine = (std::max)(0, totalLines - visibleLines);

    if (maxFirstLine <= 0)
    {
        this->scrollFirstLine = 0;
        this->scrollBarDragging = false;
        return;
    }

    // Geometrie du thumb.
    const float thumbHeight = (std::max)(RH(kMinThumbHeight), (scrollTrack.h * static_cast<float>(visibleLines) / static_cast<float>(totalLines)));
    const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);

    // Position souris et conversion en ratio [0..1] de scroll.
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    const float newThumbY = clampf(mouseY - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
    const float scrollRatio = (newThumbY - scrollTrack.y) / thumbTravel;
    this->scrollFirstLine = static_cast<int>(scrollRatio * static_cast<float>(maxFirstLine) + 0.5f);
}

bool ChatWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)mouseID;

    // Synchronise le rect avant hit-tests (centre + offsets drag).
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };

    if (!this->visible)
    {
        return false;
    }

    // Clic hors widget: retire focus input + stop drag.
    if (!isPointInRect(x, y, this->widgetRect))
    {
        this->clearFocus();
        this->scrollBarDragging = false;
        this->widgetResizing = false;
        return false;
    }

    // Recalcule zones.
    const float extraW = this->widgetRect.w - kRefW;
    const float extraH = this->widgetRect.h - kRefH;
    const auto RX = [&](float xValue) { return this->widgetRect.x + xValue; };
    const auto RY = [&](float yValue) { return this->widgetRect.y + yValue; };
    const auto RW = [&](float wValue) { return wValue; };
    const auto RH = [&](float hValue) { return hValue; };

    const SDL_FRect msgArea = SDL_FRect{RX(12.0f), RY(82.0f), RW(478.0f) + extraW, RH(214.0f) + extraH};
    const SDL_FRect inputArea = SDL_FRect{RX(12.0f), RY(298.0f) + extraH, RW(478.0f) + extraW, RH(39.0f)};
    const SDL_FRect header = SDL_FRect{
        this->widgetRect.x + RW(6.0f),
        this->widgetRect.y + RH(7.0f),
        this->widgetRect.w - RW(10.0f),
        RH(35.0f)
    };
    const SDL_FRect closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - RW(31.0f),
        header.y + ((header.h - RH(20.0f)) * 0.5f),
        RW(20.0f),
        RH(20.0f)
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - RW(24.0f),
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect resizeHandleRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - RW(16.0f),
        this->widgetRect.y + this->widgetRect.h - RH(16.0f),
        RW(14.0f),
        RH(14.0f)
    };
    const SDL_FRect scrollTrack = SDL_FRect{
        msgArea.x + msgArea.w - RW(kScrollBarWidth + kScrollBarPadding),
        msgArea.y + RH(kScrollBarPadding),
        RW(kScrollBarWidth),
        msgArea.h - RH(kScrollBarPadding * 2.0f)
    };

    // Fallback wheel remonte via boutons 4/5 (certains environnements).
    const int buttonValue = static_cast<int>(button);
    if ((buttonValue == 4 || buttonValue == 5) && isPointInRect(x, y, msgArea))
    {
        const SDL_FRect msgTextClip = SDL_FRect{
            msgArea.x + RW(6.0f),
            msgArea.y + RH(6.0f),
            msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
            msgArea.h - RH(6.0f * 2.0f)
        };
        const float lineHeight = measureTextHeight(&this->bodyFont) + RH(2.0f);
        const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(this->chatMessages, &this->bodyFont, msgTextClip.w);
        const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
        const int maxFirstLine = (std::max)(0, static_cast<int>(wrappedLines.size()) - visibleLines);

        this->scrollFirstLine += (buttonValue == 4) ? -2 : 2;
        this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
        return true;
    }

    // Ne consomme que le clic gauche pour edition/drag.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    // Bouton cadenas: active/desactive le verrouillage du drag de fenetre.
    if (isPointInRect(x, y, lockButtonRect))
    {
        this->widgetDragLocked = !this->widgetDragLocked;
        this->widgetDragging = false;
        this->widgetResizing = false;
        return true;
    }

    // Encoche bas-droite: debut du redimensionnement.
    if (isPointInRect(x, y, resizeHandleRect))
    {
        this->widgetResizing = true;
        this->resizeStartMouseX = x;
        this->resizeStartMouseY = y;
        this->resizeStartWidth = this->widgetWidth;
        this->resizeStartHeight = this->widgetHeight;
        this->widgetDragging = false;
        this->scrollBarDragging = false;
        this->clearFocus();
        return true;
    }

    // Croix: ferme le chat.
    if (isPointInRect(x, y, closeButtonRect))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        this->clearFocus();
        return true;
    }

    // Barre header: drag de la fenetre chat.
    if (isPointInRect(x, y, header))
    {
        if (this->widgetDragLocked)
        {
            // Header verrouille: on consomme le clic sans lancer de drag.
            return true;
        }
        this->widgetDragging = true;
        this->widgetResizing = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearFocus();
        return true;
    }

    // --- Clic dans la barre input ---
    if (isPointInRect(x, y, inputArea))
    {
        this->inputFocused = true;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        this->syncPlatformTextInput();

        // Double-clic: selection complete du texte saisi.
        if (clicks >= 2)
        {
            this->selectionAnchorIndex = 0;
            this->cursorIndex = this->inputBuffer.size();
            this->inputSelectingWithMouse = false;
            return true;
        }

        // Clic simple: pose le curseur et prepare une selection eventuelle au drag.
        const std::size_t clickedIndex = this->getInputCursorIndexFromPosition(x, inputArea);
        this->cursorIndex = clickedIndex;
        this->selectionAnchorIndex = clickedIndex;
        this->inputSelectingWithMouse = true;
        return true;
    }

    // Clic ailleurs dans le widget => retire focus input.
    this->clearFocus();

    // --- Clic dans la zone messages / scrollbar ---
    if (isPointInRect(x, y, msgArea))
    {
        const float lineHeight = measureTextHeight(&this->bodyFont) + RH(2.0f);
        const SDL_FRect msgTextClip = SDL_FRect{
            msgArea.x + RW(6.0f),
            msgArea.y + RH(6.0f),
            msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
            msgArea.h - RH(6.0f * 2.0f)
        };
        const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(this->chatMessages, &this->bodyFont, msgTextClip.w);
        const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
        const int totalLines = static_cast<int>(wrappedLines.size());
        const int maxFirstLine = (std::max)(0, totalLines - visibleLines);

        if (maxFirstLine > 0 && isPointInRect(x, y, scrollTrack))
        {
            const float thumbHeight = (std::max)(RH(kMinThumbHeight), (scrollTrack.h * static_cast<float>(visibleLines) / static_cast<float>(totalLines)));
            const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);
            const float currentRatio = static_cast<float>((std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine))) / static_cast<float>(maxFirstLine);
            const float thumbY = scrollTrack.y + (thumbTravel * currentRatio);
            const SDL_FRect thumb = SDL_FRect{scrollTrack.x, thumbY, scrollTrack.w, thumbHeight};

            if (isPointInRect(x, y, thumb))
            {
                // Grab direct du thumb.
                this->scrollBarDragging = true;
                this->scrollDragOffsetY = y - thumb.y;
            }
            else
            {
                // Clic piste: positionne le thumb sous le pointeur + active drag.
                this->scrollBarDragging = true;
                this->scrollDragOffsetY = thumbHeight * 0.5f;
                const float thumbTop = clampf(y - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
                const float scrollRatio = (thumbTop - scrollTrack.y) / thumbTravel;
                this->scrollFirstLine = static_cast<int>(scrollRatio * static_cast<float>(maxFirstLine) + 0.5f);
            }
        }
    }

    return true;
}

bool ChatWidget::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float wheel_x,
    float wheel_y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)wheel_x;
    (void)integer_x;
    (void)mouseID;

    // Synchronise le rect avant hit-tests (centre + offsets drag).
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };

    if (!this->visible)
    {
        return false;
    }

    // On consomme la molette uniquement si pointeur dans le widget + zone messages.
    if (!isPointInRect(mouse_x, mouse_y, this->widgetRect))
    {
        return false;
    }

    const float extraW = this->widgetRect.w - kRefW;
    const float extraH = this->widgetRect.h - kRefH;
    const auto RX = [&](float xValue) { return this->widgetRect.x + xValue; };
    const auto RY = [&](float yValue) { return this->widgetRect.y + yValue; };
    const auto RW = [&](float wValue) { return wValue; };
    const auto RH = [&](float hValue) { return hValue; };

    const SDL_FRect msgArea = SDL_FRect{RX(12.0f), RY(82.0f), RW(478.0f) + extraW, RH(214.0f) + extraH};
    if (!isPointInRect(mouse_x, mouse_y, msgArea))
    {
        return false;
    }

    // Prepare le contexte de scroll.
    const SDL_FRect msgTextClip = SDL_FRect{
        msgArea.x + RW(6.0f),
        msgArea.y + RH(6.0f),
        msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
        msgArea.h - RH(6.0f * 2.0f)
    };
    const float lineHeight = measureTextHeight(&this->bodyFont) + RH(2.0f);
    const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(this->chatMessages, &this->bodyFont, msgTextClip.w);
    const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
    const int maxFirstLine = (std::max)(0, static_cast<int>(wrappedLines.size()) - visibleLines);

    if (maxFirstLine <= 0)
    {
        this->scrollFirstLine = 0;
        return true;
    }

    // Convertit l'event RC2D en delta lignes.
    int delta = static_cast<int>(integer_y);
    if (delta == 0)
    {
        if (wheel_y > 0.0f) { delta = 1; }
        else if (wheel_y < 0.0f) { delta = -1; }
    }
    if (delta == 0)
    {
        if (direction == RC2D_SCROLL_UP) { delta = 1; }
        else if (direction == RC2D_SCROLL_DOWN) { delta = -1; }
    }

    // Applique et clamp.
    if (delta != 0)
    {
        const int lineBefore = this->scrollFirstLine;
        this->scrollFirstLine -= delta;
        this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
        if (this->scrollFirstLine != lineBefore)
        {
            this->scrollBarWheelHighlightSec = kScrollThumbWheelHighlightSec;
        }
    }
    return true;
}

bool ChatWidget::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    // Si le chat est masque, il ne doit plus consommer le clavier.
    if (!this->visible)
    {
        return false;
    }

    // Si l'input n'a pas le focus, on ne consomme pas.
    if (!this->inputFocused)
    {
        return false;
    }

    (void)isrepeat;

    // Raccourcis d'edition.
    switch (scancode)
    {
        case SDL_SCANCODE_LEFT:
            if (this->hasInputSelection())
            {
                this->cursorIndex = this->getInputSelectionStart();
            }
            else if (this->cursorIndex > 0)
            {
                const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
                const std::size_t caretIndex = findCodepointIndexForByteOffset(offsets, this->cursorIndex);
                if (caretIndex > 0U)
                {
                    this->cursorIndex = offsets[caretIndex - 1U];
                }
            }
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_RIGHT:
            if (this->hasInputSelection())
            {
                this->cursorIndex = this->getInputSelectionEnd();
            }
            else if (this->cursorIndex < this->inputBuffer.size())
            {
                const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
                const std::size_t caretIndex = findCodepointIndexForByteOffset(offsets, this->cursorIndex);
                if (caretIndex + 1U < offsets.size())
                {
                    this->cursorIndex = offsets[caretIndex + 1U];
                }
            }
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_HOME:
            this->cursorIndex = 0;
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_END:
            this->cursorIndex = this->inputBuffer.size();
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_BACKSPACE:
            if (this->hasInputSelection())
            {
                this->deleteSelectedInputText();
            }
            else if (this->cursorIndex > 0 && !this->inputBuffer.empty())
            {
                const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
                const std::size_t caretIndex = findCodepointIndexForByteOffset(offsets, this->cursorIndex);
                if (caretIndex > 0U)
                {
                    const std::size_t eraseStart = offsets[caretIndex - 1U];
                    this->inputBuffer.erase(eraseStart, this->cursorIndex - eraseStart);
                    this->cursorIndex = eraseStart;
                }
            }
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_DELETE:
            if (this->hasInputSelection())
            {
                this->deleteSelectedInputText();
            }
            else if (this->cursorIndex < this->inputBuffer.size())
            {
                const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
                const std::size_t caretIndex = findCodepointIndexForByteOffset(offsets, this->cursorIndex);
                if (caretIndex + 1U < offsets.size())
                {
                    this->inputBuffer.erase(this->cursorIndex, offsets[caretIndex + 1U] - this->cursorIndex);
                }
            }
            this->clearInputSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            // Validation: pousse le message puis reset input.
            if (!this->inputBuffer.empty())
            {
                this->publishChatMessage(ChatWidget::ChatMessageAuthor::SELF, this->inputBuffer);
                this->inputBuffer.clear();
                this->cursorIndex = 0;
                this->clearInputSelection();
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_ESCAPE:
            this->clearFocus();
            return true;

        case SDL_SCANCODE_UP:
            this->scrollFirstLine -= 1;
            this->scrollFirstLine = (std::max)(0, this->scrollFirstLine);
            return true;

        case SDL_SCANCODE_DOWN:
            this->scrollFirstLine += 1;
            return true;

        default:
            break;
    }

    if (!isrepeat &&
        (mod & SDL_KMOD_CTRL) != 0 &&
        scancode == SDL_SCANCODE_A)
    {
        this->selectionAnchorIndex = 0U;
        this->cursorIndex = this->inputBuffer.size();
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    // Les caracteres imprimables sont maintenant fournis par le callback textinput.
    char ignoredCharacter = '\0';
    if (keyToPrintableChar(key, scancode, keycode, mod, &ignoredCharacter))
    {
        return true;
    }

    return false;
}

bool ChatWidget::textinput(const char* text)
{
    if (!this->visible || !this->inputFocused)
    {
        return false;
    }

    const std::string sanitized = sanitizeTextInput(text);
    if (sanitized.empty())
    {
        return false;
    }

    if (this->hasInputSelection())
    {
        this->deleteSelectedInputText();
    }

    if (this->inputBuffer.size() + sanitized.size() > kMaxChatInputBytes)
    {
        return true;
    }

    const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
    this->cursorIndex = clampByteOffsetToUtf8Boundary(offsets, this->cursorIndex);
    this->inputBuffer.insert(this->cursorIndex, sanitized);
    this->cursorIndex += sanitized.size();
    this->clearInputSelection();
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    return true;
}

void ChatWidget::draw(void) const
{
    // Cast local non-const (aucune mutation logique de l'etat metier).
    ChatWidget* self = const_cast<ChatWidget*>(this);

    // Le widget recalcule son rect depuis le gameScreen + offsets de drag.
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        self->widgetWidth,
        self->widgetHeight
    };
    if (!self->visible)
    {
        return;
    }
    if (self->widgetRect.w < 100.0f || self->widgetRect.h < 100.0f)
    {
        return;
    }

    // Helpers de conversion reference -> ecran.
    const float extraW = self->widgetRect.w - kRefW;
    const float extraH = self->widgetRect.h - kRefH;
    const auto RX = [&](float x) { return self->widgetRect.x + x; };
    const auto RY = [&](float y) { return self->widgetRect.y + y; };
    const auto RW = [&](float w) { return w; };
    const auto RH = [&](float h) { return h; };

    // Zones de layout principales.
    const SDL_FRect inner = SDL_FRect{RX(5.0f), RY(5.0f), RW(492.0f) + extraW, RH(339.0f) + extraH};
    const SDL_FRect header = SDL_FRect{inner.x + RW(1.0f), inner.y + RH(2.0f), inner.w - RW(2.0f), RH(35.0f)};
    const SDL_FRect tabGlobal = SDL_FRect{RX(12.0f), RY(45.0f), RW(239.0f), RH(35.0f)};
    const SDL_FRect msgArea = SDL_FRect{RX(12.0f), RY(82.0f), RW(478.0f) + extraW, RH(214.0f) + extraH};
    const SDL_FRect inputArea = SDL_FRect{RX(12.0f), RY(298.0f) + extraH, RW(478.0f) + extraW, RH(39.0f)};
    const SDL_FRect closeButtonRect = SDL_FRect{
        self->widgetRect.x + self->widgetRect.w - RW(31.0f),
        header.y + ((header.h - RH(20.0f)) * 0.5f),
        RW(20.0f),
        RH(20.0f)
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - RW(24.0f),
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect resizeHandleRect = SDL_FRect{
        self->widgetRect.x + self->widgetRect.w - RW(16.0f),
        self->widgetRect.y + self->widgetRect.h - RH(16.0f),
        RW(14.0f),
        RH(14.0f)
    };

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // 1) Cadre principal.
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &self->widgetRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &self->widgetRect);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    // 2) Header.
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);
    drawLeftCenteredY(&self->titleFont, "Chat", header, header.x + RW(10.0f), kTextGold);

    // 3) Onglet.
    rc2d_graphics_setColor(kTabFill);
    rc2d_graphics_rectangle("fill", &tabGlobal);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &tabGlobal);
    drawCentered(&self->bodyFont, "Global", tabGlobal, kTextGold);

    // 4) Zone messages + clipping logique.
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &msgArea);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &msgArea);

    const SDL_FRect msgTextClip = SDL_FRect{
        msgArea.x + RW(6.0f),
        msgArea.y + RH(6.0f),
        msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
        msgArea.h - RH(6.0f * 2.0f)
    };

    const float lineHeight = measureTextHeight(&self->bodyFont) + RH(2.0f);
    const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(self->chatMessages, &self->bodyFont, msgTextClip.w);
    const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
    const int totalLines = static_cast<int>(wrappedLines.size());
    const int maxFirstLine = (std::max)(0, totalLines - visibleLines);
    const int firstLine = (std::max)(0, (std::min)(self->scrollFirstLine, maxFirstLine));

    // Dessin lignes visibles.
    for (int i = 0; i < visibleLines; ++i)
    {
        const int lineIndex = firstLine + i;
        if (lineIndex < 0 || lineIndex >= totalLines)
        {
            break;
        }

        const ChatWrappedLine& line = wrappedLines[static_cast<std::size_t>(lineIndex)];
        const float drawY = msgTextClip.y + (static_cast<float>(i) * lineHeight);

        // Coloration prefixe "avant ':'" en or.
        bool handledWithPrefixColor = false;
        if (line.messageIndex >= 0 &&
            line.messageIndex < static_cast<int>(self->chatMessages.size()))
        {
            const std::string& srcMessage = self->chatMessages[static_cast<std::size_t>(line.messageIndex)];
            const std::size_t colonPos = srcMessage.find(':');
            if (colonPos != std::string::npos && line.startIndex < colonPos)
            {
                const std::size_t goldEnd = (std::min)(colonPos, line.endIndex);
                const std::size_t goldLen = (goldEnd > line.startIndex) ? (goldEnd - line.startIndex) : 0;
                if (goldLen > 0)
                {
                    const std::string goldPart = line.text.substr(0, goldLen);
                    drawTextAt(&self->bodyFont, goldPart, msgTextClip.x, drawY, kTextGold);

                    const std::string restPart = line.text.substr(goldLen);
                    const float restX = msgTextClip.x + measureTextWidth(&self->bodyFont, goldPart);
                    drawTextAt(&self->bodyFont, restPart, restX, drawY, kMessageTextColor);
                    handledWithPrefixColor = true;
                }
            }
        }

        if (!handledWithPrefixColor)
        {
            drawTextAt(&self->bodyFont, line.text, msgTextClip.x, drawY, kMessageTextColor);
        }
    }

    // 5) Scrollbar.
    const SDL_FRect scrollTrack = SDL_FRect{
        msgArea.x + msgArea.w - RW(kScrollBarWidth + kScrollBarPadding),
        msgArea.y + RH(kScrollBarPadding),
        RW(kScrollBarWidth),
        msgArea.h - RH(kScrollBarPadding * 2.0f)
    };
    rc2d_graphics_setColor(kScrollTrackColor);
    rc2d_graphics_rectangle("fill", &scrollTrack);

    if (maxFirstLine > 0)
    {
        const float thumbHeight = (std::max)(RH(kMinThumbHeight), (scrollTrack.h * static_cast<float>(visibleLines) / static_cast<float>(totalLines)));
        const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);
        const float scrollRatio = static_cast<float>(firstLine) / static_cast<float>(maxFirstLine);
        const SDL_FRect scrollThumb = SDL_FRect{
            scrollTrack.x,
            scrollTrack.y + (thumbTravel * scrollRatio),
            scrollTrack.w,
            thumbHeight
        };
        rc2d_graphics_setColor(self->scrollBarDragging || (self->scrollBarWheelHighlightSec > 0.0f) ? kScrollThumbDragFill : kScrollThumbColor);
        rc2d_graphics_rectangle("fill", &scrollThumb);
    }

    // 6) Barre input + texte visible.
    rc2d_graphics_setColor(kInputFill);
    rc2d_graphics_rectangle("fill", &inputArea);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &inputArea);

    const float inputTextX = inputArea.x + RW(8.0f);
    const float inputTextY = inputArea.y + ((inputArea.h - lineHeight) * 0.5f);
    const float inputMaxWidth = inputArea.w - RW(16.0f);

    // Fenetre texte horizontale: on garde le curseur visible meme en saisie longue.
    std::size_t cursor = (std::min)(self->cursorIndex, self->inputBuffer.size());
    std::size_t renderStart = 0;
    std::size_t renderEnd = cursor;
    self->computeInputVisibleRange(inputMaxWidth, cursor, &renderStart, &renderEnd);

    const std::string visibleInput = self->inputBuffer.substr(renderStart, renderEnd - renderStart);
    const bool showPlaceholder = self->inputBuffer.empty() && !self->inputFocused;
    if (showPlaceholder)
    {
        // Placeholder uniquement hors focus pour guider l'utilisateur.
        drawTextAt(&self->bodyFont, "Tape ton message ici pour discuter…", inputTextX, inputTextY, kPlaceholderTextColor);
    }
    else
    {
        if (self->hasInputSelection())
        {
            const std::size_t selectionStart = self->getInputSelectionStart();
            const std::size_t selectionEnd = self->getInputSelectionEnd();
            const std::size_t visibleSelectionStart = (std::max)(renderStart, selectionStart);
            const std::size_t visibleSelectionEnd = (std::min)(renderEnd, selectionEnd);
            if (visibleSelectionStart < visibleSelectionEnd)
            {
                const std::string prefixText = self->inputBuffer.substr(renderStart, visibleSelectionStart - renderStart);
                const std::string selectedText = self->inputBuffer.substr(visibleSelectionStart, visibleSelectionEnd - visibleSelectionStart);
                const float prefixWidth = measureTextWidth(&self->bodyFont, prefixText);
                const float selectedWidth = measureTextWidth(&self->bodyFont, selectedText);
                const SDL_FRect selectionRect = SDL_FRect{
                    static_cast<float>(std::round(inputTextX + prefixWidth - RW(1.0f))),
                    static_cast<float>(std::round(inputArea.y + RH(7.0f))),
                    (std::max)(RW(2.0f), selectedWidth + RW(2.0f)),
                    inputArea.h - RH(14.0f)
                };
                rc2d_graphics_setColor(kInputSelectionFill);
                rc2d_graphics_rectangle("fill", &selectionRect);
            }
        }
        drawTextAt(&self->bodyFont, visibleInput, inputTextX, inputTextY, kMessageTextColor);
        if (self->hasInputSelection())
        {
            const std::size_t selectionStart = self->getInputSelectionStart();
            const std::size_t selectionEnd = self->getInputSelectionEnd();
            const std::size_t visibleSelectionStart = (std::max)(renderStart, selectionStart);
            const std::size_t visibleSelectionEnd = (std::min)(renderEnd, selectionEnd);
            if (visibleSelectionStart < visibleSelectionEnd)
            {
                const std::string prefixText = self->inputBuffer.substr(renderStart, visibleSelectionStart - renderStart);
                const std::string selectedText = self->inputBuffer.substr(visibleSelectionStart, visibleSelectionEnd - visibleSelectionStart);
                const float prefixWidth = measureTextWidth(&self->bodyFont, prefixText);
                drawTextAt(&self->bodyFont, selectedText, inputTextX + prefixWidth, inputTextY, kMessageTextColor);
            }
        }
    }

    // Curseur vertical clignotant.
    if (self->inputFocused && self->cursorVisible)
    {
        const std::string cursorPrefix = self->inputBuffer.substr(renderStart, cursor - renderStart);
        const float cursorX = inputTextX + measureTextWidth(&self->bodyFont, cursorPrefix);
        rc2d_graphics_setColor(kCursorColor);
        rc2d_graphics_line(cursorX, inputArea.y + RH(8.0f), cursorX, inputArea.y + inputArea.h - RH(8.0f));
    }

    // 7) Boutons cadenas / fermeture.
    self->controlIcons.drawLockButton(
        lockButtonRect,
        self->widgetDragLocked,
        self->widgetDragLocked ? kTabFill : kHeaderFill,
        kGold);
    self->controlIcons.drawCloseButton(closeButtonRect, kHeaderFill, kGold);

    // 9) Poignee bas-droite: on reutilise l'icone commune de scale.
    self->controlIcons.drawResizeHandle(resizeHandleRect, kPanelFill, kGold);

    // Restaure le mode blend par defaut.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void ChatWidget::clearFocus(void)
{
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
    this->clearInputSelection();
    this->syncPlatformTextInput();
}

void ChatWidget::computeInputVisibleRange(
    float inputMaxWidth,
    std::size_t focusIndex,
    std::size_t* outStart,
    std::size_t* outEnd) const
{
    if (outStart == nullptr || outEnd == nullptr)
    {
        return;
    }

    const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
    const std::size_t clampedFocusIndex = clampByteOffsetToUtf8Boundary(offsets, (std::min)(focusIndex, this->inputBuffer.size()));
    const std::size_t focusCodepointIndex = findCodepointIndexForByteOffset(offsets, clampedFocusIndex);

    std::size_t renderStart = 0U;
    std::size_t probeStartCodepointIndex = 0U;
    while (probeStartCodepointIndex < focusCodepointIndex)
    {
        const std::size_t candidateStart = offsets[probeStartCodepointIndex];
        if (measureTextWidth(&const_cast<ChatWidget*>(this)->bodyFont, this->inputBuffer.substr(candidateStart, clampedFocusIndex - candidateStart)) <= inputMaxWidth)
        {
            renderStart = candidateStart;
            break;
        }

        ++probeStartCodepointIndex;
    }
    if (probeStartCodepointIndex >= focusCodepointIndex)
    {
        renderStart = clampedFocusIndex;
    }

    std::size_t renderEnd = clampedFocusIndex;
    std::size_t renderEndCodepointIndex = focusCodepointIndex;
    while (renderEndCodepointIndex + 1U < offsets.size())
    {
        const std::size_t candidateEnd = offsets[renderEndCodepointIndex + 1U];
        if (measureTextWidth(&const_cast<ChatWidget*>(this)->bodyFont, this->inputBuffer.substr(renderStart, candidateEnd - renderStart)) > inputMaxWidth)
        {
            break;
        }

        ++renderEndCodepointIndex;
        renderEnd = candidateEnd;
    }

    *outStart = renderStart;
    *outEnd = renderEnd;
}

std::size_t ChatWidget::getInputCursorIndexFromPosition(float renderX, const SDL_FRect& inputArea) const
{
    const float inputTextX = inputArea.x + 8.0f;
    const float inputMaxWidth = inputArea.w - 16.0f;
    std::size_t renderStart = 0;
    std::size_t renderEnd = 0;
    this->computeInputVisibleRange(inputMaxWidth, this->cursorIndex, &renderStart, &renderEnd);

    const std::string visibleText = this->inputBuffer.substr(renderStart, renderEnd - renderStart);
    const std::vector<std::size_t> visibleOffsets = buildUtf8CodepointOffsets(visibleText);
    const float localX = clampf(renderX - inputTextX, 0.0f, inputMaxWidth);
    std::size_t newCursor = renderStart;
    float previousWidth = 0.0f;
    const std::size_t codepointCount = visibleOffsets.empty() ? 0U : (visibleOffsets.size() - 1U);
    for (std::size_t i = 0; i < codepointCount; ++i)
    {
        const float nextWidth = measureTextWidth(
            &const_cast<ChatWidget*>(this)->bodyFont,
            visibleText.substr(0U, visibleOffsets[i + 1U]));
        const float midpoint = previousWidth + ((nextWidth - previousWidth) * 0.5f);
        if (localX <= midpoint)
        {
            newCursor = renderStart + visibleOffsets[i];
            return (std::min)(newCursor, this->inputBuffer.size());
        }
        previousWidth = nextWidth;
        newCursor = renderStart + visibleOffsets[i + 1U];
    }

    return (std::min)(newCursor, this->inputBuffer.size());
}

bool ChatWidget::hasInputSelection(void) const
{
    return this->getInputSelectionStart() != this->getInputSelectionEnd();
}

std::size_t ChatWidget::getInputSelectionStart(void) const
{
    const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
    const std::size_t clampedCursor = clampByteOffsetToUtf8Boundary(offsets, (std::min)(this->cursorIndex, this->inputBuffer.size()));
    const std::size_t clampedAnchor = clampByteOffsetToUtf8Boundary(offsets, (std::min)(this->selectionAnchorIndex, this->inputBuffer.size()));
    return (std::min)(clampedAnchor, clampedCursor);
}

std::size_t ChatWidget::getInputSelectionEnd(void) const
{
    const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
    const std::size_t clampedCursor = clampByteOffsetToUtf8Boundary(offsets, (std::min)(this->cursorIndex, this->inputBuffer.size()));
    const std::size_t clampedAnchor = clampByteOffsetToUtf8Boundary(offsets, (std::min)(this->selectionAnchorIndex, this->inputBuffer.size()));
    return (std::max)(clampedAnchor, clampedCursor);
}

void ChatWidget::clearInputSelection(void)
{
    const std::vector<std::size_t> offsets = buildUtf8CodepointOffsets(this->inputBuffer);
    this->selectionAnchorIndex = clampByteOffsetToUtf8Boundary(
        offsets,
        (std::min)(this->cursorIndex, this->inputBuffer.size()));
}

void ChatWidget::deleteSelectedInputText(void)
{
    if (!this->hasInputSelection())
    {
        return;
    }

    const std::size_t selectionStart = this->getInputSelectionStart();
    const std::size_t selectionEnd = this->getInputSelectionEnd();
    this->inputBuffer.erase(selectionStart, selectionEnd - selectionStart);
    this->cursorIndex = selectionStart;
    this->clearInputSelection();
}

void ChatWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
    this->widgetResizing = false;
    this->scrollBarDragging = false;
    this->clearFocus();
}

void ChatWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->widgetResizing = false;
    this->scrollBarDragging = false;
    this->scrollBarWheelHighlightSec = 0.0f;
    this->clearFocus();
}

void ChatWidget::syncPlatformTextInput(void)
{
    rc2d_keyboard_setTextInput(this->visible && this->inputFocused);
}

bool ChatWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getChatWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType ChatWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->widgetResizing)
    {
        return HudCursorType::RESIZE_DIAGONAL;
    }
    if (this->scrollBarDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->widgetDragging && !this->widgetDragLocked)
    {
        return HudCursorType::MOVE;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const float extraW = this->widgetRect.w - kRefW;
    const float extraH = this->widgetRect.h - kRefH;
    const auto RX = [&](float xValue) { return this->widgetRect.x + xValue; };
    const auto RY = [&](float yValue) { return this->widgetRect.y + yValue; };
    const auto RW = [&](float wValue) { return wValue; };
    const auto RH = [&](float hValue) { return hValue; };

    const SDL_FRect msgArea = SDL_FRect{RX(12.0f), RY(82.0f), RW(478.0f) + extraW, RH(214.0f) + extraH};
    const SDL_FRect inputArea = SDL_FRect{RX(12.0f), RY(298.0f) + extraH, RW(478.0f) + extraW, RH(39.0f)};
    const SDL_FRect header = SDL_FRect{this->widgetRect.x + RW(6.0f), this->widgetRect.y + RH(7.0f), this->widgetRect.w - RW(10.0f), RH(35.0f)};
    const SDL_FRect closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - RW(31.0f),
        header.y + ((header.h - RH(20.0f)) * 0.5f),
        RW(20.0f),
        RH(20.0f)
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - RW(24.0f),
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect resizeHandleRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - RW(16.0f),
        this->widgetRect.y + this->widgetRect.h - RH(16.0f),
        RW(14.0f),
        RH(14.0f)
    };
    const SDL_FRect scrollTrack = SDL_FRect{
        msgArea.x + msgArea.w - RW(kScrollBarWidth + kScrollBarPadding),
        msgArea.y + RH(kScrollBarPadding),
        RW(kScrollBarWidth),
        msgArea.h - RH(kScrollBarPadding * 2.0f)
    };
    const SDL_FRect msgTextClip = SDL_FRect{
        msgArea.x + RW(6.0f),
        msgArea.y + RH(6.0f),
        msgArea.w - RW(6.0f * 2.0f) - RW(kScrollBarWidth + (kScrollBarPadding * 2.0f)),
        msgArea.h - RH(6.0f * 2.0f)
    };
    const float lineHeight = measureTextHeight(&const_cast<ChatWidget*>(this)->bodyFont) + RH(2.0f);
    const std::vector<ChatWrappedLine> wrappedLines = buildWrappedLines(
        this->chatMessages,
        &const_cast<ChatWidget*>(this)->bodyFont,
        msgTextClip.w);
    const int visibleLines = (std::max)(1, static_cast<int>(msgTextClip.h / lineHeight));
    const int maxFirstLine = (std::max)(0, static_cast<int>(wrappedLines.size()) - visibleLines);

    if (isPointInRect(x, y, resizeHandleRect))
    {
        return HudCursorType::RESIZE_DIAGONAL;
    }
    if (isPointInRect(x, y, inputArea))
    {
        return HudCursorType::TEXT;
    }
    if (isPointInRect(x, y, closeButtonRect) || isPointInRect(x, y, lockButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (maxFirstLine > 0 && isPointInRect(x, y, scrollTrack))
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (x >= lockButtonRect.x && x <= (closeButtonRect.x + closeButtonRect.w) &&
        y >= header.y && y <= (header.y + header.h))
    {
        return HudCursorType::DEFAULT;
    }
    if (isPointInRect(x, y, header) && !this->widgetDragLocked)
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}



