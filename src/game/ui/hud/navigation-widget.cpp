#include "game/ui/hud/navigation-widget.h"

#include "core/context.h"
#include "game/assets/title-asset-cache.h"
#include "game/map/map.h"
#include "game/ui/ingame-hud-overlay.h"
#include "game/ui/text-input-shortcuts.h"

#include <RC2D/RC2D_system.h>

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kWidgetWidth = 178.0f;
constexpr float kWidgetHeight = 126.0f;
constexpr double kCursorBlinkPeriod = 0.55;
constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 255};
constexpr RC2D_Color kPanelBorder = RC2D_Color{184, 132, 30, 250};
constexpr RC2D_Color kInnerBorder = RC2D_Color{211, 214, 220, 232};
constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
constexpr RC2D_Color kFieldBorder = RC2D_Color{184, 132, 30, 230};
constexpr RC2D_Color kFieldFocusedBorder = RC2D_Color{211, 214, 220, 240};
constexpr RC2D_Color kButtonFill = RC2D_Color{12, 12, 14, 235};
constexpr RC2D_Color kButtonHoverFill = RC2D_Color{27, 18, 8, 242};
constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};
constexpr RC2D_Color kTextMuted = RC2D_Color{124, 109, 84, 255};
constexpr RC2D_Color kSelectionFill = RC2D_Color{67, 96, 144, 215};
constexpr RC2D_Color kStatusError = RC2D_Color{255, 118, 96, 255};

bool pointInRect(float x, float y, const SDL_FRect& rect)
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

void getMouseRenderPosition(float* outX, float* outY)
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

float measureTextWidth(RC2D_Font* font, const std::string& text)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)height;
    return static_cast<float>(width);
}

float measureTextHeight(RC2D_Font* font, const char* text)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return 0.0f;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_destroyText(&textObject);
    (void)width;
    return static_cast<float>(height);
}

void drawTextLeft(RC2D_Font* font, const char* text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    rc2d_graphics_drawText(&textObject, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&textObject);
}

void drawTextCentered(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text);
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    rc2d_graphics_drawText(
        &textObject,
        std::round(rect.x + ((rect.w - static_cast<float>(width)) * 0.5f)),
        std::round(rect.y + ((rect.h - static_cast<float>(height)) * 0.5f)));
    rc2d_graphics_destroyText(&textObject);
}
} // namespace

NavigationWidget::NavigationWidget(void)
    : titleFont{},
      bodyFont{},
      controlIcons{},
      widgetRect{0.0f, 0.0f, kWidgetWidth, kWidgetHeight},
      visible(false),
      widgetDragging(false),
      widgetDragLocked(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      focusedField(FocusedField::NONE),
      inputSelectingWithMouse(false),
      cursorVisible(false),
      cursorBlinkElapsed(0.0),
      sectorNumberField{"", 0U, 0U},
      sectorLettersField{"", 0U, 0U},
      statusMessage{},
      statusIsError(false)
{
}

NavigationWidget::~NavigationWidget(void)
{
}

void NavigationWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 17.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 13.0f);
    this->controlIcons.load();

    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetDragging = false;
    this->widgetDragLocked = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->visible = false;
    this->focusedField = FocusedField::NONE;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
    this->sectorNumberField = InputFieldState{"", 0U, 0U};
    this->sectorLettersField = InputFieldState{"", 0U, 0U};
    this->statusMessage.clear();
    this->statusIsError = false;
    this->widgetRect = this->getBaseRect();
}

void NavigationWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
    this->sectorNumberField.editHistory.clear();
    this->sectorLettersField.editHistory.clear();
}

SDL_FRect NavigationWidget::getBaseRect(void) const
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kWidgetWidth) * 0.5f),
        screenRect.y + ((screenRect.h - kWidgetHeight) * 0.5f),
        kWidgetWidth,
        kWidgetHeight
    };
}

SDL_FRect NavigationWidget::getHeaderRect(void) const
{
    return SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        30.0f
    };
}

