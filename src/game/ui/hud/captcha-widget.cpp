#include "game/ui/hud/captcha-widget.h"

#include "game/assets/title-asset-cache.h"
#include "game/ui/text-input-shortcuts.h"

#include "core/context.h"

#include <RC2D/RC2D_system.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

static constexpr float kRefW = 416.0f;
/** Hauteur du panneau : marge sous Valider pour le chrono et la phrase anti-bot. */
static constexpr float kRefH = 273.0f;

static constexpr float kPanelEdgeGap = 14.0f;  /**< Espace sous le bandeau titre et sous le bouton Valider. */
static constexpr float kPanelMidGap = 12.0f;   /**< Espace entre les blocs Defi / reponse / Valider. */
static constexpr float kLabelBandH = 18.0f;
static constexpr float kLabelToFieldGap = 4.0f;
static constexpr float kChallengeFieldH = 40.0f;
static constexpr float kResponseInputH = 34.0f;
static constexpr float kValidateButtonH = 40.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 255};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kButtonHoverFill = RC2D_Color{27, 18, 8, 242};
static constexpr RC2D_Color kInputSelectionFill = RC2D_Color{67, 96, 144, 215};
static constexpr RC2D_Color kTitleAccent = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTimerWarnColor = RC2D_Color{255, 120, 90, 255};
static constexpr double kCursorBlinkPeriod = 0.55;
/** Delai maximum pour repondre au defi (secondes), affiche en compte a rebours. */
static constexpr double kCaptchaSolveDurationSec = 180.0;
/** Seuil d'affichage en couleur d'alerte pour le chrono (secondes). */
static constexpr double kCaptchaTimerWarnBelowSec = 30.0;

/** Texte a gauche du compte a rebours (contexte anti-bot). */
static const char kCaptchaTimerExplanation[] =
    "Anti-bot : delai pour confirmer une action humaine.";

static SDL_FRect getCaptchaRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH};
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

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

static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& r, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    const float drawX = std::round(r.x + ((r.w - static_cast<float>(w)) * 0.5f));
    const float drawY = std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

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

