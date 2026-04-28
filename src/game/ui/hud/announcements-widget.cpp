#include "game/ui/hud/announcements-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

static constexpr float kRefW = 314.0f;
static constexpr float kRefH = 202.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kMessageTextColor = RC2D_Color{210, 215, 225, 255}; // Meme teinte que les messages du chat.
static constexpr RC2D_Color kScrollTrackColor = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumbColor = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kScrollThumbDragFill = RC2D_Color{184, 132, 30, 245};
static constexpr RC2D_Color kSeparatorColor = RC2D_Color{134, 102, 39, 220};
static constexpr int kScrollLinesStep = 3;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 16.0f;
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
static constexpr const char* kAnnouncementPrefix = "Annonce serveur";

static SDL_FRect getAnnouncementsRectFromGameScreen(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH
    };
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
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
    rc2d_graphics_drawText(&t, std::round(x), std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f)));
    rc2d_graphics_destroyText(&t);
}

static void drawTextAt(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text t = rc2d_graphics_createText(font, text.c_str());
    t.color = color;
    rc2d_graphics_setTextColor(&t);
    rc2d_graphics_drawText(&t, std::round(x), std::round(y));
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
    (void)h;
    return static_cast<float>(w);
}

static float measureLineHeight(RC2D_Font* font)
{
    if (font == nullptr || font->sdl_font == nullptr)
    {
        return 14.0f;
    }

    RC2D_Text probe = rc2d_graphics_createText(font, "Ag");
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&probe, &w, &h);
    rc2d_graphics_destroyText(&probe);
    (void)w;
    return static_cast<float>((std::max)(h, 14));
}

struct AnnonceWrappedRow
{
    std::string text;
    bool messageFirstLine;
    bool separator;
};

static std::string buildFormattedAnnouncement(const std::string& rawMessage)
{
    std::string body = rawMessage;
    const std::size_t firstColon = rawMessage.find(':');
    const std::size_t firstNewline = rawMessage.find('\n');
    if (firstColon != std::string::npos && (firstNewline == std::string::npos || firstColon < firstNewline) && firstColon <= 24)
    {
        std::size_t bodyStart = firstColon + 1;
        while (bodyStart < rawMessage.size() && std::isspace(static_cast<unsigned char>(rawMessage[bodyStart])) != 0)
        {
            ++bodyStart;
        }
        body = rawMessage.substr(bodyStart);
    }
    return std::string(kAnnouncementPrefix) + ": " + body;
}

static void wrapSingleParagraph(RC2D_Font* font, const std::string& paragraph, float maxWidth, std::vector<std::string>& outLines)
{
    if (paragraph.empty())
    {
        outLines.emplace_back("");
        return;
    }

    std::istringstream iss(paragraph);
    std::string word;
    std::string line;
    while (iss >> word)
    {
        std::string candidate = line.empty() ? word : (line + " " + word);
        if (measureTextWidth(font, candidate) <= maxWidth)
        {
            line = candidate;
            continue;
        }

        if (!line.empty())
        {
            outLines.push_back(line);
            line.clear();
        }

        if (measureTextWidth(font, word) <= maxWidth)
        {
            line = word;
            continue;
        }

        std::string chunk;
        for (const char c : word)
        {
            std::string next = chunk + c;
            if (!chunk.empty() && measureTextWidth(font, next) > maxWidth)
            {
                outLines.push_back(chunk);
                chunk.clear();
            }
            chunk.push_back(c);
        }
        line = chunk;
    }

    if (!line.empty())
    {
        outLines.push_back(line);
    }
}

static std::vector<std::string> buildWrappedMessageLines(RC2D_Font* font, const std::string& rawMessage, float maxWidth)
{
    std::vector<std::string> wrappedMessage;
    if (maxWidth <= 6.0f)
    {
        return wrappedMessage;
    }

    std::size_t start = 0;
    while (true)
    {
        const std::size_t breakPos = rawMessage.find('\n', start);
        const std::string paragraph = (breakPos == std::string::npos) ? rawMessage.substr(start) : rawMessage.substr(start, breakPos - start);
        wrapSingleParagraph(font, paragraph, maxWidth, wrappedMessage);
        if (breakPos == std::string::npos)
        {
            break;
        }
        start = breakPos + 1;
    }
    return wrappedMessage;
}

