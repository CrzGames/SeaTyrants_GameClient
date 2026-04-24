#include "game/ui/hud/espion-search-player-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>

static constexpr float kRefW = 377.0f;
static constexpr float kRefH = 176.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
// Teinte or globale.
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kButtonHoverFill = RC2D_Color{27, 18, 8, 242};
static constexpr RC2D_Color kInputSelectionFill = RC2D_Color{67, 96, 144, 215};
// Teinte dediee au titre "Espion" pour forcer exactement la nuance voulue.
static constexpr RC2D_Color kEspionTitleColor = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{124, 109, 84, 255};
static constexpr double kCursorBlinkPeriod = 0.55;
static constexpr std::size_t kMaxPlayerIdDigits = 15;

static SDL_FRect getEspionRectFromGameScreen(void)
{
    // On recupere l'espace de rendu du jeu pour centrer la fenetre.
    const SDL_FRect screenRect = GetGameScreen().rect;
    // Position initiale: centree horizontalement et verticalement.
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH
    };
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    // Test d'appartenance simple pour savoir si un clic est dans un rectangle.
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

static void getMouseRenderPosition(float* outX, float* outY)
{
    // Protection defensive: si un pointeur de sortie est nul, on quitte.
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

    // Position brute de la souris en coordonnees "fenetre".
    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);

    // On essaye de convertir en coordonnees de rendu (viewport/camera).
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        // Fallback securise: si pas de renderer, on garde les coords fenetre.
        *outX = windowX;
        *outY = windowY;
        return;
    }

    float renderX = windowX;
    float renderY = windowY;
    if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &renderX, &renderY))
    {
        // En cas d'echec de conversion, on reutilise aussi les coords fenetre.
        renderX = windowX;
        renderY = windowY;
    }

    // Valeurs finales renvoyees a l'appelant.
    *outX = renderX;
    *outY = renderY;
}

static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& r, RC2D_Color color)
{
    // Garde-fous: pas de rendu possible sans police/texte valides.
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    // Creation d'un objet texte RC2D puis application explicite de la couleur.
    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    // Arrondi au pixel pour eviter le flou visuel sur les glyphes.
    const float drawX = std::round(r.x + ((r.w - static_cast<float>(w)) * 0.5f));
    const float drawY = std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static float measureTextWidth(RC2D_Font* font, const std::string& text)
{
    // Si la police n'est pas prete ou texte vide, largeur nulle.
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return 0.0f;
    }

    // Mesure de largeur utile pour le placement du curseur.
    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    rc2d_graphics_destroyText(&t);
    return static_cast<float>(w);
}

static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& r, float x, RC2D_Color color)
{
    // Meme logique de securite qu'un rendu centre.
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    // On force la couleur directement dans la structure texte.
    RC2D_Text t = rc2d_graphics_createText(font, text);
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    (void)w;
    // Texte aligne a gauche sur X fourni, centre verticalement dans la case.
    const float drawX = std::round(x);
    const float drawY = std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f));
    rc2d_graphics_drawText(&t, drawX, drawY);
    rc2d_graphics_destroyText(&t);
}