static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& r, float x, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    (void)w;
    const float drawX = std::round(x);
    const float drawY = std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static void drawTextRightAlignedTop(
    RC2D_Font* font,
    const char* text,
    float rightX,
    float topY,
    RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    (void)h;
    const float drawX = std::round(rightX - static_cast<float>(w));
    const float drawY = std::round(topY);
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static void drawTextLeftTop(RC2D_Font* font, const char* text, float leftX, float topY, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    (void)w;
    const float drawX = std::round(leftX);
    const float drawY = std::round(topY);
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

struct CaptchaPanelLayout {
    SDL_FRect header;
    SDL_FRect challengeLabel;
    SDL_FRect challengeField;
    SDL_FRect responseLabel;
    SDL_FRect responseInput;
    SDL_FRect validateButton;
};

/**
 * @brief Calcule les rectangles : bandeau titre, blocs Defi serveur, reponse, Valider.
 *
 * Les marges verticales @ref kPanelEdgeGap (entre le titre et le bloc Defi, et sous Valider)
 * sont egales ; @ref kPanelMidGap separe les trois zones principales.
 */
static void buildCaptchaPanelLayout(const SDL_FRect& outer, CaptchaPanelLayout* out)
{
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    out->header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};

    const float padX = 10.0f;
    const float contentW = outer.w - (padX * 2.0f);
    float y = out->header.y + out->header.h + kPanelEdgeGap;

    out->challengeLabel = SDL_FRect{outer.x + padX, y, contentW, kLabelBandH};
    y += kLabelBandH + kLabelToFieldGap;
    out->challengeField = SDL_FRect{outer.x + padX, y, contentW, kChallengeFieldH};
    y += kChallengeFieldH + kPanelMidGap;

    out->responseLabel = SDL_FRect{outer.x + padX, y, contentW, kLabelBandH};
    y += kLabelBandH + kLabelToFieldGap;
    out->responseInput = SDL_FRect{outer.x + padX, y, contentW, kResponseInputH};
    y += kResponseInputH + kPanelMidGap;

    out->validateButton = SDL_FRect{outer.x + padX, y, contentW, kValidateButtonH};
}

std::string CaptchaWidget::sanitizeChallengePayload(const std::string& raw)
{
    std::string out;
    out.reserve((std::min)(raw.size(), CaptchaWidget::kMaxChallengeCharacters));
    for (unsigned char uc : raw)
    {
        if (out.size() >= CaptchaWidget::kMaxChallengeCharacters)
        {
            break;
        }
        if (std::isalnum(uc) == 0)
        {
            continue;
        }
        out.push_back(static_cast<char>(std::toupper(uc)));
    }
    return out;
}

char CaptchaWidget::extractInputCharacterFromKeyLabel(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    if (key[1] == '\0')
    {
        const unsigned char character = static_cast<unsigned char>(key[0]);
        if (std::isalnum(character) != 0)
        {
            return static_cast<char>(std::toupper(character));
        }
        return '\0';
    }

    std::string lower(key);
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool looksLikeKeypad = (lower.find("kp") != std::string::npos) ||
                                 (lower.find("keypad") != std::string::npos) ||
                                 (lower.find("numpad") != std::string::npos);
    if (!looksLikeKeypad)
    {
        return '\0';
    }

    for (const char c : lower)
    {
        if (c >= '0' && c <= '9')
        {
            return c;
        }
    }

    return '\0';
}

CaptchaWidget::CaptchaWidget(void)
    : titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(false),
      cursorEnabled(true),
      resourcesLoaded(false),
      challengeDisplay{},
      userResponseInput{},
      cursorIndex(0U),
      selectionAnchorIndex(0U),
      inputFocused(false),
      inputSelectingWithMouse(false),
      cursorVisible(true),
      cursorBlinkElapsed(0.0),
      solveTimeRemainingSec(0.0),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      widgetDragging(false),
      controlIcons{},
      onValidateRequested{}
{
}

CaptchaWidget::~CaptchaWidget(void)
{
}

void CaptchaWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->controlIcons.load();
    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h};
    this->visible = false;
    this->challengeDisplay.clear();
    this->userResponseInput.clear();
    this->cursorIndex = 0U;
    this->selectionAnchorIndex = 0U;
    this->responseEditHistory.clear();
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->widgetDragging = false;
    this->solveTimeRemainingSec = 0.0;
    this->resourcesLoaded = true;
}

void CaptchaWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
    this->responseEditHistory.clear();
    this->resourcesLoaded = false;
}

void CaptchaWidget::publishCaptchaChallenge(const std::string& challengeFromServer)
{
    this->challengeDisplay = sanitizeChallengePayload(challengeFromServer);
    this->userResponseInput.clear();
    this->cursorIndex = 0U;
    this->selectionAnchorIndex = 0U;
    this->responseEditHistory.clear();
    this->clearResponseSelection();
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->show();
}

void CaptchaWidget::setOnValidateRequested(const std::function<void(const std::string&)>& callback)
{
    this->onValidateRequested = callback;
}

void CaptchaWidget::show(void)
{
    this->visible = true;
    this->solveTimeRemainingSec = kCaptchaSolveDurationSec;
}

void CaptchaWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->responseEditHistory.clear();
    this->clearFocus();
}

void CaptchaWidget::update(double dt)
{
    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h};

    if (this->visible && this->solveTimeRemainingSec > 0.0)
    {
        this->solveTimeRemainingSec -= dt;
        if (this->solveTimeRemainingSec <= 0.0)
        {
            this->solveTimeRemainingSec = 0.0;
            this->hide();
        }
    }

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

    CaptchaPanelLayout layout{};
    buildCaptchaPanelLayout(this->widgetRect, &layout);

    if (this->inputSelectingWithMouse)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->inputSelectingWithMouse = false;
        }
        else if (this->inputFocused)
        {
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            (void)mouseY;
            this->cursorIndex = this->getResponseCursorIndexFromPosition(mouseX, layout.responseInput);
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
        }
    }

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
}