static std::vector<AnnonceWrappedRow> buildWrappedRows(RC2D_Font* font, const std::vector<std::string>& announcementRows, float maxWidth)
{
    std::vector<AnnonceWrappedRow> rows;
    for (std::size_t i = 0; i < announcementRows.size(); ++i)
    {
        const std::string formattedMessage = buildFormattedAnnouncement(announcementRows[i]);
        const std::vector<std::string> wrappedMessage = buildWrappedMessageLines(font, formattedMessage, maxWidth);
        for (std::size_t lineIndex = 0; lineIndex < wrappedMessage.size(); ++lineIndex)
        {
            rows.push_back(AnnonceWrappedRow{
                wrappedMessage[lineIndex],
                lineIndex == 0,
                false
            });
        }

        if (i + 1 < announcementRows.size())
        {
            // Espace equivalent au-dessus et en-dessous de la barre de separation.
            rows.push_back(AnnonceWrappedRow{std::string{}, false, false});
            rows.push_back(AnnonceWrappedRow{std::string{}, false, true});
            rows.push_back(AnnonceWrappedRow{std::string{}, false, false});
        }
    }

    if (rows.empty())
    {
        rows.push_back(AnnonceWrappedRow{std::string{}, false, false});
    }
    return rows;
}

AnnouncementsWidget::AnnouncementsWidget(void)
    : announcementRows{},
      titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      scrollFirstLine(0),
      scrollBarDragging(false),
      scrollDragOffsetY(0.0f),
      scrollBarWheelHighlightSec(0.0f),
      widgetDragging(false),
      widgetDragLocked(false),
      widgetPlacementCustomized(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      widgetWidth(kRefW),
      widgetHeight(kRefH),
      widgetResizing(false),
      resizeStartMouseX(0.0f),
      resizeStartMouseY(0.0f),
      resizeStartWidth(kRefW),
      resizeStartHeight(kRefH),
      cursorEnabled(true),
      controlIcons{}
{
}

AnnouncementsWidget::~AnnouncementsWidget(void)
{
}

void AnnouncementsWidget::load(void)
{
    // Charge la police du titre (meme style visuel que les autres fenetres HUD).
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    // Charge la police du corps de texte (annonces).
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->controlIcons.load();
    // Recupere la position de base de la fenetre dans l'ecran de jeu.
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    // Reinitialise les offsets de deplacement utilisateur.
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetWidth = kRefW;
    this->widgetHeight = kRefH;
    this->widgetResizing = false;
    this->resizeStartMouseX = 0.0f;
    this->resizeStartMouseY = 0.0f;
    this->resizeStartWidth = kRefW;
    this->resizeStartHeight = kRefH;
    // Applique la rect de base.
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, this->widgetWidth, this->widgetHeight};
    // La fenetre s'ouvre via son icone top bar.
    this->visible = false;
    // Aucun drag en cours au demarrage.
    this->widgetDragging = false;
    // Cadenas ouvert par defaut (drag autorise).
    this->widgetDragLocked = false;
    this->widgetPlacementCustomized = false;
    // Le scroll repart de la premiere ligne.
    this->scrollFirstLine = 0;
    this->scrollBarDragging = false;
    this->scrollDragOffsetY = 0.0f;
    this->scrollBarWheelHighlightSec = 0.0f;
    // Nettoie l'historique d'annonces.
    this->announcementRows.clear();
}

void AnnouncementsWidget::unload(void)
{
    this->controlIcons.unload();
    // Libere la police du contenu.
    ResetStorageFontRef(&this->bodyFont);
    // Libere la police du titre.
    ResetStorageFontRef(&this->titleFont);
}