SDL_FRect NavigationWidget::getCloseButtonRect(void) const
{
    const SDL_FRect headerRect = this->getHeaderRect();
    return SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 28.0f,
        headerRect.y + ((headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
}

SDL_FRect NavigationWidget::getLockButtonRect(void) const
{
    const SDL_FRect closeButtonRect = this->getCloseButtonRect();
    return SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
}

SDL_FRect NavigationWidget::getSectorNumberFieldRect(void) const
{
    return SDL_FRect{
        this->widgetRect.x + 18.0f,
        this->widgetRect.y + 50.0f,
        34.0f,
        30.0f
    };
}

SDL_FRect NavigationWidget::getSectorLettersFieldRect(void) const
{
    return SDL_FRect{
        this->widgetRect.x + 72.0f,
        this->widgetRect.y + 50.0f,
        34.0f,
        30.0f
    };
}

SDL_FRect NavigationWidget::getGoButtonRect(void) const
{
    return SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 18.0f - 34.0f,
        this->widgetRect.y + 50.0f,
        34.0f,
        30.0f
    };
}

void NavigationWidget::update(double dt)
{
    const SDL_FRect baseRect = this->getBaseRect();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (this->widgetDragLocked)
    {
        this->widgetDragging = false;
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

    if (this->inputSelectingWithMouse)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->inputSelectingWithMouse = false;
        }
        else
        {
            InputFieldState* focusedState = this->getFocusedFieldState();
            const SDL_FRect fieldRect = this->getFocusedFieldRect();
            if (focusedState != nullptr && fieldRect.w > 0.0f)
            {
                float mouseX = 0.0f;
                float mouseY = 0.0f;
                getMouseRenderPosition(&mouseX, &mouseY);
                (void)mouseY;
                focusedState->cursorIndex = this->getCursorIndexFromPosition(*focusedState, mouseX, fieldRect);
                this->cursorVisible = true;
                this->cursorBlinkElapsed = 0.0;
            }
        }
    }

    if (this->focusedField != FocusedField::NONE)
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

NavigationWidget::InputFieldState* NavigationWidget::getFocusedFieldState(void)
{
    switch (this->focusedField)
    {
        case FocusedField::SECTOR_NUMBER:
            return &this->sectorNumberField;
        case FocusedField::SECTOR_LETTERS:
            return &this->sectorLettersField;
        case FocusedField::NONE:
        default:
            return nullptr;
    }
}

const NavigationWidget::InputFieldState* NavigationWidget::getFocusedFieldState(void) const
{
    switch (this->focusedField)
    {
        case FocusedField::SECTOR_NUMBER:
            return &this->sectorNumberField;
        case FocusedField::SECTOR_LETTERS:
            return &this->sectorLettersField;
        case FocusedField::NONE:
        default:
            return nullptr;
    }
}

SDL_FRect NavigationWidget::getFocusedFieldRect(void) const
{
    switch (this->focusedField)
    {
        case FocusedField::SECTOR_NUMBER:
            return this->getSectorNumberFieldRect();
        case FocusedField::SECTOR_LETTERS:
            return this->getSectorLettersFieldRect();
        case FocusedField::NONE:
        default:
            return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }
}

std::size_t NavigationWidget::getMaxLengthForField(FocusedField field) const
{
    switch (field)
    {
        case FocusedField::SECTOR_NUMBER:
            return 2U;
        case FocusedField::SECTOR_LETTERS:
            return 2U;
        case FocusedField::NONE:
        default:
            return 0U;
    }
}

std::string NavigationWidget::sanitizeInputForField(FocusedField field, const char* text) const
{
    std::string sanitized;
    if (text == nullptr)
    {
        return sanitized;
    }

    sanitized.reserve(this->getMaxLengthForField(field));
    for (const char* cursor = text; *cursor != '\0'; ++cursor)
    {
        const unsigned char current = static_cast<unsigned char>(*cursor);
        if (field == FocusedField::SECTOR_NUMBER)
        {
            if (current >= '0' && current <= '9')
            {
                sanitized.push_back(static_cast<char>(current));
            }
        }
        else if (field == FocusedField::SECTOR_LETTERS)
        {
            if (std::isalpha(current) != 0)
            {
                sanitized.push_back(static_cast<char>(std::toupper(current)));
            }
        }
    }

    const std::size_t maxLength = this->getMaxLengthForField(field);
    if (sanitized.size() > maxLength)
    {
        sanitized.resize(maxLength);
    }

    return sanitized;
}

bool NavigationWidget::hasSelection(const InputFieldState& fieldState) const
{
    return this->getSelectionStart(fieldState) != this->getSelectionEnd(fieldState);
}

std::size_t NavigationWidget::getSelectionStart(const InputFieldState& fieldState) const
{
    const std::size_t clampedCursor = (std::min)(fieldState.cursorIndex, fieldState.value.size());
    const std::size_t clampedAnchor = (std::min)(fieldState.selectionAnchor, fieldState.value.size());
    return (std::min)(clampedCursor, clampedAnchor);
}

std::size_t NavigationWidget::getSelectionEnd(const InputFieldState& fieldState) const
{
    const std::size_t clampedCursor = (std::min)(fieldState.cursorIndex, fieldState.value.size());
    const std::size_t clampedAnchor = (std::min)(fieldState.selectionAnchor, fieldState.value.size());
    return (std::max)(clampedCursor, clampedAnchor);
}

void NavigationWidget::clearSelection(InputFieldState& fieldState)
{
    fieldState.selectionAnchor = (std::min)(fieldState.cursorIndex, fieldState.value.size());
}

void NavigationWidget::deleteSelectedText(InputFieldState& fieldState)
{
    if (!this->hasSelection(fieldState))
    {
        return;
    }

    const std::size_t selectionStart = this->getSelectionStart(fieldState);
    const std::size_t selectionEnd = this->getSelectionEnd(fieldState);
    fieldState.value.erase(selectionStart, selectionEnd - selectionStart);
    fieldState.cursorIndex = selectionStart;
    this->clearSelection(fieldState);
}

std::size_t NavigationWidget::getCursorIndexFromPosition(
    const InputFieldState& fieldState,
    float renderX,
    const SDL_FRect& fieldRect) const
{
    const float textX = fieldRect.x + 8.0f;
    const float localX = (std::max)(0.0f, renderX - textX);

    std::size_t result = 0U;
    for (std::size_t index = 0; index <= fieldState.value.size(); ++index)
    {
        const float width = measureTextWidth(
            const_cast<RC2D_Font*>(&this->bodyFont),
            fieldState.value.substr(0, index));
        if (localX <= width)
        {
            result = index;
            break;
        }
        result = index;
    }

    return (std::min)(result, fieldState.value.size());
}

bool NavigationWidget::insertSanitizedTextIntoFocusedField(const std::string& sanitizedText)
{
    InputFieldState* fieldState = this->getFocusedFieldState();
    if (fieldState == nullptr)
    {
        return false;
    }

    if (sanitizedText.empty())
    {
        return true;
    }

        fieldState->editHistory.rememberState(
            fieldState->value,
            fieldState->cursorIndex,
            fieldState->selectionAnchor);

    if (this->hasSelection(*fieldState))
    {
        this->deleteSelectedText(*fieldState);
    }

    const std::size_t maxLength = this->getMaxLengthForField(this->focusedField);
    const std::size_t available = (fieldState->value.size() < maxLength) ? (maxLength - fieldState->value.size()) : 0U;
    if (available == 0U)
    {
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    std::string clampedText = sanitizedText;
    if (clampedText.size() > available)
    {
        clampedText.resize(available);
    }

    fieldState->cursorIndex = (std::min)(fieldState->cursorIndex, fieldState->value.size());
    fieldState->value.insert(fieldState->cursorIndex, clampedText);
    fieldState->cursorIndex += clampedText.size();
    this->clearSelection(*fieldState);
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->statusMessage.clear();
    this->statusIsError = false;
    return true;
}

void NavigationWidget::clearFocus(void)
{
    this->focusedField = FocusedField::NONE;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
}

void NavigationWidget::show(void)
{
    this->visible = true;
}

void NavigationWidget::hide(void)
{
    this->visible = false;
    this->clearFocus();
    this->widgetDragging = false;
}

bool NavigationWidget::hasBlockingTextInputFocus(void) const
{
    return this->visible && this->focusedField != FocusedField::NONE;
}

bool NavigationWidget::containsPoint(float x, float y) const
{
    return pointInRect(x, y, this->widgetRect);
}

HudCursorType NavigationWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (pointInRect(x, y, this->getSectorNumberFieldRect()) ||
        pointInRect(x, y, this->getSectorLettersFieldRect()))
    {
        return HudCursorType::TEXT;
    }

    if (pointInRect(x, y, this->getGoButtonRect()) ||
        pointInRect(x, y, this->getCloseButtonRect()) ||
        pointInRect(x, y, this->getLockButtonRect()))
    {
        return HudCursorType::POINTER;
    }

    if (pointInRect(x, y, this->getHeaderRect()))
    {
        return this->widgetDragLocked ? HudCursorType::DEFAULT : HudCursorType::MOVE;
    }

    if (this->containsPoint(x, y))
    {
        return HudCursorType::DEFAULT;
    }

    return HudCursorType::NONE;
}

bool NavigationWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = this->getBaseRect();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!pointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    if (pointInRect(x, y, this->getLockButtonRect()))
    {
        this->widgetDragLocked = !this->widgetDragLocked;
        this->widgetDragging = false;
        this->clearFocus();
        return true;
    }

    if (pointInRect(x, y, this->getCloseButtonRect()))
    {
        this->hide();
        return true;
    }

    auto focusFieldFromRect = [&](FocusedField field, InputFieldState& fieldState, const SDL_FRect& fieldRect) {
        this->focusedField = field;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;

        if (clicks >= 2)
        {
            fieldState.selectionAnchor = 0U;
            fieldState.cursorIndex = fieldState.value.size();
            this->inputSelectingWithMouse = false;
            this->widgetDragging = false;
            return true;
        }

        const std::size_t clickedIndex = this->getCursorIndexFromPosition(fieldState, x, fieldRect);
        fieldState.cursorIndex = clickedIndex;
        fieldState.selectionAnchor = clickedIndex;
        this->inputSelectingWithMouse = true;
        this->widgetDragging = false;
        return true;
    };

    const SDL_FRect numberRect = this->getSectorNumberFieldRect();
    if (pointInRect(x, y, numberRect))
    {
        return focusFieldFromRect(FocusedField::SECTOR_NUMBER, this->sectorNumberField, numberRect);
    }

    const SDL_FRect lettersRect = this->getSectorLettersFieldRect();
    if (pointInRect(x, y, lettersRect))
    {
        return focusFieldFromRect(FocusedField::SECTOR_LETTERS, this->sectorLettersField, lettersRect);
    }

    if (pointInRect(x, y, this->getGoButtonRect()))
    {
        this->commitNavigationRequest();
        return true;
    }

    if (pointInRect(x, y, this->getHeaderRect()))
    {
        if (this->widgetDragLocked)
        {
            this->clearFocus();
            return true;
        }

        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearFocus();
        return true;
    }

    this->clearFocus();
    return true;
}