bool CaptchaWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h};
    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    CaptchaPanelLayout layout{};
    buildCaptchaPanelLayout(this->widgetRect, &layout);

    const SDL_FRect headerDrag = SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 28.0f,
        layout.header.y + ((layout.header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f};

    if (isPointInRect(x, y, closeButtonRect))
    {
        this->hide();
        return true;
    }

    if (isPointInRect(x, y, layout.responseInput))
    {
        this->inputFocused = true;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;

        if (clicks >= 2)
        {
            this->selectionAnchorIndex = 0U;
            this->cursorIndex = this->userResponseInput.size();
            this->inputSelectingWithMouse = false;
            this->widgetDragging = false;
            return true;
        }

        const std::size_t clickedIndex = this->getResponseCursorIndexFromPosition(x, layout.responseInput);
        this->cursorIndex = clickedIndex;
        this->selectionAnchorIndex = clickedIndex;
        this->inputSelectingWithMouse = true;
        this->widgetDragging = false;
        return true;
    }

    if (isPointInRect(x, y, layout.validateButton))
    {
        this->widgetDragging = false;
        this->clearFocus();
        this->triggerValidateRequest();
        return true;
    }

    if (isPointInRect(x, y, headerDrag))
    {
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearFocus();
    }
    else
    {
        this->clearFocus();
    }
    return true;
}