void AnnouncementsWidget::update(double dt)
{
    const float dtF = static_cast<float>(dt);
    if (this->scrollBarWheelHighlightSec > 0.0f)
    {
        this->scrollBarWheelHighlightSec -= dtF;
        if (this->scrollBarWheelHighlightSec < 0.0f)
        {
            this->scrollBarWheelHighlightSec = 0.0f;
        }
    }

    // Recalcule la rect finale a partir de la base + offsets.
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    this->widgetRect = SDL_FRect{baseRect.x + this->widgetOffsetX, baseRect.y + this->widgetOffsetY, this->widgetWidth, this->widgetHeight};

    if (!this->visible)
    {
        return;
    }

    // Si aucun drag/resize/scrollbar actif, rien d'autre a faire.
    if (!this->widgetDragging && !this->widgetResizing && !this->scrollBarDragging)
    {
        return;
    }
    // Si le bouton gauche est relache, on stoppe drag, resize et drag scrollbar.
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        return;
    }

    // Priorite au resize si une prise est active.
    if (this->widgetResizing)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);

        const SDL_FRect screenRect = GetGameScreen().rect;
        const float maxWidth = (screenRect.x + screenRect.w) - this->widgetRect.x;
        const float maxHeight = (screenRect.y + screenRect.h) - this->widgetRect.y;
        const float targetWidth = this->resizeStartWidth + (mouseX - this->resizeStartMouseX);
        const float targetHeight = this->resizeStartHeight + (mouseY - this->resizeStartMouseY);
        this->widgetWidth = clampf(targetWidth, kRefW, (std::max)(kRefW, maxWidth));
        this->widgetHeight = clampf(targetHeight, kRefH, (std::max)(kRefH, maxHeight));
        this->widgetRect.w = this->widgetWidth;
        this->widgetRect.h = this->widgetHeight;
        return;
    }

    // Priorite suivante: drag du pouce de scrollbar.
    if (this->scrollBarDragging)
    {
        const SDL_FRect outer = this->widgetRect;
        const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
        const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
        const SDL_FRect body = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, outer.h - (header.h + 14.0f)};

        const float lineHeight = measureLineHeight(&this->bodyFont);
        const int visibleLines = (std::max)(1, static_cast<int>(std::floor((body.h - 8.0f) / (lineHeight + 1.0f))));
        const std::vector<AnnonceWrappedRow> wrappedRows = buildWrappedRows(&this->bodyFont, this->announcementRows, body.w - 10.0f);
        const int totalLines = static_cast<int>(wrappedRows.size());
        const int maxFirstLine = (std::max)(0, totalLines - visibleLines);
        if (maxFirstLine <= 0)
        {
            this->scrollFirstLine = 0;
            this->scrollBarDragging = false;
            return;
        }

        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        const float thumbHeight = (std::max)(kMinThumbHeight, (scrollTrack.h * static_cast<float>(visibleLines) / static_cast<float>(totalLines)));
        const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        (void)mouseX;

        const float thumbTop = clampf(mouseY - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
        const float t = (thumbTop - scrollTrack.y) / thumbTravel;
        this->scrollFirstLine = static_cast<int>(t * static_cast<float>(maxFirstLine) + 0.5f);
        this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
        return;
    }

    // Lit la souris en coordonnees de rendu pour eviter un decalage visuel.
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    // Recalcule les offsets en conservant le point d'accroche initial.
    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    // Met a jour immediatement la position effective.
    this->widgetRect.x = baseRect.x + this->widgetOffsetX;
    this->widgetRect.y = baseRect.y + this->widgetOffsetY;
}

void AnnouncementsWidget::publishAnnouncementRow(const std::string& rowText)
{
    // Ignore les textes vides pour ne pas polluer l'affichage.
    if (rowText.empty())
    {
        return;
    }
    // Ajoute l'annonce brute; le wrapping est calcule au rendu.
    this->announcementRows.push_back(rowText);

    // Conserve uniquement les annonces les plus recentes.
    if (this->announcementRows.size() > maxStoredAnnouncementRows)
    {
        const std::size_t overflowCount = this->announcementRows.size() - maxStoredAnnouncementRows;
        this->announcementRows.erase(this->announcementRows.begin(), this->announcementRows.begin() + overflowCount);
    }
}