static void drawImageFit(const RC2D_Image& image, const SDL_FRect& target, float padding)
{
    if (image.sdl_texture == nullptr)
    {
        return;
    }

    float texW = 0.0f;
    float texH = 0.0f;
    if (!SDL_GetTextureSize(image.sdl_texture, &texW, &texH) || texW <= 0.0f || texH <= 0.0f)
    {
        return;
    }

    const float clampedPadding = (std::max)(0.0f, padding);
    const float maxWidth = (std::max)(1.0f, target.w - (clampedPadding * 2.0f));
    const float maxHeight = (std::max)(1.0f, target.h - (clampedPadding * 2.0f));
    const float scale = (std::min)(maxWidth / texW, maxHeight / texH);
    const float drawW = texW * scale;
    const float drawH = texH * scale;
    const float drawX = std::round(target.x + ((target.w - drawW) * 0.5f));
    const float drawY = std::round(target.y + ((target.h - drawH) * 0.5f));

    RC2D_Image imageCopy = image;
    const RC2D_Quad quad = rc2d_graphics_newQuad(&imageCopy, 0.0f, 0.0f, texW, texH);
    if (quad.src.w <= 0.0f || quad.src.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawQuad(
        &imageCopy,
        &quad,
        drawX,
        drawY,
        0.0,
        scale,
        scale,
        -1.0f,
        -1.0f,
        false,
        false);
}

static char extractDigitFromKeyLabel(const char* key)
{
    // Cas invalide: aucune touche exploitable.
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    // Cas direct: une touche "0"..."9".
    if (key[1] == '\0' && key[0] >= '0' && key[0] <= '9')
    {
        return key[0];
    }

    std::string lower(key);
    // Normalisation en minuscule pour simplifier les comparaisons.
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool looksLikeKeypad = (lower.find("kp") != std::string::npos) ||
                                 (lower.find("keypad") != std::string::npos) ||
                                 (lower.find("numpad") != std::string::npos);
    // Si ce n'est pas un label numerique clavier/pave, on ignore.
    if (!looksLikeKeypad)
    {
        return '\0';
    }

    // On extrait le premier chiffre present dans le label.
    for (const char c : lower)
    {
        if (c >= '0' && c <= '9')
        {
            return c;
        }
    }

    return '\0';
}

EspionSearchPlayerWidget::EspionSearchPlayerWidget(void)
    : titleFont{},
      bodyFont{},
      rubiesPriceIcon{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      playerIdInput{},
      cursorIndex(0),
      selectionAnchorIndex(0),
      inputFocused(false),
      inputSelectingWithMouse(false),
      cursorVisible(true),
      cursorBlinkElapsed(0.0),
      widgetDragging(false),
      widgetDragLocked(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      searchResultText{},
      rubiesPrice(250),
      cursorEnabled(true),
      controlIcons{},
      onFindPlayerRequested{}
{
}

EspionSearchPlayerWidget::~EspionSearchPlayerWidget(void)
{
}

void EspionSearchPlayerWidget::load(void)
{
    // Meme rendu de titre que la fenetre Chat.
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->rubiesPriceIcon = LoadStorageImage("assets/images/ui-scene-game/money-rubies.png", RC2D_STORAGE_TITLE);
    this->controlIcons.load();
    const SDL_FRect baseRect = getEspionRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    this->visible = false;
    this->playerIdInput.clear();
    this->cursorIndex = 0;
    this->selectionAnchorIndex = 0;
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = true;
    this->cursorBlinkElapsed = 0.0;
    this->widgetDragging = false;
    this->widgetDragLocked = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->searchResultText.clear();
    this->rubiesPrice = 250;
}

void EspionSearchPlayerWidget::unload(void)
{
    this->controlIcons.unload();
    ResetStorageImageRef(&this->rubiesPriceIcon);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void EspionSearchPlayerWidget::update(double dt)
{
    // Base de position (ancrage ecran) + offset de drag courant.
    const SDL_FRect baseRect = getEspionRectFromGameScreen();
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
        // Si le clic gauche est relache, on stoppe immediatement le drag.
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->widgetDragging = false;
        }
        else
        {
            // Sinon, on recalcule l'offset en coordonnees de rendu.
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
            this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
            this->widgetRect.x = baseRect.x + this->widgetOffsetX;
            this->widgetRect.y = baseRect.y + this->widgetOffsetY;
        }
    }

    const SDL_FRect topRowBox = SDL_FRect{
        this->widgetRect.x + 10.0f,
        this->widgetRect.y + 40.0f,
        this->widgetRect.w - 20.0f,
        34.0f
    };
    const SDL_FRect topRowInput = SDL_FRect{
        topRowBox.x + 132.0f,
        topRowBox.y + 4.0f,
        topRowBox.w - 138.0f,
        topRowBox.h - 8.0f
    };

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
            this->cursorIndex = this->getPlayerIdCursorIndexFromPosition(mouseX, topRowInput);
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
        }
    }

    if (this->inputFocused)
    {
        // Clignotement du curseur tant que le champ est focus.
        this->cursorBlinkElapsed += dt;
        while (this->cursorBlinkElapsed >= kCursorBlinkPeriod)
        {
            this->cursorBlinkElapsed -= kCursorBlinkPeriod;
            this->cursorVisible = !this->cursorVisible;
        }
    }
    else
    {
        // Sans focus, on masque le curseur et on reinitialise le timer.
        this->cursorBlinkElapsed = 0.0;
        this->cursorVisible = false;
    }
}

