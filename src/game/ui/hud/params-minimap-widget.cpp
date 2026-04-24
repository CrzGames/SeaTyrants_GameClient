#include "game/ui/hud/params-minimap-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <cmath>

static constexpr float kRefW = 277.0f;
static constexpr float kRefH = 226.0f;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 240};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 235};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextWhite = RC2D_Color{210, 215, 225, 255};

static SDL_FRect getParamsMinimapRectFromGameScreen(void)
{
    // Rectangle de l'ecran de jeu (zone de rendu UI).
    const SDL_FRect screenRect = GetGameScreen().rect;
    // Position de base: centree sur l'ecran en tenant compte de la taille widget.
    return SDL_FRect{
        screenRect.x + ((screenRect.w - kRefW) * 0.5f),
        screenRect.y + ((screenRect.h - kRefH) * 0.5f),
        kRefW,
        kRefH
    };
}

static bool isPointInRect(float x, float y, const SDL_FRect& r)
{
    // Test d'inclusion classique (bornes inclusives).
    return (x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h));
}

static void getMouseRenderPosition(float* outX, float* outY)
{
    // Protection defensive: on ne fait rien si les sorties sont invalides.
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

    // Lecture de la position souris en coordonnees "fenetre".
    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);

    // Recuperation du renderer pour convertir fenetre -> rendu.
    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        // Fallback: si renderer absent, on renvoie les coordonnees brutes.
        *outX = windowX;
        *outY = windowY;
        return;
    }

    // Valeurs de travail initialisees avec les coords fenetre.
    float renderX = windowX;
    float renderY = windowY;
    // Conversion vers l'espace de rendu (important si viewport/scale).
    if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &renderX, &renderY))
    {
        // Si la conversion echoue, fallback sur les coords fenetre.
        renderX = windowX;
        renderY = windowY;
    }

    // Ecriture des sorties pour l'appelant.
    *outX = renderX;
    *outY = renderY;
}

static void drawLeftCenteredY(RC2D_Font* font, const char* text, const SDL_FRect& r, float x, RC2D_Color color)
{
    // Validation des entrees: police prete + texte non vide.
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    // Creation de l'objet texte RC2D.
    RC2D_Text t = rc2d_graphics_createText(font, text);
    // Couleur forcee dans la structure texte.
    t.color = color;
    // Application de la couleur de texte au renderer.
    rc2d_graphics_setTextColor(&t);
    // Mesure largeur/hauteur du texte.
    int w = 0;
    int h = 0;
    rc2d_graphics_getTextSize(&t, &w, &h);
    // La largeur n'est pas utile ici (alignement a gauche).
    (void)w;
    // Dessin a gauche (x impose) et centre verticalement dans le rectangle.
    rc2d_graphics_drawText(&t, std::round(x), std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f)));
    // Destruction de l'objet texte temporaire.
    rc2d_graphics_destroyText(&t);
}

static void drawCheckBox(const SDL_FRect& boxRect, bool checked)
{
    // Fond interne de la case.
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &boxRect);
    // Contour de la case.
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &boxRect);
    // Si non cochee, on s'arrete ici.
    if (!checked)
    {
        return;
    }

    // Trace du "check" en deux segments.
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_line(boxRect.x + 4.0f, boxRect.y + (boxRect.h * 0.55f), boxRect.x + 8.0f, boxRect.y + boxRect.h - 5.0f);
    rc2d_graphics_line(boxRect.x + 8.0f, boxRect.y + boxRect.h - 5.0f, boxRect.x + boxRect.w - 4.0f, boxRect.y + 4.0f);
}

ParamsMinimapWidget::ParamsMinimapWidget(void)
    : titleFont{},
      bodyFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      showPlayers(true),
      showMonsters(true),
      showShips(true),
      showTreasures(true),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      cursorEnabled(true),
      controlIcons{}
{
}

ParamsMinimapWidget::~ParamsMinimapWidget(void)
{
}