bool AnnouncementsWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Parametres non utilises, imposes par l'interface d'evenements.
    (void)clicks;
    (void)mouseID;

    // Le widget ne consomme que les clics gauches lorsqu'il est visible.
    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    // Synchronise la rect avant tous les hit-tests.
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };
    // Clic hors fenetre: on ne consomme pas l'evenement.
    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    // Construit les sous-zones interactives.
    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, outer.h - (header.h + 14.0f)};
    const SDL_FRect resizeHandleRect = SDL_FRect{
        outer.x + outer.w - 16.0f,
        outer.y + outer.h - 16.0f,
        14.0f,
        14.0f
    };

    // Clic sur la zone de resize: demarre l'agrandissement de la zone texte.
    if (isPointInRect(x, y, resizeHandleRect))
    {
        this->widgetResizing = true;
        this->widgetDragging = false;
        this->widgetPlacementCustomized = true;
        this->resizeStartMouseX = x;
        this->resizeStartMouseY = y;
        this->resizeStartWidth = this->widgetWidth;
        this->resizeStartHeight = this->widgetHeight;
        this->scrollBarDragging = false;
        return true;
    }

    // Clic sur scrollbar/piste: active le drag comme dans le chat.
    const float lineHeight = measureLineHeight(&this->bodyFont);
    const int visibleLines = (std::max)(1, static_cast<int>(std::floor((body.h - 8.0f) / (lineHeight + 1.0f))));
    const std::vector<AnnonceWrappedRow> wrappedRows = buildWrappedRows(&this->bodyFont, this->announcementRows, body.w - 10.0f);
    const int totalLines = static_cast<int>(wrappedRows.size());
    const int maxFirstLine = (std::max)(0, totalLines - visibleLines);
    if (maxFirstLine > 0)
    {
        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        if (isPointInRect(x, y, scrollTrack))
        {
            const float thumbHeight = (std::max)(kMinThumbHeight, (scrollTrack.h * static_cast<float>(visibleLines) / static_cast<float>(totalLines)));
            const float thumbTravel = (std::max)(1.0f, scrollTrack.h - thumbHeight);
            const float currentT = static_cast<float>((std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine))) / static_cast<float>(maxFirstLine);
            const float thumbY = scrollTrack.y + (thumbTravel * currentT);
            const SDL_FRect scrollThumb = SDL_FRect{scrollTrack.x, thumbY, scrollTrack.w, thumbHeight};

            this->scrollBarDragging = true;
            this->widgetDragging = false;
            this->widgetResizing = false;
            if (isPointInRect(x, y, scrollThumb))
            {
                this->scrollDragOffsetY = y - scrollThumb.y;
            }
            else
            {
                this->scrollDragOffsetY = thumbHeight * 0.5f;
                const float thumbTop = clampf(y - this->scrollDragOffsetY, scrollTrack.y, scrollTrack.y + thumbTravel);
                const float t = (thumbTop - scrollTrack.y) / thumbTravel;
                this->scrollFirstLine = static_cast<int>(t * static_cast<float>(maxFirstLine) + 0.5f);
                this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
            }
            return true;
        }
    }

    // Clic sur cadenas: toggle du verrou de drag.
    if (isPointInRect(x, y, lockButtonRect))
    {
        this->widgetDragLocked = !this->widgetDragLocked;
        // Stoppe un drag potentiel pour eviter un etat incoherent.
        this->widgetDragging = false;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        return true;
    }
    // Clic sur croix: ferme la fenetre.
    if (isPointInRect(x, y, closeButtonRect))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        return true;
    }
    // Clic sur header: demarre un drag si non verrouille.
    if (isPointInRect(x, y, header))
    {
        if (this->widgetDragLocked)
        {
            // Verrou actif: on consomme le clic mais on ne deplace pas.
            return true;
        }
        this->widgetDragging = true;
        this->widgetResizing = false;
        this->scrollBarDragging = false;
        this->widgetPlacementCustomized = true;
        // Memorise l'ancrage souris->widget pour un drag fluide.
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }
    // Clic dans la fenetre (zone contenu): consomme.
    return true;
}

bool AnnouncementsWidget::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float wheel_x,
    float wheel_y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    // Deltas non utilises dans cette implementation.
    (void)wheel_x;
    (void)wheel_y;
    (void)integer_x;
    (void)mouseID;
    // Pas de scroll si la fenetre est cachee.
    if (!this->visible)
    {
        return false;
    }

    // Recalcule la rect courante avant hit-test.
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };

    // Calcule la zone de contenu scrollable.
    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, outer.h - (header.h + 14.0f)};
    // N'active le scroll que si la souris est au-dessus du contenu.
    if (!isPointInRect(mouse_x, mouse_y, body))
    {
        return false;
    }

    // Evalue combien de lignes peuvent etre visibles.
    const float lineHeight = measureLineHeight(&this->bodyFont);
    const int visibleLines = (std::max)(1, static_cast<int>(std::floor((body.h - 8.0f) / (lineHeight + 1.0f))));
    // Largeur utile de wrapping.
    const float wrapWidth = body.w - 10.0f;
    // Construit les lignes wrappees a partir des annonces brutes.
    const std::vector<AnnonceWrappedRow> wrappedRows = buildWrappedRows(&this->bodyFont, this->announcementRows, wrapWidth);
    const int totalLines = static_cast<int>(wrappedRows.size());
    // Nombre max de lignes qu'on peut decaler vers le bas.
    const int maxFirstLine = (std::max)(0, totalLines - visibleLines);
    // Rien a scroller si le contenu tient deja.
    if (maxFirstLine <= 0)
    {
        return false;
    }

    // Pas de scroll par defaut (3 lignes), adapte si integer_y est fourni.
    int step = kScrollLinesStep;
    if (integer_y != 0)
    {
        step = (std::max)(1, std::abs(integer_y));
    }
    // Applique le sens de scroll.
    const int lineBefore = this->scrollFirstLine;
    if (direction == RC2D_SCROLL_UP) { this->scrollFirstLine -= step; }
    else if (direction == RC2D_SCROLL_DOWN) { this->scrollFirstLine += step; }
    // Clamp sur la plage autorisee.
    this->scrollFirstLine = (std::max)(0, (std::min)(this->scrollFirstLine, maxFirstLine));
    if (this->scrollFirstLine != lineBefore)
    {
        this->scrollBarWheelHighlightSec = kScrollThumbWheelHighlightSec;
    }
    return true;
}