bool EspionSearchPlayerWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Parametres non utilises (API commune).
    (void)mouseID;

    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        // Widget ferme ou bouton different du clic gauche: non consomme.
        return false;
    }

    const SDL_FRect baseRect = getEspionRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!isPointInRect(x, y, this->widgetRect))
    {
        // Clic en dehors du widget: laisse les autres composants traiter.
        return false;
    }

    const SDL_FRect headerRect = SDL_FRect{
        this->widgetRect.x + 5.0f,
        this->widgetRect.y + 5.0f,
        this->widgetRect.w - 10.0f,
        30.0f
    };
    const SDL_FRect topRowBox = SDL_FRect{this->widgetRect.x + 10.0f, this->widgetRect.y + 40.0f, this->widgetRect.w - 20.0f, 34.0f};
    const SDL_FRect topRowInput = SDL_FRect{topRowBox.x + 132.0f, topRowBox.y + 4.0f, topRowBox.w - 138.0f, topRowBox.h - 8.0f};
    const SDL_FRect actionButton = SDL_FRect{this->widgetRect.x + 172.0f, this->widgetRect.y + 81.0f, this->widgetRect.w - 182.0f, 44.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        this->widgetRect.x + this->widgetRect.w - 28.0f,
        headerRect.y + ((headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    if (isPointInRect(x, y, lockButtonRect))
    {
        this->widgetDragLocked = !this->widgetDragLocked;
        this->widgetDragging = false;
        this->clearFocus();
        return true;
    }

    if (isPointInRect(x, y, closeButtonRect))
    {
        // Ferme le widget et annule les etats interactifs en cours.
        this->visible = false;
        this->widgetDragging = false;
        this->clearFocus();
        return true;
    }

    if (isPointInRect(x, y, topRowInput))
    {
        // Activation du focus de saisie + reset clignotement curseur.
        this->inputFocused = true;
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;

        // Double-clic: selection complete du contenu de l'input.
        if (clicks >= 2)
        {
            this->selectionAnchorIndex = 0;
            this->cursorIndex = this->playerIdInput.size();
            this->inputSelectingWithMouse = false;
            this->widgetDragging = false;
            return true;
        }

        // Clic simple: placement du curseur a la position la plus proche.
        const std::size_t clickedIndex = this->getPlayerIdCursorIndexFromPosition(x, topRowInput);
        this->cursorIndex = clickedIndex;
        this->selectionAnchorIndex = clickedIndex;
        this->inputSelectingWithMouse = true;
        this->widgetDragging = false;
        return true;
    }

    if (isPointInRect(x, y, actionButton))
    {
        // Un clic sur le bouton declenche l'action publique avec l'ID saisi.
        this->widgetDragging = false;
        this->clearFocus();
        if (this->onFindPlayerRequested)
        {
            this->onFindPlayerRequested(this->playerIdInput);
        }
        return true;
    }

    if (isPointInRect(x, y, headerRect))
    {
        // Drag possible uniquement via le bandeau du haut.
        if (this->widgetDragLocked)
        {
            this->clearFocus();
            return true;
        }
        this->widgetDragging = true;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearFocus();
    }
    else
    {
        // Clic dans le widget mais hors header/input: perte de focus texte.
        this->clearFocus();
    }
    return true;
}

bool EspionSearchPlayerWidget::keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat)
{
    (void)mod;

    if (!this->visible || !this->inputFocused)
    {
        // Le clavier n'est traite que si le champ ID est actif.
        return false;
    }

    // On accepte la repetition des touches pour suppression fluide.
    (void)isrepeat;

    // Priorite absolue aux chiffres (top-row + pave numerique).
    // On le fait avant les raccourcis Home/End/Left/Right pour eviter
    // qu'un event numpad mappe en navigation soit consomme trop tot.
    char digit = '\0';
    digit = extractDigitFromKeyLabel(key);
    if (digit == '\0' && scancode >= SDL_SCANCODE_KP_0 && scancode <= SDL_SCANCODE_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(scancode - SDL_SCANCODE_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_KP_0 && keycode <= SDLK_KP_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_KP_0));
    }
    if (digit == '\0' && keycode >= SDLK_0 && keycode <= SDLK_9)
    {
        digit = static_cast<char>('0' + static_cast<int>(keycode - SDLK_0));
    }
    if (digit != '\0')
    {
        if (this->hasPlayerIdSelection())
        {
            this->deleteSelectedPlayerIdText();
        }
        // Limitation stricte de la longueur numerique.
        if (this->playerIdInput.size() >= kMaxPlayerIdDigits)
        {
            return true;
        }
        // Insertion du chiffre a la position du curseur.
        this->cursorIndex = (std::min)(this->cursorIndex, this->playerIdInput.size());
        this->playerIdInput.insert(this->cursorIndex, 1, digit);
        ++this->cursorIndex;
        this->clearPlayerIdSelection();
        this->cursorVisible = true;
        this->cursorBlinkElapsed = 0.0;
        return true;
    }

    switch (scancode)
    {
        // Navigation classique dans le champ texte.
        case SDL_SCANCODE_LEFT:
            if (this->hasPlayerIdSelection())
            {
                this->cursorIndex = this->getPlayerIdSelectionStart();
            }
            else if (this->cursorIndex > 0)
            {
                --this->cursorIndex;
            }
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RIGHT:
            if (this->hasPlayerIdSelection())
            {
                this->cursorIndex = this->getPlayerIdSelectionEnd();
            }
            else if (this->cursorIndex < this->playerIdInput.size())
            {
                ++this->cursorIndex;
            }
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_HOME:
            this->cursorIndex = 0;
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_END:
            this->cursorIndex = this->playerIdInput.size();
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_BACKSPACE:
            // Supprime d'abord la selection si elle existe.
            if (this->hasPlayerIdSelection())
            {
                this->deleteSelectedPlayerIdText();
            }
            else if (this->cursorIndex > 0 && !this->playerIdInput.empty())
            {
                this->playerIdInput.erase(this->cursorIndex - 1, 1);
                --this->cursorIndex;
            }
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_DELETE:
            // Supprime d'abord la selection si elle existe.
            if (this->hasPlayerIdSelection())
            {
                this->deleteSelectedPlayerIdText();
            }
            else if (this->cursorIndex < this->playerIdInput.size())
            {
                this->playerIdInput.erase(this->cursorIndex, 1);
            }
            this->clearPlayerIdSelection();
            this->cursorVisible = true;
            this->cursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_ESCAPE:
            // Echap retire simplement le focus.
            this->clearFocus();
            return true;
        default:
            break;
    }

    return false;
}