bool CaptchaWidget::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    if (!this->visible || !this->inputFocused)
    {
        return false;
    }

    (void)isrepeat;

    switch (keycode)
    {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            this->triggerValidateRequest();
            return true;
        case SDLK_ESCAPE:
            this->clearFocus();
            return true;
        default:
            break;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::UNDO, key, scancode, keycode, mod, isrepeat))
    {
        if (this->responseEditHistory.undo(
                &this->userResponseInput,
                &this->cursorIndex,
                &this->selectionAnchorIndex))
        {
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::SELECT_ALL, key, scancode, keycode, mod, isrepeat))
    {
        this->selectionAnchorIndex = 0U;
        this->cursorIndex = this->userResponseInput.size();
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::COPY, key, scancode, keycode, mod, isrepeat))
    {
        if (this->hasResponseSelection())
        {
            const std::size_t selectionStart = this->getResponseSelectionStart();
            const std::size_t selectionEnd = this->getResponseSelectionEnd();
            rc2d_system_setClipboardText(
                this->userResponseInput.substr(selectionStart, selectionEnd - selectionStart).c_str());
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::PASTE, key, scancode, keycode, mod, isrepeat))
    {
        char* clipboardText = rc2d_system_getClipboardText();
        if (clipboardText != nullptr)
        {
            std::string sanitized;
            sanitized.reserve(CaptchaWidget::kMaxUserResponseCharacters);
            for (const char* cursor = clipboardText; *cursor != '\0'; ++cursor)
            {
                const unsigned char uc = static_cast<unsigned char>(*cursor);
                if (std::isalnum(uc) != 0)
                {
                    sanitized.push_back(static_cast<char>(std::toupper(uc)));
                }
            }

            if (!sanitized.empty())
            {
                this->responseEditHistory.rememberState(
                    this->userResponseInput,
                    this->cursorIndex,
                    this->selectionAnchorIndex);
                if (this->hasResponseSelection())
                {
                    this->deleteSelectedResponseText();
                }

                const std::size_t availableCount =
                    CaptchaWidget::kMaxUserResponseCharacters - this->userResponseInput.size();
                sanitized.resize((std::min)(sanitized.size(), availableCount));
                this->cursorIndex = (std::min)(this->cursorIndex, this->userResponseInput.size());
                this->userResponseInput.insert(this->cursorIndex, sanitized);
                this->cursorIndex += sanitized.size();
                this->clearResponseSelection();
                this->cursorVisible = true;
                this->cursorBlinkElapsed = 0.0;
            }

            rc2d_system_freeClipboardText(clipboardText);
        }
        return true;
    }

    switch (scancode)
    {
        case SDL_SCANCODE_LEFT:
            if (this->hasResponseSelection())
            {
                this->cursorIndex = this->getResponseSelectionStart();
            }
            else if (this->cursorIndex > 0U)
            {
                --this->cursorIndex;
            }
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RIGHT:
            if (this->hasResponseSelection())
            {
                this->cursorIndex = this->getResponseSelectionEnd();
            }
            else if (this->cursorIndex < this->userResponseInput.size())
            {
                ++this->cursorIndex;
            }
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_HOME:
            this->cursorIndex = 0U;
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_END:
            this->cursorIndex = this->userResponseInput.size();
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_BACKSPACE:
            if (this->hasResponseSelection())
            {
                this->responseEditHistory.rememberState(
                    this->userResponseInput,
                    this->cursorIndex,
                    this->selectionAnchorIndex);
                this->deleteSelectedResponseText();
            }
            else if (this->cursorIndex > 0U && !this->userResponseInput.empty())
            {
                this->responseEditHistory.rememberState(
                    this->userResponseInput,
                    this->cursorIndex,
                    this->selectionAnchorIndex);
                this->userResponseInput.erase(this->cursorIndex - 1U, 1U);
                --this->cursorIndex;
            }
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_DELETE:
            if (this->hasResponseSelection())
            {
                this->responseEditHistory.rememberState(
                    this->userResponseInput,
                    this->cursorIndex,
                    this->selectionAnchorIndex);
                this->deleteSelectedResponseText();
            }
            else if (this->cursorIndex < this->userResponseInput.size())
            {
                this->responseEditHistory.rememberState(
                    this->userResponseInput,
                    this->cursorIndex,
                    this->selectionAnchorIndex);
                this->userResponseInput.erase(this->cursorIndex, 1U);
            }
            this->clearResponseSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        default:
            break;
    }

    return false;

    // Raccourcis Ctrl : pas d'insertion (AltGr est pris en charge via mod dans SDL_GetKeyFromScancode).
    if ((mod & SDL_KMOD_CTRL) != 0)
    {
        return false;
    }

    char asciiChar = '\0';

    // 1) Libelle evenement RC2D/SDL : reflete le caractere imprime si fourni en ASCII.
    if (key != nullptr && key[0] != '\0' && key[1] == '\0')
    {
        const unsigned char uc = static_cast<unsigned char>(key[0]);
        if (std::isalnum(uc) != 0)
        {
            asciiChar = static_cast<char>(std::toupper(uc));
        }
    }

    // 2) Symbole dependant de la disposition (AZERTY, QWERTY, QWERTZ...) via SDL.
    if (asciiChar == '\0')
    {
        const SDL_Keycode layoutKey = SDL_GetKeyFromScancode(scancode, mod, true);
        if (layoutKey > 0 && layoutKey < 128)
        {
            const unsigned char uc = static_cast<unsigned char>(layoutKey);
            if (std::isalnum(uc) != 0)
            {
                asciiChar = static_cast<char>(std::toupper(uc));
            }
        }
    }

    // 3) Pavé numerique via libelle "KP 7" etc.
    if (asciiChar == '\0')
    {
        asciiChar = extractInputCharacterFromKeyLabel(key);
    }

    if (asciiChar == '\0')
    {
        return false;
    }

    if (this->hasResponseSelection())
    {
        this->responseEditHistory.rememberState(
            this->userResponseInput,
            this->cursorIndex,
            this->selectionAnchorIndex);
        this->deleteSelectedResponseText();
    }
    else if (this->userResponseInput.size() < CaptchaWidget::kMaxUserResponseCharacters)
    {
        this->responseEditHistory.rememberState(
            this->userResponseInput,
            this->cursorIndex,
            this->selectionAnchorIndex);
    }
    if (this->userResponseInput.size() >= CaptchaWidget::kMaxUserResponseCharacters)
    {
        return true;
    }
    this->cursorIndex = (std::min)(this->cursorIndex, this->userResponseInput.size());
    this->userResponseInput.insert(this->cursorIndex, 1, asciiChar);
    ++this->cursorIndex;
    this->clearResponseSelection();
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    return true;
}