void ParamsMinimapWidget::load(void)
{
    // Police titre identique au style des autres fenetres HUD.
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    // Police des lignes de menu (plus petite que le titre).
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->controlIcons.load();

    // Rectangle de base calcule depuis l'ecran.
    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    // Remise a zero des offsets de deplacement utilisateur.
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    // Placement initial de la fenetre.
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    // Fenetre masquee au chargement: ouverture via l'icone top bar.
    this->visible = false;
    // Aucun drag actif au chargement.
    this->widgetDragging = false;
    // Offsets de drag remis a zero.
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
}

void ParamsMinimapWidget::unload(void)
{
    this->controlIcons.unload();
    // Liberation police de texte secondaire.
    ResetStorageFontRef(&this->bodyFont);
    // Liberation police de titre.
    ResetStorageFontRef(&this->titleFont);
}

void ParamsMinimapWidget::update(double dt)
{
    // Delta time non utilise pour l'instant (pas d'animation temporelle ici).
    (void)dt;

    // Recalcul du rectangle a partir de l'ancrage + offsets.
    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    // Si pas en mode drag, aucune mise a jour supplementaire.
    if (!this->widgetDragging)
    {
        return;
    }

    // Si le clic gauche est relache, on termine le drag.
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        return;
    }

    // Lecture souris dans l'espace rendu pour un drag stable.
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    // Recalcul des offsets en conservant l'ancrage pris au debut du drag.
    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    // Application immediate sur la rect courante.
    this->widgetRect.x = baseRect.x + this->widgetOffsetX;
    this->widgetRect.y = baseRect.y + this->widgetOffsetY;
}

bool ParamsMinimapWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Parametres inutilises dans cette implementation.
    (void)clicks;
    (void)mouseID;
    // On ignore si fenetre cachee ou si ce n'est pas un clic gauche.
    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    // Synchronise la rect avant hit-tests.
    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    // Clic hors widget: non consomme.
    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    // Sous-zones utiles pour les interactions.
    const SDL_FRect outer = this->widgetRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButton = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    // Ferme la fenetre si clic sur la croix.
    if (isPointInRect(x, y, closeButton))
    {
        this->visible = false;
        this->widgetDragging = false;
        return true;
    }

    // Parametres de grille verticale des options.
    const float rowStartY = outer.y + 56.0f;
    const float rowStep = 41.0f;
    // Rectangles des cases a cocher.
    SDL_FRect checkPlayers = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 0.0f), 24.0f, 24.0f};
    SDL_FRect checkMonsters = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 1.0f), 24.0f, 24.0f};
    SDL_FRect checkShips = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 2.0f), 24.0f, 24.0f};
    SDL_FRect checkTreasures = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 3.0f), 24.0f, 24.0f};

    // Toggle de l'option joueurs.
    if (isPointInRect(x, y, checkPlayers))
    {
        this->showPlayers = !this->showPlayers;
        return true;
    }
    // Toggle de l'option monstres.
    if (isPointInRect(x, y, checkMonsters))
    {
        this->showMonsters = !this->showMonsters;
        return true;
    }
    // Toggle de l'option navires.
    if (isPointInRect(x, y, checkShips))
    {
        this->showShips = !this->showShips;
        return true;
    }
    // Toggle de l'option tresors.
    if (isPointInRect(x, y, checkTreasures))
    {
        this->showTreasures = !this->showTreasures;
        return true;
    }

    // Drag active si clic dans le header (hors croix deja traitee).
    if (isPointInRect(x, y, header))
    {
        this->widgetDragging = true;
        // Memorise l'ancrage souris->widget pour conserver le point de prise.
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        return true;
    }
    // Clic dans la fenetre mais hors controles: evenement tout de meme consomme.
    return true;
}