bool NavigationWidget::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (!this->visible || this->focusedField == FocusedField::NONE)
    {
        return false;
    }

    InputFieldState* fieldState = this->getFocusedFieldState();
    if (fieldState == nullptr)
    {
        return false;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::UNDO, key, scancode, keycode, mod, isrepeat))
    {
        if (fieldState->editHistory.undo(
                &fieldState->value,
                &fieldState->cursorIndex,
                &fieldState->selectionAnchor))
        {
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::SELECT_ALL, key, scancode, keycode, mod, isrepeat))
    {
        fieldState->selectionAnchor = 0U;
        fieldState->cursorIndex = fieldState->value.size();
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::COPY, key, scancode, keycode, mod, isrepeat))
    {
        if (this->hasSelection(*fieldState))
        {
            const std::size_t selectionStart = this->getSelectionStart(*fieldState);
            const std::size_t selectionEnd = this->getSelectionEnd(*fieldState);
            rc2d_system_setClipboardText(fieldState->value.substr(selectionStart, selectionEnd - selectionStart).c_str());
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::PASTE, key, scancode, keycode, mod, isrepeat))
    {
        char* clipboardText = rc2d_system_getClipboardText();
        if (clipboardText != nullptr)
        {
            const std::string sanitized = this->sanitizeInputForField(this->focusedField, clipboardText);
            rc2d_system_freeClipboardText(clipboardText);
            return this->insertSanitizedTextIntoFocusedField(sanitized);
        }
        return true;
    }

    const bool shiftHeld = (mod & SDL_KMOD_SHIFT) != 0;
    switch (scancode)
    {
        case SDL_SCANCODE_TAB:
            if (this->focusedField == FocusedField::SECTOR_NUMBER)
            {
                this->focusedField = FocusedField::SECTOR_LETTERS;
                this->sectorLettersField.cursorIndex = this->sectorLettersField.value.size();
                this->sectorLettersField.selectionAnchor = this->sectorLettersField.cursorIndex;
            }
            else
            {
                this->focusedField = FocusedField::SECTOR_NUMBER;
                this->sectorNumberField.cursorIndex = this->sectorNumberField.value.size();
                this->sectorNumberField.selectionAnchor = this->sectorNumberField.cursorIndex;
            }
            this->inputSelectingWithMouse = false;
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            this->commitNavigationRequest();
            return true;
        case SDL_SCANCODE_ESCAPE:
            this->clearFocus();
            return true;
        case SDL_SCANCODE_LEFT:
            if (fieldState->cursorIndex > 0)
            {
                --fieldState->cursorIndex;
            }
            if (!shiftHeld)
            {
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RIGHT:
            if (fieldState->cursorIndex < fieldState->value.size())
            {
                ++fieldState->cursorIndex;
            }
            if (!shiftHeld)
            {
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_HOME:
            fieldState->cursorIndex = 0U;
            if (!shiftHeld)
            {
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_END:
            fieldState->cursorIndex = fieldState->value.size();
            if (!shiftHeld)
            {
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_BACKSPACE:
            if (this->hasSelection(*fieldState))
            {
                fieldState->editHistory.rememberState(
                    fieldState->value,
                    fieldState->cursorIndex,
                    fieldState->selectionAnchor);
                this->deleteSelectedText(*fieldState);
            }
            else if (fieldState->cursorIndex > 0 && !fieldState->value.empty())
            {
                fieldState->editHistory.rememberState(
                    fieldState->value,
                    fieldState->cursorIndex,
                    fieldState->selectionAnchor);
                fieldState->value.erase(fieldState->cursorIndex - 1U, 1U);
                --fieldState->cursorIndex;
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_DELETE:
            if (this->hasSelection(*fieldState))
            {
                fieldState->editHistory.rememberState(
                    fieldState->value,
                    fieldState->cursorIndex,
                    fieldState->selectionAnchor);
                this->deleteSelectedText(*fieldState);
            }
            else if (fieldState->cursorIndex < fieldState->value.size())
            {
                fieldState->editHistory.rememberState(
                    fieldState->value,
                    fieldState->cursorIndex,
                    fieldState->selectionAnchor);
                fieldState->value.erase(fieldState->cursorIndex, 1U);
                this->clearSelection(*fieldState);
            }
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        default:
            break;
    }

    return false;
}

bool NavigationWidget::textinput(const char* text)
{
    if (!this->visible || this->focusedField == FocusedField::NONE)
    {
        return false;
    }

    return this->insertSanitizedTextIntoFocusedField(this->sanitizeInputForField(this->focusedField, text));
}

bool NavigationWidget::parseSectorLetters(const std::string& letters, int* outSectorY) const
{
    if (outSectorY == nullptr || letters.size() != 2U)
    {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(letters[0]);
    const unsigned char second = static_cast<unsigned char>(letters[1]);
    if (!std::isalpha(first) || !std::isalpha(second))
    {
        return false;
    }

    const int firstIndex = std::toupper(first) - 'A';
    const int secondIndex = std::toupper(second) - 'A';
    const int sectorY = (firstIndex * 26) + secondIndex;
    if (sectorY < 0 || sectorY >= Map::NUM_SECTORS_Y)
    {
        return false;
    }

    *outSectorY = sectorY;
    return true;
}

void NavigationWidget::commitNavigationRequest(void)
{
    int sectorX = -1;
    int sectorY = -1;

    if (this->sectorNumberField.value.empty())
    {
        this->statusMessage = "Numero requis.";
        this->statusIsError = true;
        return;
    }

    sectorX = std::atoi(this->sectorNumberField.value.c_str());
    if (sectorX < 0 || sectorX >= Map::NUM_SECTORS_X)
    {
        this->statusMessage = "Numero entre 00 et 59.";
        this->statusIsError = true;
        return;
    }

    if (!this->parseSectorLetters(this->sectorLettersField.value, &sectorY))
    {
        this->statusMessage = "Lettres entre AA et CH.";
        this->statusIsError = true;
        return;
    }

    char sectorNumberBuffer[8] = {};
    std::snprintf(sectorNumberBuffer, sizeof(sectorNumberBuffer), "%02d", sectorX);
    this->sectorNumberField.value = sectorNumberBuffer;
    this->sectorNumberField.cursorIndex = this->sectorNumberField.value.size();
    this->sectorNumberField.selectionAnchor = this->sectorNumberField.cursorIndex;

    const int firstLetter = sectorY / 26;
    const int secondLetter = sectorY % 26;
    this->sectorLettersField.value = std::string{
        static_cast<char>('A' + firstLetter),
        static_cast<char>('A' + secondLetter)};
    this->sectorLettersField.cursorIndex = this->sectorLettersField.value.size();
    this->sectorLettersField.selectionAnchor = this->sectorLettersField.cursorIndex;

    this->statusMessage.clear();
    this->statusIsError = false;
    this->clearFocus();

    Map& currentMap = GetCurrentMap();
    Player& player = GetGameState().player;
    const SDL_Point targetTile = currentMap.sectorToTile(sectorX, sectorY);
    player.moveToTile(currentMap, targetTile.x, targetTile.y);
    GetIngameHudOverlay().notifyMapTileClicked(targetTile.x, targetTile.y);
}

void NavigationWidget::draw(void) const
{
    if (!this->visible)
    {
        return;
    }

    NavigationWidget* self = const_cast<NavigationWidget*>(this);
    const SDL_FRect baseRect = self->getBaseRect();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const SDL_FRect headerRect = this->getHeaderRect();
    const SDL_FRect closeRect = this->getCloseButtonRect();
    const SDL_FRect lockRect = this->getLockButtonRect();
    const SDL_FRect sectorNumberRect = this->getSectorNumberFieldRect();
    const SDL_FRect sectorLettersRect = this->getSectorLettersFieldRect();
    const SDL_FRect goRect = this->getGoButtonRect();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    const bool goHovered = pointInRect(mouseX, mouseY, goRect);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{
        outer.x + 4.0f,
        outer.y + 4.0f,
        outer.w - 8.0f,
        outer.h - 8.0f};

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kPanelBorder);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kInnerBorder);
    rc2d_graphics_rectangle("line", &inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &headerRect);
    rc2d_graphics_setColor(kPanelBorder);
    rc2d_graphics_rectangle("line", &headerRect);

    drawTextLeft(
        const_cast<RC2D_Font*>(&this->titleFont),
        "Navigation",
        headerRect.x + 10.0f,
        headerRect.y + std::round((headerRect.h - measureTextHeight(const_cast<RC2D_Font*>(&this->titleFont), "Navigation")) * 0.5f) + 1.0f,
        kTextGold);

    self->controlIcons.drawLockButton(lockRect, this->widgetDragLocked, kHeaderFill, kPanelBorder);
    self->controlIcons.drawCloseButton(closeRect, kHeaderFill, kPanelBorder);

    auto drawField = [&](const InputFieldState& fieldState, FocusedField fieldId, const SDL_FRect& rect) {
        const bool focused = this->focusedField == fieldId;
        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &rect);
        rc2d_graphics_setColor(focused ? kFieldFocusedBorder : kFieldBorder);
        rc2d_graphics_rectangle("line", &rect);

        const float textX = rect.x + 6.0f;
        const float textY = rect.y + std::round((rect.h - measureTextHeight(const_cast<RC2D_Font*>(&this->bodyFont), "Ag")) * 0.5f);
        const std::size_t selectionStart = this->getSelectionStart(fieldState);
        const std::size_t selectionEnd = this->getSelectionEnd(fieldState);
        if (focused && this->hasSelection(fieldState))
        {
            const float prefixWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), fieldState.value.substr(0, selectionStart));
            const float selectionWidth = measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), fieldState.value.substr(selectionStart, selectionEnd - selectionStart));
            SDL_FRect selectionRect = SDL_FRect{
                std::round(textX + prefixWidth),
                rect.y + 5.0f,
                selectionWidth,
                rect.h - 10.0f
            };
            rc2d_graphics_setColor(kSelectionFill);
            rc2d_graphics_rectangle("fill", &selectionRect);
        }

        drawTextLeft(
            const_cast<RC2D_Font*>(&this->bodyFont),
            fieldState.value.c_str(),
            textX,
            textY,
            kTextWhite);

        if (focused && this->cursorVisible)
        {
            const float caretX = std::round(textX + measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), fieldState.value.substr(0, fieldState.cursorIndex)));
            SDL_FRect caretRect = SDL_FRect{caretX, rect.y + 5.0f, 1.0f, rect.h - 10.0f};
            rc2d_graphics_setColor(kTextWhite);
            rc2d_graphics_rectangle("fill", &caretRect);
        }
    };

    drawField(this->sectorNumberField, FocusedField::SECTOR_NUMBER, sectorNumberRect);
    drawField(this->sectorLettersField, FocusedField::SECTOR_LETTERS, sectorLettersRect);

    drawTextCentered(
        const_cast<RC2D_Font*>(&this->titleFont),
        "-",
        SDL_FRect{
            sectorNumberRect.x + sectorNumberRect.w + 2.0f,
            sectorNumberRect.y,
            sectorLettersRect.x - (sectorNumberRect.x + sectorNumberRect.w + 4.0f),
            sectorNumberRect.h
        },
        kTextGold);

    rc2d_graphics_setColor(goHovered ? kButtonHoverFill : kButtonFill);
    rc2d_graphics_rectangle("fill", &goRect);
    rc2d_graphics_setColor(kPanelBorder);
    rc2d_graphics_rectangle("line", &goRect);
    drawTextCentered(const_cast<RC2D_Font*>(&this->titleFont), "Go", goRect, kTextGold);

    drawTextLeft(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Ex : 30-AA",
        this->widgetRect.x + 18.0f,
        this->widgetRect.y + 88.0f,
        kTextMuted);

    if (!this->statusMessage.empty())
    {
        drawTextLeft(
            const_cast<RC2D_Font*>(&this->bodyFont),
            this->statusMessage.c_str(),
            this->widgetRect.x + 18.0f,
            this->widgetRect.y + 104.0f,
            kStatusError);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