bool CaptchaWidget::textinput(const char* text)
{
    if (!this->visible || !this->inputFocused)
    {
        return false;
    }

    std::string sanitized;
    sanitized.reserve(CaptchaWidget::kMaxUserResponseCharacters);
    for (const char* cursor = text; cursor != nullptr && *cursor != '\0'; ++cursor)
    {
        const unsigned char uc = static_cast<unsigned char>(*cursor);
        if (std::isalnum(uc) != 0)
        {
            sanitized.push_back(static_cast<char>(std::toupper(uc)));
        }
    }

    if (sanitized.empty())
    {
        return true;
    }

    this->responseEditHistory.rememberState(
        this->userResponseInput,
        this->cursorIndex,
        this->selectionAnchorIndex);
    if (this->hasResponseSelection())
    {
        this->deleteSelectedResponseText();
    }

    const std::size_t availableCount =
        CaptchaWidget::kMaxUserResponseCharacters - this->userResponseInput.size();
    sanitized.resize((std::min)(sanitized.size(), availableCount));
    this->cursorIndex = (std::min)(this->cursorIndex, this->userResponseInput.size());
    this->userResponseInput.insert(this->cursorIndex, sanitized);
    this->cursorIndex += sanitized.size();
    this->clearResponseSelection();
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    return true;
}

void CaptchaWidget::clearFocus(void)
{
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
    this->clearResponseSelection();
}