void EspionSearchPlayerWidget::publishSearchResult(const std::string& resultText)
{
    this->searchResultText = resultText;
}

void EspionSearchPlayerWidget::setRubiesPrice(int newRubiesPrice)
{
    this->rubiesPrice = (std::max)(0, newRubiesPrice);
}

void EspionSearchPlayerWidget::setOnFindPlayerRequested(const std::function<void(const std::string&)>& callback)
{
    this->onFindPlayerRequested = callback;
}

std::size_t EspionSearchPlayerWidget::getPlayerIdCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const
{
    const float inputTextX = inputRect.x + 8.0f;
    const float localX = (std::max)(0.0f, renderX - inputTextX);
    std::size_t newCursor = 0;
    for (std::size_t i = 0; i <= this->playerIdInput.size(); ++i)
    {
        const float width = measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), this->playerIdInput.substr(0, i));
        if (localX <= width)
        {
            newCursor = i;
            break;
        }
        newCursor = i;
    }
    return (std::min)(newCursor, this->playerIdInput.size());
}

bool EspionSearchPlayerWidget::hasPlayerIdSelection(void) const
{
    return this->getPlayerIdSelectionStart() != this->getPlayerIdSelectionEnd();
}

std::size_t EspionSearchPlayerWidget::getPlayerIdSelectionStart(void) const
{
    const std::size_t clampedCursor = (std::min)(this->cursorIndex, this->playerIdInput.size());
    const std::size_t clampedAnchor = (std::min)(this->selectionAnchorIndex, this->playerIdInput.size());
    return (std::min)(clampedAnchor, clampedCursor);
}