bool AnnouncementsWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType AnnouncementsWidget::getDesiredCursor(float x, float y) const
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

    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, outer.h - (header.h + 14.0f)};
    const SDL_FRect resizeHandleRect = SDL_FRect{
        outer.x + outer.w - 16.0f,
        outer.y + outer.h - 16.0f,
        14.0f,
        14.0f
    };

    const float lineHeight = measureLineHeight(const_cast<RC2D_Font*>(&this->bodyFont));
    const int visibleLines = (std::max)(1, static_cast<int>(std::floor((body.h - 8.0f) / (lineHeight + 1.0f))));
    const std::vector<AnnonceWrappedRow> wrappedRows = buildWrappedRows(const_cast<RC2D_Font*>(&this->bodyFont), this->announcementRows, body.w - 10.0f);
    const int maxFirstLine = (std::max)(0, static_cast<int>(wrappedRows.size()) - visibleLines);
    const SDL_FRect scrollTrack = SDL_FRect{
        body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
        body.y + kScrollBarPadding,
        kScrollBarWidth,
        body.h - (kScrollBarPadding * 2.0f)
    };

    if (isPointInRect(x, y, resizeHandleRect))
    {
        return HudCursorType::RESIZE_DIAGONAL;
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

void AnnouncementsWidget::draw(void) const
{
    // draw est const; on cast pour mettre a jour widgetRect cachee.
    AnnouncementsWidget* self = const_cast<AnnouncementsWidget*>(this);
    // Recalcule la rect finale (base + offsets).
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        self->widgetWidth,
        self->widgetHeight
    };
    // Si ferme, rien a rendre.
    if (!self->visible)
    {
        return;
    }

    // Construit les zones de rendu principales.
    const SDL_FRect outer = self->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect body = SDL_FRect{outer.x + 8.0f, header.y + header.h + 4.0f, outer.w - 16.0f, outer.h - (header.h + 14.0f)};
    const SDL_FRect closeButtonRect = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const SDL_FRect lockButtonRect = SDL_FRect{
        closeButtonRect.x - 24.0f,
        closeButtonRect.y,
        closeButtonRect.w,
        closeButtonRect.h
    };
    const SDL_FRect resizeHandleRect = SDL_FRect{
        outer.x + outer.w - 16.0f,
        outer.y + outer.h - 16.0f,
        14.0f,
        14.0f
    };

    // Calcule le wrapping et les bornes de scroll pour le rendu texte.
    const float lineHeight = measureLineHeight(&self->bodyFont);
    const int visibleLines = (std::max)(1, static_cast<int>(std::floor((body.h - 8.0f) / (lineHeight + 1.0f))));
    std::vector<AnnonceWrappedRow> wrappedRows = buildWrappedRows(&self->bodyFont, self->announcementRows, body.w - 10.0f);
    const int totalLines = static_cast<int>(wrappedRows.size());
    const int maxFirstLine = (std::max)(0, totalLines - visibleLines);
    self->scrollFirstLine = (std::max)(0, (std::min)(self->scrollFirstLine, maxFirstLine));
    const bool showScrollBar = (maxFirstLine > 0);
    const float textPadding = 4.0f;
    const float scrollReservedW = showScrollBar ? 8.0f : 0.0f;
    const SDL_FRect textClip = SDL_FRect{
        body.x + textPadding,
        body.y + textPadding,
        body.w - (textPadding * 2.0f) - scrollReservedW,
        body.h - (textPadding * 2.0f)
    };

    // Active l'alpha blending pour les panneaux.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Cadre externe + contour or + sous-contour argent.
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    // Header + titre "Annonces".
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);
    drawLeftCenteredY(&self->titleFont, "Annonces", header, header.x + 10.0f, kTextGold);

    // Zone de contenu vide (fond + contour).
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &body);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &body);

    // Dessine uniquement les lignes visibles apres wrapping.
    if (!wrappedRows.empty())
    {
        const int lastLine = (std::min)(totalLines, self->scrollFirstLine + visibleLines);
        float drawY = textClip.y;
        for (int i = self->scrollFirstLine; i < lastLine; ++i)
        {
            const AnnonceWrappedRow& row = wrappedRows[static_cast<std::size_t>(i)];
            if (row.separator)
            {
                rc2d_graphics_setColor(kSeparatorColor);
                const float y = drawY + (lineHeight * 0.52f);
                const float separatorWidth = textClip.w * 0.70f;
                const float separatorX = textClip.x + ((textClip.w - separatorWidth) * 0.5f);
                rc2d_graphics_line(separatorX, y, separatorX + separatorWidth, y);
            }
            else
            {
                bool renderedWithPrefixStyle = false;
                if (row.messageFirstLine)
                {
                    const std::string prefixLabel = std::string(kAnnouncementPrefix);
                    if (row.text.rfind(prefixLabel, 0) == 0)
                    {
                        drawTextAt(&self->bodyFont, prefixLabel, textClip.x, drawY, kTextGold);
                        const std::string suffix = row.text.substr(prefixLabel.size());
                        const float suffixX = textClip.x + measureTextWidth(&self->bodyFont, prefixLabel);
                        drawTextAt(&self->bodyFont, suffix, suffixX, drawY, kMessageTextColor);
                        renderedWithPrefixStyle = true;
                    }
                }

                if (!renderedWithPrefixStyle)
                {
                    drawTextAt(&self->bodyFont, row.text, textClip.x, drawY, kMessageTextColor);
                }
            }
            drawY += lineHeight + 1.0f;
        }
    }

    // Boutons de controle.
    self->controlIcons.drawLockButton(
        lockButtonRect,
        self->widgetDragLocked,
        self->widgetDragLocked ? kPanelFill : kHeaderFill,
        kGold);
    self->controlIcons.drawCloseButton(closeButtonRect, kHeaderFill, kGold);

    // Scrollbar visible uniquement si le contenu depasse.
    if (showScrollBar)
    {
        const SDL_FRect scrollTrack = SDL_FRect{
            body.x + body.w - (kScrollBarWidth + kScrollBarPadding),
            body.y + kScrollBarPadding,
            kScrollBarWidth,
            body.h - (kScrollBarPadding * 2.0f)
        };
        const float viewRatio = static_cast<float>(visibleLines) / static_cast<float>((std::max)(1, totalLines));
        const float thumbH = (std::max)(kMinThumbHeight, scrollTrack.h * viewRatio);
        const float travel = (std::max)(0.0f, scrollTrack.h - thumbH);
        const float t = (maxFirstLine > 0) ? (static_cast<float>(self->scrollFirstLine) / static_cast<float>(maxFirstLine)) : 0.0f;
        const SDL_FRect scrollThumb = SDL_FRect{scrollTrack.x, scrollTrack.y + (travel * t), scrollTrack.w, thumbH};

        rc2d_graphics_setColor(kScrollTrackColor);
        rc2d_graphics_rectangle("fill", &scrollTrack);
        const bool thumbActive = self->scrollBarDragging || (self->scrollBarWheelHighlightSec > 0.0f);
        rc2d_graphics_setColor(thumbActive ? kScrollThumbDragFill : kScrollThumbColor);
        rc2d_graphics_rectangle("fill", &scrollThumb);
    }

    // Poignee de resize partagee avec le chat pour garder un style commun.
    self->controlIcons.drawResizeHandle(resizeHandleRect, kPanelFill, kGold);

    // Restaure le mode de blend par defaut.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void AnnouncementsWidget::show(void)
{
    const SDL_FRect baseRect = getAnnouncementsRectFromGameScreen();
    if (!this->widgetPlacementCustomized)
    {
        this->widgetOffsetX = 0.0f;
        this->widgetOffsetY = 0.0f;
        this->widgetWidth = kRefW;
        this->widgetHeight = kRefH;
    }

    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        this->widgetWidth,
        this->widgetHeight
    };
    this->visible = true;
    this->widgetDragging = false;
    this->widgetResizing = false;
    this->scrollBarDragging = false;
}

void AnnouncementsWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->widgetResizing = false;
    this->scrollBarDragging = false;
    this->scrollBarWheelHighlightSec = 0.0f;
}