void CaptchaWidget::draw(void) const
{
    CaptchaWidget* self = const_cast<CaptchaWidget*>(this);
    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h};

    if (!self->visible || !self->resourcesLoaded)
    {
        return;
    }

    CaptchaPanelLayout layout{};
    buildCaptchaPanelLayout(self->widgetRect, &layout);
    const SDL_FRect& header = layout.header;
    const SDL_FRect& challengeField = layout.challengeField;
    const SDL_FRect& responseInput = layout.responseInput;
    const SDL_FRect& validateButton = layout.validateButton;

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    const bool validateHovered = isPointInRect(mouseX, mouseY, validateButton);

    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect closeButton = SDL_FRect{outer.x + outer.w - 28.0f, outer.y + 8.0f, 20.0f, 20.0f};
    const SDL_FRect closeButtonCentered = SDL_FRect{
        closeButton.x,
        header.y + ((header.h - closeButton.h) * 0.5f),
        closeButton.w,
        closeButton.h};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);

    RC2D_Text titleText = rc2d_graphics_createText(&self->titleFont, "Verification Captcha");
    titleText.color = kTitleAccent;
    rc2d_graphics_setTextColor(&titleText);
    int titleW = 0;
    int titleH = 0;
    rc2d_graphics_getTextSize(&titleText, &titleW, &titleH);
    const float titleX = std::round(header.x + 10.0f);
    const float titleY = std::round(header.y + ((header.h - static_cast<float>(titleH)) * 0.5f) + 1.0f);
    rc2d_graphics_drawText(&titleText, titleX, titleY);
    rc2d_graphics_destroyText(&titleText);

    self->controlIcons.drawCloseButton(closeButtonCentered, kHeaderFill, kGold);

    drawLeftCenteredY(&self->bodyFont, "Defi serveur :", layout.challengeLabel, layout.challengeLabel.x, kTextWhite);
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &challengeField);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &challengeField);

    const char* challengeText = self->challengeDisplay.empty() ? "-" : self->challengeDisplay.c_str();
    drawCentered(&self->titleFont, challengeText, challengeField, kTextGold);

    drawLeftCenteredY(&self->bodyFont, "Votre reponse :", layout.responseLabel, layout.responseLabel.x, kTextWhite);
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &responseInput);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &responseInput);

    if (self->hasResponseSelection())
    {
        const std::size_t selectionStart = self->getResponseSelectionStart();
        const std::size_t selectionEnd = self->getResponseSelectionEnd();
        const std::string beforeSelection = self->userResponseInput.substr(0, selectionStart);
        const std::string selectedText = self->userResponseInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(responseInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        const float selectionW = measureTextWidth(&self->bodyFont, selectedText);
        const SDL_FRect selectionRect = SDL_FRect{
            selectionX - 1.0f,
            responseInput.y + 5.0f,
            (std::max)(2.0f, selectionW + 2.0f),
            responseInput.h - 10.0f};
        rc2d_graphics_setColor(kInputSelectionFill);
        rc2d_graphics_rectangle("fill", &selectionRect);
    }
    drawLeftCenteredY(&self->bodyFont, self->userResponseInput.c_str(), responseInput, responseInput.x + 8.0f, kTextGold);
    if (self->hasResponseSelection())
    {
        const std::size_t selectionStart = self->getResponseSelectionStart();
        const std::size_t selectionEnd = self->getResponseSelectionEnd();
        const std::string beforeSelection = self->userResponseInput.substr(0, selectionStart);
        const std::string selectedText = self->userResponseInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(responseInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        drawLeftCenteredY(&self->bodyFont, selectedText.c_str(), responseInput, selectionX, kTextWhite);
    }
    if (self->inputFocused && self->cursorVisible)
    {
        const std::size_t cursor = (std::min)(self->cursorIndex, self->userResponseInput.size());
        const std::string prefix = self->userResponseInput.substr(0, cursor);
        const float cursorX = std::round(responseInput.x + 8.0f + measureTextWidth(&self->bodyFont, prefix));
        const float cursorTop = std::round(responseInput.y + 6.0f);
        const float cursorBottom = std::round(responseInput.y + responseInput.h - 6.0f);
        rc2d_graphics_setColor(kTextGold);
        rc2d_graphics_line(cursorX, cursorTop, cursorX, cursorBottom);
    }

    rc2d_graphics_setColor(validateHovered ? kButtonHoverFill : kPanelFill);
    rc2d_graphics_rectangle("fill", &validateButton);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &validateButton);
    drawCentered(&self->bodyFont, "Valider", validateButton, kTextWhite);

    char timerBuf[8];
    const int displaySec = static_cast<int>(std::ceil((std::max)(0.0, self->solveTimeRemainingSec)));
    const int mm = displaySec / 60;
    const int ss = displaySec % 60;
    std::snprintf(timerBuf, sizeof(timerBuf), "%02d:%02d", mm, ss);
    const RC2D_Color timerColor =
        (self->solveTimeRemainingSec > 0.0 && self->solveTimeRemainingSec <= kCaptchaTimerWarnBelowSec) ? kTimerWarnColor
                                                                                                          : kTextGold;
    constexpr float kTimerRightMargin = 10.0f;
    constexpr float kTimerBottomMargin = 8.0f;
    constexpr float kTimerPhraseGap = 8.0f;
    constexpr float kTimerLinePadX = 10.0f;
    RC2D_Text timerMeasure = rc2d_graphics_createText(&self->bodyFont, timerBuf);
    int timerH = 0;
    int timerW = 0;
    rc2d_graphics_getTextSize(&timerMeasure, &timerW, &timerH);
    rc2d_graphics_destroyText(&timerMeasure);
    const float timerTop = outer.y + outer.h - kTimerBottomMargin - static_cast<float>(timerH);
    const float timerDrawRight = outer.x + outer.w - kTimerRightMargin;
    const float timerDrawLeft = timerDrawRight - static_cast<float>(timerW);

    RC2D_Text phraseMeasure = rc2d_graphics_createText(&self->bodyFont, kCaptchaTimerExplanation);
    int phraseW = 0;
    int phraseH = 0;
    rc2d_graphics_getTextSize(&phraseMeasure, &phraseW, &phraseH);
    rc2d_graphics_destroyText(&phraseMeasure);
    (void)phraseH;
    float phraseLeft = timerDrawLeft - kTimerPhraseGap - static_cast<float>(phraseW);
    const float phraseMinX = outer.x + kTimerLinePadX;
    if (phraseLeft < phraseMinX)
    {
        phraseLeft = phraseMinX;
    }

    drawTextLeftTop(&self->bodyFont, kCaptchaTimerExplanation, phraseLeft, timerTop, kTextWhite);
    drawTextRightAlignedTop(
        &self->bodyFont,
        timerBuf,
        outer.x + outer.w - kTimerRightMargin,
        timerTop,
        timerColor);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool CaptchaWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h};
    return isPointInRect(x, y, currentRect);
}