void ParamsMinimapWidget::draw(void) const
{
    // Methode const: on cast pour mettre a jour widgetRect calcule.
    ParamsMinimapWidget* self = const_cast<ParamsMinimapWidget*>(this);
    // Recalcule la rect finale depuis l'ancrage + offsets.
    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    // Rien a dessiner si fenetre cachee.
    if (!self->visible)
    {
        return;
    }

    // Decoupage des zones visuelles principales.
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

    // Position des 4 lignes options.
    const float rowStartY = outer.y + 56.0f;
    const float rowStep = 41.0f;
    const SDL_FRect rowPlayers = SDL_FRect{outer.x + 16.0f, rowStartY + (rowStep * 0.0f), outer.w - 32.0f, 24.0f};
    const SDL_FRect rowMonsters = SDL_FRect{outer.x + 16.0f, rowStartY + (rowStep * 1.0f), outer.w - 32.0f, 24.0f};
    const SDL_FRect rowShips = SDL_FRect{outer.x + 16.0f, rowStartY + (rowStep * 2.0f), outer.w - 32.0f, 24.0f};
    const SDL_FRect rowTreasures = SDL_FRect{outer.x + 16.0f, rowStartY + (rowStep * 3.0f), outer.w - 32.0f, 24.0f};

    // Position des cases a cocher associees.
    const SDL_FRect checkPlayers = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 0.0f), 24.0f, 24.0f};
    const SDL_FRect checkMonsters = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 1.0f), 24.0f, 24.0f};
    const SDL_FRect checkShips = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 2.0f), 24.0f, 24.0f};
    const SDL_FRect checkTreasures = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 3.0f), 24.0f, 24.0f};

    // Active le blend pour transparence du panneau.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Fond principal et double contour.
    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &inner);

    // Bandeau du haut (header).
    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &header);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &header);

    // Titre de la fenetre.
    drawLeftCenteredY(&self->titleFont, "Parametres de MiniMap", header, header.x + 8.0f, kTextGold);

    // Bouton fermer.
    self->controlIcons.drawCloseButton(closeButtonCentered, kHeaderFill, kGold);

    // Libelles des options.
    drawLeftCenteredY(&self->bodyFont, "Afficher les joueurs", rowPlayers, rowPlayers.x, kTextWhite);
    drawLeftCenteredY(&self->bodyFont, "Afficher les monstres", rowMonsters, rowMonsters.x, kTextWhite);
    drawLeftCenteredY(&self->bodyFont, "Afficher les navires", rowShips, rowShips.x, kTextWhite);
    drawLeftCenteredY(&self->bodyFont, "Afficher les tresors", rowTreasures, rowTreasures.x, kTextWhite);

    // Cases a cocher selon les etats booleens.
    drawCheckBox(checkPlayers, self->showPlayers);
    drawCheckBox(checkMonsters, self->showMonsters);
    drawCheckBox(checkShips, self->showShips);
    drawCheckBox(checkTreasures, self->showTreasures);

    // Restaure le mode de blend par defaut.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool ParamsMinimapWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType ParamsMinimapWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
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

    const SDL_FRect baseRect = getParamsMinimapRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const SDL_FRect outer = currentRect;
    const SDL_FRect inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    const SDL_FRect header = SDL_FRect{inner.x + 1.0f, inner.y + 1.0f, inner.w - 2.0f, 30.0f};
    const SDL_FRect closeButton = SDL_FRect{
        outer.x + outer.w - 28.0f,
        header.y + ((header.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };

    const float rowStartY = outer.y + 56.0f;
    const float rowStep = 41.0f;
    const SDL_FRect checkPlayers = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 0.0f), 24.0f, 24.0f};
    const SDL_FRect checkMonsters = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 1.0f), 24.0f, 24.0f};
    const SDL_FRect checkShips = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 2.0f), 24.0f, 24.0f};
    const SDL_FRect checkTreasures = SDL_FRect{outer.x + outer.w - 48.0f, rowStartY + (rowStep * 3.0f), 24.0f, 24.0f};

    if (isPointInRect(x, y, closeButton) ||
        isPointInRect(x, y, checkPlayers) ||
        isPointInRect(x, y, checkMonsters) ||
        isPointInRect(x, y, checkShips) ||
        isPointInRect(x, y, checkTreasures))
    {
        return HudCursorType::POINTER;
    }
    if (isPointInRect(x, y, header))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

void ParamsMinimapWidget::show(void)
{
    this->visible = true;
    this->widgetDragging = false;
}

void ParamsMinimapWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
}