std::size_t EspionSearchPlayerWidget::getPlayerIdSelectionEnd(void) const
{
    const std::size_t clampedCursor = (std::min)(this->cursorIndex, this->playerIdInput.size());
    const std::size_t clampedAnchor = (std::min)(this->selectionAnchorIndex, this->playerIdInput.size());
    return (std::max)(clampedAnchor, clampedCursor);
}

void EspionSearchPlayerWidget::clearPlayerIdSelection(void)
{
    this->selectionAnchorIndex = (std::min)(this->cursorIndex, this->playerIdInput.size());
}

void EspionSearchPlayerWidget::deleteSelectedPlayerIdText(void)
{
    if (!this->hasPlayerIdSelection())
    {
        return;
    }

    const std::size_t selectionStart = this->getPlayerIdSelectionStart();
    const std::size_t selectionEnd = this->getPlayerIdSelectionEnd();
    this->playerIdInput.erase(selectionStart, selectionEnd - selectionStart);
    this->cursorIndex = selectionStart;
    this->clearPlayerIdSelection();
}

void EspionSearchPlayerWidget::draw(void) const
{
    // Le draw est const, mais on met a jour la rect calculee du widget.
    EspionSearchPlayerWidget* self = const_cast<EspionSearchPlayerWidget*>(this);
    const SDL_FRect baseRect = getEspionRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!self->visible)
    {
        // Rien a rendre si la fenetre est fermee.
        return;
    }

    // Definition de toutes les zones de rendu.
    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButton = SDL_FRect{outer.x + outer.w - 28.0f, outer.y + 8.0f, 20.0f, 20.0f};
    const SDL_FRect closeButtonCentered = SDL_FRect{
        closeButton.x,
        header.y + ((header.h - closeButton.h) * 0.5f),
        closeButton.w,
        closeButton.h
    };
    const SDL_FRect lockButtonCentered = SDL_FRect{
        closeButtonCentered.x - 24.0f,
        closeButtonCentered.y,
        closeButtonCentered.w,
        closeButtonCentered.h
    };

    const SDL_FRect topRowBox = SDL_FRect{outer.x + 10.0f, outer.y + 40.0f, outer.w - 20.0f, 34.0f};
    const SDL_FRect topRowInput = SDL_FRect{topRowBox.x + 132.0f, topRowBox.y + 4.0f, topRowBox.w - 138.0f, topRowBox.h - 8.0f};
    const SDL_FRect priceRowBox = SDL_FRect{outer.x + 10.0f, outer.y + 81.0f, 160.0f, 44.0f};
    const SDL_FRect actionButton = SDL_FRect{outer.x + 172.0f, outer.y + 81.0f, outer.w - 182.0f, 44.0f};
    const SDL_FRect resultBox = SDL_FRect{outer.x + 10.0f, outer.y + outer.h - 44.0f, outer.w - 20.0f, 34.0f};
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    const bool actionButtonHovered = isPointInRect(mouseX, mouseY, actionButton);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Cadre externe/interne.
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    // Bandeau de titre.
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);

    // Rendu direct du titre avec couleur dediee (sans helper) pour eviter tout override.
    RC2D_Text titleText = rc2d_graphics_createText(&self->titleFont, "Espion");
    titleText.color = kEspionTitleColor;
    rc2d_graphics_setTextColor(&titleText);
    int titleW = 0;
    int titleH = 0;
    rc2d_graphics_getTextSize(&titleText, &titleW, &titleH);
    const float titleX = std::round(header.x + 10.0f);
    const float titleY = std::round(header.y + ((header.h - static_cast<float>(titleH)) * 0.5f) + 1.0f);
    rc2d_graphics_drawText(&titleText, titleX, titleY);
    rc2d_graphics_destroyText(&titleText);

    // Bouton fermer.
    self->controlIcons.drawLockButton(
        lockButtonCentered,
        self->widgetDragLocked,
        self->widgetDragLocked ? kPanelFill : kHeaderFill,
        kGold);
    self->controlIcons.drawCloseButton(closeButtonCentered, kHeaderFill, kGold);

    // Ligne "ID du joueur".
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &topRowBox);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &topRowBox);
    drawLeftCenteredY(&self->bodyFont, "ID du joueur :", topRowBox, topRowBox.x + 8.0f, kTextWhite);

    // Champ de saisie ID + curseur clignotant.
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &topRowInput);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &topRowInput);
    if (self->hasPlayerIdSelection())
    {
        const std::size_t selectionStart = self->getPlayerIdSelectionStart();
        const std::size_t selectionEnd = self->getPlayerIdSelectionEnd();
        const std::string beforeSelection = self->playerIdInput.substr(0, selectionStart);
        const std::string selectedText = self->playerIdInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(topRowInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        const float selectionW = measureTextWidth(&self->bodyFont, selectedText);
        const SDL_FRect selectionRect = SDL_FRect{
            selectionX - 1.0f,
            topRowInput.y + 5.0f,
            (std::max)(2.0f, selectionW + 2.0f),
            topRowInput.h - 10.0f
        };
        rc2d_graphics_setColor(kInputSelectionFill);
        rc2d_graphics_rectangle("fill", &selectionRect);
    }
    drawLeftCenteredY(&self->bodyFont, self->playerIdInput.c_str(), topRowInput, topRowInput.x + 8.0f, kTextGold);
    if (self->hasPlayerIdSelection())
    {
        const std::size_t selectionStart = self->getPlayerIdSelectionStart();
        const std::size_t selectionEnd = self->getPlayerIdSelectionEnd();
        const std::string beforeSelection = self->playerIdInput.substr(0, selectionStart);
        const std::string selectedText = self->playerIdInput.substr(selectionStart, selectionEnd - selectionStart);
        const float selectionX = std::round(topRowInput.x + 8.0f + measureTextWidth(&self->bodyFont, beforeSelection));
        drawLeftCenteredY(&self->bodyFont, selectedText.c_str(), topRowInput, selectionX, kTextWhite);
    }
    if (self->inputFocused && self->cursorVisible)
    {
        const std::size_t cursor = (std::min)(self->cursorIndex, self->playerIdInput.size());
        const std::string prefix = self->playerIdInput.substr(0, cursor);
        const float cursorX = std::round(topRowInput.x + 8.0f + measureTextWidth(&self->bodyFont, prefix));
        const float cursorTop = std::round(topRowInput.y + 6.0f);
        const float cursorBottom = std::round(topRowInput.y + topRowInput.h - 6.0f);
        rc2d_graphics_setColor(kTextGold);
        rc2d_graphics_line(cursorX, cursorTop, cursorX, cursorBottom);
    }

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &priceRowBox);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &priceRowBox);

    // Aligne le bloc "Prix : + icone + valeur" comme un groupe unique,
    // centre horizontalement avec des espacements reguliers.
    const std::string rubiesPriceText = std::to_string((std::max)(0, self->rubiesPrice));
    const float priceLabelWidth = measureTextWidth(&self->bodyFont, "Prix :");
    const float priceValueWidth = measureTextWidth(&self->bodyFont, rubiesPriceText);
    const bool hasPriceIcon = self->rubiesPriceIcon.sdl_texture != nullptr;
    const float priceGroupGap = 8.0f;
    const float priceIconSize = hasPriceIcon ? 20.0f : 0.0f;
    const float priceGroupWidth =
        priceLabelWidth +
        (hasPriceIcon ? priceGroupGap : 0.0f) +
        priceIconSize +
        priceGroupGap +
        priceValueWidth;
    const float priceGroupStartX = std::round(priceRowBox.x + ((priceRowBox.w - priceGroupWidth) * 0.5f));
    const float priceIconX = priceGroupStartX + priceLabelWidth + (hasPriceIcon ? priceGroupGap : 0.0f);
    const SDL_FRect priceIconRect = SDL_FRect{
        std::round(priceIconX),
        std::round(priceRowBox.y + ((priceRowBox.h - priceIconSize) * 0.5f)),
        priceIconSize,
        priceIconSize
    };

    drawLeftCenteredY(&self->bodyFont, "Prix :", priceRowBox, priceGroupStartX, kTextWhite);
    if (hasPriceIcon)
    {
        drawImageFit(self->rubiesPriceIcon, priceIconRect, 0.0f);
    }
    drawLeftCenteredY(
        &self->bodyFont,
        rubiesPriceText.c_str(),
        priceRowBox,
        priceIconRect.x + priceIconRect.w + priceGroupGap,
        kTextGold);

    // Bouton de recherche.
    rc2d_graphics_setColor(actionButtonHovered ? kButtonHoverFill : kFieldFill);
    rc2d_graphics_rectangle("fill", &actionButton);
    rc2d_graphics_setColor(actionButtonHovered ? kSilver : kGold);
    rc2d_graphics_rectangle("line", &actionButton);
    drawCentered(&self->bodyFont, "Trouver un joueur", actionButton, actionButtonHovered ? kTextGold : kTextWhite);

    // Case resultat (colonne de droite, tout en bas).
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &resultBox);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &resultBox);
    const std::string resultLabel = "Resultat de la recherche :";
    drawLeftCenteredY(&self->bodyFont, resultLabel.c_str(), resultBox, resultBox.x + 8.0f, kTextWhite);
    if (!self->searchResultText.empty())
    {
        const float labelWidth = measureTextWidth(&self->bodyFont, resultLabel);
        const float valueAreaStart = resultBox.x + 8.0f + labelWidth + 6.0f;
        const float valueAreaEnd = resultBox.x + resultBox.w - 8.0f;
        const float valueAreaWidth = (std::max)(0.0f, valueAreaEnd - valueAreaStart);
        const float resultTextWidth = measureTextWidth(&self->bodyFont, self->searchResultText);
        const float centeredX = valueAreaStart + ((valueAreaWidth - resultTextWidth) * 0.5f);
        const float resultX = (std::max)(valueAreaStart, centeredX);
        drawLeftCenteredY(&self->bodyFont, self->searchResultText.c_str(), resultBox, resultX, kTextGold);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EspionSearchPlayerWidget::clearFocus(void)
{
    this->inputFocused = false;
    this->inputSelectingWithMouse = false;
    this->cursorVisible = false;
    this->cursorBlinkElapsed = 0.0;
    this->clearPlayerIdSelection();
}

bool EspionSearchPlayerWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getEspionRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType EspionSearchPlayerWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->widgetDragging && !this->widgetDragLocked)
    {
        return HudCursorType::MOVE;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const SDL_FRect baseRect = getEspionRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const SDL_FRect headerRect = SDL_FRect{currentRect.x + 5.0f, currentRect.y + 5.0f, currentRect.w - 10.0f, 30.0f};
    const SDL_FRect topRowBox = SDL_FRect{currentRect.x + 10.0f, currentRect.y + 40.0f, currentRect.w - 20.0f, 34.0f};
    const SDL_FRect topRowInput = SDL_FRect{topRowBox.x + 132.0f, topRowBox.y + 4.0f, topRowBox.w - 138.0f, topRowBox.h - 8.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        currentRect.x + currentRect.w - 28.0f,
        headerRect.y + ((headerRect.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect actionButton = SDL_FRect{currentRect.x + 172.0f, currentRect.y + 81.0f, currentRect.w - 182.0f, 44.0f};

    if (isPointInRect(x, y, topRowInput))
    {
        return HudCursorType::TEXT;
    }
    if (isPointInRect(x, y, actionButton) || isPointInRect(x, y, closeButtonRect) || isPointInRect(x, y, lockButtonRect))
    {
        return HudCursorType::POINTER;
    }
    if (x >= lockButtonRect.x && x <= (closeButtonRect.x + closeButtonRect.w) &&
        y >= headerRect.y && y <= (headerRect.y + headerRect.h))
    {
        return HudCursorType::DEFAULT;
    }
    if (isPointInRect(x, y, headerRect) && !this->widgetDragLocked)
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

void EspionSearchPlayerWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
    this->clearFocus();
}

void EspionSearchPlayerWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->clearFocus();
}