HudCursorType CaptchaWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible || !this->cursorEnabled)
    {
        return HudCursorType::NONE;
    }

    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const SDL_FRect baseRect = getCaptchaRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h};

    CaptchaPanelLayout layout{};
    buildCaptchaPanelLayout(currentRect, &layout);
    const SDL_FRect& header = layout.header;

    const SDL_FRect closeButtonRect = SDL_FRect{
        currentRect.x + currentRect.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f};

    if (isPointInRect(x, y, layout.challengeLabel) || isPointInRect(x, y, layout.challengeField))
    {
        return HudCursorType::DEFAULT;
    }
    if (isPointInRect(x, y, layout.responseLabel))
    {
        return HudCursorType::DEFAULT;
    }
    if (isPointInRect(x, y, layout.responseInput))
    {
        return HudCursorType::TEXT;
    }
    if (isPointInRect(x, y, layout.validateButton) || isPointInRect(x, y, closeButtonRect))
    {
        return HudCursorType::POINTER;
    }

    const SDL_FRect headerDrag = SDL_FRect{
        currentRect.x + 5.0f,
        currentRect.y + 5.0f,
        currentRect.w - 10.0f,
        30.0f};
    if (isPointInRect(x, y, headerDrag))
    {
        return HudCursorType::MOVE;
    }

    return HudCursorType::DEFAULT;
}

std::size_t CaptchaWidget::getResponseCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const
{
    const float inputTextX = inputRect.x + 8.0f;
    const float localX = (std::max)(0.0f, renderX - inputTextX);
    std::size_t newCursor = 0U;
    for (std::size_t i = 0; i <= this->userResponseInput.size(); ++i)
    {
        const float width = measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), this->userResponseInput.substr(0, i));
        if (localX <= width)
        {
            newCursor = i;
            break;
        }
        newCursor = i;
    }
    return (std::min)(newCursor, this->userResponseInput.size());
}

bool CaptchaWidget::hasResponseSelection(void) const
{
    return this->getResponseSelectionStart() != this->getResponseSelectionEnd();
}

std::size_t CaptchaWidget::getResponseSelectionStart(void) const
{
    const std::size_t clampedCursor = (std::min)(this->cursorIndex, this->userResponseInput.size());
    const std::size_t clampedAnchor = (std::min)(this->selectionAnchorIndex, this->userResponseInput.size());
    return (std::min)(clampedAnchor, clampedCursor);
}

std::size_t CaptchaWidget::getResponseSelectionEnd(void) const
{
    const std::size_t clampedCursor = (std::min)(this->cursorIndex, this->userResponseInput.size());
    const std::size_t clampedAnchor = (std::min)(this->selectionAnchorIndex, this->userResponseInput.size());
    return (std::max)(clampedAnchor, clampedCursor);
}

void CaptchaWidget::clearResponseSelection(void)
{
    this->selectionAnchorIndex = (std::min)(this->cursorIndex, this->userResponseInput.size());
}

void CaptchaWidget::deleteSelectedResponseText(void)
{
    if (!this->hasResponseSelection())
    {
        return;
    }

    const std::size_t selectionStart = this->getResponseSelectionStart();
    const std::size_t selectionEnd = this->getResponseSelectionEnd();
    this->userResponseInput.erase(selectionStart, selectionEnd - selectionStart);
    this->cursorIndex = selectionStart;
    this->clearResponseSelection();
}

void CaptchaWidget::triggerValidateRequest(void)
{
    if (this->onValidateRequested)
    {
        this->onValidateRequested(this->userResponseInput);
    }
    this->hide();
}
