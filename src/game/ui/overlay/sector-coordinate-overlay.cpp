#include "game/ui/overlay/sector-coordinate-overlay.h"

#include <cstdio>

#include "core/context.h"

SectorCoordinateOverlay::SectorCoordinateOverlay(void)
    : font{},
      textColor{235, 242, 248, 240}
{
}

SectorCoordinateOverlay::~SectorCoordinateOverlay(void)
{
}

void SectorCoordinateOverlay::load(void)
{
    // -------------------------------------------------------------------------
    // Chargement de la police utilisee par ce widget.
    // -------------------------------------------------------------------------
    // On reutilise la meme fonte que les autres overlays gameplay afin de
    // conserver une identite visuelle uniforme dans toute la scene.
    // Taille 18: legible dans la barre haute sans trop occuper d'espace.
    // -------------------------------------------------------------------------
    this->font = rc2d_graphics_openFontFromStorage(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        18.0f);
}

void SectorCoordinateOverlay::unload(void)
{
    // -------------------------------------------------------------------------
    // Liberation de la police.
    // -------------------------------------------------------------------------
    // Le close est centralise ici pour garder une responsabilite claire:
    // ce composant charge -> ce composant decharge.
    // -------------------------------------------------------------------------
    rc2d_graphics_closeFont(&this->font);
}

void SectorCoordinateOverlay::getRowLabel(int index, char* out, int outSize) const
{
    // Secteurs horizontaux: 00..59.
    if (index < 0 || index >= Map::NUM_SECTORS_X)
    {
        out[0] = '\0';
        return;
    }

    std::snprintf(out, static_cast<size_t>(outSize), "%02d", index);
}

void SectorCoordinateOverlay::getColumnLabel(int index, char* out, int outSize) const
{
    // Secteurs verticaux: AA..CH (encodage base-26 sur 2 lettres).
    if (index < 0 || index >= Map::NUM_SECTORS_Y)
    {
        out[0] = '\0';
        return;
    }

    const char first = static_cast<char>('A' + (index / 26));
    const char second = static_cast<char>('A' + (index % 26));
    std::snprintf(out, static_cast<size_t>(outSize), "%c%c", first, second);
}

void SectorCoordinateOverlay::draw(const Map& map, const Player& player)
{
    // -------------------------------------------------------------------------
    // Garde-fou: si la police n'est pas disponible, on ne tente aucun rendu.
    // -------------------------------------------------------------------------
    if (this->font.sdl_font == nullptr)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // 1) Recuperation de la position courante du joueur dans la grille tuiles.
    // -------------------------------------------------------------------------
    // La position est flottante (pas seulement entiere), ce qui permet d'avoir
    // une coordonnee secteur qui suit le joueur pendant le mouvement.
    // -------------------------------------------------------------------------
    const SDL_FPoint tilePosition = player.getTilePosition();

    // -------------------------------------------------------------------------
    // 2) Conversion tuile -> secteur logique.
    // -------------------------------------------------------------------------
    // On convertit vers le secteur entier le plus proche (arrondi), pour
    // rester coherent avec l'affichage lisible de type "30-AE".
    // -------------------------------------------------------------------------
    const SDL_Point sector = map.tileToSectorNearest(tilePosition.x, tilePosition.y);

    // -------------------------------------------------------------------------
    // 3) Construction de la chaine finale.
    // -------------------------------------------------------------------------
    // - Si hors grille metier: "??-??"
    // - Sinon: "XX-YY" (ex: 30-AE)
    // -------------------------------------------------------------------------
    char sectorText[32];
    if (!map.isInsideSector(sector.x, sector.y))
    {
        std::snprintf(sectorText, sizeof(sectorText), "??-??");
    }
    else
    {
        char rowLabel[4];
        char columnLabel[4];
        this->getRowLabel(sector.x, rowLabel, sizeof(rowLabel));
        this->getColumnLabel(sector.y, columnLabel, sizeof(columnLabel));
        std::snprintf(sectorText, sizeof(sectorText), "%s-%s", rowLabel, columnLabel);
    }

    // -------------------------------------------------------------------------
    // 4) Creation du texte GPU + application de la couleur HUD.
    // -------------------------------------------------------------------------
    // RC2D_Text encapsule la texture texte prete a etre dessinee.
    // -------------------------------------------------------------------------
    RC2D_Text text = rc2d_graphics_createText(&this->font, sectorText);
    text.color = this->textColor;
    rc2d_graphics_setTextColor(&text);

    // -------------------------------------------------------------------------
    // 5) Mesure des dimensions du texte.
    // -------------------------------------------------------------------------
    // Ces dimensions servent au centrage precis en X et en Y.
    // -------------------------------------------------------------------------
    int textWidth = 0;
    int textHeight = 0;
    rc2d_graphics_getTextSize(&text, &textWidth, &textHeight);

    // -------------------------------------------------------------------------
    // 6) Positionnement UI:
    //    - base sur gameScreen rect (pas map rect),
    //    - centre horizontal de l'ecran de jeu,
    //    - centre vertical dans la bande UI haute (MAP_TOP_UI_MARGIN_PX).
    // -------------------------------------------------------------------------
    const SDL_FRect& gameScreenRect = GetGameScreen().rect;
    const float drawX = (gameScreenRect.x + (gameScreenRect.w * 0.5f)) - (static_cast<float>(textWidth) * 0.5f);
    const float drawY = gameScreenRect.y + ((Map::MAP_TOP_UI_MARGIN_PX - static_cast<float>(textHeight)) * 0.5f);

    // -------------------------------------------------------------------------
    // 7) Draw + destruction des ressources texte temporaires.
    // -------------------------------------------------------------------------
    rc2d_graphics_drawText(&text, drawX, drawY);
    rc2d_graphics_destroyText(&text);
}
