#include "game/ui/hud/chat-widget.h"
#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cctype>
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
static constexpr RC2D_Color kMessageTextColor = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kPlaceholderTextColor = RC2D_Color{145, 152, 166, 235};
static constexpr RC2D_Color kCursorColor = RC2D_Color{239, 226, 163, 255};
static constexpr RC2D_Color kScrollTrackColor = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumbColor = RC2D_Color{124, 132, 142, 240};

static constexpr float kCursorBlinkPeriod = 0.55f;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;

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

ChatWidget::ChatWidget(void)
    : chatMessages{},
      titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      inputBuffer{},
      cursorIndex(0),
      inputFocused(false),
      cursorVisible(true),
      cursorBlinkElapsed(0.0),
      scrollFirstLine(0),
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
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
    this->inputFocused = false;
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
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

    // 3) Messages systeme initiaux propres au widget.
    this->publishChatMessage(ChatWidget::ChatMessageAuthor::SYSTEM, "Bienvenue dans le chat du serveur ! Sois respectueux et amuse-toi ! On surveille...");
}

void ChatWidget::unload(void)
{
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
        this->inputFocused = false;
        this->cursorVisible = false;
        this->scrollBarDragging = false;
        this->widgetResizing = false;
        return;
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
    (void)clicks;
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
        this->inputFocused = false;
        this->cursorVisible = false;
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
        this->inputFocused = false;
        this->cursorVisible = false;
        return true;
    }

    // Croix: ferme le chat.
    if (isPointInRect(x, y, closeButtonRect))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        this->inputFocused = false;
        this->cursorVisible = false;
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
        this->inputFocused = false;
        this->cursorVisible = false;
        return true;
    }

    // --- Clic dans la barre input ---
    if (isPointInRect(x, y, inputArea))
    {
        this->inputFocused = true;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;

        // Place le curseur au plus proche du clic.
        const float inputTextX = inputArea.x + RW(8.0f);
        const float inputMaxWidth = inputArea.w - RW(16.0f);

        std::size_t renderStart = 0;
        while (renderStart < this->cursorIndex &&
               measureTextWidth(&this->bodyFont, this->inputBuffer.substr(renderStart, this->cursorIndex - renderStart)) > inputMaxWidth)
        {
            ++renderStart;
        }

        std::size_t renderEnd = this->cursorIndex;
        while (renderEnd < this->inputBuffer.size())
        {
            const std::string candidate = this->inputBuffer.substr(renderStart, (renderEnd - renderStart) + 1);
            if (measureTextWidth(&this->bodyFont, candidate) > inputMaxWidth)
            {
                break;
            }
            ++renderEnd;
        }

        const std::string visibleText = this->inputBuffer.substr(renderStart, renderEnd - renderStart);
        const float localX = (std::max)(0.0f, x - inputTextX);
        std::size_t newCursor = renderStart;
        float accumWidth = 0.0f;
        for (std::size_t i = 0; i < visibleText.size(); ++i)
        {
            accumWidth = measureTextWidth(&this->bodyFont, visibleText.substr(0, i + 1));
            if (localX <= accumWidth)
            {
                newCursor = renderStart + i + 1;
                break;
            }
            newCursor = renderStart + i + 1;
        }

        this->cursorIndex = (std::min)(newCursor, this->inputBuffer.size());
        return true;
    }

    // Clic ailleurs dans le widget => retire focus input.
    this->inputFocused = false;
    this->cursorVisible = false;

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
        this->scrollFirstLine -= delta;
        this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
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
            if (this->cursorIndex > 0) { --this->cursorIndex; }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_RIGHT:
            if (this->cursorIndex < this->inputBuffer.size()) { ++this->cursorIndex; }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_HOME:
            this->cursorIndex = 0;
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_END:
            this->cursorIndex = this->inputBuffer.size();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_BACKSPACE:
            if (this->cursorIndex > 0 && !this->inputBuffer.empty())
            {
                this->inputBuffer.erase(this->cursorIndex - 1, 1);
                --this->cursorIndex;
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_DELETE:
            if (this->cursorIndex < this->inputBuffer.size())
            {
                this->inputBuffer.erase(this->cursorIndex, 1);
            }
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
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;

        case SDL_SCANCODE_ESCAPE:
            this->inputFocused = false;
            this->cursorVisible = false;
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

    // Insertion caractere imprimable.
    char newCharacter = '\0';
    if (keyToPrintableChar(key, scancode, keycode, mod, &newCharacter))
    {
        this->inputBuffer.insert(this->cursorIndex, 1, newCharacter);
        ++this->cursorIndex;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    return false;
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
        rc2d_graphics_setColor(kScrollThumbColor);
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
    while (renderStart < cursor &&
           measureTextWidth(&self->bodyFont, self->inputBuffer.substr(renderStart, cursor - renderStart)) > inputMaxWidth)
    {
        ++renderStart;
    }

    std::size_t renderEnd = cursor;
    while (renderEnd < self->inputBuffer.size())
    {
        const std::string candidate = self->inputBuffer.substr(renderStart, (renderEnd - renderStart) + 1);
        if (measureTextWidth(&self->bodyFont, candidate) > inputMaxWidth)
        {
            break;
        }
        ++renderEnd;
    }

    const std::string visibleInput = self->inputBuffer.substr(renderStart, renderEnd - renderStart);
    const bool showPlaceholder = self->inputBuffer.empty() && !self->inputFocused;
    if (showPlaceholder)
    {
        // Placeholder uniquement hors focus pour guider l'utilisateur.
        drawTextAt(&self->bodyFont, "Tape ton message ici pour discuter…", inputTextX, inputTextY, kPlaceholderTextColor);
    }
    else
    {
        drawTextAt(&self->bodyFont, visibleInput, inputTextX, inputTextY, kMessageTextColor);
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
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
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
    this->clearFocus();
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



